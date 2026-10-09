#include "DownstreamSimulation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace downstream
{
namespace
{
void ValidateHeightRange(float minimumHeight, float maximumHeight)
{
    if (!std::isfinite(minimumHeight) || !std::isfinite(maximumHeight) || minimumHeight > maximumHeight)
        throw std::invalid_argument("Height limits must be finite and ordered");
}

std::size_t ValidateConfig(const Config& config)
{
    if (config.Width <= 0 || config.Height <= 0 || config.ChunkSize <= 0)
        throw std::invalid_argument("Grid dimensions and chunk size must be positive");
    if (!std::isfinite(config.TickSeconds) || config.TickSeconds <= 0.0)
        throw std::invalid_argument("Tick duration must be positive and finite");
    ValidateHeightRange(config.MinimumHeight, config.MaximumHeight);

    const auto width = static_cast<std::size_t>(config.Width);
    const auto height = static_cast<std::size_t>(config.Height);
    // Some standard libraries bound max_size in elements rather than total
    // signed-addressable bytes; reject impossible giant buffers before allocation.
    const auto maximumCells = std::min(std::vector<float>{}.max_size(),
        static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) / sizeof(float));
    if (width > maximumCells / height)
        throw std::invalid_argument("Grid dimensions exceed addressable terrain storage");
    return width * height;
}
}

float SeededHeight(std::uint32_t seed, int x, int y, float minimumHeight, float maximumHeight)
{
    if (x < 0 || y < 0)
        throw std::invalid_argument("Generator coordinates must be nonnegative");
    ValidateHeightRange(minimumHeight, maximumHeight);

    // Unsigned arithmetic defines wrapping on every platform; no global RNG is used.
    std::uint32_t value = seed ^ (static_cast<std::uint32_t>(x) * 0x9e3779b9U)
                               ^ (static_cast<std::uint32_t>(y) * 0x85ebca6bU);
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    const double fraction = static_cast<double>(value >> 8U) / 16777216.0;
    // Compute the range in double so opposite finite float extremes cannot overflow.
    const double height = static_cast<double>(minimumHeight)
                        + (static_cast<double>(maximumHeight) - minimumHeight) * fraction;
    return std::clamp(static_cast<float>(height), minimumHeight, maximumHeight);
}

Simulation::Simulation(Config config, std::uint32_t seed) : Settings(config), WorldSeed(seed)
{
    const std::size_t cells = ValidateConfig(Settings);
    ChunkColumns = (Settings.Width - 1) / Settings.ChunkSize + 1;
    ChunkRows = (Settings.Height - 1) / Settings.ChunkSize + 1;
    Heights.resize(cells);
    DirtyChunks.assign(static_cast<std::size_t>(ChunkColumns) * ChunkRows, 1);
    for (int y = 0; y < Settings.Height; ++y)
        for (int x = 0; x < Settings.Width; ++x)
            Heights[CellIndex(x, y)] = SeededHeight(WorldSeed, x, y,
                                                   Settings.MinimumHeight, Settings.MaximumHeight);
}

void Simulation::Advance(double seconds)
{
    if (!std::isfinite(seconds) || seconds < 0.0)
        throw std::invalid_argument("Elapsed frame time must be finite and nonnegative");

    const long double tickSeconds = Settings.TickSeconds;
    const long double total = AccumulatedSeconds + static_cast<long double>(seconds);
    // Account for the input double's rounding near a boundary; it must not lose a
    // tick solely because 0.1 or a 1/144-second frame is not exactly representable.
    const long double tolerance = std::max(total, tickSeconds)
                                * 8.0L * std::numeric_limits<double>::epsilon();
    const long double completeTicks = std::floor((total + tolerance) / tickSeconds);
    if (!std::isfinite(completeTicks)
        || completeTicks >= static_cast<long double>(std::numeric_limits<std::uint64_t>::max()))
        throw std::overflow_error("Elapsed frame time exceeds the tick counter range");
    const auto dueTicks = static_cast<std::uint64_t>(completeTicks);
    if (dueTicks > std::numeric_limits<std::uint64_t>::max() - Ticks)
        throw std::overflow_error("Simulation tick counter would overflow");

    if (dueTicks == 0)
    {
        AccumulatedSeconds = total;
        return;
    }

    // M0 has no recurring per-tick systems. Apply the FIFO queue at the first
    // boundary and aggregate empty ticks, keeping large advances bounded.
    for (const auto& edit : PendingEdits)
    {
        const std::size_t index = CellIndex(edit.X, edit.Y);
        const float height = std::clamp(edit.Height, Settings.MinimumHeight, Settings.MaximumHeight);
        if (Heights[index] != height)
        {
            Heights[index] = height;
            const auto chunkX = static_cast<std::size_t>(edit.X / Settings.ChunkSize);
            const auto chunkY = static_cast<std::size_t>(edit.Y / Settings.ChunkSize);
            DirtyChunks[chunkY * ChunkColumns + chunkX] = 1;
        }
    }
    PendingEdits.clear();
    Ticks += dueTicks;
    AccumulatedSeconds = std::max(0.0L, total - static_cast<long double>(dueTicks) * tickSeconds);
}

void Simulation::QueueTerrainEdit(int x, int y, float height)
{
    CellIndex(x, y);
    if (!std::isfinite(height))
        throw std::invalid_argument("Terrain target height must be finite");
    PendingEdits.push_back({x, y, height});
}

std::uint64_t Simulation::TickCount() const noexcept
{
    return Ticks;
}

double Simulation::ElapsedSeconds() const noexcept
{
    return static_cast<double>(Ticks) * Settings.TickSeconds;
}

float Simulation::HeightAt(int x, int y) const
{
    return Heights[CellIndex(x, y)];
}

std::vector<ChunkCoord> Simulation::ConsumeDirtyChunks()
{
    std::vector<ChunkCoord> result;
    for (int y = 0; y < ChunkRows; ++y)
        for (int x = 0; x < ChunkColumns; ++x)
            if (DirtyChunks[static_cast<std::size_t>(y) * ChunkColumns + x] != 0)
                result.push_back({x, y});
    // Clear only after building the result, preserving dirtiness if allocation fails.
    std::fill(DirtyChunks.begin(), DirtyChunks.end(), 0);
    return result;
}

std::uint32_t Simulation::Seed() const noexcept
{
    return WorldSeed;
}

std::size_t Simulation::CellIndex(int x, int y) const
{
    if (x < 0 || y < 0 || x >= Settings.Width || y >= Settings.Height)
        throw std::out_of_range("Terrain coordinates are outside the simulation grid");
    return static_cast<std::size_t>(y) * Settings.Width + static_cast<std::size_t>(x);
}
}
