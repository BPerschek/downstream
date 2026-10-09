#pragma once

#include "CoreMinimal.h"
#include "DownstreamCombatCharacter.h"
#include "DownstreamCombatRules.h"
#include "DownstreamPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;

UCLASS()
class DOWNSTREAMCOMBAT_API ADownstreamPlayerCharacter : public ADownstreamCombatCharacter
{
    GENERATED_BODY()

public:
    ADownstreamPlayerCharacter();
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual bool IsPlayerCombatant() const override { return true; }

    UFUNCTION(BlueprintCallable, Category="Combat") void FireAt(FVector WorldTarget);
    UFUNCTION(BlueprintCallable, Category="Combat") void PerformMelee();
    UFUNCTION(BlueprintCallable, Category="Combat") void Dodge();
    UFUNCTION(BlueprintCallable, Category="Expedition") void Interact();
    UFUNCTION(BlueprintPure, Category="Combat") FVector GetAimLocation() const { return AimLocation; }
    UFUNCTION(BlueprintPure, Category="Combat") bool IsDodging() const { return bDodging; }

protected:
    virtual void BeginPlay() override;
    virtual float InitialHealth() const override;
    virtual bool IsProtectedFromDamage() const override;
    virtual void OnCombatDeath() override;

private:
    void MoveForward(float Value);
    void MoveRight(float Value);
    void StartFire();
    void StopFire();
    void ChooseCombatSkill();
    void ChooseSalvageSkill();
    void UpdateAim();
    void EndDodge();
    FVector MovementDirection() const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta=(AllowPrivateAccess="true")) TObjectPtr<USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta=(AllowPrivateAccess="true")) TObjectPtr<UCameraComponent> IsometricCamera;
    FVector AimLocation = FVector::ZeroVector;
    float ForwardInput = 0.0f;
    float RightInput = 0.0f;
    float OriginalGroundFriction = 8.0f;
    float OriginalBraking = 2048.0f;
    double DodgeEndTime = 0.0;
    bool bFiring = false;
    bool bDodging = false;
    downstream::CombatCooldown RangedCooldown;
    downstream::CombatCooldown MeleeCooldown;
    downstream::CombatCooldown DodgeCooldown;
};
