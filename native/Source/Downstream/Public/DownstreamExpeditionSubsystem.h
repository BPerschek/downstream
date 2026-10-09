#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DownstreamExpeditionState.h"
#include "DownstreamExpeditionSubsystem.generated.h"

class UDownstreamPrototypeTuning;
class ADownstreamSalvagePickup;

// Game-thread adapter. Widgets read these accessors and issue requests; they never own inventory.
UCLASS()
class DOWNSTREAM_API UDownstreamExpeditionSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

    static const UDownstreamPrototypeTuning* GetTuning(UWorld* World);
    UFUNCTION(BlueprintPure, Category="Downstream|Expedition") static UDownstreamExpeditionSubsystem* GetForWorld(UObject* ContextObject);

    UFUNCTION(BlueprintPure, Category="Downstream|Expedition") int32 GetCarriedSalvage() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Expedition") int32 GetBankedSalvage() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Expedition") int32 GetInventoryCapacity() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Expedition") float GetRangedDamageMultiplier() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Expedition") FName GetSelectedSkillId() const;
    UFUNCTION(BlueprintCallable, Category="Downstream|Expedition") bool ChooseStarterSkill(FName SkillId);
    UFUNCTION(BlueprintPure, Category="Downstream|Expedition") bool IsAtHome(FVector Location) const;
    UFUNCTION(BlueprintCallable, Category="Downstream|Expedition") bool TryInteract(AActor* Player);
    UFUNCTION(BlueprintCallable, Category="Downstream|Expedition") void HandlePlayerDeath(FVector DeathLocation);
    UFUNCTION(BlueprintCallable, Category="Downstream|Expedition") ADownstreamSalvagePickup* SpawnSalvage(FVector Location, int32 Amount, bool bRecoveryBag = false);
    UFUNCTION(BlueprintPure, Category="Downstream|Expedition") FText GetStatusMessage() const { return StatusMessage; }

private:
    void LoadSave();
    bool WriteSave(const downstream::ExpeditionState& Candidate);
    void SetStatus(const FString& Message);

    downstream::ExpeditionState State;
    UPROPERTY(Transient) TObjectPtr<UDownstreamPrototypeTuning> LoadedTuning;
    bool bTriedTuningLoad = false;
    bool bSaveBlocked = false;
    FText StatusMessage;
};
