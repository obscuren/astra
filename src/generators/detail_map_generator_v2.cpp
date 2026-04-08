#include "astra/map_generator.h"
#include "astra/biome_profile.h"
#include "astra/terrain_channels.h"
#include "astra/terrain_compositor.h"
#include "astra/map_properties.h"
#include "astra/poi_phase.h"

#include <queue>

namespace astra {

class DetailMapGeneratorV2 : public MapGenerator {
protected:
    void generate_layout(std::mt19937& rng) override;
    void connect_rooms(std::mt19937& rng) override;
    void place_features(std::mt19937& rng) override;
    void assign_regions(std::mt19937& rng) override;
    void generate_backdrop(unsigned /*seed*/) override {}

private:
    void apply_neighbor_bleed(TerrainChannels& channels, std::mt19937& rng);

    TerrainChannels channels_;
};

// ---------------------------------------------------------------------------
// generate_layout
// ---------------------------------------------------------------------------

void DetailMapGeneratorV2::generate_layout(std::mt19937& rng) {
    const auto& prof = biome_profile(props_->biome);
    const int w = map_->width();
    const int h = map_->height();

    channels_ = TerrainChannels(w, h);

    if (prof.elevation_fn) {
        prof.elevation_fn(channels_.elevation.data(), w, h, rng, prof);
    }

    if (prof.moisture_fn) {
        prof.moisture_fn(channels_.moisture.data(), w, h, rng,
                         channels_.elevation.data(), prof);
    }

    if (prof.structure_fn) {
        prof.structure_fn(channels_.structure.data(), w, h, rng,
                          channels_.elevation.data(), channels_.moisture.data(), prof);
    }

    apply_neighbor_bleed(channels_, rng);

    composite_terrain(*map_, channels_, prof);
}

// ---------------------------------------------------------------------------
// apply_neighbor_bleed
// ---------------------------------------------------------------------------

void DetailMapGeneratorV2::apply_neighbor_bleed(TerrainChannels& channels,
                                                 std::mt19937& rng) {
    static constexpr int bleed_margin = 20;

    const int w = channels.width;
    const int h = channels.height;

    // Neighbor tiles: north, south, west, east
    const Tile neighbors[4] = {
        props_->detail_neighbor_n,
        props_->detail_neighbor_s,
        props_->detail_neighbor_w,
        props_->detail_neighbor_e,
    };

    for (int dir = 0; dir < 4; ++dir) {
        if (neighbors[dir] == Tile::Empty) continue;

        Biome neighbor_biome = detail_biome_for_terrain(neighbors[dir],
                                                         props_->biome);
        if (neighbor_biome == props_->biome) continue;

        const auto& neighbor_prof = biome_profile(neighbor_biome);
        if (!neighbor_prof.elevation_fn) continue;

        // Deterministic seed for this neighbor direction
        unsigned dir_seed = rng() ^ (static_cast<unsigned>(dir) * 7919u);
        std::mt19937 neighbor_rng(dir_seed);

        // Generate the neighbor's elevation for the full grid
        std::vector<float> neighbor_elev(w * h, 0.0f);
        neighbor_prof.elevation_fn(neighbor_elev.data(), w, h,
                                   neighbor_rng, neighbor_prof);

        // Blend at the edge
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int distance = 0;
                switch (dir) {
                    case 0: distance = y; break;              // north: from top
                    case 1: distance = (h - 1) - y; break;   // south: from bottom
                    case 2: distance = x; break;              // west: from left
                    case 3: distance = (w - 1) - x; break;   // east: from right
                }

                if (distance >= bleed_margin) continue;

                float t = 1.0f - (static_cast<float>(distance) /
                                  static_cast<float>(bleed_margin));
                t = t * t; // quadratic falloff

                int idx = y * w + x;
                channels.elevation[idx] =
                    channels.elevation[idx] * (1.0f - t) +
                    neighbor_elev[idx] * t;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// connect_rooms — connectivity safety net (Phase 5)
// Flood-fill from map center to find main walkable region. Any disconnected
// floor pockets get connected via minimal 1-wide carved paths.
// ---------------------------------------------------------------------------

void DetailMapGeneratorV2::connect_rooms(std::mt19937& rng) {
    const int w = map_->width();
    const int h = map_->height();
    const int size = w * h;

    // --- Step 1: Find a walkable starting cell near center ---
    int start_x = w / 2, start_y = h / 2;
    // Spiral outward from center to find a floor tile
    for (int r = 0; r < std::max(w, h); ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;
                int nx = w / 2 + dx, ny = h / 2 + dy;
                if (nx >= 0 && nx < w && ny >= 0 && ny < h
                    && map_->passable(nx, ny)) {
                    start_x = nx; start_y = ny;
                    goto found_start;
                }
            }
        }
    }
    found_start:

    // --- Step 2: BFS flood-fill from start to mark main region ---
    std::vector<bool> visited(size, false);
    std::queue<std::pair<int,int>> queue;
    visited[start_y * w + start_x] = true;
    queue.push({start_x, start_y});

    while (!queue.empty()) {
        auto [cx, cy] = queue.front();
        queue.pop();
        static constexpr int dirs[][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        for (auto [ddx, ddy] : dirs) {
            int nx = cx + ddx, ny = cy + ddy;
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            if (visited[ny * w + nx]) continue;
            if (!map_->passable(nx, ny)) continue;
            visited[ny * w + nx] = true;
            queue.push({nx, ny});
        }
    }

    // --- Step 3: Find disconnected floor tiles ---
    // Collect one representative per disconnected pocket
    struct Pocket { int x, y; };
    std::vector<Pocket> pockets;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!map_->passable(x, y)) continue;
            if (visited[y * w + x]) continue;
            // Found an unvisited passable tile — new pocket
            pockets.push_back({x, y});
            // Flood-fill to mark this pocket as found (so we don't add it again)
            std::queue<std::pair<int,int>> pq;
            visited[y * w + x] = true;
            pq.push({x, y});
            while (!pq.empty()) {
                auto [px, py] = pq.front();
                pq.pop();
                static constexpr int dirs[][2] = {{1,0},{-1,0},{0,1},{0,-1}};
                for (auto [ddx, ddy] : dirs) {
                    int nx2 = px + ddx, ny2 = py + ddy;
                    if (nx2 < 0 || nx2 >= w || ny2 < 0 || ny2 >= h) continue;
                    if (visited[ny2 * w + nx2]) continue;
                    if (!map_->passable(nx2, ny2)) continue;
                    visited[ny2 * w + nx2] = true;
                    pq.push({nx2, ny2});
                }
            }
        }
    }

    if (pockets.empty()) return; // fully connected, nothing to do

    // --- Step 4: Carve 1-wide L-shaped paths from each pocket to start ---
    for (auto& pocket : pockets) {
        int ax = pocket.x, ay = pocket.y;
        int bx = start_x, by = start_y;

        // Horizontal segment from pocket to start's x
        int dx = (bx > ax) ? 1 : -1;
        for (int x = ax; x != bx; x += dx) {
            if (map_->get(x, ay) == Tile::Wall) {
                map_->set(x, ay, Tile::Floor);
            }
        }
        // Vertical segment from pocket's row to start's y
        int dy = (by > ay) ? 1 : -1;
        for (int y = ay; y != by; y += dy) {
            if (map_->get(bx, y) == Tile::Wall) {
                map_->set(bx, y, Tile::Floor);
            }
        }
    }
    (void)rng;
}

// ---------------------------------------------------------------------------
// place_features — scatter layer (Phase 4)
// ---------------------------------------------------------------------------

void DetailMapGeneratorV2::place_features(std::mt19937& rng) {
    const auto& prof = biome_profile(props_->biome);
    const int w = map_->width();
    const int h = map_->height();

    // Copy scatter to avoid any reference invalidation issues
    auto scatter_copy = prof.scatter;
    for (const auto& entry : scatter_copy) {
        if (entry.density <= 0.0f) continue;

        int threshold = static_cast<int>(entry.density * 100.0f);

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (map_->get(x, y) != Tile::Floor) continue;
                if (map_->fixture_id(x, y) >= 0) continue;

                if (static_cast<int>(rng() % 100) < threshold) {
                    FixtureData fd;
                    fd.type = entry.type;
                    fd.passable = true;
                    fd.interactable = false;
                    fd.blocks_vision = entry.blocks_vision;
                    map_->add_fixture(x, y, fd);
                }
            }
        }
    }

    // --- Layer A: Shore debris (sand, rocks, dirt) near water ---
    // Biome-specific ground material in a 3-tile band around water.
    // This creates the "bedding" — the terrain transition from water to land.
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (map_->get(x, y) != Tile::Floor) continue;
            if (map_->fixture_id(x, y) >= 0) continue;

            // Compute distance to nearest water (up to 3 tiles)
            int water_dist = 4; // > 3 = not near water
            for (int dy = -3; dy <= 3 && water_dist > 0; ++dy)
                for (int dx = -3; dx <= 3 && water_dist > 0; ++dx) {
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h
                        && map_->get(nx, ny) == Tile::Water) {
                        int d = std::max(std::abs(dx), std::abs(dy));
                        water_dist = std::min(water_dist, d);
                    }
                }

            if (water_dist > 3) continue;

            // Density falls off with distance: 70% at edge, 50% at 2, 30% at 3
            int shore_chance = (water_dist == 1) ? 70 : (water_dist == 2) ? 50 : 30;
            if (static_cast<int>(rng() % 100) < shore_chance) {
                FixtureData fd;
                fd.type = FixtureType::ShoreDebris;
                fd.passable = true;
                fd.interactable = false;
                fd.blocks_vision = false;
                map_->add_fixture(x, y, fd);
            }
        }
    }

    // --- Layer B: Riparian lush zone — vegetation gradient from water ---
    // Density fades with distance from water. Closest = dense + vision-blocking,
    // further out = sparser, non-blocking. Creates natural growth gradient.
    // Skip volcanic (lava doesn't grow plants).
    bool has_lush_zone = (props_->biome != Biome::Volcanic
                       && props_->biome != Biome::ScarredScorched
                       && props_->biome != Biome::ScarredGlassed);
    if (has_lush_zone) {
        constexpr int max_lush_dist = 7;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (map_->get(x, y) != Tile::Floor) continue;
                if (map_->fixture_id(x, y) >= 0) continue;

                // Find distance to nearest water (Chebyshev, up to max_lush_dist)
                int water_dist = max_lush_dist + 1;
                int scan = max_lush_dist;
                for (int dy = -scan; dy <= scan && water_dist > 1; ++dy)
                    for (int dx = -scan; dx <= scan && water_dist > 1; ++dx) {
                        int nx = x + dx, ny = y + dy;
                        if (nx >= 0 && nx < w && ny >= 0 && ny < h
                            && map_->get(nx, ny) == Tile::Water) {
                            int d = std::max(std::abs(dx), std::abs(dy));
                            water_dist = std::min(water_dist, d);
                        }
                    }

                if (water_dist > max_lush_dist) continue;

                // Density gradient: 40% at dist 1, fading to ~5% at dist 7
                int chance = 40 - (water_dist - 1) * 6;
                if (chance < 5) chance = 5;

                if (static_cast<int>(rng() % 100) < chance) {
                    FixtureData fd;
                    fd.type = FixtureType::NaturalObstacle;
                    fd.passable = true;
                    fd.interactable = false;
                    // Vision-blocking only for the closest 2 tiles
                    fd.blocks_vision = (water_dist <= 2);
                    map_->add_fixture(x, y, fd);
                }
            }
        }
    }

    // --- POI Phase (Phase 6) ---
    if (props_->detail_has_poi) {
        Rect bounds = poi_phase(*map_, channels_, *props_, rng);
        if (!bounds.empty()) {
            map_->set_poi_bounds(bounds);
        }
    }
}

// ---------------------------------------------------------------------------
// assign_regions
// ---------------------------------------------------------------------------

void DetailMapGeneratorV2::assign_regions(std::mt19937& /*rng*/) {
    Region surface;
    surface.name = "Surface";
    surface.flavor = RoomFlavor::EmptyRoom;
    surface.lit = true;

    int rid = map_->add_region(surface);

    for (int y = 0; y < map_->height(); ++y) {
        for (int x = 0; x < map_->width(); ++x) {
            map_->set_region(x, y, rid);
        }
    }
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<MapGenerator> make_detail_map_generator_v2() {
    return std::make_unique<DetailMapGeneratorV2>();
}

} // namespace astra
