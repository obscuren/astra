#pragma once

#include "astra/item.h"

#include <random>

namespace astra {

// --- Ranged weapons ---
Item build_plasma_pistol();
Item build_ion_blaster();
Item build_pulse_rifle();
Item build_arc_caster();
Item build_void_lance();

// --- Consumables ---
Item build_battery();
Item build_ration_pack();
Item build_combat_stim();

// Pick a random ranged weapon from the catalog.
Item random_ranged_weapon(std::mt19937& rng);

} // namespace astra
