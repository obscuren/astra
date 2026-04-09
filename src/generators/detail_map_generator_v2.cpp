#include "astra/map_generator.h"
#include "astra/biome_profile.h"
#include "astra/terrain_channels.h"
#include "astra/terrain_compositor.h"
#include "astra/map_properties.h"
#include "astra/poi_phase.h"
#include "astra/edge_strip.h"

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
    void apply_edge_strips(std::mt19937& rng);
    EdgeStrip generate_synthetic_strip(Tile neighbor_tile, EdgeDirection extract_dir,
                                       std::mt19937& rng);

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

    composite_terrain(*map_, channels_, prof);

    // Apply edge strips — cached from real neighbors + synthetic for uncached
    apply_edge_strips(rng);
}

// ---------------------------------------------------------------------------
// generate_synthetic_strip — procedural fallback for uncached neighbors
// Generates the neighbor's terrain from its biome profile, composites it,
// and extracts the edge strip facing our tile.
// ---------------------------------------------------------------------------

EdgeStrip DetailMapGeneratorV2::generate_synthetic_strip(
    Tile neighbor_tile, EdgeDirection extract_dir, std::mt19937& rng) {

    const int w = map_->width();
    const int h = map_->height();

    Biome neighbor_biome = detail_biome_for_terrain(neighbor_tile, props_->biome);
    const auto& neighbor_prof = biome_profile(neighbor_biome);

    // Generate full channels using neighbor's biome profile
    TerrainChannels neighbor_channels(w, h);
    if (neighbor_prof.elevation_fn)
        neighbor_prof.elevation_fn(neighbor_channels.elevation.data(), w, h,
                                   rng, neighbor_prof);
    if (neighbor_prof.moisture_fn)
        neighbor_prof.moisture_fn(neighbor_channels.moisture.data(), w, h, rng,
                                  neighbor_channels.elevation.data(), neighbor_prof);
    if (neighbor_prof.structure_fn)
        neighbor_prof.structure_fn(neighbor_channels.structure.data(), w, h, rng,
                                   neighbor_channels.elevation.data(),
                                   neighbor_channels.moisture.data(), neighbor_prof);

    // Composite into a temporary TileMap
    TileMap temp_map(w, h, MapType::DetailMap);
    composite_terrain(temp_map, neighbor_channels, neighbor_prof);

    // Run scatter so open biomes (sandy, grassland) get their identity fixtures
    for (const auto& entry : neighbor_prof.scatter) {
        if (entry.density <= 0.0f) continue;
        int threshold = static_cast<int>(entry.density * 100.0f);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (temp_map.get(x, y) != Tile::Floor) continue;
                if (temp_map.fixture_id(x, y) >= 0) continue;
                if (static_cast<int>(rng() % 100) < threshold) {
                    FixtureData fd;
                    fd.type = entry.type;
                    fd.passable = true;
                    fd.interactable = false;
                    fd.blocks_vision = entry.blocks_vision;
                    temp_map.add_fixture(x, y, fd);
                }
            }
        }
    }

    // Extract the edge facing our tile
    return extract_edge_strip(temp_map, extract_dir, 20);
}

// ---------------------------------------------------------------------------
// apply_edge_strips
// ---------------------------------------------------------------------------

void DetailMapGeneratorV2::apply_edge_strips(std::mt19937& rng) {
    static constexpr int verbatim_depth = 5;
    static constexpr int total_depth = 20;

    const int w = map_->width();
    const int h = map_->height();

    // Cached strips from props (real neighbor data)
    const std::optional<EdgeStrip>* cached[4] = {
        &props_->edge_strip_n,
        &props_->edge_strip_s,
        &props_->edge_strip_e,
        &props_->edge_strip_w,
    };

    // Neighbor overworld tiles (for synthetic generation)
    const Tile neighbor_tiles[4] = {
        props_->detail_neighbor_n,
        props_->detail_neighbor_s,
        props_->detail_neighbor_w,
        props_->detail_neighbor_e,
    };

    // The edge of the neighbor's map that faces us
    // Our north neighbor's south edge faces us, etc.
    const EdgeDirection facing_edge[4] = {
        EdgeDirection::South,  // our north → neighbor's south
        EdgeDirection::North,  // our south → neighbor's north
        EdgeDirection::East,   // our west → neighbor's east
        EdgeDirection::West,   // our east → neighbor's west
    };

    // Generate synthetic strips for uncached neighbors with different biomes
    std::optional<EdgeStrip> synthetic[4];
    for (int dir = 0; dir < 4; ++dir) {
        if (cached[dir]->has_value()) continue;
        if (neighbor_tiles[dir] == Tile::Empty) continue;

        Biome neighbor_biome = detail_biome_for_terrain(neighbor_tiles[dir],
                                                         props_->biome);
        if (neighbor_biome == props_->biome) continue;

        // Deterministic seed per direction
        unsigned dir_seed = rng() ^ (static_cast<unsigned>(dir) * 7919u);
        std::mt19937 synth_rng(dir_seed);

        synthetic[dir] = generate_synthetic_strip(
            neighbor_tiles[dir], facing_edge[dir], synth_rng);
    }

    // Process all strips: N(0), S(1), E(2), W(3)
    // Later overwrites earlier in corners
    for (int dir = 0; dir < 4; ++dir) {
        const EdgeStrip* strip_ptr = nullptr;
        if (cached[dir]->has_value()) {
            strip_ptr = &cached[dir]->value();
        } else if (synthetic[dir].has_value()) {
            strip_ptr = &synthetic[dir].value();
        } else {
            continue;
        }
        const EdgeStrip& strip = *strip_ptr;

        // Deterministic RNG for blend probability
        unsigned strip_seed = rng() ^ (static_cast<unsigned>(dir) * 4973u);
        std::mt19937 blend_rng(strip_seed);

        int max_depth = std::min(strip.depth, total_depth);

        for (int d = 0; d < max_depth; ++d) {
            for (int a = 0; a < strip.length; ++a) {
                // Map strip coordinates to map coordinates
                int x = 0, y = 0;
                switch (dir) {
                    case 0: // North: strip boundary → map row 0
                        x = a; y = d;
                        break;
                    case 1: // South: strip boundary → map last row
                        x = a; y = (h - 1) - d;
                        break;
                    case 2: // East: strip boundary → map last col
                        x = (w - 1) - d; y = a;
                        break;
                    case 3: // West: strip boundary → map col 0
                        x = d; y = a;
                        break;
                }

                if (x < 0 || x >= w || y < 0 || y >= h) continue;

                const EdgeStripCell& src = strip.at(d, a);

                if (d < verbatim_depth) {
                    // Phase 1: Verbatim stamp
                    map_->set(x, y, src.tile);

                    // Remove existing fixture if any, then place strip's fixture
                    if (map_->fixture_id(x, y) >= 0) {
                        map_->remove_fixture(x, y);
                    }
                    if (src.fixture.has_value()) {
                        map_->add_fixture(x, y, src.fixture.value());
                    }

                    map_->set_glyph_override(x, y, src.glyph_override);
                    map_->set_custom_flags_byte(x, y, src.custom_flags);
                } else {
                    // Phase 2: Weighted blend
                    float t = 1.0f - (static_cast<float>(d - verbatim_depth) /
                                      static_cast<float>(total_depth - verbatim_depth));
                    t = t * t; // quadratic falloff

                    // Probability roll for this cell
                    float roll = static_cast<float>(blend_rng() % 10000) / 10000.0f;

                    bool stamp_tile = false;

                    // Walls, structural tiles, water: stamp if they differ from generated
                    if (src.tile == Tile::Wall || src.tile == Tile::StructuralWall ||
                        src.tile == Tile::IndoorFloor || src.tile == Tile::Path ||
                        src.tile == Tile::Water) {
                        if (map_->get(x, y) == Tile::Floor && roll < t) {
                            map_->set(x, y, src.tile);
                            stamp_tile = true;
                        }
                    }

                    // Fixtures: place if strip has one and generated cell doesn't
                    if (src.fixture.has_value() && map_->fixture_id(x, y) < 0) {
                        float fixture_roll = static_cast<float>(blend_rng() % 10000) / 10000.0f;
                        if (fixture_roll < t) {
                            map_->add_fixture(x, y, src.fixture.value());
                        }
                    }

                    // Glyph override: copy where tile was also stamped
                    if (stamp_tile && src.glyph_override != 0) {
                        map_->set_glyph_override(x, y, src.glyph_override);
                    }
                }
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

    // --- Flora Phase (ground resources) ---
    if (prof.flora_fn) {
        prof.flora_fn(*map_, w, h, rng,
                      channels_.elevation.data(),
                      channels_.moisture.data(),
                      prof);
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
