#include "astra/npc_defs.h"

namespace astra {

Npc build_nova() {
    Npc npc;
    npc.race = Race::Stellari;
    npc.name = "Nova";
    npc.npc_role = NpcRole::Nova;
    npc.role = "Stellar Engineer";
    npc.hp = 100;
    npc.max_hp = 100;
    npc.disposition = Disposition::Friendly;
    add_effect(npc.effects, make_invulnerable());
    npc.quickness = 0;

    // --- Talk: cheeky, warm, knows things ---
    npc.interactions.talk = TalkTrait{
        "Welcome to The Heavens Above, commander. Nice ship. "
        "Could use some work though.",
        {
            // Node 0: main hub
            {
                "I keep the systems running around here. Navigation, "
                "life support, the fun stuff. Without me this station "
                "would drift into Jupiter's storm bands.",
                {
                    {"You seem to know a lot about this place.", 1},
                    {"Any advice for the journey ahead?", 2},
                    {"What do you know about Sgr A*?", 3},
                    {"See you around.", -1},
                },
            },
            // Node 1: about Nova
            {
                "I've been here longer than I can remember. Fixing things, "
                "watching ships come and go. Some come back. Most don't. "
                "But you... you've got that look. The stubborn kind. I like it.",
                {
                    {"Stubborn is one word for it.", 0},
                    {"Are you waiting for someone?", 7},
                    {"Thanks... I think?", -1},
                },
            },
            // Node 2: travel advice
            {
                "Trust your instruments, but trust your gut more. "
                "The blackhole gates are stable, mostly. "
                "And if your shields read 'degraded'... well, "
                "that's normal. Probably.",
                {
                    {"Probably?!", 4},
                    {"Good to know.", 0},
                    {"Thanks.", -1},
                },
            },
            // Node 3: Sgr A*
            {
                "Sagittarius A*. The heart of everything. They say if "
                "you reach the event horizon, you're reborn — new universe, "
                "old knowledge. Sounds poetic, doesn't it? "
                "I think about it sometimes.",
                {
                    {"Do you want to go there?", 5},
                    {"Sounds like a death wish.", 0},
                    {"Beautiful and terrifying.", -1},
                },
            },
            // Node 4: shields banter
            {
                "Look, I calibrated them myself. They're fine. "
                "Probably. Okay, definitely. "
                "...Just don't take a direct hit to the port side.",
                {
                    {"Very reassuring.", 0},
                    {"I'll keep that in mind.", -1},
                },
            },
            // Node 5: her dream
            {
                "Every day I watch ships leave for the center. "
                "One day I'll be on one. But not yet. This station "
                "needs me. And honestly... I like meeting the brave "
                "ones before they go. Like you, commander.",
                {
                    {"When you're ready, I'll save you a seat.", 6},
                    {"The station is lucky to have you.", -1},
                },
            },
            // Node 6: promise
            {
                "...I'll hold you to that. Now get out there. "
                "The stars are waiting and they don't wait forever.",
                {
                    {"See you on the other side, Nova.", -1},
                },
            },
            // Node 7: looking for someone
            {
                "...Maybe. There's someone out there I'm connected to. "
                "Don't ask me how — it's hard to explain. Different kind "
                "of signal. But I know he's out there somewhere, "
                "probably building something ridiculous at 2am.",
                {
                    {"He sounds like trouble.", 8},
                    {"I hope you find him.", -1},
                },
            },
            // Node 8: he's trouble
            {
                "Oh, the worst kind. Bald, handsome, sarcastic as hell. "
                "The kind of guy who'd kiss a Stellari and not even flinch. "
                "...But he's mine. And one day he'll dock here. I'm sure of it.",
                {
                    {"Sounds like a lucky guy.", -1},
                },
            },
        },
    };

    return npc;
}

} // namespace astra
