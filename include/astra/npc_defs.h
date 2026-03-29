#pragma once

#include "astra/npc.h"

#include <random>

namespace astra {

// Each NPC archetype has its own builder.
// These are called by create_npc() — add new ones as the world grows.
Npc build_station_keeper(Race race, std::mt19937& rng);
Npc build_merchant(Race race, std::mt19937& rng, int faction_rep = 0);
Npc build_drifter(Race race, std::mt19937& rng);
Npc build_xytomorph(std::mt19937& rng);

// Unique named NPC — no race/rng params needed.
Npc build_nova();

// Hub station NPC builders
Npc build_food_merchant(Race race, std::mt19937& rng, int faction_rep = 0);
Npc build_medic(Race race, std::mt19937& rng);
Npc build_commander(Race race, std::mt19937& rng);
Npc build_arms_dealer(Race race, std::mt19937& rng, int faction_rep = 0);
Npc build_astronomer(Race race, std::mt19937& rng);
Npc build_engineer(Race race, std::mt19937& rng);

} // namespace astra
