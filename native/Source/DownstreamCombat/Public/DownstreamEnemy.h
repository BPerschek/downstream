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

protected:
    virtual void BeginPlay() override;
    virtual float InitialHealth() const override;
    virtual void OnCombatDeath() override;

private:
    void MoveToward(const FVector& Location, float AcceptanceRadius);
    void StopMoving();
    void AttackPlayer(ADownstreamPlayerCharacter* Player);
    bool HasClearShot(const ADownstreamPlayerCharacter* Player) const;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> RobotMesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ReactorCore;
    FVector SpawnLocation = FVector::ZeroVector;
    downstream::CombatCooldown AttackCooldown;
};
