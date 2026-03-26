#pragma once

#include "astra/npc.h"

namespace astra {

class Game; // forward declare

class CombatSystem {
public:
    CombatSystem() = default;

    // Targeting state
    bool targeting() const { return targeting_; }
    int target_x() const { return target_x_; }
    int target_y() const { return target_y_; }
    int blink_phase() const { return blink_phase_; }
    Npc* target_npc() const { return target_npc_; }
    void tick_blink() { ++blink_phase_; }

    // Actions
    void attack_npc(Npc& npc, Game& game);
    void process_npc_turn(Npc& npc, Game& game);
    void begin_targeting(Game& game);
    void handle_targeting_input(int key, Game& game);
    void shoot_target(Game& game);
    void reload_weapon(Game& game);
    void remove_dead_npcs(Game& game);
    void check_level_up(Game& game);

    void reset();

private:
    bool targeting_ = false;
    int target_x_ = 0;
    int target_y_ = 0;
    int blink_phase_ = 0;
    Npc* target_npc_ = nullptr;
};

} // namespace astra
