#include "astra/net_combat.h"

#include "astra/effect.h"
#include "astra/game.h"
#include "astra/net_ice.h"
#include "astra/net_pipe_path.h"
#include "astra/net_session.h"
#include "astra/net_voice.h"
#include "astra/player.h"
#include "astra/program.h"
#include "astra/program_compiler.h"
#include "astra/program_effects.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

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
    // Phase 5 S3: a hostile (ICE-origin) payload resolves as AVATAR
    // damage, not ICE damage — apply_effect_in_net only hits s.ice.
    if (f.hostile) {
        std::string m = apply_effect_at_avatar(game, s, f.spec,
                                               IceColor::Gray);
        if (!m.empty())
            s.push_log("  " + f.prog_name + ": " + m);
        return;
    }
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


namespace {
// Mirrors net_combat_mode.cpp's predicate (DRY across a 1-liner is not
// worth a shared header; keep the two TU-local copies identical).
bool in_room(const NetRoom& r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}
}  // namespace

std::string apply_effect_at_avatar(Game& game, NetSession& s,
                                   const EffectSpec& spec, IceColor by) {
    if (spec.damage <= 0) return "";
    // Mirror net_ice.cpp's damage_avatar: last_killer_color is updated
    // even under Invulnerable so post-mortem telemetry stays correct.
    if (s.last_killer_color != IceColor::Black) s.last_killer_color = by;
    if (has_effect(game.player().effects, EffectId::Invulnerable))
        return "";                                  // dev invuln: no dmg, no noise

    int dmg = spec.damage;
    bool braced = false;
    if (s.brace_turns > 0) {                         // user ruling: halve, min 1
        s.brace_turns = 0;
        dmg = std::max(1, dmg / 2);
        braced = true;
    }
    s.avatar_hp -= dmg;                              // tick_grid HP<=0 check routes NonBlackDeath
    return std::string(braced ? "braced; avatar -" : "avatar -")
           + std::to_string(dmg) + ".";
}

void ice_cast_tick(NetSession& s) {
    if (s.combat_mode != NetSession::NetCombatMode::Combat) return;

    auto conn = connected_pipe_indices(s.netspace, s.avatar_x, s.avatar_y);
    for (auto& ice : s.ice) {
        if (ice.color != IceColor::Gray) continue;   // S3: Gray-only caster
        // Charm expiry: net_ice.cpp tick_all already decremented
        // charmed_turns_left this beat, so a Gray whose charm ticked to
        // 0 may cast THIS same beat. Intentional and consistent with
        // combat_should_lock (which also treats post-decrement ==0 as a
        // threat) — do not "fix" to a one-beat delay.
        if (ice.hp <= 0 || ice.charmed_turns_left != 0) continue;
        if (ice.cast_cooldown > 0) { --ice.cast_cooldown; continue; }

        for (int idx : conn) {
            auto path = pipe_path_cells(s.netspace, idx,
                                        s.avatar_x, s.avatar_y);   // avatar -> far
            if (path.empty()) continue;
            int ri = room_index_at(s.netspace,
                                   path.back().first, path.back().second);
            if (ri < 0 || ri >= static_cast<int>(s.netspace.rooms.size()))
                continue;
            const NetRoom& far = s.netspace.rooms[static_cast<size_t>(ri)];
            if (!in_room(far, ice.x, ice.y)) continue;

            // Engaged: cast ICE-node -> avatar-node (path reversed).
            std::vector<std::pair<int,int>> rpath(path.rbegin(),
                                                  path.rend());
            EffectSpec spec;
            spec.damage = kIceGrayCastDamage;
            spec.radius = kIceGrayCastRadius;

            NetInFlight f;
            f.slot           = -1;
            f.hostile        = true;
            f.compiled       = true;
            f.spec           = spec;
            f.prog_name      = "gray ICE";
            f.turns_total    = 1;
            f.turns_left     = 1;
            f.ram_held       = 0;
            f.launched       = false;
            f.pipe_path      = rpath;
            f.seg_len        = clamp_seg_len(static_cast<int>(rpath.size()));
            f.iters_total    = 1;
            f.iters_launched = 0;
            f.payloads.clear();
            f.target_x       = rpath.back().first;     // avatar-end cell
            f.target_y       = rpath.back().second;
            s.in_flight.push_back(std::move(f));
            s.push_log(astra::net_voice::sys("gray ICE: casting."));
            ice.cast_cooldown = kIceCastCadence;
            break;                                     // one cast / ICE / beat
        }
    }
}

} // namespace astra
