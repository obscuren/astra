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

// Crafting materials (33-36)
constexpr uint16_t ITEM_NANO_FIBER          = 33;
constexpr uint16_t ITEM_POWER_CORE          = 34;
constexpr uint16_t ITEM_CIRCUIT_BOARD       = 35;
constexpr uint16_t ITEM_ALLOY_INGOT         = 36;

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

} // namespace astra
