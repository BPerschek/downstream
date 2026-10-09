#include "DownstreamCombatCharacter.h"
#include "DownstreamCombatAttributeSet.h"
#include "DownstreamCombatRules.h"
#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamPrototypeTuning.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

ADownstreamCombatCharacter::ADownstreamCombatCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(false);
    CombatAttributes = CreateDefaultSubobject<UDownstreamCombatAttributeSet>(TEXT("CombatAttributes"));
    bUseControllerRotationYaw = false;
}

void ADownstreamCombatCharacter::BeginPlay()
{
    Super::BeginPlay();
    AbilitySystem->InitAbilityActorInfo(this, this);
    AbilitySystem->AddAttributeSetSubobject(CombatAttributes.Get());
    const float Health = FMath::Max(1.0f, InitialHealth());
    AbilitySystem->SetNumericAttributeBase(UDownstreamCombatAttributeSet::GetMaxHealthAttribute(), Health);
    AbilitySystem->SetNumericAttributeBase(UDownstreamCombatAttributeSet::GetHealthAttribute(), Health);
    AbilitySystem->GetGameplayAttributeValueChangeDelegate(UDownstreamCombatAttributeSet::GetHealthAttribute())
        .AddUObject(this, &ADownstreamCombatCharacter::OnHealthChanged);
}

void ADownstreamCombatCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (DamageFlashRemaining > 0.0f)
    {
        DamageFlashRemaining = FMath::Max(0.0f, DamageFlashRemaining - DeltaSeconds);
        SetFeedbackColor(FMath::Lerp(BaseFeedbackColor, FLinearColor(5.0f, 1.0f, 0.7f), DamageFlashRemaining / 0.15f));
    }
}

UAbilitySystemComponent* ADownstreamCombatCharacter::GetAbilitySystemComponent() const { return AbilitySystem; }
float ADownstreamCombatCharacter::GetHealth() const { return CombatAttributes ? CombatAttributes->GetHealth() : 0.0f; }
float ADownstreamCombatCharacter::GetMaxHealth() const { return CombatAttributes ? CombatAttributes->GetMaxHealth() : 0.0f; }
float ADownstreamCombatCharacter::InitialHealth() const { return 100.0f; }
const UDownstreamPrototypeTuning* ADownstreamCombatCharacter::GetTuning() const { return UDownstreamExpeditionSubsystem::GetTuning(GetWorld()); }

float ADownstreamCombatCharacter::TakeDamage(float DamageAmount, const FDamageEvent&, AController*, AActor* DamageCauser)
{
    const float Previous = GetHealth();
    ReceiveCombatDamage(DamageAmount, DamageCauser);
    return Previous - GetHealth();
}

void ADownstreamCombatCharacter::ReceiveCombatDamage(float Damage, AActor* Source)
{
    if (!FMath::IsFinite(Damage) || Damage <= 0.0f || bDead || IsProtectedFromDamage()) return;
    if (const auto* Attacker = Cast<ADownstreamCombatCharacter>(Source))
        if (!downstream::CanDamage(Attacker->IsPlayerCombatant(), IsPlayerCombatant(), bDead, IsProtectedFromDamage())) return;
    AbilitySystem->SetNumericAttributeBase(UDownstreamCombatAttributeSet::GetHealthAttribute(),
        FMath::Max(0.0f, GetHealth() - Damage));
}

void ADownstreamCombatCharacter::OnHealthChanged(const FOnAttributeChangeData& Data)
{
    if (bDead) return;
    if (Data.NewValue < Data.OldValue)
    {
        DamageFlashRemaining = 0.15f;
        SetFeedbackColor(FLinearColor(5.0f, 1.0f, 0.7f));
        PlayCombatSound(false);
    }
    if (Data.NewValue <= 0.0f)
    {
        bDead = true;
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        OnCombatDeath();
    }
}

void ADownstreamCombatCharacter::OnCombatDeath() { SetLifeSpan(1.0f); }
void ADownstreamCombatCharacter::HealFully()
{
    if (!bDead) AbilitySystem->SetNumericAttributeBase(UDownstreamCombatAttributeSet::GetHealthAttribute(), GetMaxHealth());
}

void ADownstreamCombatCharacter::RegisterFeedbackMesh(UMeshComponent* FeedbackMesh, const FLinearColor& Color)
{
    if (!FeedbackMesh) return;
    BaseFeedbackColor = Color;
    for (int32 Slot = 0; Slot < FeedbackMesh->GetNumMaterials(); ++Slot)
        if (UMaterialInstanceDynamic* Material = FeedbackMesh->CreateAndSetMaterialInstanceDynamic(Slot)) FeedbackMaterials.Add(Material);
    SetFeedbackColor(Color);
}

void ADownstreamCombatCharacter::SetFeedbackColor(const FLinearColor& Color)
{
    for (UMaterialInstanceDynamic* Material : FeedbackMaterials)
    {
        if (!Material) continue;
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Material->SetVectorParameterValue(TEXT("Tint"), Color);
        Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
    }
}

void ADownstreamCombatCharacter::PlayCombatSound(bool bShot) const
{
    const UDownstreamPrototypeTuning* Tuning = GetTuning();
    USoundBase* Sound = bShot ? Tuning->ShotSound.Get() : Tuning->HitSound.Get();
    if (Sound) UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), bShot ? 0.4f : 0.55f);
}
