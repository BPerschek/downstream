#pragma once

#include "CoreMinimal.h"
#include "DownstreamCombatCharacter.h"
#include "DownstreamCombatRules.h"
#include "DownstreamSettlementTypes.h"
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
    UFUNCTION(BlueprintCallable, Category="Settlement") void ToggleBuildMenu();
    UFUNCTION(BlueprintCallable, Category="Settlement") void SelectStructure(EDownstreamStructureType Type);
    UFUNCTION(BlueprintCallable, Category="Settlement") bool PlaceSelectedStructure();
    UFUNCTION(BlueprintCallable, Category="Settlement") void CancelPlacement();
    UFUNCTION(BlueprintCallable, Category="Settlement") void ConstructHauler();
    UFUNCTION(BlueprintCallable, Category="Settlement") void ToggleHauling();
    UFUNCTION(BlueprintPure, Category="Settlement") bool IsBuildMenuOpen() const { return bBuildMenuOpen; }
    UFUNCTION(BlueprintPure, Category="Settlement") bool IsPlacementSelected() const { return bPlacementSelected; }
    UFUNCTION(BlueprintCallable, Category="Earthworks") void ConstructDigger();
    UFUNCTION(BlueprintCallable, Category="Earthworks") void ToggleDigPlanner();
    UFUNCTION(BlueprintCallable, Category="Earthworks") void ClearDigQueue();
    UFUNCTION(BlueprintCallable, Category="Earthworks") void DeepenCanals();
    UFUNCTION(BlueprintCallable, Category="Earthworks") void ShallowCanals();
    UFUNCTION(BlueprintCallable, Category="Debug") void RespawnFieldNow();
    UFUNCTION(BlueprintPure, Category="Earthworks") bool IsDigPlannerOpen() const { return bDigPlannerOpen; }
    UFUNCTION(BlueprintPure, Category="Earthworks") bool IsDraggingCanal() const { return bDraggingCanal; }
    UFUNCTION(BlueprintPure, Category="Earthworks") FIntPoint GetCanalDragStart() const { return CanalDragStart; }
    // Site cell under the mouse; false when the cursor is off the water site.
    UFUNCTION(BlueprintPure, Category="Earthworks") bool GetCanalCursorCell(FIntPoint& Cell) const { Cell = CanalCursor; return bCanalCursorValid; }
    // Test/automation entry points that bypass the mouse.
    UFUNCTION(BlueprintCallable, Category="Earthworks") bool PlanCanal(FIntPoint From, FIntPoint To);

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
    void ChooseCharger();
    void UpdatePlacementPreview();
    void UpdateCanalCursor();
    void BeginCanalDrag();
    void FinishCanalDrag();
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
    bool bBuildMenuOpen = false;
    bool bPlacementSelected = false;
    bool bDigPlannerOpen = false;
    bool bDraggingCanal = false;
    bool bCanalCursorValid = false;
    FIntPoint CanalDragStart = FIntPoint::ZeroValue;
    FIntPoint CanalCursor = FIntPoint::ZeroValue;
    EDownstreamStructureType SelectedStructure = EDownstreamStructureType::Storage;
    FVector PlacementLocation = FVector::ZeroVector;
    downstream::CombatCooldown RangedCooldown;
    downstream::CombatCooldown MeleeCooldown;
    downstream::CombatCooldown DodgeCooldown;
};
