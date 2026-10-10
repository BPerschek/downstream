#pragma once

// Seeded world generation and Dam Cult outpost rules (design brief sections 3.7, 14, 15).
// Plain standard C++. The same seed always produces the same untouched world.

#include "DownstreamWater.h"

#include <cstdint>
#include <vector>

namespace downstream
{
enum class OutpostStructureType { Hut, Generator, Stash };
enum class WorldPropType { Barrel, Crate, Container, Tank, Rock, Chimney };

struct OutpostStructure
{
    OutpostStructureType Type = OutpostStructureType::Hut;
    WaterCell Cell;
};

// A Dam Cult outpost: a flat pad sunk below a nearby river, ringed by an earth levee.
// Cutting through the levee lets the river in.
struct OutpostLayout
{
    int Id = 0;
    WaterCell Center;
    float PadHeight = 0.0f;
    float RimHeight = 0.0f;
    int PadRadius = 5;
    int RimRadius = 9;
    WaterCell RiverCell;            // nearest river cell the levee holds back
    std::vector<OutpostStructure> Structures;
    std::vector<WaterCell> GuardSpawns;
    std::vector<WaterCell> EngineerSpawns;
};

// A flooded bowl with a sealed vault on its floor, drained by a canal to the river.
struct VaultLayout
{
    int Id = 0;
    WaterCell Cell;
    WaterCell Spring;
    WaterCell RiverCell;            // river cell the spillway feeds; lower than the bowl floor
    float Floor = 0.0f;
    float SpillLevel = 0.0f;
};

struct CacheLayout { WaterCell Cell; int Amount = 4; };
struct PatrolLayout { WaterCell Cell; bool Shooter = false; };
struct PropLayout { WaterCell Cell; WorldPropType Type = WorldPropType::Barrel; float Yaw = 0.0f; };
struct RiverLayout { WaterCell Spring; std::vector<WaterCell> Path; std::vector<float> Bed; };

struct WorldGenSettings
{
    int Width = 112;
    int Height = 88;
    int Rivers = 2;
    int Outposts = 3;
    int Vaults = 1;
    int Caches = 10;
    int Patrols = 6;
    float SpringRate = 0.8f;     // m^3/s per river spring: enough that a levee cut floods a camp in about a minute
    float VaultSpringRate = 0.1f;
};

struct WorldLayout
{
    std::uint32_t Seed = 0;
    int Width = 0;
    int Height = 0;
    int LowEdge = 0;             // 0 -X, 1 +X, 2 -Y, 3 +Y: the drainage side
    std::vector<float> Ground;   // base (untouched) ground, row-major
    std::vector<float> Prefill;  // initial water depth, row-major
    std::vector<std::pair<WaterCell, float>> Sources;
    std::vector<WaterCell> Sinks;
    WaterCell Home;
    int HomeHalfX = 11;
    int HomeHalfY = 9;
    std::vector<RiverLayout> Rivers;
    std::vector<OutpostLayout> Outposts;
    std::vector<VaultLayout> Vaults;
    std::vector<CacheLayout> Caches;
    std::vector<PatrolLayout> Patrols;
    std::vector<PropLayout> Props;

    float GroundAt(int x, int y) const { return Ground[static_cast<std::size_t>(y) * Width + x]; }
    bool InBounds(int x, int y) const { return x >= 0 && y >= 0 && x < Width && y < Height; }
};

DOWNSTREAMSIM_API WorldLayout GenerateWorld(std::uint32_t seed, const WorldGenSettings& settings = {});
// The classic M1 Drowned Vault site expressed as a layout (one vault, no outposts).
DOWNSTREAMSIM_API WorldLayout MakeDrownedVaultLayout();
DOWNSTREAMSIM_API WaterConfig WorldWaterConfig(const WorldLayout& layout);
// Writes base ground, sources, sinks and (optionally) the settled initial water.
DOWNSTREAMSIM_API void ApplyWorld(WaterGrid& grid, const WorldLayout& layout, bool prefill = true);

// ---- Outpost runtime rules -------------------------------------------------
struct OutpostRules
{
    // Balanced in tests/world_tests.cpp: an unopposed breach razes an outpost in about a minute;
    // engineers that reach the breach within ~12 s usually seal it and the pump wins.
    float FloodThreshold = 0.25f;           // metres of water before structures take damage
    float DamagePerMetrePerSecond = 8.0f;   // integrity lost per metre above threshold per second
    float HutIntegrity = 70.0f;
    float StashIntegrity = 110.0f;
    float GeneratorIntegrity = 300.0f;
    float PumpRate = 0.35f;                 // m^3/s removed from the pad while the generator runs
    float DamStep = 0.4f;                   // metres an engineer adds per dam action
};

struct OutpostStructureState
{
    OutpostStructureType Type = OutpostStructureType::Hut;
    WaterCell Cell;
    float Integrity = 0.0f;
    float MaxIntegrity = 0.0f;
    bool Destroyed = false;
};

class DOWNSTREAMSIM_API OutpostState
{
public:
    OutpostState() = default;
    OutpostState(const OutpostLayout& layout, OutpostRules rules = {});

    const OutpostLayout& Layout() const noexcept { return Plan; }
    const std::vector<OutpostStructureState>& Structures() const noexcept { return Parts; }
    bool IsRazed() const;                           // generator destroyed
    int DestroyedMask() const;
    bool RestoreDestroyedMask(int mask);
    bool InPad(int x, int y) const;
    bool InLevee(int x, int y) const;               // pad edge out to just past the rim crest
    // Deepest water on the pad (m).
    float FloodDepth(const WaterGrid& grid) const;
    bool IsFlooded(const WaterGrid& grid) const { return FloodDepth(grid) > Rules.FloodThreshold; }
    // Apply flood damage for dt seconds; returns indices of structures destroyed this call.
    std::vector<int> ApplyFlood(const WaterGrid& grid, float dt);
    // While the generator stands, pumps remove water from the pad. Returns m^3 removed.
    double Pump(WaterGrid& grid, float dt);
    // A dug, wet cell in the levee that lets water in. Prefers cells nearest the crest.
    bool FindBreach(const WaterGrid& grid, WaterCell& breach) const;
    // One engineer dam action at a breach cell. Returns m^3 of fill placed.
    double DamBreach(WaterGrid& grid, WaterCell cell) const;
    // Direct damage (gunfire). Returns true when this call destroyed the structure.
    bool Damage(int index, float amount);
    const OutpostRules& GetRules() const noexcept { return Rules; }

private:
    float Distance(int x, int y) const;
    OutpostLayout Plan;
    OutpostRules Rules;
    std::vector<OutpostStructureState> Parts;
};
}
