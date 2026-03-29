#include "astra/npc_defs.h"

namespace astra {

Npc build_station_keeper(Race race, std::mt19937& rng) {
    Npc npc;
    npc.race = race;
    npc.glyph = 'K';
    npc.color = Color::Green;
    npc.role = "Station Keeper";
    npc.hp = 20;
    npc.max_hp = 20;
    npc.disposition = Disposition::Friendly;
    npc.faction = "Stellari Conclave";
    add_effect(npc.effects, make_invulnerable());
    npc.quickness = 0;
    npc.name = generate_name(race, rng);

    // --- Talk: station lore ---
    npc.interactions.talk = TalkTrait{
        "Greetings, commander. Welcome to The Heavens Above.",
        {
            // Node 0: main conversation hub
            {
                "This station has orbited Jupiter for over three centuries. "
                "Built by the first wave, before the Collapse.",
                {
                    {"Tell me more about the Collapse.", 1},
                    {"Interesting. What else?", 0},
                    {"Thanks.", -1},
                },
            },
            // Node 1: the Collapse
            {
                "Nobody knows what caused it. One cycle the relay network "
                "went dark, and the old worlds fell silent. We've been "
                "rebuilding ever since.",
                {
                    {"Is that why we travel through blackholes?", 2},
                    {"Heavy stuff. Anything else?", 0},
                    {"I see.", -1},
                },
            },
            // Node 2: blackhole travel
            {
                "The ancients left the gate network behind. Hyperspace "
                "engines and navi computers let us use them, but nobody "
                "truly understands how they work.",
                {
                    {"Back to the beginning.", 0},
                    {"Thanks for the history lesson.", -1},
                },
            },
        },
    };

    // --- Quest: missing cargo hauler ---
    npc.interactions.quest = QuestTrait{
        "Any work available?",
        {
            // Node 0: quest offer
            {
                "Actually, yes. A cargo hauler went dark near the "
                "asteroid belt. Last transponder ping was two cycles ago. "
                "Could use someone to check it out.",
                {
                    {"I'll look into it.", 1},
                    {"Sounds dangerous. Maybe later.", -1},
                },
            },
            // Node 1: accepted
            {
                "Good. I'll upload the coordinates to your navi computer. "
                "Be careful out there, commander.",
                {
                    {"Understood.", -1},
                },
            },
        },
    };

    return npc;
}

} // namespace astra
