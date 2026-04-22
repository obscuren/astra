#include "astra/ruin_decay.h"
#include "astra/noise.h"

#include <algorithm>

namespace astra {

namespace {

bool is_wall(Tile t) {
    return t == Tile::Wall || t == Tile::StructuralWall;
}

bool adjacent_to_wall(const TileMap& map, int x, int y) {
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            if (is_wall(map.get(x + dx, y + dy))) return true;
        }
    }
    return false;
}

bool is_floor(Tile t) {
    return t == Tile::Floor || t == Tile::IndoorFloor;
}

} // namespace

void RuinDecay::apply(TileMap& map, const Rect& footprint,
                      const DecayContext& ctx, Biome biome,
                      std::mt19937& rng) const {
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);

    auto effective_decay = [&](int x, int y) -> float {
        if (!ctx.use_gradient) return ctx.age_decay;

        const auto& gf = ctx.gradient_footprint;
        float dx_left  = static_cast<float>(x - gf.x);
        float dx_right = static_cast<float>(gf.x + gf.w - 1 - x);
        float dy_top   = static_cast<float>(y - gf.y);
        float dy_bot   = static_cast<float>(gf.y + gf.h - 1 - y);
        float min_edge = std::min({dx_left, dx_right, dy_top, dy_bot});
        float max_dim  = static_cast<float>(std::max(gf.w, gf.h)) * 0.5f;
        float depth_ratio = std::clamp(min_edge / max_dim, 0.0f, 1.0f);

        // Gradient: edges (depth_ratio=0) get high decay, center gets low
        float gradient_decay = ctx.age_decay * (1.5f - depth_ratio);

        // Sectoral variance
        if (ctx.use_sectoral) {
            float sector_noise = hash_noise(x / 20, y / 20,
                static_cast<unsigned>(gf.x * 7919 + gf.y * 104729));
            float jitter = (sector_noise - 0.5f) * 2.0f * ctx.sectoral_variance;
            gradient_decay += jitter;
        }

        return std::clamp(gradient_decay, 0.0f, 0.95f);
    };

    // 1. Extra wall collapse
    for (int y = footprint.y; y < footprint.y + footprint.h; ++y) {
        for (int x = footprint.x; x < footprint.x + footprint.w; ++x) {
            Tile t = map.get(x, y);
            if (!is_wall(t)) continue;

            if (chance(rng) < effective_decay(x, y)) {
                map.set(x, y, Tile::Floor);
                map.add_fixture(x, y, make_fixture(FixtureType::Debris));
            }
        }
    }

    // 2. Rubble near walls (~5% of floor tiles adjacent to a wall)
    for (int y = footprint.y; y < footprint.y + footprint.h; ++y) {
        for (int x = footprint.x; x < footprint.x + footprint.w; ++x) {
            Tile t = map.get(x, y);
            if (!is_floor(t)) continue;
            if (map.fixture_id(x, y) >= 0) continue;
            if (!adjacent_to_wall(map, x, y)) continue;

            if (chance(rng) < 0.05f) {
                map.add_fixture(x, y, make_fixture(FixtureType::Debris));
            }
        }
    }

    // 3. Overgrowth on indoor floor tiles without fixtures
    // Determine density and flora distribution by biome
    float density = 0.0f;

    struct FloraEntry {
        FixtureType type;
        float cumulative; // cumulative probability threshold
    };

    std::vector<FloraEntry> flora;

    switch (biome) {
        case Biome::Jungle:
            density = 0.20f;
            flora = {
                {FixtureType::FloraGrass,  0.50f},
                {FixtureType::FloraHerb,   0.80f},
                {FixtureType::FloraFlower, 1.00f},
            };
            break;
        case Biome::Forest:
            density = 0.12f;
            flora = {
                {FixtureType::FloraGrass,    0.40f},
                {FixtureType::FloraMushroom, 0.75f},
                {FixtureType::FloraHerb,     1.00f},
            };
            break;
        case Biome::Grassland:
            density = 0.10f;
            flora = {
                {FixtureType::FloraGrass,  0.60f},
                {FixtureType::FloraFlower, 1.00f},
            };
            break;
        case Biome::Marsh:
            density = 0.15f;
            flora = {
                {FixtureType::FloraGrass, 0.60f},
                {FixtureType::FloraHerb,  1.00f},
            };
            break;
        case Biome::Rocky:
            density = 0.03f;
            flora = {
                {FixtureType::FloraLichen, 1.00f},
            };
            break;
        case Biome::Ice:
            density = 0.02f;
            flora = {
                {FixtureType::FloraLichen, 1.00f},
            };
            break;
        case Biome::Sandy:
            density = 0.03f;
            flora = {
                {FixtureType::FloraGrass, 1.00f},
            };
            break;
        case Biome::Volcanic:
            density = 0.0f;
            break;
        default:
            // Other biomes: modest overgrowth
            density = 0.05f;
            flora = {
                {FixtureType::FloraGrass, 0.70f},
                {FixtureType::FloraHerb,  1.00f},
            };
            break;
    }

    if (density > 0.0f && !flora.empty()) {
        for (int y = footprint.y; y < footprint.y + footprint.h; ++y) {
            for (int x = footprint.x; x < footprint.x + footprint.w; ++x) {
                if (map.get(x, y) != Tile::IndoorFloor) continue;
                if (map.fixture_id(x, y) >= 0) continue;

                if (chance(rng) < density) {
                    float roll = chance(rng);
                    for (auto& fe : flora) {
                        if (roll <= fe.cumulative) {
                            map.add_fixture(x, y, make_fixture(fe.type));
                            break;
                        }
                    }
                }
            }
        }
    }

    // 4. Wall tinting for surviving walls
    for (int y = footprint.y; y < footprint.y + footprint.h; ++y) {
        for (int x = footprint.x; x < footprint.x + footprint.w; ++x) {
            if (is_wall(map.get(x, y))) {
                map.set_custom_flag(x, y, CF_RUIN_TINT);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// apply_decay — plan-free entry point for the dungeon decoration pipeline.
//
// Builds a flat DecayContext from `intensity` (no gradient, no sectoral
// variance) and applies it across the entire map footprint.  The biome is
// fixed to Dungeon so that the overgrowth pass is kept at a minimal level
// appropriate for underground / interior tile grids.
// ---------------------------------------------------------------------------
void apply_decay(TileMap& map, const CivConfig& /*civ*/,
                 float intensity, std::mt19937& rng) {
    DecayContext ctx;
    ctx.age_decay      = 0.12f * std::clamp(intensity, 0.0f, 1.0f);
    ctx.use_gradient   = false;
    ctx.use_sectoral   = false;

    Rect footprint{0, 0, map.width(), map.height()};
    RuinDecay decay;
    decay.apply(map, footprint, ctx, Biome::Dungeon, rng);
}

} // namespace astra
