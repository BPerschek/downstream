#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamSettlementManager.generated.h"

class ADownstreamHauler;
class ADownstreamDigger;
class ADownstreamSettlementBuilding;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInterface;

// One saved manager in M1 creates only transient, owned presentation actors from core records.
UCLASS()
class DOWNSTREAMCOLONY_API ADownstreamSettlementManager : public AActor
{
    GENERATED_BODY()
public:
    ADownstreamSettlementManager();
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION(BlueprintPure, Category="Downstream|Settlement") static ADownstreamSettlementManager* GetForWorld(UObject* ContextObject);
    UFUNCTION(BlueprintCallable, Category="Downstream|Settlement") void SetPlacementPreview(EDownstreamStructureType Type, FVector WorldLocation, bool bVisible);
    UFUNCTION(BlueprintCallable, Category="Downstream|Settlement") void ClearPlacementPreview();
    UFUNCTION(BlueprintPure, Category="Downstream|Settlement") ADownstreamHauler* GetHauler() const { return Hauler; }
    UFUNCTION(BlueprintPure, Category="Downstream|Settlement") FText GetHaulerStatus() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Settlement") ADownstreamSettlementBuilding* GetBuilding(EDownstreamStructureType Type) const;
    UFUNCTION(BlueprintPure, Category="Downstream|Earthworks") ADownstreamDigger* GetDigger() const { return Digger; }
    UFUNCTION(BlueprintPure, Category="Downstream|Earthworks") FText GetDiggerStatus() const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Settlement") TObjectPtr<UStaticMeshComponent> PlacementPreview;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Settlement") TObjectPtr<UTextRenderComponent> PreviewLabel;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION() void SyncSettlement();
    bool BindSubsystem();
    FVector GetHaulerSpawnLocation() const;
    UPROPERTY(Transient) TObjectPtr<UDownstreamExpeditionSubsystem> Expedition;
    UPROPERTY(Transient) TArray<TObjectPtr<ADownstreamSettlementBuilding>> Buildings;
    UPROPERTY(Transient) TObjectPtr<ADownstreamHauler> Hauler;
    UPROPERTY(Transient) TObjectPtr<ADownstreamDigger> Digger;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> ValidPreviewMaterial;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> InvalidPreviewMaterial;
    bool bSyncing = false;
};
