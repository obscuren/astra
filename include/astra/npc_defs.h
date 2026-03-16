#pragma once

#include "astra/npc.h"

#include <random>

namespace astra {

// Each NPC archetype has its own builder.
// These are called by create_npc() — add new ones as the world grows.
Npc build_station_keeper(Race race, std::mt19937& rng);
Npc build_merchant(Race race, std::mt19937& rng);
Npc build_drifter(Race race, std::mt19937& rng);
Npc build_xytomorph(std::mt19937& rng);

} // namespace astra
