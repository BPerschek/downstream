#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DownstreamSessionSubsystem.generated.h"

UENUM(BlueprintType)
enum class EDownstreamStartMode : uint8
{
    None,
    NewWorld,
    ContinueWorld,
    Classic
};

// Survives level travel: what the menu asked for, and player options that are not
// engine graphics settings (those live in UGameUserSettings).
UCLASS()
class DOWNSTREAM_API UDownstreamSessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    static const TCHAR* MainMenuMap;
    static const TCHAR* WorldMap;
    static const TCHAR* ClassicMap;

    UFUNCTION(BlueprintPure, Category="Downstream|Session") static UDownstreamSessionSubsystem* Get(const UObject* ContextObject);
    UFUNCTION(BlueprintCallable, Category="Downstream|Session") void StartNewWorld(int64 Seed);
    UFUNCTION(BlueprintCallable, Category="Downstream|Session") void ContinueWorld();
    UFUNCTION(BlueprintCallable, Category="Downstream|Session") void StartClassic();
    UFUNCTION(BlueprintCallable, Category="Downstream|Session") void ReturnToMenu();
    UFUNCTION(BlueprintCallable, Category="Downstream|Session") void QuitGame();

    // Consumed by the expedition subsystem when the world map initializes.
    EDownstreamStartMode ConsumeStartMode(uint32& OutSeed);
    UFUNCTION(BlueprintPure, Category="Downstream|Session") bool HasWorldSave() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Session") int64 GetWorldSaveSeed() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Session") static int64 MakeRandomSeed();
    static FString WorldSavePath();
    static FString ClassicSavePath();

    // Options (Saved/Config/<platform>/GameUserSettings.ini, section [Downstream.Options]).
    UFUNCTION(BlueprintPure, Category="Downstream|Options") float GetMasterVolume() const { return MasterVolume; }
    UFUNCTION(BlueprintCallable, Category="Downstream|Options") void SetMasterVolume(float Volume);
    UFUNCTION(BlueprintPure, Category="Downstream|Options") bool GetShowHints() const { return bShowHints; }
    UFUNCTION(BlueprintCallable, Category="Downstream|Options") void SetShowHints(bool bShow);
    UFUNCTION(BlueprintCallable, Category="Downstream|Options") void ApplyAudio() const;
    UFUNCTION(BlueprintPure, Category="Downstream|Options") bool GetShowFPS() const { return bShowFPS; }
    UFUNCTION(BlueprintCallable, Category="Downstream|Options") void SetShowFPS(bool bShow, UObject* WorldContext);
    // Volume and FPS counter for a freshly loaded map.
    UFUNCTION(BlueprintCallable, Category="Downstream|Options") void ApplyRuntimeOptions(UObject* WorldContext);
    UFUNCTION(BlueprintCallable, Category="Downstream|Options") void SaveOptions() const;

private:
    void Travel(const TCHAR* Map);
    EDownstreamStartMode PendingMode = EDownstreamStartMode::None;
    uint32 PendingSeed = 0;
    float MasterVolume = 0.8f;
    bool bShowHints = true;
    bool bShowFPS = false;
};
