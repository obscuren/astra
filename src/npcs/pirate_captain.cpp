#include "astra/npc_defs.h"
#include "astra/dice.h"
#include "astra/faction.h"
#include "astra/station_type.h"

namespace astra {

Npc build_pirate_captain(const StationContext& /*ctx*/) {
    Npc npc;
    npc.race     = Race::Human;
    npc.npc_role = NpcRole::Scavenger;  // Closest hostile-human role; Task 13 may refine.
    npc.role     = "Pirate Captain";
    // Task 13 will roll a real name from a pirate name pool.
    npc.name     = "Pirate Captain";
    npc.hp       = 40;
    npc.max_hp   = 40;
    npc.faction  = "";  // Pirate station — unaligned to any formal faction
    npc.quickness = 100;
    npc.base_xp  = 80;
    npc.base_damage = 4;
    npc.dv       = 12;
    npc.av       = 6;
    npc.damage_dice = Dice::make(2, 6);
    npc.damage_type = DamageType::Kinetic;
    npc.elite    = true;

    // --- Talk ---
    npc.interactions.talk = TalkTrait{
        "You've got some nerve coming in here.",
        {
            // Node 0: challenge
            {
                "This station is mine. Every crate, every corridor. "
                "You're here because I allow it — don't forget that.",
                {
                    {"Who are you?", 1},
                    {"Understood.", -1},
                },
            },
            // Node 1: identity
            {
                "The name doesn't matter. What matters is I've taken three "
                "freighters this cycle and no one's stopped me yet.",
                {
                    {"What do you want with this place?", 2},
                    {"Impressive.", -1},
                },
            },
            // Node 2: motivation
            {
                "A base. A resupply point. Somewhere off the shipping lanes "
                "where the guilds won't look too hard.",
                {
                    {"Fair enough.", -1},
                },
            },
        },
    };

    return npc;
}

} // namespace astra
