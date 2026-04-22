#pragma once

#include "astra/dungeon/dungeon_style.h"

#include <random>

namespace astra {
class TileMap;
struct CivConfig;
struct DungeonLevelSpec;
}

namespace astra::dungeon {

// Layer 5: dispatches on style.decoration_pack. Uses civ palette /
// furniture prefs + spec.decay_level.
void apply_decoration(TileMap& map, const DungeonStyle& style,
                      const CivConfig& civ, const DungeonLevelSpec& spec,
                      std::mt19937& rng);

} // namespace astra::dungeon
