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
// --- Energy shields ---
Item build_basic_deflector();
Item build_plasma_screen();
Item build_ion_barrier();
Item build_composite_barrier();
Item build_hardlight_aegis();
Item build_void_mantle();
Item random_shield(std::mt19937& rng);

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

// --- Ingredients ---
Item build_raw_meat();
Item build_carrot();
Item build_flour();
Item build_herbs();
Item build_synth_protein();

// --- Cooked dishes ---
Item build_cooked_meat();
Item build_bowl_of_broth();
Item build_flatbread();
Item build_hearty_stew();
Item build_protein_bake();
Item build_heros_feast();
Item build_burnt_slop();

// --- Cookbooks ---
Item build_cookbook_hearty_stew();
Item build_cookbook_protein_bake();
Item build_cookbook_heros_feast();

// --- Junk ---
Item build_scrap_metal();
Item build_broken_circuit();
Item build_empty_casing();

// --- Salvage ---
Item build_spare_parts();
Item build_circuitry();

// --- Crafting materials ---
Item build_nano_fiber();
Item build_power_core();
Item build_circuit_board();
Item build_alloy_ingot();

// --- Ship components ---
Item build_engine_coil_mk1();
Item build_hull_plate();
Item build_shield_generator();
Item build_navi_computer_mk2();

// Reconstruct an Item from its item_def_id by dispatching to the
// appropriate build_* function. Only cooking-related defs are
// supported today (ingredients, cooked dishes, Burnt Slop,
// cookbooks). Returns a default Item{} if id is unknown.
Item build_by_def_id(uint16_t def_id);

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
