#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "DownstreamCombatAttributeSet.generated.h"

// Prototype health is already owned by GAS. Full ability assets can consume the same attributes.
UCLASS()
class DOWNSTREAMCOMBAT_API UDownstreamCombatAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UDownstreamCombatAttributeSet();
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

    UPROPERTY(BlueprintReadOnly, Category="Combat") FGameplayAttributeData Health;
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UDownstreamCombatAttributeSet, Health)
    GAMEPLAYATTRIBUTE_VALUE_GETTER(Health)
    GAMEPLAYATTRIBUTE_VALUE_SETTER(Health)
    GAMEPLAYATTRIBUTE_VALUE_INITTER(Health)

    UPROPERTY(BlueprintReadOnly, Category="Combat") FGameplayAttributeData MaxHealth;
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UDownstreamCombatAttributeSet, MaxHealth)
    GAMEPLAYATTRIBUTE_VALUE_GETTER(MaxHealth)
    GAMEPLAYATTRIBUTE_VALUE_SETTER(MaxHealth)
    GAMEPLAYATTRIBUTE_VALUE_INITTER(MaxHealth)
};
