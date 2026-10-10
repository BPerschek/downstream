#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SCompoundWidget.h"
#include "DownstreamMenu.generated.h"

class SWidgetSwitcher;
class SEditableTextBox;
class STextBlock;
class ACameraActor;

// Shared main/pause menu built from plain Slate (no Blueprint assets).
class DOWNSTREAMRENDER_API SDownstreamMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SDownstreamMenu) : _bPauseMenu(false) {}
        SLATE_ARGUMENT(bool, bPauseMenu)
        SLATE_ARGUMENT(TWeakObjectPtr<UWorld>, World)
        SLATE_EVENT(FSimpleDelegate, OnResume)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
    virtual bool SupportsKeyboardFocus() const override { return true; }

private:
    TSharedRef<SWidget> MakeMainPage();
    TSharedRef<SWidget> MakeNewWorldPage();
    TSharedRef<SWidget> MakeSettingsPage();
    TSharedRef<SWidget> MakeControlsPage();
    TSharedRef<SWidget> MakeButton(const FText& Label, FOnClicked OnClicked, TAttribute<bool> Enabled = true, bool bPrimary = false);
    TSharedRef<SWidget> MakeChoiceRow(const FText& Label, const TArray<FText>& Options, TFunction<int32()> Current, TFunction<void(int32)> Choose);
    FReply ShowPage(int32 Page);
    class UDownstreamSessionSubsystem* Session() const;

    bool bPauseMenu = false;
    TWeakObjectPtr<UWorld> World;
    FSimpleDelegate OnResume;
    TSharedPtr<SWidgetSwitcher> Switcher;
    TSharedPtr<SEditableTextBox> SeedBox;
};

UCLASS()
class DOWNSTREAMRENDER_API ADownstreamMenuController : public APlayerController
{
    GENERATED_BODY()
public:
    ADownstreamMenuController();
    virtual void Tick(float DeltaSeconds) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    TSharedPtr<SDownstreamMenu> Menu;
    UPROPERTY(Transient) TObjectPtr<ACameraActor> OrbitCamera;
    float OrbitYaw = 30.0f;
};

// Main menu map: no pawn, a slowly orbiting camera over a live preview world.
UCLASS()
class DOWNSTREAMRENDER_API ADownstreamMenuGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ADownstreamMenuGameMode();
};

namespace DownstreamUI
{
    // Russo One (SIL OFL 1.1) when present, otherwise the engine's Roboto.
    DOWNSTREAMRENDER_API FSlateFontInfo TitleFont(int32 Size);
    DOWNSTREAMRENDER_API FSlateFontInfo BodyFont(int32 Size, bool bBold = false);
}
