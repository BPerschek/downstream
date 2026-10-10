#include "DownstreamRaider.h"

#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamPlayerCharacter.h"
#include "DownstreamPrototypeTuning.h"
#include "DownstreamWaterSubsystem.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Misc/PackageName.h"
#include "UObject/ConstructorHelpers.h"

ADownstreamRaider::ADownstreamRaider()
{
    GetCapsuleComponent()->InitCapsuleSize(40.0f, 92.0f);
    GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -92.0f));
    GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    GetMesh()->SetVisibility(true);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Quinn(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Manny(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
    if (Quinn.Succeeded()) GetMesh()->SetSkeletalMesh(Quinn.Object);
    else if (Manny.Succeeded()) GetMesh()->SetSkeletalMesh(Manny.Object);
    static ConstructorHelpers::FClassFinder<UAnimInstance> Unarmed(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
    if (Unarmed.Succeeded()) GetMesh()->SetAnimInstanceClass(Unarmed.Class);
    // Raiders turn toward where they walk; the base class still faces targets when fighting.
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0, 540, 0);
    RobotMesh->SetVisibility(false);
    ReactorCore->SetVisibility(false);
    Sandbag = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sandbag"));
    Sandbag->SetupAttachment(GetRootComponent());
    Sandbag->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Sandbag->SetRelativeLocation(FVector(-30.0f, 0.0f, 20.0f));
    Sandbag->SetVisibility(false);
    RoleLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("RoleLabel"));
    RoleLabel->SetupAttachment(GetRootComponent());
    RoleLabel->SetRelativeLocation(FVector(0, 0, 130));
    RoleLabel->SetUsingAbsoluteRotation(true);
    RoleLabel->SetWorldRotation(FRotator(55, -135, 0));
    RoleLabel->SetHorizontalAlignment(EHTA_Center);
    RoleLabel->SetWorldSize(16.0f);
    RoleLabel->SetTextRenderColor(FColor(255, 150, 90));
}

float ADownstreamRaider::InitialHealth() const
{
    const auto* Tuning = GetTuning();
    switch (RaiderRole)
    {
    case EDownstreamRaiderRole::Engineer: return Tuning->EngineerHealth;
    case EDownstreamRaiderRole::Rifleman: return Tuning->ShooterHealth;
    default: return Tuning->ChaserHealth * 1.2f;
    }
}

void ADownstreamRaider::BeginPlay()
{
    Kind = RaiderRole == EDownstreamRaiderRole::Rifleman ? EDownstreamEnemyKind::Shooter : EDownstreamEnemyKind::Chaser;
    Super::BeginPlay();
    Tags.AddUnique(TEXT("DownstreamRaider"));
    RobotMesh->SetVisibility(false);
    ReactorCore->SetVisibility(false);
    // Rust for guards, grey-violet for riflemen, hazard yellow for engineers.
    const FLinearColor Color = RaiderRole == EDownstreamRaiderRole::Engineer ? FLinearColor(0.95f, 0.72f, 0.08f)
        : RaiderRole == EDownstreamRaiderRole::Rifleman ? FLinearColor(0.45f, 0.30f, 0.62f) : FLinearColor(0.72f, 0.20f, 0.08f);
    RegisterFeedbackMesh(GetMesh(), Color);
    RoleLabel->SetText(FText::FromString(RaiderRole == EDownstreamRaiderRole::Engineer ? TEXT("ENGINEER") : TEXT("")));
    if (RaiderRole == EDownstreamRaiderRole::Engineer)
    {
        const TCHAR* Box = TEXT("/Game/Downstream/ThirdParty/Kenney/Survival/box/SM_K_box");
        UStaticMesh* BoxMesh = FPackageName::DoesPackageExist(Box) ? LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Downstream/ThirdParty/Kenney/Survival/box/SM_K_box.SM_K_box")) : nullptr;
        Sandbag->SetStaticMesh(BoxMesh ? BoxMesh : LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
        Sandbag->SetRelativeScale3D(BoxMesh ? FVector(1.6f) : FVector(0.35f, 0.45f, 0.25f));
        Sandbag->SetVisibility(true);
        GetCharacterMovement()->MaxWalkSpeed = GetTuning()->ChaserSpeed * 1.15f;
    }
}

AActor* ADownstreamRaider::ChooseTarget() const
{
    if (RaiderRole == EDownstreamRaiderRole::Engineer) return nullptr;
    AActor* Base = Super::ChooseTarget();
    const auto* Water = UDownstreamWaterSubsystem::GetForWorld(const_cast<ADownstreamRaider*>(this));
    if (!Water || OutpostIndex < 0) return Base;
    // Fend off the robot: a digger working near the levee outranks a distant player.
    const float Threat = Water->GetOutpostRadiusCm(OutpostIndex) + GetTuning()->DiggerThreatRadius;
    TArray<AActor*> Robots;
    UGameplayStatics::GetAllActorsWithTag(const_cast<ADownstreamRaider*>(this), TEXT("DownstreamDigger"), Robots);
    for (AActor* Robot : Robots)
    {
        if (!IsTargetAlive(Robot)) continue;
        if (FVector::DistSquared2D(Robot->GetActorLocation(), Water->GetOutpostCenter(OutpostIndex)) > FMath::Square(Threat)) continue;
        const ACharacter* Player = UGameplayStatics::GetPlayerCharacter(const_cast<ADownstreamRaider*>(this), 0);
        const bool bPlayerInFace = Base == Player && Player && FVector::DistSquared2D(Player->GetActorLocation(), GetActorLocation()) < FMath::Square(450.0f);
        if (!bPlayerInFace) return Robot;
    }
    return Base;
}

void ADownstreamRaider::Tick(float DeltaSeconds)
{
    LastDeltaSeconds = DeltaSeconds;
    if (RaiderRole != EDownstreamRaiderRole::Engineer)
    {
        Super::Tick(DeltaSeconds);
        return;
    }
    // Engineers skip the base fight loop entirely.
    ADownstreamCombatCharacter::Tick(DeltaSeconds);
    if (IsDead()) return;
    TickEngineer(DeltaSeconds);
}

void ADownstreamRaider::TickEngineer(float DeltaSeconds)
{
    bDamming = false;
    auto* Water = UDownstreamWaterSubsystem::GetForWorld(this);
    FIntPoint Breach;
    if (Water && OutpostIndex >= 0 && !Water->IsOutpostRazed(OutpostIndex) && Water->GetOutpostBreach(OutpostIndex, Breach))
    {
        // Stand on the bank beside the cut, not in it: the highest, driest neighbouring cell.
        // The two engineers take different banks so they don't shoulder each other into the water.
        TArray<TPair<float, FIntPoint>> Banks;
        for (int32 DY = -1; DY <= 1; ++DY)
            for (int32 DX = -1; DX <= 1; ++DX)
            {
                const FIntPoint Cell(Breach.X + DX, Breach.Y + DY);
                if ((DX == 0 && DY == 0) || Cell.X < 0 || Cell.Y < 0 || Cell.X >= Water->GetGridSize().X || Cell.Y >= Water->GetGridSize().Y) continue;
                Banks.Add({Water->GetCellGround(Cell) - 2.0f * Water->GetCellWater(Cell), Cell});
            }
        Banks.Sort([](const TPair<float, FIntPoint>& A, const TPair<float, FIntPoint>& B) { return A.Key > B.Key; });
        const int32 Pick = Banks.Num() > 1 ? static_cast<int32>(GetUniqueID() % 2) : 0;
        const FVector Target = Banks.Num() ? Water->CellToWorld(Banks[Pick].Value) : Water->CellToWorld(Breach);
        const float Distance = FVector::Dist2D(GetActorLocation(), Target);
        if (GetWorld()->GetTimeSeconds() >= NextDebugLog)
        {
            NextDebugLog = GetWorld()->GetTimeSeconds() + 2.0;
            UE_LOG(LogTemp, Verbose, TEXT("DOWNSTREAM_ENGINEER %s dist=%.0f direct=%d best=%.0f since=%.1f vel=%s"), *GetName(), Distance, bDirectToBreach ? 1 : 0,
                BestBreachDistance, GetWorld()->GetTimeSeconds() - LastProgressTime, *GetVelocity().ToString());
        }
        if (Distance > 140.0f)
        {
            // Navmesh first; if that makes no headway (flooded bowl, fresh trench), wade straight at it.
            const double Now = GetWorld()->GetTimeSeconds();
            if (Distance < BestBreachDistance - 40.0f) { BestBreachDistance = Distance; LastProgressTime = Now; }
            if (Now - LastProgressTime > 2.5 && !bDirectToBreach) { bDirectToBreach = true; LastProgressTime = Now; }
            if (bDirectToBreach)
            {
                if (auto* AI = Cast<AAIController>(GetController())) AI->StopMovement();
                FVector Direction = (Target - GetActorLocation()).GetSafeNormal2D();
                // Still stuck wading straight: slide sideways around whatever is in the way for a moment.
                if (Now - LastProgressTime > 1.5 && Now >= SidestepUntil)
                {
                    SidestepSign = -SidestepSign;
                    SidestepUntil = Now + 1.2;
                    LastProgressTime = Now;
                }
                if (Now < SidestepUntil) Direction = (Direction + FVector(-Direction.Y, Direction.X, 0.0f) * (1.6f * SidestepSign)).GetSafeNormal2D();
                ScrambleStep(Direction, DeltaSeconds);
                if (!Direction.IsNearlyZero()) SetActorRotation(Direction.Rotation());
            }
            else MoveToward(Target, 60.0f);
            RoleLabel->SetText(FText::FromString(TEXT("ENGINEER / to breach")));
        }
        else
        {
            if (GetCharacterMovement()->MovementMode == MOVE_Flying) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            StopMoving();
            bDamming = true;
            const FVector Face = (Water->CellToWorld(Breach) - GetActorLocation()).GetSafeNormal2D();
            if (!Face.IsNearlyZero()) SetActorRotation(Face.Rotation());
            const double Now = GetWorld()->GetTimeSeconds();
            if (Now >= NextDam)
            {
                NextDam = Now + FMath::Max(0.1f, GetTuning()->DamActionSeconds);
                if (Water->PerformDamAction(this, OutpostIndex))
                {
                    ++DamActions;
                    if (USoundBase* Thud = GetTuning()->SandbagSound ? GetTuning()->SandbagSound.Get() : GetTuning()->HitSound.Get())
                        UGameplayStatics::PlaySoundAtLocation(this, Thud, Target, 0.7f, FMath::FRandRange(0.9f, 1.1f));
                }
            }
            RoleLabel->SetText(FText::FromString(TEXT("ENGINEER / sandbagging")));
        }
    }
    else
    {
        ReturnToPost();
        RoleLabel->SetText(FText::FromString(TEXT("ENGINEER")));
    }
    if (!bDamming && !(Water && OutpostIndex >= 0 && Water->GetOutpostBreach(OutpostIndex, Breach)))
    {
        BestBreachDistance = TNumericLimits<float>::Max();
        LastProgressTime = GetWorld()->GetTimeSeconds();
        bDirectToBreach = false;
        if (GetCharacterMovement()->MovementMode == MOVE_Flying) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }
    CarryPhase = bDamming ? CarryPhase + DeltaSeconds * 9.0f : 0.0f;
    Sandbag->SetRelativeLocation(bDamming ? FVector(55.0f, 0.0f, -40.0f + 30.0f * FMath::Abs(FMath::Sin(CarryPhase))) : FVector(-30.0f, 0.0f, 20.0f));
}

void ADownstreamRaider::ScrambleStep(const FVector& Direction, float DeltaSeconds)
{
    // Kinematic scramble: glide along the terrain surface (levee slopes, trench walls) with a swept
    // horizontal step, independent of path following and of gravity. Used when the navmesh can't
    // get a raider out of (or into) an outpost bowl.
    auto* Water = UDownstreamWaterSubsystem::GetForWorld(this);
    if (!Water || Direction.IsNearlyZero()) return;
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (Movement->MovementMode != MOVE_Flying) Movement->SetMovementMode(MOVE_Flying);
    Movement->Velocity = FVector::ZeroVector;
    const float Speed = FMath::Max(120.0f, Movement->MaxWalkSpeed);
    const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector From = GetActorLocation();
    const FVector Step = Direction.GetSafeNormal2D() * Speed * DeltaSeconds;
    // Highest terrain under the capsule's footprint (slopes rise fast across a 40 cm radius).
    auto FootprintGround = [this, Water](const FVector& At)
    {
        float Highest = -1.0e6f;
        const float R = GetCapsuleComponent()->GetScaledCapsuleRadius() + 30.0f;
        for (const FVector& Offset : {FVector::ZeroVector, FVector(R, 0, 0), FVector(-R, 0, 0), FVector(0, R, 0), FVector(0, -R, 0)})
        {
            float Z = 0.0f;
            if (Water->GetGroundZAt(At + Offset, Z)) Highest = FMath::Max(Highest, Z);
        }
        return Highest;
    };
    const float HighZ = FMath::Max(From.Z, FMath::Max(FootprintGround(From), FootprintGround(From + Step)) + HalfHeight + 12.0f);
    SetActorLocation(FVector(From.X, From.Y, HighZ), false);
    FHitResult Hit;
    AddActorWorldOffset(FVector(Step.X, Step.Y, 0.0f), true, &Hit);
    const float Settle = FootprintGround(GetActorLocation());
    if (Settle > -1.0e5f) SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, Settle + HalfHeight + 12.0f), false);
    SetActorRotation(Direction.GetSafeNormal2D().Rotation());
}

void ADownstreamRaider::MoveToward(const FVector& Location, float AcceptanceRadius)
{
    // Guards fighting across a levee: when path following makes no headway (the bowl is often a
    // navmesh island), scramble straight at the target for a few seconds.
    const double Now = GetWorld()->GetTimeSeconds();
    const float Distance = FVector::Dist2D(GetActorLocation(), Location);
    if (Distance <= AcceptanceRadius) { StopMoving(); return; }
    if (Distance < MoveBestDistance - 40.0f) { MoveBestDistance = Distance; MoveProgressTime = Now; }
    if (Now - MoveProgressTime > 2.0)
    {
        ScrambleUntil = Now + 3.0;
        MoveProgressTime = Now;
        MoveBestDistance = Distance;
    }
    if (Now < ScrambleUntil)
    {
        if (auto* AI = Cast<AAIController>(GetController())) AI->StopMovement();
        ScrambleStep(Location - GetActorLocation(), LastDeltaSeconds);
        return;
    }
    if (GetCharacterMovement()->MovementMode == MOVE_Flying) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Super::MoveToward(Location, AcceptanceRadius);
}

void ADownstreamRaider::StopMoving()
{
    MoveBestDistance = TNumericLimits<float>::Max();
    MoveProgressTime = GetWorld()->GetTimeSeconds();
    ScrambleUntil = 0.0;
    if (GetCharacterMovement()->MovementMode == MOVE_Flying) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Super::StopMoving();
}

void ADownstreamRaider::OnCombatDeath()
{
    Super::OnCombatDeath();
    Sandbag->SetVisibility(false);
    RoleLabel->SetText(FText::GetEmpty());
    GetMesh()->SetSimulatePhysics(false);
    GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 80.0f));
}
