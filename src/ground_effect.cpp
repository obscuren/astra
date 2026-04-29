#include "astra/ground_effect.h"

#include "astra/game.h"
#include "astra/tilemap.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <cstdlib>

namespace astra {

namespace {

constexpr GroundEffectDef kDefs[] = {
    /* Smoke */ { /*radius*/2, /*center_ttl*/24, /*ring_falloff*/6, /*blocks_vision*/true },
};

// Standard Bresenham line from (x0,y0) → (x1,y1). Returns false if any
// *intermediate* tile (not the endpoints) is opaque per TileMap::opaque
// (i.e. wall, StructuralWall, or vision-blocking fixture).
bool line_of_sight_walls_only(const TileMap& map, int x0, int y0, int x1, int y1) {
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    int x = x0, y = y0;
    while (x != x1 || y != y1) {
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx)  { err += dx; y += sy; }
        if (x == x1 && y == y1) break;
        if (map.opaque(x, y)) return false;
    }
    return true;
}

void upsert(std::vector<GroundEffect>& effects,
            GroundEffectKind kind, int x, int y, int ttl) {
    for (auto& ge : effects) {
        if (ge.kind == kind && ge.x == x && ge.y == y) {
            if (ttl > ge.ttl) ge.ttl = ttl;
            return;
        }
    }
    GroundEffect ge;
    ge.kind = kind;
    ge.x = x;
    ge.y = y;
    ge.ttl = ttl;
    ge.origin_id = 0;
    effects.push_back(ge);
}

} // namespace

const GroundEffectDef& ground_effect_def_for(GroundEffectKind k) {
    return kDefs[static_cast<int>(k)];
}

void stamp_ground_effect(Game& game, GroundEffectKind kind, int ix, int iy) {
    const GroundEffectDef& def = ground_effect_def_for(kind);
    auto& effects = game.world().ground_effects();
    const TileMap& map = game.world().map();

    for (int dy = -def.radius; dy <= def.radius; ++dy) {
        for (int dx = -def.radius; dx <= def.radius; ++dx) {
            int tx = ix + dx;
            int ty = iy + dy;
            if (tx < 0 || ty < 0 || tx >= map.width() || ty >= map.height()) continue;
            if (!(dx == 0 && dy == 0) &&
                !line_of_sight_walls_only(map, ix, iy, tx, ty)) continue;
            int ring = std::max(std::abs(dx), std::abs(dy));
            int ttl = def.center_ttl - ring * def.ring_falloff;
            if (ttl < 1) ttl = 1;
            upsert(effects, kind, tx, ty, ttl);
        }
    }
}

void tick_ground_effects(Game& game) {
    auto& effects = game.world().ground_effects();
    for (auto& ge : effects) ge.ttl -= 1;
    effects.erase(
        std::remove_if(effects.begin(), effects.end(),
                       [](const GroundEffect& ge) { return ge.ttl <= 0; }),
        effects.end());
}

std::unordered_set<uint64_t> opaque_ground_effect_tiles(const Game& game) {
    std::unordered_set<uint64_t> out;
    const auto& effects = game.world().ground_effects();
    out.reserve(effects.size());
    for (const auto& ge : effects) {
        const auto& def = ground_effect_def_for(ge.kind);
        if (!def.blocks_vision) continue;
        uint64_t key = (uint64_t(uint32_t(ge.x)) << 32) | uint32_t(ge.y);
        out.insert(key);
    }
    return out;
}

} // namespace astra
