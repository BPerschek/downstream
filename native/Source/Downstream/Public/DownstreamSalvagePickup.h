#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DownstreamSalvagePickup.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class DOWNSTREAM_API ADownstreamSalvagePickup : public AActor
{
    GENERATED_BODY()

public:
    ADownstreamSalvagePickup();
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Downstream|Salvage", meta=(ClampMin="1")) int32 Amount = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Downstream|Salvage") bool bRecoveryBag = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Salvage") TObjectPtr<UStaticMeshComponent> SalvageMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Salvage") TObjectPtr<UPointLightComponent> SalvageGlow;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Salvage") TObjectPtr<UTextRenderComponent> SalvageLabel;

    bool IsClaimed() const { return bClaimed; }
    bool TryClaim();
    void ReleaseClaim();
    void RefreshAppearance();

protected:
    virtual void BeginPlay() override;

private:
    bool bClaimed = false;
    float BobSeconds = 0.0f;
    FVector RestMeshLocation = FVector(0.0f, 0.0f, 18.0f);
};
