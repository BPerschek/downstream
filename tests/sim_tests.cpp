#include "DownstreamSimulation.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
using downstream::Config;
using downstream::Simulation;

void Require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void Near(double actual, double expected, double tolerance = 1e-10)
{
    Require(std::abs(actual - expected) <= tolerance,
            "Expected " + std::to_string(expected) + ", got " + std::to_string(actual));
}

template <typename Exception, typename Action>
void Throws(Action action)
{
    try
    {
        action();
    }
    catch (const Exception&)
    {
        return;
    }
    throw std::runtime_error("Expected exception was not thrown");
}

Config SmallConfig()
{
    Config config;
    config.Width = 65;
    config.Height = 33;
    config.ChunkSize = 32;
    config.TickSeconds = 0.1;
    config.MinimumHeight = -4.0f;
    config.MaximumHeight = 7.0f;
    return config;
}

std::set<std::pair<int, int>> Dirty(Simulation& simulation)
{
    const auto chunks = simulation.ConsumeDirtyChunks();
    std::set<std::pair<int, int>> result;
    for (const auto& chunk : chunks)
        result.emplace(chunk.X, chunk.Y);
    Require(result.size() == chunks.size(), "Dirty chunks must not contain duplicates");
    return result;
}

void TickBoundariesPreservePartialTime()
{
    Simulation simulation(SmallConfig(), 7);
    simulation.Advance(0.0);
    simulation.Advance(0.025);
    Require(simulation.TickCount() == 0, "Partial tick ran early");
    Near(simulation.ElapsedSeconds(), 0.0);
    simulation.Advance(0.075);
    Require(simulation.TickCount() == 1, "Accumulated tick was lost");
    Near(simulation.ElapsedSeconds(), 0.1);
    simulation.Advance(0.299);
    Require(simulation.TickCount() == 3, "Whole ticks were not executed");
    simulation.Advance(0.001);
    Require(simulation.TickCount() == 4, "Fractional remainder was lost");
    Near(simulation.ElapsedSeconds(), 0.4);
}

void FramePartitionsProduceSameState()
{
    const Config config = SmallConfig();
    Simulation reference(config, 42);
    reference.QueueTerrainEdit(4, 5, 6.0f);
    reference.Advance(10.0);
    Require(reference.TickCount() == 100, "Ten seconds must contain 100 ticks");
    for (const int framesPerSecond : {30, 60, 144})
    {
        Simulation partitioned(config, 42);
        partitioned.QueueTerrainEdit(4, 5, 6.0f);
        for (int frame = 0; frame < framesPerSecond * 10; ++frame)
            partitioned.Advance(1.0 / framesPerSecond);
        Require(partitioned.TickCount() == reference.TickCount(), "Frame schedule changed tick count");
        Near(partitioned.ElapsedSeconds(), 10.0);
        for (int y = 0; y < config.Height; ++y)
            for (int x = 0; x < config.Width; ++x)
                Require(partitioned.HeightAt(x, y) == reference.HeightAt(x, y),
                        "Frame schedule changed terrain state");
    }

    Simulation jittered(config, 42);
    for (int batch = 0; batch < 100; ++batch)
        for (const double seconds : {0.016, 0.024, 0.037, 0.023})
            jittered.Advance(seconds);
    Require(jittered.TickCount() == 100, "Jittered frame schedule changed tick count");
}

void QueuedEditsExecuteAtTickAndInFifoOrder()
{
    Simulation simulation(SmallConfig(), 19);
    const float original = simulation.HeightAt(1, 1);
    simulation.QueueTerrainEdit(1, 1, -3.0f);
    simulation.QueueTerrainEdit(1, 1, 5.0f);
    Require(simulation.HeightAt(1, 1) == original, "Queued edit changed terrain immediately");
    simulation.Advance(0.09);
    Require(simulation.HeightAt(1, 1) == original, "Queued edit ran before tick boundary");
    simulation.Advance(0.01);
    Require(simulation.HeightAt(1, 1) == 5.0f, "Last FIFO command did not win");
    simulation.Advance(0.2);
    Require(simulation.HeightAt(1, 1) == 5.0f, "Consumed commands replayed");
}

void EditsClampToConfiguredLimits()
{
    Simulation simulation(SmallConfig(), 9);
    simulation.QueueTerrainEdit(0, 0, -500.0f);
    simulation.QueueTerrainEdit(1, 0, 500.0f);
    simulation.Advance(0.1);
    Require(simulation.HeightAt(0, 0) == -4.0f, "Minimum clamp failed");
    Require(simulation.HeightAt(1, 0) == 7.0f, "Maximum clamp failed");
}

void InvalidCoordinatesAreRejectedBeforeTick()
{
    Simulation simulation(SmallConfig(), 1);
    for (const auto& point : {std::pair<int, int>{-1, 0}, {0, -1}, {65, 0}, {0, 33}})
    {
        Throws<std::out_of_range>([&] { simulation.QueueTerrainEdit(point.first, point.second, 1.0f); });
        Throws<std::out_of_range>([&] { simulation.HeightAt(point.first, point.second); });
    }
    Require(simulation.TickCount() == 0, "Validation must not advance simulation");
}

void InvalidHeightsAreRejectedImmediately()
{
    Simulation simulation(SmallConfig(), 1);
    simulation.ConsumeDirtyChunks();
    for (const float value : {std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity(),
                              -std::numeric_limits<float>::infinity()})
        Throws<std::invalid_argument>([&] { simulation.QueueTerrainEdit(0, 0, value); });
    simulation.Advance(0.1);
    Require(Dirty(simulation).empty(), "Invalid commands altered dirty state");
}

void InvalidTimeDoesNotAlterPendingState()
{
    Simulation simulation(SmallConfig(), 6);
    simulation.QueueTerrainEdit(0, 0, 3.0f);
    for (const double value : {-0.1, std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity()})
        Throws<std::invalid_argument>([&] { simulation.Advance(value); });
    Require(simulation.TickCount() == 0, "Invalid time altered tick count");
    simulation.Advance(0.1);
    Require(simulation.HeightAt(0, 0) == 3.0f, "Invalid time consumed queued command");
}

void TickDurationIsConfigurable()
{
    Config config = SmallConfig();
    config.TickSeconds = 0.25;
    Simulation simulation(config, 6);
    simulation.Advance(0.24);
    Require(simulation.TickCount() == 0, "Configured tick ran early");
    simulation.Advance(0.01);
    Require(simulation.TickCount() == 1, "Configured tick duration was ignored");
    simulation.Advance(0.75);
    Require(simulation.TickCount() == 4, "Configured tick count is incorrect");
    Near(simulation.ElapsedSeconds(), 1.0);
}

void LargeAdvancesAreBoundedAndOverflowPreservesState()
{
    Simulation simulation(SmallConfig(), 6);
    simulation.QueueTerrainEdit(0, 0, 3.0f);
    Throws<std::overflow_error>([&] { simulation.Advance(std::numeric_limits<double>::max()); });
    Require(simulation.TickCount() == 0, "Overflowing time altered tick count");
    simulation.Advance(1000000.0);
    Require(simulation.TickCount() == 10000000, "Large advance did not aggregate ticks");
    Require(simulation.HeightAt(0, 0) == 3.0f, "Large advance lost queued edit");
    Near(simulation.ElapsedSeconds(), 1000000.0, 1e-8);
}

void InvalidConfigurationsAreRejected()
{
    const auto invalid = [](const std::function<void(Config&)>& mutate)
    {
        Config config = SmallConfig();
        mutate(config);
        Throws<std::invalid_argument>([&] { Simulation simulation(config, 1); });
    };
    invalid([](Config& c) { c.Width = 0; });
    invalid([](Config& c) { c.Height = -1; });
    invalid([](Config& c) { c.ChunkSize = 0; });
    invalid([](Config& c) { c.ChunkSize = -1; });
    invalid([](Config& c) { c.TickSeconds = 0.0; });
    invalid([](Config& c) { c.TickSeconds = -0.1; });
    invalid([](Config& c) { c.TickSeconds = std::numeric_limits<double>::quiet_NaN(); });
    invalid([](Config& c) { c.TickSeconds = std::numeric_limits<double>::infinity(); });
    invalid([](Config& c) { c.MinimumHeight = 8.0f; });
    invalid([](Config& c) { c.MinimumHeight = std::numeric_limits<float>::quiet_NaN(); });
    invalid([](Config& c) { c.MaximumHeight = std::numeric_limits<float>::infinity(); });
    invalid([](Config& c) { c.Width = std::numeric_limits<int>::max(); c.Height = std::numeric_limits<int>::max(); });
}

void InitialChunksIncludePartialEdgesAndConsumeClears()
{
    Simulation simulation(SmallConfig(), 3);
    const std::set<std::pair<int, int>> expected{{0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}};
    Require(Dirty(simulation) == expected, "Initial dirty chunks omit partial edges");
    Require(Dirty(simulation).empty(), "Consuming did not clear dirty chunks");
}

void EditsDirtyOnlyAffectedChunksWithoutDuplicates()
{
    Simulation simulation(SmallConfig(), 3);
    simulation.ConsumeDirtyChunks();
    simulation.QueueTerrainEdit(64, 32, 7.0f);
    simulation.QueueTerrainEdit(63, 31, 7.0f);
    simulation.QueueTerrainEdit(62, 30, 7.0f);
    Require(Dirty(simulation).empty(), "Queueing dirtied chunks before tick");
    simulation.Advance(0.1);
    const std::set<std::pair<int, int>> expected{{2, 1}, {1, 0}};
    Require(Dirty(simulation) == expected, "Dirty chunk coordinates are incorrect");
    Require(Dirty(simulation).empty(), "Dirty chunks repeated after consumption");
}

void NoOpEditsDoNotDirtyTerrain()
{
    Simulation simulation(SmallConfig(), 3);
    simulation.ConsumeDirtyChunks();
    simulation.QueueTerrainEdit(4, 8, simulation.HeightAt(4, 8));
    simulation.Advance(0.1);
    Require(Dirty(simulation).empty(), "No-op edit caused mesh rebuild");
    simulation.QueueTerrainEdit(4, 8, 500.0f);
    simulation.Advance(0.1);
    Require(simulation.HeightAt(4, 8) == 7.0f, "No-op fixture did not reach clamped height");
    simulation.ConsumeDirtyChunks();
    simulation.QueueTerrainEdit(4, 8, 900.0f);
    simulation.Advance(0.1);
    Require(Dirty(simulation).empty(), "Clamped no-op edit caused mesh rebuild");
}

void SeedsReproduceBoundedTerrain()
{
    const Config config = SmallConfig();
    Simulation first(config, 0x1234U);
    Simulation repeated(config, 0x1234U);
    Simulation different(config, 0x5678U);
    Require(first.Seed() == 0x1234U, "Simulation did not retain seed identity");
    bool observedDifference = false;
    bool observedSpatialDifference = false;
    for (int y = 0; y < config.Height; ++y)
        for (int x = 0; x < config.Width; ++x)
        {
            const float height = first.HeightAt(x, y);
            Require(height == repeated.HeightAt(x, y), "Same seed did not reproduce terrain");
            Require(std::isfinite(height) && height >= -4.0f && height <= 7.0f,
                    "Generated height escaped configured limits");
            observedDifference = observedDifference || height != different.HeightAt(x, y);
            observedSpatialDifference = observedSpatialDifference || height != first.HeightAt(0, 0);
        }
    Require(observedDifference, "Different seeds produced identical terrain");
    Require(observedSpatialDifference, "Generated terrain ignored coordinates");
    const float sample = downstream::SeededHeight(42, 10, 20, -4.0f, 7.0f);
    Require(sample == downstream::SeededHeight(42, 10, 20, -4.0f, 7.0f),
            "Standalone seeded height helper is not reproducible");
    Require(std::isfinite(sample) && sample >= -4.0f && sample <= 7.0f,
            "Standalone seeded height helper escaped limits");
}

void DegenerateHeightRangeAndLargeChunkRemainValid()
{
    Config config = SmallConfig();
    config.Width = 1;
    config.Height = 1;
    config.ChunkSize = 64;
    config.MinimumHeight = 2.0f;
    config.MaximumHeight = 2.0f;
    Simulation simulation(config, 4);
    Require(simulation.HeightAt(0, 0) == 2.0f, "Flat height range was not honored");
    const std::set<std::pair<int, int>> expected{{0, 0}};
    Require(Dirty(simulation) == expected, "Single cell did not produce one chunk");
    simulation.QueueTerrainEdit(0, 0, -4.0f);
    simulation.Advance(0.1);
    Require(simulation.HeightAt(0, 0) == 2.0f && Dirty(simulation).empty(),
            "Flat range edit should clamp without dirtying");
}

void SeededHelperRejectsInvalidArguments()
{
    Throws<std::invalid_argument>([] { downstream::SeededHeight(0, -1, 0, 0.0f, 1.0f); });
    Throws<std::invalid_argument>([] { downstream::SeededHeight(0, 0, -1, 0.0f, 1.0f); });
    Throws<std::invalid_argument>([] { downstream::SeededHeight(0, 0, 0, 2.0f, 1.0f); });
    Throws<std::invalid_argument>([] {
        downstream::SeededHeight(0, 0, 0, 0.0f, std::numeric_limits<float>::quiet_NaN());
    });
}

void ExtremeFiniteHeightsRemainFinite()
{
    Config config = SmallConfig();
    config.Width = 2;
    config.Height = 2;
    config.MinimumHeight = std::numeric_limits<float>::lowest();
    config.MaximumHeight = std::numeric_limits<float>::max();
    Simulation simulation(config, 55);
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x)
            Require(std::isfinite(simulation.HeightAt(x, y)),
                    "Finite configured limits overflowed generated height");
    config.MinimumHeight = std::numeric_limits<float>::max() / 2.0f;
    Simulation positiveRange(config, 55);
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x)
        {
            const float height = positiveRange.HeightAt(x, y);
            Require(std::isfinite(height) && height >= config.MinimumHeight && height <= config.MaximumHeight,
                    "Extreme finite range was not respected");
        }
}
}

int main()
{
    const std::vector<std::pair<const char*, std::function<void()>>> tests{
        {"tick boundaries preserve partial time", TickBoundariesPreservePartialTime},
        {"frame partitions produce same state", FramePartitionsProduceSameState},
        {"queued edits execute at tick in FIFO order", QueuedEditsExecuteAtTickAndInFifoOrder},
        {"edits clamp to configured limits", EditsClampToConfiguredLimits},
        {"invalid coordinates reject before tick", InvalidCoordinatesAreRejectedBeforeTick},
        {"invalid heights reject immediately", InvalidHeightsAreRejectedImmediately},
        {"invalid time preserves pending state", InvalidTimeDoesNotAlterPendingState},
        {"tick duration is configurable", TickDurationIsConfigurable},
        {"large advances are bounded and overflow preserves state", LargeAdvancesAreBoundedAndOverflowPreservesState},
        {"invalid configurations reject", InvalidConfigurationsAreRejected},
        {"initial chunks include partial edges and clear", InitialChunksIncludePartialEdgesAndConsumeClears},
        {"edits dirty only affected chunks", EditsDirtyOnlyAffectedChunksWithoutDuplicates},
        {"no-op edits do not dirty terrain", NoOpEditsDoNotDirtyTerrain},
        {"seeds reproduce bounded terrain", SeedsReproduceBoundedTerrain},
        {"flat range and large chunk remain valid", DegenerateHeightRangeAndLargeChunkRemainValid},
        {"seeded helper rejects invalid arguments", SeededHelperRejectsInvalidArguments},
        {"extreme finite heights remain finite", ExtremeFiniteHeightsRemainFinite},
    };
    int failures = 0;
    for (const auto& test : tests)
    {
        try
        {
            test.second();
            std::cout << "PASS " << test.first << '\n';
        }
        catch (const std::exception& exception)
        {
            ++failures;
            std::cerr << "FAIL " << test.first << ": " << exception.what() << '\n';
        }
    }
    std::cout << (tests.size() - static_cast<std::size_t>(failures)) << '/' << tests.size()
              << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
