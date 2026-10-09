#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DownstreamEnemyController.generated.h"

UCLASS()
class ADownstreamEnemyController : public AAIController
{
    GENERATED_BODY()
public:
    // Repath requests are throttled; the character can steer locally when no navmesh is available.
    bool MoveForCombat(const FVector& Destination, float AcceptanceRadius);
    void HaltForCombat();
private:
    double NextMoveRequest = 0.0;
    bool bLastRequestFailed = false;
};
