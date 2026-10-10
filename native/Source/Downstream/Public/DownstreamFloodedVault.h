#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DownstreamFloodedVault.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UTextRenderComponent;

// Presentation for the Drowned Vault reward. State lives in the water and expedition subsystems.
UCLASS()
class DOWNSTREAM_API ADownstreamFloodedVault : public AActor
{
    GENERATED_BODY()
public:
    ADownstreamFloodedVault();
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Vault") TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Vault") TObjectPtr<UStaticMeshComponent> Hatch;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Vault") TObjectPtr<UPointLightComponent> Beacon;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Vault") TObjectPtr<UTextRenderComponent> Label;

protected:
    virtual void BeginPlay() override;
};
