#include "DownstreamWaterSite.h"

#include "DownstreamDebugHUD.h"
#include "DownstreamPlayerCharacter.h"
#include "DownstreamTerrainMeshComponent.h"
#include "DownstreamEarthworks.h"
#include "DownstreamWater.h"
#include "DownstreamWaterSubsystem.h"
#include "AI/NavigationSystemBase.h"
#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamPrototypeTuning.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Sound/SoundBase.h"
#include "Components/InputComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "ProceduralMeshComponent.h"


namespace
{
constexpr float MetresToCm = 100.0f;
constexpr float SkirtZ = -6.0f;

template <typename T>
T* LoadIfPresent(const TCHAR* Package)
{
    if (!FPackageName::DoesPackageExist(Package)) return nullptr;
    return LoadObject<T>(nullptr, *FString::Printf(TEXT("%s.%s"), Package, *FPackageName::GetShortName(Package)));
}

FLinearColor GroundColor(float Height, float Base)
{
    const FLinearColor Dust(0.46f, 0.36f, 0.24f);
    const FLinearColor Mud(0.20f, 0.17f, 0.13f);
    const FLinearColor Fresh(0.33f, 0.21f, 0.12f);
    const FLinearColor Berm(0.60f, 0.49f, 0.33f);
    FLinearColor Color = FMath::Lerp(Mud, Dust, FMath::Clamp(Height / 1.6f, 0.0f, 1.0f));
    if (Height < Base - 0.05f) Color = FMath::Lerp(Color, Fresh, 0.75f);
    else if (Height > Base + 0.05f) Color = FMath::Lerp(Color, Berm, 0.7f);
    // Contour bands every 0.5 m keep elevation readable from the fixed camera.
    const float Band = FMath::Frac(Height / 0.5f);
    if (Band < 0.1f || Band > 0.95f) Color *= 0.82f;
    Color.A = 1.0f;
    return Color;
}
}

ADownstreamWaterSite::ADownstreamWaterSite()
{
    PrimaryActorTick.bCanEverTick = true;
    TerrainMesh = CreateDefaultSubobject<UDownstreamTerrainMeshComponent>(TEXT("TerrainMesh"));
    SetRootComponent(TerrainMesh);
    TerrainMesh->bUseComplexAsSimpleCollision = true;
    TerrainMesh->SetCollisionProfileName(TEXT("BlockAll"));
    TerrainMesh->SetCanEverAffectNavigation(true);
    TerrainMesh->SetMobility(EComponentMobility::Movable);

    WaterMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaterMesh"));
    WaterMesh->SetupAttachment(TerrainMesh);
    WaterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WaterMesh->SetCanEverAffectNavigation(false);
    WaterMesh->SetCastShadow(false);

    PlanMarkers = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PlanMarkers"));
    PlanMarkers->SetupAttachment(TerrainMesh);
    PlanMarkers->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PlanMarkers->SetCanEverAffectNavigation(false);
    PlanMarkers->SetCastShadow(false);
    QueueMarkers = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("QueueMarkers"));
    QueueMarkers->SetupAttachment(TerrainMesh);
    QueueMarkers->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    QueueMarkers->SetCanEverAffectNavigation(false);
    QueueMarkers->SetCastShadow(false);

    SpringPipe = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpringPipe"));
    SpringPipe->SetupAttachment(TerrainMesh);
    SpringPipe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SpringPipe->SetCanEverAffectNavigation(false);
    CulvertPipe = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CulvertPipe"));
    CulvertPipe->SetupAttachment(TerrainMesh);
    CulvertPipe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CulvertPipe->SetCanEverAffectNavigation(false);
    WaterTower = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WaterTower"));
    WaterTower->SetupAttachment(TerrainMesh);
    WaterTower->SetCollisionProfileName(TEXT("BlockAll"));
    WaterTower->SetCanEverAffectNavigation(true);
    CulvertSign = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CulvertSign"));
    CulvertSign->SetupAttachment(TerrainMesh);
    CulvertSign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CulvertSign->SetCanEverAffectNavigation(false);
    CulvertLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("CulvertLabel"));
    SpringLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SpringLabel"));
    for (UTextRenderComponent* Text : {CulvertLabel.Get(), SpringLabel.Get()})
    {
        Text->SetupAttachment(TerrainMesh);
        Text->SetHorizontalAlignment(EHTA_Center);
        Text->SetWorldSize(32.0f);
        Text->SetUsingAbsoluteRotation(true);
        Text->SetWorldRotation(FRotator(55, -135, 0));
    }
    CulvertLabel->SetText(FText::FromString(TEXT("OLD CULVERT")));
    CulvertLabel->SetTextRenderColor(FColor(170, 200, 190));
    SpringLabel->SetText(FText::FromString(TEXT("BURST MAIN")));
    SpringLabel->SetTextRenderColor(FColor(120, 200, 255));
}

void ADownstreamWaterSite::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (!TerrainMaterial) TerrainMaterial = LoadIfPresent<UMaterialInterface>(TEXT("/Game/Downstream/Materials/M_SiteGround"));
    if (!WaterMaterial) WaterMaterial = LoadIfPresent<UMaterialInterface>(TEXT("/Game/Downstream/Materials/M_Water"));
    if (!ValidMarkerMaterial) ValidMarkerMaterial = LoadIfPresent<UMaterialInterface>(TEXT("/Game/Downstream/Materials/M_PlacementValid"));
    if (!InvalidMarkerMaterial) InvalidMarkerMaterial = LoadIfPresent<UMaterialInterface>(TEXT("/Game/Downstream/Materials/M_PlacementInvalid"));
    if (!QueueMarkerMaterial) QueueMarkerMaterial = LoadIfPresent<UMaterialInterface>(TEXT("/Game/Downstream/Materials/M_PrototypeRecovery"));
    // The editor and the saved map see the classic site so navigation can be baked.
    // Seeded worlds are generated at runtime only.
    if ((GetWorld() && GetWorld()->IsGameWorld()) || SiteKind != EDownstreamSiteKind::Classic) return;
    // Fixed, validated site configuration: construction cannot fail here.
    downstream::WaterGrid Preview(downstream::DrownedVaultConfig());
    downstream::BuildDrownedVaultSite(Preview, true);
    SetupChunks(Preview);
    BuildAllTerrain(Preview, true);
    UpdateWater(Preview, true);
    PlaceDecor(Preview);
}

void ADownstreamWaterSite::BeginPlay()
{
    Super::BeginPlay();
    auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    PlanMarkers->SetStaticMesh(Cube);
    QueueMarkers->SetStaticMesh(Cube);
    if (QueueMarkerMaterial) QueueMarkers->SetMaterial(0, QueueMarkerMaterial);
    auto* Water = UDownstreamWaterSubsystem::GetForWorld(this);
    if (!Water) return;
    switch (SiteKind)
    {
    case EDownstreamSiteKind::Classic: bActive = Water->ActivateSite(GetActorLocation()); break;
    case EDownstreamSiteKind::World: bActive = Water->ActivateWorld(0, false); break;
    case EDownstreamSiteKind::Preview: bActive = Water->ActivateWorld(PreviewSeed, true); break;
    }
    if (!bActive || !Water->GetGrid()) return;
    if (SiteKind != EDownstreamSiteKind::Classic) SetActorLocation(Water->GetSiteOrigin());
    const auto& Grid = *Water->GetGrid();
    SetupChunks(Grid);
    BuildAllTerrain(Grid, true);
    UpdateWater(Grid, true);
    if (SiteKind == EDownstreamSiteKind::Classic) PlaceDecor(Grid);
    else
    {
        // Classic-only set dressing stays hidden in generated worlds.
        for (USceneComponent* Decor : TArray<USceneComponent*>{SpringPipe.Get(), CulvertPipe.Get(), WaterTower.Get(), CulvertSign.Get(), CulvertLabel.Get(), SpringLabel.Get()})
            if (Decor) Decor->SetVisibility(false);
        WaterTower->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if (SiteKind == EDownstreamSiteKind::World) SpawnBoundaries(Grid);
    }
    LastGroundRevision = Grid.GroundRevision();
    LastWaterTick = Grid.TickCount();
    // Generated worlds have no baked navmesh for their terrain; build it once everything has
    // registered (the menu travels here without the launcher's RebuildNavigation command).
    bNavigationRebuildPending = SiteKind == EDownstreamSiteKind::World;
    // The renderer is the only consumer of the ground dirty set.
    const_cast<downstream::WaterGrid&>(Grid).ConsumeDirtyGroundChunks();
    BindInput();
}

void ADownstreamWaterSite::SetupChunks(const downstream::WaterGrid& Grid)
{
    // Small classic site: one chunk (identical to the original single mesh). Worlds: 16x16 cells.
    ChunkCells = (Grid.Width() <= 48 && Grid.Height() <= 48) ? FMath::Max(Grid.Width(), Grid.Height()) : 16;
    ChunkColumns = (Grid.Width() + ChunkCells - 1) / ChunkCells;
    ChunkRows = (Grid.Height() + ChunkCells - 1) / ChunkCells;
    const int32 Count = ChunkColumns * ChunkRows;
    for (int32 Index = 1; Index < TerrainChunks.Num(); ++Index)
        if (TerrainChunks[Index]) TerrainChunks[Index]->DestroyComponent();
    for (int32 Index = 1; Index < WaterChunks.Num(); ++Index)
        if (WaterChunks[Index]) WaterChunks[Index]->DestroyComponent();
    TerrainChunks.Reset();
    WaterChunks.Reset();
    TerrainChunks.Add(TerrainMesh);
    WaterChunks.Add(WaterMesh);
    for (int32 Index = 1; Index < Count; ++Index)
    {
        auto* Terrain = NewObject<UDownstreamTerrainMeshComponent>(this, NAME_None, RF_Transient);
        Terrain->SetupAttachment(TerrainMesh);
        Terrain->bUseComplexAsSimpleCollision = true;
        Terrain->SetCollisionProfileName(TEXT("BlockAll"));
        Terrain->SetMobility(EComponentMobility::Movable);
        Terrain->RegisterComponent();
        TerrainChunks.Add(Terrain);
        auto* WaterChunk = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
        WaterChunk->SetupAttachment(TerrainMesh);
        WaterChunk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        WaterChunk->SetCanEverAffectNavigation(false);
        WaterChunk->SetCastShadow(false);
        WaterChunk->RegisterComponent();
        WaterChunks.Add(WaterChunk);
    }
    WaterChunkWasWet.Init(1, Count);
}

float ADownstreamWaterSite::BaseGroundAt(int32 X, int32 Y) const
{
    if (const auto* Water = UDownstreamWaterSubsystem::GetForWorld(const_cast<ADownstreamWaterSite*>(this)))
        if (Water->IsSiteActive() && !Water->GetLayout().Ground.empty()) return Water->GetLayout().GroundAt(X, Y);
    return downstream::DrownedVaultBaseGround(X, Y);
}

void ADownstreamWaterSite::BindInput()
{
    if (bInputBound || !GetWorld() || SiteKind == EDownstreamSiteKind::Preview) return;
    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    if (!Controller || !Controller->IsLocalController()) return;
    EnableInput(Controller);
    if (!InputComponent) return;
    InputComponent->BindKey(EKeys::F9, IE_Pressed, this, &ADownstreamWaterSite::ToggleOverlay);
    bInputBound = true;
}

void ADownstreamWaterSite::ToggleOverlay()
{
    if (auto* Water = UDownstreamWaterSubsystem::GetForWorld(this)) Water->ToggleFlowOverlay();
}

void ADownstreamWaterSite::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bInputBound) BindInput();
    auto* Water = UDownstreamWaterSubsystem::GetForWorld(this);
    if (!bActive || !Water || !Water->GetGrid()) return;
    if (bNavigationRebuildPending && GEngine)
    {
        bNavigationRebuildPending = false;
        GEngine->Exec(GetWorld(), TEXT("RebuildNavigation"));
    }
    auto& Grid = const_cast<downstream::WaterGrid&>(*Water->GetGrid());
    if (Grid.GroundRevision() != LastGroundRevision)
    {
        LastGroundRevision = Grid.GroundRevision();
        TSet<int32> Rebuilt;
        for (const auto& Dirty : Grid.ConsumeDirtyGroundChunks())
        {
            // Grid chunks are 16 cells; render chunks may be larger (classic site).
            const int32 CX = FMath::Clamp(Dirty.X * Grid.Config().ChunkSize / ChunkCells, 0, ChunkColumns - 1);
            const int32 CY = FMath::Clamp(Dirty.Y * Grid.Config().ChunkSize / ChunkCells, 0, ChunkRows - 1);
            const int32 Key = CY * ChunkColumns + CX;
            if (Rebuilt.Contains(Key)) continue;
            Rebuilt.Add(Key);
            BuildTerrainChunk(Grid, CX, CY, true);
        }
        ++TerrainRebuilds;
    }
    if (Grid.TickCount() != LastWaterTick)
    {
        LastWaterTick = Grid.TickCount();
        UpdateWater(Grid, false);
    }
    UpdateQueueMarkers();
    UpdatePlanPreview();
    if (Water->IsFlowOverlayEnabled()) DrawFlowOverlay(Grid);
    UpdateWaterAudio(Grid, DeltaSeconds);
}

void ADownstreamWaterSite::UpdateWaterAudio(const downstream::WaterGrid& Grid, float DeltaSeconds)
{
    const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld());
    if (!Tuning || !Tuning->WaterLoopSound || SiteKind == EDownstreamSiteKind::Preview) return;
    const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    if (!Pawn) return;
    if (!WaterAudio)
    {
        WaterAudio = NewObject<UAudioComponent>(this, TEXT("WaterAudio"));
        WaterAudio->SetupAttachment(GetRootComponent());
        WaterAudio->bAutoActivate = false;
        WaterAudio->bAllowSpatialization = false;
        WaterAudio->SetSound(Tuning->WaterLoopSound);
        WaterAudio->RegisterComponent();
        WaterAudio->SetVolumeMultiplier(0.001f);
        WaterAudio->Play();
    }
    WaterAudioTimer -= DeltaSeconds;
    if (WaterAudioTimer <= 0.0f)
    {
        WaterAudioTimer = 0.25f;
        const float S = Grid.Config().CellSize * MetresToCm;
        const FVector Local = GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation());
        const int32 PX = FMath::FloorToInt(Local.X / S), PY = FMath::FloorToInt(Local.Y / S);
        constexpr int32 Reach = 9;
        float Loudness = 0.0f;
        for (int32 Y = FMath::Max(0, PY - Reach); Y <= FMath::Min(Grid.Height() - 1, PY + Reach); ++Y)
            for (int32 X = FMath::Max(0, PX - Reach); X <= FMath::Min(Grid.Width() - 1, PX + Reach); ++X)
            {
                const float Depth = Grid.Water(X, Y);
                if (Depth < 0.03f) continue;
                const auto Flow = Grid.Flow(X, Y);
                const float Speed = FMath::Sqrt(Flow.X * Flow.X + Flow.Y * Flow.Y);
                const float Falloff = 1.0f / (1.0f + 0.15f * float((X - PX) * (X - PX) + (Y - PY) * (Y - PY)));
                Loudness += (FMath::Min(Speed, 2.0f) * FMath::Min(Depth, 1.0f) + 0.02f * FMath::Min(Depth, 1.0f)) * Falloff;
            }
        WaterAudioVolume = FMath::Clamp(Loudness * 0.6f, 0.0f, 1.0f);
    }
    const float Current = WaterAudio->VolumeMultiplier;
    WaterAudio->SetVolumeMultiplier(FMath::Max(0.001f, FMath::FInterpTo(Current, WaterAudioVolume, DeltaSeconds, 2.0f)));
}

void ADownstreamWaterSite::UpdatePlanPreview()
{
    auto* Water = UDownstreamWaterSubsystem::GetForWorld(this);
    APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    const auto* Player = Controller ? Cast<ADownstreamPlayerCharacter>(Controller->GetPawn()) : nullptr;
    TArray<FIntPoint> Cells;
    bool bValid = false;
    FIntPoint Cursor;
    if (Water && Player && Player->IsDigPlannerOpen() && Player->GetCanalCursorCell(Cursor))
    {
        if (Player->IsDraggingCanal())
        {
            Cells = Water->PreviewCanal(Player->GetCanalDragStart(), Cursor);
            bValid = Cells.Num() >= 2 && Water->IsCanalValid(Player->GetCanalDragStart(), Cursor);
        }
        else
        {
            Cells.Add(Cursor);
            bValid = true;
        }
    }
    FString Signature = bValid ? TEXT("v") : TEXT("x");
    for (const FIntPoint& Cell : Cells) Signature += FString::Printf(TEXT("%d,%d;"), Cell.X, Cell.Y);
    if (Water && Water->GetGrid()) Signature += FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(Water->GetGrid()->TickCount() / 5));
    if (Signature == LastPlanSignature) return;
    LastPlanSignature = Signature;
    if (Cells.Num() == 0) ClearCanalPlan();
    else ShowCanalPlan(Cells, bValid);
}

void ADownstreamWaterSite::BuildAllTerrain(const downstream::WaterGrid& Grid, bool bCollisionAndNavigation)
{
    for (int32 CY = 0; CY < ChunkRows; ++CY)
        for (int32 CX = 0; CX < ChunkColumns; ++CX)
            BuildTerrainChunk(Grid, CX, CY, bCollisionAndNavigation);
    ++TerrainRebuilds;
}

void ADownstreamWaterSite::BuildTerrainChunk(const downstream::WaterGrid& Grid, int32 ChunkX, int32 ChunkY, bool bCollisionAndNavigation)
{
    const int32 Index = ChunkY * ChunkColumns + ChunkX;
    if (!TerrainChunks.IsValidIndex(Index) || !TerrainChunks[Index]) return;
    UDownstreamTerrainMeshComponent* Mesh = TerrainChunks[Index];
    const int32 W = Grid.Width(), H = Grid.Height();
    const float S = Grid.Config().CellSize * MetresToCm;
    // Global vertex grid: cell centres plus one skirt ring (index 0 and W+1/H+1).
    const int32 VW = W + 2, VH = H + 2;
    const int32 I0 = ChunkX == 0 ? 0 : ChunkX * ChunkCells + 1;
    const int32 I1 = FMath::Min((ChunkX + 1) * ChunkCells + 1, VW - 1);
    const int32 J0 = ChunkY == 0 ? 0 : ChunkY * ChunkCells + 1;
    const int32 J1 = FMath::Min((ChunkY + 1) * ChunkCells + 1, VH - 1);
    const int32 CW = I1 - I0 + 1, CH = J1 - J0 + 1;
    if (CW < 2 || CH < 2) return;
    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    TArray<int32> Triangles;
    Vertices.Reserve(CW * CH);
    auto HeightAt = [&](int32 GX, int32 GY) -> float
    {
        const int32 X = GX - 1, Y = GY - 1;
        return (X >= 0 && Y >= 0 && X < W && Y < H) ? Grid.Ground(X, Y) * MetresToCm : SkirtZ;
    };
    for (int32 J = J0; J <= J1; ++J)
        for (int32 I = I0; I <= I1; ++I)
        {
            const int32 X = I - 1, Y = J - 1;
            const float PX = X < 0 ? 0.0f : X >= W ? W * S : (X + 0.5f) * S;
            const float PY = Y < 0 ? 0.0f : Y >= H ? H * S : (Y + 0.5f) * S;
            const bool bInside = X >= 0 && Y >= 0 && X < W && Y < H;
            const float Height = bInside ? Grid.Ground(X, Y) : 0.0f;
            Vertices.Add(FVector(PX, PY, HeightAt(I, J)));
            // UV0 carries data for the ground materials: x = metres dug (-) or piled (+) versus
            // the generated base, y = ground height. Vertex colours are kept for other materials.
            const float Base = bInside ? BaseGroundAt(X, Y) : 0.0f;
            UVs.Add(FVector2D(bInside ? Height - Base : 0.0f, bInside ? Height : 0.0f));
            Colors.Add(bInside ? GroundColor(Height, Base) : FLinearColor(0.42f, 0.34f, 0.24f));
            // Central-difference normal from the global grid keeps chunk seams smooth.
            const float L = HeightAt(FMath::Max(I - 1, 0), J), R = HeightAt(FMath::Min(I + 1, VW - 1), J);
            const float D = HeightAt(I, FMath::Max(J - 1, 0)), U = HeightAt(I, FMath::Min(J + 1, VH - 1));
            const FVector N = FVector(L - R, D - U, 2.0f * S).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
            Normals.Add(N);
            Tangents.Add(FProcMeshTangent((FVector::ForwardVector - N * N.X).GetSafeNormal(), false));
        }
    Triangles.Reserve((CW - 1) * (CH - 1) * 6);
    for (int32 J = 0; J < CH - 1; ++J)
        for (int32 I = 0; I < CW - 1; ++I)
        {
            // Unreal front faces: A -> +Y -> +X when viewed from above.
            const int32 A = J * CW + I, B = A + 1, C = A + CW, D = C + 1;
            Triangles.Append({A, C, B, B, C, D});
        }
    Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, bCollisionAndNavigation);
    Mesh->SetNavigationGeometry(Vertices, Triangles);
    if (TerrainMaterial) Mesh->SetMaterial(0, TerrainMaterial);
    // Dirty only this chunk's bounds for navigation instead of a global rebuild.
    if (bCollisionAndNavigation && GetWorld() && GetWorld()->IsGameWorld() && SiteKind != EDownstreamSiteKind::Preview)
        FNavigationSystem::UpdateComponentData(*Mesh);
}

void ADownstreamWaterSite::UpdateWater(const downstream::WaterGrid& Grid, bool bForce)
{
    for (int32 CY = 0; CY < ChunkRows; ++CY)
        for (int32 CX = 0; CX < ChunkColumns; ++CX)
            BuildWaterChunk(Grid, CX, CY, bForce);
    ++WaterRebuilds;
}

void ADownstreamWaterSite::BuildWaterChunk(const downstream::WaterGrid& Grid, int32 ChunkX, int32 ChunkY, bool bForce)
{
    const int32 Index = ChunkY * ChunkColumns + ChunkX;
    if (!WaterChunks.IsValidIndex(Index) || !WaterChunks[Index]) return;
    UProceduralMeshComponent* Mesh = WaterChunks[Index];
    const int32 W = Grid.Width(), H = Grid.Height();
    const float S = Grid.Config().CellSize * MetresToCm;
    const int32 X0 = ChunkX * ChunkCells, Y0 = ChunkY * ChunkCells;
    const int32 X1 = FMath::Min(X0 + ChunkCells, W - 1), Y1 = FMath::Min(Y0 + ChunkCells, H - 1);
    const int32 CW = X1 - X0 + 1, CH = Y1 - Y0 + 1;
    if (CW < 2 || CH < 2) return;
    // Skip dry chunks that were already dry: most of a world has no water.
    bool bWet = false;
    for (int32 Y = Y0; Y <= Y1 && !bWet; ++Y)
        for (int32 X = X0; X <= X1 && !bWet; ++X)
            if (Grid.Water(X, Y) > 0.01f) bWet = true;
    if (!bForce && !bWet && !WaterChunkWasWet[Index]) return;
    WaterChunkWasWet[Index] = bWet ? 1 : 0;
    if (!bWet)
    {
        Mesh->ClearAllMeshSections();
        return;
    }
    TArray<FVector> Vertices;
    TArray<FLinearColor> Colors;
    TArray<FVector> Normals;
    TArray<FVector2D> DataUVs;
    Vertices.Reserve(CW * CH);
    Colors.Reserve(CW * CH);
    DataUVs.Reserve(CW * CH);
    Normals.Init(FVector::UpVector, CW * CH);
    const FLinearColor Shallow(0.32f, 0.62f, 0.62f);
    const FLinearColor Deep(0.04f, 0.17f, 0.32f);
    for (int32 Y = Y0; Y <= Y1; ++Y)
        for (int32 X = X0; X <= X1; ++X)
        {
            const float Depth = Grid.Water(X, Y);
            const bool bCellWet = Depth > 0.01f;
            float Surface = Grid.Ground(X, Y) + (bCellWet ? Depth : -0.08f);
            if (!bCellWet)
            {
                // A dry vertex beside water takes the neighbouring water level so edges meet the bank.
                for (const FIntPoint& Offset : {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)})
                {
                    const int32 NX = X + Offset.X, NY = Y + Offset.Y;
                    if (Grid.InBounds(NX, NY) && Grid.Water(NX, NY) > 0.01f)
                        Surface = FMath::Max(Surface, FMath::Min(Grid.Surface(NX, NY), Grid.Ground(X, Y) + 0.02f));
                }
            }
            Vertices.Add(FVector((X + 0.5f) * S, (Y + 0.5f) * S, Surface * MetresToCm + 2.0f));
            const auto Flow = Grid.Flow(X, Y);
            const float Speed = FMath::Sqrt(Flow.X * Flow.X + Flow.Y * Flow.Y);
            FLinearColor Color = FMath::Lerp(Shallow, Deep, FMath::Clamp(Depth / 1.4f, 0.0f, 1.0f));
            Color = FMath::Lerp(Color, FLinearColor(0.85f, 0.92f, 0.95f), FMath::Clamp(Speed * 0.35f, 0.0f, 0.45f));
            Color.A = bCellWet ? FMath::Clamp(0.45f + Depth * 0.5f, 0.45f, 0.88f) : 0.0f;
            Colors.Add(Color);
            // UV0 for M_Water: x = depth (m), y = flow speed (m/s).
            DataUVs.Add(FVector2D(bCellWet ? Depth : 0.0f, Speed));
        }
    if (Mesh->GetNumSections() == 0 || Mesh->GetProcMeshSection(0)->ProcVertexBuffer.Num() != Vertices.Num())
    {
        TArray<int32> Triangles;
        TArray<FProcMeshTangent> Tangents;
        for (int32 J = 0; J < CH - 1; ++J)
            for (int32 I = 0; I < CW - 1; ++I)
            {
                const int32 A = J * CW + I, B = A + 1, C = A + CW, D = C + 1;
                Triangles.Append({A, C, B, B, C, D});
            }
        for (int32 V = 0; V < Vertices.Num(); ++V) Tangents.Add(FProcMeshTangent(1, 0, 0));
        Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, DataUVs, Colors, Tangents, false);
        if (WaterMaterial) Mesh->SetMaterial(0, WaterMaterial);
    }
    else
    {
        Mesh->UpdateMeshSection_LinearColor(0, Vertices, Normals, DataUVs, Colors, TArray<FProcMeshTangent>());
    }
}

void ADownstreamWaterSite::SpawnBoundaries(const downstream::WaterGrid& Grid)
{
    const float S = Grid.Config().CellSize * MetresToCm;
    const float SX = Grid.Width() * S, SY = Grid.Height() * S;
    const FVector Centres[4] = {FVector(-60, SY * 0.5f, 400), FVector(SX + 60, SY * 0.5f, 400), FVector(SX * 0.5f, -60, 400), FVector(SX * 0.5f, SY + 60, 400)};
    const FVector Extents[4] = {FVector(60, SY * 0.5f + 120, 1200), FVector(60, SY * 0.5f + 120, 1200), FVector(SX * 0.5f + 120, 60, 1200), FVector(SX * 0.5f + 120, 60, 1200)};
    for (int32 Side = 0; Side < 4; ++Side)
    {
        auto* Box = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
        Box->SetupAttachment(TerrainMesh);
        Box->SetBoxExtent(Extents[Side]);
        Box->SetRelativeLocation(Centres[Side]);
        Box->SetCollisionProfileName(TEXT("BlockAll"));
        Box->SetCanEverAffectNavigation(false);
        Box->SetHiddenInGame(true);
        Box->RegisterComponent();
        Boundaries.Add(Box);
    }
}

int32 ADownstreamWaterSite::GetNavigationTriangleCount() const
{
    int32 Count = 0;
    for (const auto& Chunk : TerrainChunks) if (Chunk) Count += Chunk->GetNavigationTriangleCount();
    return Count == 0 && TerrainMesh ? TerrainMesh->GetNavigationTriangleCount() : Count;
}

void ADownstreamWaterSite::PlaceDecor(const downstream::WaterGrid& Grid)
{
    using Site = downstream::DrownedVaultSite;
    const float S = Grid.Config().CellSize * MetresToCm;
    auto* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto* Concrete = LoadIfPresent<UMaterialInterface>(TEXT("/Game/Downstream/Materials/M_SiteConcrete"));
    // CC0 Kenney Factory/Industrial Kit models (third_party/kenney-*), with primitive fallbacks.
    auto* Valve = LoadIfPresent<UStaticMesh>(TEXT("/Game/Downstream/ThirdParty/Kenney/Factory/pipe_large_valve/SM_K_pipe_large_valve"));
    auto* LongPipe = LoadIfPresent<UStaticMesh>(TEXT("/Game/Downstream/ThirdParty/Kenney/Factory/pipe_large_long/SM_K_pipe_large_long"));
    auto* Tower = LoadIfPresent<UStaticMesh>(TEXT("/Game/Downstream/ThirdParty/Kenney/Industrial/water_tower/SM_K_water_tower"));
    auto* Sign = LoadIfPresent<UStaticMesh>(TEXT("/Game/Downstream/ThirdParty/Kenney/Factory/warning_traffic/SM_K_warning_traffic"));
    const float SpringGround = Grid.Ground(Site::Spring.X, Site::Spring.Y) * MetresToCm;
    if (Valve)
    {
        SpringPipe->SetStaticMesh(Valve);
        SpringPipe->SetRelativeLocation(FVector((Site::Spring.X + 1.3f) * S, (Site::Spring.Y + 0.5f) * S, SpringGround - 15.0f));
        SpringPipe->SetRelativeRotation(FRotator::ZeroRotator);
        SpringPipe->SetRelativeScale3D(FVector(1.1f));
    }
    else
    {
        SpringPipe->SetStaticMesh(Cylinder);
        SpringPipe->SetRelativeLocation(FVector((Site::Spring.X + 1.2f) * S, (Site::Spring.Y + 0.5f) * S, SpringGround + 35.0f));
        SpringPipe->SetRelativeRotation(FRotator(0, 0, 90));
        SpringPipe->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.6f));
        if (Concrete) SpringPipe->SetMaterial(0, Concrete);
    }
    if (LongPipe)
    {
        // Half-buried culvert running out of the south edge of the site.
        CulvertPipe->SetStaticMesh(LongPipe);
        CulvertPipe->SetRelativeLocation(FVector((Site::CulvertA.X + 1.0f) * S, -60.0f, -45.0f));
        CulvertPipe->SetRelativeRotation(FRotator(0, 90, 0));
        CulvertPipe->SetRelativeScale3D(FVector(1.0f, 1.5f, 1.5f));
    }
    else
    {
        CulvertPipe->SetStaticMesh(Cylinder);
        CulvertPipe->SetRelativeLocation(FVector((Site::CulvertA.X + 1.0f) * S, -20.0f, 45.0f));
        CulvertPipe->SetRelativeRotation(FRotator(0, 0, 90));
        CulvertPipe->SetRelativeScale3D(FVector(1.9f, 1.9f, 0.9f));
        if (Concrete) CulvertPipe->SetMaterial(0, Concrete);
    }
    if (Tower)
    {
        WaterTower->SetStaticMesh(Tower);
        WaterTower->SetRelativeLocation(FVector(29.5f * S, 14.0f * S, Site::Plateau * MetresToCm));
        WaterTower->SetRelativeScale3D(FVector(3.2f));
    }
    if (Sign)
    {
        CulvertSign->SetStaticMesh(Sign);
        CulvertSign->SetRelativeLocation(FVector((Site::CulvertB.X + 2.6f) * S, 1.6f * S, 1.2f * MetresToCm));
        CulvertSign->SetRelativeScale3D(FVector(1.0f));
    }
    CulvertLabel->SetRelativeLocation(FVector((Site::CulvertA.X + 1.0f) * S, 60.0f, 330.0f));
    SpringLabel->SetRelativeLocation(FVector((Site::Spring.X + 0.5f) * S, (Site::Spring.Y + 0.5f) * S, 420.0f));
    CulvertLabel->SetWorldRotation(FRotator(55, -135, 0));
    SpringLabel->SetWorldRotation(FRotator(55, -135, 0));
}

void ADownstreamWaterSite::ShowCanalPlan(const TArray<FIntPoint>& Cells, bool bValid)
{
    auto* Water = UDownstreamWaterSubsystem::GetForWorld(this);
    PlanMarkers->ClearInstances();
    if (!Water || !Water->GetGrid()) return;
    if (UMaterialInterface* Material = bValid ? ValidMarkerMaterial.Get() : InvalidMarkerMaterial.Get()) PlanMarkers->SetMaterial(0, Material);
    const auto& Grid = *Water->GetGrid();
    const float S = Grid.Config().CellSize * MetresToCm;
    for (const FIntPoint& Cell : Cells)
    {
        if (!Grid.InBounds(Cell.X, Cell.Y)) continue;
        const float Z = FMath::Max(Grid.Ground(Cell.X, Cell.Y), Grid.Surface(Cell.X, Cell.Y)) * MetresToCm + 10.0f;
        PlanMarkers->AddInstance(FTransform(FRotator::ZeroRotator, FVector((Cell.X + 0.5f) * S, (Cell.Y + 0.5f) * S, Z), FVector(0.86f, 0.86f, 0.06f)));
    }
}

void ADownstreamWaterSite::ClearCanalPlan()
{
    PlanMarkers->ClearInstances();
}

void ADownstreamWaterSite::UpdateQueueMarkers()
{
    auto* Water = UDownstreamWaterSubsystem::GetForWorld(this);
    if (!Water || !Water->GetGrid()) return;
    const TArray<FIntPoint> Cells = Water->GetQueuedCanalCells();
    FString Signature;
    for (const FIntPoint& Cell : Cells) Signature += FString::Printf(TEXT("%d,%d;"), Cell.X, Cell.Y);
    Signature += FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(Water->GetGrid()->GroundRevision()));
    if (Signature == LastQueueSignature) return;
    LastQueueSignature = Signature;
    QueueMarkers->ClearInstances();
    const auto& Grid = *Water->GetGrid();
    const float S = Grid.Config().CellSize * MetresToCm;
    for (const FIntPoint& Cell : Cells)
    {
        const float Z = FMath::Max(Grid.Ground(Cell.X, Cell.Y), Grid.Surface(Cell.X, Cell.Y)) * MetresToCm + 6.0f;
        QueueMarkers->AddInstance(FTransform(FRotator::ZeroRotator, FVector((Cell.X + 0.5f) * S, (Cell.Y + 0.5f) * S, Z), FVector(0.35f, 0.35f, 0.04f)));
    }
}

void ADownstreamWaterSite::DrawFlowOverlay(const downstream::WaterGrid& Grid) const
{
    const float S = Grid.Config().CellSize * MetresToCm;
    const FTransform& Transform = GetActorTransform();
    // Limit the debug draw to about 25 m around the player in big worlds.
    int32 MinX = 0, MinY = 0, MaxX = Grid.Width() - 1, MaxY = Grid.Height() - 1;
    if (const APlayerController* Controller = GetWorld()->GetFirstPlayerController())
        if (const APawn* Pawn = Controller->GetPawn())
        {
            const FVector Local = Transform.InverseTransformPosition(Pawn->GetActorLocation());
            const int32 PX = FMath::FloorToInt(Local.X / S), PY = FMath::FloorToInt(Local.Y / S);
            MinX = FMath::Max(0, PX - 25); MaxX = FMath::Min(Grid.Width() - 1, PX + 25);
            MinY = FMath::Max(0, PY - 25); MaxY = FMath::Min(Grid.Height() - 1, PY + 25);
        }
    for (int32 Y = MinY; Y <= MaxY; ++Y)
        for (int32 X = MinX; X <= MaxX; ++X)
        {
            const float Depth = Grid.Water(X, Y);
            if (Depth < 0.02f) continue;
            const FVector Center = Transform.TransformPosition(FVector((X + 0.5f) * S, (Y + 0.5f) * S, Grid.Surface(X, Y) * MetresToCm + 12.0f));
            const auto Flow = Grid.Flow(X, Y);
            const FVector Dir(Flow.X, Flow.Y, 0.0f);
            const float Speed = Dir.Size();
            const FColor DepthColor = FLinearColor::LerpUsingHSV(FLinearColor(0.2f, 0.9f, 1.0f), FLinearColor(0.1f, 0.15f, 0.9f), FMath::Clamp(Depth / 1.4f, 0.0f, 1.0f)).ToFColor(true);
            if (Speed > 0.03f)
                DrawDebugDirectionalArrow(GetWorld(), Center, Center + Dir / Speed * FMath::Clamp(Speed * 90.0f, 18.0f, 48.0f), 14.0f, FColor::White, false, -1.0f, 0, 2.0f);
            DrawDebugPoint(GetWorld(), Center, 6.0f, DepthColor, false, -1.0f);
        }
}
