#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DownstreamHaulerController.generated.h"

UCLASS()
class ADownstreamHaulerController : public AAIController
{
    GENERATED_BODY()
public:
    virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;
};
