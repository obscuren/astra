#pragma once

#include "astra/animation.h"
#include "astra/game_state.h"
#include "astra/net_ice.h"
#include "astra/netspace.h"
#include "astra/program_compiler.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace astra {

enum class WindowSeqKind : uint8_t {
    None,
    Opening,
    ClosingNormal,
    ClosingPanic,
    ForcedHold,
    BlackIceTakeover,
};

// A scripted full-window sequence (jack-in ritual, jack-out teardown,
// Black-ICE takeover). Advances on the WALL-CLOCK from the run loop,
// never the world tick. Per-frame durations come from a static table
// in net_window_anim.cpp (Task 6) keyed by `kind`.
struct WindowSequence {
    WindowSeqKind kind        = WindowSeqKind::None;
    int           frame_index = 0;
    int           elapsed_ms  = 0;
    bool          skip_held   = false;  // held-key fast-forward (Opening/ClosingNormal)
    std::chrono::steady_clock::time_point last_tick =
        std::chrono::steady_clock::now();

    bool active() const { return kind != WindowSeqKind::None; }
};

enum class JackOutKind : uint8_t {
    Voluntary,        // walked to exit node -- full loot, no penalty
    HardJackOut,      // hotkey -- Trace +10, drop 50% loot
    NonBlackDeath,    // avatar HP=0 by gray/white ICE -- body debuff, unsaved loot lost
    BlackIceDeath,    // avatar HP=0 by black ICE -- real HP damage (lethal possible)
    SoftDisconnect,   // load-time recovery -- Trace cleared, no penalty
};

struct GhostDialogChoice {
    std::string text;
    int outcome = 0;  // 0=lore  1=stash  2=provoke
};

struct GhostDialog {
    bool                           open        = false;
    std::vector<std::string>       lines;
    std::vector<GhostDialogChoice> choices;
    int                            sel         = 0;
    int                            node_index  = -1;  // index into Netspace::action_nodes to consume on resolve
};

struct NetLootBuffer {
    int credits             = 0;
    int code_fragments_t1   = 0;
    int code_fragments_t2   = 0;
    std::vector<uint16_t> programs_acquired;   // ProgramId values (cast)
    std::vector<std::string> lore_unlocked;
    bool empty() const;
};

// Phase 5 slice 3a: an in-flight net program occupying a deck slot.
// RAM is reserved at cast and returned when the program completes (NOT
// on cancel). Runtime-only; NetSession is not serialized.
struct NetInFlight {
    int        slot        = -1;   // deck slot index this occupies
    uint16_t   program_id  = 0;    // ProgramId value, for resolve + panel name
    int        turns_total = 1;
    int        turns_left  = 1;
    int        ram_held    = 0;    // reserved RAM, returned on completion only
    int        target_x    = -1;
    int        target_y    = -1;

    // Phase 5 slice 3b: compiled (player-authored fragment chain) path.
    // When `compiled` is true, tick_grid resolves via apply_effect_in_net
    // with `spec` each turn instead of apply_program_in_grid(program_id);
    // `prog_name` drives the deck-panel label.
    bool        compiled  = false;
    EffectSpec  spec{};
    std::string prog_name;

    // Phase 5 slice 3b fix: the cast turn is a no-effect LAUNCH beat.
    // combat.md worked example: a cast "enters the execution queue" and
    // "fires payloads on turns 2 and 4" — NOT on the cast turn. The first
    // tick_grid that sees a compiled entry (the same advance as the cast
    // keypress, before any render) only sets `launched`; effects fire on
    // the following turns, so the panel shows iter 1/N on cast then
    // 2/N..N/N. (Legacy non-compiled entries never set/read this.)
    bool        launched  = false;

    // Phase 5 slice 4: mechanical payload travel. pipe_path = ordered
    // cells avatar-end -> far-end (from net_pipe_path::pipe_path_cells);
    // seg_len = clamp(pipe_path.size(),2,6); each payload is a segment
    // index 0..seg_len (Impact at seg_len). iters_total = k (loop_count
    // else 1); iters_launched counts payloads spawned. target_x/target_y
    // are reused as the far-node Impact cell. Empty pipe_path = a legacy
    // non-travel (self) entry (3b behaviour).
    std::vector<std::pair<int,int>> pipe_path;
    int                             seg_len        = 0;
    std::vector<int>                payloads;
    int                             iters_total    = 1;
    int                             iters_launched = 0;
};

struct NetSession {
    // Body
    int body_x = 0;             // saved overworld/dungeon position
    int body_y = 0;
    GameState body_state = GameState::Playing;

    // Avatar
    int avatar_x = 0;
    int avatar_y = 0;
    int avatar_hp_max = 3;
    int avatar_hp = 3;

    // Resources
    int ram_max = 4;
    int ram = 4;
    int trace = 0;              // [0, 100]
    int trace_alert_pulses = 0; // bookkeeping for breakpoint side effects

    // Implant-derived bonuses cached at jack-in. RAM cap bonus is already
    // baked into ram_max above; heat/cooling/trace bonuses are pulled by
    // their respective consumers via session_effective_*() helpers.
    int heat_cap_bonus       = 0;
    int cooling_rate_bonus   = 0;
    int trace_resistance_pct = 0;  // 0..100; applied to incoming trace gain

    // Tier-derived turn ticks. Drained through trace_carry (every 2 carry units
    // = +1 Trace) so subnet's tick=1 means +1 every 2 turns instead of +1/turn.
    int trace_tick_per_turn = 1;
    int trace_carry         = 0;

    // Skill flags (cached at jack-in)
    bool skill_intrusion          = false;
    bool skill_icebreaking        = false;
    bool skill_daemon_mastery     = false;
    bool skill_ghost_protocol     = false;
    bool skill_deepgrid_navigator = false;
    bool skill_neural_fortitude   = false;
    bool ghost_protocol_used      = false;  // set true after first program of session

    // Netspace: the per-jack-in micro-dungeon (tiles, target descriptor,
    // window state). Source of truth for renderer + input.
    Netspace netspace;

    std::vector<Ice> ice;

    // DaemonHijack: while active, movement keys drive s.ice[hijacked_ice_idx]
    // instead of the avatar. -1 = no active hijack. The countdown decrements
    // once per turn and clears the index when it hits 0. The ICE's own
    // charmed_turns_left independently suppresses its AI for the same window.
    int hijacked_ice_idx    = -1;
    int hijacked_turns_left = 0;

    // Loot accumulated this session (committed on voluntary disconnect).
    NetLootBuffer loot;

    IceColor last_killer_color = IceColor::White;

    // Transient render-state for in-net effects (wall-hit glitch, future
    // pipe payload glyphs, turret rounds, etc.). Not serialized — animations
    // die with the session.
    AnimationManager animations;

    // Phase 3: scripted full-window sequence state (jack-in/out ritual,
    // takeover). Wall-clock driven from Game::run(). Not serialized.
    WindowSequence window_seq;

    // Phase 4: in-net ghost mini-dialog modal. Not serialized.
    GhostDialog ghost_dialog;

    // Phase 3: monotonically increments once per tick_grid (world turn in
    // net). Seeds the per-turn-stable RAM lie so it only re-rolls on a
    // world tick, not every render frame.
    uint32_t net_turn = 0;

    // Phase 5: meatworld clock seed (seconds proxy) captured at jack-in.
    // Footer shows base + net_turn * max(1,time_dilation), formatted
    // HH:MM:SS. Display-only — the meatworld stays paused (no sim, no
    // interrupts). TODO: a later pass could map true time-of-day from
    // DayClock instead of the world-tick proxy.
    int meat_clock_base_secs = 0;

    // Per-session log ring. Read by the Grid HUD's right pane.
    // Capped — push_log drops the oldest entry when full.
    static constexpr size_t kLogCap = 64;
    std::deque<std::string> log_lines;

    void push_log(const std::string& line) {
        log_lines.push_back(line);
        while (log_lines.size() > kLogCap) log_lines.pop_front();
    }
    void clear_log() { log_lines.clear(); }

    // Phase 5: in-net log scrollback. Number of wrapped lines scrolled UP
    // from the live tail. 0 = follow newest (auto-tail). PgUp increases,
    // PgDn decreases (toward 0). Clamped on input; draw_log_pane clamps the
    // effective start precisely against the real wrapped-line count.
    int log_scroll = 0;

    // Phase 5: one-line contextual board status rendered in the field-caption
    // band (e.g. "bolt running SLAM.exe [##....] 2/6"). Set by combat code in
    // later slices; renderer-read only. Empty = nothing drawn.
    std::string field_caption;

    // Apply incoming trace gain through the implant trace_resistance filter.
    // Negative deltas pass through unchanged (cleanses ignore resistance).
    // Returns the new clamped trace value for convenience.
    int gain_trace(int amount);

    // Plan 6: index of the slot whose Telegraph is currently open. -1 when
    // none. The Grid HUD uses this to inverse-video the active program slot.
    int active_slot = -1;

    // Phase 5 slice 4: armed (pre-confirm) program slot; -1 = none.
    int armed_slot = -1;
    // Phase 5 slice 4: index into connected_pipe_indices() of the
    // highlighted active pipe at the avatar's node (clamped on use).
    int active_pipe = 0;

    // Phase 5 slice 2: set by a tile-targeted program's Telegraph on_confirm
    // (which runs inside game.telegraph().handle_input on a later keypress).
    // The telegraph-active branch in net_input::handle consumes+clears it to
    // decide whether that keypress committed the turn. Transient, not serialized.
    bool committed_this_key = false;

    // Phase 5 slice 3a: in-flight program queue (concurrent; one per
    // occupied slot). Renderer reads it; tick_grid advances it. Transient.
    std::vector<NetInFlight> in_flight;

    // True if deck slot `slot` currently has an in-flight program.
    bool slot_in_flight(int slot) const {
        for (const auto& f : in_flight) if (f.slot == slot) return true;
        return false;
    }

};

} // namespace astra
