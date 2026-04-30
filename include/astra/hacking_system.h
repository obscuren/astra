#pragma once

#include "astra/grid_session.h"

#include <cstdint>
#include <optional>
#include <string>

namespace astra {

class Game;
struct GridNodeId;
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

    void bind_game(Game* g) { game_ = g; }

    // ── Targeting ── (full body lands in Task 8)
    bool targeting() const { return targeting_; }
    int  target_x() const { return target_x_; }
    int  target_y() const { return target_y_; }
    int  blink_phase() const { return blink_phase_; }
    void tick_blink() { ++blink_phase_; }

    void begin_quickhack_targeting(Game& game);
    void cancel_targeting() { targeting_ = false; }
    void handle_targeting_input(int key, Game& game);
    void reset();   // clear targeting + blink (called from Game::new_game)

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

    // ── Grid lifecycle ──
    bool jacked_in() const { return session_.has_value(); }
    GridSession*       session()       { return session_ ? &*session_ : nullptr; }
    const GridSession* session() const { return session_ ? &*session_ : nullptr; }

    // Returns true if jack-in succeeded (preconditions met). Logs reason on failure.
    bool jack_in(Game& game, GridNodeId entry_node);

    // Drains/persists loot per kind, restores body, returns to previous game state.
    void jack_out(Game& game, JackOutKind kind);

    // Per-turn Grid update. Called from Game::advance_world when state == Grid.
    void tick_grid(Game& game);

private:
    bool targeting_ = false;
    int  target_x_ = 0;
    int  target_y_ = 0;
    int  blink_phase_ = 0;

    DetectionState detection_;

    std::optional<GridSession> session_;

    Game* game_ = nullptr;

    // Cached zone-change detector. Composes navigation + zone_x/zone_y.
    // When the signature changes, detection_ resets to zero.
    uint64_t last_zone_signature_ = 0;
    static uint64_t compute_zone_signature(const Game& game);

    void on_detection_threshold_(int threshold);

    void commit_loot_(Game& game, GridLootBuffer& loot, int pct);
    void spawn_black_ice_(GridSession& s);
    void spawn_gray_ice_reinforcement_(GridSession& s);
};

} // namespace astra
