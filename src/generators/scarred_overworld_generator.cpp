#include "astra/overworld_generator.h"

namespace astra {

// ---------------------------------------------------------------------------
// ScarredOverworldGenerator — quest-forced scarred surface.
// Selected when body_biome_override is ScarredScorched or ScarredGlassed.
// Produces a nearly uniform blasted surface: glassed craters in low
// elevation / former seas, scorched earth on the plains, mountains left
// as jagged remnants.
// ---------------------------------------------------------------------------

class ScarredOverworldGenerator : public OverworldGeneratorBase {
public:
    explicit ScarredOverworldGenerator(Biome variant) : variant_(variant) {}

protected:
    void configure_noise(float& elev_scale, float& moist_scale,
                         const TerrainContext& ctx) override;
    Tile classify_terrain(int x, int y, float elev, float moist,
                          const TerrainContext& ctx) override;
    void place_pois(std::mt19937& rng) override;

private:
    Biome variant_;   // ScarredScorched or ScarredGlassed
};

void ScarredOverworldGenerator::configure_noise(float& elev_scale, float& moist_scale,
                                                const TerrainContext& /*ctx*/) {
    elev_scale = 0.05f;   // broad features
    moist_scale = 0.12f;  // unused — scar palette ignores moisture
}

Tile ScarredOverworldGenerator::classify_terrain(int /*x*/, int /*y*/,
                                                 float elev, float /*moist*/,
                                                 const TerrainContext& /*ctx*/) {
    // Jagged peaks remain regardless of variant.
    if (elev > 0.82f) return Tile::OW_Mountains;

    // Deep basins: former seas / impact scars → glassed craters.
    if (elev < 0.30f) return Tile::OW_GlassedCrater;

    // Variant: ScarredGlassed skews heavily toward glassed surfaces;
    // ScarredScorched is mostly scorched earth with occasional glass pools.
    if (variant_ == Biome::ScarredGlassed) {
        if (elev < 0.55f) return Tile::OW_GlassedCrater;
        return Tile::OW_ScorchedEarth;
    }
    // ScarredScorched default
    if (elev < 0.38f) return Tile::OW_GlassedCrater;   // rare glass pools
    return Tile::OW_ScorchedEarth;
}

void ScarredOverworldGenerator::place_pois(std::mt19937& rng) {
    // Reuse default POI logic — ruins, settlements, caves land naturally
    // on the scarred palette (Nova's Echo 1 drone site is a quest fixture,
    // placed by QuestLocationMeta.fixtures separately).
    place_default_pois(map_, props_, elevation_, rng);
}

std::unique_ptr<MapGenerator> make_scarred_overworld_generator(Biome variant) {
    return std::make_unique<ScarredOverworldGenerator>(variant);
}

} // namespace astra
