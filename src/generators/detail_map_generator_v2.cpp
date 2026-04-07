#include "astra/map_generator.h"
#include "astra/biome_profile.h"
#include "astra/terrain_channels.h"
#include "astra/terrain_compositor.h"
#include "astra/map_properties.h"

namespace astra {

class DetailMapGeneratorV2 : public MapGenerator {
protected:
    void generate_layout(std::mt19937& rng) override;
    void connect_rooms(std::mt19937& /*rng*/) override {}
    void place_features(std::mt19937& rng) override;
    void assign_regions(std::mt19937& rng) override;
    void generate_backdrop(unsigned /*seed*/) override {}

private:
    void apply_neighbor_bleed(TerrainChannels& channels, std::mt19937& rng);
};

// ---------------------------------------------------------------------------
// generate_layout
// ---------------------------------------------------------------------------

void DetailMapGeneratorV2::generate_layout(std::mt19937& rng) {
    const auto& prof = biome_profile(props_->biome);
    const int w = map_->width();
    const int h = map_->height();

    TerrainChannels channels(w, h);

    if (prof.elevation_fn) {
        prof.elevation_fn(channels.elevation.data(), w, h, rng, prof);
    }

    if (prof.moisture_fn) {
        prof.moisture_fn(channels.moisture.data(), w, h, rng,
                         channels.elevation.data(), prof);
    }

    if (prof.structure_fn) {
        prof.structure_fn(channels.structure.data(), w, h, rng,
                          channels.elevation.data(), channels.moisture.data(), prof);
    }

    apply_neighbor_bleed(channels, rng);

    composite_terrain(*map_, channels, prof);
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

    // --- Water-proximity scatter: tall reeds/plants along shorelines ---
    // Floor tiles adjacent to water get dense vision-blocking vegetation.
    // Creates natural shoreline effect for any biome with water.
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (map_->get(x, y) != Tile::Floor) continue;
            if (map_->fixture_id(x, y) >= 0) continue;

            // Check if adjacent to water (4-connected)
            bool near_water = false;
            if (x > 0     && map_->get(x - 1, y) == Tile::Water) near_water = true;
            if (x < w - 1 && map_->get(x + 1, y) == Tile::Water) near_water = true;
            if (y > 0     && map_->get(x, y - 1) == Tile::Water) near_water = true;
            if (y < h - 1 && map_->get(x, y + 1) == Tile::Water) near_water = true;

            if (!near_water) continue;

            // ~40% chance for shoreline vegetation
            if (static_cast<int>(rng() % 100) < 40) {
                FixtureData fd;
                fd.type = FixtureType::NaturalObstacle;
                fd.passable = true;
                fd.interactable = false;
                fd.blocks_vision = true;
                map_->add_fixture(x, y, fd);
            }
        }
    }

    // --- Second ring: lighter scatter 2 tiles from water ---
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (map_->get(x, y) != Tile::Floor) continue;
            if (map_->fixture_id(x, y) >= 0) continue;

            // Check 2-tile range for water
            bool near_water = false;
            for (int dy = -2; dy <= 2 && !near_water; ++dy)
                for (int dx = -2; dx <= 2 && !near_water; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h
                        && map_->get(nx, ny) == Tile::Water)
                        near_water = true;
                }

            if (!near_water) continue;

            // ~20% chance for shorter vegetation (not vision-blocking)
            if (static_cast<int>(rng() % 100) < 20) {
                FixtureData fd;
                fd.type = FixtureType::NaturalObstacle;
                fd.passable = true;
                fd.interactable = false;
                fd.blocks_vision = false;
                map_->add_fixture(x, y, fd);
            }
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
