#pragma once

#include "astra/anchor.h"
#include "astra/game_state.h"
#include "astra/grid_ice.h"
#include "astra/grid_network.h"
#include "astra/grid_sector.h"

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

    // Sector
    GridSector sector;
    std::vector<GridIce> ice;
    std::vector<bool> ice_seed_spawned;  // Plan 8: true once seed_idx has materialized

    // Anchors (player-facing: Anchors)
    std::vector<Imprint>&       imprints()       { return imprints_; }
    const std::vector<Imprint>& imprints() const { return imprints_; }
    Imprint* imprint_for_npc(int npc_id);
    Imprint* imprint_at(int x, int y);
    void  clear_imprints() { imprints_.clear(); next_imprint_id_ = 0; }
    Imprint* add_imprint_for_npc(int npc_id, int sx, int sy,
                               int npc_threat_tier, bool bound = false);

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

    // Plan 6: index of the slot whose Telegraph is currently open. -1 when
    // none. The Grid HUD uses this to inverse-video the active program slot.
    int active_slot = -1;

    // Spec 1: Dead-implant sector flag. When true this session was generated from
    // a corpse fixture rather than a live network node. jack_out checks this
    // to mark the corpse exhausted. corpse_fid is the fixture id (-1 = unset).
    bool is_dead_implant_transient = false;
    int  corpse_fid                = -1;

private:
    std::vector<Imprint> imprints_;
    int32_t             next_imprint_id_ = 0;
};

} // namespace astra
