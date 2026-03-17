#pragma once

namespace astra {

struct ActionCost {
    static constexpr int move = 50;
    static constexpr int attack = 100;
    static constexpr int interact = 50;
    static constexpr int wait = 50;
};

static constexpr int energy_threshold = 100;

} // namespace astra
