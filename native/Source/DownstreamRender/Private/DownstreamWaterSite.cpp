#include "DownstreamWaterSite.h"

#include "DownstreamDebugHUD.h"
#include "DownstreamPlayerCharacter.h"
#include "DownstreamTerrainMeshComponent.h"
#include "DownstreamEarthworks.h"
#include "DownstreamWater.h"
#include "DownstreamWaterSubsystem.h"
#include "AI/NavigationSystemBase.h"
#include "Components/InputComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
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
    // The editor and the saved map see the generated site so navigation can be baked.
    if (GetWorld() && GetWorld()->IsGameWorld()) return;
    // Fixed, validated site configuration: construction cannot fail here.
    downstream::WaterGrid Preview(downstream::DrownedVaultConfig());
    downstream::BuildDrownedVaultSite(Preview, true);
    BuildTerrain(Preview, true);
    BuildWater(Preview);
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
    bActive = Water && Water->ActivateSite(GetActorLocation());
    if (bActive)
    {
        const auto& Grid = *Water->GetGrid();
        BuildTerrain(Grid, true);
        BuildWater(Grid);
        PlaceDecor(Grid);
        LastGroundRevision = Grid.GroundRevision();
        LastWaterTick = Grid.TickCount();
    }
    BindInput();
}

void ADownstreamWaterSite::BindInput()
{
    if (bInputBound || !GetWorld()) return;
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
    const auto& Grid = *Water->GetGrid();
    if (Grid.GroundRevision() != LastGroundRevision)
    {
        LastGroundRevision = Grid.GroundRevision();
        BuildTerrain(Grid, true);
    }
    if (Grid.TickCount() != LastWaterTick)
    {
        LastWaterTick = Grid.TickCount();
        BuildWater(Grid);
    }
    UpdateQueueMarkers();
    UpdatePlanPreview();
    if (Water->IsFlowOverlayEnabled()) DrawFlowOverlay(Grid);
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

void ADownstreamWaterSite::BuildTerrain(const downstream::WaterGrid& Grid, bool bCollisionAndNavigation)
{
    const int32 W = Grid.Width(), H = Grid.Height();
    const float S = Grid.Config().CellSize * MetresToCm;
    // Vertices at cell centres plus one outer ring that skirts down to the surrounding ground.
    const int32 VW = W + 2, VH = H + 2;
    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    TArray<int32> Triangles;
    Vertices.Reserve(VW * VH);
    Colors.Reserve(VW * VH);
    UVs.Reserve(VW * VH);
    for (int32 J = 0; J < VH; ++J)
        for (int32 I = 0; I < VW; ++I)
        {
            const int32 X = I - 1, Y = J - 1;
            const float PX = X < 0 ? 0.0f : X >= W ? W * S : (X + 0.5f) * S;
            const float PY = Y < 0 ? 0.0f : Y >= H ? H * S : (Y + 0.5f) * S;
            const bool bInside = X >= 0 && Y >= 0 && X < W && Y < H;
            const float Height = bInside ? Grid.Ground(X, Y) : 0.0f;
            Vertices.Add(FVector(PX, PY, bInside ? Height * MetresToCm : SkirtZ));
            // UV0 carries data for M_SiteGround: x = metres dug (-) or piled (+) versus the
            // generated site, y = ground height. Vertex colours are kept for other materials.
            UVs.Add(FVector2D(bInside ? Height - downstream::DrownedVaultBaseGround(X, Y) : 0.0f, bInside ? Height : 0.0f));
            Colors.Add(bInside ? GroundColor(Height, downstream::DrownedVaultBaseGround(X, Y)) : FLinearColor(0.42f, 0.34f, 0.24f));
        }
    Normals.Init(FVector::ZeroVector, Vertices.Num());
    Triangles.Reserve((VW - 1) * (VH - 1) * 6);
    auto AddTriangle = [&](int32 A, int32 B, int32 C)
    {
        Triangles.Append({A, B, C});
        const FVector N = FVector::CrossProduct(Vertices[B] - Vertices[A], Vertices[C] - Vertices[A]);
        Normals[A] += N; Normals[B] += N; Normals[C] += N;
    };
    for (int32 J = 0; J < VH - 1; ++J)
        for (int32 I = 0; I < VW - 1; ++I)
        {
            const int32 A = J * VW + I, B = A + 1, C = A + VW, D = C + 1;
            // Unreal front faces: A -> +Y -> +X when viewed from above.
            AddTriangle(A, C, B);
            AddTriangle(B, C, D);
        }
    Tangents.Reserve(Normals.Num());
    for (FVector& N : Normals)
    {
        N = N.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
        if (N.Z < 0.0f) N = -N;
        Tangents.Add(FProcMeshTangent((FVector::ForwardVector - N * N.X).GetSafeNormal(), false));
    }
    TerrainMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, bCollisionAndNavigation);
    TerrainMesh->SetNavigationGeometry(Vertices, Triangles);
    if (TerrainMaterial) TerrainMesh->SetMaterial(0, TerrainMaterial);
    ++TerrainRebuilds;
    // Dirty only this component's bounds for navigation instead of a global rebuild.
    if (bCollisionAndNavigation && GetWorld() && GetWorld()->IsGameWorld())
        FNavigationSystem::UpdateComponentData(*TerrainMesh);
}

void ADownstreamWaterSite::BuildWater(const downstream::WaterGrid& Grid)
{
    const int32 W = Grid.Width(), H = Grid.Height();
    const float S = Grid.Config().CellSize * MetresToCm;
    TArray<FVector> Vertices;
    TArray<FLinearColor> Colors;
    TArray<FVector> Normals;
    TArray<FVector2D> DataUVs;
    Vertices.Reserve(W * H);
    Colors.Reserve(W * H);
    DataUVs.Reserve(W * H);
    Normals.Init(FVector::UpVector, W * H);
    const FLinearColor Shallow(0.32f, 0.62f, 0.62f);
    const FLinearColor Deep(0.04f, 0.17f, 0.32f);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const float Depth = Grid.Water(X, Y);
            const bool bWet = Depth > 0.01f;
            float Surface = Grid.Ground(X, Y) + (bWet ? Depth : -0.08f);
            if (!bWet)
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
            Color.A = bWet ? FMath::Clamp(0.45f + Depth * 0.5f, 0.45f, 0.88f) : 0.0f;
            Colors.Add(Color);
            // UV0 for M_Water: x = depth (m), y = flow speed (m/s).
            DataUVs.Add(FVector2D(bWet ? Depth : 0.0f, Speed));
        }
    if (WaterMesh->GetNumSections() == 0 || WaterMesh->GetProcMeshSection(0)->ProcVertexBuffer.Num() != Vertices.Num())
    {
        TArray<int32> Triangles;
        TArray<FProcMeshTangent> Tangents;
        for (int32 Y = 0; Y < H - 1; ++Y)
            for (int32 X = 0; X < W - 1; ++X)
            {
                const int32 A = Y * W + X, B = A + 1, C = A + W, D = C + 1;
                Triangles.Append({A, C, B, B, C, D});
            }
        for (int32 Index = 0; Index < Vertices.Num(); ++Index) Tangents.Add(FProcMeshTangent(1, 0, 0));
        WaterMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, DataUVs, Colors, Tangents, false);
        if (WaterMaterial) WaterMesh->SetMaterial(0, WaterMaterial);
    }
    else
    {
        WaterMesh->UpdateMeshSection_LinearColor(0, Vertices, Normals, DataUVs, Colors, TArray<FProcMeshTangent>());
    }
    ++WaterRebuilds;
}

int32 ADownstreamWaterSite::GetNavigationTriangleCount() const
{
    return TerrainMesh ? TerrainMesh->GetNavigationTriangleCount() : 0;
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
    for (int32 Y = 0; Y < Grid.Height(); ++Y)
        for (int32 X = 0; X < Grid.Width(); ++X)
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
