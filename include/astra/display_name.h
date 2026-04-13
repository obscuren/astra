#pragma once

#include "astra/dice.h"
#include "astra/item.h"
#include "astra/npc.h"
#include "astra/race.h"
#include "astra/renderer.h"

#include <string>

namespace astra {

// Consistent display_name() API for all entity types.
// Returns a colored inline string using COLOR_BEGIN/END markers
// suitable for log messages and UI text.

// Race — colored race name
inline std::string display_name(Race r) {
    Color c;
    switch (r) {
        case Race::Human:     c = Color::White; break;
        case Race::Veldrani:  c = Color::Cyan; break;
        case Race::Kreth:     c = Color::Yellow; break;
        case Race::Sylphari:  c = Color::Green; break;
        case Race::Xytomorph: c = Color::Red; break;
        case Race::Stellari:  c = Color::Magenta; break;
        default:              c = Color::White; break;
    }
    return colored(race_name(r), c);
}

// NPC — colored by race
inline std::string display_name(const Npc& npc) {
    Color c;
    switch (npc.race) {
        case Race::Human:     c = Color::White; break;
        case Race::Veldrani:  c = Color::Cyan; break;
        case Race::Kreth:     c = Color::Yellow; break;
        case Race::Sylphari:  c = Color::Green; break;
        case Race::Xytomorph: c = Color::Red; break;
        case Race::Stellari:  c = Color::Magenta; break;
        default:              c = Color::White; break;
    }
    return colored(npc.display_name(), c);
}

// Item — colored by rarity
inline std::string display_name(const Item& item) {
    return colored(item.display_name(), rarity_color(item.rarity));
}

// DamageType — colored by type
inline std::string display_name(DamageType t) {
    Color c;
    switch (t) {
        case DamageType::Kinetic:    c = Color::White; break;
        case DamageType::Plasma:     c = Color::Red; break;
        case DamageType::Electrical: c = Color::Cyan; break;
        case DamageType::Cryo:       c = Color::Blue; break;
        case DamageType::Acid:       c = Color::Green; break;
        default:                     c = Color::White; break;
    }
    return colored(damage_type_name(t), c);
}

} // namespace astra
