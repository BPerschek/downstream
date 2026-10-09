#include "DownstreamCombatAttributeSet.h"
#include "GameplayEffectExtension.h"

UDownstreamCombatAttributeSet::UDownstreamCombatAttributeSet()
{
    InitMaxHealth(100.0f);
    InitHealth(100.0f);
}

void UDownstreamCombatAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);
    if (Attribute == GetMaxHealthAttribute()) NewValue = FMath::Max(1.0f, NewValue);
    if (Attribute == GetHealthAttribute()) NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
}

void UDownstreamCombatAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);
    if (Data.EvaluatedData.Attribute == GetHealthAttribute())
        SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
    else if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
    {
        SetMaxHealth(FMath::Max(1.0f, GetMaxHealth()));
        SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
    }
}
