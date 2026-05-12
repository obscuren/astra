#pragma once

#include <cstdint>

namespace astra {

// Item definition IDs — each unique item definition gets a fixed number.
// ID 0 = unknown/unset. Defined items start at 1.
// Range 1-999: hand-crafted items. 1000+: reserved for synthesized/dynamic items.

// Ranged weapons (1-5)
constexpr uint16_t ITEM_PLASMA_PISTOL       = 1;
constexpr uint16_t ITEM_ION_BLASTER         = 2;
constexpr uint16_t ITEM_PULSE_RIFLE         = 3;
constexpr uint16_t ITEM_ARC_CASTER          = 4;
constexpr uint16_t ITEM_VOID_LANCE          = 5;

// Consumables (6-8)
constexpr uint16_t ITEM_BATTERY             = 6;
constexpr uint16_t ITEM_RATION_PACK         = 7;
constexpr uint16_t ITEM_COMBAT_STIM         = 8;

// Melee weapons (9-13)
constexpr uint16_t ITEM_COMBAT_KNIFE        = 9;
constexpr uint16_t ITEM_VIBRO_BLADE         = 10;
constexpr uint16_t ITEM_PLASMA_SABER        = 11;
constexpr uint16_t ITEM_STUN_BATON          = 12;
constexpr uint16_t ITEM_ANCIENT_MONO_EDGE   = 13;

// Armor (14-22)
constexpr uint16_t ITEM_PADDED_VEST         = 14;
constexpr uint16_t ITEM_COMPOSITE_ARMOR     = 15;
constexpr uint16_t ITEM_EXO_SUIT            = 16;
constexpr uint16_t ITEM_FLIGHT_HELMET       = 17;
constexpr uint16_t ITEM_TACTICAL_HELMET     = 18;
constexpr uint16_t ITEM_COMBAT_BOOTS        = 19;
constexpr uint16_t ITEM_MAG_LOCK_BOOTS      = 20;
constexpr uint16_t ITEM_ARM_GUARD           = 21;
constexpr uint16_t ITEM_RIOT_SHIELD         = 22;

// Accessories (23-26)
constexpr uint16_t ITEM_RECON_VISOR         = 23;
constexpr uint16_t ITEM_NIGHT_GOGGLES       = 24;
constexpr uint16_t ITEM_JETPACK             = 25;
constexpr uint16_t ITEM_CARGO_PACK          = 26;

// Grenades (27-29)
constexpr uint16_t ITEM_FRAG_GRENADE        = 27;
constexpr uint16_t ITEM_EMP_GRENADE         = 28;
constexpr uint16_t ITEM_CRYO_GRENADE        = 29;

// Junk (30-32)
constexpr uint16_t ITEM_SCRAP_METAL         = 30;
constexpr uint16_t ITEM_BROKEN_CIRCUIT      = 31;
constexpr uint16_t ITEM_EMPTY_CASING        = 32;

// Crafting materials — T2 existing (33-36)
constexpr uint16_t ITEM_NANO_FIBER          = 33;
constexpr uint16_t ITEM_POWER_CORE          = 34;
constexpr uint16_t ITEM_CIRCUIT_BOARD       = 35;
constexpr uint16_t ITEM_ALLOY_INGOT         = 36;

// Crafting materials — T1 new (96-100)
constexpr uint16_t ITEM_COPPER_WIRE             = 96;
constexpr uint16_t ITEM_POLYMER_STRIP           = 97;
constexpr uint16_t ITEM_GLASS_SHARD             = 98;
constexpr uint16_t ITEM_ADHESIVE_RESIN          = 99;
constexpr uint16_t ITEM_COOLANT_VIAL            = 100;

// Crafting materials — T2 new (101-104)
constexpr uint16_t ITEM_NANO_LATTICE            = 101;
constexpr uint16_t ITEM_POLISHED_LENS           = 102;
constexpr uint16_t ITEM_MICRO_SERVO             = 103;
constexpr uint16_t ITEM_PLASMA_CARTRIDGE        = 104;

// Crafting materials — T3 (105-110)
constexpr uint16_t ITEM_QUANTUM_RESONANCE_CRYSTAL = 105;
constexpr uint16_t ITEM_STRANGE_STROBING_CRYSTAL  = 106;
constexpr uint16_t ITEM_PRIME_CATALYST            = 107;
constexpr uint16_t ITEM_PRIME_FILAMENT            = 108;
constexpr uint16_t ITEM_VOIDSHARD                 = 109;
constexpr uint16_t ITEM_PHASE_COIL                = 110;

// Ship components (37-40)
constexpr uint16_t ITEM_ENGINE_COIL_MK1     = 37;
constexpr uint16_t ITEM_HULL_PLATE          = 38;
constexpr uint16_t ITEM_SHIELD_GENERATOR    = 39;
constexpr uint16_t ITEM_NAVI_COMPUTER_MK2   = 40;

// Energy shields (41-46)
constexpr uint16_t ITEM_BASIC_DEFLECTOR    = 41;
constexpr uint16_t ITEM_PLASMA_SCREEN      = 42;
constexpr uint16_t ITEM_ION_BARRIER        = 43;
constexpr uint16_t ITEM_COMPOSITE_BARRIER  = 44;
constexpr uint16_t ITEM_HARDLIGHT_AEGIS    = 45;
constexpr uint16_t ITEM_VOID_MANTLE        = 46;

// Salvage resources (47-48)
constexpr uint16_t ITEM_SPARE_PARTS         = 47;
constexpr uint16_t ITEM_CIRCUITRY           = 48;

// Cooking ingredients (49-53)
constexpr uint16_t ITEM_RAW_MEAT            = 49;
constexpr uint16_t ITEM_CARROT              = 50;
constexpr uint16_t ITEM_FLOUR               = 51;
constexpr uint16_t ITEM_HERBS               = 52;
constexpr uint16_t ITEM_SYNTH_PROTEIN       = 53;

// Cooked dishes (54-59)
constexpr uint16_t ITEM_COOKED_MEAT         = 54;
constexpr uint16_t ITEM_BOWL_OF_BROTH       = 55;
constexpr uint16_t ITEM_FLATBREAD           = 56;
constexpr uint16_t ITEM_HEARTY_STEW         = 57;
constexpr uint16_t ITEM_PROTEIN_BAKE        = 58;
constexpr uint16_t ITEM_HEROS_FEAST         = 59;

// Burnt Slop (60) — failure output from mismatched recipes
constexpr uint16_t ITEM_BURNT_SLOP          = 60;

// Cookbooks (61-63)
constexpr uint16_t ITEM_COOKBOOK_HEARTY_STEW   = 61;
constexpr uint16_t ITEM_COOKBOOK_PROTEIN_BAKE  = 62;
constexpr uint16_t ITEM_COOKBOOK_HEROS_FEAST   = 63;

// Solar panel mods (80-82)
constexpr uint16_t ITEM_SOLAR_PANEL_COMMON      = 80;
constexpr uint16_t ITEM_SOLAR_PANEL_UNCOMMON    = 81;
constexpr uint16_t ITEM_SOLAR_PANEL_RARE        = 82;

// Energy enhancement mods (83-85)
constexpr uint16_t ITEM_CAPACITOR_COIL          = 83;
constexpr uint16_t ITEM_CHARGE_CATALYST         = 84;
constexpr uint16_t ITEM_POLISHED_CONDUIT        = 85;

// Legendary specialty cells with procs (86-88)
constexpr uint16_t ITEM_BULWARK_CELL            = 86;
constexpr uint16_t ITEM_VOLATILE_CELL           = 87;
constexpr uint16_t ITEM_ADRENAL_CELL            = 88;

// Minor energy mods for cell customization (89-93)
constexpr uint16_t ITEM_REINFORCED_CASING       = 89;
constexpr uint16_t ITEM_RECEPTOR_PLATE          = 90;
constexpr uint16_t ITEM_BRASS_CONDUIT           = 91;
constexpr uint16_t ITEM_POWER_JUNCTION          = 92;
constexpr uint16_t ITEM_TUNED_CATALYST          = 93;

// Accessory modules (94-95)
constexpr uint16_t ITEM_AI_MODULE               = 94;
constexpr uint16_t ITEM_LIGHT_SENSOR            = 95;

// Energy cells (70-74)
constexpr uint16_t ITEM_SMALL_ENERGY_CELL       = 70;
constexpr uint16_t ITEM_STANDARD_ENERGY_CELL    = 71;
constexpr uint16_t ITEM_LARGE_ENERGY_CELL       = 72;
constexpr uint16_t ITEM_INDUSTRIAL_ENERGY_CELL  = 73;
constexpr uint16_t ITEM_ANTIMATTER_CELL         = 74;

// Consumables — schematic-craftable (200-212)
constexpr uint16_t ITEM_HEALING_STIM            = 200;
constexpr uint16_t ITEM_ENDURE_STIM             = 201;
constexpr uint16_t ITEM_FOCUS_STIM              = 202;
constexpr uint16_t ITEM_BERSERKER_STIM          = 203;
constexpr uint16_t ITEM_MEDKIT                  = 204;
constexpr uint16_t ITEM_INCENDIARY_GRENADE      = 205;
constexpr uint16_t ITEM_SMOKE_GRENADE           = 206;
constexpr uint16_t ITEM_FLASHBANG               = 207;
constexpr uint16_t ITEM_PROXIMITY_MINE          = 208;
constexpr uint16_t ITEM_EMP_MINE                = 209;
constexpr uint16_t ITEM_INCENDIARY_MINE         = 210;
constexpr uint16_t ITEM_DECOY_MINE              = 211;
constexpr uint16_t ITEM_CALTROPS                = 212;

// Turrets — deployable autonomous defenders (213-216)
constexpr uint16_t ITEM_AUTO_TURRET             = 213;
constexpr uint16_t ITEM_FLAME_TURRET            = 214;
constexpr uint16_t ITEM_ARC_TURRET              = 215;
constexpr uint16_t ITEM_SENTRY_DRONE            = 216;

// Schematics (220-235) — single-use pickup, teaches a recipe permanently
constexpr uint16_t ITEM_SCHEM_HEALING_STIM        = 220;
constexpr uint16_t ITEM_SCHEM_ADRENALINE_STIM     = 221;
constexpr uint16_t ITEM_SCHEM_ENDURE_STIM         = 222;
constexpr uint16_t ITEM_SCHEM_FOCUS_STIM          = 223;
constexpr uint16_t ITEM_SCHEM_BERSERKER_STIM      = 224;
constexpr uint16_t ITEM_SCHEM_MEDKIT              = 225;
constexpr uint16_t ITEM_SCHEM_FRAG_GRENADE        = 226;
constexpr uint16_t ITEM_SCHEM_EMP_GRENADE         = 227;
constexpr uint16_t ITEM_SCHEM_INCENDIARY_GRENADE  = 228;
constexpr uint16_t ITEM_SCHEM_SMOKE_GRENADE       = 229;
constexpr uint16_t ITEM_SCHEM_FLASHBANG           = 230;
constexpr uint16_t ITEM_SCHEM_CRYO_GRENADE        = 240;  // added later, IDs 231-239 are mines/turrets
constexpr uint16_t ITEM_SCHEM_PROXIMITY_MINE      = 231;
constexpr uint16_t ITEM_SCHEM_EMP_MINE            = 232;
constexpr uint16_t ITEM_SCHEM_INCENDIARY_MINE     = 233;
constexpr uint16_t ITEM_SCHEM_DECOY_MINE          = 234;
constexpr uint16_t ITEM_SCHEM_CALTROPS            = 235;
constexpr uint16_t ITEM_SCHEM_AUTO_TURRET         = 236;
constexpr uint16_t ITEM_SCHEM_FLAME_TURRET        = 237;
constexpr uint16_t ITEM_SCHEM_ARC_TURRET          = 238;
constexpr uint16_t ITEM_SCHEM_SENTRY_DRONE        = 239;

// Synthesized items (1000+)
constexpr uint16_t ITEM_SYNTH_PLASMA_EDGE       = 1000;
constexpr uint16_t ITEM_SYNTH_THRUSTER_PLATE    = 1001;
constexpr uint16_t ITEM_SYNTH_TARGETING_ARRAY   = 1002;
constexpr uint16_t ITEM_SYNTH_DUAL_EDGE         = 1003;
constexpr uint16_t ITEM_SYNTH_REINFORCED_PACK   = 1004;
constexpr uint16_t ITEM_SYNTH_OVERCHARGED_ENGINE = 1005;
constexpr uint16_t ITEM_SYNTH_ARTICULATED_ARMOR = 1006;
constexpr uint16_t ITEM_SYNTH_GUIDED_BLASTER    = 1007;
constexpr uint16_t ITEM_SYNTH_COMBAT_GAUNTLET   = 1008;
constexpr uint16_t ITEM_SYNTH_ARMORED_BLADE     = 1009;

// Cyberdecks (300-301)
constexpr uint16_t ITEM_PIDGIN_MK1     = 300;   // T1 cyberdeck
constexpr uint16_t ITEM_POLYGLOT_DCK2  = 301;   // T2 cyberdeck

// Programs (310-319)
constexpr uint16_t ITEM_PROG_ICEBREAKER_LITE = 310;
constexpr uint16_t ITEM_PROG_GHOST_TRACE     = 311;
constexpr uint16_t ITEM_PROG_COOLDOWN        = 312;
constexpr uint16_t ITEM_PROG_BREACH          = 313;
constexpr uint16_t ITEM_PROG_DECRYPT         = 314;
constexpr uint16_t ITEM_PROG_REBOOT_OPTICS   = 315;
constexpr uint16_t ITEM_PROG_FRIENDLY_FIRE   = 316;
constexpr uint16_t ITEM_PROG_DATA_LEECH      = 317;
constexpr uint16_t ITEM_PROG_PULSE_HAMMER    = 318;  // Plan 4 — T3 ATK AoE
constexpr uint16_t ITEM_PROG_DAEMON_HIJACK   = 319;  // Plan 4 — T3 UTL charm

// Code fragments (320-322)
constexpr uint16_t ITEM_CODE_FRAGMENT_T1     = 320;
constexpr uint16_t ITEM_CODE_FRAGMENT_T2     = 321;
constexpr uint16_t ITEM_CODE_FRAGMENT_T3     = 322;

// Implants (400+)
constexpr uint16_t ITEM_NEURAL_BACKUP        = 400;

// Relay Cortex variants (401-406)
constexpr uint16_t ITEM_RELAY_CORTEX_MK1     = 401;
constexpr uint16_t ITEM_SPIKE_CORTEX         = 402;
constexpr uint16_t ITEM_GLACIER_CORTEX       = 403;
constexpr uint16_t ITEM_SENTINEL_CORTEX      = 404;
constexpr uint16_t ITEM_ACUITY_CORTEX        = 405;
constexpr uint16_t ITEM_STOIC_CORTEX         = 406;

// Phase A implant content pack (407-418)
constexpr uint16_t ITEM_STANDARD_OPTICS      = 407;
constexpr uint16_t ITEM_TARGETING_LATTICE    = 408;
constexpr uint16_t ITEM_HEAT_SPECTRUM_VISOR  = 409;
constexpr uint16_t ITEM_STANDARD_PLATE       = 410;
constexpr uint16_t ITEM_SUBDERMAL_PLATING    = 411;
constexpr uint16_t ITEM_SERVO_GRIP           = 412;
constexpr uint16_t ITEM_PISTOL_TARGETER      = 413;
constexpr uint16_t ITEM_PLATED_SLEEVE        = 414;
constexpr uint16_t ITEM_REINFORCED_SERVOS    = 415;
constexpr uint16_t ITEM_REFLEX_SPRINGS       = 416;
constexpr uint16_t ITEM_SPRINT_COILS         = 417;
constexpr uint16_t ITEM_MAG_LOCK_SOLES       = 418;

// Phase B implant content pack (419-422)
constexpr uint16_t ITEM_VIBRO_TIP_FINGERS = 419;
constexpr uint16_t ITEM_STATIC_PALM       = 420;
constexpr uint16_t ITEM_WRIST_ROCKET      = 421;
constexpr uint16_t ITEM_COILGUN_PUNCH     = 422;

// Phase C implant content pack (423-426)
constexpr uint16_t ITEM_THREAT_OPTICS  = 423;
constexpr uint16_t ITEM_ADRENAL_PUMP   = 424;
constexpr uint16_t ITEM_EMP_BUFFER     = 425;
constexpr uint16_t ITEM_BURST_PISTONS  = 426;

// Hacker mats (430+) — fragment-system compile material.
constexpr uint16_t ITEM_PROGRAM_DISK         = 430;

} // namespace astra
