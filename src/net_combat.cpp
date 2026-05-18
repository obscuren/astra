#include "astra/net_combat.h"

#include "astra/game.h"
#include "astra/net_ice.h"
#include "astra/net_session.h"
#include "astra/program_compiler.h"

#include <cmath>
#include <string>

namespace astra {

std::string apply_effect_in_net(Game& game, NetSession& s,
                                const EffectSpec& spec, int tx, int ty) {
    // Statuses (jitter/slag/warp), DRAIN (returns_hp_pct), and relay_hops
    // have no ICE representation in 3b — deferred to a later slice.

    int hit   = 0;
    int kills = 0;

    if (spec.damage > 0) {
        int r = spec.radius;

        // First pass: damage all live ICE inside the radius box. Mirrors the
        // apply_pulse_hammer_grid convention — damage all, then resolve kills
        // in a second pass (never erase while iterating).
        for (auto& ice : s.ice) {
            if (ice.hp <= 0) continue;
            int dx = std::abs(ice.x - tx);
            int dy = std::abs(ice.y - ty);
            if (dx <= r && dy <= r) {
                net_ice::damage(s, ice, spec.damage);
                ++hit;
            }
        }

        // Second pass: resolve kills and grant XP. kill_and_persist pattern
        // from program_effects.cpp — capture color before testing hp, call
        // kill_if_dead, grant XP by color tier. Dead ICE are left in s.ice
        // for net_ice::tick_all's hp<=0 skip; no erase here.
        for (auto& ice : s.ice) {
            if (ice.hp > 0) continue;
            IceColor col = ice.color;
            if (net_ice::kill_if_dead(s, ice)) {
                int xp = (col == IceColor::White) ? kXpIceWhite
                       : (col == IceColor::Gray)  ? kXpIceGray
                       :                            kXpIceBlack;
                grant_net_xp(game, xp);
                ++kills;
            }
        }
    }

    if (hit == 0)   return "no target in range.";
    if (kills == 0) return std::to_string(hit) + " hit.";
    return std::to_string(hit) + " hit, " + std::to_string(kills) + " down.";
}

} // namespace astra
