#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "DownstreamCombatCharacter.generated.h"

class UAbilitySystemComponent;
class UDownstreamCombatAttributeSet;
class UDownstreamPrototypeTuning;
class UMaterialInstanceDynamic;
class UMeshComponent;
struct FOnAttributeChangeData;

UCLASS(Abstract)
class DOWNSTREAMCOMBAT_API ADownstreamCombatCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ADownstreamCombatCharacter();
    virtual void Tick(float DeltaSeconds) override;
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

    UFUNCTION(BlueprintPure, Category="Combat") float GetHealth() const;
    UFUNCTION(BlueprintPure, Category="Combat") float GetMaxHealth() const;
    UFUNCTION(BlueprintPure, Category="Combat") bool IsDead() const { return bDead; }
    UFUNCTION(BlueprintPure, Category="Combat") virtual bool IsPlayerCombatant() const { return false; }
    UFUNCTION(BlueprintCallable, Category="Combat") void ReceiveCombatDamage(float Damage, AActor* Source);
    // Metres of water at the pawn's feet (Drowned Vault site); zero elsewhere.
    UFUNCTION(BlueprintPure, Category="Combat") float GetWaterDepth() const { return WaterDepth; }

protected:
    virtual void BeginPlay() override;
    virtual float InitialHealth() const;
    virtual bool IsProtectedFromDamage() const { return false; }
    virtual void OnCombatDeath();
    const UDownstreamPrototypeTuning* GetTuning() const;
    void HealFully();
    void RegisterFeedbackMesh(UMeshComponent* FeedbackMesh, const FLinearColor& Color);
    void PlayCombatSound(bool bShot) const;
    void SetFeedbackColor(const FLinearColor& Color);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") TObjectPtr<UAbilitySystemComponent> AbilitySystem;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") TObjectPtr<UDownstreamCombatAttributeSet> CombatAttributes;

private:
    void OnHealthChanged(const FOnAttributeChangeData& Data);
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> FeedbackMaterials;
    FLinearColor BaseFeedbackColor = FLinearColor::White;
    float DamageFlashRemaining = 0.0f;
    float WadeBaseSpeed = 0.0f;
    float WaterDepth = 0.0f;
    bool bDead = false;
};
