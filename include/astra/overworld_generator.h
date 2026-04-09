#pragma once

#include "astra/map_generator.h"
#include "astra/celestial_body.h"

#include <vector>

namespace astra {

// Context passed to terrain classification and river generation
struct TerrainContext {
    BodyType body_type;
    Atmosphere atmosphere;
    Temperature temperature;
};

// ---------------------------------------------------------------------------
// OverworldGeneratorBase — template-method base for all overworld generators.
// generate_layout() is non-virtual and calls virtual hooks in pipeline order.
// Subclasses override hooks to customize terrain for specific body types.
// ---------------------------------------------------------------------------

class OverworldGeneratorBase : public MapGenerator {
protected:
    // --- Non-virtual pipeline (template method) ---
    void generate_layout(std::mt19937& rng) override final;
    void connect_rooms(std::mt19937& /*rng*/) override {}
    void place_features(std::mt19937& rng) override;
    void assign_regions(std::mt19937& rng) override;
    void generate_backdrop(unsigned /*seed*/) override {}

    // --- Virtual hooks (override in subclasses) ---

    // Set noise scales for elevation and moisture. Default: 0.08/0.12.
    virtual void configure_noise(float& elev_scale, float& moist_scale,
                                 const TerrainContext& ctx);

    // Classify a single cell to a terrain tile.
    virtual Tile classify_terrain(int x, int y, float elev, float moist,
                                  const TerrainContext& ctx) = 0;

    // Carve rivers into the map. Default: no-op.
    virtual void carve_rivers(std::mt19937& rng);

    // Place POIs (settlements, ruins, caves, etc.). Default: no-op.
    virtual void place_pois(std::mt19937& rng);

    // --- Shared utilities (available to subclasses) ---

    void apply_lore_overlays(std::mt19937& rng);
    void place_landing_pad();
    void ensure_connectivity();

    // --- Shared state (populated during generate_layout) ---
    TerrainContext ctx_;
    std::vector<float> elevation_;
    std::vector<float> moisture_;
    int land_x_ = 0;
    int land_y_ = 0;
};

// --- Noise helpers (used by classifiers and base) ---
float ow_fbm(float x, float y, unsigned seed, float scale, int octaves = 4);

// --- Factory ---
std::unique_ptr<MapGenerator> make_overworld_generator();

} // namespace astra
