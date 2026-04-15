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
    if (elev > 0.85f) return Tile::OW_Mountains;

    // GlassedCrater is impassable; keep it to distinct features only so
    // the player never spawns boxed in. Scorched earth (passable) is the
    // dominant terrain on both variants.
    if (variant_ == Biome::ScarredGlassed) {
        // Heavier glass presence: scattered craters at low AND very high
        // mid-elevation, but the mid band stays traversable.
        if (elev < 0.22f) return Tile::OW_GlassedCrater;   // crater basins
        if (elev > 0.78f) return Tile::OW_GlassedCrater;   // fused plateaus
        return Tile::OW_ScorchedEarth;
    }
    // ScarredScorched: sparse crater pits.
    if (elev < 0.18f) return Tile::OW_GlassedCrater;
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
