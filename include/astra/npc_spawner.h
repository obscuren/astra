#pragma once

#include "astra/npc.h"
#include "astra/tilemap.h"

#include <random>
#include <vector>

namespace astra {

// Spawn NPCs in hub station rooms based on room flavor.
void spawn_hub_npcs(TileMap& map, std::vector<Npc>& npcs,
                    int player_x, int player_y, std::mt19937& rng);

} // namespace astra
