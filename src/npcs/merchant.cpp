#include "astra/npc_defs.h"
#include "astra/item_ids.h"
#include "astra/loot_table.h"
#include "astra/faction.h"

namespace astra {

Npc build_merchant(Race race, std::mt19937& rng, int faction_rep) {
    Npc npc;
    npc.race = race;
    npc.npc_role = NpcRole::Merchant;
    npc.role = "Merchant";
    npc.hp = 15;
    npc.max_hp = 15;
    npc.faction = Faction_KrethMiningGuild;
    add_effect(npc.effects, make_invulnerable_ge());
    npc.quickness = 0;
    npc.name = generate_name(race, rng);

    // --- Talk: brief introduction ---
    npc.interactions.talk = TalkTrait{
        "What do you want? I haven't got all cycle.",
        {
            // Node 0: who are you
            {
                "I'm a trader. I move goods wherever there's demand. If you've "
                "got credits, I've got wares. Simple as that.",
                {
                    {"Where do you source your goods?", 1},
                    {"Fair enough.", -1},
                },
            },
            // Node 1: supply chain
            {
                "Outer ring salvage, mostly. Old colony ships, derelict "
                "stations. You'd be surprised what's still floating around "
                "out there.",
                {
                    {"Isn't that dangerous?", 2},
                    {"Interesting.", -1},
                },
            },
            // Node 2: danger
            {
                "Everything's dangerous out here. The trick is knowing "
                "which risks pay off. Now, are you buying or just talking?",
                {
                    {"Point taken.", -1},
                },
            },
        },
    };

    // --- Shop ---
    static const std::vector<StockManifestEntry> s_general_merchant_manifest = {
        // Always-stocked basics
        { StockManifestEntry::Mode::Always, ITEM_SMALL_ENERGY_CELL,    Category::Battery,       2 },
        { StockManifestEntry::Mode::Always, ITEM_STANDARD_ENERGY_CELL, Category::Battery,       1 },
        { StockManifestEntry::Mode::Always, ITEM_RATION_PACK,          Category::Consumable,    5 },
        { StockManifestEntry::Mode::Always, ITEM_COMBAT_STIM,          Category::Consumable,    2 },
        { StockManifestEntry::Mode::Always, ITEM_FRAG_GRENADE,         Category::Consumable,    3 },
        { StockManifestEntry::Mode::Always, ITEM_NIGHT_GOGGLES,        Category::Accessory,     1 },
        { StockManifestEntry::Mode::Always, ITEM_HULL_PLATE,           Category::ShipComponent, 1 },
        { StockManifestEntry::Mode::Always, ITEM_SHIELD_GENERATOR,     Category::ShipComponent, 1 },
        { StockManifestEntry::Mode::Always, ITEM_SOLAR_PANEL_COMMON,   Category::EnergyMod,     1 },

        // Random rotating stock
        { StockManifestEntry::Mode::Random, 0, Category::Weapon, 1 },
        { StockManifestEntry::Mode::Random, 0, Category::Armor,  1 },
        { StockManifestEntry::Mode::Random, 0, Category::Shield, 1 },

        // Liked tier (rep >= 10): more weapons + stims
        { StockManifestEntry::Mode::Random, 0, Category::Weapon,     1, /*min_rep=*/10 },
        { StockManifestEntry::Mode::Always, ITEM_COMBAT_STIM, Category::Consumable, 3, /*min_rep=*/10 },

        // Trusted tier (rep >= 50): more armor
        { StockManifestEntry::Mode::Random, 0, Category::Armor, 1, /*min_rep=*/50 },

        // Accessory modules
        { StockManifestEntry::Mode::Always, ITEM_LIGHT_SENSOR, Category::AccessoryMod, 1 },
        { StockManifestEntry::Mode::Always, ITEM_AI_MODULE,    Category::AccessoryMod, 1, /*min_rep=*/50 },
    };

    npc.interactions.shop = ShopTrait{
        npc.name + "'s Supplies",
        assemble_stock(s_general_merchant_manifest, LootSource::MerchantGeneral,
                       faction_rep, /*level=*/1, rng),
    };

    // --- Quest: supply runs ---
    npc.interactions.quest = QuestTrait{
        "Need any supplies picked up?",
        {
            // Node 0: placeholder — actual quest is generated dynamically
            {
                "I could use someone to track down some materials. "
                "Interested?",
                {
                    {"What do you need?", 1},
                    {"Not right now.", -1},
                },
            },
            // Node 1: accepted
            {
                "Good. Bring them back in one piece and I'll "
                "make it worth your while.",
                {
                    {"Consider it done.", -1},
                },
            },
        },
    };

    return npc;
}

} // namespace astra
