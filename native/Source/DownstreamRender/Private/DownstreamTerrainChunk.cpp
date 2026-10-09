#include "DownstreamTerrainChunk.h"

#include "DownstreamDebugHUD.h"
#include "DownstreamSimulationSubsystem.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ADownstreamTerrainChunk::ADownstreamTerrainChunk()
{
    PrimaryActorTick.bCanEverTick = true;
    TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
    SetRootComponent(TerrainMesh);
    TerrainMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TerrainMesh->SetCanEverAffectNavigation(false);

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> DebugMaterial(
        TEXT("/Game/Downstream/Materials/M_TerrainDebug.M_TerrainDebug"));
    if (DebugMaterial.Succeeded())
        TerrainMaterial = DebugMaterial.Object;
}

void ADownstreamTerrainChunk::BeginPlay()
{
    Super::BeginPlay();
    BindDebugInput();
    if (RebuildMesh())
    {
        // The initial snapshot already contains the initial dirty terrain.
        GetSimulationSubsystem()->ConsumeDirtyChunks();
    }
}

void ADownstreamTerrainChunk::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!InputController.IsValid())
        BindDebugInput();

    UDownstreamSimulationSubsystem* Subsystem = GetSimulationSubsystem();
    if (!Subsystem || !Subsystem->GetSimulation())
        return;

    if (RebuildCount == 0)
    {
        if (RebuildMesh())
            Subsystem->ConsumeDirtyChunks();
        return;
    }

    // M0 has one terrain actor, and therefore one destructive dirty-set consumer.
    // A future renderer must consume once centrally and dispatch to all patches.
    const auto DirtyChunks = Subsystem->ConsumeDirtyChunks();
    FIntPoint Origin = FIntPoint::ZeroValue, CellCounts = FIntPoint::ZeroValue;
    if (!GetPatchBounds(Origin, CellCounts))
        return;

    const int32 ChunkSize = Subsystem->GetTerrainChunkSize();
    const FIntPoint LastSample = Origin + CellCounts;
    for (const downstream::ChunkCoord& Dirty : DirtyChunks)
    {
        const int64 FirstX = static_cast<int64>(Dirty.X) * ChunkSize;
        const int64 FirstY = static_cast<int64>(Dirty.Y) * ChunkSize;
        const int64 LastX = FirstX + ChunkSize - 1;
        const int64 LastY = FirstY + ChunkSize - 1;
        // Samples include the forward shared edge (e.g. x/y 32 for chunk 0).
        if (FirstX <= LastSample.X && LastX >= Origin.X
            && FirstY <= LastSample.Y && LastY >= Origin.Y)
        {
            RebuildMesh();
            break;
        }
    }
}

UDownstreamSimulationSubsystem* ADownstreamTerrainChunk::GetSimulationSubsystem() const
{
    return GetWorld() ? GetWorld()->GetSubsystem<UDownstreamSimulationSubsystem>() : nullptr;
}

bool ADownstreamTerrainChunk::GetPatchBounds(FIntPoint& Origin, FIntPoint& CellCounts) const
{
    const UDownstreamSimulationSubsystem* Subsystem = GetSimulationSubsystem();
    if (!Subsystem || !Subsystem->GetSimulation() || ChunkX < 0 || ChunkY < 0)
        return false;

    const int32 ChunkSize = Subsystem->GetTerrainChunkSize();
    const int64 OriginX = static_cast<int64>(ChunkX) * ChunkSize;
    const int64 OriginY = static_cast<int64>(ChunkY) * ChunkSize;
    const int32 Width = Subsystem->GetGridWidth();
    const int32 Height = Subsystem->GetGridHeight();
    if (ChunkSize < 1 || OriginX >= Width - 1 || OriginY >= Height - 1)
        return false;

    Origin = FIntPoint(static_cast<int32>(OriginX), static_cast<int32>(OriginY));
    const int32 RequestedCells = FMath::Clamp(ChunkCellCount, 1, 32);
    CellCounts = FIntPoint(FMath::Min(RequestedCells, Width - 1 - Origin.X),
                          FMath::Min(RequestedCells, Height - 1 - Origin.Y));
    return true;
}

bool ADownstreamTerrainChunk::RebuildMesh()
{
    FIntPoint Origin = FIntPoint::ZeroValue, CellCounts = FIntPoint::ZeroValue;
    if (!GetPatchBounds(Origin, CellCounts) || !FMath::IsFinite(CellSize) || CellSize <= 0.0f
        || !FMath::IsFinite(HeightVisualScale) || HeightVisualScale < 0.0f)
        return false;

    // This pointer is read only on the game thread; no worker snapshot is claimed.
    const downstream::Simulation* Simulation = GetSimulationSubsystem()->GetSimulation();
    const int32 VertexColumns = CellCounts.X + 1;
    const int32 VertexRows = CellCounts.Y + 1;
    const int32 VertexCount = VertexColumns * VertexRows;
    TArray<FVector> Vertices, Normals;
    TArray<int32> Triangles;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    Vertices.Reserve(VertexCount);
    Normals.Init(FVector::ZeroVector, VertexCount);
    UVs.Reserve(VertexCount);
    Colors.Reserve(VertexCount);
    Tangents.Reserve(VertexCount);
    Triangles.Reserve(CellCounts.X * CellCounts.Y * 6);

    for (int32 Y = 0; Y < VertexRows; ++Y)
    {
        for (int32 X = 0; X < VertexColumns; ++X)
        {
            const float Height = Simulation->HeightAt(Origin.X + X, Origin.Y + Y);
            Vertices.Add(FVector(X * CellSize, Y * CellSize, Height * HeightVisualScale));
            UVs.Add(FVector2D(static_cast<double>(X) / CellCounts.X,
                             static_cast<double>(Y) / CellCounts.Y));
            const float Blend = FMath::Clamp((Height + 10.0f) / 20.0f, 0.0f, 1.0f);
            Colors.Add(FMath::Lerp(FLinearColor(0.08f, 0.19f, 0.24f, 1.0f),
                                  FLinearColor(0.67f, 0.82f, 0.39f, 1.0f), Blend));
        }
    }

    const auto AddTriangle = [&](int32 A, int32 B, int32 C)
    {
        Triangles.Append({A, B, C});
        const FVector Normal = FVector::CrossProduct(Vertices[B] - Vertices[A],
                                                      Vertices[C] - Vertices[A]);
        Normals[A] += Normal;
        Normals[B] += Normal;
        Normals[C] += Normal;
    };
    for (int32 Y = 0; Y < CellCounts.Y; ++Y)
    {
        for (int32 X = 0; X < CellCounts.X; ++X)
        {
            const int32 A = Y * VertexColumns + X;
            const int32 B = A + 1;
            const int32 C = A + VertexColumns;
            const int32 D = C + 1;
            AddTriangle(A, B, C);
            AddTriangle(B, D, C);
        }
    }
    for (FVector& Normal : Normals)
    {
        Normal.Normalize();
        const FVector Tangent = (FVector::ForwardVector - Normal * Normal.X).GetSafeNormal();
        Tangents.Add(FProcMeshTangent(Tangent, false));
    }

    // The presentation spike must not alter existing pawn collision or navigation.
    TerrainMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs,
                                              Colors, Tangents, false);
    TerrainMesh->SetMaterial(0, TerrainMaterial);
    ++RebuildCount;
    return true;
}

float ADownstreamTerrainChunk::GetCellHeight() const
{
    FIntPoint Origin = FIntPoint::ZeroValue, CellCounts = FIntPoint::ZeroValue;
    if (!GetPatchBounds(Origin, CellCounts))
        return 0.0f;
    return GetSimulationSubsystem()->GetSimulation()->HeightAt(
        Origin.X + CellCounts.X / 2, Origin.Y + CellCounts.Y / 2);
}

void ADownstreamTerrainChunk::RaiseCentralRegion()
{
    QueueCentralRegion(2.0f);
}

void ADownstreamTerrainChunk::LowerCentralRegion()
{
    QueueCentralRegion(-2.0f);
}

void ADownstreamTerrainChunk::QueueCentralRegion(float HeightDelta)
{
    FIntPoint Origin = FIntPoint::ZeroValue, CellCounts = FIntPoint::ZeroValue;
    if (!GetPatchBounds(Origin, CellCounts))
        return;
    UDownstreamSimulationSubsystem* Subsystem = GetSimulationSubsystem();
    const downstream::Simulation* Simulation = Subsystem->GetSimulation();
    const FIntPoint Center = Origin + FIntPoint(CellCounts.X / 2, CellCounts.Y / 2);
    for (int32 Y = FMath::Max(Origin.Y, Center.Y - 2); Y <= FMath::Min(Origin.Y + CellCounts.Y, Center.Y + 2); ++Y)
        for (int32 X = FMath::Max(Origin.X, Center.X - 2); X <= FMath::Min(Origin.X + CellCounts.X, Center.X + 2); ++X)
            Subsystem->QueueTerrainHeight(X, Y, Simulation->HeightAt(X, Y) + HeightDelta);
}

void ADownstreamTerrainChunk::BindDebugInput()
{
    APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!Controller || !Controller->IsLocalController())
        return;
    EnableInput(Controller);
    if (!InputComponent)
        return;
    InputController = Controller;
    InputComponent->BindKey(EKeys::F6, IE_Pressed, this, &ADownstreamTerrainChunk::ToggleDebugView);
    InputComponent->BindKey(EKeys::F7, IE_Pressed, this, &ADownstreamTerrainChunk::RaiseCentralRegion);
    InputComponent->BindKey(EKeys::F8, IE_Pressed, this, &ADownstreamTerrainChunk::LowerCentralRegion);
    if (!Cast<ADownstreamDebugHUD>(Controller->GetHUD()))
        Controller->ClientSetHUD(ADownstreamDebugHUD::StaticClass());
}

void ADownstreamTerrainChunk::ToggleDebugView()
{
    if (!GetWorld() || !GetWorld()->IsGameWorld())
        return;
    if (!InputController.IsValid())
        BindDebugInput();
    APlayerController* Controller = InputController.Get();
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    if (!Pawn)
        return;

    if (bAtDebugView && SavedPlayerPawn.Get() != Pawn)
    {
        if (USpringArmComponent* Arm = SavedSpringArm.Get())
        {
            Arm->TargetOffset = SavedCameraTargetOffset;
            Arm->TargetArmLength = SavedCameraArmLength;
        }
        Controller->SetIgnoreLookInput(false);
        bAtDebugView = false;
    }
    FIntPoint Origin = FIntPoint::ZeroValue, CellCounts = FIntPoint::ZeroValue;
    if (!bAtDebugView)
    {
        if (!GetPatchBounds(Origin, CellCounts) || !FMath::IsFinite(CellSize) || CellSize <= 0.0f
            || !FMath::IsFinite(HeightVisualScale) || HeightVisualScale < 0.0f)
            return;
        SavedPlayerPawn = Pawn;
        SavedPlayerLocation = Pawn->GetActorLocation();
        SavedPlayerRotation = Pawn->GetActorRotation();
        SavedControlRotation = Controller->GetControlRotation();
        SavedSpringArm = Pawn->FindComponentByClass<USpringArmComponent>();
        if (USpringArmComponent* Arm = SavedSpringArm.Get())
        {
            SavedCameraTargetOffset = Arm->TargetOffset;
            SavedCameraArmLength = Arm->TargetArmLength;
        }
    }
    Controller->StopMovement();
    if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
        Movement->StopMovementImmediately();

    const FVector Destination = bAtDebugView ? SavedPlayerLocation : FVector(1000.0, -1600.0, 100.0);
    const FRotator PawnRotation = bAtDebugView ? SavedPlayerRotation : Pawn->GetActorRotation();
    Pawn->SetActorLocationAndRotation(Destination, PawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
    Controller->SetControlRotation(SavedControlRotation);
    if (USpringArmComponent* Arm = SavedSpringArm.Get())
    {
        if (bAtDebugView)
        {
            Arm->TargetOffset = SavedCameraTargetOffset;
            Arm->TargetArmLength = SavedCameraArmLength;
        }
        else
        {
            const FVector PatchCenter = GetActorTransform().TransformPosition(FVector(
                CellCounts.X * CellSize * 0.5f, CellCounts.Y * CellSize * 0.5f,
                GetCellHeight() * HeightVisualScale + 80.0f));
            Arm->TargetOffset = PatchCenter - Pawn->GetActorLocation();
            Arm->TargetArmLength = FMath::Clamp(2800.0f, 1400.0f, 3000.0f);
        }
    }
    // Balance the controller's ignore-look counter, preserving any preexisting lock.
    Controller->SetIgnoreLookInput(!bAtDebugView);
    bAtDebugView = !bAtDebugView;
}
