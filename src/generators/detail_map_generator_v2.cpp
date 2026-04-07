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
    void place_features(std::mt19937& /*rng*/) override {}
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
