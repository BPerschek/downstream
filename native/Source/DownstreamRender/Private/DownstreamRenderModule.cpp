#include "Modules/ModuleManager.h"

#include "DownstreamTerrainChunk.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace
{
ADownstreamTerrainChunk* FindTerrainChunk(UWorld* World)
{
    if (!World || !World->IsGameWorld())
        return nullptr;
    for (TActorIterator<ADownstreamTerrainChunk> It(World); It; ++It)
        if (IsValid(*It) && !It->IsActorBeingDestroyed())
            return *It;
    return nullptr;
}

void ToggleTerrainView(UWorld* World)
{
    if (ADownstreamTerrainChunk* Chunk = FindTerrainChunk(World))
        Chunk->ToggleDebugView();
}

void RaiseTerrain(UWorld* World)
{
    if (ADownstreamTerrainChunk* Chunk = FindTerrainChunk(World))
        Chunk->RaiseCentralRegion();
}

void LowerTerrain(UWorld* World)
{
    if (ADownstreamTerrainChunk* Chunk = FindTerrainChunk(World))
        Chunk->LowerCentralRegion();
}

FAutoConsoleCommandWithWorld DebugTerrainCommand(
    TEXT("Downstream.DebugTerrain"), TEXT("Toggle the M0 terrain viewpoint (same as F6)."),
    FConsoleCommandWithWorldDelegate::CreateStatic(&ToggleTerrainView));
FAutoConsoleCommandWithWorld RaiseTerrainCommand(
    TEXT("Downstream.RaiseTerrain"), TEXT("Queue a +2 central terrain edit (same as F7)."),
    FConsoleCommandWithWorldDelegate::CreateStatic(&RaiseTerrain));
FAutoConsoleCommandWithWorld LowerTerrainCommand(
    TEXT("Downstream.LowerTerrain"), TEXT("Queue a -2 central terrain edit (same as F8)."),
    FConsoleCommandWithWorldDelegate::CreateStatic(&LowerTerrain));
}

IMPLEMENT_MODULE(FDefaultModuleImpl, DownstreamRender);
