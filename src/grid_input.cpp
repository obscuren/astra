#include "astra/grid_input.h"

#include "astra/consciousness_save.h"
#include "astra/cyberdeck.h"
#include "astra/game.h"
#include "astra/grid_combat.h"
#include "astra/grid_constants.h"
#include "astra/grid_display.h"
#include "astra/grid_session.h"
#include "astra/hacking_system.h"
#include "astra/item.h"
#include "astra/item_defs.h"
#include "astra/lan.h"
#include "astra/npc.h"
#include "astra/program.h"
#include "astra/program_effects.h"
#include "astra/renderer.h"
#include "astra/vulnerability.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <cstdio>
#include <random>
#include <string>

namespace astra::grid_input {

namespace {

bool try_move(GridSession& s, int dx, int dy) {
    int nx = s.avatar_x + dx;
    int ny = s.avatar_y + dy;
    if (!s.netspace.passable(nx, ny)) return false;
    s.avatar_x = nx;
    s.avatar_y = ny;
    return true;
}

// While DaemonHijack is active, movement keys drive the puppeted ICE
// instead of the avatar. Returns true if a turn was consumed (mirrors
// try_move's contract — caller advances the world on true).
bool try_move_hijacked_ice(GridSession& s, int dx, int dy) {
    if (s.hijacked_ice_idx < 0 ||
        s.hijacked_ice_idx >= static_cast<int>(s.ice.size())) {
        // Stale handle — clear and fall back to avatar movement.
        s.hijacked_ice_idx = -1;
        s.hijacked_turns_left = 0;
        return false;
    }
    auto& ice = s.ice[s.hijacked_ice_idx];
    if (ice.hp <= 0) {
        s.hijacked_ice_idx = -1;
        s.hijacked_turns_left = 0;
        return false;
    }
    int nx = ice.x + dx;
    int ny = ice.y + dy;
    if (!s.netspace.passable(nx, ny)) return true;          // bumped — turn still consumed
    if (nx == s.avatar_x && ny == s.avatar_y) return true; // can't trample the operator
    // Don't stack onto another ICE.
    for (size_t i = 0; i < s.ice.size(); ++i) {
        if (static_cast<int>(i) == s.hijacked_ice_idx) continue;
        if (s.ice[i].hp <= 0) continue;
        if (s.ice[i].x == nx && s.ice[i].y == ny) return true;
    }
    ice.x = nx;
    ice.y = ny;
    return true;
}

// Look up the edge from `from` to `to` (LAN root edges fan out from the
// Phase 0: the Netspace stub only carries Exit tiles for interactable
// behavior. DataNode / EncryptedFile / DeepGridGateway / WarpAnchor were
// part of the multi-region geography and are retired with the netspace
// redesign. Per-target tile interactions return in Phase 1+ grammars.
void on_step(Game& game, GridSession& s) {
    if (s.netspace.at(s.avatar_x, s.avatar_y) == NetTile::Exit) {
        s.push_log(">> Disconnect channel...");
        game.hacking().jack_out(game, JackOutKind::Voluntary);
    }
}

// ---------------------------------------------------------------------------
// Plan 6: number-key program firing via Telegraph
// ---------------------------------------------------------------------------

bool can_afford_program(GridSession& s, const CyberdeckData& cd,
                        const ProgramDef& def) {
    if (s.ram < def.ram_cost) {
        s.push_log("[BLOCK] " + display_name(def) + " — "
                   + std::to_string(def.ram_cost) + " RAM required, "
                   + std::to_string(s.ram) + " available");
        return false;
    }
    if (cd.heat_current + def.heat_cost > cd.stats.heat_cap + s.heat_cap_bonus) {
        s.push_log("[BLOCK] " + display_name(def) + " — heat over cap");
        return false;
    }
    return true;
}

void fire_program(Game& game, GridSession& s, CyberdeckData& cd,
                  const ProgramDef& def, int tx, int ty) {
    s.ram -= def.ram_cost;
    int heat = def.heat_cost;
    if (s.skill_ghost_protocol && !s.ghost_protocol_used) {
        heat = 0;
        s.ghost_protocol_used = true;
    }
    cyberdeck_add_heat(cd, heat);

    GridProgramContext ctx{game, s, tx, ty};
    std::string msg = apply_program_in_grid(def.id, ctx);
    s.push_log("> " + display_name(def));
    if (!msg.empty()) s.push_log(std::string("  ") + msg);
}

void fire_program_slot(Game& game, GridSession& s, int slot_idx) {
    auto* deck_slot = game.player().equipment.equipped_cyberdeck();
    if (!deck_slot || !*deck_slot || !(*deck_slot)->deck) {
        s.push_log("[BLOCK] no cyberdeck equipped");
        return;
    }
    auto& cd = *(*deck_slot)->deck;

    int eff_slots = std::min(kCyberdeckMaxSlots,
                             cd.stats.slots + (s.skill_daemon_mastery ? 1 : 0));
    if (slot_idx < 0 || slot_idx >= eff_slots) return;
    if (cd.loaded[slot_idx].program_def_id == 0) {
        s.push_log("[BLOCK] empty slot");
        return;
    }

    Item probe = build_by_def_id(cd.loaded[slot_idx].program_def_id);
    if (!probe.program) return;
    ProgramId pid = probe.program->id;
    const auto* def = find_program(pid);
    if (!def || def->kind == ProgramKind::Qh) return;

    if (!can_afford_program(s, cd, *def)) return;

    if (def->targeting == TargetingMode::Self) {
        fire_program(game, s, cd, *def, -1, -1);
        return;
    }

    // Tile-targeted: launch Telegraph with a Grid-aware passable predicate.
    TelegraphSpec spec = def->telegraph_spec;
    spec.passable_fn = [sess_ptr = &s](int x, int y) -> bool {
        if (!sess_ptr->netspace.in_bounds(x, y)) return false;
        return sess_ptr->netspace.at(x, y) != NetTile::Wall;
    };

    auto on_confirm = [&game, sess_ptr = &s, def_ptr = def](const TelegraphResult& r) {
        if (def_ptr->valid_target && !def_ptr->valid_target(*sess_ptr, r.dest_x, r.dest_y)) {
            sess_ptr->push_log("[ERR] invalid target for " + display_name(*def_ptr));
            return;
        }
        auto* deck_slot2 = game.player().equipment.equipped_cyberdeck();
        if (!deck_slot2 || !*deck_slot2 || !(*deck_slot2)->deck) return;
        auto& cd2 = *(*deck_slot2)->deck;
        fire_program(game, *sess_ptr, cd2, *def_ptr, r.dest_x, r.dest_y);
    };

    s.active_slot = slot_idx;
    game.telegraph().begin(spec, s.avatar_x, s.avatar_y, on_confirm);
}

} // namespace

bool handle(Game& game, int key) {
    auto* sess = game.hacking().session();
    if (!sess) return false;
    auto& s = *sess;

    // Telegraph eats input first when active.
    if (game.telegraph().active()) {
        game.telegraph().handle_input(key, game);
        // If Telegraph closed (confirm or cancel), clear the active-slot
        // highlight so the program bar returns to normal rendering.
        if (!game.telegraph().active()) s.active_slot = -1;
        return false;
    }

    auto move_with_step = [&](int dx, int dy) -> bool {
        int nx = s.avatar_x + dx;
        int ny = s.avatar_y + dy;

        // DaemonHijack: movement keys drive the puppeted ICE; skip bump-attack
        // logic for the avatar.
        if (s.hijacked_ice_idx >= 0) {
            return try_move_hijacked_ice(s, dx, dy);
        }

        // Bump-attack on Ice (Warden).
        for (size_t i = 0; i < s.ice.size(); ++i) {
            auto& warden = s.ice[i];
            if (warden.hp <= 0) continue;
            if (warden.x == nx && warden.y == ny) {
                IceColor warden_color = warden.color;
                warden.hp = std::max(0, warden.hp - kGridMeleeDamage);
                // Pay melee costs.
                s.ram = std::max(0, s.ram - kGridMeleeRamCost);
                if (kGridMeleeHeatCost > 0) {
                    auto* deck_slot = game.player().equipment.equipped_cyberdeck();
                    if (deck_slot && *deck_slot && (*deck_slot)->deck) {
                        cyberdeck_add_heat(*(*deck_slot)->deck, kGridMeleeHeatCost);
                    }
                }
                // Grant XP if the strike destroyed the Warden.
                if (warden.hp <= 0) {
                    int xp = (warden_color == IceColor::White) ? kXpIceWhite
                           : (warden_color == IceColor::Gray)  ? kXpIceGray
                           :                                     kXpIceBlack;
                    grant_grid_xp(game, xp);
                }
                char buf[80];
                std::snprintf(buf, sizeof buf,
                              "> Strike: Warden HP %d.",
                              warden.hp);
                s.push_log(buf);
                return true;   // turn consumed
            }
        }

        // Default: existing movement path.
        bool moved = try_move(s, dx, dy);
        if (moved) on_step(game, s);
        return moved;
    };

    switch (key) {
        case KEY_UP:    case 'k': return move_with_step( 0, -1);
        case KEY_DOWN:  case 'j': return move_with_step( 0,  1);
        case KEY_LEFT:  case 'h': return move_with_step(-1,  0);
        case KEY_RIGHT: case 'l': return move_with_step( 1,  0);
        case '.':                 return true;

        case '1': fire_program_slot(game, s, 0); return false;
        case '2': fire_program_slot(game, s, 1); return false;
        case '3': fire_program_slot(game, s, 2); return false;
        case '4': fire_program_slot(game, s, 3); return false;
        case '5': fire_program_slot(game, s, 4); return false;
        case '6': fire_program_slot(game, s, 5); return false;
        case '7': fire_program_slot(game, s, 6); return false;
        case '8': fire_program_slot(game, s, 7); return false;

        case 'Q':
            game.hacking().jack_out(game, JackOutKind::HardJackOut);
            return false;
    }
    return false;
}

} // namespace astra::grid_input
