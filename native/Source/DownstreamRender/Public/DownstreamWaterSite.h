#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DownstreamWaterSite.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
namespace downstream { class WaterGrid; }

// Presentation for the Drowned Vault. Terrain and water heights come from the
// water subsystem's grid; this actor never edits them. Terrain has collision
// and affects navigation so pawns walk the plateau, ditches and dug canals.
UCLASS()
class DOWNSTREAMRENDER_API ADownstreamWaterSite : public AActor
{
    GENERATED_BODY()
public:
    ADownstreamWaterSite();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Water") TObjectPtr<UProceduralMeshComponent> TerrainMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Water") TObjectPtr<UProceduralMeshComponent> WaterMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Water") TObjectPtr<UInstancedStaticMeshComponent> PlanMarkers;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Water") TObjectPtr<UInstancedStaticMeshComponent> QueueMarkers;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Water") TObjectPtr<UStaticMeshComponent> SpringPipe;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Water") TObjectPtr<UStaticMeshComponent> CulvertPipe;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Water") TObjectPtr<UTextRenderComponent> CulvertLabel;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Water") TObjectPtr<UTextRenderComponent> SpringLabel;
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
    void BuildTerrain(const downstream::WaterGrid& Grid, bool bCollisionAndNavigation);
    void BuildWater(const downstream::WaterGrid& Grid);
    void UpdateQueueMarkers();
    void UpdatePlanPreview();
    void DrawFlowOverlay(const downstream::WaterGrid& Grid) const;
    void PlaceDecor(const downstream::WaterGrid& Grid);
    void BindInput();
    void ToggleOverlay();

    uint64 LastGroundRevision = MAX_uint64;
    uint64 LastWaterTick = MAX_uint64;
    int32 TerrainRebuilds = 0;
    int32 WaterRebuilds = 0;
    bool bInputBound = false;
    bool bActive = false;
    FString LastQueueSignature;
    FString LastPlanSignature;
};
