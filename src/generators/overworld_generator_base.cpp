#include "astra/overworld_generator.h"
#include "astra/map_properties.h"
#include "astra/lore_influence_map.h"
#include "astra/world_constants.h"

#include <algorithm>
#include <cmath>
#include <queue>

namespace astra {

// ---------------------------------------------------------------------------
// Noise helpers
// ---------------------------------------------------------------------------

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

float ow_fbm(float x, float y, unsigned seed, float scale, int octaves) {
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

// ---------------------------------------------------------------------------
// Default virtual hook implementations
// ---------------------------------------------------------------------------

void OverworldGeneratorBase::configure_noise(float& elev_scale, float& moist_scale,
                                              const TerrainContext& /*ctx*/) {
    elev_scale = 0.08f;
    moist_scale = 0.12f;
}

void OverworldGeneratorBase::carve_rivers(std::mt19937& /*rng*/) {
    // Default: no rivers
}

void OverworldGeneratorBase::place_pois(std::mt19937& /*rng*/) {
    // Default: no POIs
}

// ---------------------------------------------------------------------------
// generate_layout — template method (non-virtual, calls hooks)
// ---------------------------------------------------------------------------

void OverworldGeneratorBase::generate_layout(std::mt19937& rng) {
    int w = map_->width();
    int h = map_->height();

    // Step 1: Build terrain context
    ctx_.body_type = props_->body_type;
    ctx_.atmosphere = props_->body_atmosphere;
    ctx_.temperature = props_->body_temperature;

    // Step 2: Configure noise scales (virtual hook)
    float elev_scale, moist_scale;
    configure_noise(elev_scale, moist_scale, ctx_);

    // Step 3: Generate noise fields
    std::uniform_int_distribution<unsigned> seed_dist(0, 0xFFFFFFFFu);
    unsigned elev_seed = seed_dist(rng);
    unsigned moist_seed = seed_dist(rng);

    elevation_.resize(w * h);
    moisture_.resize(w * h);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            elevation_[y * w + x] = ow_fbm(static_cast<float>(x), static_cast<float>(y),
                                             elev_seed, elev_scale);
            moisture_[y * w + x] = ow_fbm(static_cast<float>(x), static_cast<float>(y),
                                            moist_seed, moist_scale);
        }
    }

    // Step 4: Classify terrain (virtual hook)
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float e = elevation_[y * w + x];
            float m = moisture_[y * w + x];
            Tile t = classify_terrain(x, y, e, m, ctx_);
            map_->set(x, y, t);
        }
    }

    // Step 5: Apply lore overlays (shared)
    apply_lore_overlays(rng);

    // Step 6: Carve rivers (virtual hook)
    carve_rivers(rng);

    // Step 7: Place landing pad (shared)
    place_landing_pad();

    // Step 8: Ensure connectivity (shared)
    ensure_connectivity();
}

// ---------------------------------------------------------------------------
// apply_lore_overlays
// ---------------------------------------------------------------------------

void OverworldGeneratorBase::apply_lore_overlays(std::mt19937& rng) {
    if (!props_->lore_influence) return;

    int w = map_->width();
    int h = map_->height();

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float scar = props_->lore_influence->scar_at(x, y);
            float alien = props_->lore_influence->alien_at(x, y);

            if (scar > world::scar_heavy_threshold) {
                map_->set(x, y, Tile::OW_GlassedCrater);
            } else if (scar > world::scar_medium_threshold) {
                map_->set(x, y, Tile::OW_ScorchedEarth);
            } else if (alien > world::alien_full_replace) {
                map_->set(x, y, Tile::OW_AlienTerrain);
            } else if (alien > world::alien_strength_threshold) {
                if (std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) < alien)
                    map_->set(x, y, Tile::OW_AlienTerrain);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// place_landing_pad
// ---------------------------------------------------------------------------

void OverworldGeneratorBase::place_landing_pad() {
    int w = map_->width();
    int h = map_->height();
    int cx = w / 2, cy = h / 2;
    land_x_ = cx;
    land_y_ = cy;

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
                    land_x_ = px;
                    land_y_ = py;
                    found = true;
                }
            }
        }
        if (found) break;
    }

    map_->set(land_x_, land_y_, Tile::OW_Plains);
    ensure_connectivity();
    map_->set(land_x_, land_y_, Tile::OW_Landing);
}

// ---------------------------------------------------------------------------
// ensure_connectivity
// ---------------------------------------------------------------------------

void OverworldGeneratorBase::ensure_connectivity() {
    int w = map_->width();
    int h = map_->height();

    std::vector<bool> reachable(w * h, false);
    std::queue<std::pair<int,int>> frontier;

    reachable[land_y_ * w + land_x_] = true;
    frontier.push({land_x_, land_y_});

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
            if (!map_->passable(nx, ny)) continue;
            reachable[ny * w + nx] = true;
            frontier.push({nx, ny});
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                if (reachable[y * w + x]) continue;
                Tile t = map_->get(x, y);
                if (t == Tile::OW_Mountains || t == Tile::OW_Lake) continue;

                float best = 1.0f;
                int bx = -1, by = -1;
                for (int sy = 0; sy < h; ++sy) {
                    for (int sx = 0; sx < w; ++sx) {
                        if (map_->get(sx, sy) != Tile::OW_Mountains) continue;
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
                        bool adj_unreach = false;
                        for (int d = 0; d < 4; ++d) {
                            int nx2 = sx + dx4[d];
                            int ny2 = sy + dy4[d];
                            if (nx2 >= 0 && nx2 < w && ny2 >= 0 && ny2 < h &&
                                !reachable[ny2 * w + nx2] &&
                                map_->get(nx2, ny2) != Tile::OW_Mountains &&
                                map_->get(nx2, ny2) != Tile::OW_Lake) {
                                adj_unreach = true;
                                break;
                            }
                        }
                        if (!adj_unreach) continue;
                        float e = elevation_[sy * w + sx];
                        if (e < best) {
                            best = e;
                            bx = sx;
                            by = sy;
                        }
                    }
                }

                if (bx >= 0) {
                    map_->set(bx, by, Tile::OW_Plains);
                    changed = true;
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
                            if (!map_->passable(nx2, ny2)) continue;
                            reachable[ny2 * w + nx2] = true;
                            q.push({nx2, ny2});
                        }
                    }
                }
                if (changed) goto restart_scan;
            }
        }
        restart_scan:;
    }
}

// ---------------------------------------------------------------------------
// place_features — shared lore landmarks + virtual place_pois
// ---------------------------------------------------------------------------

void OverworldGeneratorBase::place_features(std::mt19937& rng) {
    int w = map_->width();
    int h = map_->height();

    // Place lore landmarks before other POIs
    if (props_->lore_influence) {
        if (props_->lore_beacon) {
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    if (props_->lore_influence->landmark_at(x, y) == LandmarkType::Beacon) {
                        bool is_center = true;
                        for (int d = 1; d <= 2; ++d) {
                            if (props_->lore_influence->landmark_at(x-d, y) != LandmarkType::Beacon ||
                                props_->lore_influence->landmark_at(x+d, y) != LandmarkType::Beacon)
                                is_center = false;
                        }
                        if (is_center && map_->get(x, y) != Tile::OW_Beacon) {
                            map_->set(x, y, Tile::OW_Beacon);
                        }
                    }
                }
            }
        }
        if (props_->lore_megastructure) {
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    if (props_->lore_influence->landmark_at(x, y) == LandmarkType::Megastructure) {
                        bool is_center = true;
                        for (int d = 1; d <= 2; ++d) {
                            if (props_->lore_influence->landmark_at(x-d, y) != LandmarkType::Megastructure ||
                                props_->lore_influence->landmark_at(x+d, y) != LandmarkType::Megastructure)
                                is_center = false;
                        }
                        if (is_center && map_->get(x, y) != Tile::OW_Megastructure) {
                            map_->set(x, y, Tile::OW_Megastructure);
                        }
                    }
                }
            }
        }
    }

    // Virtual hook for body-specific POI placement
    place_pois(rng);
}

// ---------------------------------------------------------------------------
// assign_regions
// ---------------------------------------------------------------------------

void OverworldGeneratorBase::assign_regions(std::mt19937& rng) {
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

} // namespace astra
