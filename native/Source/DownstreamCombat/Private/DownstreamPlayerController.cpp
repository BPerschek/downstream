#include "DownstreamPlayerController.h"
#include "GameFramework/HUD.h"

ADownstreamPlayerController::ADownstreamPlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = false;
    bEnableMouseOverEvents = false;
    DefaultMouseCursor = EMouseCursor::Crosshairs;
}

void ADownstreamPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) return;
    FInputModeGameOnly InputMode;
    InputMode.SetConsumeCaptureMouseDown(false);
    SetInputMode(InputMode);
    bShowMouseCursor = true;
    // Runtime class lookup installs the presentation adapter without a Combat -> Render dependency.
    if (UClass* ExpeditionHUD = LoadClass<AHUD>(nullptr, TEXT("/Script/DownstreamRender.DownstreamDebugHUD")))
        ClientSetHUD(ExpeditionHUD);
}
