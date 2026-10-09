#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DownstreamExtractionPoint.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

// Presentation marker only. Extraction is an explicit E request through the subsystem.
UCLASS()
class DOWNSTREAM_API ADownstreamExtractionPoint : public AActor
{
    GENERATED_BODY()

public:
    ADownstreamExtractionPoint();
    virtual void OnConstruction(const FTransform& Transform) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Home") TObjectPtr<UStaticMeshComponent> HomeMarker;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Home") TObjectPtr<UPointLightComponent> HomeGlow;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Home") TObjectPtr<UTextRenderComponent> HomeLabel;

protected:
    virtual void BeginPlay() override;

private:
    void RefreshAppearance();
};
