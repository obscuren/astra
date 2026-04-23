#include "astra/npc_defs.h"
#include "astra/item_defs.h"
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
    npc.interactions.shop = ShopTrait{
        npc.name + "'s Supplies",
        generate_merchant_stock(rng, faction_rep),
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
