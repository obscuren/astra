#pragma once

#include <string>

namespace astra {

class Game;
class Renderer;

namespace net_renderer {

void render(Game& game, Renderer& r);

// Slice 1 self-test: validates the six-band layout geometry across a
// range of screen sizes / deck-slot counts. Returns true if all band
// invariants hold; on failure returns false and sets `err` to the first
// violation. Pure — constructs geometry only, no rendering.
bool selftest_bands(std::string& err);

} // namespace net_renderer
} // namespace astra
