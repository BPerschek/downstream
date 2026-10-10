#pragma once

// Earthworks: the Drowned Vault site layout, canal dig jobs and the persisted
// terrain delta. Plain standard C++ on top of WaterGrid.

#include "DownstreamWater.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace downstream
{
// ---- Site --------------------------------------------------------------
// A raised plateau with a bowl-shaped basin. A burst main feeds the basin
// through a ditch; an east spillway keeps the basin full at SpillLevel, so the
// vault on the basin floor sits under ~1.3 m of water. An old culvert on the
// south edge drains anything that reaches it. Cutting a canal from the basin
// floor to the culvert drains the vault.
struct DrownedVaultSite
{
    static constexpr int Width = 32;
    static constexpr int Height = 20;
    static constexpr float CellSize = 1.0f;     // metres
    static constexpr float Plateau = 1.8f;
    static constexpr float BasinFloor = 0.1f;
    static constexpr float SpillLevel = 1.4f;
    static constexpr float FeedBed = 1.55f;
    static constexpr float SpringRate = 0.18f;  // m^3/s
    static constexpr int BasinCenterX = 15;
    static constexpr int BasinCenterY = 9;
    static constexpr float BasinRadiusX = 5.5f;
    static constexpr float BasinRadiusY = 4.5f;
    static constexpr WaterCell Vault{15, 9};
    static constexpr WaterCell Spring{27, 16};
    static constexpr WaterCell SpillOutlet{31, 9};
    static constexpr WaterCell CulvertA{6, 0};
    static constexpr WaterCell CulvertB{7, 0};
    // Water this shallow or less over the vault lets the player open it.
    static constexpr float VaultAccessDepth = 0.35f;
};

DOWNSTREAMSIM_API WaterConfig DrownedVaultConfig();
// Writes ground, sources and sinks. With prefill, the basin and ditches start
// at their flooded steady-state level so a fresh game does not need warm-up.
DOWNSTREAMSIM_API void BuildDrownedVaultSite(WaterGrid& grid, bool prefill = true);
DOWNSTREAMSIM_API float DrownedVaultBaseGround(int x, int y);

// ---- Canal designation ---------------------------------------------------
// 4-connected cells from a to b. Water cannot pass a diagonal-only gap, so a
// drawn canal must not contain one.
DOWNSTREAMSIM_API std::vector<WaterCell> RasterizeCanal(WaterCell a, WaterCell b);

struct DigRules
{
    float StepDepth = 0.25f;      // metres removed per dig action
    float MinimumBedDrop = 0.0f;  // extra depth below the graded line
    int MaxCells = 48;
};

// A canal graded from the ground height at its first cell to the ground height
// at its last. Each dig action lowers the first unfinished cell by StepDepth and
// piles the spoil on the cell to the left of travel (a berm), falling back to
// the right side; spoil with nowhere to go is counted as discarded.
class DOWNSTREAMSIM_API DigJob
{
public:
    DigJob() = default;
    DigJob(const WaterGrid& grid, std::vector<WaterCell> path, DigRules rules = {});

    bool IsValid() const noexcept { return !Path.empty(); }
    const std::vector<WaterCell>& Cells() const noexcept { return Path; }
    float TargetBed(std::size_t index) const;
    // Index of the next cell needing work, or -1 when finished.
    int NextCell(const WaterGrid& grid) const;
    bool IsComplete(const WaterGrid& grid) const { return NextCell(grid) < 0; }
    // One dig action. Returns false when nothing was left to dig.
    bool WorkNext(WaterGrid& grid);
    float Progress(const WaterGrid& grid) const;
    double SpoilPlaced() const noexcept { return Placed; }
    double SpoilDiscarded() const noexcept { return Discarded; }

private:
    bool IsPathCell(int x, int y) const;
    std::vector<WaterCell> Path;
    std::vector<float> Targets;
    std::vector<float> StartHeights;
    DigRules Rules;
    double Placed = 0.0;
    double Discarded = 0.0;
};

// ---- Persisted earthworks -------------------------------------------------
// Only ground cells that differ from the generated site are stored, in
// millimetres, so saves stay small and exact.
struct TerrainDelta
{
    std::int32_t X = 0;
    std::int32_t Y = 0;
    std::int32_t HeightMm = 0;
};

class DOWNSTREAMSIM_API EarthworksState
{
public:
    std::int32_t GetDiggerCost() const noexcept { return DiggerCost; }
    void SetDiggerCost(std::int32_t cost) noexcept { DiggerCost = cost; }
    bool IsDiggerBuilt() const noexcept { return DiggerBuilt; }
    bool IsVaultOpened() const noexcept { return VaultOpened; }
    const std::vector<TerrainDelta>& GetTerrain() const noexcept { return Terrain; }

    // Requires the workshop (checked by caller) and enough banked salvage.
    bool CanBuildDigger(std::int32_t banked) const noexcept { return !DiggerBuilt && DiggerCost > 0 && banked >= DiggerCost; }
    void MarkDiggerBuilt() noexcept { DiggerBuilt = true; }
    bool OpenVault() noexcept { if (VaultOpened) return false; VaultOpened = true; return true; }

    // Capture the difference between the grid and the generated site.
    void CaptureTerrain(const WaterGrid& grid);
    // Apply stored deltas onto a freshly built site grid.
    void ApplyTerrain(WaterGrid& grid) const;
    // Validates everything before replacing state; false leaves this unchanged.
    bool LoadPersisted(bool diggerBuilt, bool vaultOpened, const std::vector<TerrainDelta>& terrain);

private:
    std::int32_t DiggerCost = 10;
    bool DiggerBuilt = false;
    bool VaultOpened = false;
    std::vector<TerrainDelta> Terrain;
};
}
