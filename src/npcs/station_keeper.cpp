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

    // --- Talk: station lore + ship repair guidance ---
    npc.interactions.talk = TalkTrait{
        "Greetings, commander. Welcome to The Heavens Above.",
        {
            // Node 0: main conversation hub
            {
                "This station has orbited Jupiter for over three centuries. "
                "Built by the first wave, before the Collapse.",
                {
                    {"My ship is wrecked. I need parts.", 3},
                    {"Tell me more about the Collapse.", 1},
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
            // Node 3: Ship repair guidance
            {
                "Saw your ship limp in. You're lucky to be alive. "
                "Here's what I can tell you: the maintenance tunnels "
                "below deck are crawling with Xytomorph pests. Nasty, "
                "but there's salvage down there — probably an engine "
                "coil if you're lucky. Talk to the Engineer, he can "
                "point you to the hatch. For hull plating, try the "
                "Merchant in the market — he stocks ship components. "
                "And the Station Commander might have a spare nav "
                "computer. Do him a favor and he'll sort you out.",
                {
                    {"Where's the Engineer?", 4},
                    {"Where's the Merchant?", 5},
                    {"Thanks for the intel.", -1},
                },
            },
            // Node 4: Engineer location
            {
                "Engineering bay, east side of the station. Look for "
                "the fellow covered in conduit grease. He knows every "
                "bolt on this station. He'll get you to the tunnels.",
                {
                    {"Got it.", -1},
                },
            },
            // Node 5: Merchant location
            {
                "The market's in the cantina area. You'll find the "
                "Merchant there. He deals in all sorts — supplies, "
                "components, whatever the haulers bring in.",
                {
                    {"Thanks.", -1},
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
