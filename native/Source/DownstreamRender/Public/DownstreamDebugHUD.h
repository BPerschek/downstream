#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DownstreamDebugHUD.generated.h"

class ADownstreamTerrainChunk;

UCLASS()
class DOWNSTREAMRENDER_API ADownstreamDebugHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;

private:
    TWeakObjectPtr<ADownstreamTerrainChunk> ObservedChunk;
    FString LastStatusMessage;
    float StatusMessageExpiresAt = 0.0f;
};
