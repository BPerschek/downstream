#include "DownstreamMenu.h"

#include "DownstreamSessionSubsystem.h"
#include "Brushes/SlateColorBrush.h"
#include "Camera/CameraActor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Fonts/CompositeFont.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DownstreamMenu"

namespace
{
const FLinearColor Ink(0.94f, 0.93f, 0.88f);
const FLinearColor Muted(0.66f, 0.72f, 0.69f);
const FLinearColor Accent(0.98f, 0.66f, 0.24f);
const FLinearColor Teal(0.30f, 0.86f, 0.74f);

const FButtonStyle& ButtonStyle(bool bPrimary)
{
    static FButtonStyle Normal, Primary;
    static bool bInit = false;
    if (!bInit)
    {
        bInit = true;
        Normal = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
        Normal.SetNormal(FSlateColorBrush(FLinearColor(0.06f, 0.08f, 0.07f, 0.92f)));
        Normal.SetHovered(FSlateColorBrush(FLinearColor(0.16f, 0.22f, 0.19f, 0.96f)));
        Normal.SetPressed(FSlateColorBrush(FLinearColor(0.30f, 0.20f, 0.08f, 1.0f)));
        Normal.SetDisabled(FSlateColorBrush(FLinearColor(0.05f, 0.05f, 0.05f, 0.6f)));
        Normal.SetNormalPadding(FMargin(18, 10)).SetPressedPadding(FMargin(18, 11, 18, 9));
        Primary = Normal;
        Primary.SetNormal(FSlateColorBrush(FLinearColor(0.42f, 0.22f, 0.05f, 0.95f)));
        Primary.SetHovered(FSlateColorBrush(FLinearColor(0.62f, 0.33f, 0.07f, 1.0f)));
    }
    return bPrimary ? Primary : Normal;
}

// Choice buttons: grey brushes tinted per state (selected = amber) via ButtonColorAndOpacity.
const FButtonStyle& TintStyle()
{
    static FButtonStyle Style;
    static bool bInit = false;
    if (!bInit)
    {
        bInit = true;
        Style = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
        Style.SetNormal(FSlateColorBrush(FLinearColor(0.7f, 0.7f, 0.7f)));
        Style.SetHovered(FSlateColorBrush(FLinearColor(1.0f, 1.0f, 1.0f)));
        Style.SetPressed(FSlateColorBrush(FLinearColor(0.5f, 0.5f, 0.5f)));
        Style.SetNormalPadding(FMargin(12, 7)).SetPressedPadding(FMargin(12, 8, 12, 6));
    }
    return Style;
}

FText ControlsText()
{
    return LOCTEXT("Controls",
        "WASD  move            Mouse  aim            LMB  shoot            RMB  melee\n"
        "Space  dodge (in a trench: scramble up the bank)            E  collect / open / bank at home\n"
        "1 / 2  starter skill            B  builder at home: 1-3 buildings, 4 hauler, 5 digger\n"
        "J  hauler on/off            G  dig planner: drag LMB from water to where it should drain\n"
        "Shift + wheel  canal depth            C  clear canal queue            F9  water flow overlay\n"
        "Esc  pause menu\n\n"
        "Developer:  F10 respawn field enemies and caches     F11 +25 banked salvage\n\n"
        "Dam Cult outposts sit below the rivers behind earth levees. Cut a canal through a levee and the\n"
        "river pours in: flooded buildings fall apart and spill salvage. Engineers sandbag breaches and the\n"
        "garrison hunts your digger, so cover it. Kill the generator and the outpost stops reinforcing.");
}
}

FSlateFontInfo DownstreamUI::TitleFont(int32 Size)
{
    static TSharedPtr<const FCompositeFont> Future;
    static bool bTried = false;
    if (!bTried)
    {
        bTried = true;
        const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Downstream/UI/Fonts/RussoOne-Regular.ttf"));
        if (IFileManager::Get().FileExists(*Path))
            Future = MakeShared<FStandaloneCompositeFont>(NAME_None, Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
    }
    return Future.IsValid() ? FSlateFontInfo(Future, Size) : FCoreStyle::GetDefaultFontStyle("Bold", Size);
}

FSlateFontInfo DownstreamUI::BodyFont(int32 Size, bool bBold)
{
    return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
}

UDownstreamSessionSubsystem* SDownstreamMenu::Session() const
{
    return World.IsValid() ? UDownstreamSessionSubsystem::Get(World.Get()) : nullptr;
}

void SDownstreamMenu::Construct(const FArguments& InArgs)
{
    bPauseMenu = InArgs._bPauseMenu;
    World = InArgs._World;
    OnResume = InArgs._OnResume;
    static const FSlateColorBrush Shade(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f));
    static const FSlateColorBrush PanelBrush(FLinearColor(0.02f, 0.03f, 0.025f, 0.88f));
    ChildSlot
    [
        SNew(SBorder).BorderImage(bPauseMenu ? &Shade : FCoreStyle::Get().GetBrush("NoBorder")).Padding(0)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(64, 56, 0, 56)
            [
                SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(36, 30))
                [
                    SNew(SBox).WidthOverride(560)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [ SNew(STextBlock).Text(LOCTEXT("Title", "DOWNSTREAM")).Font(DownstreamUI::TitleFont(52)).ColorAndOpacity(Accent) ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 22)
                        [ SNew(STextBlock).Text(bPauseMenu ? LOCTEXT("Paused", "Paused") : LOCTEXT("Tagline", "Salvage. Build. Move the water.")).Font(DownstreamUI::BodyFont(16)).ColorAndOpacity(Muted) ]
                        + SVerticalBox::Slot().FillHeight(1.0f)
                        [
                            SAssignNew(Switcher, SWidgetSwitcher)
                            + SWidgetSwitcher::Slot()[ MakeMainPage() ]
                            + SWidgetSwitcher::Slot()[ MakeNewWorldPage() ]
                            + SWidgetSwitcher::Slot()[ MakeSettingsPage() ]
                            + SWidgetSwitcher::Slot()[ MakeControlsPage() ]
                        ]
                    ]
                ]
            ]
            + SHorizontalBox::Slot().FillWidth(1.0f)[ SNew(SSpacer) ]
        ]
    ];
}

FReply SDownstreamMenu::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
    if (KeyEvent.GetKey() == EKeys::Escape)
    {
        if (Switcher.IsValid() && Switcher->GetActiveWidgetIndex() != 0) return ShowPage(0);
        if (bPauseMenu) { OnResume.ExecuteIfBound(); return FReply::Handled(); }
    }
    return FReply::Unhandled();
}

FReply SDownstreamMenu::ShowPage(int32 Page)
{
    if (Switcher.IsValid()) Switcher->SetActiveWidgetIndex(Page);
    return FReply::Handled();
}

TSharedRef<SWidget> SDownstreamMenu::MakeButton(const FText& Label, FOnClicked OnClicked, TAttribute<bool> Enabled, bool bPrimary)
{
    return SNew(SBox).Padding(FMargin(0, 5))
    [
        SNew(SButton).ButtonStyle(&ButtonStyle(bPrimary)).OnClicked(OnClicked).IsEnabled(Enabled)
        [ SNew(STextBlock).Text(Label).Font(DownstreamUI::BodyFont(19, true)).ColorAndOpacity(Ink) ]
    ];
}

TSharedRef<SWidget> SDownstreamMenu::MakeMainPage()
{
    TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
    if (bPauseMenu)
    {
        Box->AddSlot().AutoHeight()[ MakeButton(LOCTEXT("Resume", "Resume"), FOnClicked::CreateLambda([this]() { OnResume.ExecuteIfBound(); return FReply::Handled(); }), true, true) ];
        Box->AddSlot().AutoHeight()[ MakeButton(LOCTEXT("Settings", "Settings"), FOnClicked::CreateSP(this, &SDownstreamMenu::ShowPage, 2)) ];
        Box->AddSlot().AutoHeight()[ MakeButton(LOCTEXT("ControlsBtn", "Controls"), FOnClicked::CreateSP(this, &SDownstreamMenu::ShowPage, 3)) ];
        Box->AddSlot().AutoHeight()[ MakeButton(LOCTEXT("ToMenu", "Main menu (progress is saved)"), FOnClicked::CreateLambda([this]()
        {
            if (World.IsValid()) UGameplayStatics::SetGamePaused(World.Get(), false);
            if (auto* S = Session()) S->ReturnToMenu();
            return FReply::Handled();
        })) ];
        Box->AddSlot().AutoHeight()[ MakeButton(LOCTEXT("Quit", "Quit to desktop"), FOnClicked::CreateLambda([this]() { if (auto* S = Session()) S->QuitGame(); return FReply::Handled(); })) ];
        return Box;
    }
    Box->AddSlot().AutoHeight()[ MakeButton(
        TAttribute<FText>::CreateLambda([this]() {
            const auto* S = Session();
            return S && S->HasWorldSave() ? FText::Format(LOCTEXT("ContinueSeed", "Continue frontier  #{0}"), FText::AsNumber(S->GetWorldSaveSeed(), &FNumberFormattingOptions::DefaultNoGrouping()))
                                          : LOCTEXT("ContinueNone", "Continue  (no frontier yet)"); }).Get(),
        FOnClicked::CreateLambda([this]() { if (auto* S = Session()) S->ContinueWorld(); return FReply::Handled(); }),
        TAttribute<bool>::CreateLambda([this]() { const auto* S = Session(); return S && S->HasWorldSave(); }), true) ];
    Box->AddSlot().AutoHeight()[ MakeButton(LOCTEXT("NewWorld", "New frontier"), FOnClicked::CreateSP(this, &SDownstreamMenu::ShowPage, 1)) ];
    Box->AddSlot().AutoHeight()[ MakeButton(LOCTEXT("Classic", "Training grounds  (the first map)"), FOnClicked::CreateLambda([this]() { if (auto* S = Session()) S->StartClassic(); return FReply::Handled(); })) ];
    Box->AddSlot().AutoHeight()[ MakeButton(LOCTEXT("Settings2", "Settings"), FOnClicked::CreateSP(this, &SDownstreamMenu::ShowPage, 2)) ];
    Box->AddSlot().AutoHeight()[ MakeButton(LOCTEXT("Controls2", "Controls & how to play"), FOnClicked::CreateSP(this, &SDownstreamMenu::ShowPage, 3)) ];
    Box->AddSlot().AutoHeight()[ MakeButton(LOCTEXT("Quit2", "Quit"), FOnClicked::CreateLambda([this]() { if (auto* S = Session()) S->QuitGame(); return FReply::Handled(); })) ];
    return Box;
}

TSharedRef<SWidget> SDownstreamMenu::MakeNewWorldPage()
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [ SNew(STextBlock).Text(LOCTEXT("NewHeader", "New frontier")).Font(DownstreamUI::BodyFont(24, true)).ColorAndOpacity(Ink) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 14)
        [ SNew(STextBlock).AutoWrapText(true).Font(DownstreamUI::BodyFont(14)).ColorAndOpacity(Muted)
            .Text(LOCTEXT("NewBody", "Every seed is a different 112 x 88 m frontier: rivers, a drowned vault and up to three Dam Cult outposts. Starting a new one replaces your current frontier save (the training grounds save is separate).")) ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)
            [ SNew(STextBlock).Text(LOCTEXT("Seed", "Seed")).Font(DownstreamUI::BodyFont(18, true)).ColorAndOpacity(Ink) ]
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [ SAssignNew(SeedBox, SEditableTextBox).Font(DownstreamUI::BodyFont(18)).Text(FText::AsNumber(UDownstreamSessionSubsystem::MakeRandomSeed(), &FNumberFormattingOptions::DefaultNoGrouping())) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(10, 0, 0, 0)
            [ MakeButton(LOCTEXT("Random", "Random"), FOnClicked::CreateLambda([this]()
                {
                    if (SeedBox.IsValid()) SeedBox->SetText(FText::AsNumber(UDownstreamSessionSubsystem::MakeRandomSeed(), &FNumberFormattingOptions::DefaultNoGrouping()));
                    return FReply::Handled();
                })) ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 14, 0, 0)
        [ MakeButton(LOCTEXT("Start", "Start"), FOnClicked::CreateLambda([this]()
            {
                const int64 Seed = SeedBox.IsValid() ? FCString::Atoi64(*SeedBox->GetText().ToString()) : 0;
                if (auto* S = Session()) S->StartNewWorld(Seed > 0 ? Seed : UDownstreamSessionSubsystem::MakeRandomSeed());
                return FReply::Handled();
            }), true, true) ]
        + SVerticalBox::Slot().AutoHeight()[ MakeButton(LOCTEXT("Back", "Back"), FOnClicked::CreateSP(this, &SDownstreamMenu::ShowPage, 0)) ];
}

TSharedRef<SWidget> SDownstreamMenu::MakeChoiceRow(const FText& Label, const TArray<FText>& Options, TFunction<int32()> Current, TFunction<void(int32)> Choose)
{
    TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
        [ SNew(SBox).WidthOverride(130)[ SNew(STextBlock).Text(Label).Font(DownstreamUI::BodyFont(15, true)).ColorAndOpacity(Ink) ] ];
    for (int32 Index = 0; Index < Options.Num(); ++Index)
    {
        Row->AddSlot().AutoWidth().Padding(2, 0)
        [
            SNew(SButton).ButtonStyle(&TintStyle())
            .ButtonColorAndOpacity_Lambda([Current, Index]() { return Current() == Index ? FLinearColor(0.85f, 0.45f, 0.1f) : FLinearColor(0.13f, 0.16f, 0.14f); })
            .OnClicked_Lambda([Choose, Index]() { Choose(Index); return FReply::Handled(); })
            [ SNew(STextBlock).Text(Options[Index]).Font(DownstreamUI::BodyFont(13, true)).ColorAndOpacity(Ink) ]
        ];
    }
    return SNew(SBox).Padding(FMargin(0, 5))[ Row ];
}

TSharedRef<SWidget> SDownstreamMenu::MakeSettingsPage()
{
    static const TArray<FIntPoint> Resolutions = {FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1440)};
    auto Settings = []() { return GEngine ? GEngine->GetGameUserSettings() : nullptr; };
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
        [ SNew(STextBlock).Text(LOCTEXT("SettingsHeader", "Settings")).Font(DownstreamUI::BodyFont(24, true)).ColorAndOpacity(Ink) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 5)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
            [ SNew(SBox).WidthOverride(130)[ SNew(STextBlock).Text(LOCTEXT("Volume", "Volume")).Font(DownstreamUI::BodyFont(15, true)).ColorAndOpacity(Ink) ] ]
            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
            [ SNew(SSlider).Value_Lambda([this]() { const auto* S = Session(); return S ? S->GetMasterVolume() : 0.8f; })
                .OnValueChanged_Lambda([this](float Value) { if (auto* S = Session()) S->SetMasterVolume(Value); }) ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10, 0, 0, 0)
            [ SNew(STextBlock).Font(DownstreamUI::BodyFont(14)).ColorAndOpacity(Muted)
                .Text_Lambda([this]() { const auto* S = Session(); return FText::AsPercent(S ? S->GetMasterVolume() : 0.8f); }) ]
        ]
        + SVerticalBox::Slot().AutoHeight()
        [ MakeChoiceRow(LOCTEXT("Window", "Window"), {LOCTEXT("Windowed", "Windowed"), LOCTEXT("Borderless", "Borderless"), LOCTEXT("Fullscreen", "Fullscreen")},
            [Settings]() { const auto* S = Settings(); if (!S) return 0; const auto Mode = S->GetFullscreenMode(); return Mode == EWindowMode::Windowed ? 0 : Mode == EWindowMode::WindowedFullscreen ? 1 : 2; },
            [Settings](int32 Index) { if (auto* S = Settings()) { S->SetFullscreenMode(Index == 0 ? EWindowMode::Windowed : Index == 1 ? EWindowMode::WindowedFullscreen : EWindowMode::Fullscreen); S->ApplySettings(false); S->SaveSettings(); } }) ]
        + SVerticalBox::Slot().AutoHeight()
        [ MakeChoiceRow(LOCTEXT("Resolution", "Resolution"), {FText::FromString(TEXT("1280x720")), FText::FromString(TEXT("1600x900")), FText::FromString(TEXT("1920x1080")), FText::FromString(TEXT("2560x1440"))},
            [Settings]() { const auto* S = Settings(); return S ? Resolutions.IndexOfByKey(S->GetScreenResolution()) : 0; },
            [Settings](int32 Index) { if (auto* S = Settings()) { S->SetScreenResolution(Resolutions[Index]); S->ApplySettings(false); S->SaveSettings(); } }) ]
        + SVerticalBox::Slot().AutoHeight()
        [ MakeChoiceRow(LOCTEXT("Quality", "Quality"), {LOCTEXT("Low", "Low"), LOCTEXT("Medium", "Medium"), LOCTEXT("High", "High"), LOCTEXT("Epic", "Epic")},
            [Settings]() { const auto* S = Settings(); return S ? S->GetOverallScalabilityLevel() : 2; },
            [Settings](int32 Index) { if (auto* S = Settings()) { S->SetOverallScalabilityLevel(Index); S->ApplySettings(false); S->SaveSettings(); } }) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 8)
        [ SNew(SCheckBox).IsChecked_Lambda([this]() { const auto* S = Session(); return S && S->GetShowFPS() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
            .OnCheckStateChanged_Lambda([this](ECheckBoxState State) { if (auto* S = Session()) S->SetShowFPS(State == ECheckBoxState::Checked, World.Get()); })
            [ SNew(STextBlock).Text(LOCTEXT("ShowFPS", "  Show FPS counter")).Font(DownstreamUI::BodyFont(15)).ColorAndOpacity(Ink) ] ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 4)
        [ SNew(SCheckBox).IsChecked_Lambda([this]() { const auto* S = Session(); return !S || S->GetShowHints() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
            .OnCheckStateChanged_Lambda([this](ECheckBoxState State) { if (auto* S = Session()) S->SetShowHints(State == ECheckBoxState::Checked); })
            [ SNew(STextBlock).Text(LOCTEXT("ShowHints", "  Show control hints on the HUD")).Font(DownstreamUI::BodyFont(15)).ColorAndOpacity(Ink) ] ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
        [ MakeButton(LOCTEXT("SaveBack", "Save & back"), FOnClicked::CreateLambda([this]()
            {
                if (auto* S = Session()) S->SaveOptions();
                return ShowPage(0);
            }), true, true) ];
}

TSharedRef<SWidget> SDownstreamMenu::MakeControlsPage()
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
        [ SNew(STextBlock).Text(LOCTEXT("ControlsHeader", "Controls & how to play")).Font(DownstreamUI::BodyFont(24, true)).ColorAndOpacity(Ink) ]
        + SVerticalBox::Slot().AutoHeight()
        [ SNew(STextBlock).Text(ControlsText()).Font(DownstreamUI::BodyFont(13)).ColorAndOpacity(Teal).AutoWrapText(true) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 14, 0, 0)
        [ MakeButton(LOCTEXT("Back2", "Back"), FOnClicked::CreateSP(this, &SDownstreamMenu::ShowPage, 0)) ];
}

// ---- Main menu controller / game mode --------------------------------------------

ADownstreamMenuController::ADownstreamMenuController()
{
    PrimaryActorTick.bCanEverTick = true;
    bShowMouseCursor = true;
}

void ADownstreamMenuController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController() || !GEngine || !GEngine->GameViewport) return;
    if (auto* S = UDownstreamSessionSubsystem::Get(this)) S->ApplyRuntimeOptions(this);
    OrbitCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FVector(0, -6500, 5200), FRotator(-38, 90, 0));
    if (OrbitCamera) SetViewTarget(OrbitCamera);
    SAssignNew(Menu, SDownstreamMenu).bPauseMenu(false).World(GetWorld());
    GEngine->GameViewport->AddViewportWidgetContent(Menu.ToSharedRef(), 10);
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(Menu);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
}

void ADownstreamMenuController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (Menu.IsValid() && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
    Menu.Reset();
    Super::EndPlay(EndPlayReason);
}

void ADownstreamMenuController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!OrbitCamera) return;
    OrbitYaw += DeltaSeconds * 3.0f;
    const float Radius = 6200.0f;
    const FVector Location(FMath::Cos(FMath::DegreesToRadians(OrbitYaw)) * Radius, FMath::Sin(FMath::DegreesToRadians(OrbitYaw)) * Radius, 4600.0f);
    OrbitCamera->SetActorLocationAndRotation(Location, (FVector(0, 0, 150) - Location).Rotation());
}

ADownstreamMenuGameMode::ADownstreamMenuGameMode()
{
    DefaultPawnClass = nullptr;
    PlayerControllerClass = ADownstreamMenuController::StaticClass();
}

#undef LOCTEXT_NAMESPACE
