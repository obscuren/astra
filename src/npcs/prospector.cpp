#include "astra/npc_defs.h"
#include "astra/dice.h"
#include "astra/faction.h"

namespace astra {

Npc build_prospector(Race race, std::mt19937& rng) {
    Npc npc;
    npc.race = race;
    npc.npc_role = NpcRole::Prospector;
    npc.role = "Prospector";
    npc.hp = 12;
    npc.max_hp = 12;
    npc.quickness = 50;
    npc.base_xp = 15;
    npc.base_damage = 2;
    npc.dv = 8; npc.av = 2;
    npc.damage_dice = Dice::make(1, 4, 1);
    npc.damage_type = DamageType::Kinetic;
    npc.name = generate_name(race, rng);
    npc.faction = Faction_KrethMiningGuild;

    // --- Talk: mineral deposits ---
    npc.interactions.talk = TalkTrait{
        "Careful where you step. Ground's unstable near the deposits.",
        {
            // Node 0: mining talk
            {
                "I've been surveying this sector for months. The "
                "readings are off the charts — titanium veins, "
                "trace zeronium signatures. Could be a fortune "
                "buried under all this rock.",
                {
                    {"What's zeronium?", 1},
                    {"Sounds dangerous.", -1},
                },
            },
            // Node 1: zeronium
            {
                "Rarest mineral in the galaxy. Powers hyperspace "
                "engines, shields, the works. A single gram is worth "
                "more than most starships. Problem is, it's always "
                "buried deep.",
                {
                    {"How do you extract it?", 2},
                    {"I'll leave that to you.", -1},
                },
            },
            // Node 2: extraction
            {
                "Very carefully. Zeronium is volatile until "
                "refined. One wrong move and you're staring at "
                "a crater where your drill rig used to be. But "
                "that's the job.",
                {
                    {"Brave work. Good luck.", -1},
                },
            },
        },
    };

    return npc;
}

} // namespace astra
