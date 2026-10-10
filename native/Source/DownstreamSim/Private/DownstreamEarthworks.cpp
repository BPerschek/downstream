#include "DownstreamEarthworks.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace downstream
{
namespace
{
using Site = DrownedVaultSite;

bool IsFeedDitch(int x, int y)
{
    // Burst main at (27,16) runs west along y=16, then south down x=15 into the basin.
    if (y == 16 && x >= Site::BasinCenterX && x <= Site::Spring.X) return true;
    if (x == Site::BasinCenterX && y >= 13 && y <= 16) return true;
    return false;
}

bool IsSpillway(int x, int y)
{
    return y == Site::BasinCenterY && x >= 19 && x < Site::Width;
}

bool IsCulvertApron(int x, int y)
{
    return y <= 1 && x >= Site::CulvertA.X - 1 && x <= Site::CulvertB.X + 1;
}
}

WaterConfig DrownedVaultConfig()
{
    WaterConfig config;
    config.Width = Site::Width;
    config.Height = Site::Height;
    config.CellSize = Site::CellSize;
    config.TickSeconds = 0.1;
    config.SubstepsPerTick = 4;
    config.MinimumGround = 0.0f;
    config.MaximumGround = 4.0f;
    config.FluxDamping = 0.985f;
    config.ChunkSize = 16;
    return config;
}

float DrownedVaultBaseGround(int x, int y)
{
    if (x < 0 || y < 0 || x >= Site::Width || y >= Site::Height)
        throw std::out_of_range("Site cell outside grid");
    float h = Site::Plateau;
    // Walkable ramps down to the surrounding flat ground on the west and north.
    const float west = x == 0 ? 0.3f : x == 1 ? 0.8f : x == 2 ? 1.3f : Site::Plateau;
    const float north = y == Site::Height - 1 ? 0.3f : y == Site::Height - 2 ? 0.8f : y == Site::Height - 3 ? 1.3f : Site::Plateau;
    h = std::min({h, west, north});
    if (IsCulvertApron(x, y)) h = std::min(h, y == 0 ? 0.6f : 1.2f);
    // The culvert mouth itself is a pit at grade with the surrounding flat ground.
    if (y == Site::CulvertA.Y && (x == Site::CulvertA.X || x == Site::CulvertB.X)) h = 0.0f;
    if (IsFeedDitch(x, y)) h = std::min(h, Site::FeedBed);
    if (IsSpillway(x, y)) h = std::min(h, Site::SpillLevel);
    // Smooth bowl: gentle enough (about 33 degrees at the rim) to walk once drained.
    const float dx = (x - Site::BasinCenterX) / Site::BasinRadiusX;
    const float dy = (y - Site::BasinCenterY) / Site::BasinRadiusY;
    const float r2 = dx * dx + dy * dy;
    if (r2 < 1.0f) h = std::min(h, Site::BasinFloor + (Site::Plateau - 0.1f - Site::BasinFloor) * r2);
    return h;
}

void BuildDrownedVaultSite(WaterGrid& grid, bool prefill)
{
    if (grid.Width() != Site::Width || grid.Height() != Site::Height)
        throw std::invalid_argument("Grid does not match the Drowned Vault site");
    for (int y = 0; y < Site::Height; ++y)
        for (int x = 0; x < Site::Width; ++x)
        {
            grid.SetGround(x, y, DrownedVaultBaseGround(x, y));
            grid.SetSource(x, y, 0.0f);
            grid.SetSink(x, y, false);
        }
    grid.SetSource(Site::Spring.X, Site::Spring.Y, Site::SpringRate);
    grid.SetSink(Site::SpillOutlet.X, Site::SpillOutlet.Y, true);
    grid.SetSink(Site::CulvertA.X, Site::CulvertA.Y, true);
    grid.SetSink(Site::CulvertB.X, Site::CulvertB.Y, true);
    if (!prefill) return;
    for (int y = 0; y < Site::Height; ++y)
        for (int x = 0; x < Site::Width; ++x)
        {
            const float dx = (x - Site::BasinCenterX) / Site::BasinRadiusX;
            const float dy = (y - Site::BasinCenterY) / Site::BasinRadiusY;
            if (dx * dx + dy * dy < 1.2f && !grid.IsSink(x, y))
                grid.SetWater(x, y, std::max(0.0f, Site::SpillLevel - grid.Ground(x, y)));
        }
}

std::vector<WaterCell> RasterizeCanal(WaterCell a, WaterCell b)
{
    std::vector<WaterCell> cells;
    int x = a.X, y = a.Y;
    const int dx = std::abs(b.X - a.X), dy = -std::abs(b.Y - a.Y);
    const int sx = a.X < b.X ? 1 : -1, sy = a.Y < b.Y ? 1 : -1;
    int err = dx + dy;
    cells.push_back({x, y});
    while (x != b.X || y != b.Y)
    {
        const int e2 = 2 * err;
        // Take one axis step at a time so consecutive cells always share an edge.
        if (e2 >= dy && x != b.X) { err += dy; x += sx; }
        else { err += dx; y += sy; }
        cells.push_back({x, y});
    }
    return cells;
}

DigJob::DigJob(const WaterGrid& grid, std::vector<WaterCell> path, DigRules rules)
    : Path(std::move(path)), Rules(rules)
{
    if (!std::isfinite(Rules.StepDepth) || Rules.StepDepth <= 0.0f || !std::isfinite(Rules.MinimumBedDrop) || Rules.MinimumBedDrop < 0.0f)
        throw std::invalid_argument("Dig rules must be positive and finite");
    if (Path.empty() || static_cast<int>(Path.size()) > Rules.MaxCells)
        throw std::invalid_argument("Canal path length is outside the allowed range");
    for (std::size_t i = 0; i < Path.size(); ++i)
    {
        if (!grid.InBounds(Path[i].X, Path[i].Y)) throw std::out_of_range("Canal cell outside site");
        if (i > 0 && std::abs(Path[i].X - Path[i - 1].X) + std::abs(Path[i].Y - Path[i - 1].Y) != 1)
            throw std::invalid_argument("Canal cells must be edge-connected");
    }
    const float start = grid.Ground(Path.front().X, Path.front().Y);
    const float end = grid.Ground(Path.back().X, Path.back().Y);
    const float minimum = grid.Config().MinimumGround;
    for (std::size_t i = 0; i < Path.size(); ++i)
    {
        const float t = Path.size() == 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(Path.size() - 1);
        const float graded = start + (end - start) * t - Rules.MinimumBedDrop;
        const float current = grid.Ground(Path[i].X, Path[i].Y);
        Targets.push_back(std::max(minimum, std::min(current, graded)));
        StartHeights.push_back(current);
    }
}

float DigJob::TargetBed(std::size_t index) const
{
    if (index >= Targets.size()) throw std::out_of_range("Canal index");
    return Targets[index];
}

int DigJob::NextCell(const WaterGrid& grid) const
{
    for (std::size_t i = 0; i < Path.size(); ++i)
        if (grid.Ground(Path[i].X, Path[i].Y) > Targets[i] + 1e-4f) return static_cast<int>(i);
    return -1;
}

bool DigJob::IsPathCell(int x, int y) const
{
    return std::any_of(Path.begin(), Path.end(), [x, y](const WaterCell& c) { return c.X == x && c.Y == y; });
}

bool DigJob::WorkNext(WaterGrid& grid)
{
    const int index = NextCell(grid);
    if (index < 0) return false;
    const auto cell = Path[static_cast<std::size_t>(index)];
    const double moved = grid.LowerGround(cell.X, cell.Y, Rules.StepDepth, Targets[static_cast<std::size_t>(index)]);
    if (moved <= 0.0) return false;
    // Direction of travel at this cell decides which side gets the berm.
    const auto& from = Path[static_cast<std::size_t>(index > 0 ? index - 1 : index)];
    const auto& to = Path[static_cast<std::size_t>(index + 1 < static_cast<int>(Path.size()) ? index + 1 : index)];
    int dirX = to.X - from.X, dirY = to.Y - from.Y;
    if (dirX == 0 && dirY == 0) dirX = 1;
    dirX = dirX > 0 ? 1 : dirX < 0 ? -1 : 0;
    dirY = dirY > 0 ? 1 : dirY < 0 ? -1 : 0;
    const float area = grid.Config().CellSize * grid.Config().CellSize;
    double remaining = moved;
    for (int side : {1, -1})
    {
        // Left of (dirX,dirY) is (-dirY,dirX).
        const int sx = cell.X - dirY * side, sy = cell.Y + dirX * side;
        if (!grid.InBounds(sx, sy) || IsPathCell(sx, sy) || grid.IsSink(sx, sy)) continue;
        const double placed = grid.RaiseGround(sx, sy, static_cast<float>(remaining / area), grid.Config().MaximumGround);
        Placed += placed;
        remaining -= placed;
        if (remaining <= 1e-9) break;
    }
    if (remaining > 1e-9) Discarded += remaining;
    return true;
}

float DigJob::Progress(const WaterGrid& grid) const
{
    double total = 0.0, done = 0.0;
    for (std::size_t i = 0; i < Path.size(); ++i)
    {
        const double needed = std::max(0.0f, StartHeights[i] - Targets[i]);
        total += needed;
        done += std::clamp<double>(StartHeights[i] - grid.Ground(Path[i].X, Path[i].Y), 0.0, needed);
    }
    return total <= 0.0 ? 1.0f : static_cast<float>(done / total);
}

bool EarthworksState::IsVaultOpened(int id) const
{
    return std::find(OpenedVaults.begin(), OpenedVaults.end(), id) != OpenedVaults.end();
}

bool EarthworksState::OpenVault(int id)
{
    if (id < 0 || IsVaultOpened(id)) return false;
    OpenedVaults.push_back(id);
    return true;
}

int EarthworksState::GetOutpostMask(int id) const
{
    for (const auto& m : OutpostMasks) if (m.first == id) return m.second;
    return 0;
}

void EarthworksState::SetOutpostMask(int id, int mask)
{
    for (auto& m : OutpostMasks) if (m.first == id) { m.second = mask; return; }
    OutpostMasks.push_back({id, mask});
}

bool EarthworksState::LoadWorld(std::uint32_t seed, const std::vector<int>& openedVaults, const std::vector<std::pair<int, int>>& outpostMasks)
{
    std::vector<int> vaults;
    for (int v : openedVaults)
    {
        if (v < 0 || v > 64 || std::find(vaults.begin(), vaults.end(), v) != vaults.end()) return false;
        vaults.push_back(v);
    }
    std::vector<std::pair<int, int>> masks;
    for (const auto& m : outpostMasks)
    {
        if (m.first < 0 || m.first > 64 || m.second < 0 || m.second >= (1 << 20)) return false;
        for (const auto& p : masks) if (p.first == m.first) return false;
        masks.push_back(m);
    }
    Seed = seed;
    OpenedVaults = std::move(vaults);
    OutpostMasks = std::move(masks);
    return true;
}

void EarthworksState::CaptureTerrain(const WaterGrid& grid, const std::vector<float>& baseGround)
{
    if (baseGround.size() != static_cast<std::size_t>(grid.Width()) * grid.Height())
        throw std::invalid_argument("Base ground does not match grid");
    std::vector<TerrainDelta> result;
    for (int y = 0; y < grid.Height(); ++y)
        for (int x = 0; x < grid.Width(); ++x)
        {
            const auto base = static_cast<std::int32_t>(std::lround(baseGround[static_cast<std::size_t>(y) * grid.Width() + x] * 1000.0f));
            const auto now = static_cast<std::int32_t>(std::lround(grid.Ground(x, y) * 1000.0f));
            if (base != now) result.push_back({x, y, now});
        }
    Terrain = std::move(result);
}

void EarthworksState::CaptureTerrain(const WaterGrid& grid)
{
    std::vector<TerrainDelta> result;
    for (int y = 0; y < grid.Height(); ++y)
        for (int x = 0; x < grid.Width(); ++x)
        {
            const auto base = static_cast<std::int32_t>(std::lround(DrownedVaultBaseGround(x, y) * 1000.0f));
            const auto now = static_cast<std::int32_t>(std::lround(grid.Ground(x, y) * 1000.0f));
            if (base != now) result.push_back({x, y, now});
        }
    Terrain = std::move(result);
}

void EarthworksState::ApplyTerrain(WaterGrid& grid) const
{
    for (const auto& d : Terrain)
    {
        const int x = static_cast<int>(d.X), y = static_cast<int>(d.Y);
        if (!grid.InBounds(x, y)) continue;
        // Water above an edited cell stays put and re-settles on the next ticks.
        grid.SetGround(x, y, static_cast<float>(d.HeightMm) / 1000.0f);
    }
}

bool EarthworksState::LoadPersisted(bool diggerBuilt, bool vaultOpened, const std::vector<TerrainDelta>& terrain)
{
    // Bounds are checked again when deltas are applied to a concrete site grid.
    std::vector<TerrainDelta> validated;
    for (const auto& d : terrain)
    {
        if (d.X < 0 || d.Y < 0 || d.X >= 4096 || d.Y >= 4096) return false;
        if (d.HeightMm < 0 || d.HeightMm > 20000) return false;
        for (const auto& p : validated) if (p.X == d.X && p.Y == d.Y) return false;
        validated.push_back(d);
    }
    if (validated.size() > 4096U * 64U) return false;
    DiggerBuilt = diggerBuilt;
    OpenedVaults.clear();
    if (vaultOpened) OpenedVaults.push_back(0);
    Terrain = std::move(validated);
    return true;
}
}
