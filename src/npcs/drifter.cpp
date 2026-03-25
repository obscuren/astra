#include "astra/npc_defs.h"

namespace astra {

Npc build_drifter(Race race, std::mt19937& rng) {
    Npc npc;
    npc.race = race;
    npc.glyph = 'D';
    npc.color = Color::White;
    npc.role = "Drifter";
    npc.hp = 8;
    npc.max_hp = 8;
    npc.disposition = Disposition::Neutral;
    npc.quickness = 50;
    npc.base_xp = 10;
    npc.base_damage = 1;
    npc.name = generate_name(race, rng);

    // --- Talk: passing through ---
    npc.interactions.talk = TalkTrait{
        "Just passing through. Don't mind me.",
        {
            // Node 0: small talk
            {
                "Came in on a freight shuttle from the outer ring. "
                "Heard there's work here, but so far... nothing.",
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

    return npc;
}

} // namespace astra
