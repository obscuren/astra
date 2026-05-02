#include "astra/skill_grant.h"

#include "astra/ability_bar.h"
#include "astra/consciousness_save.h"
#include "astra/deep_grid_sector.h"
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
            // 1. Load (or initialise) consciousness.dat and stamp the node.
            ConsciousnessSave cs;
            read_consciousness(cs);
            if (cs.consciousness_id == 0) {
                std::random_device rd;
                cs.consciousness_id =
                    (static_cast<uint64_t>(rd()) << 32) | static_cast<uint64_t>(rd());
            }

            // 2. Stamp ownership on every existing DeepGridAnchor node in
            //    the active GridNetwork. The LAN auto-registration sweep
            //    (register_hackables_in_lan) lazy-creates an anchor with
            //    owned_by_consciousness_id = 0 on first map enter, and
            //    every connected LAN's LanRoot edges to that anchor.
            //    Adding a second "Your.Anchor" node here would orphan the
            //    LAN→anchor edge, so instead we update the existing
            //    anchor(s) in-place. If no anchor exists yet (rare —
            //    happens only if no connected LAN has been registered),
            //    create one.
            auto& net = game.world().grid_network();
            bool stamped = false;
            for (auto& n : net.nodes_mut()) {
                if (n.kind == GridNodeKind::DeepGridAnchor) {
                    n.owned_by_consciousness_id = cs.consciousness_id;
                    n.label = "Your.Anchor";
                    stamped = true;
                }
            }
            if (!stamped) {
                GridNode n;
                n.kind                      = GridNodeKind::DeepGridAnchor;
                n.label                     = "Your.Anchor";
                n.security_tier             = 1;
                n.layout_x                  = 5;
                n.layout_y                  = 5;
                n.owned_by_consciousness_id = cs.consciousness_id;
                net.add_node(n);
            }

            // 3. Persist the 60×40 hand-authored base (Plan 5 Cut 3 Task 30)
            //    into consciousness.dat. resolve_sector_for_ reads this.
            cs.deep_grid_base = make_deep_grid_base();
            write_consciousness(cs);
            break;
        }
        default:
            break;
    }
}

} // namespace astra
