#include "astra/net_ice.h"

#include "astra/effect.h"
#include "astra/game.h"
#include "astra/net_constants.h"
#include "astra/net_display.h"
#include "astra/net_pipe_path.h"
#include "astra/net_session.h"
#include "astra/player.h"

#include <algorithm>
#include <cstdlib>
#include <random>

namespace astra {

namespace net_ice {

namespace {

bool place_random(NetSession& s, std::mt19937& rng,
                  int& out_x, int& out_y) {
    std::uniform_int_distribution<int> xd(0, s.netspace.w - 1);
    std::uniform_int_distribution<int> yd(0, s.netspace.h - 1);
    for (int tries = 0; tries < 64; ++tries) {
        int x = xd(rng);
        int y = yd(rng);
        if (!s.netspace.passable(x, y)) continue;
        if (x == s.avatar_x && y == s.avatar_y) continue;
        bool occupied = false;
        for (auto& i : s.ice) if (i.x == x && i.y == y) { occupied = true; break; }
        if (occupied) continue;
        out_x = x; out_y = y;
        return true;
    }
    return false;
}

int manhattan(int ax, int ay, int bx, int by) {
    return std::abs(ax - bx) + std::abs(ay - by);
}

[[maybe_unused]] void step_toward(NetSession& s, Ice& ice, int tx, int ty) {
    int best_dx = 0, best_dy = 0;
    int best_d = manhattan(ice.x, ice.y, tx, ty);
    static const int dx[4] = { 0, 0, -1, 1 };
    static const int dy[4] = { -1, 1,  0, 0 };
    for (int d = 0; d < 4; ++d) {
        int nx = ice.x + dx[d];
        int ny = ice.y + dy[d];
        if (!s.netspace.passable(nx, ny)) continue;
        if (nx == s.avatar_x && ny == s.avatar_y) continue;
        bool occupied = false;
        for (auto& other : s.ice) {
            if (&other == &ice) continue;
            if (other.x == nx && other.y == ny) { occupied = true; break; }
        }
        if (occupied) continue;
        int nd = manhattan(nx, ny, tx, ty);
        if (nd < best_d) {
            best_d = nd;
            best_dx = dx[d];
            best_dy = dy[d];
        }
    }
    ice.x += best_dx;
    ice.y += best_dy;
}

} // namespace

void spawn_for_sector(NetSession& s, uint32_t seed, int security_tier) {
    std::mt19937 rng(seed ^ 0xDECAFC0Du);
    int n_white = 1;
    int n_gray  = security_tier >= 2 ? 1 : 0;

    for (int i = 0; i < n_white; ++i) {
        int x, y; if (!place_random(s, rng, x, y)) break;
        Ice ice;
        ice.x = x; ice.y = y;
        ice.color = IceColor::White;
        ice.hp = 1;
        std::uniform_int_distribution<int> dir_d(0, 3);
        ice.patrol_dir = dir_d(rng);
        s.ice.push_back(ice);
    }
    for (int i = 0; i < n_gray; ++i) {
        int x, y; if (!place_random(s, rng, x, y)) break;
        Ice ice;
        ice.x = x; ice.y = y;
        ice.color = IceColor::Gray;
        ice.hp = 2;
        s.ice.push_back(ice);
    }
}

void spawn_from_seeds(NetSession&) {
    // ICE seed pipeline retired with the legacy sector generators.
    // Per-target netspace grammars (Phase 1+) seed ICE directly.
}

void promote_pending_seeds(NetSession&) {
    // No-op now that seeds aren't sourced from the sector.
}

// Centralised damage + trace mutation. Single chokepoint for the
// Invulnerable GE — extends the real-world dev-cheat to the Grid.
// `last_killer_color` is updated even while invulnerable so post-mortem
// telemetry stays correct if invuln is later removed mid-fight.
[[maybe_unused]] static void damage_avatar(NetSession& s, Game& game, int dmg, IceColor by) {
    if (s.last_killer_color != IceColor::Black) {
        s.last_killer_color = by;
    }
    if (has_effect(game.player().effects, EffectId::Invulnerable)) return;
    s.avatar_hp -= dmg;
    if (by == IceColor::Black && dmg > 0 && s.avatar_hp > 0) {
        game.hacking().request_takeover();
    }
}

static void tick_trace(NetSession& s, Game& game, int amount) {
    if (amount <= 0) return;
    if (has_effect(game.player().effects, EffectId::Invulnerable)) return;
    s.gain_trace(amount);
}

void tick_all(NetSession& s, Game& game) {
    static const int dxs[4] = { 0, 0, -1, 1 };
    static const int dys[4] = { -1, 1,  0, 0 };

    // ghost_trace.exe makes the avatar invisible to white ICE for N turns.
    const bool ghost_cloaked = has_effect(game.player().effects, EffectId::GhostCloak);

    for (auto& ice : s.ice) {
        if (ice.hp <= 0) continue;

        // DaemonHijack charm: skip enemy AI for this turn, then count down.
        if (ice.charmed_turns_left > 0) {
            --ice.charmed_turns_left;
            continue;
        }

        bool in_range = manhattan(ice.x, ice.y, s.avatar_x, s.avatar_y) <= kIceVisionRange;
        bool sees = in_range &&
                    !(ice.color == IceColor::White && ghost_cloaked);
        ice.sees_avatar = sees;

        switch (ice.color) {
            case IceColor::White: {
                if (sees) {
                    int bonus = s.skill_intrusion ? 0 : 1;
                    tick_trace(s, game, bonus);
                } else {
                    int d = ice.patrol_dir;
                    int nx = ice.x + dxs[d];
                    int ny = ice.y + dys[d];
                    // Phase 5 S6.3: White patrols within its home room only.
                    // home_room_idx == -1 means legacy/unset -> no constraint
                    // (back-compat for any pre-S6.3 spawn site). When set, the
                    // candidate cell must also resolve to the same room index.
                    bool same_room = true;
                    if (ice.home_room_idx >= 0) {
                        int dest_room = room_index_at(s.netspace, nx, ny);
                        same_room = (dest_room == ice.home_room_idx);
                    }
                    if (same_room &&
                        s.netspace.passable(nx, ny) &&
                        !(nx == s.avatar_x && ny == s.avatar_y)) {
                        ice.x = nx; ice.y = ny;
                    } else {
                        ice.patrol_dir = (d + 1) % 4;
                    }
                }
                break;
            }
            case IceColor::Gray:
                // Phase 5 S3: Gray's legacy melee/chase is RETIRED in
                // netspace. Its turn is now a ranged pipe-cast at the
                // player node, driven from net_combat.cpp ice_cast_tick
                // (called from tick_grid right after this pass). Gray
                // takes no action in tick_all anymore. White (ambient)
                // and Black (walker, S5) are unchanged below.
                break;
            case IceColor::Black:
                // Phase 5 S5: Black's legacy tile-chase + manhattan==1
                // melee is RETIRED. Black is now a pipe-graph walker
                // driven from net_combat.cpp black_walker_tick (called
                // from tick_grid right after ice_cast_tick). White
                // (ambient) above unchanged; Gray (retired S3) above
                // also unchanged this slice.
                break;
        }
    }

    // DaemonHijack: if the puppet died this tick (or earlier — defensive),
    // clear the handle BEFORE the erase below so the index can't drift onto
    // a different ICE next turn.
    if (s.hijacked_ice_idx >= 0) {
        if (s.hijacked_ice_idx >= static_cast<int>(s.ice.size()) ||
            s.ice[s.hijacked_ice_idx].hp <= 0) {
            s.hijacked_ice_idx = -1;
            s.hijacked_turns_left = 0;
            s.push_log(">> " + display_name(ProgramId::DaemonHijack)
                       + ": target lost — control reverted.");
        }
    }

    s.ice.erase(std::remove_if(s.ice.begin(), s.ice.end(),
        [](const Ice& i){ return i.hp <= 0; }), s.ice.end());
}

void damage(NetSession& /*s*/, Ice& ice, int dmg) {
    ice.hp -= dmg;
}

bool kill_if_dead(NetSession& s, Ice& ice) {
    // Phase 5 S5: idempotent. A single dead ICE may be visited by both
    // apply_effect_in_net's Pass-2 AND the resolve_inflight_impacts
    // end-sweep (which catches collision-killed ICE) in the same beat.
    // Returning true exactly once on the first call preserves the
    // existing trace/XP accounting.
    if (ice.hp > 0 || ice.killed) return false;
    ice.killed = true;
    s.gain_trace(kKillIceTrace);
    return true;
}

} // namespace net_ice

} // namespace astra
