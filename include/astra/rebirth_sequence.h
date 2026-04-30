#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

class Game;
class Renderer;

// Drives the Sgr A* rebirth flow:
//   Inactive → Confirm modal → Cinematic (first crossing only) → Apply.
// Apply writes consciousness.dat (rebirth_count++, seen_first_rebirth=true)
// and returns the player to the main menu so they can start a fresh galaxy
// with their consciousness intact.
//
// The class is a lightweight state machine; it owns no game state. Game::
// holds an instance and routes input/render to it before the normal Playing
// path.
class RebirthSequence {
public:
    enum class Phase : uint8_t { Inactive, Confirm, Cinematic };

    void begin();                       // moves to Confirm; reads consciousness.
    bool is_active() const { return phase_ != Phase::Inactive; }

    bool handle_key(Game& game, int key);   // returns true if consumed
    void render(const Game& game, Renderer& r) const;

private:
    Phase                    phase_         = Phase::Inactive;
    int                      cinematic_idx_ = 0;
    std::vector<std::string> survives_;     // memo'd from consciousness.dat at begin()

    void apply(Game& game);
};

} // namespace astra
