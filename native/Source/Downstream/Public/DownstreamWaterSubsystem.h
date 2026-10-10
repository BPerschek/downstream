#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DownstreamEarthworks.h"
#include "DownstreamWater.h"
#include <deque>
#include <memory>
#include "DownstreamWaterSubsystem.generated.h"

class ADownstreamFloodedVault;

// Game-thread adapter for the Drowned Vault water site. The plain C++ WaterGrid
// owns ground/water state; presentation actors read it, robots and the player
// submit requests here. Simulation stays on the game thread at a fixed 10 Hz
// (see M0_STATUS for the deferred worker-thread work).
UCLASS()
class DOWNSTREAM_API UDownstreamWaterSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Deinitialize() override;
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    UFUNCTION(BlueprintPure, Category="Downstream|Water") static UDownstreamWaterSubsystem* GetForWorld(UObject* ContextObject);

    // Called by the site actor. Builds the grid, applies saved terrain and settles the water.
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") bool ActivateSite(FVector Origin);
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool IsSiteActive() const { return Grid != nullptr; }
    UFUNCTION(BlueprintPure, Category="Downstream|Water") FVector GetSiteOrigin() const { return Origin; }
    UFUNCTION(BlueprintPure, Category="Downstream|Water") float GetCellSizeCm() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") FIntPoint GetGridSize() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool WorldToCell(FVector World, FIntPoint& Cell) const;
    // Centre of a cell at its ground surface.
    UFUNCTION(BlueprintPure, Category="Downstream|Water") FVector CellToWorld(FIntPoint Cell) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") float GetCellGround(FIntPoint Cell) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") float GetCellWater(FIntPoint Cell) const;
    // Metres of water at a world location; zero outside the site.
    UFUNCTION(BlueprintPure, Category="Downstream|Water") float GetWaterDepthAt(FVector World) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") float GetTotalWater() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") float GetDrainedVolume() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") int64 GetWaterTick() const;

    // Vault
    UFUNCTION(BlueprintPure, Category="Downstream|Water") FVector GetVaultLocation() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") float GetVaultDepth() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool IsVaultAccessible() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool IsPlayerNearVault(AActor* Player) const;
    // Returns true when the interaction was about the vault (even if refused).
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") bool TryInteractVault(AActor* Player);

    // Canal planning and digging
    UFUNCTION(BlueprintPure, Category="Downstream|Water") TArray<FIntPoint> PreviewCanal(FIntPoint From, FIntPoint To) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool IsCanalValid(FIntPoint From, FIntPoint To) const;
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") bool DesignateCanal(FIntPoint From, FIntPoint To, AActor* Player);
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") void ClearDigQueue();
    UFUNCTION(BlueprintPure, Category="Downstream|Water") int32 GetQueuedCanalCount() const { return static_cast<int32>(Jobs.size()); }
    UFUNCTION(BlueprintPure, Category="Downstream|Water") TArray<FIntPoint> GetQueuedCanalCells() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") float GetActiveCanalProgress() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool GetNextDigCell(FIntPoint& Cell) const;
    // Where the digger should stand for the next cell: the previous (already dug) canal cell,
    // so it advances along its own trench instead of climbing undug ground.
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool GetDigStandCell(FIntPoint& Cell) const;
    // One dig step on the active canal, if the digger stands within reach of the next cell.
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") bool PerformDigAction(AActor* Digger);
    UFUNCTION(BlueprintPure, Category="Downstream|Water") int32 GetCompletedCanalCount() const { return CompletedCanals; }

    // Developer view and tests
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") void ToggleFlowOverlay() { bFlowOverlay = !bFlowOverlay; }
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool IsFlowOverlayEnabled() const { return bFlowOverlay; }
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") void SimulateSeconds(float Seconds);
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") bool SaveTerrainNow();

    const downstream::WaterGrid* GetGrid() const { return Grid.get(); }

private:
    bool IsCellValid(FIntPoint Cell) const;
    void SpawnVault();
    void PlayFeedback(bool bSuccess, const FVector& Location) const;

    std::unique_ptr<downstream::WaterGrid> Grid;
    std::deque<downstream::DigJob> Jobs;
    FVector Origin = FVector::ZeroVector;
    UPROPERTY(Transient) TObjectPtr<ADownstreamFloodedVault> Vault;
    double NextTerrainSave = 0.0;
    double NextDigAction = 0.0;
    bool bTerrainDirty = false;
    bool bFlowOverlay = false;
    int32 CompletedCanals = 0;
};
