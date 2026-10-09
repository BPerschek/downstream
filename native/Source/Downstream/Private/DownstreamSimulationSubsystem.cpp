#include "DownstreamSimulationSubsystem.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include <exception>

UDownstreamSimulationSubsystem* UDownstreamSimulationSubsystem::GetForWorld(UObject* ContextObject)
{
    UWorld* World = ContextObject ? ContextObject->GetWorld() : nullptr;
    return World ? World->GetSubsystem<UDownstreamSimulationSubsystem>() : nullptr;
}

void UDownstreamSimulationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    downstream::Config Config;
    int32 Width = 256, Height = 256, ChunkSize = 32, Seed = 41009;
    double TickSeconds = 0.1;
    const TCHAR* Section = TEXT("Downstream.Simulation");
    GConfig->GetInt(Section, TEXT("Width"), Width, GGameIni);
    GConfig->GetInt(Section, TEXT("Height"), Height, GGameIni);
    GConfig->GetInt(Section, TEXT("ChunkSize"), ChunkSize, GGameIni);
    GConfig->GetInt(Section, TEXT("Seed"), Seed, GGameIni);
    GConfig->GetDouble(Section, TEXT("TickSeconds"), TickSeconds, GGameIni);
    Config.Width = Width;
    Config.Height = Height;
    Config.ChunkSize = ChunkSize;
    Config.TickSeconds = TickSeconds;
    try
    {
        Simulation = std::make_unique<downstream::Simulation>(Config, static_cast<uint32_t>(Seed));
        GridWidth = Width;
        GridHeight = Height;
        TerrainChunkSize = ChunkSize;
    }
    catch (const std::exception& Error)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid Downstream simulation configuration: %s"), UTF8_TO_TCHAR(Error.what()));
    }
}

void UDownstreamSimulationSubsystem::Deinitialize()
{
    Simulation.reset();
    Super::Deinitialize();
}

bool UDownstreamSimulationSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UDownstreamSimulationSubsystem::Tick(float DeltaTime)
{
    if (Simulation && FMath::IsFinite(DeltaTime) && DeltaTime >= 0.0f)
    {
        try
        {
            Simulation->Advance(DeltaTime);
        }
        catch (const std::exception& Error)
        {
            UE_LOG(LogTemp, Error, TEXT("Downstream simulation stopped: %s"), UTF8_TO_TCHAR(Error.what()));
            Simulation.reset();
        }
    }
}

TStatId UDownstreamSimulationSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UDownstreamSimulationSubsystem, STATGROUP_Tickables);
}

int64 UDownstreamSimulationSubsystem::GetSimulationTick() const
{
    return Simulation ? static_cast<int64>(Simulation->TickCount()) : 0;
}

bool UDownstreamSimulationSubsystem::QueueTerrainHeight(int32 X, int32 Y, float Height)
{
    if (!Simulation) return false;
    try
    {
        Simulation->QueueTerrainEdit(X, Y, Height);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

float UDownstreamSimulationSubsystem::GetTerrainHeight(int32 X, int32 Y) const
{
    if (!Simulation) return 0.0f;
    try { return Simulation->HeightAt(X, Y); }
    catch (const std::exception&) { return 0.0f; }
}

std::vector<downstream::ChunkCoord> UDownstreamSimulationSubsystem::ConsumeDirtyChunks()
{
    return Simulation ? Simulation->ConsumeDirtyChunks() : std::vector<downstream::ChunkCoord>{};
}
