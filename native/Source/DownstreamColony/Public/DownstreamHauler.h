#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DownstreamHauler.generated.h"

class ADownstreamSalvagePickup;
class UDownstreamExpeditionSubsystem;
class UDownstreamPrototypeTuning;
class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;

// Navigation/presentation only. Cargo, battery, construction and intent belong to Downstream.
UCLASS()
class DOWNSTREAMCOLONY_API ADownstreamHauler : public ACharacter
{
    GENERATED_BODY()
public:
    ADownstreamHauler();
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION(BlueprintPure, Category="Downstream|Hauler") FText GetStatus() const { return Status; }
    UFUNCTION(BlueprintPure, Category="Downstream|Hauler") ADownstreamSalvagePickup* GetTargetPickup() const;
    void OnNavigationFinished(bool bSucceeded, bool bAborted);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Hauler") TObjectPtr<UStaticMeshComponent> BotMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Hauler") TObjectPtr<UStaticMeshComponent> CargoMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Hauler") TObjectPtr<UStaticMeshComponent> StatusCore;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Hauler") TObjectPtr<UTextRenderComponent> WorldLabel;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Hauler") TObjectPtr<UPointLightComponent> StatusGlow;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    enum class EJob : uint8 { None, Pickup, Storage, Charger };
    void DecideNextAction();
    void RefreshAppearance();
    bool IsPickupSafe(const ADownstreamSalvagePickup* Pickup) const;
    ADownstreamSalvagePickup* FindPickup();
    bool FindDock(const FVector& BuildingLocation, float Radius, FVector& OutLocation) const;
    bool HasCompletePath(const FVector& Destination, float* OutLength = nullptr) const;
    bool MoveTo(const FVector& Destination, float AcceptanceRadius);
    void ReturnToStorage(bool bPaused);
    void ReturnToCharger();
    void BlockCurrentJob();
    void StopNavigation();
    void SetStatus(const TCHAR* NewStatus);
    bool GetStructureLocation(uint8 Type, FVector& OutLocation) const;

    UPROPERTY(Transient) TObjectPtr<UDownstreamExpeditionSubsystem> Expedition;
    // The subsystem keeps the loaded data asset alive for the lifetime of this world.
    const UDownstreamPrototypeTuning* Tuning = nullptr;
    TWeakObjectPtr<ADownstreamSalvagePickup> TargetPickup;
    TMap<TWeakObjectPtr<ADownstreamSalvagePickup>, double> PickupBackoff;
    FText Status;
    EJob Job = EJob::None;
    FVector MoveDestination = FVector::ZeroVector;
    FVector ProgressLocation = FVector::ZeroVector;
    double NextDecision = 0.0;
    double NextMoveRequest = 0.0;
    double NextNavigationRetry = 0.0;
    double LastProgressTime = 0.0;
    double BlockedUntil = 0.0;
    double NextCheckpoint = 0.0;
    double NextCargoTransaction = 0.0;
    float LastCheckpointBattery = -1.0f;
    bool bHasMoveDestination = false;
    bool bNavigationFailed = false;
    bool bChargeRequired = false;
    bool bCharging = false;
    bool bIdleRechargeCompleted = false;
    bool bLastHaulingEnabled = false;
};
