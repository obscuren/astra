#include "astra/ruin_types.h"
#include "astra/noise.h"

#include <algorithm>
#include <cmath>

namespace astra {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

bool is_wall(Tile t) {
    return t == Tile::Wall || t == Tile::StructuralWall;
}

bool is_floor(Tile t) {
    return t == Tile::Floor || t == Tile::IndoorFloor || t == Tile::Path;
}

// ---------------------------------------------------------------------------
// BattleScarred — blast craters that destroy walls and leave debris
// ---------------------------------------------------------------------------

void stamp_battle_scarred(TileMap& map, const Rect& fp,
                          float intensity, std::mt19937& rng) {
    int count = 2 + static_cast<int>(intensity * 3.0f);
    std::uniform_int_distribution<int> dx(fp.x + 5, fp.x + fp.w - 6);
    std::uniform_int_distribution<int> dy(fp.y + 5, fp.y + fp.h - 6);

    // Guard: footprint too small for craters
    if (fp.w < 12 || fp.h < 12) return;

    for (int i = 0; i < count; ++i) {
        int cx = dx(rng);
        int cy = dy(rng);
        float radius = 3.0f + intensity * 4.0f;
        int r = static_cast<int>(std::ceil(radius));

        for (int y = cy - r; y <= cy + r; ++y) {
            for (int x = cx - r; x <= cx + r; ++x) {
                if (x < fp.x || x >= fp.x + fp.w ||
                    y < fp.y || y >= fp.y + fp.h)
                    continue;

                float dist = std::sqrt(static_cast<float>((x - cx) * (x - cx) +
                                                           (y - cy) * (y - cy)));
                if (dist > radius) continue;

                Tile t = map.get(x, y);
                float norm = dist / radius;

                if (norm < 0.5f) {
                    // Inner ring: destroy walls, scatter debris
                    if (is_wall(t)) {
                        map.set(x, y, Tile::Floor);
                        map.remove_fixture(x, y);
                    }
                    if (is_floor(map.get(x, y)) && map.fixture_id(x, y) == -1) {
                        std::uniform_real_distribution<float> chance(0.0f, 1.0f);
                        if (chance(rng) < 0.4f) {
                            map.add_fixture(x, y, make_fixture(FixtureType::Debris));
                        }
                    }
                } else if (norm < 0.8f) {
                    // Outer ring: debris on existing floors
                    if (is_floor(t) && map.fixture_id(x, y) == -1) {
                        std::uniform_real_distribution<float> chance(0.0f, 1.0f);
                        if (chance(rng) < 0.3f) {
                            map.add_fixture(x, y, make_fixture(FixtureType::Debris));
                        }
                    }
                }

                // Tint surviving walls in the crater area
                if (is_wall(map.get(x, y))) {
                    map.set_custom_flag(x, y, 0x02);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Infested — organic growths scattered on walkable floors
// ---------------------------------------------------------------------------

void stamp_infested(TileMap& map, const Rect& fp,
                    float intensity, std::mt19937& rng) {
    float density = 0.05f + intensity * 0.10f;
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    std::uniform_real_distribution<float> type_roll(0.0f, 1.0f);

    for (int y = fp.y; y < fp.y + fp.h; ++y) {
        for (int x = fp.x; x < fp.x + fp.w; ++x) {
            Tile t = map.get(x, y);
            if (!is_floor(t)) continue;
            if (map.fixture_id(x, y) != -1) continue;
            if (chance(rng) >= density) continue;

            FixtureType ft = (type_roll(rng) < 0.6f)
                ? FixtureType::FloraMushroom
                : FixtureType::FloraLichen;
            map.add_fixture(x, y, make_fixture(ft));
        }
    }
}

// ---------------------------------------------------------------------------
// Flooded — fbm noise-driven water pools
// ---------------------------------------------------------------------------

void stamp_flooded(TileMap& map, const Rect& fp,
                   float intensity, std::mt19937& rng) {
    unsigned seed = static_cast<unsigned>(fp.x * 7919 + fp.y * 6271);
    float threshold = 0.55f - intensity * 0.15f;

    for (int y = fp.y; y < fp.y + fp.h; ++y) {
        for (int x = fp.x; x < fp.x + fp.w; ++x) {
            Tile t = map.get(x, y);
            if (!is_floor(t)) continue;

            float n = fbm(static_cast<float>(x), static_cast<float>(y),
                          seed, 0.05f, 3);
            if (n < threshold) {
                map.remove_fixture(x, y);
                map.set(x, y, Tile::Water);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Excavated — clean rectangular cuts through the ruin
// ---------------------------------------------------------------------------

void stamp_excavated(TileMap& map, const Rect& fp,
                     float intensity, std::mt19937& rng) {
    int count = 1 + static_cast<int>(intensity * 2.0f);

    for (int i = 0; i < count; ++i) {
        std::uniform_int_distribution<int> wd(8, 15);
        std::uniform_int_distribution<int> hd(6, 10);
        int sw = wd(rng);
        int sh = hd(rng);

        // Clamp section to fit within footprint
        if (sw > fp.w - 2) sw = fp.w - 2;
        if (sh > fp.h - 2) sh = fp.h - 2;
        if (sw < 3 || sh < 3) continue;

        std::uniform_int_distribution<int> sx(fp.x + 1, fp.x + fp.w - sw - 1);
        std::uniform_int_distribution<int> sy(fp.y + 1, fp.y + fp.h - sh - 1);
        int ox = sx(rng);
        int oy = sy(rng);

        for (int y = oy; y < oy + sh; ++y) {
            for (int x = ox; x < ox + sw; ++x) {
                if (is_wall(map.get(x, y))) {
                    map.set(x, y, Tile::Floor);
                }
                map.remove_fixture(x, y);
            }
        }
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public dispatcher
// ---------------------------------------------------------------------------

void apply_ruin_stamps(TileMap& map, const RuinPlan& plan,
                       Biome /*biome*/, std::mt19937& rng) {
    for (const auto& stamp : plan.stamps) {
        switch (stamp.type) {
            case RuinStampType::BattleScarred:
                stamp_battle_scarred(map, plan.footprint, stamp.intensity, rng);
                break;
            case RuinStampType::Infested:
                stamp_infested(map, plan.footprint, stamp.intensity, rng);
                break;
            case RuinStampType::Flooded:
                stamp_flooded(map, plan.footprint, stamp.intensity, rng);
                break;
            case RuinStampType::Excavated:
                stamp_excavated(map, plan.footprint, stamp.intensity, rng);
                break;
        }
    }
}

} // namespace astra
