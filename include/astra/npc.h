#pragma once

#include "astra/interaction.h"
#include "astra/renderer.h"

#include <cstdint>
#include <random>
#include <string>

namespace astra {

enum class Disposition : uint8_t {
    Friendly,
    Neutral,
    Hostile,
};

enum class Race : uint8_t {
    Human,
    Veldrani,   // tall, blue-skinned traders and diplomats
    Kreth,      // stocky, mineral-skinned engineers
    Sylphari,   // wispy, luminescent wanderers
    Xytomorph,  // hostile chitinous predators
};

struct Npc {
    int x = 0;
    int y = 0;
    char glyph = '?';
    Color color = Color::White;
    std::string name;           // personal name, e.g. "Krath"
    std::string role;           // title, e.g. "Station Keeper"
    Race race = Race::Human;
    int hp = 1;
    int max_hp = 1;
    Disposition disposition = Disposition::Neutral;
    bool invulnerable = false;
    InteractionData interactions;

    // "Krath the Station Keeper" or just "Angry Xytomorph" if no personal name
    std::string display_name() const;
};

// NPC templates define the archetype; the factory fills in name/race.
enum class NpcRole : uint8_t {
    StationKeeper,
    Merchant,
    Drifter,
    Xytomorph,
};

// Create a fully configured NPC. Race is used for name generation.
// For hostile types the race is implicit (e.g. Xytomorph).
Npc create_npc(NpcRole role, Race race, std::mt19937& rng);

// Generate a personal name appropriate for a given race.
std::string generate_name(Race race, std::mt19937& rng);

} // namespace astra
