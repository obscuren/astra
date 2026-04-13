#include "astra/npc_defs.h"
#include "astra/item_defs.h"
#include "astra/faction.h"
#include "astra/station_type.h"

namespace astra {

Npc build_black_market_vendor(const StationContext& ctx) {
    Npc npc;
    npc.race     = Race::Human;
    npc.npc_role = NpcRole::Merchant;
    npc.role     = "Fixer";
    // Task 13 will roll a real name.
    npc.name     = "Fixer";
    npc.hp       = 15;
    npc.max_hp   = 15;
    npc.faction  = "";  // Neutral — operates on both sides of the law
    add_effect(npc.effects, make_invulnerable());
    npc.quickness = 0;

    // Seed merchant stock from keeper_seed for determinism.
    std::mt19937 stock_rng(static_cast<uint32_t>(ctx.keeper_seed ^ 0xBADB007));
    // TODO(contraband-flag): mark black-market inventory items as contraband once
    // the Item::contraband_ field exists.  For now reuse the standard stock
    // generator; the Fixer's shop name signals the illicit nature.

    // --- Talk ---
    npc.interactions.talk = TalkTrait{
        "I didn't see you come in. Good.",
        {
            // Node 0: introduction
            {
                "I move things that other people don't want moved. "
                "You need something, I probably have it. Credits only.",
                {
                    {"What kind of things?", 1},
                    {"I'll look.", -1},
                },
            },
            // Node 1: inventory hint
            {
                "Weapons. Parts. The occasional restricted component. "
                "Don't ask about the serial numbers.",
                {
                    {"Understood.", -1},
                },
            },
        },
    };

    // --- Shop ---
    npc.interactions.shop = ShopTrait{
        "Fixer's Wares",
        generate_merchant_stock(stock_rng, 0),
    };

    return npc;
}

} // namespace astra
