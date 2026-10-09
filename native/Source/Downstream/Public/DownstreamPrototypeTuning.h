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
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback") TObjectPtr<USoundBase> ShotSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback") TObjectPtr<USoundBase> HitSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback") TObjectPtr<USoundBase> PickupSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback") TObjectPtr<USoundBase> ExtractionSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance") TObjectPtr<UMaterialInterface> EnemyMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance") TObjectPtr<UMaterialInterface> ProjectileMaterial;
};
