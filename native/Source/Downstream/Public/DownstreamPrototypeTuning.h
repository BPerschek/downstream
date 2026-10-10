#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DownstreamPrototypeTuning.generated.h"

class USoundBase;
class UMaterialInterface;

// Optional /Game/Downstream/Data/DA_PrototypeTuning overrides these playable defaults.
UCLASS(BlueprintType)
class DOWNSTREAM_API UDownstreamPrototypeTuning : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player", meta=(ClampMin="1")) float PlayerHealth = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player", meta=(ClampMin="0")) float PlayerMoveSpeed = 550.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0")) float RangedDamage = 25.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.01")) float RangedCooldown = 0.22f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="1")) float ProjectileSpeed = 2400.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="1")) float ProjectileRange = 2000.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0")) float MeleeDamage = 40.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="1")) float MeleeRadius = 180.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.01")) float MeleeCooldown = 0.65f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0")) float DodgeStrength = 1100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.01")) float DodgeCooldown = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemies", meta=(ClampMin="1")) float ChaserHealth = 65.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemies", meta=(ClampMin="0")) float ChaserSpeed = 280.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemies", meta=(ClampMin="0")) float ChaserDamage = 12.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemies", meta=(ClampMin="1")) float ChaserAttackRange = 140.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemies", meta=(ClampMin="0.01")) float ChaserCooldown = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemies", meta=(ClampMin="1")) float ShooterHealth = 50.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemies", meta=(ClampMin="0")) float ShooterSpeed = 220.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemies", meta=(ClampMin="0")) float ShooterDamage = 10.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemies", meta=(ClampMin="0.01")) float ShooterCooldown = 1.4f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemies", meta=(ClampMin="1")) float ShooterRange = 1100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemies", meta=(ClampMin="1")) float EnemyProjectileSpeed = 1200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Enemies", meta=(ClampMin="1")) float AggroRange = 1700.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player", meta=(ClampMin="0")) float RespawnDelay = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Expedition", meta=(ClampMin="1")) int32 InventoryCapacity = 12;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Expedition", meta=(ClampMin="1")) float HomeRadius = 340.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Expedition") FVector HomeLocation = FVector(-1450.0f, -850.0f, 105.0f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settlement", meta=(ClampMin="1")) int32 StorageCost = 4;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settlement", meta=(ClampMin="1")) int32 WorkshopCost = 6;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settlement", meta=(ClampMin="1")) int32 ChargerCost = 4;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settlement", meta=(ClampMin="1")) int32 HaulerCost = 8;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settlement", meta=(ClampMin="0")) int32 StorageCapacityBonus = 8;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settlement") FVector BuildGridCenter = FVector(-1500.0f, -550.0f, 10.0f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settlement", meta=(ClampMin="1")) float BuildGridCellSize = 200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settlement") int32 BuildGridMinX = -4;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settlement") int32 BuildGridMaxX = 4;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settlement") int32 BuildGridMinY = -3;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settlement") int32 BuildGridMaxY = 3;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hauler", meta=(ClampMin="1")) int32 HaulerCargoCapacity = 4;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hauler", meta=(ClampMin="0")) float HaulerMoveSpeed = 330.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hauler", meta=(ClampMin="0")) float HaulerBatteryDrain = 1.8f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hauler", meta=(ClampMin="0")) float HaulerChargeRate = 18.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hauler", meta=(ClampMin="0")) float HaulerAvoidHostileRadius = 650.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hauler", meta=(ClampMin="0", ClampMax="100")) float HaulerBatteryReserve = 12.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hauler", meta=(ClampMin="100", ClampMax="100")) float HaulerBatteryMax = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Earthworks", meta=(ClampMin="1")) int32 DiggerCost = 10;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Earthworks", meta=(ClampMin="1")) float DiggerMoveSpeed = 380.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Earthworks", meta=(ClampMin="0.05")) float DigActionSeconds = 0.55f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Earthworks", meta=(ClampMin="0.01", Units="m")) float DigStepDepth = 0.3f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Earthworks", meta=(ClampMin="1")) float DigReach = 230.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Earthworks", meta=(ClampMin="1", ClampMax="3")) int32 MaxQueuedCanals = 3;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Earthworks", meta=(ClampMin="1")) int32 VaultReward = 16;
    // Movement speed multiplier when standing in 1 m of water; deeper water is slower still.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Earthworks", meta=(ClampMin="0.05", ClampMax="1")) float WadeSpeedAtOneMetre = 0.45f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Earthworks") FVector WaterSiteOrigin = FVector(300.0f, -2600.0f, 0.0f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback") TObjectPtr<USoundBase> ShotSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback") TObjectPtr<USoundBase> HitSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback") TObjectPtr<USoundBase> PickupSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback") TObjectPtr<USoundBase> ExtractionSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback") TObjectPtr<USoundBase> BuildSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback") TObjectPtr<USoundBase> ErrorSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback") TObjectPtr<USoundBase> DockSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance") TObjectPtr<UMaterialInterface> EnemyMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance") TObjectPtr<UMaterialInterface> ProjectileMaterial;
};
