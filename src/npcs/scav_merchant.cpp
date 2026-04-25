#include "astra/npc_defs.h"
#include "astra/item_ids.h"
#include "astra/loot_table.h"

namespace astra {

Npc build_scav_junk_dealer(Race race, std::mt19937& rng) {
    Npc npc;
    npc.race     = race;
    npc.npc_role = NpcRole::Merchant;
    npc.role     = "Junk Dealer";
    npc.hp       = 15;
    npc.max_hp   = 15;
    npc.faction  = "";  // Unaligned
    add_effect(npc.effects, make_invulnerable_ge());
    npc.quickness = 0;
    // Task 13 will roll a real name.
    npc.name = "Junk Dealer";

    // --- Talk ---
    npc.interactions.talk = TalkTrait{
        "Everything here's for sale. Don't ask where it came from.",
        {
            // Node 0: what do you sell
            {
                "Salvage, mostly. Parts, components, the occasional "
                "weapon. If it floated by, I've got it.",
                {
                    {"Where do you get all this?", 1},
                    {"Sounds useful.", -1},
                },
            },
            // Node 1: sourcing
            {
                "Wrecks, mostly. Derelicts. Sometimes a hauler runs short "
                "on credits. I don't judge. Credits are credits.",
                {
                    {"Right.", -1},
                },
            },
        },
    };

    // --- Shop ---
    // TODO(scav-pricing): discount scav merchants relative to hub merchants.
    static const std::vector<StockManifestEntry> s_scav_merchant_manifest = {
        { StockManifestEntry::Mode::Always, ITEM_SMALL_ENERGY_CELL, Category::Battery,          2 },
        { StockManifestEntry::Mode::Always, ITEM_RATION_PACK,       Category::Consumable,       3 },
        { StockManifestEntry::Mode::Always, ITEM_COMBAT_STIM,       Category::Consumable,       1 },
        { StockManifestEntry::Mode::Always, ITEM_SCRAP_METAL,       Category::Junk,             5 },
        { StockManifestEntry::Mode::Always, ITEM_BROKEN_CIRCUIT,    Category::Junk,             3 },

        { StockManifestEntry::Mode::Random, 0, Category::CraftingMaterial, 3 },
        { StockManifestEntry::Mode::Random, 0, Category::EnergyMod,        2 },
        { StockManifestEntry::Mode::Random, 0, Category::Weapon,           1 },
        { StockManifestEntry::Mode::Random, 0, Category::Armor,            1 },
        { StockManifestEntry::Mode::Random, 0, Category::Accessory,        1 },
    };

    npc.interactions.shop = ShopTrait{
        npc.name + "'s Salvage",
        assemble_stock(s_scav_merchant_manifest, LootSource::ScavMerchant,
                       /*faction_rep=*/0, /*level=*/1, rng),
    };

    return npc;
}

} // namespace astra
