#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "DownstreamSimulation.h"
#include <memory>
#include "DownstreamSimulationSubsystem.generated.h"

// The game-thread adapter owns lifetime; the plain C++ simulation owns state.
// Worker ownership is deferred until the full command/snapshot bridge is profiled.
UCLASS()
class DOWNSTREAM_API UDownstreamSimulationSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    UFUNCTION(BlueprintPure, Category="Downstream|Simulation")
    static UDownstreamSimulationSubsystem* GetForWorld(UObject* ContextObject);

    UFUNCTION(BlueprintPure, Category="Downstream|Simulation")
    int64 GetSimulationTick() const;

    UFUNCTION(BlueprintCallable, Category="Downstream|Simulation")
    bool QueueTerrainHeight(int32 X, int32 Y, float Height);

    UFUNCTION(BlueprintPure, Category="Downstream|Simulation")
    float GetTerrainHeight(int32 X, int32 Y) const;

    UFUNCTION(BlueprintPure, Category="Downstream|Simulation")
    int32 GetGridWidth() const { return GridWidth; }

    UFUNCTION(BlueprintPure, Category="Downstream|Simulation")
    int32 GetGridHeight() const { return GridHeight; }

    UFUNCTION(BlueprintPure, Category="Downstream|Simulation")
    int32 GetTerrainChunkSize() const { return TerrainChunkSize; }

    // Same-game-thread presentation access. This is not a worker snapshot API.
    const downstream::Simulation* GetSimulation() const { return Simulation.get(); }
    std::vector<downstream::ChunkCoord> ConsumeDirtyChunks();

private:
    std::unique_ptr<downstream::Simulation> Simulation;
    int32 GridWidth = 0;
    int32 GridHeight = 0;
    int32 TerrainChunkSize = 0;
};
