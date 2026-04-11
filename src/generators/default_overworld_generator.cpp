#include "astra/overworld_generator.h"
#include "astra/lore_influence_map.h"
#include "astra/map_properties.h"
#include "astra/overworld_stamps.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace astra {

// ---------------------------------------------------------------------------
// place_default_pois — shared free function for POI placement
// ---------------------------------------------------------------------------

void place_default_pois(TileMap* map, const MapProperties* props,
                        const std::vector<float>& /*elevation*/, std::mt19937& rng) {
    int w = map->width();
    int h = map->height();

    // Classify world type
    bool habitable = props->body_type == BodyType::Terrestrial &&
        (props->body_atmosphere == Atmosphere::Standard ||
         props->body_atmosphere == Atmosphere::Dense);
    bool marginal = props->body_type == BodyType::Terrestrial &&
        (props->body_atmosphere == Atmosphere::Thin ||
         props->body_atmosphere == Atmosphere::Toxic ||
         props->body_atmosphere == Atmosphere::Reducing);
    bool airless = (props->body_type == BodyType::Rocky ||
                    props->body_type == BodyType::DwarfPlanet) ||
                   (props->body_type == BodyType::Terrestrial &&
                    props->body_atmosphere == Atmosphere::None);
    bool asteroid = props->body_type == BodyType::AsteroidBelt;

    // Collect candidate anchor positions
    struct Pos { int x, y; };
    std::vector<Pos> candidates;
    for (int y = 2; y < h - 2; ++y) {
        for (int x = 2; x < w - 2; ++x) {
            Tile t = map->get(x, y);
            if (t != Tile::OW_Mountains && t != Tile::OW_Lake &&
                t != Tile::OW_River &&
                t != Tile::OW_Beacon && t != Tile::OW_Megastructure) {
                if (props->lore_influence &&
                    props->lore_influence->landmark_at(x, y) != LandmarkType::None)
                    continue;
                candidates.push_back({x, y});
            }
        }
    }
    if (candidates.empty()) return;
    std::shuffle(candidates.begin(), candidates.end(), rng);

    std::vector<Pos> placed;

    auto too_close = [&](int px, int py) {
        for (const auto& p : placed) {
            if (std::abs(p.x - px) + std::abs(p.y - py) < 8) return true;
        }
        return false;
    };

    auto stamp_fits = [&](const StampDef& stamp, int ax, int ay) -> bool {
        for (int i = 0; i < stamp.cell_count; ++i) {
            int cx = ax + stamp.cells[i].dx;
            int cy = ay + stamp.cells[i].dy;
            if (cx < 0 || cx >= w || cy < 0 || cy >= h) return false;
            Tile t = map->get(cx, cy);
            if (t == Tile::OW_Mountains || t == Tile::OW_Lake ||
                t == Tile::OW_River) return false;
            if (t >= Tile::OW_CaveEntrance && t <= Tile::OW_Outpost) return false;
        }
        return true;
    };

    auto place_stamp = [&](const StampDef& stamp, int ax, int ay) {
        for (int i = 0; i < stamp.cell_count; ++i) {
            int cx = ax + stamp.cells[i].dx;
            int cy = ay + stamp.cells[i].dy;
            map->set(cx, cy, stamp.cells[i].tile);
            if (stamp.cells[i].glyph_index != SG_None) {
                map->set_glyph_override(cx, cy, stamp.cells[i].glyph_index);
            }
        }
        placed.push_back({ax, ay});
    };

    auto try_place = [&](const StampDef* pool, int pool_size,
                         int max_stamp_idx = -1) -> bool {
        int limit = (max_stamp_idx < 0) ? pool_size : std::min(max_stamp_idx + 1, pool_size);
        std::vector<int> stamp_order(limit);
        for (int i = 0; i < limit; ++i) stamp_order[i] = i;
        std::shuffle(stamp_order.begin(), stamp_order.end(), rng);

        for (const auto& c : candidates) {
            if (too_close(c.x, c.y)) continue;
            for (int si : stamp_order) {
                if (stamp_fits(pool[si], c.x, c.y)) {
                    place_stamp(pool[si], c.x, c.y);
                    return true;
                }
            }
        }
        return false;
    };

    auto place_n = [&](const StampDef* pool, int pool_size, int count,
                       int max_stamp_idx = -1) {
        for (int i = 0; i < count; ++i) {
            try_place(pool, pool_size, max_stamp_idx);
        }
    };

    std::uniform_int_distribution<int> pct(0, 99);

    // --- Settlements ---
    if (habitable) {
        if (!try_place(settlement_stamps, settlement_stamp_count, -1)) {
            try_place(settlement_stamps, settlement_stamp_count, 0);
        }
        int extra = std::uniform_int_distribution<int>(0, 2)(rng);
        place_n(settlement_stamps, settlement_stamp_count, extra);
    } else if (marginal) {
        if (pct(rng) < 40) {
            int count = std::uniform_int_distribution<int>(1, 1)(rng);
            place_n(settlement_stamps, settlement_stamp_count, count, 1);
        }
    }

    // --- Space habitats ---
    if (airless && !asteroid && pct(rng) < 40) {
        int count = std::uniform_int_distribution<int>(1, 2)(rng);
        place_n(habitat_stamps, habitat_stamp_count, count);
    }

    // --- Caves ---
    if (props->body_has_dungeon) {
        int count = std::uniform_int_distribution<int>(2, 5)(rng);

        std::vector<Pos> mountain_edge;
        std::vector<Pos> water_edge;
        for (const auto& c : candidates) {
            Tile t = map->get(c.x, c.y);
            if (t >= Tile::OW_CaveEntrance && t <= Tile::OW_GlassedCrater) continue;
            bool adj_mountain = false, adj_water = false;
            static const int dx4[] = {0, 0, -1, 1};
            static const int dy4[] = {-1, 1, 0, 0};
            for (int d = 0; d < 4; ++d) {
                Tile nb = map->get(c.x + dx4[d], c.y + dy4[d]);
                if (nb == Tile::OW_Mountains) adj_mountain = true;
                if (nb == Tile::OW_Lake) adj_water = true;
            }
            if (adj_mountain) mountain_edge.push_back(c);
            if (adj_water) water_edge.push_back(c);
        }
        std::shuffle(mountain_edge.begin(), mountain_edge.end(), rng);
        std::shuffle(water_edge.begin(), water_edge.end(), rng);

        int placed_caves = 0;

        int mountain_caves = std::max(1, count * 3 / 5);
        for (const auto& c : mountain_edge) {
            if (placed_caves >= mountain_caves) break;
            if (too_close(c.x, c.y)) continue;
            std::uniform_int_distribution<int> si(0, cave_stamp_count - 1);
            int idx = si(rng);
            if (stamp_fits(cave_stamps[idx], c.x, c.y)) {
                place_stamp(cave_stamps[idx], c.x, c.y);
                ++placed_caves;
            }
        }

        int water_caves = std::max(0, count / 5);
        for (const auto& c : water_edge) {
            if (placed_caves >= mountain_caves + water_caves) break;
            if (too_close(c.x, c.y)) continue;
            std::uniform_int_distribution<int> si(0, cave_stamp_count - 1);
            int idx = si(rng);
            if (stamp_fits(cave_stamps[idx], c.x, c.y)) {
                place_stamp(cave_stamps[idx], c.x, c.y);
                ++placed_caves;
            }
        }

        int remaining = count - placed_caves;
        if (remaining > 0) {
            place_n(cave_stamps, cave_stamp_count, remaining);
        }
    }

    // --- Ruins ---
    {
        int count = 0;
        if (props->body_danger_level >= 3) {
            count = std::uniform_int_distribution<int>(1, 4)(rng);
        } else if (pct(rng) < 30) {
            count = std::uniform_int_distribution<int>(1, 2)(rng);
        }
        if (props->lore_tier >= 3)
            count += std::uniform_int_distribution<int>(4, 6)(rng);
        else if (props->lore_tier >= 2)
            count += std::uniform_int_distribution<int>(2, 4)(rng);
        else if (props->lore_tier >= 1)
            count += std::uniform_int_distribution<int>(1, 2)(rng);

        if (count > 0) {
            int max_idx = (asteroid || airless) ? 1 : -1;
            place_n(ruin_stamps, ruin_stamp_count, count, max_idx);
        }
    }

    // --- Crashed ships ---
    {
        int base_chance = 20 + props->body_danger_level * 10;
        if (props->lore_battle_site) base_chance = 100;
        if (pct(rng) < base_chance) {
            int count = std::uniform_int_distribution<int>(1, 3)(rng);
            if (props->lore_battle_site)
                count += std::uniform_int_distribution<int>(2, 4)(rng);
            int max_idx = (asteroid) ? 1 : -1;
            place_n(crashed_ship_stamps, crashed_ship_stamp_count, count, max_idx);
        }
    }

    // --- Outposts ---
    {
        int chance = 30;
        if (props->lore_tier >= 2) chance = 70;
        if (props->lore_plague_origin) chance = 80;
        if (pct(rng) < chance) {
            int count = std::uniform_int_distribution<int>(1, 2)(rng);
            if (props->lore_tier >= 2)
                count += std::uniform_int_distribution<int>(1, 2)(rng);
            int max_idx = (asteroid) ? 0 : -1;
            place_n(outpost_stamps, outpost_stamp_count, count, max_idx);
        }
    }

    // --- Lore weapon test sites ---
    if (props->lore_weapon_test && props->body_has_dungeon) {
        int count = std::uniform_int_distribution<int>(2, 4)(rng);
        place_n(cave_stamps, cave_stamp_count, count);
    }
}

// ---------------------------------------------------------------------------
// DefaultOverworldGenerator — ports all current overworld generation logic.
// Used as the fallback for all body types without a dedicated generator.
// ---------------------------------------------------------------------------

class DefaultOverworldGenerator : public OverworldGeneratorBase {
protected:
    void configure_noise(float& elev_scale, float& moist_scale,
                         const TerrainContext& ctx) override;
    Tile classify_terrain(int x, int y, float elev, float moist,
                          const TerrainContext& ctx) override;
    void carve_rivers(std::mt19937& rng) override;
    void place_pois(std::mt19937& rng) override;

private:
    // Classifier helpers
    static bool has_atmosphere(const TerrainContext& ctx);
    static bool is_terrestrial_with_atmo(const TerrainContext& ctx);
    static bool is_rocky_no_atmo(const TerrainContext& ctx);
    Tile classify_terrestrial(float elev, float moist, const TerrainContext& ctx);
    static Tile classify_rocky(float elev);
    static Tile classify_asteroid(float elev);
};

// ---------------------------------------------------------------------------
// Classifier helpers
// ---------------------------------------------------------------------------

bool DefaultOverworldGenerator::has_atmosphere(const TerrainContext& ctx) {
    return ctx.atmosphere != Atmosphere::None;
}

bool DefaultOverworldGenerator::is_terrestrial_with_atmo(const TerrainContext& ctx) {
    return ctx.body_type == BodyType::Terrestrial && has_atmosphere(ctx);
}

bool DefaultOverworldGenerator::is_rocky_no_atmo(const TerrainContext& ctx) {
    return ctx.body_type == BodyType::Rocky ||
           (ctx.body_type == BodyType::Terrestrial && !has_atmosphere(ctx));
}

// ---------------------------------------------------------------------------
// configure_noise
// ---------------------------------------------------------------------------

void DefaultOverworldGenerator::configure_noise(float& elev_scale, float& moist_scale,
                                                 const TerrainContext& ctx) {
    elev_scale = 0.08f;
    moist_scale = 0.12f;

    if (ctx.body_type == BodyType::AsteroidBelt || ctx.body_type == BodyType::DwarfPlanet) {
        elev_scale = 0.2f;
    }
}

// ---------------------------------------------------------------------------
// classify_terrain — dispatches to body-type-specific classifiers
// ---------------------------------------------------------------------------

Tile DefaultOverworldGenerator::classify_terrain(int /*x*/, int /*y*/, float elev, float moist,
                                                  const TerrainContext& ctx) {
    if (is_terrestrial_with_atmo(ctx)) {
        return classify_terrestrial(elev, moist, ctx);
    } else if (ctx.body_type == BodyType::AsteroidBelt) {
        return classify_asteroid(elev);
    } else if (is_rocky_no_atmo(ctx) || ctx.body_type == BodyType::DwarfPlanet) {
        return classify_rocky(elev);
    } else {
        return classify_rocky(elev);
    }
}

Tile DefaultOverworldGenerator::classify_terrestrial(float elev, float moist,
                                                      const TerrainContext& ctx) {
    Tile base;
    if (elev > 0.72f) {
        base = Tile::OW_Mountains;
    } else if (elev < 0.25f) {
        base = Tile::OW_Lake;
    } else if (elev < 0.35f && moist > 0.5f) {
        base = Tile::OW_Swamp;
    } else if (moist > 0.6f) {
        base = Tile::OW_Forest;
    } else if (moist > 0.3f) {
        base = Tile::OW_Plains;
    } else {
        base = Tile::OW_Desert;
    }

    switch (ctx.temperature) {
        case Temperature::Frozen:
            if (base == Tile::OW_Plains || base == Tile::OW_Forest ||
                base == Tile::OW_Swamp)
                return Tile::OW_IceField;
            if (base == Tile::OW_Lake) return Tile::OW_IceField;
            break;
        case Temperature::Cold:
            if (base == Tile::OW_Swamp) return Tile::OW_Plains;
            if (base == Tile::OW_Forest && moist < 0.7f) return Tile::OW_Plains;
            if (base == Tile::OW_Plains && elev > 0.6f) return Tile::OW_IceField;
            break;
        case Temperature::Temperate:
            break;
        case Temperature::Hot:
            if (base == Tile::OW_Forest) return Tile::OW_Desert;
            if (base == Tile::OW_Swamp) return Tile::OW_Desert;
            if (base == Tile::OW_Lake && ctx.atmosphere != Atmosphere::Dense)
                return Tile::OW_Desert;
            break;
        case Temperature::Scorching:
            if (base == Tile::OW_Lake || base == Tile::OW_River ||
                base == Tile::OW_Swamp || base == Tile::OW_Forest)
                return Tile::OW_Desert;
            if (base == Tile::OW_Plains && elev < 0.4f)
                return Tile::OW_LavaFlow;
            break;
    }

    if (ctx.atmosphere == Atmosphere::Toxic || ctx.atmosphere == Atmosphere::Reducing) {
        if (base == Tile::OW_Forest) return Tile::OW_Fungal;
        if (base == Tile::OW_Plains && moist > 0.5f) return Tile::OW_Swamp;
    }

    if (ctx.atmosphere == Atmosphere::Thin) {
        if (base == Tile::OW_Forest) return Tile::OW_Plains;
        if (base == Tile::OW_Lake && moist < 0.7f) return Tile::OW_Plains;
        if (base == Tile::OW_Swamp) return Tile::OW_Plains;
    }

    return base;
}

Tile DefaultOverworldGenerator::classify_rocky(float elev) {
    if (elev > 0.72f) return Tile::OW_Mountains;
    if (elev > 0.55f) return Tile::OW_Crater;
    // No grass on airless/rocky bodies — just barren rocky wastes
    return Tile::OW_Barren;
}

Tile DefaultOverworldGenerator::classify_asteroid(float elev) {
    if (elev > 0.65f) return Tile::OW_Mountains;
    if (elev > 0.45f) return Tile::OW_Crater;
    return Tile::OW_Barren;
}

// ---------------------------------------------------------------------------
// carve_rivers
// ---------------------------------------------------------------------------

void DefaultOverworldGenerator::carve_rivers(std::mt19937& rng) {
    if (!is_terrestrial_with_atmo(ctx_)) return;
    if (ctx_.temperature == Temperature::Frozen ||
        ctx_.temperature == Temperature::Scorching) return;

    int w = map_->width();
    int h = map_->height();

    struct Pos { int x, y; };
    std::vector<Pos> sources;
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            float e = elevation_[y * w + x];
            if (e < 0.55f || e > 0.72f) continue;
            Tile t = map_->get(x, y);
            if (t == Tile::OW_Mountains || t == Tile::OW_Lake) continue;

            bool adj_mountain = false;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    if (map_->get(x + dx, y + dy) == Tile::OW_Mountains)
                        adj_mountain = true;
                }
            }
            if (adj_mountain) sources.push_back({x, y});
        }
    }

    if (sources.empty()) return;

    std::shuffle(sources.begin(), sources.end(), rng);
    std::uniform_int_distribution<int> river_count(2, std::min(4, static_cast<int>(sources.size())));
    int num_rivers = river_count(rng);

    for (int r = 0; r < num_rivers && r < static_cast<int>(sources.size()); ++r) {
        int cx = sources[r].x;
        int cy = sources[r].y;
        std::vector<std::vector<bool>> visited(h, std::vector<bool>(w, false));

        for (int step = 0; step < 40; ++step) {
            if (cx <= 0 || cx >= w - 1 || cy <= 0 || cy >= h - 1) break;

            Tile cur = map_->get(cx, cy);
            if (cur == Tile::OW_Lake || cur == Tile::OW_River) break;
            if (cur != Tile::OW_Mountains) {
                map_->set(cx, cy, Tile::OW_River);
            }
            visited[cy][cx] = true;

            float best_elev = elevation_[cy * w + cx];
            int bx = -1, by = -1;
            static const int dx4[] = {0, 0, -1, 1};
            static const int dy4[] = {-1, 1, 0, 0};

            for (int d = 0; d < 4; ++d) {
                int nx = cx + dx4[d];
                int ny = cy + dy4[d];
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                if (visited[ny][nx]) continue;
                if (map_->get(nx, ny) == Tile::OW_Mountains) continue;
                float ne = elevation_[ny * w + nx];
                if (ne < best_elev) {
                    best_elev = ne;
                    bx = nx;
                    by = ny;
                }
            }

            if (bx < 0) {
                float cur_elev = elevation_[cy * w + cx];
                for (int d = 0; d < 4; ++d) {
                    int nx = cx + dx4[d];
                    int ny = cy + dy4[d];
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                    if (visited[ny][nx]) continue;
                    if (map_->get(nx, ny) == Tile::OW_Mountains) continue;
                    float ne = elevation_[ny * w + nx];
                    if (std::abs(ne - cur_elev) < 0.05f) {
                        bx = nx;
                        by = ny;
                        break;
                    }
                }
            }

            if (bx < 0) break;
            cx = bx;
            cy = by;
        }
    }
}

// ---------------------------------------------------------------------------
// place_pois — delegates to shared free function
// ---------------------------------------------------------------------------

void DefaultOverworldGenerator::place_pois(std::mt19937& rng) {
    place_default_pois(map_, props_, elevation_, rng);
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

// Forward declarations
std::unique_ptr<MapGenerator> make_temperate_overworld_generator();
std::unique_ptr<MapGenerator> make_cold_rocky_overworld_generator();

std::unique_ptr<MapGenerator> make_overworld_generator(const MapProperties& props) {
    // Temperate terrestrial planets (Earth-like)
    if (props.body_type == BodyType::Terrestrial &&
        props.body_temperature == Temperature::Temperate &&
        (props.body_atmosphere == Atmosphere::Standard ||
         props.body_atmosphere == Atmosphere::Dense)) {
        return make_temperate_overworld_generator();
    }
    // Cold rocky planets with thin atmosphere (Mars-like)
    if (props.body_type == BodyType::Rocky &&
        props.body_temperature == Temperature::Cold &&
        props.body_atmosphere == Atmosphere::Thin) {
        return make_cold_rocky_overworld_generator();
    }
    return std::make_unique<DefaultOverworldGenerator>();
}

} // namespace astra
