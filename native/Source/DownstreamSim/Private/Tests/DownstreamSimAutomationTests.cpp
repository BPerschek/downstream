#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DownstreamSimulation.h"

#include <cmath>
#include <initializer_list>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace
{
using downstream::Config;
using downstream::Simulation;
using ChunkSet = std::set<std::pair<int, int>>;

Config SmallConfig()
{
    Config Settings;
    Settings.Width = 65;
    Settings.Height = 33;
    Settings.ChunkSize = 32;
    Settings.TickSeconds = 0.1;
    Settings.MinimumHeight = -4.0f;
    Settings.MaximumHeight = 7.0f;
    return Settings;
}

template <typename ExpectedException, typename Action>
bool TestThrows(FAutomationTestBase& Test, const TCHAR* What, Action Run)
{
    try
    {
        Run();
    }
    catch (const ExpectedException&)
    {
        return true;
    }
    catch (...)
    {
        Test.AddError(FString::Printf(TEXT("%s: unexpected exception type"), What));
        return false;
    }
    Test.AddError(FString::Printf(TEXT("%s: expected exception was not thrown"), What));
    return false;
}

bool TestDirtyChunks(FAutomationTestBase& Test, const TCHAR* What, Simulation& Sim,
                     std::initializer_list<std::pair<int, int>> Expected)
{
    const auto Chunks = Sim.ConsumeDirtyChunks();
    ChunkSet Actual;
    for (const auto& Chunk : Chunks)
        Actual.emplace(Chunk.X, Chunk.Y);

    const bool Unique = Test.TestEqual(FString::Printf(TEXT("%s: no duplicate chunks"), What),
                                      static_cast<int32>(Chunks.size()),
                                      static_cast<int32>(Actual.size()));
    const bool Match = Test.TestTrue(What, Actual == ChunkSet(Expected));
    return Unique && Match;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDownstreamFixedTickPartitionsTest,
    "Downstream.Simulation.FixedTickPartitions",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDownstreamFixedTickPartitionsTest::RunTest(const FString& Parameters)
{
    bool Passed = true;
    Simulation Boundary(SmallConfig(), 7);
    Boundary.Advance(0.0);
    Boundary.Advance(0.025);
    Passed &= TestEqual(TEXT("Partial time does not run a tick"), Boundary.TickCount(), std::uint64_t{0});
    Passed &= TestEqual(TEXT("Partial time is excluded from elapsed simulation time"),
                        Boundary.ElapsedSeconds(), 0.0, 1e-10);
    Boundary.Advance(0.075);
    Passed &= TestEqual(TEXT("Partial frames combine into the first tick"), Boundary.TickCount(), std::uint64_t{1});
    Boundary.Advance(0.299);
    Passed &= TestEqual(TEXT("Only complete ticks advance"), Boundary.TickCount(), std::uint64_t{3});
    Boundary.Advance(0.001);
    Passed &= TestEqual(TEXT("The fractional remainder survives across frames"), Boundary.TickCount(), std::uint64_t{4});
    Passed &= TestEqual(TEXT("Four default ticks contain 0.4 seconds"), Boundary.ElapsedSeconds(), 0.4, 1e-10);

    const Config Settings = SmallConfig();
    Simulation Reference(Settings, 42);
    Reference.QueueTerrainEdit(4, 5, 6.0f);
    Reference.Advance(10.0);
    Passed &= TestEqual(TEXT("Ten seconds contain 100 default ticks"), Reference.TickCount(), std::uint64_t{100});
    Passed &= TestEqual(TEXT("The reference applies the queued target"), Reference.HeightAt(4, 5), 6.0f, 0.0f);

    // Losing the accumulated remainder or stepping once per frame breaks these schedules.
    for (const int FramesPerSecond : {30, 60, 144})
    {
        Simulation Partitioned(Settings, 42);
        Partitioned.QueueTerrainEdit(4, 5, 6.0f);
        for (int Frame = 0; Frame < FramesPerSecond * 10; ++Frame)
            Partitioned.Advance(1.0 / FramesPerSecond);

        const FString Schedule = FString::Printf(TEXT("%d FPS"), FramesPerSecond);
        Passed &= TestEqual(*(Schedule + TEXT(": 100 ticks")), Partitioned.TickCount(), std::uint64_t{100});
        Passed &= TestEqual(Schedule + TEXT(": ten completed seconds"), Partitioned.ElapsedSeconds(), 10.0, 1e-10);
        int32 DifferentCells = 0;
        for (int Y = 0; Y < Settings.Height; ++Y)
            for (int X = 0; X < Settings.Width; ++X)
                DifferentCells += Partitioned.HeightAt(X, Y) != Reference.HeightAt(X, Y) ? 1 : 0;
        Passed &= TestEqual(Schedule + TEXT(": same terrain for the same command"), DifferentCells, 0);
    }

    Simulation Jittered(Settings, 42);
    Jittered.QueueTerrainEdit(4, 5, 6.0f);
    for (int Batch = 0; Batch < 100; ++Batch)
        for (const double Seconds : {0.016, 0.024, 0.037, 0.023})
            Jittered.Advance(Seconds);
    Passed &= TestEqual(TEXT("Jittered frames contain 100 ticks"), Jittered.TickCount(), std::uint64_t{100});
    Passed &= TestEqual(TEXT("Jittered frames apply the queued target"), Jittered.HeightAt(4, 5), 6.0f, 0.0f);

    Config QuarterSecondSettings = SmallConfig();
    QuarterSecondSettings.TickSeconds = 0.25;
    Simulation QuarterSecond(QuarterSecondSettings, 6);
    QuarterSecond.Advance(0.24);
    Passed &= TestEqual(TEXT("Configured tick duration does not run early"), QuarterSecond.TickCount(), std::uint64_t{0});
    QuarterSecond.Advance(0.01);
    QuarterSecond.Advance(0.75);
    Passed &= TestEqual(TEXT("One second contains four configured ticks"), QuarterSecond.TickCount(), std::uint64_t{4});
    Passed &= TestEqual(TEXT("Elapsed time uses the configured duration"), QuarterSecond.ElapsedSeconds(), 1.0, 1e-10);
    return Passed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDownstreamTerrainFifoTest,
    "Downstream.Simulation.TerrainCommands.FifoAtTick",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDownstreamTerrainFifoTest::RunTest(const FString& Parameters)
{
    bool Passed = true;
    Simulation Sim(SmallConfig(), 19);
    const float Original = Sim.HeightAt(1, 1);
    Sim.ConsumeDirtyChunks();
    Sim.QueueTerrainEdit(1, 1, -3.0f);
    Sim.QueueTerrainEdit(1, 1, 5.0f);
    Passed &= TestEqual(TEXT("Queueing does not change terrain"), Sim.HeightAt(1, 1), Original, 0.0f);
    Sim.Advance(0.09);
    Passed &= TestEqual(TEXT("Queued commands wait for the tick boundary"), Sim.HeightAt(1, 1), Original, 0.0f);
    Passed &= TestDirtyChunks(*this, TEXT("Pending commands do not dirty terrain"), Sim, {});
    Sim.Advance(0.01);
    // Reversing the queue or applying absolute heights as deltas changes this literal result.
    Passed &= TestEqual(TEXT("The final FIFO command assigns its absolute height"), Sim.HeightAt(1, 1), 5.0f, 0.0f);
    Passed &= TestDirtyChunks(*this, TEXT("Applied commands dirty their chunk"), Sim, {{0, 0}});
    Sim.Advance(0.2);
    Passed &= TestEqual(TEXT("Consumed commands do not replay"), Sim.HeightAt(1, 1), 5.0f, 0.0f);
    Passed &= TestDirtyChunks(*this, TEXT("Empty ticks do not repeat dirty chunks"), Sim, {});
    return Passed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDownstreamTerrainClampTest,
    "Downstream.Simulation.TerrainCommands.Clamp",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDownstreamTerrainClampTest::RunTest(const FString& Parameters)
{
    bool Passed = true;
    Simulation Sim(SmallConfig(), 9);
    Sim.QueueTerrainEdit(0, 0, -500.0f);
    Sim.QueueTerrainEdit(1, 0, 500.0f);
    Sim.QueueTerrainEdit(2, 0, 2.5f);
    Sim.Advance(0.1);
    Passed &= TestEqual(TEXT("Targets below the range clamp to -4"), Sim.HeightAt(0, 0), -4.0f, 0.0f);
    Passed &= TestEqual(TEXT("Targets above the range clamp to 7"), Sim.HeightAt(1, 0), 7.0f, 0.0f);
    Passed &= TestEqual(TEXT("Targets inside the range are preserved"), Sim.HeightAt(2, 0), 2.5f, 0.0f);

    Config FlatSettings = SmallConfig();
    FlatSettings.Width = 1;
    FlatSettings.Height = 1;
    FlatSettings.ChunkSize = 64;
    FlatSettings.MinimumHeight = 2.0f;
    FlatSettings.MaximumHeight = 2.0f;
    Simulation Flat(FlatSettings, 4);
    Passed &= TestEqual(TEXT("A degenerate range creates flat terrain"), Flat.HeightAt(0, 0), 2.0f, 0.0f);
    Passed &= TestDirtyChunks(*this, TEXT("A one-cell grid still has one chunk"), Flat, {{0, 0}});
    Flat.QueueTerrainEdit(0, 0, -4.0f);
    Flat.Advance(0.1);
    Passed &= TestEqual(TEXT("An edit clamps to the sole valid flat height"), Flat.HeightAt(0, 0), 2.0f, 0.0f);
    Passed &= TestDirtyChunks(*this, TEXT("Clamping to the existing flat height is a no-op"), Flat, {});
    return Passed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDownstreamTerrainValidationTest,
    "Downstream.Simulation.TerrainCommands.Validation",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDownstreamTerrainValidationTest::RunTest(const FString& Parameters)
{
    bool Passed = true;
    Simulation Sim(SmallConfig(), 6);
    const float Original = Sim.HeightAt(0, 0);
    Sim.ConsumeDirtyChunks();
    Sim.QueueTerrainEdit(0, 0, 3.0f);
    for (const auto& Point : {std::pair<int, int>{-1, 0}, {0, -1}, {65, 0}, {0, 33}})
    {
        Passed &= TestThrows<std::out_of_range>(*this, TEXT("Invalid edit coordinates reject immediately"),
            [&] { Sim.QueueTerrainEdit(Point.first, Point.second, 1.0f); });
        Passed &= TestThrows<std::out_of_range>(*this, TEXT("Invalid read coordinates reject immediately"),
            [&] { Sim.HeightAt(Point.first, Point.second); });
    }
    for (const float Height : {std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity(),
                              -std::numeric_limits<float>::infinity()})
        Passed &= TestThrows<std::invalid_argument>(*this, TEXT("Non-finite target heights reject immediately"),
            [&] { Sim.QueueTerrainEdit(0, 0, Height); });
    for (const double Seconds : {-0.1, std::numeric_limits<double>::quiet_NaN(),
                                std::numeric_limits<double>::infinity(),
                                -std::numeric_limits<double>::infinity()})
        Passed &= TestThrows<std::invalid_argument>(*this, TEXT("Invalid frame time rejects immediately"),
            [&] { Sim.Advance(Seconds); });
    Passed &= TestThrows<std::overflow_error>(*this, TEXT("Overflowing time rejects before mutation"),
        [&] { Sim.Advance(std::numeric_limits<double>::max()); });
    Passed &= TestEqual(TEXT("Invalid inputs preserve the tick count"), Sim.TickCount(), std::uint64_t{0});
    Passed &= TestEqual(TEXT("Invalid inputs preserve terrain before the tick"), Sim.HeightAt(0, 0), Original, 0.0f);
    Passed &= TestDirtyChunks(*this, TEXT("Rejected commands do not dirty chunks"), Sim, {});
    Sim.Advance(0.1);
    Passed &= TestEqual(TEXT("Invalid time does not consume the pending valid command"), Sim.HeightAt(0, 0), 3.0f, 0.0f);
    Passed &= TestDirtyChunks(*this, TEXT("Only the pending valid command dirties terrain"), Sim, {{0, 0}});

    Config InvalidSettings = SmallConfig();
    InvalidSettings.Width = 0;
    Passed &= TestThrows<std::invalid_argument>(*this, TEXT("A zero width is invalid"),
        [&] { Simulation Invalid(InvalidSettings, 1); });
    InvalidSettings = SmallConfig();
    InvalidSettings.TickSeconds = 0.0;
    Passed &= TestThrows<std::invalid_argument>(*this, TEXT("A zero tick duration is invalid"),
        [&] { Simulation Invalid(InvalidSettings, 1); });
    InvalidSettings = SmallConfig();
    InvalidSettings.MinimumHeight = 8.0f;
    Passed &= TestThrows<std::invalid_argument>(*this, TEXT("An inverted height range is invalid"),
        [&] { Simulation Invalid(InvalidSettings, 1); });
    return Passed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDownstreamDirtyChunksTest,
    "Downstream.Simulation.DirtyChunksOnly",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDownstreamDirtyChunksTest::RunTest(const FString& Parameters)
{
    bool Passed = true;
    Simulation Sim(SmallConfig(), 3);
    Passed &= TestDirtyChunks(*this, TEXT("Initial chunks include both partial grid edges"), Sim,
                             {{0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}});
    Passed &= TestDirtyChunks(*this, TEXT("Consumption clears the initial dirty state"), Sim, {});
    Sim.QueueTerrainEdit(64, 32, 7.0f);
    Sim.QueueTerrainEdit(63, 31, 7.0f);
    Sim.QueueTerrainEdit(62, 30, 7.0f);
    Passed &= TestDirtyChunks(*this, TEXT("Queueing does not dirty chunks before a tick"), Sim, {});
    Sim.Advance(0.1);
    // These cells touch only (2,1) and (1,0); their neighbors must stay clean.
    Passed &= TestDirtyChunks(*this, TEXT("Edits dirty only affected chunks, once each"), Sim, {{2, 1}, {1, 0}});
    Passed &= TestDirtyChunks(*this, TEXT("Consumed edit chunks do not repeat"), Sim, {});
    Sim.QueueTerrainEdit(4, 8, Sim.HeightAt(4, 8));
    Sim.QueueTerrainEdit(64, 32, 900.0f);
    Sim.Advance(0.1);
    Passed &= TestEqual(TEXT("The already-clamped cell remains at its limit"), Sim.HeightAt(64, 32), 7.0f, 0.0f);
    Passed &= TestDirtyChunks(*this, TEXT("Exact and clamped no-op commands do not dirty chunks"), Sim, {});
    Sim.QueueTerrainEdit(64, 32, -4.0f);
    Sim.Advance(0.1);
    Passed &= TestDirtyChunks(*this, TEXT("A later edit dirties the partial edge chunk again"), Sim, {{2, 1}});
    return Passed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDownstreamSeedReproductionTest,
    "Downstream.Simulation.SeedReproduction",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDownstreamSeedReproductionTest::RunTest(const FString& Parameters)
{
    bool Passed = true;
    const Config Settings = SmallConfig();
    Simulation First(Settings, 0x1234U);
    Simulation Different(Settings, 0x5678U);
    Simulation Repeated(Settings, 0x1234U);
    Passed &= TestEqual(TEXT("The simulation retains the requested seed"), First.Seed(), std::uint32_t{0x1234U});
    int32 ReproductionMismatches = 0;
    int32 OutOfRangeCells = 0;
    int32 DifferentSeedCells = 0;
    int32 SpatiallyDifferentCells = 0;
    const float Origin = First.HeightAt(0, 0);
    for (int Y = 0; Y < Settings.Height; ++Y)
        for (int X = 0; X < Settings.Width; ++X)
        {
            const float Height = First.HeightAt(X, Y);
            ReproductionMismatches += Height != Repeated.HeightAt(X, Y) ? 1 : 0;
            OutOfRangeCells += !std::isfinite(Height) || Height < -4.0f || Height > 7.0f ? 1 : 0;
            DifferentSeedCells += Height != Different.HeightAt(X, Y) ? 1 : 0;
            SpatiallyDifferentCells += Height != Origin ? 1 : 0;
        }
    Passed &= TestEqual(TEXT("The same seed reproduces every cell after another world is generated"), ReproductionMismatches, 0);
    Passed &= TestEqual(TEXT("Every generated height is finite and inside the configured range"), OutOfRangeCells, 0);
    Passed &= TestTrue(TEXT("Changing the seed changes terrain"), DifferentSeedCells > 0);
    Passed &= TestTrue(TEXT("Generator coordinates affect terrain"), SpatiallyDifferentCells > 0);

    Config ExtremeSettings = SmallConfig();
    ExtremeSettings.Width = 2;
    ExtremeSettings.Height = 2;
    ExtremeSettings.MinimumHeight = std::numeric_limits<float>::lowest();
    ExtremeSettings.MaximumHeight = std::numeric_limits<float>::max();
    Simulation Extreme(ExtremeSettings, 55);
    bool FiniteExtremeTerrain = true;
    for (int Y = 0; Y < 2; ++Y)
        for (int X = 0; X < 2; ++X)
            FiniteExtremeTerrain &= std::isfinite(Extreme.HeightAt(X, Y));
    Passed &= TestTrue(TEXT("Finite height extremes do not overflow generation"), FiniteExtremeTerrain);
    return Passed;
}

#endif // WITH_DEV_AUTOMATION_TESTS
