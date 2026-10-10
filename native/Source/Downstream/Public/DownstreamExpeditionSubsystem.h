#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DownstreamExpeditionState.h"
#include "DownstreamSettlementState.h"
#include "DownstreamEarthworks.h"
#include "DownstreamSettlementTypes.h"
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

    UFUNCTION(BlueprintPure, Category="Downstream|Settlement") TArray<FDownstreamStructureRecord> GetBuiltStructures() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Settlement") bool HasStructure(EDownstreamStructureType Type) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Settlement") int32 GetStructureCost(EDownstreamStructureType Type) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Settlement") bool CanPlaceStructure(EDownstreamStructureType Type, FVector Location) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Settlement") FVector GetSnappedBuildLocation(FVector Location) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Settlement") FVector GetBuildCellWorldLocation(FIntPoint Cell) const;
    UFUNCTION(BlueprintCallable, Category="Downstream|Settlement") bool TryPlaceStructure(EDownstreamStructureType Type, FVector Location, AActor* Player);
    UFUNCTION(BlueprintPure, Category="Downstream|Settlement") bool IsHaulerConstructed() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Settlement") int32 GetHaulerCost() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Settlement") int32 GetHaulerCargoCapacity() const;
    UFUNCTION(BlueprintCallable, Category="Downstream|Settlement") bool TryConstructHauler(AActor* Player);
    UFUNCTION(BlueprintPure, Category="Downstream|Settlement") bool IsHaulingEnabled() const;
    UFUNCTION(BlueprintCallable, Category="Downstream|Settlement") bool SetHaulingEnabled(bool bEnabled, AActor* Player);
    UFUNCTION(BlueprintPure, Category="Downstream|Settlement") float GetHaulerBattery() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Settlement") int32 GetHaulerCargo() const;
    UFUNCTION(BlueprintCallable, Category="Downstream|Settlement") bool TryLoadHaulerCargo(ADownstreamSalvagePickup* Pickup, AActor* Hauler);
    UFUNCTION(BlueprintCallable, Category="Downstream|Settlement") bool TryUnloadHaulerCargo(AActor* Hauler);
    UFUNCTION(BlueprintCallable, Category="Downstream|Settlement") void UpdateHaulerBattery(float SignedDelta);
    UFUNCTION(BlueprintCallable, Category="Downstream|Settlement") bool SaveHaulerCheckpoint();
    UPROPERTY(BlueprintAssignable, Category="Downstream|Settlement") FDownstreamSettlementChanged OnSettlementChanged;

    // Earthworks (save schema 3): digger ownership, vault state and dug terrain.
    UFUNCTION(BlueprintPure, Category="Downstream|Earthworks") bool IsDiggerBuilt() const { return Earthworks.IsDiggerBuilt(); }
    UFUNCTION(BlueprintPure, Category="Downstream|Earthworks") int32 GetDiggerCost() const { return Earthworks.GetDiggerCost(); }
    UFUNCTION(BlueprintCallable, Category="Downstream|Earthworks") bool TryConstructDigger(AActor* Player);
    UFUNCTION(BlueprintPure, Category="Downstream|Earthworks") bool IsVaultOpened() const { return Earthworks.IsVaultOpened(); }
    UFUNCTION(BlueprintPure, Category="Downstream|Earthworks") int32 GetSavedTerrainDeltaCount() const { return static_cast<int32>(Earthworks.GetTerrain().size()); }
    UFUNCTION(BlueprintPure, Category="Downstream|Expedition") bool IsSaveBlocked() const { return bSaveBlocked; }
    UFUNCTION(BlueprintCallable, Category="Downstream|Expedition") void PostStatus(const FString& Message) { SetStatus(Message); }
    // Commits only when the save write succeeds.
    bool CommitVaultOpened();
    bool CommitEarthworksTerrain(const downstream::WaterGrid& Grid);
    const downstream::EarthworksState& GetEarthworks() const { return Earthworks; }

private:
    void LoadSave();
    bool WriteSave(const downstream::ExpeditionState& Candidate, const downstream::SettlementState& CandidateSettlement);
    bool WriteSave(const downstream::ExpeditionState& Candidate, const downstream::SettlementState& CandidateSettlement,
        const downstream::EarthworksState& CandidateEarthworks);
    bool TryGetBuildCell(FVector Location, FIntPoint& Cell) const;
    void SetStatus(const FString& Message);

    downstream::ExpeditionState State;
    downstream::SettlementState Settlement;
    downstream::EarthworksState Earthworks;
    bool bHaulerBatteryDirty = false;
    UPROPERTY(Transient) TObjectPtr<UDownstreamPrototypeTuning> LoadedTuning;
    bool bTriedTuningLoad = false;
    bool bSaveBlocked = false;
    FText StatusMessage;
};
