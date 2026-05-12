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
    void attack_npc(Npc& npc, Game& game, bool in_extra_hit = false);
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

// Centralized post-death book-keeping. Replaces 4 ad-hoc inlined blocks
// across attack_npc / shoot_target / process_npc_turn (DoT) /
// apply_to_anchor (Spike/Sigil). Always runs:
//  - kills counter
//  - faction reputation decay
//  - quest on_npc_killed hook
//  - XP grant + level-up check
//  - credits drop
//  - loot drop (offset from corpse tile via find_loot_drop_tile)
//  - salvage (mechanical NPC path)
//  - NpcCorpse fixture stamped at npc.x/npc.y if the NPC carried a Crystal
//
// The NPC's hp/alive flag must already be cleared by the caller.
// Erasure from the npcs vector is NOT done here — the remove_dead_npcs path
// handles that in the next game tick.
void award_npc_kill(Game& game, Npc& npc);

} // namespace astra
