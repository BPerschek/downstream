#include "DownstreamDigger.h"
#include "Sound/SoundBase.h"

#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamPrototypeTuning.h"
#include "DownstreamWaterSubsystem.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Navigation/PathFollowingComponent.h"

namespace
{
constexpr double StallSeconds = 3.0;
constexpr double DirectSteerSeconds = 3.5;

template <typename T>
T* LoadIfPresent(const TCHAR* Package)
{
    if (!FPackageName::DoesPackageExist(Package)) return nullptr;
    return LoadObject<T>(nullptr, *FString::Printf(TEXT("%s.%s"), Package, *FPackageName::GetShortName(Package)));
}
}

ADownstreamDigger::ADownstreamDigger()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.0f;
    Tags.AddUnique(TEXT("DownstreamDigger"));
    // Hostiles may target and shoot a working robot; the tag drops while it is wrecked.
    Tags.AddUnique(TEXT("DownstreamRobot"));
    GetCapsuleComponent()->InitCapsuleSize(38.0f, 58.0f);
    GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
    GetCapsuleComponent()->SetCanEverAffectNavigation(false);
    SetCanAffectNavigationGeneration(false);
    GetMesh()->SetVisibility(false);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetCharacterMovement()->MaxWalkSpeed = 380.0f;
    GetCharacterMovement()->MaxAcceleration = 1400.0f;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0, 420, 0);
    GetCharacterMovement()->SetWalkableFloorAngle(50.0f);
    GetCharacterMovement()->MaxStepHeight = 55.0f;
    GetCharacterMovement()->GetNavAgentPropertiesRef().AgentRadius = 38.0f;
    bUseControllerRotationYaw = false;
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = AAIController::StaticClass();

    BotMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BotMesh"));
    BotMesh->SetupAttachment(GetRootComponent());
    BotMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BotMesh->SetCanEverAffectNavigation(false);
    Scoop = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Scoop"));
    Scoop->SetupAttachment(GetRootComponent());
    Scoop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Scoop->SetCanEverAffectNavigation(false);
    Scoop->SetRelativeLocation(FVector(62, 0, -20));
    Scoop->SetRelativeScale3D(FVector(0.35f, 0.7f, 0.12f));
    WorkLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("WorkLight"));
    WorkLight->SetupAttachment(GetRootComponent());
    WorkLight->SetRelativeLocation(FVector(30, 0, 70));
    WorkLight->SetIntensity(250.0f);
    WorkLight->SetAttenuationRadius(240.0f);
    WorkLight->SetLightColor(FLinearColor(1.0f, 0.6f, 0.15f));
    WorkLight->SetCastShadows(false);
    WorldLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("WorldLabel"));
    WorldLabel->SetupAttachment(GetRootComponent());
    WorldLabel->SetRelativeLocation(FVector(0, 0, 130));
    WorldLabel->SetUsingAbsoluteRotation(true);
    WorldLabel->SetWorldRotation(FRotator(55, -135, 0));
    WorldLabel->SetHorizontalAlignment(EHTA_Center);
    WorldLabel->SetWorldSize(17.0f);
    WorldLabel->SetTextRenderColor(FColor(255, 190, 90));
    SetStatus(TEXT("Idle"));
}

void ADownstreamDigger::BeginPlay()
{
    Super::BeginPlay();
    if (!GetController()) SpawnDefaultController();
    const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld());
    GetCharacterMovement()->MaxWalkSpeed = FMath::Max(1.0f, Tuning->DiggerMoveSpeed);
    // CC0 Kenney Space Kit rover; the original salvage bot is the fallback.
    UStaticMesh* Robot = LoadIfPresent<UStaticMesh>(TEXT("/Game/Downstream/ThirdParty/Kenney/Space/rover/SM_K_rover"));
    const bool bRover = Robot != nullptr;
    if (!Robot) Robot = LoadIfPresent<UStaticMesh>(TEXT("/Game/Downstream/Art/SM_SalvageBot"));
    if (!Robot) Robot = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    BotMesh->SetStaticMesh(Robot);
    if (Robot)
    {
        const auto Bounds = Robot->GetBounds();
        const FVector Size = Bounds.BoxExtent * 2.0f;
        const float Scale = FMath::Min(115.0f / FMath::Max(1.0f, FMath::Max(Size.X, Size.Y)), 110.0f / FMath::Max(1.0f, Size.Z));
        BotMesh->SetRelativeScale3D(FVector(Scale));
        BotMesh->SetRelativeRotation(FRotator(0, bRover ? 0 : 90, 0));
        BotMesh->SetRelativeLocation(FVector(Bounds.Origin.Y * Scale, -Bounds.Origin.X * Scale, -58.0f - (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale));
    }
    Scoop->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    if (auto* Orange = LoadIfPresent<UMaterialInterface>(TEXT("/Game/Downstream/Materials/M_PrototypeRecovery")))
    {
        Scoop->SetMaterial(0, Orange);
        // The rover keeps its own palette; only the fallback bot is tinted.
        if (!bRover) for (int32 Index = 0; Index < BotMesh->GetNumMaterials(); ++Index) BotMesh->SetMaterial(Index, Orange);
    }
    ProgressLocation = GetActorLocation();
    LastProgressTime = GetWorld()->GetTimeSeconds();
    Health = GetMaxHealth();
}

float ADownstreamDigger::GetMaxHealth() const
{
    return FMath::Max(1.0f, UDownstreamExpeditionSubsystem::GetTuning(GetWorld())->DiggerHealth);
}

float ADownstreamDigger::GetRebuildRemaining() const
{
    return bWrecked && GetWorld() ? FMath::Max(0.0f, static_cast<float>(RebuildAt - GetWorld()->GetTimeSeconds())) : 0.0f;
}

float ADownstreamDigger::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (bWrecked || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f) return 0.0f;
    if (Health < 0.0f) Health = GetMaxHealth();
    Health = FMath::Max(0.0f, Health - DamageAmount);
    FlashRemaining = 0.2f;
    if (const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld()); Tuning->HitSound)
        UGameplayStatics::PlaySoundAtLocation(this, Tuning->HitSound, GetActorLocation(), 0.6f, 0.7f);
    if (Health <= 0.0f) Wreck();
    return DamageAmount;
}

void ADownstreamDigger::Wreck()
{
    bWrecked = true;
    ++TimesWrecked;
    Tags.Remove(TEXT("DownstreamRobot"));
    StopMoving();
    GetCharacterMovement()->StopMovementImmediately();
    RebuildAt = GetWorld()->GetTimeSeconds() + FMath::Max(1.0f, UDownstreamExpeditionSubsystem::GetTuning(GetWorld())->DiggerRebuildSeconds);
    NextRebuildAttempt = RebuildAt;
    BotMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 35.0f));
    if (const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld()); Tuning && Tuning->WreckSound)
        UGameplayStatics::PlaySoundAtLocation(this, Tuning->WreckSound, GetActorLocation());
    WorkLight->SetLightColor(FLinearColor(1.0f, 0.1f, 0.05f));
    if (auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this))
        Expedition->PostStatus(FString::Printf(TEXT("Your digger was wrecked! It reassembles at the workshop in %.0f s for %d salvage."),
            UDownstreamExpeditionSubsystem::GetTuning(GetWorld())->DiggerRebuildSeconds, UDownstreamExpeditionSubsystem::GetTuning(GetWorld())->DiggerRebuildCost));
}

void ADownstreamDigger::TickWrecked(double Now)
{
    const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld());
    if (Now < RebuildAt)
    {
        SetStatus(FString::Printf(TEXT("WRECKED / rebuild %.0fs"), RebuildAt - Now));
        return;
    }
    if (Now < NextRebuildAttempt) return;
    NextRebuildAttempt = Now + 3.0;
    auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this);
    if (!Expedition || !Expedition->TrySpendBanked(Tuning->DiggerRebuildCost, TEXT("Digger rebuild")))
    {
        SetStatus(FString::Printf(TEXT("WRECKED / needs %d banked salvage"), Tuning->DiggerRebuildCost));
        return;
    }
    FVector Home;
    if (GetHomeLocation(Home)) TeleportTo(Home + FVector(0, 0, 70.0f), FRotator::ZeroRotator);
    bWrecked = false;
    Health = GetMaxHealth();
    Tags.AddUnique(TEXT("DownstreamRobot"));
    BotMesh->SetRelativeRotation(FRotator::ZeroRotator);
    WorkLight->SetLightColor(FLinearColor(1.0f, 0.6f, 0.15f));
    LastProgressTime = Now;
    Expedition->PostStatus(TEXT("Digger rebuilt at the workshop. Its queued canal is waiting."));
}

void ADownstreamDigger::SetStatus(const FString& NewStatus)
{
    if (Status.ToString() != NewStatus) Status = FText::FromString(NewStatus);
    const FString Label = FString::Printf(TEXT("DIGGER %d%% / %s"),
        Health < 0.0f ? 100 : FMath::RoundToInt(100.0f * Health / FMath::Max(1.0f, GetMaxHealth())), *NewStatus);
    if (WorldLabel->Text.ToString() != Label) WorldLabel->SetText(FText::FromString(Label));
}

bool ADownstreamDigger::GetHomeLocation(FVector& Out) const
{
    const auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(const_cast<ADownstreamDigger*>(this));
    if (!Expedition) return false;
    for (const auto& Record : Expedition->GetBuiltStructures())
        if (Record.Type == EDownstreamStructureType::Workshop) { Out = Record.WorldLocation + FVector(0, 170, 0); return true; }
    return false;
}

void ADownstreamDigger::StopMoving()
{
    if (auto* AI = Cast<AAIController>(GetController())) AI->StopMovement();
    bHasGoal = false;
    bDirectSteering = false;
}

void ADownstreamDigger::Steer(const FVector& Destination, float Acceptance, double Now)
{
    const bool bNewGoal = !bHasGoal || FVector::DistSquared2D(Destination, MoveGoal) > FMath::Square(40.0f);
    if (bNewGoal)
    {
        MoveGoal = Destination;
        bHasGoal = true;
        bDirectSteering = false;
        NextMoveRequest = 0.0;
        ProgressLocation = GetActorLocation();
        LastProgressTime = Now;
    }
    if (FVector::DistSquared2D(GetActorLocation(), ProgressLocation) > FMath::Square(25.0f))
    {
        ProgressLocation = GetActorLocation();
        LastProgressTime = Now;
    }
    auto* AI = Cast<AAIController>(GetController());
    // Navigation first. If the path fails or the robot stops making progress
    // (fresh canal walls, navmesh still updating), steer straight for a while.
    if (!bDirectSteering && Now - LastProgressTime > StallSeconds)
    {
        bDirectSteering = true;
        DirectUntil = Now + DirectSteerSeconds;
        if (AI) AI->StopMovement();
    }
    if (bDirectSteering)
    {
        const FVector Direction = (Destination - GetActorLocation()).GetSafeNormal2D();
        AddMovementInput(Direction, 1.0f);
        if (Now >= DirectUntil)
        {
            bDirectSteering = false;
            LastProgressTime = Now;
            NextMoveRequest = 0.0;
        }
        return;
    }
    if (AI && Now >= NextMoveRequest && (bNewGoal || AI->GetMoveStatus() != EPathFollowingStatus::Moving))
    {
        NextMoveRequest = Now + 0.5;
        const auto Result = AI->MoveToLocation(Destination, Acceptance, false, true, true, true, nullptr, false);
        if (Result == EPathFollowingRequestResult::Failed)
        {
            bDirectSteering = true;
            DirectUntil = Now + DirectSteerSeconds;
        }
    }
}

void ADownstreamDigger::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!GetWorld() || !FMath::IsFinite(DeltaSeconds)) return;
    const double Now = GetWorld()->GetTimeSeconds();
    auto* Water = UDownstreamWaterSubsystem::GetForWorld(this);
    const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld());
    bDigging = false;
    if (Health < 0.0f) Health = GetMaxHealth();
    if (FlashRemaining > 0.0f)
    {
        FlashRemaining = FMath::Max(0.0f, FlashRemaining - DeltaSeconds);
        WorkLight->SetLightColor(FlashRemaining > 0.0f ? FLinearColor(1.0f, 0.15f, 0.1f) : FLinearColor(1.0f, 0.6f, 0.15f));
    }
    if (bWrecked)
    {
        TickWrecked(Now);
        return;
    }
    // Field repairs at home.
    if (const auto* Expedition = UDownstreamExpeditionSubsystem::GetForWorld(this); Expedition && Expedition->IsAtHome(GetActorLocation()))
        Health = FMath::Min(GetMaxHealth(), Health + Tuning->DiggerRepairPerSecond * DeltaSeconds);
    FIntPoint Cell;
    if (Water && Water->GetNextDigCell(Cell))
    {
        const FVector Target = Water->CellToWorld(Cell);
        FIntPoint StandCell = Cell;
        Water->GetDigStandCell(StandCell);
        const FVector Stand = Water->CellToWorld(StandCell);
        const float Reach = FMath::Max(1.0f, Tuning->DigReach);
        const bool bInReach = FVector::DistSquared2D(GetActorLocation(), Target) <= FMath::Square(Reach * 0.9f);
        const bool bAtStand = FVector::DistSquared2D(GetActorLocation(), Stand) <= FMath::Square(60.0f);
        if (bInReach && (bAtStand || FVector::DistSquared2D(GetActorLocation(), Target) <= FMath::Square(Reach * 0.6f) || Now - LastProgressTime > 1.5))
        {
            if (bHasGoal) StopMoving();
            bDigging = true;
            LastProgressTime = Now;
            const FVector Face = (Target - GetActorLocation()).GetSafeNormal2D();
            if (!Face.IsNearlyZero()) SetActorRotation(FMath::RInterpTo(GetActorRotation(), Face.Rotation(), DeltaSeconds, 8.0f));
            if (Water->PerformDigAction(this)) ++DigActions;
            SetStatus(FString::Printf(TEXT("Digging %d%%"), FMath::RoundToInt(Water->GetActiveCanalProgress() * 100.0f)));
        }
        else
        {
            // Track progress while driving on the site so a wedged robot still digs from where it is.
            if (FVector::DistSquared2D(GetActorLocation(), ProgressLocation) > FMath::Square(20.0f))
            {
                ProgressLocation = GetActorLocation();
                LastProgressTime = Now;
            }
            // Navigation reaches the edge of the site; the terrain itself is driven directly,
            // since dug canals and berms change faster than navigation data can follow.
            FIntPoint Here;
            const bool bOnSite = Water->WorldToCell(GetActorLocation(), Here);
            const FVector SiteMin = Water->GetSiteOrigin();
            const FVector2D Size = FVector2D(Water->GetGridSize()) * Water->GetCellSizeCm();
            FVector Entry = GetActorLocation();
            Entry.X = FMath::Clamp(Entry.X, SiteMin.X, SiteMin.X + Size.X);
            Entry.Y = FMath::Clamp(Entry.Y, SiteMin.Y, SiteMin.Y + Size.Y);
            const FVector Outward = (GetActorLocation() - Entry).GetSafeNormal2D();
            Entry += Outward * 40.0f;
            const bool bNearEdge = FVector::DistSquared2D(GetActorLocation(), Entry) < FMath::Square(140.0f);
            if (bOnSite || bNearEdge)
            {
                if (bHasGoal) StopMoving();
                const FVector Direction = (Stand - GetActorLocation()).GetSafeNormal2D();
                AddMovementInput(Direction, 1.0f);
                SetStatus(TEXT("Crossing site"));
                // Wedged against a canal wall or berm: hop toward the stand point.
                if (Now - LastProgressTime > 3.0 && GetCharacterMovement()->IsMovingOnGround())
                {
                    LaunchCharacter(Direction * 280.0f + FVector(0, 0, 520.0f), true, true);
                    LastProgressTime = Now;
                    ++Hops;
                }
            }
            else
            {
                Steer(Entry, 60.0f, Now);
                SetStatus(bDirectSteering ? TEXT("Rerouting") : TEXT("Heading to site"));
            }
        }
    }
    else
    {
        FVector Home;
        if (GetHomeLocation(Home) && FVector::DistSquared2D(GetActorLocation(), Home) > FMath::Square(160.0f))
        {
            Steer(Home, 60.0f, Now);
            SetStatus(TEXT("Returning to workshop"));
        }
        else
        {
            if (bHasGoal) StopMoving();
            SetStatus(TEXT("Idle / press G to plan a canal"));
        }
    }
    // Wading slows robots exactly like people.
    if (Water)
    {
        const float Depth = Water->GetWaterDepthAt(GetActorLocation());
        const float Slow = FMath::Lerp(1.0f, FMath::Clamp(Tuning->WadeSpeedAtOneMetre, 0.05f, 1.0f), FMath::Clamp(Depth, 0.0f, 1.5f));
        GetCharacterMovement()->MaxWalkSpeed = FMath::Max(1.0f, Tuning->DiggerMoveSpeed) * FMath::Max(0.25f, Slow);
    }
    ScoopPhase = bDigging ? ScoopPhase + DeltaSeconds * 11.0f : FMath::FInterpTo(ScoopPhase, 0.0f, DeltaSeconds, 4.0f);
    Scoop->SetRelativeRotation(FRotator(bDigging ? FMath::Sin(ScoopPhase) * 35.0f : 0.0f, 0, 0));
    WorkLight->SetIntensity(bDigging ? 700.0f + 300.0f * FMath::Sin(ScoopPhase * 2.0f) : 250.0f);
}
