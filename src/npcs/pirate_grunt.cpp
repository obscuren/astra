#include "astra/npc_defs.h"
#include "astra/creature_flags.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

Npc build_pirate_grunt() {
    Npc npc;
    npc.race     = Race::Human;
    npc.npc_role = NpcRole::Scavenger;  // Closest hostile-human role; no new enums this task.
    npc.role     = "Pirate";
    // Task 13 will roll a real name.
    npc.name     = "Pirate";
    npc.hp       = 18;
    npc.max_hp   = 18;
    npc.faction  = "";
    npc.quickness = 100;
    npc.base_xp  = 30;
    npc.base_damage = 2;
    npc.dv       = 8;
    npc.av       = 2;
    npc.damage_dice = Dice::make(1, 6);
    npc.damage_type = DamageType::Kinetic;

    // --- Talk ---
    npc.interactions.talk = TalkTrait{
        "Back off.",
        {
            // Node 0: dismissal
            {
                "I've got nothing to say to you. Keep moving.",
                {
                    {"Fine.", -1},
                },
            },
        },
    };

    npc.flags |= static_cast<uint64_t>(CreatureFlag::Biological);
    return npc;
}

} // namespace astra
