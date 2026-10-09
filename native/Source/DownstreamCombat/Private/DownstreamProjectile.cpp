#include "DownstreamProjectile.h"
#include "DownstreamCombatCharacter.h"
#include "DownstreamCombatRules.h"
#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamPrototypeTuning.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ADownstreamProjectile::ADownstreamProjectile()
{
    PrimaryActorTick.bCanEverTick = false;
    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    SetRootComponent(Collision);
    Collision->InitSphereRadius(13.0f);
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Collision->SetCollisionObjectType(ECC_WorldDynamic);
    Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Collision->SetGenerateOverlapEvents(true);
    Collision->SetNotifyRigidBodyCollision(true);
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileVisual"));
    Visual->SetupAttachment(Collision);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetRelativeScale3D(FVector(0.28f));
    Visual->SetCastShadow(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Sphere.Succeeded()) Visual->SetStaticMesh(Sphere.Object);
    Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    Movement->SetUpdatedComponent(Collision);
    Movement->ProjectileGravityScale = 0.0f;
    Movement->bRotationFollowsVelocity = true;
    Movement->bInitialVelocityInLocalSpace = false;
    Movement->bShouldBounce = false;
    Movement->bForceSubStepping = true;
    Collision->OnComponentBeginOverlap.AddDynamic(this, &ADownstreamProjectile::OnOverlap);
    Collision->OnComponentHit.AddDynamic(this, &ADownstreamProjectile::OnHit);
}

void ADownstreamProjectile::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner()) Collision->IgnoreActorWhenMoving(GetOwner(), true);
    if (GetInstigator()) Collision->IgnoreActorWhenMoving(GetInstigator(), true);
}

void ADownstreamProjectile::Launch(const FVector& Direction, float Damage, float Speed, float Range, bool bPlayerFaction)
{
    DamageAmount = FMath::Max(0.0f, Damage);
    bFromPlayer = bPlayerFaction;
    const float SafeSpeed = FMath::Max(1.0f, Speed);
    Movement->InitialSpeed = SafeSpeed;
    Movement->MaxSpeed = SafeSpeed;
    Movement->Velocity = Direction.GetSafeNormal() * SafeSpeed;
    SetLifeSpan(FMath::Max(0.01f, Range / SafeSpeed));
    const auto* Tuning = UDownstreamExpeditionSubsystem::GetTuning(GetWorld());
    if (Tuning->ProjectileMaterial) Visual->SetMaterial(0, Tuning->ProjectileMaterial);
    if (UMaterialInstanceDynamic* Material = Visual->CreateAndSetMaterialInstanceDynamic(0))
    {
        const FLinearColor Color = bFromPlayer ? FLinearColor(0.15f, 3.0f, 2.0f) : FLinearColor(5.0f, 0.35f, 0.1f);
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Material->SetVectorParameterValue(TEXT("Tint"), Color);
        Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
    }
}

void ADownstreamProjectile::OnOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (bConsumed || OtherActor == GetOwner() || OtherActor == GetInstigator()) return;
    auto* Target = Cast<ADownstreamCombatCharacter>(OtherActor);
    if (!Target || !downstream::CanDamage(bFromPlayer, Target->IsPlayerCombatant(), Target->IsDead(), false)) return;
    bConsumed = true;
    Target->ReceiveCombatDamage(DamageAmount, GetOwner());
    Destroy();
}

void ADownstreamProjectile::OnHit(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, FVector, const FHitResult&)
{
    if (!bConsumed && OtherActor != GetOwner() && OtherActor != GetInstigator())
    {
        bConsumed = true;
        Destroy();
    }
}
