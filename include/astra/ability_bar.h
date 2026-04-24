#pragma once

#include "astra/skill_defs.h"

#include <optional>
#include <vector>

namespace astra {

class Game;
struct Player;

namespace ability_bar {

inline constexpr int kSlotsPerRow    = 4;
inline constexpr int kMaxSlotsPerRow = 9;
inline constexpr int kMaxRows        = 9;
static_assert(kSlotsPerRow >= 1 && kSlotsPerRow <= kMaxSlotsPerRow,
              "kSlotsPerRow must be in [1, kMaxSlotsPerRow]");

// Read-only helpers operating on a slot list.
int  row_count(const std::vector<SkillId>& slots);   // always >= 1
std::optional<SkillId> slot_at(const std::vector<SkillId>& slots, int row, int col);

// Read-only helpers operating on a Player.
int  row_count(const Player& player);
std::optional<SkillId> slot_at(const Player& player, int row, int col);

// Mutators — defined here, no-op / minimal bodies until Phase 2 wires
// ability_slots onto Player. Once wired, assign pushes / remove erases.
bool assign_on_learn   (Player& player, SkillId id);
bool remove_and_compact(Player& player, SkillId id);

// Paging: wrap around.
void page_up  (int& visible_row, const Player& player);
void page_down(int& visible_row, const Player& player);

// Clamp a visible-row index after a learn/remove that could change row_count.
void clamp_visible_row(int& visible_row, const Player& player);

// Execute the ability in (visible_row, col) of the player's bar.
bool use_slot(Game& game, int visible_row, int col);

// Drop any slots whose SkillId the player no longer has; drop duplicates.
// Idempotent. Safe to call any time.
void validate_and_dedupe(Player& player);

// For every learned skill not currently in the bar that is ability-eligible,
// append via assign_on_learn. Idempotent. Covers the case where a grant
// site was missed in the audit.
void reconcile_from_learned(Player& player);

} // namespace ability_bar
} // namespace astra
