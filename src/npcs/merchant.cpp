#include "astra/npc_defs.h"

namespace astra {

Npc build_merchant(Race race, std::mt19937& rng) {
    Npc npc;
    npc.race = race;
    npc.glyph = 'M';
    npc.color = Color::Cyan;
    npc.role = "Merchant";
    npc.hp = 15;
    npc.max_hp = 15;
    npc.disposition = Disposition::Neutral;
    npc.invulnerable = true;
    npc.quickness = 0;
    npc.name = generate_name(race, rng);

    // --- Talk: brief introduction ---
    npc.interactions.talk = TalkTrait{
        "What do you want? I haven't got all cycle.",
        {
            // Node 0: who are you
            {
                "I'm a trader. I move goods between stations. If you've "
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
    };

    return npc;
}

} // namespace astra
