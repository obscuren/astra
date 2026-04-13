#pragma once

#include <cstdint>
#include <random>
#include <string>

namespace astra {

enum class DamageType : uint8_t {
    Kinetic,
    Plasma,
    Electrical,
    Cryo,
    Acid,
};

constexpr int damage_type_count = 5;

const char* damage_type_name(DamageType t);

struct TypeAffinity {
    int kinetic = 0;
    int plasma = 0;
    int electrical = 0;
    int cryo = 0;
    int acid = 0;

    int for_type(DamageType t) const;
};

struct Dice {
    int count = 0;     // number of dice (e.g. 2 in "2d6+3")
    int sides = 0;     // sides per die (e.g. 6 in "2d6+3")
    int modifier = 0;  // flat modifier (e.g. 3 in "2d6+3")

    int roll(std::mt19937& rng) const;
    int min() const;
    int max() const;
    std::string to_string() const;
    bool empty() const { return count == 0 || sides == 0; }

    static Dice parse(const std::string& expr);
    static Dice make(int count, int sides, int modifier = 0);
};

} // namespace astra
