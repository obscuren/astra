#include "astra/net_combat.h"

#include "astra/game.h"
#include "astra/net_ice.h"
#include "astra/net_session.h"
#include "astra/program.h"
#include "astra/program_compiler.h"
#include "astra/program_effects.h"

#include <cmath>
#include <string>

namespace astra {

std::string apply_effect_in_net(Game& game, NetSession& s,
                                const EffectSpec& spec, int tx, int ty) {
    // Statuses (jitter/slag/warp) and DRAIN (returns_hp_pct) have no ICE
    // representation yet — deferred. RELAY (relay_hops) IS handled below.

    int hit   = 0;
    int kills = 0;

    if (spec.damage > 0) {
        int r = spec.radius;
        std::vector<char> struck(s.ice.size(), 0);   // RELAY: don't re-chain

        // Pass 1: primary AoE box. Mirrors the apply_pulse_hammer_grid
        // convention — damage all, resolve kills in a later pass (never
        // erase while iterating).
        for (size_t i = 0; i < s.ice.size(); ++i) {
            Ice& ice = s.ice[i];
            if (ice.hp <= 0) continue;
            int dx = std::abs(ice.x - tx);
            int dy = std::abs(ice.y - ty);
            if (dx <= r && dy <= r) {
                net_ice::damage(s, ice, spec.damage);
                struck[i] = 1;
                ++hit;
            }
        }

        // Pass 1b: RELAY chain — jump to the nearest not-yet-struck live
        // ICE within kRelayArcRange (Chebyshev) of the last hit; damage
        // falls off by relay_falloff per hop. No-op when relay_hops==0.
        if (spec.relay_hops > 0) {
            int cx = tx, cy = ty;
            float factor = 1.0f;
            for (int hop = 0; hop < spec.relay_hops; ++hop) {
                factor *= spec.relay_falloff;
                int d = static_cast<int>(spec.damage * factor + 0.5f);
                if (d <= 0) break;
                int best = -1, bestdist = kRelayArcRange + 1;
                for (size_t i = 0; i < s.ice.size(); ++i) {
                    if (struck[i] || s.ice[i].hp <= 0) continue;
                    int dx = std::abs(s.ice[i].x - cx);
                    int dy = std::abs(s.ice[i].y - cy);
                    int dd = dx > dy ? dx : dy;
                    if (dd <= kRelayArcRange && dd < bestdist) {
                        best = static_cast<int>(i); bestdist = dd;
                    }
                }
                if (best < 0) break;
                net_ice::damage(s, s.ice[best], d);
                struck[best] = 1;
                ++hit;
                cx = s.ice[best].x; cy = s.ice[best].y;
            }
        }

        // Pass 2: resolve kills + grant XP over the whole vector (catches
        // chain kills too). kill_and_persist pattern; dead ICE left in
        // s.ice for net_ice::tick_all's hp<=0 skip; no erase here.
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

void impact_resolve(Game& game, NetSession& s, NetInFlight& f) {
    // Breakwall at the far node: demote by one density step.
    if (s.netspace.breakwall_lookup.count({f.target_x, f.target_y})) {
        std::string m = demote_breakwall_at(game, s, f.target_x, f.target_y);
        if (!m.empty())
            s.push_log("  " + f.prog_name + ": " + m);
        return;
    }
    // Otherwise: 3b resolution at the far node.
    if (f.compiled) {
        std::string m = apply_effect_in_net(game, s, f.spec, f.target_x, f.target_y);
        if (!m.empty())
            s.push_log("  " + f.prog_name + ": " + m);
    } else {
        NetProgramContext ctx{game, s, f.target_x, f.target_y};
        std::string m = apply_program_in_grid(static_cast<ProgramId>(f.program_id), ctx);
        if (!m.empty())
            s.push_log("  " + m);
    }
}

} // namespace astra
