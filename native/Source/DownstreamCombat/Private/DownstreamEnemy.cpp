#include "DownstreamEnemy.h"
#include "DownstreamEnemyController.h"
#include "DownstreamPlayerCharacter.h"
#include "DownstreamProjectile.h"
#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamPrototypeTuning.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ADownstreamEnemy::ADownstreamEnemy()
{
    PrimaryActorTick.TickInterval = 0.05f;
    GetCapsuleComponent()->InitCapsuleSize(42.0f, 88.0f);
    GetMesh()->SetVisibility(false);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->MaxWalkSpeed = 280.0f;
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = ADownstreamEnemyController::StaticClass();
    RobotMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SalvageBot"));
    RobotMesh->SetupAttachment(GetRootComponent());
    RobotMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
    RobotMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Bot(TEXT("/Game/Downstream/Art/SM_SalvageBot.SM_SalvageBot"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Fallback(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Bot.Succeeded()) RobotMesh->SetStaticMesh(Bot.Object);
    else if (Fallback.Succeeded())
    {
        RobotMesh->SetStaticMesh(Fallback.Object);
        RobotMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -10.0f));
    }
    ReactorCore = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReactorCore"));
    ReactorCore->SetupAttachment(GetRootComponent());
    ReactorCore->SetRelativeLocation(FVector(0.0f, 0.0f, 65.0f));
    ReactorCore->SetRelativeScale3D(FVector(0.36f));
    ReactorCore->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ReactorCore->SetCastShadow(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Sphere.Succeeded()) ReactorCore->SetStaticMesh(Sphere.Object);
}

float ADownstreamEnemy::InitialHealth() const
{
    return Kind == EDownstreamEnemyKind::Chaser ? GetTuning()->ChaserHealth : GetTuning()->ShooterHealth;
}

void ADownstreamEnemy::BeginPlay()
{
    Super::BeginPlay();
    Tags.AddUnique(TEXT("DownstreamHostile"));
    SpawnLocation = GetActorLocation();
    const auto* Tuning = GetTuning();
    const bool bChaser = Kind == EDownstreamEnemyKind::Chaser;
    GetCharacterMovement()->MaxWalkSpeed = bChaser ? Tuning->ChaserSpeed : Tuning->ShooterSpeed;
    RobotMesh->SetRelativeScale3D(FVector(bChaser ? 0.8f : 1.0f));
    if (Tuning->EnemyMaterial)
        for (int32 Slot = 0; Slot < RobotMesh->GetNumMaterials(); ++Slot) RobotMesh->SetMaterial(Slot, Tuning->EnemyMaterial);
    if (Tuning->ProjectileMaterial) ReactorCore->SetMaterial(0, Tuning->ProjectileMaterial);
    const FLinearColor Color = bChaser ? FLinearColor(0.85f, 0.19f, 0.06f) : FLinearColor(0.42f, 0.16f, 0.85f);
    RegisterFeedbackMesh(RobotMesh, Color);
    RegisterFeedbackMesh(ReactorCore, Color);
}

bool ADownstreamEnemy::IsTargetAlive(const AActor* Target)
{
    if (!IsValid(Target)) return false;
    if (const auto* Combatant = Cast<ADownstreamCombatCharacter>(Target)) return !Combatant->IsDead();
    // Robots drop the tag while wrecked.
    return Target->ActorHasTag(TEXT("DownstreamRobot"));
}

AActor* ADownstreamEnemy::ChooseTarget() const
{
    const auto* Tuning = GetTuning();
    auto* Player = Cast<ADownstreamPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(const_cast<ADownstreamEnemy*>(this), 0));
    const auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>();
    AActor* Best = nullptr;
    float BestDistance = FMath::Square(Tuning->AggroRange);
    if (Player && !Player->IsDead() && !(Expedition && Expedition->IsAtHome(Player->GetActorLocation())))
    {
        const float Distance = FVector::DistSquared2D(GetActorLocation(), Player->GetActorLocation());
        if (Distance <= BestDistance) { Best = Player; BestDistance = Distance; }
    }
    TArray<AActor*> Robots;
    UGameplayStatics::GetAllActorsWithTag(this, TEXT("DownstreamRobot"), Robots);
    for (AActor* Robot : Robots)
    {
        if (!IsTargetAlive(Robot) || (Expedition && Expedition->IsAtHome(Robot->GetActorLocation()))) continue;
        // Robots draw less attention than the player unless they are much closer.
        const float Distance = FVector::DistSquared2D(GetActorLocation(), Robot->GetActorLocation()) * 1.6f;
        if (Distance <= BestDistance) { Best = Robot; BestDistance = Distance; }
    }
    return Best;
}

void ADownstreamEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (IsDead()) return;
    AActor* Target = ChooseTarget();
    CurrentTarget = Target;
    if (!Target)
    {
        ReturnToPost();
        return;
    }
    FightTarget(Target);
}

void ADownstreamEnemy::ReturnToPost()
{
    if (FVector::DistSquared2D(GetActorLocation(), SpawnLocation) > FMath::Square(90.0f)) MoveToward(SpawnLocation, 65.0f);
    else StopMoving();
}

void ADownstreamEnemy::FightTarget(AActor* Target)
{
    const auto* Tuning = GetTuning();
    const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
    const float Distance = ToTarget.Size2D();
    if (!ToTarget.IsNearlyZero()) SetActorRotation(ToTarget.GetSafeNormal2D().Rotation());
    const bool bChaser = Kind == EDownstreamEnemyKind::Chaser;
    const float AttackRange = bChaser ? Tuning->ChaserAttackRange : Tuning->ShooterRange;
    const bool bLineOfSight = HasClearShot(Target);
    if (Distance > (bChaser ? AttackRange * 0.85f : AttackRange * 0.78f) || !bLineOfSight)
        MoveToward(Target->GetActorLocation(), bChaser ? AttackRange * 0.55f : AttackRange * 0.6f);
    else StopMoving();
    if (Distance <= AttackRange && bLineOfSight) AttackTarget(Target);
}

bool ADownstreamEnemy::HasClearShot(const AActor* Target) const
{
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DownstreamEnemySight), false, this);
    if (!GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation() + FVector(0.0f, 0.0f, 12.0f),
        Target->GetActorLocation() + FVector(0.0f, 0.0f, 12.0f), ECC_Visibility, Query)) return true;
    return Hit.GetActor() == Target;
}

void ADownstreamEnemy::MoveToward(const FVector& Location, float AcceptanceRadius)
{
    if (auto* AI = Cast<ADownstreamEnemyController>(GetController()))
    {
        if (AI->MoveForCombat(Location, AcceptanceRadius)) return;
    }
    // CharacterMovement still sweeps against walls; this only permits a useful preview before navmesh generation.
    if (FVector::DistSquared2D(GetActorLocation(), Location) > FMath::Square(AcceptanceRadius))
        AddMovementInput((Location - GetActorLocation()).GetSafeNormal2D());
}

void ADownstreamEnemy::StopMoving()
{
    if (auto* AI = Cast<ADownstreamEnemyController>(GetController())) AI->HaltForCombat();
    GetCharacterMovement()->StopMovementImmediately();
}

void ADownstreamEnemy::AttackTarget(AActor* Target)
{
    const auto* Tuning = GetTuning();
    const bool bChaser = Kind == EDownstreamEnemyKind::Chaser;
    if (!AttackCooldown.TryConsume(GetWorld()->GetTimeSeconds(), bChaser ? Tuning->ChaserCooldown : Tuning->ShooterCooldown)) return;
    if (bChaser)
    {
        if (auto* Combatant = Cast<ADownstreamCombatCharacter>(Target)) Combatant->ReceiveCombatDamage(Tuning->ChaserDamage, this);
        else UGameplayStatics::ApplyDamage(Target, Tuning->ChaserDamage, GetController(), this, nullptr);
        return;
    }
    const FVector Direction = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    const FTransform SpawnTransform(Direction.Rotation(),
        GetActorLocation() + Direction * 62.0f + FVector(0.0f, 0.0f, 12.0f));
    if (auto* Projectile = GetWorld()->SpawnActorDeferred<ADownstreamProjectile>(ADownstreamProjectile::StaticClass(),
        SpawnTransform, this, this, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
    {
        Projectile->Launch(Direction, Tuning->ShooterDamage, Tuning->EnemyProjectileSpeed, Tuning->ShooterRange + 100.0f, false);
        UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
        PlayCombatSound(true);
    }
}

void ADownstreamEnemy::OnCombatDeath()
{
    Tags.Remove(TEXT("DownstreamHostile"));
    StopMoving();
    if (auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>())
        if (SalvageDrop() > 0) Expedition->SpawnSalvage(GetActorLocation(), SalvageDrop());
    SetActorEnableCollision(false);
    RobotMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 75.0f));
    ReactorCore->SetVisibility(false);
    SetLifeSpan(0.45f);
}
