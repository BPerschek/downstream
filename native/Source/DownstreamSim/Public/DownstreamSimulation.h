#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Unreal supplies this export macro; standalone builds have no DLL boundary.
#ifndef DOWNSTREAMSIM_API
#define DOWNSTREAMSIM_API
#endif

namespace downstream
{
struct Config
{
    int Width = 256;
    int Height = 256;
    int ChunkSize = 32;
    double TickSeconds = 0.1;
    float MinimumHeight = -10.0f;
    float MaximumHeight = 10.0f;
};

struct ChunkCoord
{
    int X = 0;
    int Y = 0;
};

// Temporary M0 generator: no Unreal random state or platform-specific hashing.
DOWNSTREAMSIM_API float SeededHeight(std::uint32_t seed, int x, int y,
                                    float minimumHeight, float maximumHeight);

class DOWNSTREAMSIM_API Simulation
{
public:
    explicit Simulation(Config config, std::uint32_t seed);

    void Advance(double seconds);
    void QueueTerrainEdit(int x, int y, float height);
    std::uint64_t TickCount() const noexcept;
    double ElapsedSeconds() const noexcept;
    float HeightAt(int x, int y) const;
    std::vector<ChunkCoord> ConsumeDirtyChunks();
    std::uint32_t Seed() const noexcept;

private:
    struct TerrainEdit
    {
        int X;
        int Y;
        float Height;
    };

    std::size_t CellIndex(int x, int y) const;
    Config Settings;
    std::uint32_t WorldSeed;
    std::uint64_t Ticks = 0;
    long double AccumulatedSeconds = 0.0L;
    int ChunkColumns = 0;
    int ChunkRows = 0;
    std::vector<float> Heights;
    std::vector<unsigned char> DirtyChunks;
    std::vector<TerrainEdit> PendingEdits;
};
}
