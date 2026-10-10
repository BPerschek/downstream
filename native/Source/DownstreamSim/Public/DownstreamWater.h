#pragma once

// Shallow-water grid for DOWNSTREAM (design doc section 9).
// Plain standard C++: no UObject, Actor or rendering dependencies.
//
// Model: "virtual pipes" between 4-neighbour cells. Each cell has GroundHeight
// and WaterDepth (metres). Water moves toward lower total surface height
// (ground + water). Fluxes live on edges, so whatever leaves one cell enters
// its neighbour: volume is conserved except for explicit sources, sinks,
// edge drainage and evaporation, which are all counted.

#include <cstddef>
#include <cstdint>
#include <vector>

#ifndef DOWNSTREAMSIM_API
#define DOWNSTREAMSIM_API
#endif

namespace downstream
{
struct WaterConfig
{
    int Width = 32;
    int Height = 32;
    float CellSize = 1.0f;          // metres per cell side
    double TickSeconds = 0.1;       // fixed simulation tick
    int SubstepsPerTick = 4;
    float Gravity = 9.81f;
    float FluxDamping = 0.985f;     // per substep; removes pipe oscillation
    float MinimumGround = 0.0f;
    float MaximumGround = 20.0f;
    float EvaporationPerSecond = 0.0f; // metres of depth per second, applied to wet cells
    bool DrainAtEdges = false;      // grid boundary removes water that flows out
    int ChunkSize = 16;             // ground dirty tracking granularity
};

struct WaterCell
{
    int X = 0;
    int Y = 0;
};

struct FlowVector
{
    float X = 0.0f; // m/s, +X
    float Y = 0.0f; // m/s, +Y
};

class DOWNSTREAMSIM_API WaterGrid
{
public:
    explicit WaterGrid(WaterConfig config);

    int Width() const noexcept { return Settings.Width; }
    int Height() const noexcept { return Settings.Height; }
    const WaterConfig& Config() const noexcept { return Settings; }
    bool InBounds(int x, int y) const noexcept;

    // Ground and water access (metres).
    float Ground(int x, int y) const;
    float Water(int x, int y) const;
    float Surface(int x, int y) const;
    void SetGround(int x, int y, float height);
    void SetWater(int x, int y, float depth);
    FlowVector Flow(int x, int y) const;

    // Terrain edits. Return the volume of ground actually moved (m^3).
    // Lowering never goes below floor; raising never goes above ceiling.
    double LowerGround(int x, int y, float amount, float floor);
    double RaiseGround(int x, int y, float amount, float ceiling);

    // Removes up to Volume m^3 from a cell (pumps, drains). Counted as sunk. Returns removed volume.
    double RemoveWater(int x, int y, double volume);

    // Sources add volume each second; sinks remove all water that reaches them.
    void SetSource(int x, int y, float cubicMetresPerSecond);
    float Source(int x, int y) const;
    void SetSink(int x, int y, bool sink);
    bool IsSink(int x, int y) const;

    // Fixed-timestep advance. Partial ticks accumulate. Large advances are capped
    // at MaxTicksPerAdvance so a frame hitch cannot stall the game thread.
    static constexpr int MaxTicksPerAdvance = 8;
    int Advance(double seconds);
    void Tick();
    std::uint64_t TickCount() const noexcept { return Ticks; }

    // Accounting (m^3). Total == initial + sourced - sunk - drained - evaporated.
    double TotalWater() const;
    double SourcedVolume() const noexcept { return Sourced; }
    double SunkVolume() const noexcept { return Sunk; }
    double DrainedVolume() const noexcept { return Drained; }
    double EvaporatedVolume() const noexcept { return Evaporated; }
    double AddedVolume() const noexcept { return Added; }
    float MaxDepth() const;

    // Render dirtiness. Ground edits mark chunks; water changes every tick.
    std::uint64_t GroundRevision() const noexcept { return GroundRevisionCounter; }
    std::vector<WaterCell> ConsumeDirtyGroundChunks();

private:
    std::size_t Index(int x, int y) const;
    void CheckedIndex(int x, int y) const;
    void Substep(float dt);
    void MarkGroundDirty(int x, int y);

    WaterConfig Settings;
    std::uint64_t Ticks = 0;
    double Accumulated = 0.0;
    std::vector<float> GroundHeights;
    std::vector<float> Depths;
    std::vector<float> Sources;
    std::vector<unsigned char> Sinks;
    // Edge fluxes in m^3/s. FluxX has (Width+1)*Height edges: edge (e,y) carries
    // flow from cell (e-1,y) to (e,y); edges 0 and Width touch the boundary and
    // are used only when DrainAtEdges is set. FluxY likewise has Width*(Height+1).
    std::vector<float> FluxX;
    std::vector<float> FluxY;
    std::vector<float> Scratch;
    double Sourced = 0.0;
    double Sunk = 0.0;
    double Drained = 0.0;
    double Evaporated = 0.0;
    double Added = 0.0;
    std::uint64_t GroundRevisionCounter = 0;
    int ChunkColumns = 0;
    int ChunkRows = 0;
    std::vector<unsigned char> DirtyGround;
};
}
