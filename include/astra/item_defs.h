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

// --- Melee weapons ---
Item build_combat_knife();
Item build_vibro_blade();
Item build_plasma_saber();
Item build_stun_baton();
Item build_ancient_mono_edge();

// --- Armor ---
Item build_padded_vest();
Item build_composite_armor();
Item build_exo_suit();
Item build_flight_helmet();
Item build_tactical_helmet();
Item build_combat_boots();
Item build_mag_lock_boots();
Item build_arm_guard();
Item build_riot_shield();

// --- Accessories ---
Item build_recon_visor();
Item build_night_goggles();
Item build_jetpack();
Item build_cargo_pack();

// --- Grenades ---
Item build_frag_grenade();
Item build_emp_grenade();
Item build_cryo_grenade();

// --- Consumables ---
Item build_battery();
Item build_ration_pack();
Item build_combat_stim();

// --- Junk ---
Item build_scrap_metal();
Item build_broken_circuit();
Item build_empty_casing();

// --- Crafting materials ---
Item build_nano_fiber();
Item build_power_core();
Item build_circuit_board();
Item build_alloy_ingot();

// --- Ship components ---
Item build_hull_plate();
Item build_shield_generator();
Item build_navi_computer_mk2();

// Random item pickers
Item random_ranged_weapon(std::mt19937& rng);
Item random_melee_weapon(std::mt19937& rng);
Item random_armor(std::mt19937& rng);
Item random_junk(std::mt19937& rng);

// Merchant stock generators (faction_rep controls tiered availability)
std::vector<Item> generate_merchant_stock(std::mt19937& rng, int faction_rep = 0);
std::vector<Item> generate_arms_dealer_stock(std::mt19937& rng, int faction_rep = 0);
std::vector<Item> generate_food_merchant_stock(std::mt19937& rng, int faction_rep = 0);

} // namespace astra
