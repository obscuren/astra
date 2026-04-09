#include "astra/overworld_generator.h"
#include "astra/lore_influence_map.h"
#include "astra/map_properties.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace astra {

// ---------------------------------------------------------------------------
// TemperateOverworldGenerator — Voronoi region-based terrain for temperate
// terrestrial planets. Large coherent biome patches with A+B rivers.
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
    struct VoronoiSeed {
        float x, y;
        Tile biome;
    };
    std::vector<VoronoiSeed> seeds_;

    void init_seeds(std::mt19937& rng);
    Tile biome_for_elev_moist(float elev, float moist);
    int nearest_seed(float x, float y) const;
    int second_nearest_seed(float x, float y, int skip) const;
    float distance_to_boundary(float x, float y) const;
};

// ---------------------------------------------------------------------------
// Noise configuration
// ---------------------------------------------------------------------------

void TemperateOverworldGenerator::configure_noise(float& elev_scale, float& moist_scale,
                                                   const TerrainContext& /*ctx*/) {
    elev_scale = 0.08f;
    moist_scale = 0.12f;
}

// ---------------------------------------------------------------------------
// Voronoi seed initialization
// ---------------------------------------------------------------------------

Tile TemperateOverworldGenerator::biome_for_elev_moist(float elev, float moist) {
    if (elev > 0.72f) return Tile::OW_Mountains;
    if (elev < 0.25f) return Tile::OW_Lake;
    if (elev < 0.35f && moist > 0.5f) return Tile::OW_Swamp;
    if (moist > 0.6f) return Tile::OW_Forest;
    if (moist > 0.3f) return Tile::OW_Plains;
    return Tile::OW_Desert;
}

void TemperateOverworldGenerator::init_seeds(std::mt19937& rng) {
    int w = map_->width();
    int h = map_->height();

    std::uniform_int_distribution<int> count_dist(15, 25);
    int num_seeds = count_dist(rng);

    std::uniform_real_distribution<float> x_dist(0.0f, static_cast<float>(w));
    std::uniform_real_distribution<float> y_dist(0.0f, static_cast<float>(h));

    seeds_.clear();
    seeds_.reserve(num_seeds);

    for (int i = 0; i < num_seeds; ++i) {
        float sx = x_dist(rng);
        float sy = y_dist(rng);
        int ix = std::clamp(static_cast<int>(sx), 0, w - 1);
        int iy = std::clamp(static_cast<int>(sy), 0, h - 1);
        float elev = elevation_[iy * w + ix];
        float moist = moisture_[iy * w + ix];
        Tile biome = biome_for_elev_moist(elev, moist);
        seeds_.push_back({sx, sy, biome});
    }
}

void TemperateOverworldGenerator::pre_classify(std::mt19937& rng) {
    init_seeds(rng);
}

int TemperateOverworldGenerator::nearest_seed(float x, float y) const {
    int best = 0;
    float best_dist = 1e9f;
    for (int i = 0; i < static_cast<int>(seeds_.size()); ++i) {
        float dx = x - seeds_[i].x;
        float dy = y - seeds_[i].y;
        float d = dx * dx + dy * dy;
        if (d < best_dist) {
            best_dist = d;
            best = i;
        }
    }
    return best;
}

int TemperateOverworldGenerator::second_nearest_seed(float x, float y, int skip) const {
    int best = -1;
    float best_dist = 1e9f;
    for (int i = 0; i < static_cast<int>(seeds_.size()); ++i) {
        if (i == skip) continue;
        float dx = x - seeds_[i].x;
        float dy = y - seeds_[i].y;
        float d = dx * dx + dy * dy;
        if (d < best_dist) {
            best_dist = d;
            best = i;
        }
    }
    return best;
}

float TemperateOverworldGenerator::distance_to_boundary(float x, float y) const {
    int n1 = nearest_seed(x, y);
    int n2 = second_nearest_seed(x, y, n1);
    if (n2 < 0) return 1e9f;

    float dx1 = x - seeds_[n1].x, dy1 = y - seeds_[n1].y;
    float dx2 = x - seeds_[n2].x, dy2 = y - seeds_[n2].y;
    float d1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
    float d2 = std::sqrt(dx2 * dx2 + dy2 * dy2);
    return d2 - d1;
}

// ---------------------------------------------------------------------------
// classify_terrain — Voronoi lookup with edge smoothing
// ---------------------------------------------------------------------------

Tile TemperateOverworldGenerator::classify_terrain(int x, int y, float elev, float moist,
                                                    const TerrainContext& /*ctx*/) {
    // Extreme elevation overrides Voronoi
    if (elev > 0.72f) return Tile::OW_Mountains;
    if (elev < 0.25f) return Tile::OW_Lake;

    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);

    int n1 = nearest_seed(fx, fy);
    Tile biome = seeds_[n1].biome;

    // Mountain/lake seeds: let the elevation override handle them
    if (biome == Tile::OW_Mountains) return Tile::OW_Mountains;
    if (biome == Tile::OW_Lake) return Tile::OW_Lake;

    // Edge smoothing at Voronoi boundaries
    float boundary_dist = distance_to_boundary(fx, fy);
    if (boundary_dist < 2.0f) {
        int n2 = second_nearest_seed(fx, fy, n1);
        if (n2 >= 0) {
            Tile biome2 = seeds_[n2].biome;
            float jitter = (elev - 0.5f) * 2.0f;
            float threshold = boundary_dist / 2.0f;
            if (jitter > threshold) {
                biome = biome2;
            }
        }
    }

    return biome;
}

// ---------------------------------------------------------------------------
// carve_rivers — Combined A+B: downhill from mountains, bending toward
// Voronoi boundaries as elevation decreases
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

    // River count: 3 + mountain_seeds/3, clamped 3-6
    int mountain_seeds = 0;
    for (const auto& s : seeds_) {
        if (s.biome == Tile::OW_Mountains) ++mountain_seeds;
    }
    int num_rivers = std::clamp(3 + mountain_seeds / 3, 3, 6);
    num_rivers = std::min(num_rivers, static_cast<int>(sources.size()));

    for (int r = 0; r < num_rivers; ++r) {
        int cx = sources[r].x;
        int cy = sources[r].y;
        std::vector<std::vector<bool>> visited(h, std::vector<bool>(w, false));

        for (int step = 0; step < 100; ++step) {
            if (cx <= 0 || cx >= w - 1 || cy <= 0 || cy >= h - 1) break;

            Tile cur = map_->get(cx, cy);
            if (cur == Tile::OW_Lake || cur == Tile::OW_River) break;
            if (cur != Tile::OW_Mountains) {
                map_->set(cx, cy, Tile::OW_River);
            }
            visited[cy][cx] = true;

            float cur_elev = elevation_[cy * w + cx];
            float downhill_weight = cur_elev * 2.0f;
            float boundary_weight = (1.0f - cur_elev) * 1.5f;

            static const int dx4[] = {0, 0, -1, 1};
            static const int dy4[] = {-1, 1, 0, 0};

            float best_score = -1e9f;
            int bx = -1, by = -1;

            for (int d = 0; d < 4; ++d) {
                int nx = cx + dx4[d];
                int ny = cy + dy4[d];
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                if (visited[ny][nx]) continue;
                if (map_->get(nx, ny) == Tile::OW_Mountains) continue;

                float ne = elevation_[ny * w + nx];
                float downhill = (cur_elev - ne) * downhill_weight;

                float bd = distance_to_boundary(static_cast<float>(nx),
                                                 static_cast<float>(ny));
                float boundary = (1.0f / (bd + 1.0f)) * boundary_weight;

                float score = downhill + boundary;
                if (score > best_score) {
                    best_score = score;
                    bx = nx;
                    by = ny;
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
