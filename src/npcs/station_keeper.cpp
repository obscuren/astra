#include "astra/npc_defs.h"
#include "astra/renderer.h"
#include "astra/faction.h"
#include "astra/station_type.h"
#include "astra/keeper_personas.h"

namespace astra {

static const char* specialty_flavor(StationSpecialty s) {
    switch (s) {
        case StationSpecialty::Mining:     return "Mostly ore runs out of here.";
        case StationSpecialty::Research:   return "A lot of lab work. Quiet crew.";
        case StationSpecialty::Frontier:   return "Rough crowd — we handle our own.";
        case StationSpecialty::Trade:      return "Trade routes converge here.";
        case StationSpecialty::Industrial: return "Machines never sleep.";
        default:                           return "Just a waypoint.";
    }
}

Npc build_station_keeper(Race race, std::mt19937& rng, const StationContext& ctx) {
    Npc npc;
    npc.race = race;
    npc.npc_role = NpcRole::StationKeeper;
    npc.role = "Station Keeper";
    npc.hp = 20;
    npc.max_hp = 20;
    npc.faction = Faction_StellariConclave;
    add_effect(npc.effects, make_invulnerable());
    npc.quickness = 0;
    npc.name = generate_name(race, rng);

    if (ctx.is_tha) {
        // --- THA: station lore + ship repair guidance ---
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
                    "Here's what I can tell you: the " +
                    colored("Maintenance Tunnels", Color::Yellow) +
                    " below deck are crawling with Xytomorph pests. Nasty, "
                    "but there's salvage down there — probably an " +
                    colored("Engine Coil", Color::White) +
                    " if you're lucky. Talk to the " +
                    colored("Engineer", Color::White) + " " +
                    colored("(E)", Color::Yellow) +
                    ", he can point you to the hatch. For " +
                    colored("Hull Plating", Color::White) +
                    ", try the " +
                    colored("Merchant", Color::White) + " " +
                    colored("(M)", Color::Cyan) +
                    " in the market — he stocks ship components. "
                    "And the " +
                    colored("Station Commander", Color::White) + " " +
                    colored("(C)", Color::White) +
                    " might have a spare " +
                    colored("Nav Computer", Color::White) +
                    ". Do him a favor and he'll sort you out.",
                    {
                        {"Where's the Engineer?", 4},
                        {"Where's the Merchant?", 5},
                        {"Thanks for the intel.", -1},
                    },
                },
                // Node 4: Engineer location
                {
                    "Engineering bay, east side of the station. Look for "
                    "the fellow covered in conduit grease — " +
                    colored("Engineer", Color::White) + " " +
                    colored("(E)", Color::Yellow) +
                    ". He knows every bolt on this station. He'll get you "
                    "to the tunnels.",
                    {
                        {"Got it.", -1},
                    },
                },
                // Node 5: Merchant location
                {
                    "The market's in the cantina area. You'll find the " +
                    colored("Merchant", Color::White) + " " +
                    colored("(M)", Color::Cyan) +
                    " there. He deals in all sorts — supplies, "
                    "components, whatever the haulers bring in.",
                    {
                        {"Thanks.", -1},
                    },
                },
            },
        };

        // --- THA Quest: missing cargo hauler ---
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
    } else {
        // --- Generic: rolled name, archetype greeting, specialty flavor + hook ---
        npc.name = pick_keeper_name(ctx.keeper_seed);

        std::string flavor = specialty_flavor(ctx.specialty);
        std::string station = ctx.station_name.empty() ? "this station" : ctx.station_name;
        std::string hook = keeper_specialty_hook(ctx);

        KeeperArchetype arch = pick_keeper_archetype(ctx.keeper_seed);
        std::string greeting;
        switch (arch) {
            case KeeperArchetype::GruffVeteran:
                greeting = "State your business.";
                break;
            case KeeperArchetype::ChattyBureaucrat:
                greeting = "Welcome, welcome! Do you have your papers in order?";
                break;
            case KeeperArchetype::NervousNewcomer:
                greeting = "Oh — hi, hello. Sorry, still learning the ropes.";
                break;
            case KeeperArchetype::RetiredSpacer:
                greeting = "Another face from out there. Sit, sit.";
                break;
            case KeeperArchetype::CorporateStiff:
                greeting = "Greetings. I represent station management.";
                break;
            case KeeperArchetype::EccentricLoner:
                greeting = "Mm. You're new.";
                break;
        }

        npc.interactions.talk = TalkTrait{
            greeting,
            {
                // Node 0: brief orientation
                {
                    flavor + " Heard pirates have been hitting the outer belts lately — "
                    "watch yourself if you're heading out.",
                    {
                        {"Anything else going on?", 1},
                        {"Understood.", -1},
                    },
                },
                // Node 1: specialty hook
                {
                    hook,
                    {
                        {"Good to know.", -1},
                    },
                },
            },
        };
    }

    return npc;
}

} // namespace astra
