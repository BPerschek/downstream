#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DownstreamDebugHUD.generated.h"

class ADownstreamTerrainChunk;
class SDownstreamMenu;

UCLASS()
class DOWNSTREAMRENDER_API ADownstreamDebugHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
    UFUNCTION(BlueprintCallable, Category="Downstream|UI") void TogglePauseMenu();
    UFUNCTION(BlueprintPure, Category="Downstream|UI") bool IsPauseMenuOpen() const { return PauseMenu.IsValid(); }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void BindPauseKey();
    void ClosePauseMenu();
    TWeakObjectPtr<ADownstreamTerrainChunk> ObservedChunk;
    TSharedPtr<SDownstreamMenu> PauseMenu;
    FString LastStatusMessage;
    float StatusMessageExpiresAt = 0.0f;
    bool bPauseBound = false;
};
