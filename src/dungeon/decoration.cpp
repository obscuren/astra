#include "astra/dungeon/decoration.h"

#include "astra/dungeon_recipe.h"
#include "astra/ruin_decay.h"
#include "astra/ruin_types.h"
#include "astra/tilemap.h"

#include <algorithm>

namespace astra::dungeon {

namespace {

void decorate_ruin_debris(TileMap& map, const CivConfig& civ,
                          int decay_level, std::mt19937& rng) {
    // Map decay_level (0..3) to intensity (0.0..1.0).
    const float intensity = std::clamp(
        static_cast<float>(decay_level) / 3.0f, 0.0f, 1.0f);
    apply_decay(map, civ, intensity, rng);
}

void decorate_natural_minimal(TileMap& map, const CivConfig& civ,
                              int decay_level, std::mt19937& rng) {
    (void)map; (void)civ; (void)decay_level; (void)rng;
    // Deferred: cave flora / mushroom stamps. Reserved for cave slice.
}

void decorate_station_scrap(TileMap&, const CivConfig&, int, std::mt19937&) {
    // Deferred: station slice.
}

void decorate_cave_flora(TileMap&, const CivConfig&, int, std::mt19937&) {
    // Deferred: cave slice.
}

} // namespace

void apply_decoration(TileMap& map, const DungeonStyle& style,
                      const CivConfig& civ, const DungeonLevelSpec& spec,
                      std::mt19937& rng) {
    const std::string& pack = style.decoration_pack;
    if      (pack == "ruin_debris")      decorate_ruin_debris(map, civ, spec.decay_level, rng);
    else if (pack == "natural_minimal")  decorate_natural_minimal(map, civ, spec.decay_level, rng);
    else if (pack == "station_scrap")    decorate_station_scrap(map, civ, spec.decay_level, rng);
    else if (pack == "cave_flora")       decorate_cave_flora(map, civ, spec.decay_level, rng);
    // Unknown pack: silently skip.
}

} // namespace astra::dungeon
