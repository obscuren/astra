#pragma once

#include "astra/net_session.h"
#include "astra/program_compiler.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace astra {

class Game;
struct Hackable;
struct Item;
class TileMap;

// An in-flight LOOP sustain — a program that re-fires its body each turn
// until its loop count expires. Runtime-only state; not persisted.
struct ActiveSustain {
    CompiledProgram program;
    int             turns_remaining = 0;
    int             target_x = 0;
    int             target_y = 0;
    int             ram_held = 0;
};

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
    NetSession*       session()       { return session_ ? &*session_ : nullptr; }
    const NetSession* session() const { return session_ ? &*session_ : nullptr; }
    // Dev-tooling accessor — mutable session pointer (same null-safety as session()).
    NetSession* session_mut() { return session_ ? &*session_ : nullptr; }

    // Returns true if jack-in succeeded (preconditions met). Logs reason on failure.
    // The TargetDescriptor selects the per-target grammar; callers build
    // it from the meatworld fixture (FixtureType + HackTagMask + seed)
    // or pass an explicit kind for dev / scripted jack-ins.
    bool jack_in(Game& game, TargetDescriptor desc);

    // Drains/persists loot per kind, restores body, returns to previous game state.
    void jack_out(Game& game, JackOutKind kind);

    // Per-turn Grid update. Called from Game::advance_world when state == Grid.
    void tick_grid(Game& game);

    // True while a blocking full-window sequence is playing — input and
    // world progression are suspended.
    bool in_blocking_transition() const;

    // Called from the run loop after WindowSequence is ticked. Finalizes
    // a completed sequence (band recompute / deferred jack-out teardown).
    void on_window_sequence_complete(Game& game);

    // Records the kind that just finished (called from game.cpp run loop
    // before on_window_sequence_complete, where hacking_ is in scope).
    void notify_sequence_finished(WindowSeqKind k);

    // True after the first Opening sequence has fully completed this process.
    // Used by net_input to enable the skip-held fast-forward on repeat jacks.
    bool has_seen_ritual() const;

    // One-frame transient: true for the first meatworld frame after a
    // panic jack-out, so the meatworld player glyph draws corrupted (~@~).
    // Auto-clears on read.
    bool consume_panic_meat_glitch();

    // Register an in-flight LOOP sustain. Called by fire_program when a
    // program with loop_count > 0 fires. The body will re-fire each
    // subsequent world tick at loop_intensity_mult intensity until the
    // turn counter expires; the reserved RAM is returned on expiration.
    void register_sustain(const CompiledProgram& prog, int tx, int ty);

    // Initiate a Black ICE takeover sequence. Stub — real impl in Task 8.
    void request_takeover();

private:
    bool targeting_ = false;
    int  target_x_ = 0;
    int  target_y_ = 0;
    int  blink_phase_ = 0;

    DetectionState detection_;

    std::optional<NetSession> session_;

    // Active LOOP sustains. Runtime-only; not persisted (the design accepts
    // that a save-load mid-sustain drops the in-flight effect).
    std::vector<ActiveSustain> sustains_;

    Game* game_ = nullptr;

    // Cached zone-change detector. Composes navigation + zone_x/zone_y.
    // When the signature changes, detection_ resets to zero.
    uint64_t last_zone_signature_ = 0;
    static uint64_t compute_zone_signature(const Game& game);

    void on_detection_threshold_(int threshold);

    JackOutKind   pending_jack_out_ = JackOutKind::Voluntary;
    bool          panic_meat_glitch_ = false;
    WindowSeqKind finished_seq_ = WindowSeqKind::None;

    void commit_loot_(Game& game, NetLootBuffer& loot, int pct);
    void spawn_black_ice_(NetSession& s);
    void spawn_gray_ice_reinforcement_(NetSession& s);

    // Resolve the sector for `node` into `s.sector`, applying any persisted
    // mutations from `lan_metadata`. Pure data-side helper — does NOT touch
    // avatar position, ICE, or session identity.
};

} // namespace astra
