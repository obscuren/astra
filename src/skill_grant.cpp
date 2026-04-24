#include "astra/skill_grant.h"

#include "astra/ability_bar.h"
#include "astra/player.h"

#include <algorithm>

namespace astra {

bool grant_skill(Player& player, SkillId id) {
    auto& ls = player.learned_skills;
    if (std::find(ls.begin(), ls.end(), id) != ls.end()) {
        return false; // already learned
    }
    ls.push_back(id);
    ability_bar::assign_on_learn(player, id);
    return true;
}

bool revoke_skill(Player& player, SkillId id) {
    auto& ls = player.learned_skills;
    auto it = std::find(ls.begin(), ls.end(), id);
    if (it == ls.end()) {
        return false; // not learned
    }
    ls.erase(it);
    ability_bar::remove_and_compact(player, id);
    return true;
}

} // namespace astra
