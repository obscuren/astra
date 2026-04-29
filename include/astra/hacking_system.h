#pragma once

#include <cstdint>
#include <string>

namespace astra {

class Game;
struct Hackable;
struct Item;
class TileMap;

// Detection counter [0, 100]. One active counter for the player's current
// zone. Reset on zone change. Decays linearly while the player is in
// steady state (no quickhacks fired recently).
struct DetectionState {
    int  value      = 0;     // [0, 100]
    int  decay_acc  = 0;     // tick accumulator; -1 per N ticks
};

class HackingSystem {
public:
    HackingSystem() = default;

    // ── Targeting ── (full body lands in Task 8)
    bool targeting() const { return targeting_; }
    int  target_x() const { return target_x_; }
    int  target_y() const { return target_y_; }
    int  blink_phase() const { return blink_phase_; }
    void tick_blink() { ++blink_phase_; }

    void begin_quickhack_targeting(Game& game);
    void cancel_targeting() { targeting_ = false; }
    void handle_targeting_input(int key, Game& game);

    // ── Detection ──
    int  detection() const { return detection_.value; }
    void add_detection(int delta);
    void reset_zone();
    void tick(Game& game);   // called from Game::advance_world

    DetectionState& detection_state_mut() { return detection_; }
    const DetectionState& detection_state() const { return detection_; }

    // ── Quickhack execution ── (full body lands in Task 9 once
    // program_effects.h is real; in Task 7 the dispatch is a stub.)
    std::string execute_quickhack(Game& game, const Item& program, Hackable& target,
                                  int target_x, int target_y);

private:
    bool targeting_ = false;
    int  target_x_ = 0;
    int  target_y_ = 0;
    int  blink_phase_ = 0;

    DetectionState detection_;

    // Cached zone-change detector. Composes navigation + zone_x/zone_y.
    // When the signature changes, detection_ resets to zero.
    uint64_t last_zone_signature_ = 0;
    static uint64_t compute_zone_signature(const Game& game);
};

} // namespace astra
