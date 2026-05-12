#pragma once
#include <string>
#include "astra/renderer.h"  // colored() + Color

namespace astra {

// White "Passive:" / "Active:" / "Trigger:" prefix, then a space, then body text.
inline std::string desc_passive(const std::string& body) {
    return colored("Passive:", Color::White) + " " + body;
}
inline std::string desc_active(const std::string& body) {
    return colored("Active:", Color::White) + " " + body;
}
inline std::string desc_trigger(const std::string& body) {
    return colored("Trigger:", Color::White) + " " + body;
}

// Dice notation (e.g. "1d4") rendered yellow.
inline std::string desc_dice(const std::string& dice) {
    return colored(dice, Color::Yellow);
}

// Keybind (e.g. 'd') rendered "[d]" in yellow.
inline std::string desc_key(char k) {
    std::string s = "[";
    s += k;
    s += "]";
    return colored(s, Color::Yellow);
}

// Effect name rendered in the effect's canonical color.
inline std::string desc_effect(const std::string& name, Color c) {
    return colored(name, c);
}

}  // namespace astra
