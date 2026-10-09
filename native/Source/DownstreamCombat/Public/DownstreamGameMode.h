#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DownstreamGameMode.generated.h"

UCLASS()
class DOWNSTREAMCOMBAT_API ADownstreamGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ADownstreamGameMode();
    virtual void RestartPlayer(AController* NewPlayer) override;
    void RequestPlayerRespawn(AController* PlayerController, APawn* DeadPawn);

private:
    FTimerHandle RespawnTimer;
};
