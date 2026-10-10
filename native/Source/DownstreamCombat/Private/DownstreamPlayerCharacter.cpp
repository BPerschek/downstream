#include "DownstreamPlayerCharacter.h"
#include "DownstreamEnemy.h"
#include "DownstreamProjectile.h"
#include "DownstreamGameMode.h"
#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamPrototypeTuning.h"
#include "DownstreamSettlementManager.h"
#include "DownstreamWaterSubsystem.h"
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
    Input->BindAction(TEXT("BuildMenu"), IE_Pressed, this, &ADownstreamPlayerCharacter::ToggleBuildMenu);
    Input->BindAction(TEXT("BuildCharger"), IE_Pressed, this, &ADownstreamPlayerCharacter::ChooseCharger);
    Input->BindAction(TEXT("BuildHauler"), IE_Pressed, this, &ADownstreamPlayerCharacter::ConstructHauler);
    Input->BindAction(TEXT("ToggleHauling"), IE_Pressed, this, &ADownstreamPlayerCharacter::ToggleHauling);
    Input->BindAction(TEXT("BuildDigger"), IE_Pressed, this, &ADownstreamPlayerCharacter::ConstructDigger);
    Input->BindAction(TEXT("DigPlanner"), IE_Pressed, this, &ADownstreamPlayerCharacter::ToggleDigPlanner);
    Input->BindAction(TEXT("ClearDigQueue"), IE_Pressed, this, &ADownstreamPlayerCharacter::ClearDigQueue);
    Input->BindAction(TEXT("DigDepthUp"), IE_Pressed, this, &ADownstreamPlayerCharacter::DeepenCanals);
    Input->BindAction(TEXT("DigDepthDown"), IE_Pressed, this, &ADownstreamPlayerCharacter::ShallowCanals);
    Input->BindAction(TEXT("DevRespawnField"), IE_Pressed, this, &ADownstreamPlayerCharacter::RespawnFieldNow);
    Input->BindAction(TEXT("DevGrantSalvage"), IE_Pressed, this, &ADownstreamPlayerCharacter::DevGrantSalvage);
}

void ADownstreamPlayerCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (IsDead()) return;
    if (bDodging && GetWorld()->GetTimeSeconds() >= DodgeEndTime) EndDodge();
    UpdateAim();
    if (bBuildMenuOpen) UpdatePlacementPreview();
    else if (bDigPlannerOpen) UpdateCanalCursor();
    else if (bFiring && !bDodging) FireAt(AimLocation);
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

void ADownstreamPlayerCharacter::StartFire()
{
    if (bBuildMenuOpen) { PlaceSelectedStructure(); return; }
    if (bDigPlannerOpen) { BeginCanalDrag(); return; }
    bFiring = true;
    if (!IsDead()) { UpdateAim(); FireAt(AimLocation); }
}
void ADownstreamPlayerCharacter::StopFire()
{
    bFiring = false;
    if (bDigPlannerOpen && bDraggingCanal) FinishCanalDrag();
}

void ADownstreamPlayerCharacter::FireAt(FVector WorldTarget)
{
    if (IsDead() || bDodging || bBuildMenuOpen || bDigPlannerOpen || WorldTarget.ContainsNaN()) return;
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
    if (bBuildMenuOpen) { CancelPlacement(); return; }
    if (bDigPlannerOpen)
    {
        if (bDraggingCanal) bDraggingCanal = false;
        else ToggleDigPlanner();
        return;
    }
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
    // In a canal or trench the dodge becomes a scramble up the bank.
    const float Trench = GetTrenchDepth();
    const float Lift = Trench > 0.5f ? FMath::Clamp(250.0f + Trench * 330.0f, 400.0f, 1200.0f) : 60.0f;
    LaunchCharacter(Direction * Tuning->DodgeStrength * (Trench > 0.5f ? 0.55f : 1.0f) + FVector(0.0f, 0.0f, Lift), true, true);
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
    if (auto* Water = UDownstreamWaterSubsystem::GetForWorld(this))
        if (Water->TryInteractVault(this)) return;
    if (auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>())
    {
        Expedition->TryInteract(this);
        if (Expedition->IsAtHome(GetActorLocation())) HealFully();
    }
}
void ADownstreamPlayerCharacter::ChooseCombatSkill()
{
    if (bBuildMenuOpen) { SelectStructure(EDownstreamStructureType::Storage); return; }
    if (!IsDead()) if (auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>())
        Expedition->ChooseStarterSkill(TEXT("steady_aim"));
}
void ADownstreamPlayerCharacter::ChooseSalvageSkill()
{
    if (bBuildMenuOpen) { SelectStructure(EDownstreamStructureType::Workshop); return; }
    if (!IsDead()) if (auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>())
        Expedition->ChooseStarterSkill(TEXT("salvage_handling"));
}

void ADownstreamPlayerCharacter::OnCombatDeath()
{
    bFiring = false;
    bBuildMenuOpen = false;
    bDigPlannerOpen = false;
    bDraggingCanal = false;
    CancelPlacement();
    EndDodge();
    if (auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>())
        Expedition->HandlePlayerDeath(GetActorLocation());
    if (auto* GameMode = GetWorld()->GetAuthGameMode<ADownstreamGameMode>())
        GameMode->RequestPlayerRespawn(GetController(), this);
    else SetLifeSpan(GetTuning()->RespawnDelay + 0.1f);
}

void ADownstreamPlayerCharacter::ToggleBuildMenu()
{
    if (IsDead()) return;
    auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>();
    if (!bBuildMenuOpen && (!Expedition || !Expedition->IsAtHome(GetActorLocation()))) return;
    bBuildMenuOpen = !bBuildMenuOpen;
    if (bBuildMenuOpen) { bDigPlannerOpen = false; bDraggingCanal = false; }
    StopFire();
    if (!bBuildMenuOpen) CancelPlacement();
}

void ADownstreamPlayerCharacter::SelectStructure(EDownstreamStructureType Type)
{
    if (!bBuildMenuOpen || IsDead()) return;
    SelectedStructure = Type;
    bPlacementSelected = true;
    UpdatePlacementPreview();
}

void ADownstreamPlayerCharacter::ChooseCharger()
{
    SelectStructure(EDownstreamStructureType::Charger);
}

void ADownstreamPlayerCharacter::UpdatePlacementPreview()
{
    auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>();
    if (!Expedition || !Expedition->IsAtHome(GetActorLocation()))
    {
        bBuildMenuOpen = false;
        CancelPlacement();
        return;
    }
    if (!bPlacementSelected) return;
    auto* PC = Cast<APlayerController>(GetController());
    FHitResult Cursor;
    if (PC && PC->GetHitResultUnderCursor(ECC_Visibility, false, Cursor))
        PlacementLocation = Expedition->GetSnappedBuildLocation(Cursor.ImpactPoint);
    else
        PlacementLocation = Expedition->GetSnappedBuildLocation(AimLocation);
    if (auto* Manager = ADownstreamSettlementManager::GetForWorld(this))
        Manager->SetPlacementPreview(SelectedStructure, PlacementLocation, true);
}

bool ADownstreamPlayerCharacter::PlaceSelectedStructure()
{
    if (!bBuildMenuOpen || !bPlacementSelected || IsDead()) return false;
    UpdatePlacementPreview();
    auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>();
    if (!Expedition || !Expedition->TryPlaceStructure(SelectedStructure, PlacementLocation, this)) return false;
    CancelPlacement();
    return true;
}

void ADownstreamPlayerCharacter::CancelPlacement()
{
    bPlacementSelected = false;
    if (auto* Manager = ADownstreamSettlementManager::GetForWorld(this)) Manager->ClearPlacementPreview();
}

void ADownstreamPlayerCharacter::ConstructHauler()
{
    if (!bBuildMenuOpen || IsDead()) return;
    if (auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>())
        if (Expedition->TryConstructHauler(this)) CancelPlacement();
}

void ADownstreamPlayerCharacter::ToggleHauling()
{
    if (!IsDead()) if (auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>())
        Expedition->SetHaulingEnabled(!Expedition->IsHaulingEnabled(), this);
}

void ADownstreamPlayerCharacter::ConstructDigger()
{
    if (!bBuildMenuOpen || IsDead()) return;
    if (auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>())
        if (Expedition->TryConstructDigger(this)) CancelPlacement();
}

void ADownstreamPlayerCharacter::ToggleDigPlanner()
{
    if (IsDead()) return;
    auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>();
    auto* Water = UDownstreamWaterSubsystem::GetForWorld(this);
    if (!bDigPlannerOpen)
    {
        if (!Water || !Water->IsSiteActive()) return;
        if (!Expedition || !Expedition->IsDiggerBuilt())
        {
            if (Expedition) Expedition->PostStatus(TEXT("No digger yet. At home: B, then 5 to assemble one at the workshop."));
            return;
        }
        if (bBuildMenuOpen) { bBuildMenuOpen = false; CancelPlacement(); }
        StopFire();
    }
    bDigPlannerOpen = !bDigPlannerOpen;
    bDraggingCanal = false;
    if (bDigPlannerOpen && Expedition)
        Expedition->PostStatus(TEXT("Dig planner: drag LMB from the water to where it should drain. Shift+wheel sets depth, RMB cancels, G closes."));
}

void ADownstreamPlayerCharacter::ClearDigQueue()
{
    if (IsDead()) return;
    if (auto* Water = UDownstreamWaterSubsystem::GetForWorld(this)) Water->ClearDigQueue();
}

void ADownstreamPlayerCharacter::UpdateCanalCursor()
{
    bCanalCursorValid = false;
    auto* Water = UDownstreamWaterSubsystem::GetForWorld(this);
    auto* PC = Cast<APlayerController>(GetController());
    if (!Water || !PC) return;
    FHitResult Cursor;
    if (PC->GetHitResultUnderCursor(ECC_Visibility, false, Cursor))
        bCanalCursorValid = Water->WorldToCell(Cursor.ImpactPoint, CanalCursor);
}

void ADownstreamPlayerCharacter::BeginCanalDrag()
{
    UpdateCanalCursor();
    if (!bCanalCursorValid) return;
    CanalDragStart = CanalCursor;
    bDraggingCanal = true;
}

void ADownstreamPlayerCharacter::FinishCanalDrag()
{
    UpdateCanalCursor();
    bDraggingCanal = false;
    if (bCanalCursorValid) PlanCanal(CanalDragStart, CanalCursor);
}

bool ADownstreamPlayerCharacter::PlanCanal(FIntPoint From, FIntPoint To)
{
    auto* Water = UDownstreamWaterSubsystem::GetForWorld(this);
    return !IsDead() && Water && Water->DesignateCanal(From, To, this);
}

void ADownstreamPlayerCharacter::DeepenCanals()
{
    if (!bDigPlannerOpen) return;
    if (auto* Water = UDownstreamWaterSubsystem::GetForWorld(this)) Water->AdjustCanalDepth(1);
}

void ADownstreamPlayerCharacter::ShallowCanals()
{
    if (!bDigPlannerOpen) return;
    if (auto* Water = UDownstreamWaterSubsystem::GetForWorld(this)) Water->AdjustCanalDepth(-1);
}

void ADownstreamPlayerCharacter::RespawnFieldNow()
{
    if (auto* GameMode = GetWorld()->GetAuthGameMode<ADownstreamGameMode>())
    {
        const int32 Count = GameMode->RespawnFieldNow();
        if (auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>())
            Expedition->PostStatus(FString::Printf(TEXT("Developer: respawned %d enemies/caches."), Count));
    }
}

float ADownstreamPlayerCharacter::GetTrenchDepth() const
{
    const auto* Water = UDownstreamWaterSubsystem::GetForWorld(const_cast<ADownstreamPlayerCharacter*>(this));
    FIntPoint Cell;
    if (!Water || !Water->WorldToCell(GetActorLocation(), Cell)) return 0.0f;
    const float Here = Water->GetCellGround(Cell);
    float Highest = Here;
    for (int32 DY = -2; DY <= 2; ++DY)
        for (int32 DX = -2; DX <= 2; ++DX)
        {
            const FIntPoint Near(Cell.X + DX, Cell.Y + DY);
            if (Near.X >= 0 && Near.Y >= 0 && Near.X < Water->GetGridSize().X && Near.Y < Water->GetGridSize().Y)
                Highest = FMath::Max(Highest, Water->GetCellGround(Near));
        }
    return Highest - Here;
}

void ADownstreamPlayerCharacter::DevGrantSalvage()
{
    if (auto* Expedition = GetWorld()->GetSubsystem<UDownstreamExpeditionSubsystem>()) Expedition->DevGrantSalvage(25);
}
