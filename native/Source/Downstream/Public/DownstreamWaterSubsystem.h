#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DownstreamEarthworks.h"
#include "DownstreamWater.h"
#include "DownstreamWorld.h"
#include <deque>
#include <memory>
#include <vector>
#include "DownstreamWaterSubsystem.generated.h"

class ADownstreamFloodedVault;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDownstreamOutpostStructureDestroyed, int32, Outpost, int32, Structure, FVector, Location);

// Game-thread adapter for the water simulation. It runs either the classic Drowned Vault
// site (M1) or a seeded procedural world (M2_World). The plain C++ WaterGrid owns ground and
// water; Dam Cult outpost state (flood damage, pumps, breaches) lives here too because it
// reads and edits the same grid. Presentation actors read state and submit requests.
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

    // Classic M1 site at Origin. Builds the grid, applies saved terrain and settles the water.
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") bool ActivateSite(FVector Origin);
    // Procedural world for Seed, centred on the world origin. Seed 0 uses the expedition's seed.
    // Preview worlds (menu background) skip saves, outposts and the home override.
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") bool ActivateWorld(int64 Seed, bool bPreview = false);
    // In the world map: generate the world now if nothing has activated it yet.
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") bool EnsureWorldActive();
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool IsSiteActive() const { return Grid != nullptr; }
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool IsWorldSite() const { return bWorldSite; }
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool IsPreviewSite() const { return bPreview; }
    UFUNCTION(BlueprintPure, Category="Downstream|Water") FVector GetSiteOrigin() const { return Origin; }
    UFUNCTION(BlueprintPure, Category="Downstream|Water") float GetCellSizeCm() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") FIntPoint GetGridSize() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool WorldToCell(FVector World, FIntPoint& Cell) const;
    // Centre of a cell at its ground surface.
    UFUNCTION(BlueprintPure, Category="Downstream|Water") FVector CellToWorld(FIntPoint Cell) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") float GetCellGround(FIntPoint Cell) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") float GetCellWater(FIntPoint Cell) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") float GetCellBaseGround(FIntPoint Cell) const;
    // Metres of water at a world location; zero outside the site.
    UFUNCTION(BlueprintPure, Category="Downstream|Water") float GetWaterDepthAt(FVector World) const;
    // Ground height (cm, world Z) at a location; false outside the site.
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool GetGroundZAt(FVector World, float& Z) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") float GetTotalWater() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") float GetDrainedVolume() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") int64 GetWaterTick() const;

    // Vaults. The index-free versions address vault 0 (the classic site's only vault).
    UFUNCTION(BlueprintPure, Category="Downstream|Water") int32 GetVaultCount() const { return static_cast<int32>(Layout.Vaults.size()); }
    UFUNCTION(BlueprintPure, Category="Downstream|Water") FVector GetVaultLocation() const { return GetVaultLocationAt(0); }
    UFUNCTION(BlueprintPure, Category="Downstream|Water") float GetVaultDepth() const { return GetVaultDepthAt(0); }
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool IsVaultAccessible() const { return IsVaultAccessibleAt(0); }
    UFUNCTION(BlueprintPure, Category="Downstream|Water") FVector GetVaultLocationAt(int32 Index) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") float GetVaultDepthAt(int32 Index) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool IsVaultAccessibleAt(int32 Index) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") int32 GetVaultId(int32 Index) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool IsVaultOpenedAt(int32 Index) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") FIntPoint GetVaultRiverCell(int32 Index) const;
    // Index of the closest unopened-or-opened vault within interaction range, or -1.
    UFUNCTION(BlueprintPure, Category="Downstream|Water") int32 GetVaultNear(AActor* Player) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool IsPlayerNearVault(AActor* Player) const { return GetVaultNear(Player) >= 0; }
    // Returns true when the interaction was about a vault (even if refused).
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") bool TryInteractVault(AActor* Player);

    // Dam Cult outposts (world only).
    UFUNCTION(BlueprintPure, Category="Downstream|Outposts") int32 GetOutpostCount() const { return static_cast<int32>(Outposts.size()); }
    UFUNCTION(BlueprintPure, Category="Downstream|Outposts") FVector GetOutpostCenter(int32 Outpost) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Outposts") FIntPoint GetOutpostCenterCell(int32 Outpost) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Outposts") FIntPoint GetOutpostRiverCell(int32 Outpost) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Outposts") int32 GetOutpostId(int32 Outpost) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Outposts") float GetOutpostRadiusCm(int32 Outpost) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Outposts") int32 GetOutpostStructureCount(int32 Outpost) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Outposts") FVector GetOutpostStructureLocation(int32 Outpost, int32 Structure) const;
    // 0 hut, 1 generator, 2 stash.
    UFUNCTION(BlueprintPure, Category="Downstream|Outposts") int32 GetOutpostStructureType(int32 Outpost, int32 Structure) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Outposts") float GetOutpostStructureIntegrity(int32 Outpost, int32 Structure) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Outposts") float GetOutpostStructureMaxIntegrity(int32 Outpost, int32 Structure) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Outposts") bool IsOutpostStructureDestroyed(int32 Outpost, int32 Structure) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Outposts") float GetOutpostStructureWater(int32 Outpost, int32 Structure) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Outposts") bool IsOutpostRazed(int32 Outpost) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Outposts") int32 GetOutpostStandingCount(int32 Outpost) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Outposts") float GetOutpostFloodDepth(int32 Outpost) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Outposts") bool GetOutpostBreach(int32 Outpost, FIntPoint& Cell) const;
    // Index of the outpost whose levee ring contains this location, or -1.
    UFUNCTION(BlueprintPure, Category="Downstream|Outposts") int32 GetOutpostAt(FVector World, float ExtraRadiusCm = 0.0f) const;
    // One engineer dam action on this outpost's worst breach if the engineer is within reach.
    UFUNCTION(BlueprintCallable, Category="Downstream|Outposts") bool PerformDamAction(AActor* Engineer, int32 Outpost);
    UFUNCTION(BlueprintCallable, Category="Downstream|Outposts") bool DamageOutpostStructure(int32 Outpost, int32 Structure, float Amount);
    UPROPERTY(BlueprintAssignable, Category="Downstream|Outposts") FDownstreamOutpostStructureDestroyed OnOutpostStructureDestroyed;

    // Canal planning and digging
    UFUNCTION(BlueprintPure, Category="Downstream|Water") TArray<FIntPoint> PreviewCanal(FIntPoint From, FIntPoint To) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool IsCanalValid(FIntPoint From, FIntPoint To) const;
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") bool DesignateCanal(FIntPoint From, FIntPoint To, AActor* Player);
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") void ClearDigQueue();
    UFUNCTION(BlueprintPure, Category="Downstream|Water") float GetCanalDepth() const;
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") void SetCanalDepth(float Metres);
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") void AdjustCanalDepth(int32 Steps);
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool GetCanalBedRange(FIntPoint From, FIntPoint To, float& StartBed, float& EndBed) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") int32 GetQueuedCanalCount() const { return static_cast<int32>(Jobs.size()); }
    UFUNCTION(BlueprintPure, Category="Downstream|Water") TArray<FIntPoint> GetQueuedCanalCells() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") float GetActiveCanalProgress() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool GetNextDigCell(FIntPoint& Cell) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool GetDigStandCell(FIntPoint& Cell) const;
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") bool PerformDigAction(AActor* Digger);
    UFUNCTION(BlueprintPure, Category="Downstream|Water") int32 GetCompletedCanalCount() const { return CompletedCanals; }

    // Developer view and tests
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") void ToggleFlowOverlay() { bFlowOverlay = !bFlowOverlay; }
    UFUNCTION(BlueprintPure, Category="Downstream|Water") bool IsFlowOverlayEnabled() const { return bFlowOverlay; }
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") void SimulateSeconds(float Seconds);
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") bool SaveTerrainNow();

    const downstream::WaterGrid* GetGrid() const { return Grid.get(); }
    const downstream::WorldLayout& GetLayout() const { return Layout; }

private:
    bool IsCellValid(FIntPoint Cell) const;
    bool FinishActivation(std::unique_ptr<downstream::WaterGrid> NewGrid, double WarmupSeconds);
    void SpawnVaults();
    void TickOutposts(float Seconds);
    void HandleStructureDestroyed(int32 Outpost, int32 Structure);
    void PlayFeedback(bool bSuccess, const FVector& Location) const;
    FVector CellCentre(int32 X, int32 Y) const;

    std::unique_ptr<downstream::WaterGrid> Grid;
    downstream::WorldLayout Layout;
    std::vector<downstream::OutpostState> Outposts;
    std::deque<downstream::DigJob> Jobs;
    FVector Origin = FVector::ZeroVector;
    UPROPERTY(Transient) TArray<TObjectPtr<ADownstreamFloodedVault>> Vaults;
    double NextTerrainSave = 0.0;
    double NextDigAction = 0.0;
    float OutpostAccumulator = 0.0f;
    bool bTerrainDirty = false;
    bool bFlowOverlay = false;
    bool bWorldSite = false;
    bool bPreview = false;
    bool bActivating = false;
    int32 CompletedCanals = 0;
    float CanalDepth = -1.0f; // <0: use tuning default
};
