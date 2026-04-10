#include "astra/overworld_generator.h"

#include <cmath>

namespace astra {

// ---------------------------------------------------------------------------
// ColdRockyOverworldGenerator — Mars-like cold rocky planets with thin atmo.
// Rust-colored barren terrain with polar ice caps, craters, and mountains.
// Dispatched for Rocky + Thin atmosphere + Cold bodies.
// ---------------------------------------------------------------------------

class ColdRockyOverworldGenerator : public OverworldGeneratorBase {
protected:
    void configure_noise(float& elev_scale, float& moist_scale,
                         const TerrainContext& ctx) override;
    Tile classify_terrain(int x, int y, float elev, float moist,
                          const TerrainContext& ctx) override;
    void place_pois(std::mt19937& rng) override;
};

// ---------------------------------------------------------------------------
// Noise configuration
// ---------------------------------------------------------------------------

void ColdRockyOverworldGenerator::configure_noise(float& elev_scale, float& moist_scale,
                                                    const TerrainContext& /*ctx*/) {
    elev_scale = 0.09f;
    moist_scale = 0.12f;  // unused — no moisture-based biomes
}

// ---------------------------------------------------------------------------
// classify_terrain — latitude-based ice caps + elevation-based rocky terrain
// ---------------------------------------------------------------------------

Tile ColdRockyOverworldGenerator::classify_terrain(int /*x*/, int y, float elev, float /*moist*/,
                                                    const TerrainContext& /*ctx*/) {
    int h = map_->height();

    // Latitude: 0 at equator, 1 at poles
    float lat_from_center = std::abs(static_cast<float>(y) / static_cast<float>(h - 1) - 0.5f);
    float polar_factor = lat_from_center * 2.0f;

    // --- Polar ice caps ---
    if (polar_factor > 0.85f) {
        if (elev > 0.7f) return Tile::OW_Mountains;  // peaks poke through ice
        return Tile::OW_IceField;
    }
    if (polar_factor > 0.75f) {
        // Sub-polar transition zone
        if (elev > 0.65f) return Tile::OW_Mountains;
        if (elev > 0.5f) return Tile::OW_IceField;
        return Tile::OW_Barren;
    }

    // --- Main rocky terrain ---
    if (elev > 0.72f) return Tile::OW_Mountains;
    if (elev > 0.58f) return Tile::OW_Crater;      // impact craters at mid-high elevation
    if (elev < 0.28f) return Tile::OW_Crater;      // deep basins/valleys as craters
    return Tile::OW_Barren;                         // default rust-red flatland
}

// ---------------------------------------------------------------------------
// place_pois — delegate to shared default POI logic
// ---------------------------------------------------------------------------

void ColdRockyOverworldGenerator::place_pois(std::mt19937& rng) {
    place_default_pois(map_, props_, elevation_, rng);
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<MapGenerator> make_cold_rocky_overworld_generator() {
    return std::make_unique<ColdRockyOverworldGenerator>();
}

} // namespace astra
