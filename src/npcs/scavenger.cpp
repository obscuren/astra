#include "astra/npc_defs.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

Npc build_scavenger(Race race, std::mt19937& rng) {
    Npc npc;
    npc.race = race;
    npc.npc_role = NpcRole::Scavenger;
    npc.role = "Scavenger";
    npc.hp = 10;
    npc.max_hp = 10;
    npc.quickness = 60;
    npc.base_xp = 12;
    npc.base_damage = 2;
    npc.dv = 9; npc.av = 3;
    npc.damage_dice = Dice::make(1, 6);
    npc.damage_type = DamageType::Kinetic;
    npc.type_affinity = {1, 0, -1, 0, 0};
    npc.name = generate_name(race, rng);
    npc.faction = Faction_DriftCollective;

    // --- Talk: salvaging ruins ---
    npc.interactions.talk = TalkTrait{
        "You here to pick through the wreckage too?",
        {
            // Node 0: salvage talk
            {
                "Most of this place is picked clean, but every now "
                "and then you find something the others missed. A "
                "circuit board, a power cell... it adds up.",
                {
                    {"Find anything good lately?", 1},
                    {"Good luck with that.", -1},
                },
            },
            // Node 1: recent finds
            {
                "Pulled a nav module out of a collapsed bulkhead "
                "yesterday. Pre-war tech. Sold it to a trader for "
                "enough rations to last a cycle.",
                {
                    {"Where do you sell the scraps?", 2},
                    {"Not bad.", -1},
                },
            },
            // Node 2: the trade
            {
                "Anywhere there's someone desperate enough to buy. "
                "Frontier stations, merchant convoys... even the "
                "military pays for salvage if you know who to ask.",
                {
                    {"I'll keep that in mind.", -1},
                },
            },
        },
    };

    return npc;
}

} // namespace astra
