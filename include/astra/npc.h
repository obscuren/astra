#pragma once

#include "astra/renderer.h"

#include <cstdint>
#include <string>

namespace astra {

enum class Disposition : uint8_t {
    Friendly,
    Neutral,
    Hostile,
};

struct Npc {
    int x = 0;
    int y = 0;
    char glyph = '?';
    Color color = Color::White;
    std::string name;
    int hp = 1;
    int max_hp = 1;
    Disposition disposition = Disposition::Neutral;
    bool invulnerable = false;
};

} // namespace astra
