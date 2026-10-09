#include "DownstreamPlayerCharacter.h"
#include "DownstreamEnemy.h"
#include "DownstreamProjectile.h"
#include "DownstreamGameMode.h"
#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamPrototypeTuning.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ADownstreamPlayerCharacter::ADownstreamPlayerCharacter()
{
    GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
    GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
    GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Manny(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
    if (Manny.Succeeded()) GetMesh()->SetSkeletalMesh(Manny.Object);
    static ConstructorHelpers::FClassFinder<UAnimInstance> Unarmed(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
    if (Unarmed.Succeeded()) GetMesh()->SetAnimInstanceClass(Unarmed.Class);
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->MaxWalkSpeed = 550.0f;
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->TargetArmLength = 1850.0f;
    CameraBoom->bDoCollisionTest = false;
    CameraBoom->bUsePawnControlRotation = false;
    CameraBoom->SetUsingAbsoluteRotation(true);
    CameraBoom->SetRelativeRotation(FRotator(-55.0f, 45.0f, 0.0f));
    IsometricCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("IsometricCamera"));
    IsometricCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    IsometricCamera->bUsePawnControlRotation = false;
    IsometricCamera->FieldOfView = 55.0f;
}

void ADownstreamPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
    GetCharacterMovement()->MaxWalkSpeed = GetTuning()->PlayerMoveSpeed;
    OriginalGroundFriction = GetCharacterMovement()->GroundFriction;
    OriginalBraking = GetCharacterMovement()->BrakingDecelerationWalking;
    AimLocation = GetActorLocation() + GetActorForwardVector() * 1000.0f;
    RegisterFeedbackMesh(GetMesh(), FLinearColor::White);
}

float ADownstreamPlayerCharacter::InitialHealth() const { return GetTuning()->PlayerHealth; }

void ADownstreamPlayerCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis(TEXT("MoveForward"), this, &ADownstreamPlayerCharacter::MoveForward);
    Input->BindAxis(TEXT("MoveRight"), this, &ADownstreamPlayerCharacter::MoveRight);
    Input->BindAction(TEXT("Fire"), IE_Pressed, this, &ADownstreamPlayerCharacter::StartFire);
    Input->BindAction(TEXT("Fire"), IE_Released, this, &ADownstreamPlayerCharacter::StopFire);
    Input->BindAction(TEXT("Melee"), IE_Pressed, this, &ADownstreamPlayerCharacter::PerformMelee);
    Input->BindAction(TEXT("Dodge"), IE_Pressed, this, &ADownstreamPlayerCharacter::Dodge);
    Input->BindAction(TEXT("Interact"), IE_Pressed, this, &ADownstreamPlayerCharacter::Interact);
    Input->BindAction(TEXT("SkillCombat"), IE_Pressed, this, &ADownstreamPlayerCharacter::ChooseCombatSkill);
    Input->BindAction(TEXT("SkillSalvage"), IE_Pressed, this, &ADownstreamPlayerCharacter::ChooseSalvageSkill);
}

void ADownstreamPlayerCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (IsDead()) return;
    if (bDodging && GetWorld()->GetTimeSeconds() >= DodgeEndTime) EndDodge();
    UpdateAim();
    if (bFiring && !bDodging) FireAt(AimLocation);
}

FVector ADownstreamPlayerCharacter::MovementDirection() const
{
    const FRotator CameraYaw(0.0f, CameraBoom->GetComponentRotation().Yaw, 0.0f);
    return (FRotationMatrix(CameraYaw).GetUnitAxis(EAxis::X) * ForwardInput
        + FRotationMatrix(CameraYaw).GetUnitAxis(EAxis::Y) * RightInput).GetSafeNormal();
}

void ADownstreamPlayerCharacter::MoveForward(float Value)
{
    ForwardInput = Value;
    if (!IsDead() && !bDodging)
        AddMovementInput(FRotationMatrix(FRotator(0.0f, CameraBoom->GetComponentRotation().Yaw, 0.0f)).GetUnitAxis(EAxis::X), Value);
}
void ADownstreamPlayerCharacter::MoveRight(float Value)
{
    RightInput = Value;
    if (!IsDead() && !bDodging)
        AddMovementInput(FRotationMatrix(FRotator(0.0f, CameraBoom->GetComponentRotation().Yaw, 0.0f)).GetUnitAxis(EAxis::Y), Value);
}

void ADownstreamPlayerCharacter::UpdateAim()
{
    auto* PlayerController = Cast<APlayerController>(GetController());
    FVector RayOrigin, RayDirection;
    if (PlayerController && PlayerController->DeprojectMousePositionToWorld(RayOrigin, RayDirection)
        && FMath::Abs(RayDirection.Z) > KINDA_SMALL_NUMBER)
    {
        const float Distance = (GetActorLocation().Z - RayOrigin.Z) / RayDirection.Z;
        if (Distance > 0.0f) AimLocation = RayOrigin + RayDirection * Distance;
    }
    const FVector Direction = (AimLocation - GetActorLocation()).GetSafeNormal2D();
    if (!Direction.IsNearlyZero()) SetActorRotation(Direction.Rotation());
}

void ADownstreamPlayerCharacter::StartFire() { bFiring = true; if (!IsDead()) { UpdateAim(); FireAt(AimLocation); } }
void ADownstreamPlayerCharacter::StopFire() { bFiring = false; }

void ADownstreamPlayerCharacter::FireAt(FVector WorldTarget)
{
    if (IsDead() || bDodging || WorldTarget.ContainsNaN()) return;
    FVector Direction = (WorldTarget - GetActorLocation()).GetSafeNormal2D();
    if (Direction.IsNearlyZero()) Direction = GetActorForwardVector();
    const auto* Tuning = GetTuning();
    if (!RangedCooldown.TryConsume(GetWorld()->GetTimeSeconds(), Tuning->RangedCooldown)) return;
    const FVector Muzzle = GetActorLocation() + Direction * 68.0f + FVector(0.0f, 0.0f, 12.0f);
    const FTransform SpawnTransform(Direction.Rotation(), Muzzle);
    if (auto* Projectile = GetWorld()->SpawnActorDeferred<ADownstreamProjectile>(ADownstreamProjectile::StaticClass(),
        SpawnTransform, this, this, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
    {
        const auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>();
        Projectile->Launch(Direction, Tuning->RangedDamage * (Expedition ? Expedition->GetRangedDamageMultiplier() : 1.0f),
            Tuning->ProjectileSpeed, Tuning->ProjectileRange, true);
        UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
        PlayCombatSound(true);
    }
}

void ADownstreamPlayerCharacter::PerformMelee()
{
    if (IsDead() || bDodging) return;
    UpdateAim();
    const auto* Tuning = GetTuning();
    if (!MeleeCooldown.TryConsume(GetWorld()->GetTimeSeconds(), Tuning->MeleeCooldown)) return;
    const FVector Center = GetActorLocation() + GetActorForwardVector() * Tuning->MeleeRadius * 0.45f;
    TArray<FOverlapResult> Hits;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DownstreamMelee), false, this);
    GetWorld()->OverlapMultiByObjectType(Hits, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
        FCollisionShape::MakeSphere(Tuning->MeleeRadius), Query);
    TSet<AActor*> Damaged;
    for (const FOverlapResult& Hit : Hits)
    {
        auto* Enemy = Cast<ADownstreamEnemy>(Hit.GetActor());
        if (!Enemy || Enemy->IsDead() || Damaged.Contains(Enemy)) continue;
        FHitResult Obstruction;
        const bool bHitWall = GetWorld()->LineTraceSingleByChannel(Obstruction, GetActorLocation(),
            Enemy->GetActorLocation(), ECC_Visibility, Query);
        if (bHitWall && Obstruction.GetActor() != Enemy) continue;
        Damaged.Add(Enemy);
        Enemy->ReceiveCombatDamage(Tuning->MeleeDamage, this);
    }
    DrawDebugCircle(GetWorld(), Center - FVector(0.0f, 0.0f, 65.0f), Tuning->MeleeRadius, 32,
        FColor::Cyan, false, 0.18f, 0, 5.0f, FVector::ForwardVector, FVector::RightVector, false);
    PlayCombatSound(true);
}

void ADownstreamPlayerCharacter::Dodge()
{
    if (IsDead() || bDodging) return;
    const auto* Tuning = GetTuning();
    if (!DodgeCooldown.TryConsume(GetWorld()->GetTimeSeconds(), Tuning->DodgeCooldown)) return;
    FVector Direction = MovementDirection();
    if (Direction.IsNearlyZero()) Direction = GetActorForwardVector();
    bDodging = true;
    DodgeEndTime = GetWorld()->GetTimeSeconds() + 0.22;
    GetCharacterMovement()->GroundFriction = 0.0f;
    GetCharacterMovement()->BrakingDecelerationWalking = 0.0f;
    LaunchCharacter(Direction * Tuning->DodgeStrength + FVector(0.0f, 0.0f, 60.0f), true, true);
}

void ADownstreamPlayerCharacter::EndDodge()
{
    bDodging = false;
    GetCharacterMovement()->GroundFriction = OriginalGroundFriction;
    GetCharacterMovement()->BrakingDecelerationWalking = OriginalBraking;
}

bool ADownstreamPlayerCharacter::IsProtectedFromDamage() const
{
    const auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>();
    return bDodging || (Expedition && Expedition->IsAtHome(GetActorLocation()));
}

void ADownstreamPlayerCharacter::Interact()
{
    if (IsDead()) return;
    if (auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>())
    {
        Expedition->TryInteract(this);
        if (Expedition->IsAtHome(GetActorLocation())) HealFully();
    }
}
void ADownstreamPlayerCharacter::ChooseCombatSkill()
{
    if (!IsDead()) if (auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>())
        Expedition->ChooseStarterSkill(TEXT("steady_aim"));
}
void ADownstreamPlayerCharacter::ChooseSalvageSkill()
{
    if (!IsDead()) if (auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>())
        Expedition->ChooseStarterSkill(TEXT("salvage_handling"));
}

void ADownstreamPlayerCharacter::OnCombatDeath()
{
    bFiring = false;
    EndDodge();
    if (auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>())
        Expedition->HandlePlayerDeath(GetActorLocation());
    if (auto* GameMode = GetWorld()->GetAuthGameMode<ADownstreamGameMode>())
        GameMode->RequestPlayerRespawn(GetController(), this);
    else SetLifeSpan(GetTuning()->RespawnDelay + 0.1f);
}
