#include "DownstreamSettlementManager.h"

#include "DownstreamHauler.h"
#include "DownstreamDigger.h"
#include "DownstreamPrototypeTuning.h"
#include "DownstreamSettlementBuilding.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "NavigationSystem.h"

namespace
{
UMaterialInterface* LoadPreviewMaterial(const TCHAR* Package)
{
    return FPackageName::DoesPackageExist(Package)
        ? LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s.%s"), Package, *FPackageName::GetShortName(Package))) : nullptr;
}
}

ADownstreamSettlementManager::ADownstreamSettlementManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.5f;
    auto* Root = CreateDefaultSubobject<USceneComponent>(TEXT("SettlementRoot"));
    SetRootComponent(Root);
    PlacementPreview = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlacementPreview"));
    PlacementPreview->SetupAttachment(Root);
    PlacementPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PlacementPreview->SetCanEverAffectNavigation(false);
    PlacementPreview->SetCastShadow(false);
    PlacementPreview->SetRelativeScale3D(FVector(1.8f, 1.8f, 0.06f));
    PlacementPreview->SetVisibility(false);
    PreviewLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PreviewLabel"));
    PreviewLabel->SetupAttachment(Root);
    PreviewLabel->SetHorizontalAlignment(EHTA_Center);
    PreviewLabel->SetWorldSize(23.0f);
    PreviewLabel->SetRelativeRotation(FRotator(55.0f, -135.0f, 0.0f));
    PreviewLabel->SetVisibility(false);
    SetActorEnableCollision(false);
}

ADownstreamSettlementManager* ADownstreamSettlementManager::GetForWorld(UObject* ContextObject)
{
    UWorld* World = ContextObject ? ContextObject->GetWorld() : nullptr;
    if (!World) return nullptr;
    for (TActorIterator<ADownstreamSettlementManager> It(World); It; ++It)
        if (IsValid(*It) && !It->IsActorBeingDestroyed()) return *It;
    return nullptr;
}

void ADownstreamSettlementManager::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Existing = GetForWorld(this); Existing && Existing != this)
    {
        Destroy();
        return;
    }
    PlacementPreview->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    ValidPreviewMaterial = LoadPreviewMaterial(TEXT("/Game/Downstream/Materials/M_PlacementValid"));
    InvalidPreviewMaterial = LoadPreviewMaterial(TEXT("/Game/Downstream/Materials/M_PlacementInvalid"));
    BindSubsystem();
    SyncSettlement();
}

bool ADownstreamSettlementManager::BindSubsystem()
{
    if (IsValid(Expedition)) return true;
    Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this);
    if (!Expedition) return false;
    Expedition->OnSettlementChanged.AddDynamic(this, &ADownstreamSettlementManager::SyncSettlement);
    return true;
}

void ADownstreamSettlementManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (BindSubsystem()) SyncSettlement();
}

void ADownstreamSettlementManager::SyncSettlement()
{
    if (bSyncing || !BindSubsystem() || !GetWorld() || IsActorBeingDestroyed()) return;
    TGuardValue<bool> SyncGuard(bSyncing, true);
    Buildings.RemoveAll([](const TObjectPtr<ADownstreamSettlementBuilding>& Building) { return !IsValid(Building); });
    for (const FDownstreamStructureRecord& Record : Expedition->GetBuiltStructures())
    {
        if (GetBuilding(Record.Type)) continue;
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.ObjectFlags |= RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Params.bDeferConstruction = true;
        if (auto* Building = GetWorld()->SpawnActor<ADownstreamSettlementBuilding>(ADownstreamSettlementBuilding::StaticClass(), Record.WorldLocation, FRotator::ZeroRotator, Params))
        {
            Building->InitializeStructure(Record.Type);
            Building->FinishSpawning(FTransform(FRotator::ZeroRotator, Record.WorldLocation));
            Buildings.Add(Building);
        }
    }
    if (Expedition->IsHaulerConstructed() && !IsValid(Hauler))
    {
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.ObjectFlags |= RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
        Hauler = GetWorld()->SpawnActor<ADownstreamHauler>(ADownstreamHauler::StaticClass(), GetHaulerSpawnLocation(), FRotator::ZeroRotator, Params);
    }
    if (Expedition->IsDiggerBuilt() && !IsValid(Digger))
    {
        FVector Base = Expedition->GetBuildGridCenter();
        if (auto* Workshop = GetBuilding(EDownstreamStructureType::Workshop)) Base = Workshop->GetActorLocation();
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.ObjectFlags |= RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        Digger = GetWorld()->SpawnActor<ADownstreamDigger>(ADownstreamDigger::StaticClass(), Base + FVector(0, 170, 70), FRotator::ZeroRotator, Params);
    }
}

FText ADownstreamSettlementManager::GetDiggerStatus() const
{
    return IsValid(Digger) ? Digger->GetStatus()
        : FText::FromString(Expedition && Expedition->IsDiggerBuilt() ? TEXT("Starting") : TEXT("Not assembled"));
}

ADownstreamSettlementBuilding* ADownstreamSettlementManager::GetBuilding(EDownstreamStructureType Type) const
{
    for (const auto& Building : Buildings)
        if (IsValid(Building) && Building->StructureType == Type) return Building;
    return nullptr;
}

FVector ADownstreamSettlementManager::GetHaulerSpawnLocation() const
{
    FVector Base = Expedition ? Expedition->GetBuildGridCenter() : UDownstreamExpeditionSubsystem::GetTuning(GetWorld())->BuildGridCenter;
    if (auto* Storage = GetBuilding(EDownstreamStructureType::Storage)) Base = Storage->GetActorLocation();
    // A cardinal dock avoids the box's nav expansion and is within the core unload radius
    // for restored cargo. Never spawn a character capsule inside the building itself.
    static const FVector Offsets[] = { FVector(0,-145,0), FVector(145,0,0), FVector(0,145,0), FVector(-145,0,0) };
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    for (const FVector& Offset : Offsets)
    {
        FNavLocation Projected;
        FVector Candidate = Base + Offset;
        if (Nav && Nav->ProjectPointToNavigation(Candidate, Projected, FVector(20,20,250))) Candidate = Projected.Location;
        else if (Nav) continue;
        Candidate.Z += 60.0f;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(DownstreamHaulerSpawn), false, this);
        if (!GetWorld()->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_Pawn,
            FCollisionShape::MakeCapsule(34.0f, 58.0f), Query)) return Candidate;
    }
    return Base + FVector(0,-210,60);
}

void ADownstreamSettlementManager::SetPlacementPreview(EDownstreamStructureType Type, FVector WorldLocation, bool bVisible)
{
    if (!bVisible || !BindSubsystem()) { ClearPlacementPreview(); return; }
    const FVector Snapped = Expedition->GetSnappedBuildLocation(WorldLocation);
    const bool bValid = Expedition->CanPlaceStructure(Type, WorldLocation);
    PlacementPreview->SetWorldLocation(Snapped + FVector(0,0,4));
    PlacementPreview->SetVisibility(true);
    if (auto* Material = bValid ? ValidPreviewMaterial.Get() : InvalidPreviewMaterial.Get()) PlacementPreview->SetMaterial(0, Material);
    PreviewLabel->SetWorldLocation(Snapped + FVector(0,0,230));
    PreviewLabel->SetWorldRotation(FRotator(55,-135,0));
    PreviewLabel->SetText(FText::FromString(bValid ? TEXT("PLACE") : TEXT("BLOCKED")));
    PreviewLabel->SetTextRenderColor(bValid ? FColor(94,236,151) : FColor(248,92,83));
    PreviewLabel->SetVisibility(true);
}

void ADownstreamSettlementManager::ClearPlacementPreview()
{
    PlacementPreview->SetVisibility(false);
    PreviewLabel->SetVisibility(false);
}

FText ADownstreamSettlementManager::GetHaulerStatus() const
{
    return IsValid(Hauler) ? Hauler->GetStatus()
        : FText::FromString(Expedition && Expedition->IsHaulerConstructed() ? TEXT("Blocked") : TEXT("Not constructed"));
}

void ADownstreamSettlementManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(Expedition)) Expedition->OnSettlementChanged.RemoveDynamic(this, &ADownstreamSettlementManager::SyncSettlement);
    if (IsValid(Hauler)) Hauler->Destroy();
    if (IsValid(Digger)) Digger->Destroy();
    for (auto& Building : Buildings) if (IsValid(Building)) Building->Destroy();
    Super::EndPlay(EndPlayReason);
}
