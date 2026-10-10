#include "DownstreamWater.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace downstream
{
namespace
{
void Validate(const WaterConfig& c)
{
    if (c.Width <= 0 || c.Height <= 0 || c.Width > 4096 || c.Height > 4096)
        throw std::invalid_argument("Water grid dimensions must be in 1..4096");
    if (!std::isfinite(c.CellSize) || c.CellSize <= 0.0f)
        throw std::invalid_argument("Cell size must be positive");
    if (!std::isfinite(c.TickSeconds) || c.TickSeconds <= 0.0 || c.SubstepsPerTick <= 0 || c.SubstepsPerTick > 64)
        throw std::invalid_argument("Tick timing must be positive");
    if (!std::isfinite(c.Gravity) || c.Gravity <= 0.0f)
        throw std::invalid_argument("Gravity must be positive");
    if (!std::isfinite(c.FluxDamping) || c.FluxDamping <= 0.0f || c.FluxDamping > 1.0f)
        throw std::invalid_argument("Flux damping must be in (0,1]");
    if (!std::isfinite(c.MinimumGround) || !std::isfinite(c.MaximumGround) || c.MinimumGround > c.MaximumGround)
        throw std::invalid_argument("Ground limits must be finite and ordered");
    if (!std::isfinite(c.EvaporationPerSecond) || c.EvaporationPerSecond < 0.0f)
        throw std::invalid_argument("Evaporation must be nonnegative");
    if (c.ChunkSize <= 0)
        throw std::invalid_argument("Chunk size must be positive");
    // Explicit pipe model stability: g * dt^2 / dx must stay well below 0.25.
    const double dt = c.TickSeconds / c.SubstepsPerTick;
    if (c.Gravity * dt * dt / c.CellSize > 0.05)
        throw std::invalid_argument("Substep too large for stable water flow; raise SubstepsPerTick");
}

void RequireFinite(float value, const char* what)
{
    if (!std::isfinite(value)) throw std::invalid_argument(what);
}
}

WaterGrid::WaterGrid(WaterConfig config) : Settings(config)
{
    Validate(Settings);
    const auto cells = static_cast<std::size_t>(Settings.Width) * Settings.Height;
    GroundHeights.assign(cells, Settings.MinimumGround);
    Depths.assign(cells, 0.0f);
    Sources.assign(cells, 0.0f);
    Sinks.assign(cells, 0);
    Scratch.assign(cells, 0.0f);
    FluxX.assign(static_cast<std::size_t>(Settings.Width + 1) * Settings.Height, 0.0f);
    FluxY.assign(static_cast<std::size_t>(Settings.Width) * (Settings.Height + 1), 0.0f);
    ChunkColumns = (Settings.Width - 1) / Settings.ChunkSize + 1;
    ChunkRows = (Settings.Height - 1) / Settings.ChunkSize + 1;
    DirtyGround.assign(static_cast<std::size_t>(ChunkColumns) * ChunkRows, 1);
}

bool WaterGrid::InBounds(int x, int y) const noexcept
{
    return x >= 0 && y >= 0 && x < Settings.Width && y < Settings.Height;
}

void WaterGrid::CheckedIndex(int x, int y) const
{
    if (!InBounds(x, y)) throw std::out_of_range("Water cell outside grid");
}

std::size_t WaterGrid::Index(int x, int y) const
{
    CheckedIndex(x, y);
    return static_cast<std::size_t>(y) * Settings.Width + static_cast<std::size_t>(x);
}

float WaterGrid::Ground(int x, int y) const { return GroundHeights[Index(x, y)]; }
float WaterGrid::Water(int x, int y) const { return Depths[Index(x, y)]; }
float WaterGrid::Surface(int x, int y) const { const auto i = Index(x, y); return GroundHeights[i] + Depths[i]; }

void WaterGrid::MarkGroundDirty(int x, int y)
{
    ++GroundRevisionCounter;
    // A vertex shared with a neighbouring chunk changes that chunk's mesh too.
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
        {
            const int nx = x + dx, ny = y + dy;
            if (!InBounds(nx, ny)) continue;
            DirtyGround[static_cast<std::size_t>(ny / Settings.ChunkSize) * ChunkColumns + nx / Settings.ChunkSize] = 1;
        }
}

void WaterGrid::SetGround(int x, int y, float height)
{
    RequireFinite(height, "Ground height must be finite");
    const auto i = Index(x, y);
    const float clamped = std::clamp(height, Settings.MinimumGround, Settings.MaximumGround);
    if (GroundHeights[i] == clamped) return;
    GroundHeights[i] = clamped;
    MarkGroundDirty(x, y);
}

void WaterGrid::SetWater(int x, int y, float depth)
{
    RequireFinite(depth, "Water depth must be finite");
    if (depth < 0.0f) throw std::invalid_argument("Water depth must be nonnegative");
    const auto i = Index(x, y);
    const double area = static_cast<double>(Settings.CellSize) * Settings.CellSize;
    Added += (static_cast<double>(depth) - Depths[i]) * area;
    Depths[i] = depth;
}

double WaterGrid::LowerGround(int x, int y, float amount, float floor)
{
    RequireFinite(amount, "Dig amount must be finite");
    RequireFinite(floor, "Dig floor must be finite");
    if (amount < 0.0f) throw std::invalid_argument("Dig amount must be nonnegative");
    const auto i = Index(x, y);
    const float target = std::max({GroundHeights[i] - amount, floor, Settings.MinimumGround});
    if (target >= GroundHeights[i]) return 0.0;
    const double moved = (static_cast<double>(GroundHeights[i]) - target) * Settings.CellSize * Settings.CellSize;
    GroundHeights[i] = target;
    MarkGroundDirty(x, y);
    return moved;
}

double WaterGrid::RaiseGround(int x, int y, float amount, float ceiling)
{
    RequireFinite(amount, "Fill amount must be finite");
    RequireFinite(ceiling, "Fill ceiling must be finite");
    if (amount < 0.0f) throw std::invalid_argument("Fill amount must be nonnegative");
    const auto i = Index(x, y);
    const float target = std::min({GroundHeights[i] + amount, ceiling, Settings.MaximumGround});
    if (target <= GroundHeights[i]) return 0.0;
    const double moved = (static_cast<double>(target) - GroundHeights[i]) * Settings.CellSize * Settings.CellSize;
    // Raising ground under water displaces it upward; volume stays the same.
    GroundHeights[i] = target;
    MarkGroundDirty(x, y);
    return moved;
}

void WaterGrid::SetSource(int x, int y, float rate)
{
    RequireFinite(rate, "Source rate must be finite");
    if (rate < 0.0f) throw std::invalid_argument("Source rate must be nonnegative");
    Sources[Index(x, y)] = rate;
}

float WaterGrid::Source(int x, int y) const { return Sources[Index(x, y)]; }
void WaterGrid::SetSink(int x, int y, bool sink) { Sinks[Index(x, y)] = sink ? 1 : 0; }
bool WaterGrid::IsSink(int x, int y) const { return Sinks[Index(x, y)] != 0; }

FlowVector WaterGrid::Flow(int x, int y) const
{
    const auto i = Index(x, y);
    const int W = Settings.Width;
    const float depth = Depths[i];
    if (depth < 1e-4f) return {};
    const float left = FluxX[static_cast<std::size_t>(y) * (W + 1) + x];
    const float right = FluxX[static_cast<std::size_t>(y) * (W + 1) + x + 1];
    const float down = FluxY[static_cast<std::size_t>(y) * W + x];
    const float up = FluxY[static_cast<std::size_t>(y + 1) * W + x];
    const float crossSection = depth * Settings.CellSize;
    return {0.5f * (left + right) / crossSection, 0.5f * (down + up) / crossSection};
}

int WaterGrid::Advance(double seconds)
{
    if (!std::isfinite(seconds) || seconds < 0.0)
        throw std::invalid_argument("Elapsed time must be finite and nonnegative");
    Accumulated += seconds;
    int ran = 0;
    // Small tolerance so 0.1 + 0.1 + ... does not lose a tick to rounding.
    while (Accumulated + 1e-9 >= Settings.TickSeconds && ran < MaxTicksPerAdvance)
    {
        Tick();
        Accumulated -= Settings.TickSeconds;
        ++ran;
    }
    if (ran == MaxTicksPerAdvance) Accumulated = std::min(Accumulated, Settings.TickSeconds);
    Accumulated = std::max(0.0, Accumulated);
    return ran;
}

void WaterGrid::Tick()
{
    const float dt = static_cast<float>(Settings.TickSeconds / Settings.SubstepsPerTick);
    for (int s = 0; s < Settings.SubstepsPerTick; ++s) Substep(dt);
    ++Ticks;
}

void WaterGrid::Substep(float dt)
{
    const int W = Settings.Width;
    const int H = Settings.Height;
    const float area = Settings.CellSize * Settings.CellSize;
    // Pipe cross-section = cell area, pipe length = cell size.
    const float k = dt * Settings.Gravity * area / Settings.CellSize;
    const float damping = Settings.FluxDamping;
    const bool drain = Settings.DrainAtEdges;

    // 1. Sources.
    for (std::size_t i = 0; i < Depths.size(); ++i)
        if (Sources[i] > 0.0f)
        {
            const float added = Sources[i] * dt;
            Depths[i] += added / area;
            Sourced += added;
        }

    auto surface = [&](int x, int y) { const auto i = static_cast<std::size_t>(y) * W + x; return GroundHeights[i] + Depths[i]; };
    auto depthAt = [&](int x, int y) { return Depths[static_cast<std::size_t>(y) * W + x]; };

    // 2. Accelerate edge fluxes by surface difference.
    for (int y = 0; y < H; ++y)
        for (int e = 0; e <= W; ++e)
        {
            float& f = FluxX[static_cast<std::size_t>(y) * (W + 1) + e];
            if (e == 0 || e == W)
            {
                if (!drain) { f = 0.0f; continue; }
                // Outside is "ground level of the edge cell with no water".
                const int x = e == 0 ? 0 : W - 1;
                const float head = depthAt(x, y);
                const float sign = e == 0 ? -1.0f : 1.0f; // positive flux = +X direction
                float out = std::max(0.0f, sign * f * damping + k * head);
                f = sign * out;
                continue;
            }
            const float a = depthAt(e - 1, y), b = depthAt(e, y);
            if (a <= 0.0f && b <= 0.0f) { f = 0.0f; continue; }
            f = f * damping + k * (surface(e - 1, y) - surface(e, y));
            // Water cannot flow out of a dry cell.
            if (f > 0.0f && a <= 0.0f) f = 0.0f;
            if (f < 0.0f && b <= 0.0f) f = 0.0f;
        }
    for (int e = 0; e <= H; ++e)
        for (int x = 0; x < W; ++x)
        {
            float& f = FluxY[static_cast<std::size_t>(e) * W + x];
            if (e == 0 || e == H)
            {
                if (!drain) { f = 0.0f; continue; }
                const int y = e == 0 ? 0 : H - 1;
                const float head = depthAt(x, y);
                const float sign = e == 0 ? -1.0f : 1.0f;
                float out = std::max(0.0f, sign * f * damping + k * head);
                f = sign * out;
                continue;
            }
            const float a = depthAt(x, e - 1), b = depthAt(x, e);
            if (a <= 0.0f && b <= 0.0f) { f = 0.0f; continue; }
            f = f * damping + k * (surface(x, e - 1) - surface(x, e));
            if (f > 0.0f && a <= 0.0f) f = 0.0f;
            if (f < 0.0f && b <= 0.0f) f = 0.0f;
        }

    // 3. Limit each cell's total outflow to the water it actually holds.
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
        {
            float& left = FluxX[static_cast<std::size_t>(y) * (W + 1) + x];
            float& right = FluxX[static_cast<std::size_t>(y) * (W + 1) + x + 1];
            float& down = FluxY[static_cast<std::size_t>(y) * W + x];
            float& up = FluxY[static_cast<std::size_t>(y + 1) * W + x];
            const float out = std::max(0.0f, -left) + std::max(0.0f, right) + std::max(0.0f, -down) + std::max(0.0f, up);
            if (out <= 0.0f) { Scratch[static_cast<std::size_t>(y) * W + x] = 1.0f; continue; }
            const float available = depthAt(x, y) * area / dt;
            Scratch[static_cast<std::size_t>(y) * W + x] = out > available ? available / out : 1.0f;
        }
    for (int y = 0; y < H; ++y)
        for (int e = 0; e <= W; ++e)
        {
            float& f = FluxX[static_cast<std::size_t>(y) * (W + 1) + e];
            if (f > 0.0f && e > 0) f *= Scratch[static_cast<std::size_t>(y) * W + e - 1];
            else if (f < 0.0f && e < W) f *= Scratch[static_cast<std::size_t>(y) * W + e];
        }
    for (int e = 0; e <= H; ++e)
        for (int x = 0; x < W; ++x)
        {
            float& f = FluxY[static_cast<std::size_t>(e) * W + x];
            if (f > 0.0f && e > 0) f *= Scratch[static_cast<std::size_t>(e - 1) * W + x];
            else if (f < 0.0f && e < H) f *= Scratch[static_cast<std::size_t>(e) * W + x];
        }

    // 4. Apply net flux. Boundary edges carry only outflow.
    for (int y = 0; y < H; ++y)
    {
        Drained += static_cast<double>(std::max(0.0f, -FluxX[static_cast<std::size_t>(y) * (W + 1)])) * dt;
        Drained += static_cast<double>(std::max(0.0f, FluxX[static_cast<std::size_t>(y) * (W + 1) + W])) * dt;
    }
    for (int x = 0; x < W; ++x)
    {
        Drained += static_cast<double>(std::max(0.0f, -FluxY[static_cast<std::size_t>(x)])) * dt;
        Drained += static_cast<double>(std::max(0.0f, FluxY[static_cast<std::size_t>(H) * W + x])) * dt;
    }
    const float evaporation = Settings.EvaporationPerSecond * dt;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
        {
            const auto i = static_cast<std::size_t>(y) * W + x;
            const float net = FluxX[static_cast<std::size_t>(y) * (W + 1) + x]
                            - FluxX[static_cast<std::size_t>(y) * (W + 1) + x + 1]
                            + FluxY[static_cast<std::size_t>(y) * W + x]
                            - FluxY[static_cast<std::size_t>(y + 1) * W + x];
            float depth = Depths[i] + net * dt / area;
            if (depth < 0.0f)
            {
                // Float rounding after limiting; count it so accounting stays exact.
                Added -= static_cast<double>(depth) * area;
                depth = 0.0f;
            }
            if (Sinks[i])
            {
                Sunk += static_cast<double>(depth) * area;
                depth = 0.0f;
            }
            else if (depth > 0.0f && depth < 1e-6f)
            {
                // Film thinner than a micron: count it as evaporated so floats never go denormal.
                Evaporated += static_cast<double>(depth) * area;
                depth = 0.0f;
            }
            else if (evaporation > 0.0f && depth > 0.0f)
            {
                const float lost = std::min(depth, evaporation);
                Evaporated += static_cast<double>(lost) * area;
                depth -= lost;
            }
            Depths[i] = depth;
        }
}

double WaterGrid::TotalWater() const
{
    double total = 0.0;
    for (float d : Depths) total += d;
    return total * Settings.CellSize * Settings.CellSize;
}

float WaterGrid::MaxDepth() const
{
    return Depths.empty() ? 0.0f : *std::max_element(Depths.begin(), Depths.end());
}

std::vector<WaterCell> WaterGrid::ConsumeDirtyGroundChunks()
{
    std::vector<WaterCell> result;
    for (int y = 0; y < ChunkRows; ++y)
        for (int x = 0; x < ChunkColumns; ++x)
            if (DirtyGround[static_cast<std::size_t>(y) * ChunkColumns + x]) result.push_back({x, y});
    std::fill(DirtyGround.begin(), DirtyGround.end(), 0);
    return result;
}
}
