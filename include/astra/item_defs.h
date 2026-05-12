#pragma once

#include "astra/item.h"

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

// --- Accessories ---
Item build_recon_visor();
Item build_nightvision_goggles();
Item build_jetpack();
Item build_cargo_pack();

// --- Grenades ---
Item build_frag_grenade();
Item build_emp_grenade();
Item build_cryo_grenade();

// --- Consumables ---
Item build_battery();
Item build_small_energy_cell();
Item build_standard_energy_cell();
Item build_large_energy_cell();
Item build_industrial_energy_cell();
Item build_antimatter_cell();
Item build_bulwark_cell();
Item build_volatile_cell();
Item build_adrenal_cell();
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

// --- Energy mods (tinkering materials for cells) ---
Item build_solar_panel_common();
Item build_solar_panel_uncommon();
Item build_solar_panel_rare();
Item build_capacitor_coil();
Item build_charge_catalyst();
Item build_polished_conduit();
Item build_reinforced_casing();
Item build_receptor_plate();
Item build_brass_conduit();
Item build_power_junction();
Item build_tuned_catalyst();

// --- Accessory modules ---
Item build_ai_module();
Item build_light_sensor();

// --- Ship components ---
Item build_engine_coil_mk1();
Item build_hull_plate();
Item build_shield_generator();
Item build_navi_computer_mk2();

// --- Cyberdecks ---
Item build_pidgin_mk1();
Item build_polyglot_dck2();

// --- Code fragments ---
Item build_code_fragment_t1();
Item build_code_fragment_t2();
Item build_code_fragment_t3();

// --- Implants ---
Item build_neural_backup();

// --- Relay Cortex variants ---
Item build_relay_cortex_mk1();
Item build_spike_cortex();
Item build_glacier_cortex();
Item build_sentinel_cortex();
Item build_acuity_cortex();
Item build_stoic_cortex();

// --- Hacker mats ---
Item build_program_disk();

// --- Cyberdeck mods (Plan 7 §15) ---
Item build_aerojack();
Item build_untether();

// --- Programs ---
Item build_program_icebreaker_lite();
Item build_program_ghost_trace();
Item build_program_cooldown();
Item build_program_breach();
Item build_program_decrypt();
Item build_program_reboot_optics();
Item build_program_friendly_fire();
Item build_program_data_leech();
Item build_program_pulse_hammer();    // Plan 4 — T3 ATK AoE
Item build_program_daemon_hijack();   // Plan 4 — T3 UTL charm
// Sigils — Spec 1 §5.2
Item build_program_echo();
Item build_program_lag();
Item build_program_veil();
Item build_program_jitter();
Item build_program_shroud();
Item build_program_worm();
Item build_program_brick();
Item build_program_rot();
Item build_program_spike();

// Universal item constructor: rebuild an Item from its item_def_id by
// dispatching to the appropriate build_*() function. Covers every item
// in s_loot_table plus a few legacy/utility defs (ITEM_BATTERY alias,
// cookbook recipes, ingredients, dishes). Returns a default Item{}
// if def_id is unknown.
Item build_by_def_id(uint16_t def_id);

} // namespace astra
