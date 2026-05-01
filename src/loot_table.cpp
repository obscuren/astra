#include "astra/loot_table.h"
#include "astra/item_defs.h"
#include "astra/item_gen.h"
#include "astra/item_ids.h"

#include <cctype>
#include <cstdio>
#include <unordered_set>

namespace astra {

namespace {

// ---------------------------------------------------------------------------
// The loot table. One entry per item. Each entry is the single source of
// truth for: where the item can drop (source_mask), at what rarity range,
// at what weight, in which category, with which theme.
// ---------------------------------------------------------------------------

const std::vector<LootEntry>& s_loot_table_data() {
    using R = Rarity;
    using C = Category;
    using T = Theme;

    static const std::vector<LootEntry> data = {
        // ----- Ranged weapons --------------------------------------------
        LootEntry{ ITEM_PLASMA_PISTOL,  "plasma_pistol",  R::Common,    R::Rare,      40, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                T::Tech,     1, C::Weapon },
        LootEntry{ ITEM_ION_BLASTER,    "ion_blaster",    R::Common,    R::Rare,      30, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                T::Tech,     1, C::Weapon },
        LootEntry{ ITEM_PULSE_RIFLE,    "pulse_rifle",    R::Uncommon,  R::Epic,      18, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                T::Military, 2, C::Weapon },
        LootEntry{ ITEM_ARC_CASTER,     "arc_caster",     R::Rare,      R::Epic,       9, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket, T::Tech, 3, C::Weapon },
        LootEntry{ ITEM_VOID_LANCE,     "void_lance",     R::Epic,      R::Legendary,  3, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::BlackMarket,                  T::Ancient,  5, C::Weapon },

        // ----- Melee weapons ---------------------------------------------
        LootEntry{ ITEM_COMBAT_KNIFE,       "combat_knife",       R::Common,    R::Rare,      35, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms | LootSource::ScavMerchant, T::Civilian, 1, C::Weapon },
        LootEntry{ ITEM_STUN_BATON,         "stun_baton",         R::Common,    R::Rare,      20, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                              T::Tech,     1, C::Weapon },
        LootEntry{ ITEM_VIBRO_BLADE,        "vibro_blade",        R::Uncommon,  R::Epic,      20, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                              T::Tech,     2, C::Weapon },
        LootEntry{ ITEM_PLASMA_SABER,       "plasma_saber",       R::Rare,      R::Epic,      17, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,    T::Military, 3, C::Weapon },
        LootEntry{ ITEM_ANCIENT_MONO_EDGE,  "ancient_mono_edge",  R::Epic,      R::Legendary,  8, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::BlackMarket,                                T::Ancient,  5, C::Weapon },

        // ----- Armor -----------------------------------------------------
        LootEntry{ ITEM_PADDED_VEST,     "padded_vest",     R::Common,    R::Rare,      22, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantGeneral | LootSource::ScavMerchant, T::Civilian, 1, C::Armor },
        LootEntry{ ITEM_FLIGHT_HELMET,   "flight_helmet",   R::Common,    R::Rare,      16, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantGeneral,                              T::Civilian, 1, C::Armor },
        LootEntry{ ITEM_COMBAT_BOOTS,    "combat_boots",    R::Common,    R::Rare,      16, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms,    T::Military, 1, C::Armor },
        LootEntry{ ITEM_ARM_GUARD,       "arm_guard",       R::Common,    R::Rare,      13, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                                  T::Military, 1, C::Armor },
        LootEntry{ ITEM_COMPOSITE_ARMOR, "composite_armor", R::Uncommon,  R::Epic,      13, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                                  T::Military, 2, C::Armor },
        LootEntry{ ITEM_TACTICAL_HELMET, "tactical_helmet", R::Uncommon,  R::Epic,      10, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                                  T::Military, 2, C::Armor },
        LootEntry{ ITEM_MAG_LOCK_BOOTS,  "mag_lock_boots",  R::Rare,      R::Epic,       7, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,        T::Tech,     3, C::Armor },
        LootEntry{ ITEM_EXO_SUIT,        "exo_suit",        R::Epic,      R::Legendary,  3, {}, LootSource::Chest | LootSource::BlackMarket,                                                          T::Military, 5, C::Armor },

        // ----- Shields ---------------------------------------------------
        LootEntry{ ITEM_BASIC_DEFLECTOR,    "basic_deflector",    R::Common,    R::Rare,      35, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms, T::Tech,     1, C::Shield },
        LootEntry{ ITEM_PLASMA_SCREEN,      "plasma_screen",      R::Uncommon,  R::Epic,      20, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                                T::Tech,     2, C::Shield },
        LootEntry{ ITEM_ION_BARRIER,        "ion_barrier",        R::Uncommon,  R::Epic,      20, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                                T::Tech,     2, C::Shield },
        LootEntry{ ITEM_COMPOSITE_BARRIER,  "composite_barrier",  R::Rare,      R::Epic,      15, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,    T::Military, 3, C::Shield },
        LootEntry{ ITEM_HARDLIGHT_AEGIS,    "hardlight_aegis",    R::Rare,      R::Legendary,  7, {}, LootSource::Chest | LootSource::BlackMarket,                                                       T::Tech,     4, C::Shield },
        LootEntry{ ITEM_VOID_MANTLE,        "void_mantle",        R::Epic,      R::Legendary,  3, {}, LootSource::Chest | LootSource::BlackMarket,                                                       T::Ancient,  5, C::Shield },

        // ----- Accessories -----------------------------------------------
        LootEntry{ ITEM_NIGHT_GOGGLES,      "nightvision_goggles", R::Common,    R::Rare,      20, {}, LootSource::Chest | LootSource::MerchantGeneral,                                                  T::Civilian, 1, C::Accessory },
        LootEntry{ ITEM_RECON_VISOR,        "recon_visor",        R::Uncommon,  R::Epic,      10, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms,                       T::Tech,     2, C::Accessory },
        LootEntry{ ITEM_JETPACK,            "jetpack",            R::Rare,      R::Epic,       5, {}, LootSource::Chest | LootSource::BlackMarket,                                                       T::Tech,     3, C::Accessory },
        LootEntry{ ITEM_CARGO_PACK,         "cargo_pack",         R::Common,    R::Rare,      15, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::ScavMerchant,                       T::Civilian, 1, C::Accessory },

        // ----- Consumables -----------------------------------------------
        LootEntry{ ITEM_RATION_PACK,        "ration_pack",        R::Common,    R::Common,    50, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantFood, T::Civilian, 1, C::Consumable },
        LootEntry{ ITEM_COMBAT_STIM,        "combat_stim",        R::Common,    R::Uncommon,  30, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantFood | LootSource::MerchantArms, T::Tech, 1, C::Consumable },
        LootEntry{ ITEM_FRAG_GRENADE,       "frag_grenade",       R::Common,    R::Uncommon,  20, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                              T::Military, 1, C::Consumable },
        LootEntry{ ITEM_EMP_GRENADE,        "emp_grenade",        R::Uncommon,  R::Rare,      15, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                              T::Tech,     2, C::Consumable },
        LootEntry{ ITEM_CRYO_GRENADE,       "cryo_grenade",       R::Uncommon,  R::Rare,      10, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,                          T::Tech,     2, C::Consumable },
        // New stims (crafted via schematics; also drop occasionally pre-built)
        LootEntry{ ITEM_HEALING_STIM,       "healing_stim",       R::Common,    R::Uncommon,  20, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantFood, T::Civilian, 1, C::Consumable },
        LootEntry{ ITEM_ENDURE_STIM,        "endure_stim",        R::Uncommon,  R::Uncommon,  15, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms,                      T::Military, 2, C::Consumable },
        LootEntry{ ITEM_FOCUS_STIM,         "focus_stim",         R::Uncommon,  R::Uncommon,  15, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms,                      T::Tech,     2, C::Consumable },
        LootEntry{ ITEM_BERSERKER_STIM,     "berserker_stim",     R::Rare,      R::Rare,       6, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,                          T::Military, 3, C::Consumable },
        LootEntry{ ITEM_MEDKIT,             "medkit",             R::Uncommon,  R::Uncommon,  12, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantFood,                      T::Civilian, 2, C::Consumable },
        // New grenades
        LootEntry{ ITEM_INCENDIARY_GRENADE, "incendiary_grenade", R::Uncommon,  R::Rare,      12, {}, LootSource::Chest | LootSource::MerchantArms,                                                    T::Military, 2, C::Consumable },
        LootEntry{ ITEM_SMOKE_GRENADE,      "smoke_grenade",      R::Common,    R::Common,    18, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                              T::Military, 1, C::Consumable },
        LootEntry{ ITEM_FLASHBANG,          "flashbang",          R::Uncommon,  R::Uncommon,  10, {}, LootSource::Chest | LootSource::MerchantArms,                                                    T::Military, 2, C::Consumable },
        // Mines
        LootEntry{ ITEM_PROXIMITY_MINE,     "proximity_mine",     R::Uncommon,  R::Rare,      10, {}, LootSource::Chest | LootSource::MerchantArms,                                                    T::Military, 2, C::Consumable },
        LootEntry{ ITEM_EMP_MINE,           "emp_mine",           R::Uncommon,  R::Rare,       8, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,                          T::Tech,     3, C::Consumable },
        LootEntry{ ITEM_INCENDIARY_MINE,    "incendiary_mine",    R::Uncommon,  R::Rare,       8, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,                          T::Military, 3, C::Consumable },
        LootEntry{ ITEM_DECOY_MINE,         "decoy_mine",         R::Common,    R::Common,    12, {}, LootSource::Chest | LootSource::MerchantArms,                                                    T::Tech,     2, C::Consumable },
        LootEntry{ ITEM_CALTROPS,           "caltrops",           R::Common,    R::Common,    20, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantGeneral | LootSource::ScavMerchant, T::Civilian, 1, C::Consumable },
        // Turrets
        LootEntry{ ITEM_AUTO_TURRET,        "auto_turret",        R::Uncommon,  R::Rare,       6, {}, LootSource::Chest | LootSource::MerchantArms,                                                    T::Military, 2, C::Consumable },
        LootEntry{ ITEM_FLAME_TURRET,       "flame_turret",       R::Rare,      R::Rare,       4, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,                          T::Military, 3, C::Consumable },
        LootEntry{ ITEM_ARC_TURRET,         "arc_turret",         R::Rare,      R::Rare,       4, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,                          T::Tech,     3, C::Consumable },
        LootEntry{ ITEM_SENTRY_DRONE,       "sentry_drone",       R::Epic,      R::Epic,       2, {}, LootSource::Chest | LootSource::BlackMarket,                                                     T::Tech,     5, C::Consumable },

        // ----- Cyberdecks ----------------------------------------------
        LootEntry{ ITEM_PIDGIN_MK1,         "pidgin_mk1",         R::Common,    R::Uncommon,   8, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::ScavMerchant,                            T::Tech,     1, C::Cyberdeck },
        LootEntry{ ITEM_POLYGLOT_DCK2,      "polyglot_dck2",      R::Uncommon,  R::Rare,       4, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,                            T::Tech,     2, C::Cyberdeck },

        // ----- Programs ------------------------------------------------
        LootEntry{ ITEM_PROG_ICEBREAKER_LITE, "icebreaker_lite",  R::Common,    R::Uncommon,  10, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,                            T::Tech,     1, C::Program },
        LootEntry{ ITEM_PROG_GHOST_TRACE,     "ghost_trace",      R::Uncommon,  R::Rare,       6, {}, LootSource::Chest | LootSource::BlackMarket,                                                       T::Tech,     2, C::Program },
        LootEntry{ ITEM_PROG_COOLDOWN,        "cooldown",         R::Common,    R::Common,    12, {}, LootSource::Chest | LootSource::MerchantArms,                                                      T::Tech,     1, C::Program },
        LootEntry{ ITEM_PROG_BREACH,          "breach",           R::Uncommon,  R::Rare,       8, {}, LootSource::Chest | LootSource::BlackMarket,                                                       T::Tech,     1, C::Program },
        LootEntry{ ITEM_PROG_DECRYPT,         "decrypt",          R::Common,    R::Uncommon,  10, {}, LootSource::Chest | LootSource::MerchantArms,                                                      T::Tech,     1, C::Program },
        LootEntry{ ITEM_PROG_REBOOT_OPTICS,   "reboot_optics",    R::Common,    R::Common,    14, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::ScavMerchant,                          T::Tech,     1, C::Program },
        LootEntry{ ITEM_PROG_FRIENDLY_FIRE,   "friendly_fire",    R::Uncommon,  R::Rare,       6, {}, LootSource::Chest | LootSource::BlackMarket,                                                       T::Tech,     2, C::Program },
        LootEntry{ ITEM_PROG_DATA_LEECH,      "data_leech",       R::Common,    R::Uncommon,   8, {}, LootSource::Chest | LootSource::MerchantArms,                                                      T::Tech,     1, C::Program },
        LootEntry{ ITEM_PROG_PULSE_HAMMER,    "pulse_hammer",     R::Rare,      R::Rare,       3, {}, LootSource::Chest | LootSource::BlackMarket,                                                       T::Tech,     3, C::Program },
        LootEntry{ ITEM_PROG_DAEMON_HIJACK,   "daemon_hijack",    R::Rare,      R::Rare,       3, {}, LootSource::Chest | LootSource::BlackMarket,                                                       T::Tech,     3, C::Program },

        // ----- Implants (Plan 4) ---------------------------------------
        LootEntry{ ITEM_NEURAL_BACKUP,        "neural_backup",    R::Rare,      R::Rare,       2, {}, LootSource::Chest | LootSource::BlackMarket,                                                       T::Tech,     2, C::Implant },

        // ----- Code fragments (crafting material) ----------------------
        LootEntry{ ITEM_CODE_FRAGMENT_T1,     "code_fragment_t1", R::Common,    R::Common,    20, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                                T::Tech,     1, C::CodeFragment },
        LootEntry{ ITEM_CODE_FRAGMENT_T2,     "code_fragment_t2", R::Uncommon,  R::Uncommon,  10, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,      T::Tech,     2, C::CodeFragment },
        LootEntry{ ITEM_CODE_FRAGMENT_T3,     "code_fragment_t3", R::Rare,      R::Rare,       3, {}, LootSource::Chest | LootSource::BlackMarket,                                                       T::Tech,     3, C::CodeFragment },

        // ----- Batteries (own category, distinct from Consumable) -------
        LootEntry{ ITEM_SMALL_ENERGY_CELL,      "cell_small",      R::Common,    R::Common,    50, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms | LootSource::ScavMerchant, T::Tech, 1, C::Battery },
        LootEntry{ ITEM_STANDARD_ENERGY_CELL,   "cell_standard",   R::Common,    R::Uncommon,  35, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms,                          T::Tech, 1, C::Battery },
        LootEntry{ ITEM_LARGE_ENERGY_CELL,      "cell_large",      R::Uncommon,  R::Rare,      18, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,                                                     T::Tech, 2, C::Battery },
        LootEntry{ ITEM_INDUSTRIAL_ENERGY_CELL, "cell_industrial", R::Rare,      R::Epic,       7, {}, LootSource::Chest | LootSource::BlackMarket,                                                                                T::Tech, 3, C::Battery },
        LootEntry{ ITEM_ANTIMATTER_CELL,        "cell_antimatter", R::Epic,      R::Legendary,  2, {}, LootSource::Chest | LootSource::BlackMarket,                                                                                T::Ancient, 5, C::Battery },
        LootEntry{ ITEM_BULWARK_CELL,           "cell_bulwark",    R::Legendary, R::Legendary,  1, {}, LootSource::Chest | LootSource::BlackMarket,                                                                                T::Tech, 5, C::Battery },
        LootEntry{ ITEM_VOLATILE_CELL,          "cell_volatile",   R::Legendary, R::Legendary,  1, {}, LootSource::Chest | LootSource::BlackMarket,                                                                                T::Tech, 5, C::Battery },
        LootEntry{ ITEM_ADRENAL_CELL,           "cell_adrenal",    R::Legendary, R::Legendary,  1, {}, LootSource::Chest | LootSource::BlackMarket,                                                                                T::Tech, 5, C::Battery },

        // ----- Junk ------------------------------------------------------
        LootEntry{ ITEM_SCRAP_METAL,        "scrap_metal",        R::Common,    R::Common,    35, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::ScavMerchant, T::Scrap, 1, C::Junk },
        LootEntry{ ITEM_BROKEN_CIRCUIT,     "broken_circuit",     R::Common,    R::Common,    35, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::ScavMerchant, T::Scrap, 1, C::Junk },
        LootEntry{ ITEM_EMPTY_CASING,       "empty_casing",       R::Common,    R::Common,    30, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::ScavMerchant, T::Scrap, 1, C::Junk },

        // ----- Crafting materials ----------------------------------------
        LootEntry{ ITEM_NANO_FIBER,         "nano_fiber",         R::Common,    R::Uncommon,  30, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::ScavMerchant, T::Tech,     1, C::CraftingMaterial },
        LootEntry{ ITEM_POWER_CORE,         "power_core",         R::Uncommon,  R::Rare,      25, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::ScavMerchant, T::Tech,     2, C::CraftingMaterial },
        LootEntry{ ITEM_CIRCUIT_BOARD,      "circuit_board",      R::Common,    R::Uncommon,  25, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::ScavMerchant, T::Tech,     1, C::CraftingMaterial },
        LootEntry{ ITEM_ALLOY_INGOT,        "alloy_ingot",        R::Uncommon,  R::Rare,      20, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::ScavMerchant, T::Civilian, 2, C::CraftingMaterial },
        LootEntry{ ITEM_SPARE_PARTS,        "spare_parts",        R::Common,    R::Uncommon,  30, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::ScavMerchant,        T::Scrap,    1, C::CraftingMaterial },
        LootEntry{ ITEM_CIRCUITRY,          "circuitry",          R::Common,    R::Uncommon,  20, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::ScavMerchant,        T::Scrap,    1, C::CraftingMaterial },

        // T1 pure-mat reagents
        LootEntry{ ITEM_COPPER_WIRE,        "copper_wire",        R::Common,    R::Common,    35, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::ScavMerchant | LootSource::MerchantGeneral, T::Tech,     1, C::CraftingMaterial },
        LootEntry{ ITEM_POLYMER_STRIP,      "polymer_strip",      R::Common,    R::Common,    35, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::ScavMerchant | LootSource::MerchantGeneral, T::Civilian, 1, C::CraftingMaterial },
        LootEntry{ ITEM_GLASS_SHARD,        "glass_shard",        R::Common,    R::Common,    30, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::ScavMerchant,                              T::Scrap,    1, C::CraftingMaterial },
        LootEntry{ ITEM_ADHESIVE_RESIN,     "adhesive_resin",     R::Common,    R::Common,    30, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::ScavMerchant,                      T::Civilian, 1, C::CraftingMaterial },
        LootEntry{ ITEM_COOLANT_VIAL,       "coolant_vial",       R::Common,    R::Uncommon,  25, {}, LootSource::Chest | LootSource::MerchantGeneral,                                                  T::Tech,     1, C::CraftingMaterial },
        // T2 new pure-mats
        LootEntry{ ITEM_NANO_LATTICE,       "nano_lattice",       R::Uncommon,  R::Rare,      18, {}, LootSource::Chest | LootSource::MerchantGeneral,                                                  T::Tech,     2, C::CraftingMaterial },
        LootEntry{ ITEM_POLISHED_LENS,      "polished_lens",      R::Uncommon,  R::Rare,      20, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms,                       T::Tech,     2, C::CraftingMaterial },
        LootEntry{ ITEM_MICRO_SERVO,        "micro_servo",        R::Uncommon,  R::Rare,      20, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms,                       T::Tech,     2, C::CraftingMaterial },
        LootEntry{ ITEM_PLASMA_CARTRIDGE,   "plasma_cartridge",   R::Uncommon,  R::Rare,      15, {}, LootSource::Chest | LootSource::MerchantArms,                                                     T::Military, 2, C::CraftingMaterial },
        // T3 — boss / deep dungeon / quest only (gated by min_level + restricted sources)
        LootEntry{ ITEM_QUANTUM_RESONANCE_CRYSTAL, "quantum_resonance_crystal", R::Rare, R::Epic,      4, {}, LootSource::Chest | LootSource::BlackMarket, T::Ancient, 5, C::CraftingMaterial },
        LootEntry{ ITEM_STRANGE_STROBING_CRYSTAL,  "strange_strobing_crystal",  R::Rare, R::Legendary, 3, {}, LootSource::Chest | LootSource::BlackMarket, T::Ancient, 5, C::CraftingMaterial },
        LootEntry{ ITEM_PRIME_CATALYST,            "prime_catalyst",            R::Rare, R::Epic,      3, {}, LootSource::Chest | LootSource::BlackMarket, T::Ancient, 5, C::CraftingMaterial },
        LootEntry{ ITEM_PRIME_FILAMENT,            "prime_filament",            R::Rare, R::Epic,      3, {}, LootSource::Chest | LootSource::BlackMarket, T::Ancient, 5, C::CraftingMaterial },
        LootEntry{ ITEM_VOIDSHARD,                 "voidshard",                 R::Epic, R::Legendary, 2, {}, LootSource::Chest | LootSource::BlackMarket, T::Ancient, 6, C::CraftingMaterial },
        LootEntry{ ITEM_PHASE_COIL,                "phase_coil",                R::Epic, R::Legendary, 2, {}, LootSource::Chest | LootSource::BlackMarket, T::Ancient, 6, C::CraftingMaterial },

        // ----- Ship components -------------------------------------------
        LootEntry{ ITEM_HULL_PLATE,         "hull_plate",         R::Common,    R::Rare,      35, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms | LootSource::MaintenanceTunnel, T::Tech, 1, C::ShipComponent },
        LootEntry{ ITEM_SHIELD_GENERATOR,   "shield_generator",   R::Uncommon,  R::Epic,      25, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms | LootSource::MaintenanceTunnel, T::Tech, 2, C::ShipComponent },
        LootEntry{ ITEM_NAVI_COMPUTER_MK2,  "navi_computer_mk2",  R::Rare,      R::Epic,      15, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::MaintenanceTunnel,                            T::Tech, 3, C::ShipComponent },
        // Note: ITEM_ENGINE_COIL_MK1 deliberately omitted — placed by hand, never rolled.

        // ----- Energy mods -----------------------------------------------
        LootEntry{ ITEM_SOLAR_PANEL_COMMON,   "solar_panel",          R::Common,    R::Common,    30, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms, T::Tech, 1, C::EnergyMod },
        LootEntry{ ITEM_SOLAR_PANEL_UNCOMMON, "solar_panel_uncommon", R::Uncommon,  R::Uncommon,  20, {}, LootSource::Chest | LootSource::MerchantArms,                              T::Tech, 2, C::EnergyMod },
        LootEntry{ ITEM_SOLAR_PANEL_RARE,     "solar_panel_rare",     R::Rare,      R::Rare,      10, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,    T::Tech, 4, C::EnergyMod },
        LootEntry{ ITEM_CAPACITOR_COIL,       "capacitor_coil",       R::Uncommon,  R::Uncommon,  20, {}, LootSource::Chest | LootSource::MerchantArms,                              T::Tech, 2, C::EnergyMod },
        LootEntry{ ITEM_CHARGE_CATALYST,      "charge_catalyst",      R::Uncommon,  R::Uncommon,  20, {}, LootSource::Chest | LootSource::MerchantArms,                              T::Tech, 2, C::EnergyMod },
        LootEntry{ ITEM_POLISHED_CONDUIT,     "polished_conduit",     R::Rare,      R::Rare,      10, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,    T::Tech, 4, C::EnergyMod },
        LootEntry{ ITEM_REINFORCED_CASING,    "reinforced_casing",    R::Common,    R::Common,    30, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::ScavMerchant,   T::Scrap, 1, C::EnergyMod },
        LootEntry{ ITEM_RECEPTOR_PLATE,       "receptor_plate",       R::Common,    R::Common,    30, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::ScavMerchant,   T::Scrap, 1, C::EnergyMod },
        LootEntry{ ITEM_BRASS_CONDUIT,        "brass_conduit",        R::Common,    R::Common,    30, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::ScavMerchant,   T::Scrap, 1, C::EnergyMod },
        LootEntry{ ITEM_POWER_JUNCTION,       "power_junction",       R::Uncommon,  R::Uncommon,  15, {}, LootSource::Chest | LootSource::MerchantArms,                              T::Tech, 2, C::EnergyMod },
        LootEntry{ ITEM_TUNED_CATALYST,       "tuned_catalyst",       R::Rare,      R::Rare,       8, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,    T::Tech, 4, C::EnergyMod },

        // ----- Accessory mods -------------------------------------------
        LootEntry{ ITEM_AI_MODULE,    "ai_module",    R::Rare,     R::Rare,      5, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,       T::Tech, 3, C::AccessoryMod },
        LootEntry{ ITEM_LIGHT_SENSOR, "light_sensor", R::Uncommon, R::Uncommon, 12, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms,   T::Tech, 1, C::AccessoryMod },

        // ----- Cookbooks (merchant-only, themed by Civilian) -------------
        LootEntry{ ITEM_COOKBOOK_HEARTY_STEW,  "cookbook_hearty_stew",  R::Common,    R::Common,    1, {}, loot_source_bit(LootSource::MerchantFood), T::Civilian, 1, C::Cookbook },
        LootEntry{ ITEM_COOKBOOK_PROTEIN_BAKE, "cookbook_protein_bake", R::Uncommon,  R::Uncommon,  1, {}, loot_source_bit(LootSource::MerchantFood), T::Civilian, 2, C::Cookbook },
        LootEntry{ ITEM_COOKBOOK_HEROS_FEAST,  "cookbook_heros_feast",  R::Rare,      R::Rare,      1, {}, loot_source_bit(LootSource::MerchantFood), T::Civilian, 4, C::Cookbook },

        // ----- Schematics (chest drops + select merchants) ---------------
        // Categorized as Junk for loot dispatch; the Schematic ItemType is what the use_item handler keys on.
        LootEntry{ ITEM_SCHEM_HEALING_STIM,        "schem_healing_stim",        R::Common,    R::Common,     6, {}, LootSource::Chest | LootSource::MerchantGeneral, T::Tech,     1, C::Junk },
        LootEntry{ ITEM_SCHEM_ADRENALINE_STIM,     "schem_adrenaline_stim",     R::Uncommon,  R::Uncommon,   4, {}, LootSource::Chest | LootSource::MerchantGeneral, T::Tech,     2, C::Junk },
        LootEntry{ ITEM_SCHEM_ENDURE_STIM,         "schem_endure_stim",         R::Uncommon,  R::Uncommon,   4, {}, loot_source_bit(LootSource::Chest),             T::Tech,     2, C::Junk },
        LootEntry{ ITEM_SCHEM_FOCUS_STIM,          "schem_focus_stim",          R::Uncommon,  R::Uncommon,   4, {}, loot_source_bit(LootSource::Chest),             T::Tech,     2, C::Junk },
        LootEntry{ ITEM_SCHEM_BERSERKER_STIM,      "schem_berserker_stim",      R::Rare,      R::Rare,       2, {}, LootSource::Chest | LootSource::BlackMarket,    T::Tech,     4, C::Junk },
        LootEntry{ ITEM_SCHEM_MEDKIT,              "schem_medkit",              R::Uncommon,  R::Uncommon,   4, {}, loot_source_bit(LootSource::Chest),             T::Civilian, 2, C::Junk },
        LootEntry{ ITEM_SCHEM_FRAG_GRENADE,        "schem_frag_grenade",        R::Common,    R::Common,     6, {}, LootSource::Chest | LootSource::MerchantArms,   T::Military, 1, C::Junk },
        LootEntry{ ITEM_SCHEM_EMP_GRENADE,         "schem_emp_grenade",         R::Uncommon,  R::Uncommon,   4, {}, LootSource::Chest | LootSource::MerchantArms,   T::Tech,     2, C::Junk },
        LootEntry{ ITEM_SCHEM_INCENDIARY_GRENADE,  "schem_incendiary_grenade",  R::Uncommon,  R::Uncommon,   4, {}, LootSource::Chest | LootSource::MerchantArms,   T::Military, 2, C::Junk },
        LootEntry{ ITEM_SCHEM_SMOKE_GRENADE,       "schem_smoke_grenade",       R::Common,    R::Common,     5, {}, LootSource::Chest | LootSource::MerchantArms,   T::Military, 1, C::Junk },
        LootEntry{ ITEM_SCHEM_FLASHBANG,           "schem_flashbang",           R::Uncommon,  R::Uncommon,   3, {}, LootSource::Chest | LootSource::MerchantArms,   T::Military, 2, C::Junk },
        LootEntry{ ITEM_SCHEM_CRYO_GRENADE,        "schem_cryo_grenade",        R::Rare,      R::Rare,       2, {}, LootSource::Chest | LootSource::BlackMarket,                              T::Tech,     3, C::Junk },
        LootEntry{ ITEM_SCHEM_PROXIMITY_MINE,      "schem_proximity_mine",      R::Uncommon,  R::Uncommon,   3, {}, LootSource::Chest | LootSource::MerchantArms,   T::Military, 2, C::Junk },
        LootEntry{ ITEM_SCHEM_EMP_MINE,            "schem_emp_mine",            R::Rare,      R::Rare,       2, {}, LootSource::Chest | LootSource::BlackMarket,    T::Tech,     3, C::Junk },
        LootEntry{ ITEM_SCHEM_INCENDIARY_MINE,     "schem_incendiary_mine",     R::Rare,      R::Rare,       2, {}, LootSource::Chest | LootSource::BlackMarket,    T::Military, 3, C::Junk },
        LootEntry{ ITEM_SCHEM_DECOY_MINE,          "schem_decoy_mine",          R::Common,    R::Common,     4, {}, loot_source_bit(LootSource::Chest),             T::Tech,     1, C::Junk },
        LootEntry{ ITEM_SCHEM_CALTROPS,            "schem_caltrops",            R::Common,    R::Common,     5, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::ScavMerchant, T::Civilian, 1, C::Junk },
        LootEntry{ ITEM_SCHEM_AUTO_TURRET,         "schem_auto_turret",         R::Uncommon,  R::Uncommon,   3, {}, LootSource::Chest | LootSource::MerchantArms,                              T::Tech,     2, C::Junk },
        LootEntry{ ITEM_SCHEM_FLAME_TURRET,        "schem_flame_turret",        R::Rare,      R::Rare,       2, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,    T::Military, 3, C::Junk },
        LootEntry{ ITEM_SCHEM_ARC_TURRET,          "schem_arc_turret",          R::Rare,      R::Rare,       2, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,    T::Tech,     3, C::Junk },
        LootEntry{ ITEM_SCHEM_SENTRY_DRONE,        "schem_sentry_drone",        R::Epic,      R::Epic,       1, {}, LootSource::Chest | LootSource::BlackMarket,                              T::Tech,     5, C::Junk },

        // Note: ingredients (raw_meat, carrot, flour, herbs, synth_protein),
        // cooked dishes, and burnt_slop are deliberately NOT in the loot
        // table — they're placed by the cooking system, not by drops.
        // Same for synthesized items (1000+) — those are tinkering output.
    };
    return data;
}

const std::map<LootSource, std::map<Category, int>>& s_category_weights() {
    using C = Category;
    static const std::map<LootSource, std::map<C, int>> data = {
        { LootSource::NpcDrop, {
            { C::Weapon,           30 },
            { C::Armor,            25 },
            { C::Consumable,       15 },
            { C::Battery,           5 },
            { C::Junk,             15 },
            { C::CraftingMaterial, 10 },
        }},
        { LootSource::Chest, {
            { C::Weapon,           15 },
            { C::Armor,            15 },
            { C::Shield,            5 },
            { C::Accessory,         5 },
            { C::Consumable,       15 },
            { C::Battery,          10 },
            { C::Junk,             10 },
            { C::CraftingMaterial, 15 },
            { C::EnergyMod,        10 },
            { C::AccessoryMod,      5 },   // accessory modules
        }},
        { LootSource::MaintenanceTunnel, {
            { C::ShipComponent,    40 },
            { C::CraftingMaterial, 30 },
            { C::Junk,             30 },
        }},
        // Merchants drive their own selection through manifests; no entry here.
    };
    return data;
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
// roll_loot
// ---------------------------------------------------------------------------

std::optional<Item> roll_loot(LootSource source,
                              int level,
                              std::mt19937& rng,
                              std::optional<Category> forced_category) {
    Rarity rarity = roll_rarity(rng);

    // Step 2: pick category.
    std::optional<Category> picked_category = forced_category;
    if (!picked_category.has_value()) {
        const auto& cw = s_category_weights();
        auto it = cw.find(source);
        if (it != cw.end() && !it->second.empty()) {
            int total = 0;
            for (auto& kv : it->second) total += kv.second;
            if (total > 0) {
                int roll = std::uniform_int_distribution<int>(0, total - 1)(rng);
                int acc  = 0;
                for (auto& kv : it->second) {
                    acc += kv.second;
                    if (roll < acc) {
                        picked_category = kv.first;
                        break;
                    }
                }
            }
        }
    }

    // Step 3 + 4: filter and weighted-pick.
    const auto& table = s_loot_table_data();
    int total_weight = 0;
    std::vector<std::pair<const LootEntry*, int>> eligible;
    eligible.reserve(table.size());

    uint64_t source_bit = loot_source_bit(source);
    for (const auto& entry : table) {
        if ((entry.source_mask & source_bit) == 0)                            continue;
        if (rarity < entry.min_rarity || rarity > entry.max_rarity)           continue;
        if (level < entry.min_level)                                           continue;
        if (picked_category.has_value() && entry.category != *picked_category) continue;

        int w = entry.default_weight;
        auto sw = entry.source_weights.find(source);
        if (sw != entry.source_weights.end()) w = sw->second;
        if (w <= 0) continue;

        eligible.push_back({&entry, w});
        total_weight += w;
    }

    if (eligible.empty() || total_weight <= 0) {
        return std::nullopt;
    }

    int roll = std::uniform_int_distribution<int>(0, total_weight - 1)(rng);
    int acc  = 0;
    const LootEntry* chosen = eligible.back().first;
    for (const auto& [entry_ptr, w] : eligible) {
        acc += w;
        if (roll < acc) { chosen = entry_ptr; break; }
    }

    // Steps 5-8: build, scale, scale, affixes.
    Item item = build_by_def_id(chosen->item_def_id);
    scale_item_to_rarity(item, rarity);
    scale_item_to_level(item, level);
    apply_rarity_affixes(item, rarity, rng);
    return item;
}

// ---------------------------------------------------------------------------
// assemble_stock
// ---------------------------------------------------------------------------

std::vector<Item> assemble_stock(const std::vector<StockManifestEntry>& manifest,
                                 LootSource source,
                                 int faction_rep,
                                 int level,
                                 std::mt19937& rng) {
    std::vector<Item> stock;
    stock.reserve(manifest.size() * 2);

    for (const auto& entry : manifest) {
        if (faction_rep < entry.min_reputation) continue;

        for (int i = 0; i < entry.quantity; ++i) {
            if (entry.mode == StockManifestEntry::Mode::Always) {
                Item item = build_by_def_id(entry.item_def_id);
                if (item.item_def_id != 0) {
                    stock.push_back(std::move(item));
                }
            } else { // Random
                auto rolled = roll_loot(source, level, rng, entry.category);
                if (rolled.has_value()) {
                    stock.push_back(std::move(*rolled));
                }
            }
        }
    }

    return stock;
}

// ---------------------------------------------------------------------------
// find_entry_by_identifier, loot_table_all_entries, verify_dispatch_coverage
// ---------------------------------------------------------------------------

const LootEntry* find_entry_by_identifier(std::string_view identifier) {
    auto lower = [](char c) -> char {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    };
    auto ieq = [&](std::string_view a, std::string_view b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (lower(a[i]) != lower(b[i])) return false;
        }
        return true;
    };

    for (const auto& entry : s_loot_table_data()) {
        if (ieq(entry.identifier, identifier)) return &entry;
    }
    return nullptr;
}

const std::vector<LootEntry>& loot_table_all_entries() {
    return s_loot_table_data();
}

bool verify_dispatch_coverage() {
    bool ok = true;
    std::unordered_set<uint16_t> seen;
    for (const auto& entry : s_loot_table_data()) {
        if (entry.item_def_id == 0) {
            std::fprintf(stderr,
                "[loot_table] entry '%s' has item_def_id=0\n",
                entry.identifier.c_str());
            ok = false;
            continue;
        }
        if (!seen.insert(entry.item_def_id).second) {
            std::fprintf(stderr,
                "[loot_table] duplicate item_def_id %u (identifier '%s')\n",
                entry.item_def_id, entry.identifier.c_str());
            ok = false;
        }
        Item probe = build_by_def_id(entry.item_def_id);
        if (probe.item_def_id != entry.item_def_id) {
            std::fprintf(stderr,
                "[loot_table] build_by_def_id(%u) for '%s' returned def_id=%u "
                "(probable missing dispatch arm)\n",
                entry.item_def_id, entry.identifier.c_str(), probe.item_def_id);
            ok = false;
        }
    }
    return ok;
}

} // namespace astra
