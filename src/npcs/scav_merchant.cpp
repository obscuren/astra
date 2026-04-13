#include "astra/npc_defs.h"
#include "astra/item_defs.h"
#include "astra/faction.h"

namespace astra {

Npc build_scav_junk_dealer(Race race, std::mt19937& rng) {
    Npc npc;
    npc.race     = race;
    npc.npc_role = NpcRole::Merchant;
    npc.role     = "Junk Dealer";
    npc.hp       = 15;
    npc.max_hp   = 15;
    npc.faction  = "";  // Unaligned
    add_effect(npc.effects, make_invulnerable());
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
    npc.interactions.shop = ShopTrait{
        npc.name + "'s Salvage",
        generate_merchant_stock(rng, 0),
    };

    return npc;
}

} // namespace astra
