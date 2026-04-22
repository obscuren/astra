#pragma once

#include "astra/dungeon/dungeon_style.h"
#include "astra/dungeon/level_context.h"

#include <random>

namespace astra { class TileMap; }

namespace astra::dungeon {

// Layer 3: if style.connectivity_required, verifies that every tagged
// region is reachable from ctx.entry_region_id. On failure, logs a
// warning in dev mode. In slice 1 this is a no-op for unconnected
// layouts (none are registered).
void apply_connectivity(TileMap& map, const DungeonStyle& style,
                        LevelContext& ctx, std::mt19937& rng);

} // namespace astra::dungeon
