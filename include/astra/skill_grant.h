#pragma once

#include "astra/skill_defs.h"

namespace astra {

struct Player;

// Record the player as having `id` (if not already), and append it to the
// ability bar via ability_bar::assign_on_learn. Returns true if this was a
// new grant (player didn't already have it), false otherwise.
bool grant_skill(Player& player, SkillId id);

// Record the player as no longer having `id`, and remove it from the bar
// via ability_bar::remove_and_compact. Returns true if the player had it.
bool revoke_skill(Player& player, SkillId id);

} // namespace astra
