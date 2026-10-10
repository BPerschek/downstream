#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DownstreamDigger.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;

// Terrain robot. Walks to the next unfinished cell of the queued canal and asks
// the water subsystem for one dig step at a time. It owns no terrain state.
UCLASS()
class DOWNSTREAMCOLONY_API ADownstreamDigger : public ACharacter
{
    GENERATED_BODY()
public:
    ADownstreamDigger();
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION(BlueprintPure, Category="Downstream|Digger") FText GetStatus() const { return Status; }
    UFUNCTION(BlueprintPure, Category="Downstream|Digger") int32 GetDigActions() const { return DigActions; }
    UFUNCTION(BlueprintPure, Category="Downstream|Digger") bool IsUsingDirectSteering() const { return bDirectSteering; }
    UFUNCTION(BlueprintPure, Category="Downstream|Digger") int32 GetHopCount() const { return Hops; }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Digger") TObjectPtr<UStaticMeshComponent> BotMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Digger") TObjectPtr<UStaticMeshComponent> Scoop;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Digger") TObjectPtr<UTextRenderComponent> WorldLabel;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Digger") TObjectPtr<UPointLightComponent> WorkLight;

protected:
    virtual void BeginPlay() override;

private:
    void Steer(const FVector& Destination, float Acceptance, double Now);
    void StopMoving();
    bool GetHomeLocation(FVector& Out) const;
    void SetStatus(const FString& NewStatus);

    FText Status;
    FVector MoveGoal = FVector::ZeroVector;
    FVector ProgressLocation = FVector::ZeroVector;
    double LastProgressTime = 0.0;
    double DirectUntil = 0.0;
    double NextMoveRequest = 0.0;
    int32 DigActions = 0;
    int32 Hops = 0;
    float ScoopPhase = 0.0f;
    bool bHasGoal = false;
    bool bDirectSteering = false;
    bool bDigging = false;
};
