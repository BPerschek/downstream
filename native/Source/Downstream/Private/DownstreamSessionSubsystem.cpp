#include "DownstreamSessionSubsystem.h"

#include "AudioDevice.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

const TCHAR* UDownstreamSessionSubsystem::MainMenuMap = TEXT("/Game/Downstream/Maps/MainMenu");
const TCHAR* UDownstreamSessionSubsystem::WorldMap = TEXT("/Game/Downstream/Maps/M2_World");
const TCHAR* UDownstreamSessionSubsystem::ClassicMap = TEXT("/Game/Downstream/Maps/M1_Expedition");

namespace
{
const TCHAR* OptionsSection = TEXT("Downstream.Options");

FString SaveDirectory()
{
    FString OverrideDirectory;
    if (FParse::Value(FCommandLine::Get(), TEXT("DownstreamSaveDir="), OverrideDirectory)
        && !OverrideDirectory.IsEmpty() && !FPaths::IsRelative(OverrideDirectory))
        return OverrideDirectory;
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Downstream"));
}
}

void UDownstreamSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (GConfig)
    {
        GConfig->GetFloat(OptionsSection, TEXT("MasterVolume"), MasterVolume, GGameUserSettingsIni);
        GConfig->GetBool(OptionsSection, TEXT("ShowHints"), bShowHints, GGameUserSettingsIni);
        GConfig->GetBool(OptionsSection, TEXT("ShowFPS"), bShowFPS, GGameUserSettingsIni);
    }
    MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
}

UDownstreamSessionSubsystem* UDownstreamSessionSubsystem::Get(const UObject* ContextObject)
{
    const UWorld* World = ContextObject ? ContextObject->GetWorld() : nullptr;
    UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
    return Instance ? Instance->GetSubsystem<UDownstreamSessionSubsystem>() : nullptr;
}

FString UDownstreamSessionSubsystem::WorldSavePath() { return FPaths::Combine(SaveDirectory(), TEXT("world-save.json")); }
FString UDownstreamSessionSubsystem::ClassicSavePath() { return FPaths::Combine(SaveDirectory(), TEXT("prototype-save.json")); }

int64 UDownstreamSessionSubsystem::MakeRandomSeed()
{
    return static_cast<int64>(FMath::RandRange(1, 999999));
}

void UDownstreamSessionSubsystem::Travel(const TCHAR* Map)
{
    if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
        UGameplayStatics::OpenLevel(World, FName(Map));
}

void UDownstreamSessionSubsystem::StartNewWorld(int64 Seed)
{
    PendingMode = EDownstreamStartMode::NewWorld;
    PendingSeed = static_cast<uint32>(FMath::Clamp<int64>(Seed, 1, MAX_int32));
    Travel(WorldMap);
}

void UDownstreamSessionSubsystem::ContinueWorld()
{
    PendingMode = EDownstreamStartMode::ContinueWorld;
    Travel(WorldMap);
}

void UDownstreamSessionSubsystem::StartClassic()
{
    PendingMode = EDownstreamStartMode::Classic;
    Travel(ClassicMap);
}

void UDownstreamSessionSubsystem::ReturnToMenu()
{
    PendingMode = EDownstreamStartMode::None;
    Travel(MainMenuMap);
}

void UDownstreamSessionSubsystem::QuitGame()
{
    if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
        UKismetSystemLibrary::QuitGame(World, World->GetFirstPlayerController(), EQuitPreference::Quit, false);
}

EDownstreamStartMode UDownstreamSessionSubsystem::ConsumeStartMode(uint32& OutSeed)
{
    const EDownstreamStartMode Mode = PendingMode;
    OutSeed = PendingSeed;
    PendingMode = EDownstreamStartMode::None;
    return Mode;
}

bool UDownstreamSessionSubsystem::HasWorldSave() const
{
    return GetWorldSaveSeed() > 0;
}

int64 UDownstreamSessionSubsystem::GetWorldSaveSeed() const
{
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *WorldSavePath())) return 0;
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()) return 0;
    const TSharedPtr<FJsonObject>* Earth = nullptr;
    double Seed = 0.0;
    if (!Root->TryGetObjectField(TEXT("earthworks"), Earth) || !Earth || !(*Earth)->TryGetNumberField(TEXT("seed"), Seed)) return 0;
    return static_cast<int64>(Seed);
}

void UDownstreamSessionSubsystem::SetMasterVolume(float Volume)
{
    MasterVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
    ApplyAudio();
}

void UDownstreamSessionSubsystem::SetShowHints(bool bShow) { bShowHints = bShow; }

void UDownstreamSessionSubsystem::ApplyAudio() const
{
    if (!GEngine) return;
    if (FAudioDeviceHandle Device = GEngine->GetMainAudioDevice())
        Device->SetTransientPrimaryVolume(MasterVolume);
}

void UDownstreamSessionSubsystem::SaveOptions() const
{
    if (!GConfig) return;
    GConfig->SetFloat(OptionsSection, TEXT("MasterVolume"), MasterVolume, GGameUserSettingsIni);
    GConfig->SetBool(OptionsSection, TEXT("ShowHints"), bShowHints, GGameUserSettingsIni);
    GConfig->SetBool(OptionsSection, TEXT("ShowFPS"), bShowFPS, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}

void UDownstreamSessionSubsystem::SetShowFPS(bool bShow, UObject* WorldContext)
{
    bShowFPS = bShow;
    ApplyRuntimeOptions(WorldContext);
}

void UDownstreamSessionSubsystem::ApplyRuntimeOptions(UObject* WorldContext)
{
    ApplyAudio();
    UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
    if (!World || !GEngine || !World->GetGameViewport()) return;
    if (World->GetGameViewport()->IsStatEnabled(TEXT("FPS")) != bShowFPS)
        GEngine->Exec(World, TEXT("stat fps"));
}
