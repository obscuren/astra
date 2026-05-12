#include "astra/grid_ice.h"

#include "astra/effect.h"
#include "astra/game.h"
#include "astra/grid_constants.h"
#include "astra/grid_display.h"
#include "astra/grid_session.h"
#include "astra/player.h"

#include <algorithm>
#include <cstdlib>
#include <random>

namespace astra {

namespace grid_ice {

namespace {

bool place_random(GridSession& s, std::mt19937& rng,
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

void step_toward(GridSession& s, GridIce& ice, int tx, int ty) {
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

void spawn_for_sector(GridSession& s, uint32_t seed, int security_tier) {
    std::mt19937 rng(seed ^ 0xDECAFC0Du);
    int n_white = 1;
    int n_gray  = security_tier >= 2 ? 1 : 0;

    for (int i = 0; i < n_white; ++i) {
        int x, y; if (!place_random(s, rng, x, y)) break;
        GridIce ice;
        ice.x = x; ice.y = y;
        ice.color = IceColor::White;
        ice.hp = 1;
        std::uniform_int_distribution<int> dir_d(0, 3);
        ice.patrol_dir = dir_d(rng);
        s.ice.push_back(ice);
    }
    for (int i = 0; i < n_gray; ++i) {
        int x, y; if (!place_random(s, rng, x, y)) break;
        GridIce ice;
        ice.x = x; ice.y = y;
        ice.color = IceColor::Gray;
        ice.hp = 2;
        s.ice.push_back(ice);
    }
}

void spawn_from_seeds(GridSession&) {
    // ICE seed pipeline retired with the legacy sector generators.
    // Per-target netspace grammars (Phase 1+) seed ICE directly.
}

void promote_pending_seeds(GridSession&) {
    // No-op now that seeds aren't sourced from the sector.
}

// Centralised damage + trace mutation. Single chokepoint for the
// Invulnerable GE — extends the real-world dev-cheat to the Grid.
// `last_killer_color` is updated even while invulnerable so post-mortem
// telemetry stays correct if invuln is later removed mid-fight.
static void damage_avatar(GridSession& s, Game& game, int dmg, IceColor by) {
    if (s.last_killer_color != IceColor::Black) {
        s.last_killer_color = by;
    }
    if (has_effect(game.player().effects, EffectId::Invulnerable)) return;
    s.avatar_hp -= dmg;
}

static void tick_trace(GridSession& s, Game& game, int amount) {
    if (amount <= 0) return;
    if (has_effect(game.player().effects, EffectId::Invulnerable)) return;
    s.gain_trace(amount);
}

void tick_all(GridSession& s, Game& game) {
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
                    if (s.netspace.passable(nx, ny) &&
                        !(nx == s.avatar_x && ny == s.avatar_y)) {
                        ice.x = nx; ice.y = ny;
                    } else {
                        ice.patrol_dir = (d + 1) % 4;
                    }
                }
                break;
            }
            case IceColor::Gray: {
                if (!sees) break;
                if (manhattan(ice.x, ice.y, s.avatar_x, s.avatar_y) == 1) {
                    damage_avatar(s, game, 1, IceColor::Gray);
                } else {
                    step_toward(s, ice, s.avatar_x, s.avatar_y);
                }
                break;
            }
            case IceColor::Black: {
                if (manhattan(ice.x, ice.y, s.avatar_x, s.avatar_y) == 1) {
                    int dmg = s.skill_neural_fortitude ? 1 : 2;
                    damage_avatar(s, game, dmg, IceColor::Black);
                } else {
                    step_toward(s, ice, s.avatar_x, s.avatar_y);
                }
                break;
            }
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
        [](const GridIce& i){ return i.hp <= 0; }), s.ice.end());
}

void damage(GridSession& /*s*/, GridIce& ice, int dmg) {
    ice.hp -= dmg;
}

bool kill_if_dead(GridSession& s, GridIce& ice) {
    if (ice.hp > 0) return false;
    s.gain_trace(kKillIceTrace);
    return true;
}

} // namespace grid_ice

} // namespace astra
