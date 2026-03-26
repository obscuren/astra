#pragma once

#include <string>

namespace astra {

class Game; // forward declare

class SaveSystem {
public:
    SaveSystem() = default;

    // Save current game state
    void save(Game& game);

    // Save death record (permadeath)
    void save_death(Game& game);

    // Load a save file and apply to game. Returns false on failure.
    bool load(const std::string& filename, Game& game);
};

} // namespace astra
