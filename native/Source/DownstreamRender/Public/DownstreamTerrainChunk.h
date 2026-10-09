#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DownstreamTerrainChunk.generated.h"

class APlayerController;
class APawn;
class UDownstreamSimulationSubsystem;
class UMaterialInterface;
class UProceduralMeshComponent;
class USpringArmComponent;

// Presentation/input for the M0 patch. The simulation owns every terrain height.
UCLASS()
class DOWNSTREAMRENDER_API ADownstreamTerrainChunk : public AActor
{
    GENERATED_BODY()

public:
    ADownstreamTerrainChunk();
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Downstream|Terrain")
    TObjectPtr<UProceduralMeshComponent> TerrainMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Downstream|Terrain", meta=(ClampMin="0"))
    int32 ChunkX = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Downstream|Terrain", meta=(ClampMin="0"))
    int32 ChunkY = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Downstream|Terrain", meta=(ClampMin="1", ClampMax="32"))
    int32 ChunkCellCount = 32;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Downstream|Terrain", meta=(ClampMin="1.0", Units="cm"))
    float CellSize = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Downstream|Terrain", meta=(ClampMin="0.0"))
    float HeightVisualScale = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Downstream|Terrain")
    TObjectPtr<UMaterialInterface> TerrainMaterial;

    UFUNCTION(BlueprintPure, Category="Downstream|Terrain")
    int32 GetRebuildCount() const { return RebuildCount; }

    // Reads the authoritative central cell, including edits applied at the last tick.
    UFUNCTION(BlueprintPure, Category="Downstream|Terrain")
    float GetCellHeight() const;

    UFUNCTION(BlueprintCallable, Category="Downstream|Terrain")
    void RaiseCentralRegion();

    UFUNCTION(BlueprintCallable, Category="Downstream|Terrain")
    void LowerCentralRegion();

    UFUNCTION(BlueprintCallable, Category="Downstream|Debug")
    void ToggleDebugView();

protected:
    virtual void BeginPlay() override;

private:
    UDownstreamSimulationSubsystem* GetSimulationSubsystem() const;
    bool GetPatchBounds(FIntPoint& Origin, FIntPoint& CellCounts) const;
    bool RebuildMesh();
    void QueueCentralRegion(float HeightDelta);
    void BindDebugInput();

    int32 RebuildCount = 0;
    TWeakObjectPtr<APlayerController> InputController;
    TWeakObjectPtr<APawn> SavedPlayerPawn;
    TWeakObjectPtr<USpringArmComponent> SavedSpringArm;
    FVector SavedPlayerLocation = FVector::ZeroVector;
    FRotator SavedPlayerRotation = FRotator::ZeroRotator;
    FRotator SavedControlRotation = FRotator::ZeroRotator;
    FVector SavedCameraTargetOffset = FVector::ZeroVector;
    float SavedCameraArmLength = 0.0f;
    bool bAtDebugView = false;
};
