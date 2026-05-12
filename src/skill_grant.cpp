#include "astra/skill_grant.h"

#include "astra/ability_bar.h"
#include "astra/consciousness_save.h"
#include "astra/fragment.h"
#include "astra/game.h"
#include "astra/grid_sector.h"
#include "astra/player.h"

#include <algorithm>
#include <random>

namespace astra {

namespace {

bool fragment_is_known(const Player& p, FragmentId id) {
    for (auto f : p.learned_fragments) if (f == id) return true;
    return false;
}

void grant_fragment(Player& p, FragmentId id) {
    if (!fragment_is_known(p, id)) p.learned_fragments.push_back(id);
}

void roll_random_fragments(Player& p, int n) {
    std::vector<FragmentId> pool;
    for (const auto& def : fragment_catalog()) {
        if (def.id == FragmentId::None) continue;
        if (!fragment_is_known(p, def.id)) pool.push_back(def.id);
    }
    if (pool.empty()) return;
    std::mt19937 rng{std::random_device{}()};
    std::shuffle(pool.begin(), pool.end(), rng);
    for (int i = 0; i < n && i < static_cast<int>(pool.size()); ++i) {
        grant_fragment(p, pool[i]);
    }
}

}  // namespace

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

void apply_skill_side_effects(Game& game, SkillId id) {
    switch (id) {
        case SkillId::ConsciousnessAnchor: {
            // ConsciousnessAnchor capstone now records only the consciousness
            // id; the multi-region "Your.Anchor" node and the deep-grid base
            // sector both retired with the netspace redesign.
            ConsciousnessSave cs;
            read_consciousness(cs);
            if (cs.consciousness_id == 0) {
                std::random_device rd;
                cs.consciousness_id =
                    (static_cast<uint64_t>(rd()) << 32) | static_cast<uint64_t>(rd());
            }
            write_consciousness(cs);
            break;
        }
        case SkillId::Programming1:
            // Default starter grant: 1 producer (VOLT) + RELAY + AMPLIFY.
            // Player-pick UI for the producer is a follow-up; for now grant a
            // safe trio so testability flows.
            grant_fragment(game.player(), FragmentId::Volt);
            grant_fragment(game.player(), FragmentId::Relay);
            grant_fragment(game.player(), FragmentId::Amplify);
            game.log("Programming I learned. Compiler unlocked. Starter fragments: VOLT, RELAY, AMPLIFY.");
            break;
        case SkillId::Programming2:
            roll_random_fragments(game.player(), 2);
            game.log("Programming II learned. 2 random fragments rolled. Program ceiling now 4.");
            break;
        case SkillId::Programming3:
            roll_random_fragments(game.player(), 2);
            game.log("Programming III learned. 2 random fragments rolled. Program ceiling now 5.");
            break;
        default:
            break;
    }
}

} // namespace astra
