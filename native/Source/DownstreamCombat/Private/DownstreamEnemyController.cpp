#include "DownstreamEnemyController.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"

bool ADownstreamEnemyController::MoveForCombat(const FVector& Destination, float AcceptanceRadius)
{
    if (GetWorld()->GetTimeSeconds() >= NextMoveRequest)
    {
        NextMoveRequest = GetWorld()->GetTimeSeconds() + 0.4;
        bLastRequestFailed = MoveToLocation(Destination, AcceptanceRadius, true, true, false, true,
            nullptr, false) == EPathFollowingRequestResult::Failed;
    }
    return !bLastRequestFailed;
}

void ADownstreamEnemyController::HaltForCombat()
{
    StopMovement();
    NextMoveRequest = 0.0;
    bLastRequestFailed = false;
}
