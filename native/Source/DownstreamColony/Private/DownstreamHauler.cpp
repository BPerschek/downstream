#include "DownstreamHauler.h"

#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamHaulerController.h"
#include "DownstreamPrototypeTuning.h"
#include "DownstreamSalvagePickup.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogDownstreamHauler, Log, All);

namespace
{
constexpr float PickupRange = 120.0f;
constexpr float StorageRange = 150.0f;
constexpr float ChargeRange = 220.0f;
constexpr double DecisionInterval = 0.4;
constexpr double JobBackoff = 15.0;
constexpr float MaxJobDistance = 6000.0f;

UStaticMesh* LoadHaulerMesh(const TCHAR* Package, const TCHAR* Fallback)
{
    if (FPackageName::DoesPackageExist(Package))
        if (auto* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), Package, *FPackageName::GetShortName(Package)))) return Mesh;
    return LoadObject<UStaticMesh>(nullptr, Fallback);
}
}

ADownstreamHauler::ADownstreamHauler()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.05f;
    Tags.AddUnique(TEXT("DownstreamHauler"));
    GetCapsuleComponent()->InitCapsuleSize(34.0f, 58.0f);
    GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
    GetCapsuleComponent()->SetCanEverAffectNavigation(false);
    SetCanAffectNavigationGeneration(false);
    GetMesh()->SetVisibility(false);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetCanEverAffectNavigation(false);
    GetCharacterMovement()->MaxWalkSpeed = 330.0f;
    GetCharacterMovement()->MaxAcceleration = 1200.0f;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0,360,0);
    GetCharacterMovement()->GetNavAgentPropertiesRef().AgentRadius = 34.0f;
    GetCharacterMovement()->GetNavAgentPropertiesRef().AgentHeight = 116.0f;
    bUseControllerRotationYaw = false;
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = ADownstreamHaulerController::StaticClass();

    BotMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BotMesh"));
    BotMesh->SetupAttachment(GetRootComponent());
    BotMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BotMesh->SetCanEverAffectNavigation(false);
    CargoMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CargoMesh"));
    CargoMesh->SetupAttachment(GetRootComponent());
    CargoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CargoMesh->SetCanEverAffectNavigation(false);
    CargoMesh->SetRelativeLocation(FVector(-14,0,57));
    CargoMesh->SetVisibility(false);
    StatusCore = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatusCore"));
    StatusCore->SetupAttachment(GetRootComponent());
    StatusCore->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    StatusCore->SetCanEverAffectNavigation(false);
    StatusCore->SetRelativeLocation(FVector(0,0,37));
    StatusCore->SetRelativeScale3D(FVector(0.23f));
    StatusCore->SetCastShadow(false);
    StatusGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusGlow"));
    StatusGlow->SetupAttachment(GetRootComponent());
    StatusGlow->SetRelativeLocation(FVector(0,0,55));
    StatusGlow->SetIntensity(180.0f);
    StatusGlow->SetAttenuationRadius(170.0f);
    StatusGlow->SetCastShadows(false);
    WorldLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("WorldLabel"));
    WorldLabel->SetupAttachment(GetRootComponent());
    WorldLabel->SetRelativeLocation(FVector(0,0,125));
    WorldLabel->SetRelativeRotation(FRotator(55,-135,0));
    WorldLabel->SetHorizontalAlignment(EHTA_Center);
    WorldLabel->SetWorldSize(17.0f);
    SetStatus(TEXT("Idle"));
}

void ADownstreamHauler::BeginPlay()
{
    Super::BeginPlay();
    Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this);
    Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld());
    GetCharacterMovement()->MaxWalkSpeed = FMath::Max(1.0f, Tuning->HaulerMoveSpeed);
    if (!GetController()) SpawnDefaultController();

    UStaticMesh* Robot = LoadHaulerMesh(TEXT("/Game/Downstream/Art/SM_SalvageBot"), TEXT("/Engine/BasicShapes/Cube.Cube"));
    BotMesh->SetStaticMesh(Robot);
    if (Robot)
    {
        const auto Bounds = Robot->GetBounds();
        const FVector Size = Bounds.BoxExtent * 2.0f;
        const float Scale = FMath::Min(85.0f / FMath::Max(1.0f, FMath::Max(Size.X, Size.Y)), 108.0f / FMath::Max(1.0f, Size.Z));
        BotMesh->SetRelativeScale3D(FVector(Scale));
        BotMesh->SetRelativeRotation(FRotator(0,90,0));
        BotMesh->SetRelativeLocation(FVector(Bounds.Origin.Y * Scale, -Bounds.Origin.X * Scale,
            -58.0f - (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale));
    }
    UStaticMesh* Crate = LoadHaulerMesh(TEXT("/Game/Downstream/Art/SM_Crate"), TEXT("/Engine/BasicShapes/Cube.Cube"));
    CargoMesh->SetStaticMesh(Crate);
    if (Crate)
    {
        const FVector Size = Crate->GetBounds().BoxExtent * 2.0f;
        CargoMesh->SetRelativeScale3D(FVector(30.0f / FMath::Max(1.0f, Size.GetMax())));
    }
    StatusCore->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
    if (FPackageName::DoesPackageExist(TEXT("/Game/Downstream/Materials/M_PrototypeFriendly")))
    {
        if (auto* Friendly = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Downstream/Materials/M_PrototypeFriendly.M_PrototypeFriendly")))
        {
            for (int32 Index = 0; Index < BotMesh->GetNumMaterials(); ++Index) BotMesh->SetMaterial(Index, Friendly);
            StatusCore->SetMaterial(0, Friendly);
            StatusCore->CreateAndSetMaterialInstanceDynamic(0);
        }
    }
    ProgressLocation = GetActorLocation();
    LastProgressTime = GetWorld()->GetTimeSeconds();
    NextCheckpoint = LastProgressTime + 5.0;
    LastCheckpointBattery = Expedition ? Expedition->GetHaulerBattery() : -1.0f;
    bLastHaulingEnabled = Expedition && Expedition->IsHaulingEnabled();
    // Restored cargo is already durable. Deposit from the home spawn once, then use the
    // usual movement/transaction flow if spawning was pushed outside the unload radius.
    if (Expedition && Expedition->GetHaulerCargo() > 0) Expedition->TryUnloadHaulerCargo(this);
    RefreshAppearance();
}

void ADownstreamHauler::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.0f || !GetWorld()) return;
    if (!IsValid(Expedition)) Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this);
    if (!Expedition || !Expedition->IsHaulerConstructed()) return;
    if (!Tuning) Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld());
    const double Now = GetWorld()->GetTimeSeconds();
    if (GetVelocity().SizeSquared2D() > FMath::Square(5.0f))
        Expedition->UpdateHaulerBattery(-FMath::Max(0.0f, Tuning->HaulerBatteryDrain) * DeltaSeconds);
    // Zero battery remains a visible slow emergency return. Recast still controls motion,
    // so an exhausted robot does not silently stall or walk straight through a wall.
    const float Speed = FMath::Max(1.0f, Tuning->HaulerMoveSpeed);
    GetCharacterMovement()->MaxWalkSpeed = Expedition->GetHaulerBattery() <= 0.0f ? FMath::Max(80.0f, Speed * 0.3f) : Speed;
    if (Now >= NextDecision)
    {
        NextDecision = Now + DecisionInterval;
        DecideNextAction();
    }
    if (bCharging)
    {
        Expedition->UpdateHaulerBattery(FMath::Max(0.0f, Tuning->HaulerChargeRate) * DeltaSeconds);
        if (Expedition->GetHaulerBattery() >= Tuning->HaulerBatteryMax - 0.5f)
        {
            bCharging = false;
            bChargeRequired = false;
            bIdleRechargeCompleted = true;
            NextDecision = Now;
        }
    }
    if (Now >= NextCheckpoint)
    {
        NextCheckpoint = Now + 5.0;
        if (!FMath::IsNearlyEqual(LastCheckpointBattery, Expedition->GetHaulerBattery()))
            if (Expedition->SaveHaulerCheckpoint()) LastCheckpointBattery = Expedition->GetHaulerBattery();
    }
    RefreshAppearance();
}

void ADownstreamHauler::DecideNextAction()
{
    const double Now = GetWorld()->GetTimeSeconds();
    bCharging = false;
    if (bLastHaulingEnabled != Expedition->IsHaulingEnabled())
    {
        bLastHaulingEnabled = Expedition->IsHaulingEnabled();
        bIdleRechargeCompleted = false;
    }
    FVector Charger;
    if (!GetStructureLocation(static_cast<uint8>(EDownstreamStructureType::Charger), Charger))
    {
        StopNavigation();
        SetStatus(TEXT("Blocked"));
        return;
    }
    const float Reserve = FVector::Dist2D(GetActorLocation(), Charger) / FMath::Max(1.0f, Tuning->HaulerMoveSpeed)
        * FMath::Max(0.0f, Tuning->HaulerBatteryDrain) + FMath::Max(0.0f, Tuning->HaulerBatteryReserve);
    if (Expedition->GetHaulerBattery() <= Reserve) bChargeRequired = true;
    if (bChargeRequired)
    {
        TargetPickup.Reset();
        ReturnToCharger();
        return;
    }
    if (Expedition->GetHaulerCargo() > 0)
    {
        TargetPickup.Reset();
        ReturnToStorage(!Expedition->IsHaulingEnabled());
        return;
    }
    if (!Expedition->IsHaulingEnabled())
    {
        TargetPickup.Reset();
        ReturnToStorage(true);
        return;
    }
    if (Now < BlockedUntil)
    {
        ReturnToStorage(false);
        return;
    }
    if (auto* Pickup = TargetPickup.Get())
    {
        if (!IsPickupSafe(Pickup) || Pickup->IsClaimed() || Pickup->Amount > Expedition->GetHaulerCargoCapacity())
        {
            BlockCurrentJob();
            ReturnToStorage(false);
            return;
        }
    }
    else
    {
        ADownstreamSalvagePickup* Candidate = FindPickup();
        if (Candidate)
        {
            StopNavigation();
            TargetPickup = Candidate;
            bIdleRechargeCompleted = false;
        }
        if (!TargetPickup.IsValid())
        {
            if (bChargeRequired) ReturnToCharger();
            else ReturnToStorage(false);
            return;
        }
    }
    auto* Pickup = TargetPickup.Get();
    Job = EJob::Pickup;
    SetStatus(TEXT("Hauling"));
    if (FVector::DistSquared2D(GetActorLocation(), Pickup->GetActorLocation()) <= FMath::Square(PickupRange))
    {
        StopNavigation();
        // Safe-target/capacity rules are checked again immediately before the core's
        // save-before-destroy transaction. Failed saves release its claim and keep it intact.
        if (IsPickupSafe(Pickup) && Expedition->TryLoadHaulerCargo(Pickup, this))
        {
            TargetPickup.Reset();
            Job = EJob::Storage;
            SetStatus(TEXT("Delivering"));
            NextDecision = Now;
        }
        else BlockCurrentJob();
        return;
    }
    if (!MoveTo(Pickup->GetActorLocation(), 70.0f)) BlockCurrentJob();
}

bool ADownstreamHauler::GetStructureLocation(uint8 Type, FVector& OutLocation) const
{
    if (!Expedition) return false;
    for (const auto& Record : Expedition->GetBuiltStructures())
        if (static_cast<uint8>(Record.Type) == Type) { OutLocation = Record.WorldLocation; return true; }
    return false;
}

bool ADownstreamHauler::IsPickupSafe(const ADownstreamSalvagePickup* Pickup) const
{
    if (!IsValid(Pickup) || Pickup->IsActorBeingDestroyed() || Pickup->Amount <= 0 || !GetWorld() || !Tuning) return false;
    const float Radius = FMath::Max(0.0f, Tuning->HaulerAvoidHostileRadius);
    TArray<FOverlapResult> Overlaps;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DownstreamHaulerSafety), false, this);
    GetWorld()->OverlapMultiByObjectType(Overlaps, Pickup->GetActorLocation() + FVector(0,0,50), FQuat::Identity,
        Objects, FCollisionShape::MakeSphere(Radius), Query);
    for (const FOverlapResult& Overlap : Overlaps)
    {
        const AActor* Actor = Overlap.GetActor();
        if (IsValid(Actor) && Actor->ActorHasTag(TEXT("DownstreamHostile"))
            && FVector::DistSquared2D(Pickup->GetActorLocation(), Actor->GetActorLocation()) <= FMath::Square(Radius)) return false;
    }
    return true;
}

ADownstreamSalvagePickup* ADownstreamHauler::FindPickup()
{
    const double Now = GetWorld()->GetTimeSeconds();
    for (auto It = PickupBackoff.CreateIterator(); It; ++It)
        if (!It.Key().IsValid() || It.Value() <= Now) It.RemoveCurrent();
    TArray<ADownstreamSalvagePickup*> Candidates;
    for (TActorIterator<ADownstreamSalvagePickup> It(GetWorld()); It; ++It)
    {
        auto* Pickup = *It;
        if (Pickup->IsClaimed() || Pickup->Amount <= 0 || Pickup->Amount > Expedition->GetHaulerCargoCapacity()
            || FVector::DistSquared2D(GetActorLocation(), Pickup->GetActorLocation()) > FMath::Square(MaxJobDistance)
            || PickupBackoff.Contains(Pickup) || !IsPickupSafe(Pickup)) continue;
        Candidates.Add(Pickup);
    }
    Candidates.Sort([this](const ADownstreamSalvagePickup& Left, const ADownstreamSalvagePickup& Right)
        { return FVector::DistSquared2D(GetActorLocation(), Left.GetActorLocation()) < FVector::DistSquared2D(GetActorLocation(), Right.GetActorLocation()); });
    FVector Charger;
    GetStructureLocation(static_cast<uint8>(EDownstreamStructureType::Charger), Charger);
    bool bRejectedPath = false;
    for (auto* Pickup : Candidates)
    {
        float PathLength = 0.0f;
        if (!HasCompletePath(Pickup->GetActorLocation(), &PathLength))
        {
            PickupBackoff.Add(Pickup, Now + JobBackoff);
            bRejectedPath = true;
            continue;
        }
        const float Required = (PathLength + FVector::Dist2D(Pickup->GetActorLocation(), Charger))
            / FMath::Max(1.0f, Tuning->HaulerMoveSpeed) * FMath::Max(0.0f, Tuning->HaulerBatteryDrain)
            + FMath::Max(0.0f, Tuning->HaulerBatteryReserve);
        if (Expedition->GetHaulerBattery() < Required)
        {
            if (Expedition->GetHaulerBattery() < Tuning->HaulerBatteryMax - 0.5f) bChargeRequired = true;
            continue;
        }
        return Pickup;
    }
    if (bRejectedPath) BlockedUntil = Now + JobBackoff;
    return nullptr;
}

bool ADownstreamHauler::HasCompletePath(const FVector& Destination, float* OutLength) const
{
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!Nav) return false;
    FNavLocation Projected;
    if (!Nav->ProjectPointToNavigation(Destination, Projected, FVector(80,80,250), &GetCharacterMovement()->GetNavAgentPropertiesRef())) return false;
    UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), GetActorLocation(), Projected.Location, const_cast<ADownstreamHauler*>(this));
    if (!Path || !Path->IsValid() || Path->IsPartial()) return false;
    if (OutLength) *OutLength = static_cast<float>(Path->GetPathLength());
    return true;
}

bool ADownstreamHauler::FindDock(const FVector& BuildingLocation, float Radius, FVector& OutLocation) const
{
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!Nav) return false;
    const FVector Offsets[] = { FVector(Radius,0,0), FVector(0,Radius,0), FVector(-Radius,0,0), FVector(0,-Radius,0) };
    float BestLength = TNumericLimits<float>::Max();
    bool bFound = false;
    for (const FVector& Offset : Offsets)
    {
        FNavLocation Projected;
        if (!Nav->ProjectPointToNavigation(BuildingLocation + Offset, Projected, FVector(12,12,250), &GetCharacterMovement()->GetNavAgentPropertiesRef())) continue;
        if (FVector::DistSquared2D(BuildingLocation, Projected.Location) > FMath::Square(Radius + 5.0f)) continue;
        float PathLength = 0.0f;
        if (HasCompletePath(Projected.Location, &PathLength) && PathLength < BestLength)
        {
            BestLength = PathLength;
            OutLocation = Projected.Location;
            bFound = true;
        }
    }
    return bFound;
}

bool ADownstreamHauler::MoveTo(const FVector& Destination, float AcceptanceRadius)
{
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now < NextNavigationRetry) return false;
    auto* AI = Cast<AAIController>(GetController());
    if (!AI)
    {
        SpawnDefaultController();
        AI = Cast<AAIController>(GetController());
        if (!AI) return false;
    }
    const bool bChangedGoal = !bHasMoveDestination || FVector::DistSquared2D(Destination, MoveDestination) > FMath::Square(35.0f);
    if (bChangedGoal)
    {
        bNavigationFailed = false;
        LastProgressTime = Now;
        ProgressLocation = GetActorLocation();
    }
    if (FVector::DistSquared2D(GetActorLocation(), ProgressLocation) > FMath::Square(15.0f))
    {
        ProgressLocation = GetActorLocation();
        LastProgressTime = Now;
    }
    else if (Now - LastProgressTime > 8.0)
    {
        UE_LOG(LogDownstreamHauler, Warning, TEXT("Hauler stalled on job %d at %s; goal %s"), static_cast<int32>(Job), *GetActorLocation().ToCompactString(), *Destination.ToCompactString());
        NextNavigationRetry = Now + 2.0;
        StopNavigation();
        return false;
    }
    if (Now >= NextMoveRequest && (bChangedGoal || AI->GetMoveStatus() != EPathFollowingStatus::Moving || bNavigationFailed))
    {
        // Request at most 2.5Hz; retaining an active path prevents repeated MoveTo aborts.
        NextMoveRequest = Now + DecisionInterval;
        MoveDestination = Destination;
        bHasMoveDestination = true;
        bNavigationFailed = AI->MoveToLocation(Destination, AcceptanceRadius, false, true, false, true, nullptr, false) == EPathFollowingRequestResult::Failed;
        if (bNavigationFailed) NextNavigationRetry = Now + 2.0;
    }
    return !bNavigationFailed;
}

void ADownstreamHauler::ReturnToStorage(bool bPaused)
{
    const double Now = GetWorld()->GetTimeSeconds();
    if (Job != EJob::Storage) { StopNavigation(); Job = EJob::Storage; }
    const bool bCargo = Expedition->GetHaulerCargo() > 0;
    SetStatus(Now < BlockedUntil ? TEXT("Blocked") : bCargo ? TEXT("Delivering") : bPaused ? TEXT("Paused") : TEXT("Idle"));
    FVector Storage;
    if (!GetStructureLocation(static_cast<uint8>(EDownstreamStructureType::Storage), Storage))
    {
        StopNavigation();
        SetStatus(TEXT("Blocked"));
        return;
    }
    if (FVector::DistSquared2D(GetActorLocation(), Storage) <= FMath::Square(StorageRange))
    {
        StopNavigation();
        if (bCargo && Now >= NextCargoTransaction)
        {
            NextCargoTransaction = Now + 5.0;
            if (!Expedition->TryUnloadHaulerCargo(this))
            {
                BlockedUntil = Now + 5.0;
                SetStatus(TEXT("Blocked"));
                return;
            }
            bIdleRechargeCompleted = false;
        }
        // Recharge once when work ends, then return to storage. The short return trip
        // drains a little battery; that must not cause an endless idle dock shuttle.
        if (Expedition->GetHaulerCargo() == 0 && !bIdleRechargeCompleted
            && Expedition->GetHaulerBattery() < Tuning->HaulerBatteryMax - 0.5f)
        {
            bChargeRequired = true;
            ReturnToCharger();
        }
        return;
    }
    FVector Dock;
    // Projection may shift a dock by 5cm and path following may stop 4cm short.
    // 140 + 5 + 4 remains inside the 150cm delivery threshold.
    if ((!bHasMoveDestination || bNavigationFailed) && !FindDock(Storage, 140.0f, Dock))
    {
        SetStatus(TEXT("Blocked"));
        return;
    }
    if (bHasMoveDestination && !bNavigationFailed) Dock = MoveDestination;
    if (!MoveTo(Dock, 4.0f))
    {
        bHasMoveDestination = false;
        SetStatus(TEXT("Blocked"));
    }
}

void ADownstreamHauler::ReturnToCharger()
{
    if (Job != EJob::Charger) { StopNavigation(); Job = EJob::Charger; }
    SetStatus(TEXT("Returning to charge"));
    FVector Charger;
    if (!GetStructureLocation(static_cast<uint8>(EDownstreamStructureType::Charger), Charger)) { SetStatus(TEXT("Blocked")); return; }
    if (FVector::DistSquared2D(GetActorLocation(), Charger) <= FMath::Square(ChargeRange))
    {
        StopNavigation();
        bCharging = true;
        SetStatus(TEXT("Charging"));
        return;
    }
    FVector Dock;
    if ((!bHasMoveDestination || bNavigationFailed) && !FindDock(Charger, 180.0f, Dock))
    {
        SetStatus(TEXT("Blocked"));
        return;
    }
    if (bHasMoveDestination && !bNavigationFailed) Dock = MoveDestination;
    if (!MoveTo(Dock, 10.0f))
    {
        bHasMoveDestination = false;
        SetStatus(TEXT("Blocked"));
    }
}

void ADownstreamHauler::BlockCurrentJob()
{
    const double Now = GetWorld()->GetTimeSeconds();
    if (TargetPickup.IsValid()) PickupBackoff.Add(TargetPickup, Now + JobBackoff);
    TargetPickup.Reset();
    StopNavigation();
    BlockedUntil = Now + JobBackoff;
    Job = EJob::None;
    SetStatus(TEXT("Blocked"));
}

void ADownstreamHauler::StopNavigation()
{
    if (auto* AI = Cast<AAIController>(GetController())) AI->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
    bHasMoveDestination = false;
    bNavigationFailed = false;
}

void ADownstreamHauler::OnNavigationFinished(bool bSucceeded, bool bAborted)
{
    if (bAborted) return; // New goals and deliberate docking stops abort the preceding path.
    bNavigationFailed = !bSucceeded;
    if (!bSucceeded)
    {
        if (Job == EJob::Pickup) BlockCurrentJob();
        else
        {
            bHasMoveDestination = false;
            NextNavigationRetry = GetWorld() ? GetWorld()->GetTimeSeconds() + 2.0 : 0.0;
            SetStatus(TEXT("Blocked"));
        }
    }
}

ADownstreamSalvagePickup* ADownstreamHauler::GetTargetPickup() const
{
    return TargetPickup.Get();
}

void ADownstreamHauler::SetStatus(const TCHAR* NewStatus)
{
    Status = FText::FromString(NewStatus);
}

void ADownstreamHauler::RefreshAppearance()
{
    if (!Expedition) return;
    const int32 Cargo = Expedition->GetHaulerCargo();
    const float Battery = Expedition->GetHaulerBattery();
    CargoMesh->SetVisibility(Cargo > 0);
    WorldLabel->SetWorldRotation(FRotator(55,-135,0));
    WorldLabel->SetText(FText::FromString(FString::Printf(TEXT("HAULER %d%%  %d/%d\n%s"), FMath::RoundToInt(Battery), Cargo,
        Expedition->GetHaulerCargoCapacity(), *Status.ToString())));
    const FColor Color = bCharging ? FColor(176,247,113) : Battery <= 12.0f ? FColor(255,157,82) : FColor(86,235,222);
    WorldLabel->SetTextRenderColor(Color);
    StatusGlow->SetLightColor(Color);
    StatusGlow->SetIntensity(bCharging ? 380.0f : 180.0f);
    const float Pulse = bCharging && GetWorld() ? 1.0f + 0.15f * FMath::Sin(GetWorld()->GetTimeSeconds() * 5.0f) : 1.0f;
    StatusCore->SetRelativeScale3D(FVector(0.23f * Pulse));
    if (auto* Material = Cast<UMaterialInstanceDynamic>(StatusCore->GetMaterial(0)))
    {
        Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(Color));
        Material->SetVectorParameterValue(TEXT("Tint"), FLinearColor(Color));
        Material->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(Color));
    }
}

void ADownstreamHauler::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopNavigation();
    if (IsValid(Expedition)) Expedition->SaveHaulerCheckpoint();
    Super::EndPlay(EndPlayReason);
}
