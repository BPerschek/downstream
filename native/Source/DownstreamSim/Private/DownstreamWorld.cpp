#include "DownstreamWorld.h"
#include "DownstreamEarthworks.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace downstream
{
namespace
{

std::uint32_t Hash(std::uint32_t seed, int x, int y, std::uint32_t salt)
{
    std::uint32_t v = seed ^ (salt * 0x632be5abU) ^ (static_cast<std::uint32_t>(x) * 0x9e3779b9U) ^ (static_cast<std::uint32_t>(y) * 0x85ebca6bU);
    v ^= v >> 16U; v *= 0x7feb352dU; v ^= v >> 15U; v *= 0x846ca68bU; v ^= v >> 16U;
    return v;
}

float Unit(std::uint32_t seed, int x, int y, std::uint32_t salt)
{
    return static_cast<float>(Hash(seed, x, y, salt) >> 8U) / 16777216.0f;
}

float Smooth(float t) { t = std::clamp(t, 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); }

float ValueNoise(std::uint32_t seed, float x, float y, float spacing, std::uint32_t salt)
{
    const float fx = x / spacing, fy = y / spacing;
    const int ix = static_cast<int>(std::floor(fx)), iy = static_cast<int>(std::floor(fy));
    const float tx = Smooth(fx - ix), ty = Smooth(fy - iy);
    const float a = Unit(seed, ix, iy, salt), b = Unit(seed, ix + 1, iy, salt);
    const float c = Unit(seed, ix, iy + 1, salt), d = Unit(seed, ix + 1, iy + 1, salt);
    return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
}

// Deterministic integer RNG for placement decisions.
struct Rng
{
    std::uint32_t State;
    explicit Rng(std::uint32_t seed) : State(seed ^ 0xa511e9b3U) {}
    std::uint32_t Next() { State ^= State << 13U; State ^= State >> 17U; State ^= State << 5U; return State; }
    int Range(int lo, int hi) { return lo + static_cast<int>(Next() % static_cast<std::uint32_t>(hi - lo + 1)); }
    float Float() { return static_cast<float>(Next() >> 8U) / 16777216.0f; }
};

struct Builder
{
    WorldLayout& L;
    std::vector<unsigned char> Channel;  // river channel cells (never stamped over)
    std::vector<unsigned char> Reserved; // occupied by a feature stamp
    explicit Builder(WorldLayout& layout) : L(layout),
        Channel(static_cast<std::size_t>(layout.Width) * layout.Height, 0),
        Reserved(static_cast<std::size_t>(layout.Width) * layout.Height, 0) {}
    std::size_t I(int x, int y) const { return static_cast<std::size_t>(y) * L.Width + x; }
    float& H(int x, int y) { return L.Ground[I(x, y)]; }
    float EdgeDistance(int x, int y) const
    {
        switch (L.LowEdge)
        {
        case 0: return static_cast<float>(x);
        case 1: return static_cast<float>(L.Width - 1 - x);
        case 2: return static_cast<float>(y);
        default: return static_cast<float>(L.Height - 1 - y);
        }
    }
    float EdgeSpan() const { return static_cast<float>((L.LowEdge < 2 ? L.Width : L.Height) - 1); }
    int MinChannelDistance(int x, int y) const
    {
        int best = std::numeric_limits<int>::max();
        for (const auto& r : L.Rivers)
            for (const auto& c : r.Path)
                best = std::min(best, std::max(std::abs(c.X - x), std::abs(c.Y - y)));
        return best;
    }
};

float Dist(WaterCell a, WaterCell b) { return std::hypot(static_cast<float>(a.X - b.X), static_cast<float>(a.Y - b.Y)); }

void BaseTerrain(Builder& b, std::uint32_t seed)
{
    WorldLayout& L = b.L;
    for (int y = 0; y < L.Height; ++y)
        for (int x = 0; x < L.Width; ++x)
        {
            float h = 1.0f + 2.4f * ValueNoise(seed, static_cast<float>(x), static_cast<float>(y), 18.0f, 1)
                    + 0.8f * ValueNoise(seed, static_cast<float>(x), static_cast<float>(y), 7.0f, 2);
            h += 2.0f * (b.EdgeDistance(x, y) / b.EdgeSpan());
            b.H(x, y) = h;
        }
    // Two box-blur passes keep slopes walkable.
    std::vector<float> tmp(L.Ground.size());
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int y = 0; y < L.Height; ++y)
            for (int x = 0; x < L.Width; ++x)
            {
                float sum = 0.0f; int n = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if (L.InBounds(x + dx, y + dy)) { sum += L.GroundAt(x + dx, y + dy); ++n; }
                tmp[b.I(x, y)] = sum / static_cast<float>(n);
            }
        L.Ground.swap(tmp);
    }
    // Drainage trench along the low edge; its outer row is a sink.
    for (int y = 0; y < L.Height; ++y)
        for (int x = 0; x < L.Width; ++x)
        {
            const float d = b.EdgeDistance(x, y);
            if (d <= 1.0f) b.H(x, y) = 0.0f;
            else if (d <= 6.0f) b.H(x, y) = std::min(b.H(x, y), 0.4f + (b.H(x, y) - 0.4f) * (d - 1.0f) / 5.0f);
            if (d == 0.0f) L.Sinks.push_back({x, y});
        }
}

bool CarveRiver(Builder& b, Rng& rng, WaterCell spring)
{
    WorldLayout& L = b.L;
    RiverLayout river;
    river.Spring = spring;
    std::vector<unsigned char> visited(L.Ground.size(), 0);
    WaterCell c = spring;
    for (int step = 0; step < 900 && b.EdgeDistance(c.X, c.Y) > 1.0f; ++step)
    {
        river.Path.push_back(c);
        visited[b.I(c.X, c.Y)] = 1;
        const WaterCell options[4] = {{c.X + 1, c.Y}, {c.X - 1, c.Y}, {c.X, c.Y + 1}, {c.X, c.Y - 1}};
        float best = std::numeric_limits<float>::max();
        WaterCell next = c;
        for (const auto& o : options)
        {
            if (!L.InBounds(o.X, o.Y) || visited[b.I(o.X, o.Y)]) continue;
            // Lower ground and progress toward the drainage edge both pull the river.
            const float score = L.GroundAt(o.X, o.Y) + 0.06f * b.EdgeDistance(o.X, o.Y) + 0.05f * rng.Float();
            if (score < best) { best = score; next = o; }
        }
        if (next.X == c.X && next.Y == c.Y) return false;
        c = next;
    }
    if (b.EdgeDistance(c.X, c.Y) > 1.0f || river.Path.size() < 20) return false;
    river.Path.push_back(c);
    // Carve a monotonically descending bed so water always reaches the edge.
    float bed = L.GroundAt(spring.X, spring.Y) - 0.3f;
    for (std::size_t i = 0; i < river.Path.size(); ++i)
    {
        const auto& p = river.Path[i];
        bed = i == 0 ? bed : std::min(bed - 0.012f, L.GroundAt(p.X, p.Y) - 0.45f);
        bed = std::max(bed, b.EdgeDistance(p.X, p.Y) <= 1.0f ? 0.0f : 0.02f);
        river.Bed.push_back(bed);
    }
    for (std::size_t i = 0; i < river.Path.size(); ++i)
    {
        const auto& p = river.Path[i];
        const float bedHere = river.Bed[i];
        b.H(p.X, p.Y) = std::min(b.H(p.X, p.Y), bedHere);
        b.Channel[b.I(p.X, p.Y)] = 1;
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
            {
                const int x = p.X + dx, y = p.Y + dy;
                if (!L.InBounds(x, y) || (dx == 0 && dy == 0)) continue;
                const int ring = std::max(std::abs(dx), std::abs(dy));
                const float target = ring == 1 ? bedHere + 0.25f : bedHere + 0.7f;
                b.H(x, y) = std::min(b.H(x, y), target);
                if (ring == 1 && std::abs(dx) + std::abs(dy) == 1) b.Channel[b.I(x, y)] = 1;
            }
    }
    L.Sources.push_back({spring, 0.0f});
    L.Rivers.push_back(std::move(river));
    return true;
}

void PlaceHome(Builder& b)
{
    WorldLayout& L = b.L;
    const int hx = L.HomeHalfX, hy = L.HomeHalfY;
    float bestScore = std::numeric_limits<float>::max();
    WaterCell best{L.Width / 2, L.Height / 2};
    // Prefer a clearing well away from rivers; relax the distance if rivers crowd the map.
    for (int clearance = std::max(hx, hy) + 8; clearance >= std::max(hx, hy) + 2 && bestScore == std::numeric_limits<float>::max(); clearance -= 2)
    for (int y = hy + 8; y < L.Height - hy - 8; y += 3)
        for (int x = hx + 8; x < L.Width - hx - 8; x += 3)
        {
            const float t = b.EdgeDistance(x, y) / b.EdgeSpan();
            if (t < 0.2f || t > 0.85f) continue;
            if (b.MinChannelDistance(x, y) < clearance) continue;
            float lo = 1e9f, hi = -1e9f;
            for (int dy = -hy; dy <= hy; dy += 2)
                for (int dx = -hx; dx <= hx; dx += 2)
                { const float h = L.GroundAt(x + dx, y + dy); lo = std::min(lo, h); hi = std::max(hi, h); }
            const float score = (hi - lo) + 0.02f * std::abs(static_cast<float>(x - L.Width / 2));
            if (score < bestScore) { bestScore = score; best = {x, y}; }
        }
    L.Home = best;
    float sum = 0.0f; int n = 0;
    for (int dy = -hy; dy <= hy; ++dy)
        for (int dx = -hx; dx <= hx; ++dx) { sum += L.GroundAt(best.X + dx, best.Y + dy); ++n; }
    const float h0 = sum / static_cast<float>(n);
    const int ramp = 5;
    for (int dy = -hy - ramp; dy <= hy + ramp; ++dy)
        for (int dx = -hx - ramp; dx <= hx + ramp; ++dx)
        {
            const int x = best.X + dx, y = best.Y + dy;
            if (!L.InBounds(x, y) || b.Channel[b.I(x, y)]) continue;
            const int ox = std::max(0, std::abs(dx) - hx), oy = std::max(0, std::abs(dy) - hy);
            const float out = static_cast<float>(std::max(ox, oy));
            b.H(x, y) = out <= 0.0f ? h0 : h0 + (b.H(x, y) - h0) * Smooth(out / static_cast<float>(ramp));
            b.Reserved[b.I(x, y)] = 1;
        }
}

bool FarFromFeatures(const WorldLayout& L, WaterCell c, float fromHome, float fromOutpost, float fromVault)
{
    if (Dist(c, L.Home) < fromHome) return false;
    for (const auto& o : L.Outposts) if (Dist(c, o.Center) < fromOutpost) return false;
    for (const auto& v : L.Vaults) if (Dist(c, v.Cell) < fromVault) return false;
    return true;
}

bool StampOutpost(Builder& b, Rng& rng, int id, bool relaxed = false)
{
    WorldLayout& L = b.L;
    const float featureScale = relaxed ? 0.7f : 1.0f;
    for (int attempt = 0; attempt < (relaxed ? 3000 : 1200); ++attempt)
    {
        const auto& river = L.Rivers[static_cast<std::size_t>(rng.Range(0, static_cast<int>(L.Rivers.size()) - 1))];
        const int n = static_cast<int>(river.Path.size());
        const int i = rng.Range(n / 8, (7 * n) / 8);
        const auto& a = river.Path[static_cast<std::size_t>(std::max(0, i - 3))];
        const auto& z = river.Path[static_cast<std::size_t>(std::min(n - 1, i + 3))];
        float tx = static_cast<float>(z.X - a.X), ty = static_cast<float>(z.Y - a.Y);
        const float len = std::hypot(tx, ty);
        if (len < 1.0f) continue;
        tx /= len; ty /= len;
        const float side = rng.Next() & 1U ? 1.0f : -1.0f;
        const WaterCell p = river.Path[static_cast<std::size_t>(i)];
        const float offset = 12.0f + static_cast<float>(rng.Range(0, relaxed ? 4 : 2));
        const WaterCell center{p.X + static_cast<int>(std::lround(-ty * side * offset)), p.Y + static_cast<int>(std::lround(tx * side * offset))};
        const int margin = 14;
        if (center.X < margin || center.Y < margin || center.X >= L.Width - margin || center.Y >= L.Height - margin) continue;
        if (!FarFromFeatures(L, center, 30.0f * featureScale, 28.0f, 20.0f * featureScale)) continue;
        if (b.MinChannelDistance(center.X, center.Y) < (relaxed ? 10 : 11)) continue;
        // Use the cell's current ground: a later river may have cut this bed deeper where they cross.
        // Springs spill around their head before the channel takes the flow: keep camps clear.
        bool nearSpring = false;
        for (const auto& src : L.Sources) if (Dist(src.first, center) < 17.0f) nearSpring = true;
        if (nearSpring) continue;
        const float padH = std::min(river.Bed[static_cast<std::size_t>(i)], L.GroundAt(p.X, p.Y)) - 1.3f;
        if (padH < 0.15f) continue;
        // The levee must not be crossed by any reserved feature.
        bool clear = true;
        for (int dy = -13; dy <= 13 && clear; ++dy)
            for (int dx = -13; dx <= 13 && clear; ++dx)
                if (L.InBounds(center.X + dx, center.Y + dy) && b.Reserved[b.I(center.X + dx, center.Y + dy)] && dx * dx + dy * dy <= 169) clear = false;
        if (!clear) continue;

        OutpostLayout o;
        o.Id = id;
        o.Center = center;
        o.PadHeight = padH;
        o.RimHeight = padH + 3.0f;
        o.RiverCell = p;
        for (int dy = -13; dy <= 13; ++dy)
            for (int dx = -13; dx <= 13; ++dx)
            {
                const int x = center.X + dx, y = center.Y + dy;
                if (!L.InBounds(x, y) || b.Channel[b.I(x, y)]) continue;
                const float r = std::hypot(static_cast<float>(dx), static_cast<float>(dy));
                if (r > 13.0f) continue;
                float h;
                if (r <= 5.0f) h = padH;
                else if (r <= 9.0f) h = padH + 3.0f * Smooth((r - 5.0f) / 4.0f);
                else h = o.RimHeight + (b.H(x, y) - o.RimHeight) * Smooth((r - 9.0f) / 4.0f);
                // Outside the crest, never dig below the river bank: the levee holds.
                if (r > 9.0f) h = std::max(h, std::min(b.H(x, y), o.RimHeight));
                b.H(x, y) = h;
                b.Reserved[b.I(x, y)] = 1;
            }
        const WaterCell c = center;
        o.Structures = {{OutpostStructureType::Generator, c}, {OutpostStructureType::Stash, {c.X + 2, c.Y - 1}},
                        {OutpostStructureType::Hut, {c.X - 3, c.Y + 2}}, {OutpostStructureType::Hut, {c.X + 3, c.Y + 2}},
                        {OutpostStructureType::Hut, {c.X - 3, c.Y - 2}}, {OutpostStructureType::Hut, {c.X, c.Y + 4}}};
        o.GuardSpawns = {{c.X - 1, c.Y - 3}, {c.X + 1, c.Y + 3}, {c.X - 4, c.Y}, {c.X + 4, c.Y}};
        o.EngineerSpawns = {{c.X - 2, c.Y}, {c.X + 2, c.Y + 1}};
        L.Outposts.push_back(std::move(o));
        return true;
    }
    return false;
}

bool StampVault(Builder& b, Rng& rng, int id, float springRate, bool relaxed = false)
{
    WorldLayout& L = b.L;
    const float featureScale = relaxed ? 0.6f : 1.0f;
    const int channelClearance = relaxed ? 7 : 8;
    for (int attempt = 0; attempt < (relaxed ? 1500 : 400); ++attempt)
    {
        const auto& river = L.Rivers[static_cast<std::size_t>(rng.Range(0, static_cast<int>(L.Rivers.size()) - 1))];
        const int n = static_cast<int>(river.Path.size());
        const int i = rng.Range(n / 6, (5 * n) / 6);
        const auto& a = river.Path[static_cast<std::size_t>(std::max(0, i - 3))];
        const auto& z = river.Path[static_cast<std::size_t>(std::min(n - 1, i + 3))];
        float tx = static_cast<float>(z.X - a.X), ty = static_cast<float>(z.Y - a.Y);
        const float len = std::hypot(tx, ty);
        if (len < 1.0f) continue;
        tx /= len; ty /= len;
        const float side = rng.Next() & 1U ? 1.0f : -1.0f;
        const WaterCell p = river.Path[static_cast<std::size_t>(i)];
        const WaterCell center{p.X + static_cast<int>(std::lround(-ty * side * 9.0f)), p.Y + static_cast<int>(std::lround(tx * side * 9.0f))};
        const int margin = 11;
        if (center.X < margin || center.Y < margin || center.X >= L.Width - margin || center.Y >= L.Height - margin) continue;
        if (!FarFromFeatures(L, center, 26.0f * featureScale, 24.0f * featureScale, 24.0f)) continue;
        if (b.MinChannelDistance(center.X, center.Y) < channelClearance) continue;
        bool clear = true;
        for (int dy = -9; dy <= 9 && clear; ++dy)
            for (int dx = -9; dx <= 9 && clear; ++dx)
                if (L.InBounds(center.X + dx, center.Y + dy) && b.Reserved[b.I(center.X + dx, center.Y + dy)]) clear = false;
        if (!clear) continue;
        // The spillway to the river must not cut through another feature (an outpost levee).
        {
            const float sx0 = static_cast<float>(p.X - center.X), sy0 = static_cast<float>(p.Y - center.Y);
            const float sl0 = std::hypot(sx0, sy0);
            bool blocked = false;
            for (float t = 9.5f; t <= sl0 && !blocked; t += 0.5f)
            {
                const int x = center.X + static_cast<int>(std::lround(sx0 / sl0 * t));
                const int y = center.Y + static_cast<int>(std::lround(sy0 / sl0 * t));
                if (L.InBounds(x, y) && b.Reserved[b.I(x, y)] && !b.Channel[b.I(x, y)]) blocked = true;
            }
            if (blocked) continue;
        }
        VaultLayout v;
        v.Id = id;
        v.Cell = center;
        v.RiverCell = p;
        // High enough that a 1 m canal's vault end still sits above the river's surface.
        v.Floor = std::min(river.Bed[static_cast<std::size_t>(i)], L.GroundAt(p.X, p.Y)) + 2.0f;
        v.SpillLevel = v.Floor + 1.4f;
        const float rimTop = v.Floor + 1.9f;
        for (int dy = -9; dy <= 9; ++dy)
            for (int dx = -9; dx <= 9; ++dx)
            {
                const int x = center.X + dx, y = center.Y + dy;
                if (!L.InBounds(x, y) || b.Channel[b.I(x, y)]) continue;
                const float r = std::hypot(static_cast<float>(dx), static_cast<float>(dy));
                if (r > 9.0f) continue;
                float h;
                if (r <= 5.0f) h = v.Floor + 1.7f * (r / 5.0f) * (r / 5.0f);
                else if (r <= 6.5f) h = rimTop;
                else h = rimTop + (b.H(x, y) - rimTop) * Smooth((r - 6.5f) / 2.5f);
                if (r > 6.5f) h = std::max(h, std::min(b.H(x, y), rimTop));
                b.H(x, y) = h;
                b.Reserved[b.I(x, y)] = 1;
            }
        // Spillway from the bowl edge toward the river at the spill level keeps the bowl full.
        const float sx = static_cast<float>(p.X - center.X), sy = static_cast<float>(p.Y - center.Y);
        const float sl = std::hypot(sx, sy);
        for (float t = 4.0f; t <= sl; t += 0.5f)
        {
            const int x = center.X + static_cast<int>(std::lround(sx / sl * t));
            const int y = center.Y + static_cast<int>(std::lround(sy / sl * t));
            if (L.InBounds(x, y) && !b.Channel[b.I(x, y)]) { b.H(x, y) = std::min(b.H(x, y), v.SpillLevel); b.Reserved[b.I(x, y)] = 1; }
        }
        v.Spring = {center.X - static_cast<int>(std::lround(sx / sl * 3.0f)), center.Y - static_cast<int>(std::lround(sy / sl * 3.0f))};
        L.Sources.push_back({v.Spring, springRate});
        for (int dy = -7; dy <= 7; ++dy)
            for (int dx = -7; dx <= 7; ++dx)
            {
                const int x = center.X + dx, y = center.Y + dy;
                if (L.InBounds(x, y) && dx * dx + dy * dy <= 42)
                    L.Prefill[b.I(x, y)] = std::max(0.0f, v.SpillLevel - b.H(x, y));
            }
        L.Vaults.push_back(v);
        return true;
    }
    return false;
}

bool DryOpenCell(Builder& b, WaterCell c)
{
    const WorldLayout& L = b.L;
    if (c.X < 4 || c.Y < 4 || c.X >= L.Width - 4 || c.Y >= L.Height - 4) return false;
    if (b.Reserved[b.I(c.X, c.Y)] || b.MinChannelDistance(c.X, c.Y) < 4 || b.EdgeDistance(c.X, c.Y) < 8.0f) return false;
    const float h = L.GroundAt(c.X, c.Y);
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
            if (std::abs(L.GroundAt(c.X + dx, c.Y + dy) - h) > 0.5f) return false;
    return true;
}
}

namespace
{
WorldLayout GenerateAttempt(std::uint32_t seed, std::uint32_t terrainSeed, const WorldGenSettings& s)
{
    if (s.Width < 48 || s.Height < 48 || s.Width > 512 || s.Height > 512)
        throw std::invalid_argument("World size must be 48..512 cells");
    WorldLayout L;
    L.Seed = seed;
    L.Width = s.Width;
    L.Height = s.Height;
    L.LowEdge = static_cast<int>(Hash(terrainSeed, 7, 7, 99) % 4U);
    L.Ground.assign(static_cast<std::size_t>(s.Width) * s.Height, 0.0f);
    L.Prefill.assign(L.Ground.size(), 0.0f);
    Builder b(L);
    Rng rng(terrainSeed);
    BaseTerrain(b, terrainSeed);

    // Springs on high ground far from the drainage edge.
    std::vector<WaterCell> springs;
    for (int attempt = 0; attempt < 4000 && static_cast<int>(springs.size()) < s.Rivers; ++attempt)
    {
        WaterCell best{-1, -1};
        float bestH = -1.0f;
        for (int k = 0; k < 60; ++k)
        {
            const WaterCell c{rng.Range(8, s.Width - 9), rng.Range(8, s.Height - 9)};
            if (b.EdgeDistance(c.X, c.Y) / b.EdgeSpan() < 0.7f) continue;
            bool far = true;
            for (const auto& o : springs) if (Dist(o, c) < 30.0f) far = false;
            if (far && L.GroundAt(c.X, c.Y) > bestH) { bestH = L.GroundAt(c.X, c.Y); best = c; }
        }
        if (best.X < 0) continue;
        if (CarveRiver(b, rng, best)) springs.push_back(best);
    }
    if (L.Rivers.empty()) throw std::runtime_error("World generation produced no river");
    for (auto& src : L.Sources) src.second = s.SpringRate;
    for (const auto& r : L.Rivers)
        for (std::size_t i = 0; i < r.Path.size(); ++i)
            L.Prefill[b.I(r.Path[i].X, r.Path[i].Y)] = 0.2f;

    PlaceHome(b);
    for (int i = 0; i < s.Outposts; ++i)
        if (!StampOutpost(b, rng, static_cast<int>(L.Outposts.size())) && L.Outposts.empty())
            StampOutpost(b, rng, 0, true);
    for (int i = 0; i < s.Vaults; ++i)
        if (!StampVault(b, rng, i, s.VaultSpringRate)) StampVault(b, rng, i, s.VaultSpringRate, true);

    for (int attempt = 0; attempt < 3000 && static_cast<int>(L.Caches.size()) < s.Caches; ++attempt)
    {
        const WaterCell c{rng.Range(4, s.Width - 5), rng.Range(4, s.Height - 5)};
        if (!DryOpenCell(b, c) || !FarFromFeatures(L, c, 18.0f, 15.0f, 11.0f)) continue;
        bool spaced = true;
        for (const auto& o : L.Caches) if (Dist(o.Cell, c) < 12.0f) spaced = false;
        if (!spaced) continue;
        L.Caches.push_back({c, 4});
        // A small ruin cluster around each cache: cover and landmarks.
        const int props = rng.Range(2, 4);
        for (int k = 0; k < props; ++k)
        {
            const WaterCell pc{c.X + rng.Range(-3, 3), c.Y + rng.Range(-3, 3)};
            if ((pc.X == c.X && pc.Y == c.Y) || !DryOpenCell(b, pc)) continue;
            L.Props.push_back({pc, static_cast<WorldPropType>(rng.Range(0, 5)), rng.Float() * 360.0f});
        }
    }
    for (int attempt = 0; attempt < 3000 && static_cast<int>(L.Patrols.size()) < s.Patrols; ++attempt)
    {
        const WaterCell c{rng.Range(4, s.Width - 5), rng.Range(4, s.Height - 5)};
        if (!DryOpenCell(b, c) || !FarFromFeatures(L, c, 26.0f, 16.0f, 10.0f)) continue;
        L.Patrols.push_back({c, (L.Patrols.size() % 3) == 2});
    }
    for (int attempt = 0; attempt < 600 && L.Props.size() < 60; ++attempt)
    {
        const WaterCell c{rng.Range(4, s.Width - 5), rng.Range(4, s.Height - 5)};
        if (!DryOpenCell(b, c) || !FarFromFeatures(L, c, 16.0f, 15.0f, 11.0f)) continue;
        L.Props.push_back({c, rng.Float() < 0.6f ? WorldPropType::Rock : WorldPropType::Barrel, rng.Float() * 360.0f});
    }
    for (auto& h : L.Ground) h = std::clamp(h, 0.0f, 12.0f);
    return L;
}
}

WorldLayout GenerateWorld(std::uint32_t seed, const WorldGenSettings& s)
{
    if (s.Width < 48 || s.Height < 48 || s.Width > 512 || s.Height > 512)
        throw std::invalid_argument("World size must be 48..512 cells");
    // Some terrain leaves no room for a camp or vault. Retry with derived terrain seeds and
    // fewer rivers; the sequence depends only on the seed, so worlds stay reproducible.
    WorldLayout best;
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        WorldGenSettings variant = s;
        variant.Rivers = std::max(1, s.Rivers - attempt / 3);
        const std::uint32_t terrainSeed = attempt == 0 ? seed : Hash(seed, attempt, 17, 0xc0ffeeU);
        try
        {
            WorldLayout L = GenerateAttempt(seed, terrainSeed, variant);
            if (!L.Outposts.empty() && !L.Vaults.empty()) return L;
            if (best.Ground.empty()) best = std::move(L);
        }
        catch (const std::runtime_error&) {}
    }
    if (best.Ground.empty()) throw std::runtime_error("World generation failed for this seed");
    return best;
}

WorldLayout MakeDrownedVaultLayout()
{
    using Site = DrownedVaultSite;
    WorldLayout L;
    L.Seed = 0;
    L.Width = Site::Width;
    L.Height = Site::Height;
    L.Ground.resize(static_cast<std::size_t>(L.Width) * L.Height);
    L.Prefill.assign(L.Ground.size(), 0.0f);
    WaterGrid g(DrownedVaultConfig());
    BuildDrownedVaultSite(g, true);
    for (int y = 0; y < L.Height; ++y)
        for (int x = 0; x < L.Width; ++x)
        {
            L.Ground[static_cast<std::size_t>(y) * L.Width + x] = g.Ground(x, y);
            L.Prefill[static_cast<std::size_t>(y) * L.Width + x] = g.Water(x, y);
            if (g.Source(x, y) > 0.0f) L.Sources.push_back({{x, y}, g.Source(x, y)});
            if (g.IsSink(x, y)) L.Sinks.push_back({x, y});
        }
    VaultLayout v;
    v.Id = 0;
    v.Cell = Site::Vault;
    v.RiverCell = Site::CulvertB;
    v.Spring = Site::Spring;
    v.Floor = Site::BasinFloor;
    v.SpillLevel = Site::SpillLevel;
    L.Vaults.push_back(v);
    L.Home = {-1, -1};
    return L;
}

WaterConfig WorldWaterConfig(const WorldLayout& layout)
{
    WaterConfig c;
    c.Width = layout.Width;
    c.Height = layout.Height;
    c.CellSize = 1.0f;
    c.TickSeconds = 0.1;
    c.SubstepsPerTick = 4;
    c.MinimumGround = 0.0f;
    c.MaximumGround = 12.0f;
    c.ChunkSize = 16;
    return c;
}

void ApplyWorld(WaterGrid& grid, const WorldLayout& L, bool prefill)
{
    if (grid.Width() != L.Width || grid.Height() != L.Height) throw std::invalid_argument("Grid does not match layout");
    for (int y = 0; y < L.Height; ++y)
        for (int x = 0; x < L.Width; ++x)
        {
            grid.SetGround(x, y, L.GroundAt(x, y));
            grid.SetSource(x, y, 0.0f);
            grid.SetSink(x, y, false);
        }
    for (const auto& s : L.Sources) grid.SetSource(s.first.X, s.first.Y, s.second);
    for (const auto& s : L.Sinks) grid.SetSink(s.X, s.Y, true);
    if (!prefill) return;
    for (int y = 0; y < L.Height; ++y)
        for (int x = 0; x < L.Width; ++x)
        {
            const float d = L.Prefill[static_cast<std::size_t>(y) * L.Width + x];
            if (d > 0.0f && !grid.IsSink(x, y)) grid.SetWater(x, y, d);
        }
}

// ---- Outposts ---------------------------------------------------------------
OutpostState::OutpostState(const OutpostLayout& layout, OutpostRules rules) : Plan(layout), Rules(rules)
{
    for (const auto& s : Plan.Structures)
    {
        OutpostStructureState st;
        st.Type = s.Type;
        st.Cell = s.Cell;
        st.MaxIntegrity = s.Type == OutpostStructureType::Generator ? Rules.GeneratorIntegrity
                        : s.Type == OutpostStructureType::Stash ? Rules.StashIntegrity : Rules.HutIntegrity;
        st.Integrity = st.MaxIntegrity;
        Parts.push_back(st);
    }
}

float OutpostState::Distance(int x, int y) const
{
    return std::hypot(static_cast<float>(x - Plan.Center.X), static_cast<float>(y - Plan.Center.Y));
}

bool OutpostState::InPad(int x, int y) const { return Distance(x, y) <= static_cast<float>(Plan.PadRadius) + 0.5f; }
bool OutpostState::InLevee(int x, int y) const
{
    const float r = Distance(x, y);
    return r > static_cast<float>(Plan.PadRadius) && r <= static_cast<float>(Plan.RimRadius) + 2.5f;
}

bool OutpostState::IsRazed() const
{
    return std::any_of(Parts.begin(), Parts.end(), [](const OutpostStructureState& s) { return s.Type == OutpostStructureType::Generator && s.Destroyed; });
}

int OutpostState::DestroyedMask() const
{
    int mask = 0;
    for (std::size_t i = 0; i < Parts.size() && i < 30; ++i) if (Parts[i].Destroyed) mask |= 1 << i;
    return mask;
}

bool OutpostState::RestoreDestroyedMask(int mask)
{
    if (mask < 0 || mask >= (1 << static_cast<int>(Parts.size()))) return false;
    for (std::size_t i = 0; i < Parts.size(); ++i)
    {
        Parts[i].Destroyed = (mask >> i) & 1;
        Parts[i].Integrity = Parts[i].Destroyed ? 0.0f : Parts[i].MaxIntegrity;
    }
    return true;
}

float OutpostState::FloodDepth(const WaterGrid& grid) const
{
    float deepest = 0.0f;
    const int r = Plan.PadRadius;
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx)
        {
            const int x = Plan.Center.X + dx, y = Plan.Center.Y + dy;
            if (grid.InBounds(x, y) && InPad(x, y)) deepest = std::max(deepest, grid.Water(x, y));
        }
    return deepest;
}

std::vector<int> OutpostState::ApplyFlood(const WaterGrid& grid, float dt)
{
    std::vector<int> destroyed;
    if (!std::isfinite(dt) || dt <= 0.0f) return destroyed;
    for (std::size_t i = 0; i < Parts.size(); ++i)
    {
        auto& p = Parts[i];
        if (p.Destroyed || !grid.InBounds(p.Cell.X, p.Cell.Y)) continue;
        const float over = grid.Water(p.Cell.X, p.Cell.Y) - Rules.FloodThreshold;
        if (over <= 0.0f) continue;
        p.Integrity -= Rules.DamagePerMetrePerSecond * over * dt;
        if (p.Integrity <= 0.0f)
        {
            p.Integrity = 0.0f;
            p.Destroyed = true;
            destroyed.push_back(static_cast<int>(i));
        }
    }
    return destroyed;
}

double OutpostState::Pump(WaterGrid& grid, float dt)
{
    if (IsRazed() || !std::isfinite(dt) || dt <= 0.0f) return 0.0;
    double budget = static_cast<double>(Rules.PumpRate) * dt;
    double removed = 0.0;
    const int r = Plan.PadRadius;
    // Pump the deepest cells first, cell by cell, until the budget is used.
    std::vector<std::pair<float, WaterCell>> wet;
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx)
        {
            const int x = Plan.Center.X + dx, y = Plan.Center.Y + dy;
            if (grid.InBounds(x, y) && InPad(x, y) && grid.Water(x, y) > 0.0f) wet.push_back({grid.Water(x, y), {x, y}});
        }
    std::sort(wet.begin(), wet.end(), [](const auto& a, const auto& b) { return a.first > b.first || (a.first == b.first && (a.second.Y < b.second.Y || (a.second.Y == b.second.Y && a.second.X < b.second.X))); });
    for (const auto& w : wet)
    {
        if (budget <= 0.0) break;
        const double took = grid.RemoveWater(w.second.X, w.second.Y, budget);
        budget -= took;
        removed += took;
    }
    return removed;
}

bool OutpostState::FindBreach(const WaterGrid& grid, WaterCell& breach) const
{
    float bestScore = -1.0f;
    const int r = Plan.RimRadius + 3;
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx)
        {
            const int x = Plan.Center.X + dx, y = Plan.Center.Y + dy;
            if (!grid.InBounds(x, y) || !InLevee(x, y)) continue;
            // Water can cross this cell if it holds any, or a neighbour's surface stands above its ground.
            bool leaking = grid.Water(x, y) > 0.002f;
            for (const WaterCell n : {WaterCell{x + 1, y}, WaterCell{x - 1, y}, WaterCell{x, y + 1}, WaterCell{x, y - 1}})
                if (!leaking && grid.InBounds(n.X, n.Y) && grid.Water(n.X, n.Y) > 0.01f && grid.Surface(n.X, n.Y) > grid.Ground(x, y) - 0.15f) leaking = true;
            if (!leaking) continue;
            const float d = Distance(x, y);
            // The levee's designed height at this radius; a breach is ground cut well below it.
            const float t = std::clamp((d - 5.0f) / 4.0f, 0.0f, 1.0f);
            const float designed = Plan.PadHeight + (Plan.RimHeight - Plan.PadHeight) * t * t * (3.0f - 2.0f * t);
            const float cut = designed - grid.Ground(x, y);
            if (cut < 0.25f) continue;
            // Damming near the crest blocks the most water for the least fill.
            const float score = cut + 0.5f * d;
            if (score > bestScore) { bestScore = score; breach = {x, y}; }
        }
    return bestScore >= 0.0f;
}

bool OutpostState::Damage(int index, float amount)
{
    if (index < 0 || index >= static_cast<int>(Parts.size()) || !std::isfinite(amount) || amount <= 0.0f) return false;
    auto& p = Parts[static_cast<std::size_t>(index)];
    if (p.Destroyed) return false;
    p.Integrity -= amount;
    if (p.Integrity > 0.0f) return false;
    p.Integrity = 0.0f;
    p.Destroyed = true;
    return true;
}

double OutpostState::DamBreach(WaterGrid& grid, WaterCell cell) const
{
    if (!grid.InBounds(cell.X, cell.Y) || !InLevee(cell.X, cell.Y)) return 0.0;
    return grid.RaiseGround(cell.X, cell.Y, Rules.DamStep, Plan.RimHeight);
}
}
