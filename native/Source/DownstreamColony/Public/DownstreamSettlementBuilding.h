#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DownstreamExpeditionSubsystem.h"
#include "DownstreamSettlementBuilding.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

// Presentation and a dynamic navigation obstacle. The subsystem owns the saved cell/type.
UCLASS()
class DOWNSTREAMCOLONY_API ADownstreamSettlementBuilding : public AActor
{
    GENERATED_BODY()
public:
    ADownstreamSettlementBuilding();
    virtual void OnConstruction(const FTransform& Transform) override;
    void InitializeStructure(EDownstreamStructureType Type);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Settlement") EDownstreamStructureType StructureType = EDownstreamStructureType::Storage;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Settlement") TObjectPtr<UBoxComponent> BuildingCollision;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Settlement") TObjectPtr<UStaticMeshComponent> BuildingMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Settlement") TObjectPtr<UStaticMeshComponent> FoundationMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Settlement") TObjectPtr<UTextRenderComponent> WorldLabel;

protected:
    virtual void BeginPlay() override;

private:
    void RefreshAppearance();
};
