#include "astra/overworld_generator.h"
#include "astra/lore_influence_map.h"
#include "astra/map_properties.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace astra {

// Forest variant flags stored in bits 0-1 of overworld custom_flags
static constexpr uint8_t FOREST_VARIANT_MASK = 0x03;
static constexpr uint8_t FOREST_TEMPERATE   = 0;  // standard green
static constexpr uint8_t FOREST_AUTUMN      = 1;  // red/orange (cool + mid elevation)
static constexpr uint8_t FOREST_CONIFER     = 2;  // dark blue-green (cold + high)

// ---------------------------------------------------------------------------
// TemperateOverworldGenerator — layered simulation for temperate terrestrial
// planets. Biomes emerge from elevation + latitude-based temperature + moisture.
// ---------------------------------------------------------------------------

class TemperateOverworldGenerator : public OverworldGeneratorBase {
protected:
    void configure_noise(float& elev_scale, float& moist_scale,
                         const TerrainContext& ctx) override;
    void pre_classify(std::mt19937& rng) override;
    Tile classify_terrain(int x, int y, float elev, float moist,
                          const TerrainContext& ctx) override;
    void carve_rivers(std::mt19937& rng) override;
    void place_pois(std::mt19937& rng) override;

private:
    // Derived layers (computed in pre_classify)
    std::vector<float> temperature_;  // 0 = freezing, 1 = scorching
};

// ---------------------------------------------------------------------------
// Noise configuration — lower frequency for larger features
// ---------------------------------------------------------------------------

void TemperateOverworldGenerator::configure_noise(float& elev_scale, float& moist_scale,
                                                   const TerrainContext& /*ctx*/) {
    elev_scale = 0.06f;   // lower = larger mountain ranges and valleys
    moist_scale = 0.08f;  // lower = larger wet/dry zones
}

// ---------------------------------------------------------------------------
// pre_classify — build temperature layer from latitude + elevation
// ---------------------------------------------------------------------------

void TemperateOverworldGenerator::pre_classify(std::mt19937& rng) {
    int w = map_->width();
    int h = map_->height();

    temperature_.resize(w * h);

    // Temperature noise seed for variation
    unsigned temp_seed = rng();

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // Latitude: 0.0 at top (cold/polar), 1.0 at bottom (hot/tropical)
            float latitude = static_cast<float>(y) / static_cast<float>(h - 1);

            // Base temperature from latitude: poles cold, equator hot
            // Use a curve that makes the middle temperate band wider
            float base_temp = latitude;

            // Elevation cooling: high elevation = colder (gentle effect)
            float elev = elevation_[y * w + x];
            float elev_cooling = std::max(0.0f, (elev - 0.5f) * 0.8f);

            // Small noise variation for natural irregularity
            float noise = ow_fbm(static_cast<float>(x), static_cast<float>(y),
                                  temp_seed, 0.05f, 3);
            float variation = (noise - 0.5f) * 0.15f;

            float temp = base_temp - elev_cooling + variation;
            temperature_[y * w + x] = std::clamp(temp, 0.0f, 1.0f);
        }
    }
}

// ---------------------------------------------------------------------------
// classify_terrain — biomes from elevation + temperature + moisture
// ---------------------------------------------------------------------------

Tile TemperateOverworldGenerator::classify_terrain(int x, int y, float elev, float moist,
                                                    const TerrainContext& /*ctx*/) {
    int w = map_->width();
    float temp = temperature_[y * w + x];

    // Helper: return forest tile with variant flag based on temperature + elevation
    auto forest = [&]() -> Tile {
        uint8_t variant = FOREST_TEMPERATE;
        if (temp < 0.3f && elev > 0.5f) {
            variant = FOREST_CONIFER;     // cold + high = dark conifers
        } else if (temp < 0.4f || elev > 0.6f) {
            variant = FOREST_AUTUMN;      // cool or elevated = autumn colors
        }
        map_->set_custom_flags_byte(x, y, variant & FOREST_VARIANT_MASK);
        return Tile::OW_Forest;
    };

    // --- Elevation extremes ---
    if (elev > 0.75f) return Tile::OW_Mountains;
    if (elev < 0.2f)  return Tile::OW_Lake;

    // --- Cold regions — ice with mountain peaks poking through ---
    if (temp < 0.15f) {
        // Polar: mountain peaks at high elevation, ice everywhere else
        if (elev > 0.6f) return Tile::OW_Mountains;
        return Tile::OW_IceField;
    }
    if (temp < 0.25f) {
        // Sub-polar: mountain peaks at high elevation, ice at mid-high, plains below
        if (elev > 0.6f) return Tile::OW_Mountains;
        if (elev > 0.5f) return Tile::OW_IceField;
        return Tile::OW_Plains;
    }

    // --- Hot regions (temp > 0.75) — desert and savanna ---
    if (temp > 0.8f) {
        if (moist > 0.6f) return Tile::OW_Swamp;
        if (moist > 0.4f) return Tile::OW_Plains;
        return Tile::OW_Desert;
    }
    if (temp > 0.65f) {
        if (moist > 0.55f) return forest();
        if (moist > 0.35f) return Tile::OW_Plains;
        return Tile::OW_Desert;
    }

    // --- Temperate band (0.3 - 0.65) ---

    // Low elevation + wet = swamp/marsh
    if (elev < 0.32f && moist > 0.5f) {
        return Tile::OW_Swamp;
    }

    // Wet = forest
    if (moist > 0.55f) {
        return forest();
    }

    // Moderate moisture = plains/grassland
    if (moist > 0.3f) {
        return Tile::OW_Plains;
    }

    // Dry = desert
    return Tile::OW_Desert;
}

// ---------------------------------------------------------------------------
// carve_rivers — downhill from mountains, longer paths, lake formation
// ---------------------------------------------------------------------------

void TemperateOverworldGenerator::carve_rivers(std::mt19937& rng) {
    int w = map_->width();
    int h = map_->height();

    // Find river sources: mountain-adjacent tiles at high elevation
    struct Pos { int x, y; };
    std::vector<Pos> sources;
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            float e = elevation_[y * w + x];
            if (e < 0.55f || e > 0.75f) continue;
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

    int num_rivers = std::clamp(static_cast<int>(sources.size()) / 3, 3, 8);
    num_rivers = std::min(num_rivers, static_cast<int>(sources.size()));

    for (int r = 0; r < num_rivers; ++r) {
        int cx = sources[r].x;
        int cy = sources[r].y;
        std::vector<std::vector<bool>> visited(h, std::vector<bool>(w, false));

        for (int step = 0; step < 120; ++step) {
            if (cx <= 0 || cx >= w - 1 || cy <= 0 || cy >= h - 1) break;

            Tile cur = map_->get(cx, cy);
            if (cur == Tile::OW_Lake || cur == Tile::OW_River) break;
            if (cur != Tile::OW_Mountains) {
                map_->set(cx, cy, Tile::OW_River);
            }
            visited[cy][cx] = true;

            // Follow steepest descent
            float cur_elev = elevation_[cy * w + cx];
            static const int dx4[] = {0, 0, -1, 1};
            static const int dy4[] = {-1, 1, 0, 0};

            float best_elev = cur_elev;
            int bx = -1, by = -1;

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

            // If stuck, try same-elevation neighbors
            if (bx < 0) {
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

            if (bx < 0) {
                // Basin: flood a small lake
                int lake_size = 3 + static_cast<int>(rng() % 6);
                std::vector<std::pair<int,int>> lake_candidates;
                for (int dy = -2; dy <= 2; ++dy) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        int lx = cx + dx, ly = cy + dy;
                        if (lx < 0 || lx >= w || ly < 0 || ly >= h) continue;
                        Tile lt = map_->get(lx, ly);
                        if (lt == Tile::OW_Mountains || lt == Tile::OW_Lake) continue;
                        lake_candidates.push_back({lx, ly});
                    }
                }
                std::sort(lake_candidates.begin(), lake_candidates.end(),
                    [&](const auto& a, const auto& b) {
                        return elevation_[a.second * w + a.first] <
                               elevation_[b.second * w + b.first];
                    });
                for (int i = 0; i < std::min(lake_size,
                     static_cast<int>(lake_candidates.size())); ++i) {
                    map_->set(lake_candidates[i].first,
                              lake_candidates[i].second, Tile::OW_Lake);
                }
                break;
            }

            cx = bx;
            cy = by;
        }
    }
}

// ---------------------------------------------------------------------------
// place_pois — delegate to shared default POI logic
// ---------------------------------------------------------------------------

void TemperateOverworldGenerator::place_pois(std::mt19937& rng) {
    place_default_pois(map_, props_, elevation_, rng);
}

// ---------------------------------------------------------------------------
// Factory helper
// ---------------------------------------------------------------------------

std::unique_ptr<MapGenerator> make_temperate_overworld_generator() {
    return std::make_unique<TemperateOverworldGenerator>();
}

} // namespace astra
