#pragma once

#include <cstdint>

namespace astra {

enum class GameState : uint8_t {
    MainMenu,
    Playing,
    GameOver,
    LoadMenu,
    HallOfFame,
    Net,
};

} // namespace astra
