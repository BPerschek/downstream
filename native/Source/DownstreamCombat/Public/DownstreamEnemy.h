#pragma once

#include "CoreMinimal.h"
#include "DownstreamCombatCharacter.h"
#include "DownstreamCombatRules.h"
#include "DownstreamEnemy.generated.h"

class UStaticMeshComponent;
class ADownstreamPlayerCharacter;

UENUM(BlueprintType)
enum class EDownstreamEnemyKind : uint8
{
    Chaser,
    Shooter
};

UCLASS()
class DOWNSTREAMCOMBAT_API ADownstreamEnemy : public ADownstreamCombatCharacter
{
    GENERATED_BODY()
public:
    ADownstreamEnemy();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") EDownstreamEnemyKind Kind = EDownstreamEnemyKind::Chaser;

    // Where this enemy returns when it has no target (spawn point or a post).
    UFUNCTION(BlueprintCallable, Category="Combat") void SetHomePost(FVector Location) { SpawnLocation = Location; }
    UFUNCTION(BlueprintPure, Category="Combat") AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

protected:
    virtual void BeginPlay() override;
    virtual float InitialHealth() const override;
    virtual void OnCombatDeath() override;
    // Player within aggro, or a working robot (DownstreamRobot tag) when it is closer.
    virtual AActor* ChooseTarget() const;
    virtual int32 SalvageDrop() const { return 2; }
    void FightTarget(AActor* Target);
    void ReturnToPost();
    virtual void MoveToward(const FVector& Location, float AcceptanceRadius);
    virtual void StopMoving();
    void AttackTarget(AActor* Target);
    bool HasClearShot(const AActor* Target) const;
    static bool IsTargetAlive(const AActor* Target);
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> RobotMesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ReactorCore;
    FVector SpawnLocation = FVector::ZeroVector;
    downstream::CombatCooldown AttackCooldown;
    TWeakObjectPtr<AActor> CurrentTarget;
};
