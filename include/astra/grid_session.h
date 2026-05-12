#pragma once

#include "astra/game_state.h"
#include "astra/grid_ice.h"
#include "astra/grid_network.h"
#include "astra/grid_sector.h"
#include "astra/netspace.h"

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace astra {

enum class JackOutKind : uint8_t {
    Voluntary,        // walked to exit node -- full loot, no penalty
    HardJackOut,      // hotkey -- Trace +10, drop 50% loot
    NonBlackDeath,    // avatar HP=0 by gray/white ICE -- body debuff, unsaved loot lost
    BlackIceDeath,    // avatar HP=0 by black ICE -- real HP damage (lethal possible)
    SoftDisconnect,   // load-time recovery -- Trace cleared, no penalty
};

struct GridLootBuffer {
    int credits             = 0;
    int code_fragments_t1   = 0;
    int code_fragments_t2   = 0;
    std::vector<uint16_t> programs_acquired;   // ProgramId values (cast)
    std::vector<std::string> lore_unlocked;
    bool empty() const;
};

struct GridSession {
    // Identity
    GridNodeId entry_node;
    GridNodeId current_node;
    GridNodeId return_node;        // mid-jack-in: previous sector's node, used
                                   // by ⊙ inside a subnet to bounce back to LAN
                                   // instead of jacking out.

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

    // Netspace (Phase 0+): the source-of-truth per-jack-in micro-dungeon.
    // The legacy `sector` below mirrors this during Phase 0 so the existing
    // renderer/input keep working; the mirror is removed in Phase 0 Step 7
    // when those layers pivot to read Netspace directly.
    Netspace netspace;

    // Sector (legacy, removed in Phase 0 Step 7)
    GridSector sector;
    std::vector<GridIce> ice;
    std::vector<bool> ice_seed_spawned;  // legacy, removed in Phase 0 Step 7

    // DaemonHijack: while active, movement keys drive s.ice[hijacked_ice_idx]
    // instead of the avatar. -1 = no active hijack. The countdown decrements
    // once per turn and clears the index when it hits 0. The ICE's own
    // charmed_turns_left independently suppresses its AI for the same window.
    int hijacked_ice_idx    = -1;
    int hijacked_turns_left = 0;

    // Loot accumulated this session (committed on voluntary disconnect).
    GridLootBuffer loot;

    IceColor last_killer_color = IceColor::White;

    // Per-session log ring. Read by the Grid HUD's right pane.
    // Capped — push_log drops the oldest entry when full.
    static constexpr size_t kLogCap = 64;
    std::deque<std::string> log_lines;

    void push_log(const std::string& line) {
        log_lines.push_back(line);
        while (log_lines.size() > kLogCap) log_lines.pop_front();
    }
    void clear_log() { log_lines.clear(); }

    // Apply incoming trace gain through the implant trace_resistance filter.
    // Negative deltas pass through unchanged (cleanses ignore resistance).
    // Returns the new clamped trace value for convenience.
    int gain_trace(int amount);

    // Plan 6: index of the slot whose Telegraph is currently open. -1 when
    // none. The Grid HUD uses this to inverse-video the active program slot.
    int active_slot = -1;

    // Spec 1: Dead-implant sector flag. When true this session was generated from
    // a corpse fixture rather than a live network node. jack_out checks this
    // to mark the corpse exhausted. corpse_fid is the fixture id (-1 = unset).
    bool is_dead_implant_transient = false;
    int  corpse_fid                = -1;

};

} // namespace astra
