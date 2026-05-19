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
#include <climits>
#include <cmath>
#include <string>
#include <tuple>
#include <vector>

namespace astra {

std::string apply_effect_in_net(Game& game, NetSession& s,
                                const EffectSpec& spec, int tx, int ty) {
    // Statuses (jitter/slag/warp) and DRAIN (returns_hp_pct) have no ICE
    // representation yet — deferred. RELAY (relay_hops) IS handled below.

    int hit   = 0;
    int kills = 0;

    if (spec.damage > 0) {
        std::vector<std::size_t> tgt = net_node_targets(s, spec, tx, ty);
        std::vector<char> struck(s.ice.size(), 0);   // RELAY: don't re-chain
        for (std::size_t i : tgt) {
            net_ice::damage(s, s.ice[i], spec.damage);
            struck[i] = 1;
            ++hit;
        }
        // RELAY chain — origin = the single struck ICE's cell for a
        // single-target hit, else the Impact cell. Falloff/struck
        // bookkeeping unchanged from the shipped behaviour.
        if (spec.relay_hops > 0) {
            int cx = tx, cy = ty;
            if (spec.radius < 1 && tgt.size() == 1) {
                cx = s.ice[tgt[0]].x; cy = s.ice[tgt[0]].y;
            }
            float factor = 1.0f;
            for (int hop = 0; hop < spec.relay_hops; ++hop) {
                factor *= spec.relay_falloff;
                int d = static_cast<int>(spec.damage * factor + 0.5f);
                if (d <= 0) break;
                int best = -1, bestdist = kRelayArcRange + 1;
                for (std::size_t i = 0; i < s.ice.size(); ++i) {
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
        // Pass 2: resolve kills + grant XP (unchanged).
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

std::vector<std::size_t> net_node_targets(const NetSession& s,
                                          const EffectSpec& spec,
                                          int tx, int ty) {
    std::vector<std::size_t> out;
    if (spec.damage <= 0) return out;
    int ri = room_index_at(s.netspace, tx, ty);
    if (ri < 0 || ri >= static_cast<int>(s.netspace.rooms.size())) {
        // Defensive: legacy Chebyshev box so S4 never regresses if the
        // room lookup somehow fails (shouldn't in a real netspace).
        const int r = spec.radius;
        for (std::size_t i = 0; i < s.ice.size(); ++i) {
            if (s.ice[i].hp <= 0) continue;
            if (std::abs(s.ice[i].x - tx) <= r &&
                std::abs(s.ice[i].y - ty) <= r) out.push_back(i);
        }
        return out;
    }
    const NetRoom& room = s.netspace.rooms[static_cast<std::size_t>(ri)];
    if (spec.radius >= 1) {                       // AOE: all live ICE in node
        for (std::size_t i = 0; i < s.ice.size(); ++i)
            if (s.ice[i].hp > 0 && in_room(room, s.ice[i].x, s.ice[i].y))
                out.push_back(i);
        return out;
    }
    // Single-target: closest live ICE in the node to the Impact cell
    // (Chebyshev), tiebreak by ICE index for determinism.
    int best = -1, bestd = INT_MAX;
    for (std::size_t i = 0; i < s.ice.size(); ++i) {
        if (s.ice[i].hp <= 0) continue;
        if (!in_room(room, s.ice[i].x, s.ice[i].y)) continue;
        int dx = std::abs(s.ice[i].x - tx);
        int dy = std::abs(s.ice[i].y - ty);
        int dd = dx > dy ? dx : dy;
        if (dd < bestd) { bestd = dd; best = static_cast<int>(i); }
    }
    if (best >= 0) out.push_back(static_cast<std::size_t>(best));
    return out;
}

void net_inflight_advance(NetSession& s) {
    for (auto& f : s.in_flight) {
        if (f.pipe_path.empty()) continue;          // legacy/self: impacts phase
        if (!f.launched) { f.launched = true; continue; }   // launch beat
        if (f.iters_launched < f.iters_total) {
            f.payloads.push_back(0);
            ++f.iters_launched;
        }
        for (int& seg : f.payloads) ++seg;
    }
}

namespace {
// Distance of payload `seg` from the avatar end of its pipe. Player
// travels avatar->far (u=seg); ICE travels far->avatar on the reversed
// path (u=seg_len-seg). Common coordinate on a shared pipe.
int payload_u(const NetInFlight& f, int seg) {
    return f.hostile ? (f.seg_len - seg) : seg;
}

// payload<->payload contact (the S5 Black seam: a payload<->Black
// branch is added here). Equal damage annihilates both; else the loser
// payload is removed and the winner entry carries the damage
// difference. Mutates payloads + spec.damage only. Terse log line.
std::string resolve_pipe_contact(NetInFlight& a, std::size_t ai,
                                  NetInFlight& b, std::size_t bi) {
    const int ad = a.spec.damage;
    const int bd = b.spec.damage;
    if (ad == bd) {
        a.payloads.erase(a.payloads.begin() + static_cast<long>(ai));
        b.payloads.erase(b.payloads.begin() + static_cast<long>(bi));
        return "pipe collision: payloads annihilate.";
    }
    if (ad > bd) {
        a.spec.damage = ad - bd;
        b.payloads.erase(b.payloads.begin() + static_cast<long>(bi));
    } else {
        b.spec.damage = bd - ad;
        a.payloads.erase(a.payloads.begin() + static_cast<long>(ai));
    }
    return "pipe collision: payload survives ("
         + std::to_string(ad > bd ? ad - bd : bd - ad) + ").";
}
}  // namespace

void resolve_pipe_collisions(NetSession& s) {
    // Repeat until no contact remains this beat (a carrying winner can
    // meet the next opponent). Pipes are short and payload counts small,
    // so the full rescan after each resolve is cheap and makes
    // erase-invalidation a non-issue.
    for (;;) {
        bool found = false;
        std::tuple<int,int,std::size_t,std::size_t,std::size_t,std::size_t>
            best_key;
        std::size_t bp_e = 0, bp_p = 0, bi_e = 0, bi_p = 0;
        for (std::size_t pe = 0; pe < s.in_flight.size(); ++pe) {
            NetInFlight& P = s.in_flight[pe];
            if (P.hostile || P.pipe_index < 0 || P.pipe_path.empty())
                continue;
            for (std::size_t pp = 0; pp < P.payloads.size(); ++pp) {
                const int uP = payload_u(P, P.payloads[pp]);
                for (std::size_t ie = 0; ie < s.in_flight.size(); ++ie) {
                    NetInFlight& I = s.in_flight[ie];
                    if (!I.hostile || I.pipe_index != P.pipe_index ||
                        I.pipe_path.empty())
                        continue;
                    for (std::size_t ip = 0; ip < I.payloads.size(); ++ip) {
                        const int uI = payload_u(I, I.payloads[ip]);
                        const int diff = uP - uI;
                        if (diff < 0 || diff > 1) continue;   // not in contact
                        auto key = std::make_tuple(P.pipe_index, uI,
                                                   pe, pp, ie, ip);
                        if (!found || key < best_key) {
                            found    = true;
                            best_key = key;
                            bp_e = pe; bp_p = pp; bi_e = ie; bi_p = ip;
                        }
                    }
                }
            }
        }
        if (!found) break;
        std::string m = resolve_pipe_contact(s.in_flight[bp_e], bp_p,
                                             s.in_flight[bi_e], bi_p);
        s.push_log(astra::net_voice::sys(m));
    }
}

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
            f.pipe_index     = idx;
            s.in_flight.push_back(std::move(f));
            s.push_log(astra::net_voice::sys("gray ICE: casting."));
            ice.cast_cooldown = kIceCastCadence;
            break;                                     // one cast / ICE / beat
        }
    }
}

void resolve_inflight_impacts(Game& game, NetSession& s) {
    for (auto it = s.in_flight.begin(); it != s.in_flight.end(); ) {
        if (it->pipe_path.empty()) {
            // Legacy non-travel / self entry — exact pre-Slice-4 3b
            // behaviour (unchanged).
            if (!it->launched) { it->launched = true; ++it; continue; }
            if (it->compiled) {
                std::string msg = apply_effect_in_net(game, s, it->spec,
                                                      it->target_x,
                                                      it->target_y);
                if (!msg.empty())
                    s.push_log("  " + it->prog_name + ": " + msg);
            } else {
                NetProgramContext ctx{game, s, it->target_x, it->target_y};
                std::string msg = apply_program_in_grid(
                    static_cast<ProgramId>(it->program_id), ctx);
                if (!msg.empty()) s.push_log(std::string("  ") + msg);
            }
            if (--it->turns_left > 0) { ++it; continue; }
            s.ram = std::min(s.ram_max, s.ram + it->ram_held);
            it = s.in_flight.erase(it); continue;
        }
        // Slice-4 payload travel: Impact survivors at/over seg_len.
        // (launch beat was consumed in net_inflight_advance; this guard
        // is a defensive no-op.)
        if (!it->launched) { ++it; continue; }
        for (std::size_t k = 0; k < it->payloads.size(); ) {
            if (it->payloads[k] >= it->seg_len) {
                impact_resolve(game, s, *it);
                it->payloads.erase(it->payloads.begin()
                                   + static_cast<long>(k));
            } else ++k;
        }
        if (it->iters_launched >= it->iters_total && it->payloads.empty()) {
            s.ram = std::min(s.ram_max, s.ram + it->ram_held);
            it = s.in_flight.erase(it);
        } else ++it;
    }
}

} // namespace astra
