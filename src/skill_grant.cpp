#include "astra/skill_grant.h"

#include "astra/ability_bar.h"
#include "astra/consciousness_save.h"
#include "astra/game.h"
#include "astra/grid_sector.h"
#include "astra/player.h"

#include <algorithm>
#include <random>

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

void apply_skill_side_effects(Game& game, SkillId id) {
    switch (id) {
        case SkillId::ConsciousnessAnchor: {
            // 1. Generate the player's persistent base sector.
            auto base = make_player_deep_grid_base();

            // 2. Register a DeepGridAnchor node in the active GridNetwork.
            GridNode n;
            n.kind          = GridNodeKind::DeepGridAnchor;
            n.label         = "Your.Anchor";
            n.security_tier = 1;
            n.layout_x      = 5;
            n.layout_y      = 5;

            // 3. Load (or initialise) consciousness.dat and stamp the node.
            ConsciousnessSave cs;
            read_consciousness(cs);
            if (cs.consciousness_id == 0) {
                std::random_device rd;
                cs.consciousness_id =
                    (static_cast<uint64_t>(rd()) << 32) | static_cast<uint64_t>(rd());
            }
            n.owned_by_consciousness_id = cs.consciousness_id;
            game.world().grid_network().add_node(n);

            // 4. Persist the base into consciousness.dat.
            cs.deep_grid_base = std::move(base);
            write_consciousness(cs);
            break;
        }
        default:
            break;
    }
}

} // namespace astra
