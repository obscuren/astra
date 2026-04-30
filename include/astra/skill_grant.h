#pragma once

#include "astra/skill_defs.h"

namespace astra {

struct Player;
class  Game;

// Record the player as having `id` (if not already), and append it to the
// ability bar via ability_bar::assign_on_learn. Returns true if this was a
// new grant (player didn't already have it), false otherwise.
bool grant_skill(Player& player, SkillId id);

// Record the player as no longer having `id`, and remove it from the bar
// via ability_bar::remove_and_compact. Returns true if the player had it.
bool revoke_skill(Player& player, SkillId id);

// Fire any one-time side effects associated with unlocking skill `id`.
// Called from pda_screen.cpp on learn and from the :unlock-anchor dev verb.
// No-op for skills without side effects.
void apply_skill_side_effects(Game& game, SkillId id);

} // namespace astra
