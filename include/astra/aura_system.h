#pragma once

namespace astra {

class Game;

// Scans every emitter on the current map and applies their auras to
// receivers in range. Run once per world tick inside advance_world,
// after tick_effects + expire_effects and before passive regen.
class AuraSystem {
public:
    void tick(Game& game);
};

} // namespace astra
