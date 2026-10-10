#include "DownstreamHaulerController.h"

#include "DownstreamHauler.h"
#include "Navigation/PathFollowingComponent.h"

void ADownstreamHaulerController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
    Super::OnMoveCompleted(RequestID, Result);
    if (auto* Hauler = Cast<ADownstreamHauler>(GetPawn()))
        Hauler->OnNavigationFinished(Result.IsSuccess(), Result.Code == EPathFollowingResult::Aborted);
}
