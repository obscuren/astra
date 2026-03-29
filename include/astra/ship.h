#pragma once

#include "astra/item.h"

#include <optional>
#include <random>
#include <string>

namespace astra {

struct Starship {
    std::string name;
    std::string type = "Light Freighter";

    std::optional<Item> engine;
    std::optional<Item> hull;
    std::optional<Item> navi_computer;
    std::optional<Item> shield;
    std::optional<Item> utility1;
    std::optional<Item> utility2;

    std::optional<Item>& slot_ref(ShipSlot slot);
    const std::optional<Item>& slot_ref(ShipSlot slot) const;

    ShipModifiers total_modifiers() const;

    bool operational() const;       // true if engine installed
    bool has_navigation() const;    // true if navi_computer installed
};

std::string generate_ship_name(std::mt19937& rng);

} // namespace astra
