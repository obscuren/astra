#include "astra/ship.h"
#include "astra/item.h"

namespace astra {

const char* ship_slot_name(ShipSlot slot) {
    switch (slot) {
        case ShipSlot::Engine:       return "Engine";
        case ShipSlot::Hull:         return "Hull";
        case ShipSlot::NaviComputer: return "Navi Computer";
        case ShipSlot::Shield:       return "Shield";
        case ShipSlot::Utility1:     return "Utility 1";
        case ShipSlot::Utility2:     return "Utility 2";
    }
    return "Unknown";
}

std::optional<Item>& Starship::slot_ref(ShipSlot slot) {
    switch (slot) {
        case ShipSlot::Engine:       return engine;
        case ShipSlot::Hull:         return hull;
        case ShipSlot::NaviComputer: return navi_computer;
        case ShipSlot::Shield:       return shield;
        case ShipSlot::Utility1:     return utility1;
        case ShipSlot::Utility2:     return utility2;
    }
    return engine; // unreachable
}

const std::optional<Item>& Starship::slot_ref(ShipSlot slot) const {
    switch (slot) {
        case ShipSlot::Engine:       return engine;
        case ShipSlot::Hull:         return hull;
        case ShipSlot::NaviComputer: return navi_computer;
        case ShipSlot::Shield:       return shield;
        case ShipSlot::Utility1:     return utility1;
        case ShipSlot::Utility2:     return utility2;
    }
    return engine; // unreachable
}

ShipModifiers Starship::total_modifiers() const {
    ShipModifiers total;
    for (int i = 0; i < ship_slot_count; ++i) {
        const auto& sl = slot_ref(static_cast<ShipSlot>(i));
        if (sl) {
            total.hull_hp += sl->ship_modifiers.hull_hp;
            total.shield_hp += sl->ship_modifiers.shield_hp;
            total.warp_range += sl->ship_modifiers.warp_range;
            total.cargo_capacity += sl->ship_modifiers.cargo_capacity;
        }
    }
    return total;
}

bool Starship::operational() const {
    return engine.has_value();
}

bool Starship::has_navigation() const {
    return navi_computer.has_value();
}

std::string generate_ship_name(std::mt19937& rng) {
    static const char* names[] = {
        "The Wanderer", "The Drifter", "Voidrunner", "Stardust",
        "Iron Wake", "Silent Arrow", "Horizon's Edge", "Pale Ember",
        "Nighthawk", "Solar Wind", "The Vagrant", "Deep Six",
        "Starfall", "Ghost Light", "The Nomad", "Dust Devil",
        "Frostbite", "The Corsair", "Void Dancer", "Ash Runner",
    };
    static constexpr int count = sizeof(names) / sizeof(names[0]);
    std::uniform_int_distribution<int> dist(0, count - 1);
    return names[dist(rng)];
}

} // namespace astra
