#pragma once

#include "astra/game_state.h"
#include "astra/grid_ice.h"
#include "astra/grid_network.h"
#include "astra/grid_sector.h"

#include <cstdint>
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

    // Tier-derived turn ticks
    int trace_tick_per_turn = 1;

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

    // Loot accumulated this session (committed on voluntary disconnect).
    GridLootBuffer loot;

    IceColor last_killer_color = IceColor::White;
};

} // namespace astra
