#pragma once

#include "CoreMinimal.h"
#include "DownstreamEnemy.h"
#include "DownstreamRaider.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EDownstreamRaiderRole : uint8
{
    Guard,      // melee bruiser
    Rifleman,   // ranged
    Engineer    // runs to levee breaches and sandbags them shut
};

// Dam Cult raider: a human (animated mannequin) bound to one outpost. Guards hunt the
// digger when it works near their levee; engineers dam breaches through the water system.
UCLASS()
class DOWNSTREAMCOMBAT_API ADownstreamRaider : public ADownstreamEnemy
{
    GENERATED_BODY()
public:
    ADownstreamRaider();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raider") EDownstreamRaiderRole RaiderRole = EDownstreamRaiderRole::Guard;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raider") int32 OutpostIndex = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raider") TObjectPtr<UStaticMeshComponent> Sandbag;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raider") TObjectPtr<UTextRenderComponent> RoleLabel;
    UFUNCTION(BlueprintPure, Category="Raider") int32 GetDamActions() const { return DamActions; }
    UFUNCTION(BlueprintPure, Category="Raider") bool IsDamming() const { return bDamming; }

protected:
    virtual void BeginPlay() override;
    virtual float InitialHealth() const override;
    virtual AActor* ChooseTarget() const override;
    virtual int32 SalvageDrop() const override { return RaiderRole == EDownstreamRaiderRole::Engineer ? 1 : 2; }
    virtual void OnCombatDeath() override;
    virtual void MoveToward(const FVector& Location, float AcceptanceRadius) override;
    virtual void StopMoving() override;

private:
    void TickEngineer(float DeltaSeconds);
    void ScrambleStep(const FVector& Direction, float DeltaSeconds);
    float LastDeltaSeconds = 0.016f;
    float MoveBestDistance = TNumericLimits<float>::Max();
    double MoveProgressTime = 0.0;
    double ScrambleUntil = 0.0;
    double NextDam = 0.0;
    int32 DamActions = 0;
    bool bDamming = false;
    float CarryPhase = 0.0f;
    float BestBreachDistance = TNumericLimits<float>::Max();
    double LastProgressTime = 0.0;
    bool bDirectToBreach = false;
    double SidestepUntil = 0.0;
    double NextDebugLog = 0.0;
    float SidestepSign = 1.0f;
};
