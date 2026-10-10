#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DownstreamWaterSite.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
class UDownstreamTerrainMeshComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UBoxComponent;
class UAudioComponent;
namespace downstream { class WaterGrid; }

UENUM(BlueprintType)
enum class EDownstreamSiteKind : uint8
{
    // The M1 Drowned Vault site, placed where this actor stands.
    Classic,
    // The seeded M2 world for the current save; repositions itself to the world origin.
    World,
    // A seeded world without saves or gameplay (main menu background).
    Preview
};

// Presentation for the active water site (classic Drowned Vault or a seeded world).
// Terrain and water heights come from the water subsystem's grid; this actor never edits
// them. Terrain is split into chunks so a dig rebuilds mesh, collision and navigation only
// where the ground changed. Chunk (0,0) uses the root TerrainMesh/WaterMesh components.
UCLASS()
class DOWNSTREAMRENDER_API ADownstreamWaterSite : public AActor
{
    GENERATED_BODY()
public:
    ADownstreamWaterSite();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Water") TObjectPtr<UDownstreamTerrainMeshComponent> TerrainMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Water") TObjectPtr<UProceduralMeshComponent> WaterMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Water") TObjectPtr<UInstancedStaticMeshComponent> PlanMarkers;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Water") TObjectPtr<UInstancedStaticMeshComponent> QueueMarkers;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Water") TObjectPtr<UStaticMeshComponent> SpringPipe;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Water") TObjectPtr<UStaticMeshComponent> CulvertPipe;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Water") TObjectPtr<UTextRenderComponent> CulvertLabel;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Water") TObjectPtr<UTextRenderComponent> SpringLabel;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Water") TObjectPtr<UStaticMeshComponent> WaterTower;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Water") TObjectPtr<UStaticMeshComponent> CulvertSign;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") int32 GetNavigationTriangleCount() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Water") int32 GetChunkCount() const { return TerrainChunks.Num(); }
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Downstream|Water") EDownstreamSiteKind SiteKind = EDownstreamSiteKind::Classic;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Downstream|Water") int64 PreviewSeed = 7;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Downstream|Water") TObjectPtr<UMaterialInterface> TerrainMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Downstream|Water") TObjectPtr<UMaterialInterface> WaterMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Downstream|Water") TObjectPtr<UMaterialInterface> ValidMarkerMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Downstream|Water") TObjectPtr<UMaterialInterface> InvalidMarkerMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Downstream|Water") TObjectPtr<UMaterialInterface> QueueMarkerMaterial;

    UFUNCTION(BlueprintPure, Category="Downstream|Water") int32 GetTerrainRebuildCount() const { return TerrainRebuilds; }
    UFUNCTION(BlueprintPure, Category="Downstream|Water") int32 GetWaterRebuildCount() const { return WaterRebuilds; }
    // Planned canal shown while the player drags in dig-planner mode.
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") void ShowCanalPlan(const TArray<FIntPoint>& Cells, bool bValid);
    UFUNCTION(BlueprintCallable, Category="Downstream|Water") void ClearCanalPlan();

protected:
    virtual void BeginPlay() override;

private:
    void SetupChunks(const downstream::WaterGrid& Grid);
    void BuildAllTerrain(const downstream::WaterGrid& Grid, bool bCollisionAndNavigation);
    void BuildTerrainChunk(const downstream::WaterGrid& Grid, int32 ChunkX, int32 ChunkY, bool bCollisionAndNavigation);
    void BuildWaterChunk(const downstream::WaterGrid& Grid, int32 ChunkX, int32 ChunkY, bool bForce);
    void UpdateWater(const downstream::WaterGrid& Grid, bool bForce);
    float BaseGroundAt(int32 X, int32 Y) const;
    void SpawnBoundaries(const downstream::WaterGrid& Grid);
    void UpdateQueueMarkers();
    void UpdatePlanPreview();
    void DrawFlowOverlay(const downstream::WaterGrid& Grid) const;
    void PlaceDecor(const downstream::WaterGrid& Grid);
    void BindInput();
    void ToggleOverlay();
    void UpdateWaterAudio(const downstream::WaterGrid& Grid, float DeltaSeconds);

    UPROPERTY(Transient) TArray<TObjectPtr<UDownstreamTerrainMeshComponent>> TerrainChunks;
    UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> WaterChunks;
    UPROPERTY(Transient) TArray<TObjectPtr<UBoxComponent>> Boundaries;
    // Running-water loop placed at the loudest flow near the player.
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> WaterAudio;
    float WaterAudioTimer = 0.0f;
    float WaterAudioVolume = 0.0f;
    TArray<uint8> WaterChunkWasWet;
    int32 ChunkCells = 16;
    int32 ChunkColumns = 1;
    int32 ChunkRows = 1;
    uint64 LastGroundRevision = MAX_uint64;
    uint64 LastWaterTick = MAX_uint64;
    int32 TerrainRebuilds = 0;
    int32 WaterRebuilds = 0;
    bool bInputBound = false;
    bool bActive = false;
    bool bNavigationRebuildPending = false;
    FString LastQueueSignature;
    FString LastPlanSignature;
};
