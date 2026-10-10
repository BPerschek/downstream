#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DownstreamEnemy.h"
#include "DownstreamGameMode.generated.h"

class ADownstreamSalvagePickup;

UCLASS()
class DOWNSTREAMCOMBAT_API ADownstreamGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ADownstreamGameMode();
    virtual void StartPlay() override;
    virtual void RestartPlayer(AController* NewPlayer) override;
    void RequestPlayerRespawn(AController* PlayerController, APawn* DeadPawn);

    // Field respawn: enemies and salvage caches placed in the level come back after
    // EnemyRespawnSeconds / CacheRespawnSeconds while the player is outside
    // FieldRespawnSafeRadius of that spot. Disabled with -DownstreamNoFieldRespawn.
    UFUNCTION(BlueprintCallable, Category="Downstream|Field") int32 RespawnFieldNow();
    UFUNCTION(BlueprintPure, Category="Downstream|Field") int32 GetFieldRespawnCount() const { return FieldRespawns; }
    UFUNCTION(BlueprintPure, Category="Downstream|Field") int32 GetTrackedEnemyCount() const { return EnemySlots.Num(); }
    UFUNCTION(BlueprintPure, Category="Downstream|Field") int32 GetTrackedCacheCount() const { return CacheSlots.Num(); }
    UFUNCTION(BlueprintPure, Category="Downstream|Field") bool IsFieldRespawnEnabled() const { return bFieldRespawnEnabled; }
    // Runtime-spawned world content (M2) joins the same respawn rules as level-placed actors.
    UFUNCTION(BlueprintCallable, Category="Downstream|Field") void RegisterFieldEnemy(ADownstreamEnemy* Enemy);
    UFUNCTION(BlueprintCallable, Category="Downstream|Field") void RegisterFieldCache(ADownstreamSalvagePickup* Pickup);

private:
    struct FEnemySlot
    {
        TWeakObjectPtr<ADownstreamEnemy> Enemy;
        FTransform Spawn;
        EDownstreamEnemyKind Kind = EDownstreamEnemyKind::Chaser;
        double DownSince = -1.0;
    };
    struct FCacheSlot
    {
        TWeakObjectPtr<ADownstreamSalvagePickup> Pickup;
        FVector Location = FVector::ZeroVector;
        int32 Amount = 4;
        double GoneSince = -1.0;
    };
    void RegisterField();
    void TickField();
    bool SpawnEnemy(FEnemySlot& Slot);
    bool SpawnCache(FCacheSlot& Slot);

    FTimerHandle RespawnTimer;
    FTimerHandle FieldTimer;
    TArray<FEnemySlot> EnemySlots;
    TArray<FCacheSlot> CacheSlots;
    bool bFieldRegistered = false;
    bool bFieldRespawnEnabled = true;
    int32 FieldRespawns = 0;
};
