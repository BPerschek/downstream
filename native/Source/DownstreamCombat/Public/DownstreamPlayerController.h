#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DownstreamPlayerController.generated.h"

UCLASS()
class DOWNSTREAMCOMBAT_API ADownstreamPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    ADownstreamPlayerController();
protected:
    virtual void BeginPlay() override;
};
