#include "astra/ruin_decay.h"

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

    // 1. Extra wall collapse
    for (int y = footprint.y; y < footprint.y + footprint.h; ++y) {
        for (int x = footprint.x; x < footprint.x + footprint.w; ++x) {
            Tile t = map.get(x, y);
            if (!is_wall(t)) continue;

            if (chance(rng) < ctx.age_decay) {
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

} // namespace astra
