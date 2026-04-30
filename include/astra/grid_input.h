#pragma once

namespace astra {

class Game;

namespace grid_input {

// Returns true if the input consumed a turn (caller drives advance_world).
bool handle(Game& game, int key);

} // namespace grid_input
} // namespace astra
