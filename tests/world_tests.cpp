// Headless tests for seeded world generation and Dam Cult outpost rules.
#include "DownstreamEarthworks.h"
#include "DownstreamWater.h"
#include "DownstreamWorld.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace downstream;

namespace
{
int Failures = 0, Passed = 0;
void Require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }
void Run(const char* name, const std::function<void()>& t)
{
    try { t(); ++Passed; std::cout << "PASS " << name << "\n"; }
    catch (const std::exception& e) { ++Failures; std::cout << "FAIL " << name << ": " << e.what() << "\n"; }
}
void Seconds(WaterGrid& g, double s) { const int n = static_cast<int>(std::lround(s / g.Config().TickSeconds)); for (int i = 0; i < n; ++i) g.Tick(); }
WaterGrid Settled(const WorldLayout& L, double seconds)
{
    WaterGrid g(WorldWaterConfig(L));
    ApplyWorld(g, L, true);
    Seconds(g, seconds);
    return g;
}
double Accounted(const WaterGrid& g) { return g.SourcedVolume() + g.AddedVolume() - g.SunkVolume() - g.DrainedVolume() - g.EvaporatedVolume(); }
std::string S(std::uint32_t seed) { return "seed " + std::to_string(seed) + ": "; }

// Cut a canal like the digger would, ticking water between steps.
void Dig(WaterGrid& g, WaterCell from, WaterCell to, float depth, double secondsPerStep = 0.4)
{
    DigRules rules;
    rules.MinimumBedDrop = depth;
    rules.MaxCells = 64;
    DigJob job(g, RasterizeCanal(from, to), rules);
    int guard = 0;
    while (job.WorkNext(g)) { Seconds(g, secondsPerStep); Require(++guard < 2000, "dig job did not finish"); }
}
}

int main(int argc, char** argv)
{
    const int seedCount = argc > 1 ? std::atoi(argv[1]) : 25;

    Run("same seed reproduces the same world; different seeds differ", [] {
        const auto a = GenerateWorld(1234), b = GenerateWorld(1234), c = GenerateWorld(4321);
        Require(a.Ground.size() == b.Ground.size() && std::memcmp(a.Ground.data(), b.Ground.data(), a.Ground.size() * sizeof(float)) == 0, "ground differs for equal seeds");
        Require(a.Outposts.size() == b.Outposts.size() && a.Caches.size() == b.Caches.size(), "features differ for equal seeds");
        Require(std::memcmp(a.Ground.data(), c.Ground.data(), a.Ground.size() * sizeof(float)) != 0, "different seeds produced identical ground");
    });

    Run("invalid sizes are rejected", [] {
        WorldGenSettings s; s.Width = 10;
        try { GenerateWorld(1, s); } catch (const std::invalid_argument&) { return; }
        throw std::runtime_error("tiny world accepted");
    });

    int outpostsTotal = 0, vaultsTotal = 0;
    Run("every seed: rivers, home, outposts and vault placed and walkable", [&] {
        for (int seed = 1; seed <= seedCount; ++seed)
        {
            const auto L = GenerateWorld(static_cast<std::uint32_t>(seed));
            Require(!L.Rivers.empty(), S(seed) + "no river");
            Require(!L.Sinks.empty(), S(seed) + "no drainage");
            Require(L.Home.X > 0 && L.InBounds(L.Home.X, L.Home.Y), S(seed) + "home not placed");
            Require(!L.Outposts.empty(), S(seed) + "no outpost");
            Require(!L.Vaults.empty(), S(seed) + "no vault");
            Require(L.Caches.size() >= 6, S(seed) + "too few caches");
            outpostsTotal += static_cast<int>(L.Outposts.size());
            vaultsTotal += static_cast<int>(L.Vaults.size());
            // Home clearing is flat.
            const float h0 = L.GroundAt(L.Home.X, L.Home.Y);
            for (int dy = -L.HomeHalfY; dy <= L.HomeHalfY; ++dy)
                for (int dx = -L.HomeHalfX; dx <= L.HomeHalfX; ++dx)
                    Require(std::abs(L.GroundAt(L.Home.X + dx, L.Home.Y + dy) - h0) < 0.01f, S(seed) + "home clearing not flat");
            // Outposts sit below their river so a levee cut floods them.
            for (const auto& o : L.Outposts)
            {
                Require(o.PadHeight + 0.8f < L.GroundAt(o.RiverCell.X, o.RiverCell.Y) + 0.4f, S(seed) + "outpost pad not below river");
                Require(o.Structures.size() >= 4, S(seed) + "outpost has too few structures");
            }
        }
        std::printf("  %d seeds: %d outposts, %d vaults\n", seedCount, outpostsTotal, vaultsTotal);
    });

    Run("every seed at rest: outposts and home dry, vault flooded, river drains, volume conserved", [&] {
        for (int seed = 1; seed <= seedCount; ++seed)
        {
            const auto L = GenerateWorld(static_cast<std::uint32_t>(seed));
            auto g = Settled(L, 150.0);
            for (const auto& o : L.Outposts)
            {
                OutpostState st(o);
                Require(st.FloodDepth(g) < 0.05f, S(seed) + "outpost " + std::to_string(o.Id) + " wet at rest (" + std::to_string(st.FloodDepth(g)) + " m)");
            }
            Require(g.Water(L.Home.X, L.Home.Y) < 0.01f, S(seed) + "home wet at rest");
            for (const auto& v : L.Vaults)
                Require(g.Water(v.Cell.X, v.Cell.Y) > 0.8f, S(seed) + "vault not flooded (" + std::to_string(g.Water(v.Cell.X, v.Cell.Y)) + " m)");
            Require(g.SunkVolume() > 1.0, S(seed) + "river never reached the drainage edge");
            Require(std::abs(g.TotalWater() - Accounted(g)) < 0.5, S(seed) + "volume not conserved");
        }
    });

    Run("levee breach floods an outpost, structures fall, engineers dam it, pump recovers", [] {
        int tested = 0;
        for (int seed = 1; seed <= 6; ++seed)
        {
            const auto L = GenerateWorld(static_cast<std::uint32_t>(seed));
            auto g = Settled(L, 120.0);
            const auto& o = L.Outposts.front();
            OutpostState st(o);
            Dig(g, o.RiverCell, o.Center, 1.0f);
            float peak = 0.0f;
            int fallen = 0;
            for (int t = 0; t < 90; ++t)
            {
                Seconds(g, 1.0);
                peak = std::max(peak, st.FloodDepth(g));
                fallen += static_cast<int>(st.ApplyFlood(g, 1.0f).size());
            }
            std::printf("  seed %d: peak flood %.2f m, %d structures fell\n", seed, peak, fallen);
            Require(peak > 0.4f, S(seed) + "breach did not flood the outpost");
            Require(fallen >= 2, S(seed) + "flood did not destroy structures");
            WaterCell breach;
            Require(st.FindBreach(g, breach), S(seed) + "breach not detected");
            int actions = 0;
            while (st.FindBreach(g, breach) && actions < 200) { st.DamBreach(g, breach); Seconds(g, 1.2); ++actions; }
            Require(actions < 200, S(seed) + "engineers could not seal the breach");
            const float before = st.FloodDepth(g);
            OutpostState fresh(o); // generator standing: pumps run
            for (int t = 0; t < 120; ++t) { fresh.Pump(g, 1.0f); Seconds(g, 1.0); }
            std::printf("  seed %d: sealed in %d dam actions; flood %.2f -> %.2f m after 120 s of pumping\n", seed, actions, before, fresh.FloodDepth(g));
            Require(fresh.FloodDepth(g) < before, S(seed) + "pump did not lower a sealed flood");
            ++tested;
        }
        Require(tested == 6, "not all seeds tested");
    });

    Run("engineers can save an outpost from a sloppy cut, not from one aimed at the generator", [] {
        // The frontier's core tension. Unopposed, a breach drowns the outpost. Engineers who
        // reach it in ~12 s seal a canal that stops short on the pad, but a cut driven right
        // under the generator floods it before the sandbags win.
        int razedShort = 0, heldShort = 0, heldCentre = 0;
        for (int seed = 1; seed <= 8; ++seed)
        {
            const auto L = GenerateWorld(static_cast<std::uint32_t>(seed));
            const auto& o = L.Outposts.front();
            const double dx = o.RiverCell.X - o.Center.X, dy = o.RiverCell.Y - o.Center.Y;
            const double len = std::max(1.0, std::sqrt(dx * dx + dy * dy));
            const WaterCell shortEnd{static_cast<int>(std::lround(o.Center.X + 2.0 * dx / len)), static_cast<int>(std::lround(o.Center.Y + 2.0 * dy / len))};
            for (int scenario = 0; scenario < 3; ++scenario)
            {
                const bool engineers = scenario != 0;
                const WaterCell end = scenario == 2 ? o.Center : shortEnd;
                auto g = Settled(L, 120.0);
                OutpostState st(o);
                Dig(g, o.RiverCell, end, 1.0f);
                double nextDam = 12.0; // walking time from their post to the breach
                for (int step = 0; step < 1800; ++step)
                {
                    const double t = step * 0.1;
                    Seconds(g, 0.1);
                    st.ApplyFlood(g, 0.1f);
                    st.Pump(g, 0.1f);
                    WaterCell breach;
                    if (engineers && !st.IsRazed() && t >= nextDam)
                    {
                        if (st.FindBreach(g, breach)) st.DamBreach(g, breach);
                        nextDam = t + 1.1;
                    }
                }
                if (scenario == 0 && st.IsRazed()) ++razedShort;
                if (scenario == 1 && !st.IsRazed()) ++heldShort;
                if (scenario == 2 && !st.IsRazed()) ++heldCentre;
            }
        }
        std::printf("  short cut: unopposed razed %d/8, engineers held %d/8; cut under the generator: engineers held %d/8\n", razedShort, heldShort, heldCentre);
        Require(razedShort >= 6, "an unopposed breach should usually raze the outpost within 3 minutes");
        Require(heldShort >= 6, "engineers should usually seal a cut that stops short of the generator");
        Require(heldCentre <= 3, "a cut aimed at the generator should usually beat the engineers");
    });

    Run("a razed outpost no longer pumps; destroyed mask round-trips", [] {
        const auto L = GenerateWorld(7);
        OutpostState st(L.Outposts.front());
        Require(!st.IsRazed(), "fresh outpost razed");
        const int generatorIndex = 0;
        int mask = 1 << generatorIndex;
        Require(st.RestoreDestroyedMask(mask) && st.IsRazed() && st.DestroyedMask() == mask, "mask restore failed");
        WaterGrid g(WorldWaterConfig(L));
        ApplyWorld(g, L, true);
        g.SetWater(L.Outposts.front().Center.X, L.Outposts.front().Center.Y, 1.0f);
        Require(st.Pump(g, 5.0f) == 0.0, "razed outpost pumped");
        Require(!st.RestoreDestroyedMask(1 << 30), "out of range mask accepted");
    });

    Run("vault drains through a canal to its river", [] {
        for (int seed = 1; seed <= 4; ++seed)
        {
            const auto L = GenerateWorld(static_cast<std::uint32_t>(seed));
            auto g = Settled(L, 90.0);
            const auto& v = L.Vaults.front();
            Dig(g, v.Cell, v.RiverCell, 1.0f);
            Seconds(g, 150.0);
            std::printf("  seed %d: vault depth after canal %.2f m\n", seed, g.Water(v.Cell.X, v.Cell.Y));
            Require(g.Water(v.Cell.X, v.Cell.Y) < DrownedVaultSite::VaultAccessDepth, S(seed) + "vault did not drain");
        }
    });

    Run("world earthworks deltas round-trip against the generated base", [] {
        const auto L = GenerateWorld(99);
        WaterGrid g(WorldWaterConfig(L));
        ApplyWorld(g, L, false);
        g.LowerGround(L.Home.X, L.Home.Y, 0.5f, 0.0f);
        EarthworksState e;
        e.CaptureTerrain(g, L.Ground);
        Require(e.GetTerrain().size() == 1, "expected one delta");
        WaterGrid f(WorldWaterConfig(L));
        ApplyWorld(f, L, false);
        e.ApplyTerrain(f);
        Require(std::abs(f.Ground(L.Home.X, L.Home.Y) - g.Ground(L.Home.X, L.Home.Y)) < 0.001f, "delta not applied");
        Require(e.LoadWorld(99, {0, 2}, {{0, 3}, {1, 1}}) && e.IsVaultOpened(2) && e.GetOutpostMask(0) == 3, "world state load failed");
        Require(!e.LoadWorld(99, {1, 1}, {}), "duplicate vault ids accepted");
    });

    Run("classic Drowned Vault layout matches the M1 site", [] {
        const auto L = MakeDrownedVaultLayout();
        Require(L.Width == DrownedVaultSite::Width && L.Vaults.size() == 1 && L.Vaults[0].Cell.X == DrownedVaultSite::Vault.X, "layout mismatch");
        auto g = Settled(L, 60.0);
        Require(g.Water(DrownedVaultSite::Vault.X, DrownedVaultSite::Vault.Y) > 1.0f, "classic vault not flooded");
    });

    std::cout << Passed << " passed, " << Failures << " failed\n";
    return Failures == 0 ? 0 : 1;
}
