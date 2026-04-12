#include "astra/npc_defs.h"

namespace astra {

// Random civilian role titles
static const char* civilian_roles[] = {
    "Resident", "Traveler", "Spacer", "Pilgrim", "Scavenger",
    "Hauler", "Refugee", "Prospector", "Dockhand", "Navigator",
    "Mechanic", "Courier", "Surveyor", "Freelancer",
};
static constexpr int civilian_role_count = 14;

// Dialog pools by theme
static const char* station_dialog[] = {
    "This station's seen better days, but it beats drifting.",
    "I've been stuck here for three cycles. Waiting on a hauler contract.",
    "Watch your credits around here. Not everyone's honest.",
    "You hear about the Xytomorph sightings in the lower decks?",
    "The food terminal's protein bars taste like recycled hull sealant.",
    "I used to run cargo for the Kreth Mining Guild. Pays well if you survive.",
    "Jupiter looks different every cycle. Never gets old.",
    "They say something's stirring near Sgr A*. Anomalies, energy spikes.",
    "The Station Keeper's been here since before most of us were born.",
    "If you're heading out past the belt, stock up. Supply runs thin fast.",
    "I knew a crew that went deep into Xytomorph territory. Never came back.",
    "The Conclave used to have a bigger presence here. Times change.",
    "My ship's in dry dock. Thruster alignment's shot.",
    "Word of advice? Don't trust anyone selling discount nav data.",
    "I've seen things out past the relay network. Things that shouldn't exist.",
    "The old gate network still works, mostly. Nobody knows who built it.",
};
static constexpr int station_dialog_count = 16;

static const char* settlement_dialog[] = {
    "We don't get many visitors out here.",
    "This settlement's small, but it's home.",
    "The soil here's decent for growing. Better than most rocks.",
    "Keep an eye out for predators after dark.",
    "We trade what we grow for supplies from the haulers.",
    "I came here to get away from station politics.",
    "Life's quiet here. That's how I like it.",
    "You looking for work? There's always something needs doing.",
};
static constexpr int settlement_dialog_count = 8;

static const char* general_dialog[] = {
    "Hmm?",
    "Don't mind me.",
    "Safe travels, stranger.",
    "You look like you've been through something.",
    "...",
    "I've got nothing for you.",
    "Another day, another cycle.",
};
static constexpr int general_dialog_count = 7;

Npc build_civilian(Race race, std::mt19937& rng) {
    Npc npc;
    npc.race = race;
    npc.npc_role = NpcRole::Civilian;
    npc.role = civilian_roles[std::uniform_int_distribution<int>(
        0, civilian_role_count - 1)(rng)];
    npc.hp = std::uniform_int_distribution<int>(5, 10)(rng);
    npc.max_hp = npc.hp;
    npc.disposition = Disposition::Friendly;
    npc.quickness = 0;
    npc.name = generate_name(race, rng);

    // Pick 2 random dialog lines — one greeting, one deeper
    auto pick_line = [&](const char** pool, int count) -> std::string {
        return pool[std::uniform_int_distribution<int>(0, count - 1)(rng)];
    };

    std::string greeting = pick_line(general_dialog, general_dialog_count);
    std::string topic = pick_line(general_dialog, general_dialog_count);

    npc.interactions.talk = TalkTrait{
        greeting,
        {
            {
                topic,
                {
                    {"Interesting.", -1},
                },
            },
        },
    };

    return npc;
}

Npc build_random_civilian(std::mt19937& rng) {
    static constexpr Race friendly_races[] = {
        Race::Human, Race::Veldrani, Race::Kreth, Race::Sylphari,
    };
    Race race = friendly_races[std::uniform_int_distribution<int>(0, 3)(rng)];
    return build_civilian(race, rng);
}

Npc build_hub_civilian(Race race, std::mt19937& rng) {
    Npc npc = build_civilian(race, rng);

    // Override dialog with station-specific flavor
    auto pick_line = [&](const char** pool, int count) -> std::string {
        return pool[std::uniform_int_distribution<int>(0, count - 1)(rng)];
    };

    std::string greeting = pick_line(general_dialog, general_dialog_count);
    std::string topic = pick_line(station_dialog, station_dialog_count);

    npc.interactions.talk = TalkTrait{
        greeting,
        {
            {
                topic,
                {
                    {"Interesting.", -1},
                },
            },
        },
    };

    return npc;
}

Npc build_random_hub_civilian(std::mt19937& rng) {
    static constexpr Race friendly_races[] = {
        Race::Human, Race::Veldrani, Race::Kreth, Race::Sylphari,
    };
    Race race = friendly_races[std::uniform_int_distribution<int>(0, 3)(rng)];
    return build_hub_civilian(race, rng);
}

} // namespace astra
