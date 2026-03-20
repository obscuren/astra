#include "astra/map_generator.h"
#include "astra/map_properties.h"
#include "astra/overworld_stamps.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <vector>

namespace astra {

class OverworldGenerator : public MapGenerator {
protected:
    void generate_layout(std::mt19937& rng) override;
    void connect_rooms(std::mt19937& /*rng*/) override {}
    void place_features(std::mt19937& rng) override;
    void assign_regions(std::mt19937& rng) override;
    void generate_backdrop(unsigned /*seed*/) override {}
};

// --- Value noise + fBm ---

static float hash_noise(int x, int y, unsigned seed) {
    unsigned h = static_cast<unsigned>(x) * 374761393u
               + static_cast<unsigned>(y) * 668265263u
               + seed * 1274126177u;
    h = (h ^ (h >> 13)) * 1103515245u;
    h = h ^ (h >> 16);
    return static_cast<float>(h & 0xFFFFu) / 65535.0f;
}

static float smooth_noise(float fx, float fy, unsigned seed) {
    int ix = static_cast<int>(std::floor(fx));
    int iy = static_cast<int>(std::floor(fy));
    float dx = fx - ix;
    float dy = fy - iy;

    // Smoothstep
    float sx = dx * dx * (3.0f - 2.0f * dx);
    float sy = dy * dy * (3.0f - 2.0f * dy);

    float n00 = hash_noise(ix, iy, seed);
    float n10 = hash_noise(ix + 1, iy, seed);
    float n01 = hash_noise(ix, iy + 1, seed);
    float n11 = hash_noise(ix + 1, iy + 1, seed);

    float top = n00 + sx * (n10 - n00);
    float bot = n01 + sx * (n11 - n01);
    return top + sy * (bot - top);
}

static float fbm(float x, float y, unsigned seed, float scale, int octaves = 4) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float total_amp = 0.0f;
    float freq = scale;

    for (int i = 0; i < octaves; ++i) {
        value += amplitude * smooth_noise(x * freq, y * freq, seed + i * 31u);
        total_amp += amplitude;
        amplitude *= 0.5f;
        freq *= 2.0f;
    }
    return value / total_amp;
}

// --- Terrain classification helpers ---

struct TerrainContext {
    BodyType body_type;
    Atmosphere atmosphere;
    Temperature temperature;
};

static bool has_atmosphere(const TerrainContext& ctx) {
    return ctx.atmosphere != Atmosphere::None;
}

static bool is_terrestrial_with_atmo(const TerrainContext& ctx) {
    return ctx.body_type == BodyType::Terrestrial && has_atmosphere(ctx);
}

static bool is_rocky_no_atmo(const TerrainContext& ctx) {
    return ctx.body_type == BodyType::Rocky ||
           (ctx.body_type == BodyType::Terrestrial && !has_atmosphere(ctx));
}

static Tile classify_terrestrial(float elev, float moist, const TerrainContext& ctx) {
    // Base classification from elevation + moisture
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

    // Temperature modifiers
    switch (ctx.temperature) {
        case Temperature::Frozen:
            if (base == Tile::OW_Plains || base == Tile::OW_Forest ||
                base == Tile::OW_Swamp)
                return Tile::OW_IceField;
            if (base == Tile::OW_Lake) return Tile::OW_IceField; // frozen over
            break;
        case Temperature::Cold:
            if (base == Tile::OW_Swamp) return Tile::OW_Plains;
            if (base == Tile::OW_Forest && moist < 0.7f) return Tile::OW_Plains;
            if (base == Tile::OW_Plains && elev > 0.6f) return Tile::OW_IceField;
            break;
        case Temperature::Temperate:
            break; // default thresholds
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

    // Atmosphere modifiers for toxic/reducing
    if (ctx.atmosphere == Atmosphere::Toxic || ctx.atmosphere == Atmosphere::Reducing) {
        if (base == Tile::OW_Forest) return Tile::OW_Fungal;
        if (base == Tile::OW_Plains && moist > 0.5f) return Tile::OW_Swamp;
    }

    // Thin atmosphere: reduce moisture features
    if (ctx.atmosphere == Atmosphere::Thin) {
        if (base == Tile::OW_Forest) return Tile::OW_Plains;
        if (base == Tile::OW_Lake && moist < 0.7f) return Tile::OW_Plains;
        if (base == Tile::OW_Swamp) return Tile::OW_Plains;
    }

    return base;
}

static Tile classify_rocky(float elev, float /*moist*/) {
    if (elev > 0.72f) return Tile::OW_Mountains;
    if (elev > 0.55f) return Tile::OW_Crater;
    if (elev < 0.3f) return Tile::OW_Desert;
    return Tile::OW_Plains;
}

static Tile classify_asteroid(float elev) {
    if (elev > 0.65f) return Tile::OW_Mountains;
    if (elev > 0.45f) return Tile::OW_Crater;
    return Tile::OW_Plains;
}

// --- River carving ---

struct RiverState {
    int x, y;
};

static void carve_rivers(TileMap* map, const std::vector<float>& elevation,
                         int w, int h, std::mt19937& rng) {
    // Find river sources: mountain-adjacent cells with elev > 0.55
    struct Pos { int x, y; };
    std::vector<Pos> sources;
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            float e = elevation[y * w + x];
            if (e < 0.55f || e > 0.72f) continue;
            Tile t = map->get(x, y);
            if (t == Tile::OW_Mountains || t == Tile::OW_Lake) continue;

            // Check adjacent for mountain
            bool adj_mountain = false;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    if (map->get(x + dx, y + dy) == Tile::OW_Mountains)
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

            Tile cur = map->get(cx, cy);
            if (cur == Tile::OW_Lake || cur == Tile::OW_River) break;
            if (cur != Tile::OW_Mountains) {
                map->set(cx, cy, Tile::OW_River);
            }
            visited[cy][cx] = true;

            // Find lowest unvisited neighbor (4-directional)
            float best_elev = elevation[cy * w + cx];
            int bx = -1, by = -1;
            static const int dx4[] = {0, 0, -1, 1};
            static const int dy4[] = {-1, 1, 0, 0};

            for (int d = 0; d < 4; ++d) {
                int nx = cx + dx4[d];
                int ny = cy + dy4[d];
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                if (visited[ny][nx]) continue;
                if (map->get(nx, ny) == Tile::OW_Mountains) continue;
                float ne = elevation[ny * w + nx];
                if (ne < best_elev) {
                    best_elev = ne;
                    bx = nx;
                    by = ny;
                }
            }

            // If stuck, try same-elevation neighbors
            if (bx < 0) {
                float cur_elev = elevation[cy * w + cx];
                for (int d = 0; d < 4; ++d) {
                    int nx = cx + dx4[d];
                    int ny = cy + dy4[d];
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                    if (visited[ny][nx]) continue;
                    if (map->get(nx, ny) == Tile::OW_Mountains) continue;
                    float ne = elevation[ny * w + nx];
                    if (std::abs(ne - cur_elev) < 0.05f) {
                        bx = nx;
                        by = ny;
                        break;
                    }
                }
            }

            if (bx < 0) break; // truly stuck
            cx = bx;
            cy = by;
        }
    }
}

// --- Mountain pass guarantee (flood-fill connectivity) ---

static void ensure_connectivity(TileMap* map, const std::vector<float>& elevation,
                                int w, int h, int land_x, int land_y) {
    std::vector<bool> reachable(w * h, false);
    std::queue<std::pair<int,int>> frontier;

    reachable[land_y * w + land_x] = true;
    frontier.push({land_x, land_y});

    while (!frontier.empty()) {
        auto [fx, fy] = frontier.front();
        frontier.pop();
        static const int dx4[] = {0, 0, -1, 1};
        static const int dy4[] = {-1, 1, 0, 0};
        for (int d = 0; d < 4; ++d) {
            int nx = fx + dx4[d];
            int ny = fy + dy4[d];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            if (reachable[ny * w + nx]) continue;
            if (!map->passable(nx, ny)) continue;
            reachable[ny * w + nx] = true;
            frontier.push({nx, ny});
        }
    }

    // Find unreachable passable tiles and carve passes to them
    bool changed = true;
    while (changed) {
        changed = false;
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                if (reachable[y * w + x]) continue;
                Tile t = map->get(x, y);
                if (t == Tile::OW_Mountains || t == Tile::OW_Lake) continue;
                // This is an unreachable passable tile — find blocking mountain
                // with lowest elevation and carve a pass

                // BFS from landing pad toward this cell, carving through mountains
                // Simpler approach: scan the border between reachable and unreachable
                // for the mountain cell with lowest elevation and convert it
                float best = 1.0f;
                int bx = -1, by = -1;
                for (int sy = 0; sy < h; ++sy) {
                    for (int sx = 0; sx < w; ++sx) {
                        if (map->get(sx, sy) != Tile::OW_Mountains) continue;
                        // Check if adjacent to reachable cell
                        bool adj_reach = false;
                        static const int dx4[] = {0, 0, -1, 1};
                        static const int dy4[] = {-1, 1, 0, 0};
                        for (int d = 0; d < 4; ++d) {
                            int nx2 = sx + dx4[d];
                            int ny2 = sy + dy4[d];
                            if (nx2 >= 0 && nx2 < w && ny2 >= 0 && ny2 < h &&
                                reachable[ny2 * w + nx2]) {
                                adj_reach = true;
                                break;
                            }
                        }
                        if (!adj_reach) continue;
                        // Also check if adjacent to unreachable non-mountain
                        bool adj_unreach = false;
                        for (int d = 0; d < 4; ++d) {
                            int nx2 = sx + dx4[d];
                            int ny2 = sy + dy4[d];
                            if (nx2 >= 0 && nx2 < w && ny2 >= 0 && ny2 < h &&
                                !reachable[ny2 * w + nx2] &&
                                map->get(nx2, ny2) != Tile::OW_Mountains &&
                                map->get(nx2, ny2) != Tile::OW_Lake) {
                                adj_unreach = true;
                                break;
                            }
                        }
                        if (!adj_unreach) continue;
                        float e = elevation[sy * w + sx];
                        if (e < best) {
                            best = e;
                            bx = sx;
                            by = sy;
                        }
                    }
                }

                if (bx >= 0) {
                    map->set(bx, by, Tile::OW_Plains);
                    changed = true;
                    // Re-flood from the carved pass
                    reachable[by * w + bx] = true;
                    std::queue<std::pair<int,int>> q;
                    q.push({bx, by});
                    while (!q.empty()) {
                        auto [qx, qy] = q.front();
                        q.pop();
                        static const int dx4[] = {0, 0, -1, 1};
                        static const int dy4[] = {-1, 1, 0, 0};
                        for (int d = 0; d < 4; ++d) {
                            int nx2 = qx + dx4[d];
                            int ny2 = qy + dy4[d];
                            if (nx2 < 0 || nx2 >= w || ny2 < 0 || ny2 >= h) continue;
                            if (reachable[ny2 * w + nx2]) continue;
                            if (!map->passable(nx2, ny2)) continue;
                            reachable[ny2 * w + nx2] = true;
                            q.push({nx2, ny2});
                        }
                    }
                }
                // Break out to restart scan after carving
                if (changed) goto restart_scan;
            }
        }
        restart_scan:;
    }
}

// --- Main generation ---

void OverworldGenerator::generate_layout(std::mt19937& rng) {
    int w = map_->width();
    int h = map_->height();

    TerrainContext ctx;
    ctx.body_type = props_->body_type;
    ctx.atmosphere = props_->body_atmosphere;
    ctx.temperature = props_->body_temperature;

    // Generate seeds for noise
    std::uniform_int_distribution<unsigned> seed_dist(0, 0xFFFFFFFFu);
    unsigned elev_seed = seed_dist(rng);
    unsigned moist_seed = seed_dist(rng);

    // Noise scales
    float elev_scale = 0.08f;
    float moist_scale = 0.12f;

    // Asteroid/dwarf planet: higher frequency for chaotic terrain
    if (ctx.body_type == BodyType::AsteroidBelt || ctx.body_type == BodyType::DwarfPlanet) {
        elev_scale = 0.2f;
    }

    // Generate noise fields
    std::vector<float> elevation(w * h);
    std::vector<float> moisture(w * h);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            elevation[y * w + x] = fbm(static_cast<float>(x), static_cast<float>(y),
                                        elev_seed, elev_scale);
            moisture[y * w + x] = fbm(static_cast<float>(x), static_cast<float>(y),
                                       moist_seed, moist_scale);
        }
    }

    // Classify terrain
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float e = elevation[y * w + x];
            float m = moisture[y * w + x];
            Tile t;

            if (is_terrestrial_with_atmo(ctx)) {
                t = classify_terrestrial(e, m, ctx);
            } else if (ctx.body_type == BodyType::AsteroidBelt) {
                t = classify_asteroid(e);
            } else if (is_rocky_no_atmo(ctx) || ctx.body_type == BodyType::DwarfPlanet) {
                t = classify_rocky(e, m);
            } else {
                // GasGiant, IceGiant fallback
                t = classify_rocky(e, m);
            }

            map_->set(x, y, t);
        }
    }

    // River carving (terrestrial with atmosphere, not frozen or scorching)
    if (is_terrestrial_with_atmo(ctx) &&
        ctx.temperature != Temperature::Frozen &&
        ctx.temperature != Temperature::Scorching) {
        carve_rivers(map_, elevation, w, h, rng);
    }

    // Place landing pad near center — find passable non-water tile
    int cx = w / 2, cy = h / 2;
    int land_x = cx, land_y = cy;
    for (int r = 0; r < std::max(w, h); ++r) {
        bool found = false;
        for (int dy = -r; dy <= r && !found; ++dy) {
            for (int dx = -r; dx <= r && !found; ++dx) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;
                int px = cx + dx, py = cy + dy;
                if (px < 0 || px >= w || py < 0 || py >= h) continue;
                Tile t = map_->get(px, py);
                if (t != Tile::OW_Mountains && t != Tile::OW_Lake &&
                    t != Tile::OW_River && t != Tile::OW_Swamp) {
                    land_x = px;
                    land_y = py;
                    found = true;
                }
            }
        }
        if (found) break;
    }

    // Ensure connectivity before placing landing pad
    // (temporarily mark landing spot as passable for flood fill)
    map_->set(land_x, land_y, Tile::OW_Plains);
    ensure_connectivity(map_, elevation, w, h, land_x, land_y);
    map_->set(land_x, land_y, Tile::OW_Landing);
}

void OverworldGenerator::place_features(std::mt19937& rng) {
    int w = map_->width();
    int h = map_->height();

    // Classify world type
    bool habitable = props_->body_type == BodyType::Terrestrial &&
        (props_->body_atmosphere == Atmosphere::Standard ||
         props_->body_atmosphere == Atmosphere::Dense);
    bool marginal = props_->body_type == BodyType::Terrestrial &&
        (props_->body_atmosphere == Atmosphere::Thin ||
         props_->body_atmosphere == Atmosphere::Toxic ||
         props_->body_atmosphere == Atmosphere::Reducing);
    bool airless = (props_->body_type == BodyType::Rocky ||
                    props_->body_type == BodyType::DwarfPlanet) ||
                   (props_->body_type == BodyType::Terrestrial &&
                    props_->body_atmosphere == Atmosphere::None);
    bool asteroid = props_->body_type == BodyType::AsteroidBelt;

    // Collect candidate anchor positions (passable, not water/river/landing)
    struct Pos { int x, y; };
    std::vector<Pos> candidates;
    for (int y = 2; y < h - 2; ++y) {
        for (int x = 2; x < w - 2; ++x) {
            Tile t = map_->get(x, y);
            if (t != Tile::OW_Mountains && t != Tile::OW_Lake &&
                t != Tile::OW_River && t != Tile::OW_Landing) {
                candidates.push_back({x, y});
            }
        }
    }
    if (candidates.empty()) return;
    std::shuffle(candidates.begin(), candidates.end(), rng);

    // Track placed anchor positions for spacing
    std::vector<Pos> placed;

    auto too_close = [&](int px, int py) {
        for (const auto& p : placed) {
            if (std::abs(p.x - px) + std::abs(p.y - py) < 8) return true;
        }
        return false;
    };

    // Check if a stamp fits at (ax, ay): all cells in-bounds, passable terrain, no POIs
    auto stamp_fits = [&](const StampDef& stamp, int ax, int ay) -> bool {
        for (int i = 0; i < stamp.cell_count; ++i) {
            int cx = ax + stamp.cells[i].dx;
            int cy = ay + stamp.cells[i].dy;
            if (cx < 0 || cx >= w || cy < 0 || cy >= h) return false;
            Tile t = map_->get(cx, cy);
            if (t == Tile::OW_Mountains || t == Tile::OW_Lake ||
                t == Tile::OW_River || t == Tile::OW_Landing) return false;
            if (t >= Tile::OW_CaveEntrance && t <= Tile::OW_Outpost) return false;
        }
        return true;
    };

    // Place a stamp at (ax, ay)
    auto place_stamp = [&](const StampDef& stamp, int ax, int ay) {
        for (int i = 0; i < stamp.cell_count; ++i) {
            int cx = ax + stamp.cells[i].dx;
            int cy = ay + stamp.cells[i].dy;
            map_->set(cx, cy, stamp.cells[i].tile);
            if (stamp.cells[i].glyph_index != SG_None) {
                map_->set_glyph_override(cx, cy, stamp.cells[i].glyph_index);
            }
        }
        placed.push_back({ax, ay});
    };

    // Try to place a stamp from the given pool
    auto try_place = [&](const StampDef* pool, int pool_size,
                         int max_stamp_idx = -1) -> bool {
        // max_stamp_idx: limit which stamps to consider (-1 = all)
        int limit = (max_stamp_idx < 0) ? pool_size : std::min(max_stamp_idx + 1, pool_size);

        // Shuffle stamp indices
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

    // Place N stamps from a pool
    auto place_n = [&](const StampDef* pool, int pool_size, int count,
                       int max_stamp_idx = -1) {
        for (int i = 0; i < count; ++i) {
            try_place(pool, pool_size, max_stamp_idx);
        }
    };

    std::uniform_int_distribution<int> pct(0, 99);

    // --- Guaranteed settlement for habitable worlds (place first) ---
    if (habitable) {
        // Try large stamps first (indices 1-3), fall back to 1x1 (index 0)
        if (!try_place(settlement_stamps, settlement_stamp_count, -1)) {
            // Extremely unlikely, but try 1x1 only
            try_place(settlement_stamps, settlement_stamp_count, 0);
        }
        // Additional settlements: 0-2 more
        int extra = std::uniform_int_distribution<int>(0, 2)(rng);
        place_n(settlement_stamps, settlement_stamp_count, extra);
    } else if (marginal) {
        // Settlement possible but not guaranteed
        if (pct(rng) < 40) {
            int count = std::uniform_int_distribution<int>(1, 1)(rng);
            // Only 1x1 and small stamps (indices 0-1)
            place_n(settlement_stamps, settlement_stamp_count, count, 1);
        }
    }

    // --- Space habitats for airless worlds ---
    if (airless && !asteroid && pct(rng) < 40) {
        int count = std::uniform_int_distribution<int>(1, 2)(rng);
        place_n(habitat_stamps, habitat_stamp_count, count);
    }

    // --- Caves ---
    if (props_->body_has_dungeon) {
        int count = std::uniform_int_distribution<int>(2, 5)(rng);
        place_n(cave_stamps, cave_stamp_count, count);
    }

    // --- Ruins ---
    {
        int count = 0;
        if (props_->body_danger_level >= 3) {
            count = std::uniform_int_distribution<int>(1, 4)(rng);
        } else if (pct(rng) < 30) {
            count = std::uniform_int_distribution<int>(1, 2)(rng);
        }
        if (count > 0) {
            // On asteroids/airless, only 1x1 stamps
            int max_idx = (asteroid || airless) ? 1 : -1;
            place_n(ruin_stamps, ruin_stamp_count, count, max_idx);
        }
    }

    // --- Crashed ships ---
    if (pct(rng) < 20 + props_->body_danger_level * 10) {
        int count = std::uniform_int_distribution<int>(1, 3)(rng);
        int max_idx = (asteroid) ? 1 : -1;
        place_n(crashed_ship_stamps, crashed_ship_stamp_count, count, max_idx);
    }

    // --- Outposts ---
    if (pct(rng) < 30) {
        int count = std::uniform_int_distribution<int>(1, 2)(rng);
        int max_idx = (asteroid) ? 0 : -1;
        place_n(outpost_stamps, outpost_stamp_count, count, max_idx);
    }
}

void OverworldGenerator::assign_regions(std::mt19937& rng) {
    Region reg;
    reg.type = RegionType::Room;
    reg.lit = true;
    reg.flavor = RoomFlavor::EmptyRoom;
    reg.name = "Surface";
    reg.enter_message = "";
    int rid = map_->add_region(reg);

    for (int y = 0; y < map_->height(); ++y) {
        for (int x = 0; x < map_->width(); ++x) {
            map_->set_region(x, y, rid);
        }
    }
    (void)rng;
}

std::unique_ptr<MapGenerator> make_overworld_generator() {
    return std::make_unique<OverworldGenerator>();
}

} // namespace astra
