#pragma once

#include "astra/npc.h"
#include "astra/tilemap.h"

#include <random>
#include <vector>

namespace astra {

struct Player;

// Spawn NPCs in hub station rooms based on room flavor.
// Player is used for faction reputation (affects shop stock tiers).
void spawn_hub_npcs(TileMap& map, std::vector<Npc>& npcs,
                    int player_x, int player_y, std::mt19937& rng,
                    const Player* player = nullptr);

// Spawn NPCs in settlement detail maps (near fixtures).
void spawn_settlement_npcs(TileMap& map, std::vector<Npc>& npcs,
                           int player_x, int player_y, std::mt19937& rng,
                           const Player* player = nullptr);

// Spawn NPCs in outpost detail maps (near fixtures).
void spawn_outpost_npcs(TileMap& map, std::vector<Npc>& npcs,
                        int player_x, int player_y, std::mt19937& rng,
                        const Player* player = nullptr);

} // namespace astra
