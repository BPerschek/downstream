// Headless tests for WaterGrid and the Drowned Vault earthworks.
#include "DownstreamEarthworks.h"
#include "DownstreamWater.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace downstream;

namespace
{
int Failures = 0;
int Passed = 0;

void Require(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

void Near(double actual, double expected, double tolerance, const std::string& what)
{
    if (std::abs(actual - expected) > tolerance)
        throw std::runtime_error(what + ": expected " + std::to_string(expected) + " got " + std::to_string(actual));
}

template <typename E, typename F>
void Throws(F f, const std::string& what)
{
    try { f(); } catch (const E&) { return; }
    throw std::runtime_error("Expected exception: " + what);
}

void Run(const char* name, const std::function<void()>& test)
{
    try { test(); ++Passed; std::cout << "PASS " << name << "\n"; }
    catch (const std::exception& e) { ++Failures; std::cout << "FAIL " << name << ": " << e.what() << "\n"; }
}

WaterConfig Box(int w, int h)
{
    WaterConfig c;
    c.Width = w;
    c.Height = h;
    return c;
}

double Accounted(const WaterGrid& g, double initial)
{
    return initial + g.SourcedVolume() + g.AddedVolume() - g.SunkVolume() - g.DrainedVolume() - g.EvaporatedVolume();
}

void Seconds(WaterGrid& g, double s)
{
    const int ticks = static_cast<int>(std::lround(s / g.Config().TickSeconds));
    for (int i = 0; i < ticks; ++i) g.Tick();
}
}

int main()
{
    Run("closed box conserves volume and levels out", [] {
        WaterGrid g(Box(12, 8));
        g.SetWater(2, 3, 4.0f);
        const double start = g.TotalWater();
        Near(start, 4.0, 1e-6, "initial volume");
        Seconds(g, 60.0);
        Near(g.TotalWater(), Accounted(g, 0.0), 1e-3, "accounted volume");
        Near(g.TotalWater(), 4.0, 1e-3, "closed volume");
        const float mean = 4.0f / (12 * 8);
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 12; ++x)
                Near(g.Water(x, y), mean, 0.01, "level at " + std::to_string(x) + "," + std::to_string(y));
    });

    Run("depth never negative and stays finite", [] {
        WaterGrid g(Box(10, 10));
        for (int y = 0; y < 10; ++y)
            for (int x = 0; x < 10; ++x)
                g.SetGround(x, y, static_cast<float>((x * 7 + y * 3) % 5) * 0.4f);
        g.SetWater(5, 5, 3.0f);
        g.SetWater(0, 0, 2.0f);
        for (int t = 0; t < 600; ++t)
        {
            g.Tick();
            for (int y = 0; y < 10; ++y)
                for (int x = 0; x < 10; ++x)
                    Require(g.Water(x, y) >= 0.0f && std::isfinite(g.Water(x, y)), "bad depth");
        }
        Near(g.TotalWater(), Accounted(g, 0.0), 1e-3, "accounted");
    });

    Run("a wall dams water; lowering it releases water", [] {
        WaterGrid g(Box(9, 3));
        for (int y = 0; y < 3; ++y) g.SetGround(4, y, 2.0f);
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 4; ++x) g.SetWater(x, y, 1.0f);
        Seconds(g, 30.0);
        for (int y = 0; y < 3; ++y)
            for (int x = 5; x < 9; ++x) Require(g.Water(x, y) == 0.0f, "water leaked through dam");
        // Cut a 1-cell notch through the dam.
        g.LowerGround(4, 1, 2.0f, 0.0f);
        Seconds(g, 60.0);
        Require(g.Water(7, 1) > 0.3f, "water did not pass the notch");
        Near(g.Surface(1, 1), g.Surface(7, 1), 0.05, "levels across notch");
    });

    Run("source fills; sink removes and is counted", [] {
        WaterGrid g(Box(6, 1));
        g.SetSource(0, 0, 0.5f);
        g.SetSink(5, 0, true);
        Seconds(g, 40.0);
        Near(g.SourcedVolume(), 20.0, 0.01, "sourced");
        Require(g.SunkVolume() > 15.0, "sink did not drain flow");
        Require(g.Water(5, 0) == 0.0f, "sink cell holds water");
        Near(g.TotalWater(), Accounted(g, 0.0), 1e-3, "accounted");
    });

    Run("edge drainage removes water at the boundary", [] {
        auto c = Box(5, 5);
        c.DrainAtEdges = true;
        WaterGrid g(c);
        g.SetWater(2, 2, 2.0f);
        Seconds(g, 60.0);
        Require(g.TotalWater() < 0.05, "edges did not drain");
        Near(g.TotalWater(), Accounted(g, 0.0), 1e-3, "accounted");
    });

    Run("evaporation dries shallow water", [] {
        auto c = Box(3, 3);
        c.EvaporationPerSecond = 0.01f;
        WaterGrid g(c);
        g.SetWater(1, 1, 0.05f);
        Seconds(g, 20.0);
        Require(g.TotalWater() < 1e-6, "water remained");
        Near(g.EvaporatedVolume(), 0.05, 1e-4, "evaporated");
    });

    Run("deterministic: identical runs match bit for bit", [] {
        auto make = [] {
            WaterGrid g(DrownedVaultConfig());
            BuildDrownedVaultSite(g);
            Seconds(g, 20.0);
            return g;
        };
        auto a = make();
        auto b = make();
        for (int y = 0; y < a.Height(); ++y)
            for (int x = 0; x < a.Width(); ++x)
            {
                const float wa = a.Water(x, y), wb = b.Water(x, y);
                Require(std::memcmp(&wa, &wb, sizeof(float)) == 0, "nondeterministic depth");
            }
    });

    Run("advance accumulates partial ticks and caps hitches", [] {
        WaterGrid g(Box(4, 4));
        Require(g.Advance(0.05) == 0, "early tick");
        Require(g.Advance(0.05) == 1, "missed tick at boundary");
        Require(g.Advance(10.0) == WaterGrid::MaxTicksPerAdvance, "hitch not capped");
        Require(g.Advance(0.0) <= 1, "hitch backlog retained");
        Throws<std::invalid_argument>([&] { g.Advance(-1.0); }, "negative time");
        Throws<std::invalid_argument>([&] { g.Advance(std::nan("")); }, "nan time");
    });

    Run("invalid input is rejected", [] {
        Throws<std::invalid_argument>([] { WaterGrid g(Box(0, 4)); }, "zero width");
        auto unstable = Box(4, 4);
        unstable.SubstepsPerTick = 1;
        unstable.CellSize = 0.1f;
        Throws<std::invalid_argument>([&] { WaterGrid g(unstable); }, "unstable step");
        WaterGrid g(Box(4, 4));
        Throws<std::out_of_range>([&] { g.Water(4, 0); }, "out of range");
        Throws<std::invalid_argument>([&] { g.SetWater(1, 1, -1.0f); }, "negative water");
        Throws<std::invalid_argument>([&] { g.SetGround(1, 1, std::nanf("")); }, "nan ground");
        Throws<std::invalid_argument>([&] { g.SetSource(1, 1, -2.0f); }, "negative source");
    });

    Run("ground edits mark render chunks dirty", [] {
        auto c = Box(32, 32);
        c.ChunkSize = 16;
        WaterGrid g(c);
        Require(g.ConsumeDirtyGroundChunks().size() == 4, "initial chunks not dirty");
        Require(g.ConsumeDirtyGroundChunks().empty(), "dirty not cleared");
        g.LowerGround(5, 5, 0.0f, 0.0f);
        Require(g.ConsumeDirtyGroundChunks().empty(), "no-op edit marked dirty");
        g.RaiseGround(15, 3, 1.0f, 5.0f);
        Require(g.ConsumeDirtyGroundChunks().size() == 2, "chunk-border edit must dirty both chunks");
    });

    Run("canal raster is edge-connected", [] {
        for (auto [a, b] : std::vector<std::pair<WaterCell, WaterCell>>{{{0, 0}, {7, 3}}, {{9, 9}, {2, 1}}, {{3, 3}, {3, 3}}, {{0, 5}, {6, 5}}})
        {
            const auto cells = RasterizeCanal(a, b);
            Require(cells.front().X == a.X && cells.front().Y == a.Y && cells.back().X == b.X && cells.back().Y == b.Y, "endpoints");
            for (std::size_t i = 1; i < cells.size(); ++i)
                Require(std::abs(cells[i].X - cells[i - 1].X) + std::abs(cells[i].Y - cells[i - 1].Y) == 1, "diagonal step");
        }
    });

    Run("dig job grades the canal and conserves spoil", [] {
        WaterGrid g(Box(10, 5));
        for (int y = 0; y < 5; ++y)
            for (int x = 0; x < 10; ++x) g.SetGround(x, y, 2.0f);
        g.SetGround(0, 2, 0.5f);
        g.SetGround(9, 2, 0.1f);
        DigJob job(g, RasterizeCanal({0, 2}, {9, 2}));
        double before = 0.0;
        for (int y = 0; y < 5; ++y) for (int x = 0; x < 10; ++x) before += g.Ground(x, y);
        int actions = 0;
        while (job.WorkNext(g)) Require(++actions < 1000, "dig did not finish");
        Require(job.IsComplete(g), "incomplete");
        Near(job.Progress(g), 1.0, 1e-5, "progress");
        for (int x = 1; x < 9; ++x)
        {
            Near(g.Ground(x, 2), 0.5 + (0.1 - 0.5) * x / 9.0, 1e-4, "graded bed");
            Require(g.Ground(x, 1) > 2.0f || g.Ground(x, 3) > 2.0f, "no berm");
        }
        double after = 0.0;
        for (int y = 0; y < 5; ++y) for (int x = 0; x < 10; ++x) after += g.Ground(x, y);
        Near(after + job.SpoilDiscarded(), before, 1e-3, "spoil conserved");
        Throws<std::invalid_argument>([&] { DigJob bad(g, {{0, 0}, {1, 1}}); }, "diagonal path");
    });

    Run("earthworks persistence validates and round-trips", [] {
        WaterGrid g(DrownedVaultConfig());
        BuildDrownedVaultSite(g);
        EarthworksState e;
        e.CaptureTerrain(g);
        Require(e.GetTerrain().empty(), "fresh site has deltas");
        g.LowerGround(3, 3, 0.5f, 0.0f);
        e.CaptureTerrain(g);
        Require(e.GetTerrain().size() == 1 && e.GetTerrain()[0].HeightMm == 1300, "delta not captured");
        WaterGrid fresh(DrownedVaultConfig());
        BuildDrownedVaultSite(fresh);
        e.ApplyTerrain(fresh);
        Near(fresh.Ground(3, 3), 1.3, 1e-6, "applied delta");
        EarthworksState other;
        Require(!other.LoadPersisted(true, false, {{5000, 0, 100}}), "out-of-grid delta accepted");
        Require(!other.LoadPersisted(true, false, {{1, 1, 100}, {1, 1, 200}}), "duplicate delta accepted");
        Require(!other.LoadPersisted(true, false, {{1, 1, 99999}}), "height out of range accepted");
        Require(!other.IsDiggerBuilt(), "failed load changed state");
        Require(other.LoadPersisted(true, true, {{1, 1, 100}}) && other.IsDiggerBuilt() && other.IsVaultOpened(), "valid load");
        Require(!other.OpenVault(), "vault opened twice");
        Require(other.CanBuildDigger(100) == false, "digger built twice");
    });

    Run("Drowned Vault: flooded at rest, drained by the culvert canal", [] {
        WaterGrid g(DrownedVaultConfig());
        BuildDrownedVaultSite(g);
        const auto vault = DrownedVaultSite::Vault;
        Seconds(g, 120.0);
        const float flooded = g.Water(vault.X, vault.Y);
        std::printf("  steady vault depth %.3f m, total %.2f m^3, max %.2f\n", flooded, g.TotalWater(), g.MaxDepth());
        Require(flooded > 1.0f, "vault should start deep underwater");
        // Plateau stays dry away from the ditches: water must not spill over the site.
        Require(g.Water(4, 15) < 0.01f && g.Water(25, 4) < 0.01f, "plateau flooded");
        Near(g.TotalWater(), Accounted(g, 0.0), 0.05, "site accounting");

        // Dig from the basin floor to the culvert, ticking water as a robot would.
        DigJob job(g, RasterizeCanal(vault, DrownedVaultSite::CulvertB));
        std::printf("  canal cells %zu\n", job.Cells().size());
        int actions = 0;
        while (!job.IsComplete(g))
        {
            job.WorkNext(g);
            ++actions;
            Seconds(g, 0.8);
            Require(actions < 500, "dig job stalled");
        }
        Seconds(g, 150.0);
        const float drained = g.Water(vault.X, vault.Y);
        std::printf("  actions %d, vault depth after %.3f m, culvert sunk %.1f m^3\n", actions, drained, g.SunkVolume());
        Require(drained < DrownedVaultSite::VaultAccessDepth, "canal did not drain the vault");
        Near(g.TotalWater(), Accounted(g, 0.0), 0.05, "site accounting after dig");
    });

    Run("Drowned Vault: a canal that misses the culvert does not drain it", [] {
        WaterGrid g(DrownedVaultConfig());
        BuildDrownedVaultSite(g);
        Seconds(g, 60.0);
        // Ends on the plateau: graded at plateau height, so nothing below spill level is cut.
        DigJob job(g, RasterizeCanal({4, 10}, {4, 3}));
        while (job.WorkNext(g)) {}
        Seconds(g, 120.0);
        Require(g.Water(DrownedVaultSite::Vault.X, DrownedVaultSite::Vault.Y) > 1.0f, "unrelated ditch drained vault");
    });

    Run("Drowned Vault: canal depth decides whether a shore start drains the vault", [] {
        auto dig = [](float depth) {
            WaterGrid g(DrownedVaultConfig());
            BuildDrownedVaultSite(g);
            Seconds(g, 60.0);
            DigRules rules;
            rules.MinimumBedDrop = depth;
            // Starts on the basin slope (0.58 m ground), not at the vault.
            DigJob job(g, RasterizeCanal({12, 9}, DrownedVaultSite::CulvertB), rules);
            int actions = 0;
            while (job.WorkNext(g)) { ++actions; Seconds(g, 0.5); }
            Seconds(g, 150.0);
            std::printf("  depth %.2f m: %d actions, vault depth %.3f m\n", depth, actions, g.Water(DrownedVaultSite::Vault.X, DrownedVaultSite::Vault.Y));
            return g.Water(DrownedVaultSite::Vault.X, DrownedVaultSite::Vault.Y);
        };
        Require(dig(0.0f) > DrownedVaultSite::VaultAccessDepth, "grade-only canal from the slope should leave the vault wet");
        Require(dig(1.0f) < DrownedVaultSite::VaultAccessDepth, "1 m canal from the slope should drain the vault");
    });

    std::cout << Passed << " passed, " << Failures << " failed\n";
    return Failures == 0 ? 0 : 1;
}
