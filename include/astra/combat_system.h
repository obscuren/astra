#pragma once

#include "astra/npc.h"

#include <cstdint>

namespace astra {

class Game; // forward declare
struct EnergyStore; // forward declare
struct Item; // forward declare

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
    void attack_npc_vs_npc(Npc& attacker, Npc& defender, Game& game);
    void process_npc_turn(Npc& npc, Game& game);
    void begin_targeting(Game& game);
    void handle_targeting_input(int key, Game& game);
    void shoot_target(Game& game);
    bool recharge_weapon(Game& game, bool log_full = true, bool advance = true);
    bool recharge_shield(Game& game, bool log_full = true, bool advance = true);
    void remove_dead_npcs(Game& game);
    void check_level_up(Game& game);

    void reset();

    // Recharge target type — used to gate cell proc effects.
    enum class RechargeTargetKind : uint8_t {
        Generic = 0,         // a non-equipped inventory item
        EquippedWeapon = 1,
        EquippedShield = 2,
    };

private:
    // Drain cells from inventory (highest-charge first) into target until full.
    // Returns total energy deposited.
    int recharge_target_(Game& game, EnergyStore& target, RechargeTargetKind kind);

    bool targeting_ = false;
    int target_x_ = 0;
    int target_y_ = 0;
    int blink_phase_ = 0;
    Npc* target_npc_ = nullptr;
};

// Fire any cell proc owned by `cell` after `drained` units flowed out of it.
// Side-effects only: may modify `target` (overcharge), or add a player effect.
void apply_cell_proc(Item& cell, int drained,
                     CombatSystem::RechargeTargetKind kind,
                     EnergyStore* target, Game& game);

} // namespace astra
