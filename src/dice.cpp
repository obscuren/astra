#include "astra/dice.h"

namespace astra {

const char* damage_type_name(DamageType t) {
    switch (t) {
        case DamageType::Kinetic:    return "Kinetic";
        case DamageType::Plasma:     return "Plasma";
        case DamageType::Electrical: return "Electrical";
        case DamageType::Cryo:       return "Cryo";
        case DamageType::Acid:       return "Acid";
    }
    return "Unknown";
}

int TypeAffinity::for_type(DamageType t) const {
    switch (t) {
        case DamageType::Kinetic:    return kinetic;
        case DamageType::Plasma:     return plasma;
        case DamageType::Electrical: return electrical;
        case DamageType::Cryo:       return cryo;
        case DamageType::Acid:       return acid;
    }
    return 0;
}

int Dice::roll(std::mt19937& rng) const {
    if (count <= 0 || sides <= 0) return modifier;
    int total = 0;
    std::uniform_int_distribution<int> dist(1, sides);
    for (int i = 0; i < count; ++i) total += dist(rng);
    return total + modifier;
}

int Dice::min() const { return count + modifier; }
int Dice::max() const { return count * sides + modifier; }

std::string Dice::to_string() const {
    if (count <= 0 || sides <= 0) {
        return std::to_string(modifier);
    }
    std::string s = std::to_string(count) + "d" + std::to_string(sides);
    if (modifier > 0) s += "+" + std::to_string(modifier);
    else if (modifier < 0) s += std::to_string(modifier);
    return s;
}

Dice Dice::parse(const std::string& expr) {
    Dice d;
    // Format: NdS+M, NdS-M, NdS, or just M
    auto pos_d = expr.find('d');
    if (pos_d == std::string::npos) {
        d.modifier = std::stoi(expr);
        return d;
    }
    d.count = std::stoi(expr.substr(0, pos_d));
    auto rest = expr.substr(pos_d + 1);
    auto pos_plus = rest.find('+');
    auto pos_minus = rest.find('-');
    if (pos_plus != std::string::npos) {
        d.sides = std::stoi(rest.substr(0, pos_plus));
        d.modifier = std::stoi(rest.substr(pos_plus + 1));
    } else if (pos_minus != std::string::npos) {
        d.sides = std::stoi(rest.substr(0, pos_minus));
        d.modifier = -std::stoi(rest.substr(pos_minus + 1));
    } else {
        d.sides = std::stoi(rest);
    }
    return d;
}

Dice Dice::make(int count, int sides, int modifier) {
    return Dice{count, sides, modifier};
}

} // namespace astra
