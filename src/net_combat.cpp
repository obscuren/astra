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

// Map a walker's abstract segment index (walk_seg) to a physical cell
// index in walk_path. Linear interpolation so a clamped-seg_len pipe
// (walk_path.size() > walk_seg_len) covers the whole physical pipe in
// walk_seg_len beats (Black "skips" cells, symmetric with how player
// payloads on long pipes impact after seg_len beats regardless of
// physical length). Identity on unclamped pipes (N == walk_seg_len).
int walker_cell_idx(int walk_seg, int walk_seg_len, int N) {
    if (walk_seg_len <= 1 || N <= 1) return 0;
    int idx = (walk_seg - 1) * (N - 1) / (walk_seg_len - 1);
    if (idx < 0) idx = 0;
    if (idx >= N) idx = N - 1;
    return idx;
}

// Phase 5 S6: predicate for core_action_run's edge-trigger halt. True
// if any live, un-charmed Black walker is one pipe-hop (or less) from
// the avatar's room. "Adjacent" = (a) sitting in the avatar's room
// already; (b) in-transit on a pipe that terminates in the avatar's
// room and has fully traversed it; (c) at a node whose next-hop pipe
// (per pipe_graph_next_hop) leads into the avatar's room. Game-free.
bool any_black_one_hop(const NetSession& s) {
    const int avatar_room =
        room_index_at(s.netspace, s.avatar_x, s.avatar_y);
    if (avatar_room < 0) return false;
    for (const auto& blk : s.ice) {
        if (blk.color != IceColor::Black) continue;
        if (blk.hp <= 0 || blk.charmed_turns_left != 0) continue;
        // Already in the avatar's room? That's the GameOver beat
        // (handled by black_walker_tick already); treat as "adjacent"
        // for the RUN interrupt so RUN halts before consuming it.
        int blk_room = (blk.walk_pipe_index >= 0)
            ? -1
            : room_index_at(s.netspace, blk.x, blk.y);
        if (blk_room == avatar_room) return true;
        // In-transit: if its current pipe terminates in the avatar's
        // room AND walker has reached (or passed) the arrival threshold,
        // it's adjacent. Threshold = walk_seg_len (the clamped beat
        // count), NOT walk_path.size() -- on clamped pipes the walker
        // covers walk_path in walk_seg_len beats via walker_cell_idx,
        // and arrival fires at walk_seg > walk_seg_len. Using
        // walk_path.size() would never fire on a clamped pipe.
        if (blk.walk_pipe_index >= 0 && !blk.walk_path.empty()) {
            const auto& last = blk.walk_path.back();
            int end_room = room_index_at(s.netspace, last.first, last.second);
            if (end_room == avatar_room &&
                blk.walk_seg >= blk.walk_seg_len)
                return true;
        }
        // At a node: next-hop pipe's far room == avatar's room?
        if (blk.walk_pipe_index < 0 && blk_room >= 0) {
            int hop = pipe_graph_next_hop(s.netspace, blk_room, avatar_room);
            if (hop < 0) continue;
            int other = -1;
            const auto& p =
                s.netspace.pipes[static_cast<std::size_t>(hop)];
            int ra = room_index_at(s.netspace, p.x0, p.y0);
            int rb = room_index_at(s.netspace, p.x1, p.y1);
            other = (ra == blk_room) ? rb : ra;
            if (other == avatar_room) return true;
        }
    }
    return false;
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

int pipe_graph_next_hop(const Netspace& ns, int from_room, int to_room) {
    const int N = static_cast<int>(ns.rooms.size());
    if (from_room < 0 || to_room < 0 ||
        from_room >= N || to_room >= N ||
        from_room == to_room) return -1;

    // Build adjacency from netspace.pipes endpoints. Each pipe connects
    // the rooms containing its (x0,y0) and (x1,y1) endpoints.
    struct Edge { int a; int b; int pipe; };
    std::vector<Edge> edges;
    edges.reserve(ns.pipes.size());
    for (std::size_t i = 0; i < ns.pipes.size(); ++i) {
        const auto& p = ns.pipes[i];
        int ra = room_index_at(ns, p.x0, p.y0);
        int rb = room_index_at(ns, p.x1, p.y1);
        if (ra < 0 || rb < 0 || ra == rb) continue;
        edges.push_back({ra, rb, static_cast<int>(i)});
    }

    // BFS from from_room. parent_pipe[r] = the pipe used to enter r;
    // parent_room[r] = the room we entered r from.
    std::vector<int>  parent_room(static_cast<std::size_t>(N), -1);
    std::vector<int>  parent_pipe(static_cast<std::size_t>(N), -1);
    std::vector<char> seen(static_cast<std::size_t>(N), 0);
    std::vector<int>  q;
    q.push_back(from_room);
    seen[static_cast<std::size_t>(from_room)] = 1;
    bool found = false;
    for (std::size_t qi = 0; qi < q.size() && !found; ++qi) {
        int cur = q[qi];
        // Deterministic neighbor order: edges iterated in pipes-index
        // order (the edges vector preserves it).
        for (const auto& e : edges) {
            int nb = (e.a == cur) ? e.b : (e.b == cur) ? e.a : -1;
            if (nb < 0 || seen[static_cast<std::size_t>(nb)]) continue;
            seen[static_cast<std::size_t>(nb)]        = 1;
            parent_room[static_cast<std::size_t>(nb)] = cur;
            parent_pipe[static_cast<std::size_t>(nb)] = e.pipe;
            if (nb == to_room) { found = true; break; }
            q.push_back(nb);
        }
    }
    if (!found) return -1;

    // Walk parents from to_room back until the parent IS from_room.
    int cur = to_room;
    while (parent_room[static_cast<std::size_t>(cur)] != from_room) {
        cur = parent_room[static_cast<std::size_t>(cur)];
        if (cur < 0) return -1;
    }
    return parent_pipe[static_cast<std::size_t>(cur)];
}

void black_walker_tick(NetSession& s) {
    const int avatar_room =
        room_index_at(s.netspace, s.avatar_x, s.avatar_y);

    for (auto& ice : s.ice) {
        if (ice.color != IceColor::Black) continue;
        if (ice.hp <= 0 || ice.charmed_turns_left != 0) continue;

        // (I2) Stale-walk-state detection: a DaemonHijack may have moved
        // ice.x/ice.y off the interpolated walker position. Reset to at-
        // node so we BFS afresh from the current cell this beat.
        if (ice.walk_pipe_index >= 0 && ice.walk_seg >= 1 &&
            ice.walk_seg <= ice.walk_seg_len) {
            const int N   = static_cast<int>(ice.walk_path.size());
            const int idx = walker_cell_idx(ice.walk_seg,
                                            ice.walk_seg_len, N);
            const auto& expected =
                ice.walk_path[static_cast<std::size_t>(idx)];
            if (ice.x != expected.first || ice.y != expected.second) {
                ice.walk_pipe_index = -1;
                ice.walk_seg        = 0;
                ice.walk_seg_len    = 0;
                ice.walk_path.clear();
                // Falls into the at-node section below.
            }
        }

        // (1) In-pipe: advance one cell.
        if (ice.walk_pipe_index >= 0) {
            // Phase 5 S5 fix (I1): step over [1, walk_seg_len] not
            // [1, walk_path.size()]; on long pipes where
            // clamp_seg_len(walk_path.size()) < walk_path.size(),
            // map walk_seg -> physical cell via linear interpolation.
            // Keeps uB = walk_seg_len - walk_seg in [0, walk_seg_len-1]
            // for the entire in-pipe traversal so collision works on
            // long pipes (mechanics.md promise upheld).
            ++ice.walk_seg;
            if (ice.walk_seg <= ice.walk_seg_len) {
                const int N       = static_cast<int>(ice.walk_path.size());
                const int cell_idx = walker_cell_idx(ice.walk_seg,
                                                     ice.walk_seg_len, N);
                const auto& c = ice.walk_path[static_cast<std::size_t>(
                    cell_idx)];
                ice.x = c.first;
                ice.y = c.second;
                continue;     // still in pipe; collision pass handles
            }
            // Arrived at the far end: snap into the next room, reset.
            if (!ice.walk_path.empty()) {
                ice.x = ice.walk_path.back().first;
                ice.y = ice.walk_path.back().second;
            }
            ice.walk_pipe_index = -1;
            ice.walk_seg        = 0;
            ice.walk_seg_len    = 0;
            ice.walk_path.clear();
            int my_room =
                room_index_at(s.netspace, ice.x, ice.y);
            if (my_room == avatar_room && avatar_room >= 0) {
                s.black_reached_player_node = true;
                return;       // stop processing further ICE this beat
            }
            // Otherwise fall through: pick next hop THIS beat (Black
            // doesn't idle on arrival -- the boss clock keeps ticking).
        }

        // (2) At a node: pick next-hop pipe toward the avatar's room.
        int my_room =
            room_index_at(s.netspace, ice.x, ice.y);
        if (my_room < 0 || avatar_room < 0) continue;
        if (my_room == avatar_room) {
            // Already in the avatar's room (e.g. spawned there).
            s.black_reached_player_node = true;
            return;
        }
        int hop = pipe_graph_next_hop(s.netspace, my_room, avatar_room);
        if (hop < 0) continue;          // stranded -- idle this beat
        ice.walk_pipe_index = hop;
        ice.walk_path = pipe_path_cells(s.netspace, hop, ice.x, ice.y);
        if (ice.walk_path.empty()) {
            ice.walk_pipe_index = -1;
            continue;
        }
        ice.walk_seg_len = clamp_seg_len(
            static_cast<int>(ice.walk_path.size()));
        ice.walk_seg = 1;
        const auto& c0 = ice.walk_path[0];
        ice.x = c0.first;
        ice.y = c0.second;
    }
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
        // Phase 5 S5: if no payload<->payload contact this iteration,
        // try payload<->Black (the S4-named resolve_pipe_contact seam
        // built here). A Black walker with walk_pipe_index>=0 occupies
        // walk_path[walk_seg-1] in segment-space; its u-from-avatar-end
        // = walk_seg_len - walk_seg (mirrors a hostile payload's u).
        if (!found) {
            int      best_pipe_b = INT_MAX;
            int      best_ub     = INT_MAX;
            std::size_t bp_e_b = 0, bp_p_b = 0, bi_b = 0;
            bool     found_b = false;
            for (std::size_t pe = 0; pe < s.in_flight.size(); ++pe) {
                NetInFlight& P = s.in_flight[pe];
                if (P.hostile || P.pipe_index < 0 ||
                    P.pipe_path.empty()) continue;
                for (std::size_t pp = 0; pp < P.payloads.size(); ++pp) {
                    const int uP = payload_u(P, P.payloads[pp]);
                    for (std::size_t bi = 0; bi < s.ice.size(); ++bi) {
                        const Ice& B = s.ice[bi];
                        if (B.color != IceColor::Black) continue;
                        if (B.hp <= 0 || B.killed) continue;
                        if (B.walk_pipe_index != P.pipe_index) continue;
                        const int uB = B.walk_seg_len - B.walk_seg;
                        const int diff = uP - uB;
                        if (diff < 0 || diff > 1) continue;
                        if (!found_b ||
                            P.pipe_index < best_pipe_b ||
                            (P.pipe_index == best_pipe_b &&
                             uB < best_ub)) {
                            found_b      = true;
                            best_pipe_b  = P.pipe_index;
                            best_ub      = uB;
                            bp_e_b = pe; bp_p_b = pp; bi_b = bi;
                        }
                    }
                }
            }
            if (found_b) {
                NetInFlight& P = s.in_flight[bp_e_b];
                Ice&         B = s.ice[bi_b];
                const int dmg = P.spec.damage;
                B.hp -= dmg;
                P.payloads.erase(P.payloads.begin()
                                 + static_cast<long>(bp_p_b));
                s.push_log(astra::net_voice::sys(
                    "pipe collision: black ICE struck ("
                    + std::to_string(dmg) + ")."));
                continue;     // rescan from the top
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
        // 0 may proceed THIS same beat. Consistent with S3.
        if (ice.hp <= 0 || ice.charmed_turns_left != 0) {
            // Charmed/dead Gray: cancel any in-progress windup so the
            // telegraph doesn't lie about an inert ICE.
            ice.cast_windup_left  = 0;
            ice.cast_windup_total = 0;
            continue;
        }

        // (A) Windup in progress: tick down, spawn at zero.
        if (ice.cast_windup_left > 0) {
            --ice.cast_windup_left;
            if (ice.cast_windup_left > 0) continue;
            // Windup just hit 0 -- spawn the telegraphed payload now.
            ice.cast_windup_total = 0;
            for (int idx : conn) {
                auto path = pipe_path_cells(s.netspace, idx,
                                            s.avatar_x, s.avatar_y);
                if (path.empty()) continue;
                int ri = room_index_at(s.netspace,
                                       path.back().first, path.back().second);
                if (ri < 0 || ri >= static_cast<int>(s.netspace.rooms.size()))
                    continue;
                const NetRoom& far =
                    s.netspace.rooms[static_cast<std::size_t>(ri)];
                if (!in_room(far, ice.x, ice.y)) continue;
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
                f.target_x       = rpath.back().first;
                f.target_y       = rpath.back().second;
                f.pipe_index     = idx;
                f.source_ice_idx = static_cast<int>(&ice - s.ice.data());
                s.in_flight.push_back(std::move(f));
                s.push_log(astra::net_voice::sys("gray ICE: fires."));
                ice.cast_cooldown = kIceCastCadence;
                break;   // one cast / ICE / beat
            }
            continue;
        }

        // (B) At rest: tick cooldown; if it's expired AND we have a
        // valid engagement (a connected pipe whose far room contains
        // this ICE), START a windup. The engagement check is the same
        // predicate the legacy launch used -- we're just deferring the
        // payload spawn by kIceGrayWindupBeats so the player has a
        // visible window to react.
        if (ice.cast_cooldown > 0) { --ice.cast_cooldown; continue; }

        bool engaged = false;
        for (int idx : conn) {
            auto path = pipe_path_cells(s.netspace, idx,
                                        s.avatar_x, s.avatar_y);
            if (path.empty()) continue;
            int ri = room_index_at(s.netspace,
                                   path.back().first, path.back().second);
            if (ri < 0 || ri >= static_cast<int>(s.netspace.rooms.size()))
                continue;
            const NetRoom& far =
                s.netspace.rooms[static_cast<std::size_t>(ri)];
            if (in_room(far, ice.x, ice.y)) { engaged = true; break; }
        }
        if (!engaged) continue;

        ice.cast_windup_left  = kIceGrayWindupBeats;
        ice.cast_windup_total = kIceGrayWindupBeats;
        s.push_log(astra::net_voice::sys("gray ICE: charging."));
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

    // Phase 5 S5: any ICE reduced to hp<=0 by collision in
    // resolve_pipe_collisions this beat did NOT go through
    // apply_effect_in_net's Pass-2 (collisions don't call that). Sweep
    // here so collision-killed Black (and any future collision-kill
    // case) grants XP + trace exactly once. kill_if_dead is idempotent
    // via Ice::killed, so this is safe to call after Pass-2 has
    // already processed the same ICE.
    for (auto& ice : s.ice) {
        if (ice.hp > 0 || ice.killed) continue;
        IceColor col = ice.color;
        if (net_ice::kill_if_dead(s, ice)) {
            int xp = (col == IceColor::White) ? kXpIceWhite
                   : (col == IceColor::Gray)  ? kXpIceGray
                   :                            kXpIceBlack;
            grant_net_xp(game, xp);
        }
    }
}

void core_action_run(Game& game) {
    NetSession* sp = game.hacking().session();   // may be null pre-jack-in
    if (!sp) return;
    NetSession& s = *sp;
    if (s.run_active) return;                    // re-entrancy guard

    s.run_active = true;
    s.push_log(astra::net_voice::cmd("autopilot engaged."));

    bool pre_adjacent = any_black_one_hop(s);
    const char* reason = "cap";                   // safety default
    for (int i = 0; i < kRunAutopilotCap; ++i) {
        game.hacking().tick_grid(game);
        if (game.state() != GameState::Playing) {
            reason = "session ended";
            break;
        }
        // Session may have been reset by jack_out within tick_grid;
        // re-fetch to avoid use-after-reset.
        NetSession* re = game.hacking().session();
        if (!re) { reason = "session ended"; break; }
        NetSession& sn = *re;
        bool now_adjacent = any_black_one_hop(sn);
        if (!pre_adjacent && now_adjacent) { reason = "black adjacent"; break; }
        pre_adjacent = now_adjacent;
    }

    NetSession* finals = game.hacking().session();
    if (finals) {
        finals->run_active = false;
        finals->push_log(astra::net_voice::cmd(
            std::string("autopilot halted: ") + reason + "."));
    }
}

} // namespace astra
