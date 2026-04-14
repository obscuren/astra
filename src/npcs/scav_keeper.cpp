#include "astra/npc_defs.h"
#include "astra/faction.h"

namespace astra {

Npc build_scav_keeper(Race race, std::mt19937& rng) {
    Npc npc;
    npc.race     = race;
    npc.npc_role = NpcRole::StationKeeper;
    npc.role     = "Station Keeper";
    npc.hp       = 20;
    npc.max_hp   = 20;
    npc.faction  = "";  // Scav stations are unaligned
    add_effect(npc.effects, make_invulnerable());
    npc.quickness = 0;
    // Task 13 will roll a real name from a scav-specific name pool.
    npc.name = "Scav Keeper";

    // --- Talk ---
    npc.interactions.talk = TalkTrait{
        "Everything here's used — take what you pay for.",
        {
            // Node 0: station background
            {
                "This place? Found it drifting. Spent two cycles patching "
                "the hull and getting life support running. It's home now.",
                {
                    {"Who else is here?", 1},
                    {"Fair enough.", -1},
                },
            },
            // Node 1: crew
            {
                "Few salvagers. Junk dealer. We keep to ourselves. "
                "Don't cause trouble and you're welcome to stay.",
                {
                    {"Where's the junk dealer?", 2},
                    {"Got it.", -1},
                },
            },
            // Node 2: junk dealer location
            {
                "Scrap yard. Can't miss the smell.",
                {
                    {"Thanks.", -1},
                },
            },
        },
    };

    return npc;
}

} // namespace astra
