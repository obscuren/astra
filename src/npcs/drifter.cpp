#include "astra/npc_defs.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

Npc build_drifter(Race race, std::mt19937& rng) {
    Npc npc;
    npc.race = race;
    npc.npc_role = NpcRole::Drifter;
    npc.role = "Drifter";
    npc.hp = 8;
    npc.max_hp = 8;
    npc.quickness = 50;
    npc.base_xp = 10;
    npc.base_damage = 1;
    npc.dv = 10; npc.av = 2;
    npc.damage_dice = Dice::make(1, 4);
    npc.damage_type = DamageType::Kinetic;
    npc.name = generate_name(race, rng);
    npc.faction = Faction_DriftCollective;

    // --- Talk: passing through ---
    npc.interactions.talk = TalkTrait{
        "Just passing through. Don't mind me.",
        {
            // Node 0: small talk
            {
                "Been wandering for a while now. Wherever the next "
                "ride goes, that's where I'll be.",
                {
                    {"Where are you headed next?", 1},
                    {"Good luck out there.", -1},
                },
            },
            // Node 1: destination
            {
                "Wherever the next shuttle goes. Maybe the belt, maybe "
                "further. When you've got nothing to lose, every direction "
                "is the right one.",
                {
                    {"That's one way to look at it.", 2},
                    {"Safe travels.", -1},
                },
            },
            // Node 2: philosophy
            {
                "You learn things out in the drift. The void doesn't "
                "care about your plans. Best thing you can do is keep "
                "moving and stay sharp.",
                {
                    {"I'll keep that in mind.", -1},
                },
            },
        },
    };

    // --- Quest: odd jobs from the drift ---
    npc.interactions.quest = QuestTrait{
        "Heard any rumors out there?",
        {
            // Node 0: quest offer
            {
                "Matter of fact, yeah. Ran into some trouble on "
                "my way through. Could use someone to deal with it.",
                {
                    {"Tell me more.", 1},
                    {"Sounds like your problem.", -1},
                },
            },
            // Node 1: accepted
            {
                "Appreciate it. Watch yourself out there.",
                {
                    {"Will do.", -1},
                },
            },
        },
    };

    return npc;
}

Npc build_hub_drifter(Race race, std::mt19937& rng) {
    Npc npc = build_drifter(race, rng);

    // Override dialog with station-specific flavor
    npc.interactions.talk = TalkTrait{
        "Just passing through. Don't mind me.",
        {
            {
                "Came in on a freight shuttle from the outer ring. "
                "Heard there's work on this station, but so far... nothing.",
                {
                    {"Where are you headed next?", 1},
                    {"Good luck out there.", -1},
                },
            },
            {
                "Wherever the next shuttle goes. Maybe the belt, maybe "
                "further. When you've got nothing to lose, every direction "
                "is the right one.",
                {
                    {"That's one way to look at it.", 2},
                    {"Safe travels.", -1},
                },
            },
            {
                "You learn things out in the drift. The void doesn't "
                "care about your plans. Best thing you can do is keep "
                "moving and stay sharp.",
                {
                    {"I'll keep that in mind.", -1},
                },
            },
        },
    };

    return npc;
}

} // namespace astra
