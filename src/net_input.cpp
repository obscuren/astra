#include "astra/net_input.h"

#include "astra/grammars/gen_elevator_netspace.h"
#include "astra/consciousness_save.h"
#include "astra/cyberdeck.h"
#include "astra/game.h"
#include "astra/net_combat.h"
#include "astra/net_combat_mode.h"
#include "astra/net_constants.h"
#include "astra/net_display.h"
#include "astra/net_pipe_path.h"
#include "astra/net_session.h"
#include "astra/net_voice.h"
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

namespace astra::net_input {

namespace {

// Forward declarations — defined below resolve_action_node.
void resolve_ghost_choice(Game& game, NetSession& s, const GhostDialogChoice& c);
void open_ghost_dialog(NetSession& s, uint32_t seed, int node_index);

bool try_move(NetSession& s, int dx, int dy) {
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
bool try_move_hijacked_ice(NetSession& s, int dx, int dy) {
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

// Phase 4: resolve an action node when the avatar steps onto it.
void resolve_action_node(Game& game, NetSession& s, NetNode& n) {
    switch (n.kind) {
        case NetNodeKind::None:
            // Inert placeholder — no effect, no log, no consume.
            break;
        case NetNodeKind::Stash: {
            int cr = 10 + static_cast<int>(n.payload % 40u);
            s.loot.credits += cr;
            s.loot.lore_unlocked.push_back("stash-lead");  // TODO: real lore-key table in a later phase
            s.push_log(">> Stash lead recovered (+" + std::to_string(cr) + " cr).");
            n.consumed = true;
            break;
        }
        case NetNodeKind::VaultGrab: {
            int cr = static_cast<int>(n.payload);
            s.loot.credits += cr;
            int spike = 20 + s.netspace.target.tier * 8;
            s.gain_trace(spike);
            s.push_log(">> VAULT cracked: +" + std::to_string(cr) +
                       " cr. Trace +" + std::to_string(spike) + ".");
            n.consumed = true;
            break;
        }
        case NetNodeKind::TurretDisarm: {
            game.hacking().turret_outcome(game, s, /*flip=*/false);
            n.consumed = true;
            game.hacking().jack_out(game, JackOutKind::Voluntary);
            break;
        }
        case NetNodeKind::TurretFlip: {
            game.hacking().turret_outcome(game, s, /*flip=*/true);
            s.gain_trace(10);
            n.consumed = true;
            game.hacking().jack_out(game, JackOutKind::Voluntary);
            break;
        }
        case NetNodeKind::GhostTalk: {
            // Guard: don't re-open if the dialog is already showing
            // (avatar standing on the tile while dialog is open).
            if (!s.ghost_dialog.open) {
                int idx = s.netspace.action_node_at(n.x, n.y);
                open_ghost_dialog(s, n.payload, idx);
            }
            // NOT consumed here — resolve_ghost_choice / Esc consumes it.
            break;
        }
        default:
            s.push_log(">> [unimpl node] kind=" +
                       std::to_string(static_cast<int>(n.kind)));
            n.consumed = true;
            break;
    }
}

void resolve_ghost_choice(Game& game, NetSession& s, const GhostDialogChoice& c) {
    (void)game;
    auto& gd = s.ghost_dialog;
    switch (c.outcome) {
        case 1: // stash lead
            s.loot.credits += kGhostStashCredits;
            s.loot.lore_unlocked.push_back("stash-lead");
            s.push_log(">> The ghost murmurs a location. Stash lead recorded.");
            break;
        case 2: { // provoke — a guard fragment manifests
            Ice g; g.color = IceColor::Gray; g.hp = 2;
            int ax = s.avatar_x, ay = s.avatar_y;
            const int dx[4] = {0, 0, -1, 1}, dy[4] = {-1, 1, 0, 0};
            bool placed = false;
            for (int i = 0; i < 4 && !placed; ++i) {
                int nx = ax + dx[i], ny = ay + dy[i];
                if (s.netspace.passable(nx, ny)) {
                    bool occ = false;
                    for (auto& ic : s.ice) if (ic.x == nx && ic.y == ny) { occ = true; break; }
                    if (!occ) { g.x = nx; g.y = ny; s.ice.push_back(g); placed = true; }
                }
            }
            s.gain_trace(kGhostProvokeTrace);
            s.push_log(">> You pushed too hard. Something old wakes up.");
            break;
        }
        default: // 0 = lore
            s.loot.lore_unlocked.push_back("ghost-lore");
            s.push_log(">> The ghost tells you what it remembers.");
            break;
    }
    if (gd.node_index >= 0 &&
        gd.node_index < static_cast<int>(s.netspace.action_nodes.size()))
        s.netspace.action_nodes[gd.node_index].consumed = true;
}

void open_ghost_dialog(NetSession& s, uint32_t seed, int node_index) {
    auto& gd = s.ghost_dialog;
    gd.open       = true;
    gd.node_index = node_index;
    gd.sel        = 0;
    gd.lines.clear();
    gd.choices.clear();

    // Three authored scripts; pick deterministically by seed.
    switch (seed % 3u) {
        case 0: // mournful lore
            gd.lines = {
                "A face flickers in the dead deck. It does not",
                "recognize you. \"...did I make it out? Tell me",
                "I made it out.\"",
            };
            gd.choices = {
                { "\"You made it out.\" (let it rest)", 0 },
                { "Ask what it was running from",        0 },
                { "Say nothing. Take what you need.",    2 },
            };
            break;
        case 1: // stash lead
            gd.lines = {
                "The ghost is lucid for a moment. \"There's a",
                "cache. Behind the noodle bar on Jig-Jig. I",
                "never spent it. You should.\"",
            };
            gd.choices = {
                { "Press for the exact location", 1 },
                { "Ask who it was",               0 },
                { "Pull the data by force",       2 },
            };
            break;
        default: // still alive; wants the deck back
            gd.lines = {
                "Not a ghost -- a live uplink. \"That's MY deck.",
                "I'm not dead yet. Put it down and walk away",
                "and maybe I forget your face.\"",
            };
            gd.choices = {
                { "Promise to return it (lie)", 0 },
                { "Ask for the stash as payment", 1 },
                { "Cut the uplink hard",          2 },
            };
            break;
    }
}

// Look up the edge from `from` to `to` (LAN root edges fan out from the
// Phase 0: the Netspace stub only carries Exit tiles for interactable
// behavior. DataNode / EncryptedFile / DeepGridGateway / WarpAnchor were
// part of the multi-region geography and are retired with the netspace
// redesign. Per-target tile interactions return in Phase 1+ grammars.
void on_step(Game& game, NetSession& s) {
    // Phase 4: action node dispatch — only intercepts for non-None kinds;
    // None nodes are inert and fall through to Exit handling below.
    int ni = s.netspace.action_node_at(s.avatar_x, s.avatar_y);
    if (ni >= 0 && s.netspace.action_nodes[ni].kind != NetNodeKind::None) {
        resolve_action_node(game, s, s.netspace.action_nodes[ni]);
        return;
    }
    if (s.netspace.at(s.avatar_x, s.avatar_y) == NetTile::Exit) {
        if (s.netspace.press_luck_step > 0) {
            int floor = elevator_floor_for_y(s.netspace, s.avatar_y);
            if (floor > 0) {
                s.gain_trace(floor * s.netspace.press_luck_step);
                // Top-floor "bleeding from the ears" cost. Reachable: every upper
                // floor now has a dedicated side-step Exit (gen_elevator_netspace.cpp)
                // so floor>0 fires whenever the avatar jacks out from a non-LOBBY floor.
                if (s.netspace.floor_count - floor <= 1) {
                    s.avatar_hp -= 1;
                    if (s.avatar_hp < 0) s.avatar_hp = 0;
                }
                s.push_log(">> Disconnect from F" + std::to_string(floor) +
                           " — trace +" + std::to_string(floor * s.netspace.press_luck_step) + ".");
            }
        }
        s.push_log(">> Disconnect channel...");
        game.hacking().jack_out(game, JackOutKind::Voluntary);
    }
}

// ---------------------------------------------------------------------------
// Plan 6: number-key program firing via Telegraph
// ---------------------------------------------------------------------------

bool can_afford_program(NetSession& s, const CyberdeckData& cd,
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

void fire_program(Game& game, NetSession& s, CyberdeckData& cd,
                  const ProgramDef& def, int tx, int ty, int slot_idx) {
    s.ram -= def.ram_cost;                       // reserved at cast
    int heat = def.heat_cost;
    if (s.skill_ghost_protocol && !s.ghost_protocol_used) {
        heat = 0;
        s.ghost_protocol_used = true;
    }
    cyberdeck_add_heat(cd, heat);

    NetInFlight f;
    f.slot        = slot_idx;
    f.program_id  = static_cast<uint16_t>(def.id);
    f.turns_total = std::max(1, def.net_exec_turns);
    f.turns_left  = f.turns_total;
    f.ram_held    = def.ram_cost;
    f.target_x    = tx;
    f.target_y    = ty;
    s.in_flight.push_back(f);
    s.push_log(astra::net_voice::cmd("run " + std::string(display_name(def))
               + (f.turns_total > 1
                  ? ". exec 0/" + std::to_string(f.turns_total) + "."
                  : ".")));
}

// Arm a slot for directed fire: validates the slot, sets s.armed_slot, and
// returns false (arming spends no world turn). Pressing the same or a
// different slot key while armed simply re-arms to the new slot.
bool arm_slot(Game& game, NetSession& s, int slot) {
    if (s.slot_in_flight(slot)) {
        s.push_log("[BLOCK] slot busy.");
        return false;
    }
    auto* deck_slot = game.player().equipment.equipped_cyberdeck();
    if (!deck_slot || !*deck_slot || !(*deck_slot)->deck) {
        s.push_log("[BLOCK] no cyberdeck equipped");
        return false;
    }
    auto& cd = *(*deck_slot)->deck;
    if (slot < 0 || slot >= kCyberdeckMaxSlots || slot_is_empty(cd.loaded[slot])) {
        s.push_log("[BLOCK] empty slot");
        return false;
    }
    s.armed_slot  = slot;
    s.active_pipe = 0;
    s.push_log(astra::net_voice::cmd(
        "armed slot " + std::to_string(slot + 1)
        + ". Tab=pipe  Space=run  Esc=cancel."));
    return false;   // arming spends no world turn
}

// Slice 4 Task 4 fills this. Returns true iff a world turn was committed.
bool confirm_armed(Game& game, NetSession& s, const std::vector<int>& conn) {
    int slot = s.armed_slot;   // guaranteed >= 0 by the arm-mode dispatch above

    auto* deck_slot = game.player().equipment.equipped_cyberdeck();
    if (!deck_slot || !*deck_slot || !(*deck_slot)->deck) {
        s.push_log("[BLOCK] no cyberdeck equipped");
        s.armed_slot = -1;
        return false;
    }
    auto& cd = *(*deck_slot)->deck;

    if (slot_is_empty(cd.loaded[slot])) {
        s.armed_slot = -1;
        return false;
    }

    // ── Compiled (player-authored fragment chain) ─────────────────────────
    if (cd.loaded[slot].compiled.has_value()) {
        const CompiledProgram& cp = *cd.loaded[slot].compiled;

        // Affordability gate: no reserve, no turn when unaffordable, so
        // RAM can never go below 0 (matches can_afford_program semantics).
        if (s.ram < cp.ram_held ||
            cd.heat_current + cp.heat_cost > cd.stats.heat_cap) {
            s.push_log("[BLOCK] insufficient RAM/heat");
            s.armed_slot = -1;
            return false;
        }

        // Pipe availability gate.
        if (conn.empty()) {
            s.push_log("[BLOCK] no pipe from here.");
            s.armed_slot = -1;
            return false;
        }
        int pidx = conn[std::min<int>(s.active_pipe,
                                      static_cast<int>(conn.size()) - 1)];
        auto path = pipe_path_cells(s.netspace, pidx, s.avatar_x, s.avatar_y);
        if (path.empty()) {
            s.push_log("[BLOCK] no pipe path.");
            s.armed_slot = -1;
            return false;
        }

        // Reserve RAM + heat at cast (returned on completion only).
        s.ram -= cp.ram_held;
        cyberdeck_add_heat(cd, cp.heat_cost);

        int iters = cp.resolved.loop_count > 0 ? cp.resolved.loop_count
                  : cp.resolved.tick_count > 0 ? cp.resolved.tick_count
                  : 1;

        NetInFlight f;
        f.slot           = slot;
        f.compiled       = true;
        f.spec           = cp.resolved;
        f.prog_name      = cp.name.empty() ? std::string("program") : cp.name;
        f.turns_total    = iters;
        f.turns_left     = iters;
        f.ram_held       = cp.ram_held;
        f.launched       = false;
        f.pipe_path      = path;
        f.seg_len        = clamp_seg_len(static_cast<int>(path.size()));
        f.iters_total    = iters;
        f.iters_launched = 0;
        f.payloads.clear();
        f.target_x       = path.back().first;
        f.target_y       = path.back().second;
        f.pipe_index     = pidx;
        s.in_flight.push_back(std::move(f));
        s.push_log(astra::net_voice::cmd(
            "run " + cd.loaded[slot].compiled->name + ". launching."));
        s.armed_slot = -1;
        return true;
    }

    // ── ProgramDef (legacy authored program) ──────────────────────────────
    Item probe = build_by_def_id(cd.loaded[slot].program_def_id);
    if (!probe.program) { s.armed_slot = -1; return false; }
    ProgramId pid = probe.program->id;
    const auto* def = find_program(pid);
    if (!def || def->kind == ProgramKind::Qh) { s.armed_slot = -1; return false; }

    if (!can_afford_program(s, cd, *def)) {
        s.armed_slot = -1;
        return false;
    }

    // Self-targeted: resolve instantly — exactly as the legacy self branch does.
    if (def->targeting == TargetingMode::Self) {
        fire_program(game, s, cd, *def, -1, -1, slot);
        s.armed_slot = -1;
        return true;
    }

    // Tile-targeted / pipe travel.
    if (conn.empty()) {
        s.push_log("[BLOCK] no pipe from here.");
        s.armed_slot = -1;
        return false;
    }
    int pidx = conn[std::min<int>(s.active_pipe,
                                  static_cast<int>(conn.size()) - 1)];
    auto path = pipe_path_cells(s.netspace, pidx, s.avatar_x, s.avatar_y);
    if (path.empty()) {
        s.push_log("[BLOCK] no pipe path.");
        s.armed_slot = -1;
        return false;
    }

    // Reserve RAM + heat (mirrors fire_program body).
    s.ram -= def->ram_cost;
    int heat = def->heat_cost;
    if (s.skill_ghost_protocol && !s.ghost_protocol_used) {
        heat = 0;
        s.ghost_protocol_used = true;
    }
    cyberdeck_add_heat(cd, heat);

    NetInFlight f;
    f.slot           = slot;
    f.compiled       = false;
    f.program_id     = static_cast<uint16_t>(def->id);
    f.prog_name      = std::string(display_name(*def));
    f.turns_total    = std::max(1, def->net_exec_turns);
    f.turns_left     = f.turns_total;
    f.ram_held       = def->ram_cost;
    f.launched       = false;
    f.pipe_path      = path;
    f.seg_len        = clamp_seg_len(static_cast<int>(path.size()));
    f.iters_total    = 1;
    f.iters_launched = 0;
    f.payloads.clear();
    f.target_x       = path.back().first;
    f.target_y       = path.back().second;
    f.pipe_index     = pidx;
    s.in_flight.push_back(std::move(f));
    s.push_log(astra::net_voice::cmd(
        "run " + std::string(display_name(*def)) + ". launching."));
    s.armed_slot = -1;
    return true;
}

} // namespace

bool handle(Game& game, int key) {
    if (game.hacking().in_blocking_transition()) {
        if (auto* s = game.hacking().session_mut()) {
            auto k = s->window_seq.kind;
            if ((k == WindowSeqKind::Opening || k == WindowSeqKind::ClosingNormal)
                && game.hacking().has_seen_ritual())
                s->window_seq.skip_held = true;
        }
        return false;
    }

    auto* sess = game.hacking().session();
    if (!sess) return false;
    auto& s = *sess;

    // Ghost dialog modal — intercepts ALL keys while open; world never advances.
    if (s.ghost_dialog.open) {
        auto& gd = s.ghost_dialog;
        switch (key) {
            case KEY_UP:
                if (gd.sel > 0) --gd.sel;
                return false;
            case KEY_DOWN:
                if (gd.sel + 1 < static_cast<int>(gd.choices.size())) ++gd.sel;
                return false;
            case ' ': case '\r': case '\n':
                if (!gd.choices.empty())
                    resolve_ghost_choice(game, s, gd.choices[gd.sel]);
                gd.open = false;
                return false;
            case 27: /* Esc — leave without payload; consume the node */
                if (gd.node_index >= 0 &&
                    gd.node_index < static_cast<int>(s.netspace.action_nodes.size()))
                    s.netspace.action_nodes[gd.node_index].consumed = true;
                gd.open = false;
                return false;
            default:
                // All other keys (incl. Q hard-jack-out) deliberately blocked while the ghost speaks; Esc dismisses the dialog, then normal keys resume.
                return false;  // modal swallows everything else; no world tick
        }
    }

    // Arm-mode modal — intercepts ALL keys while a slot is armed.
    if (s.armed_slot >= 0) {
        auto conn = connected_pipe_indices(s.netspace, s.avatar_x, s.avatar_y);
        switch (key) {
            case 27:                                  // Esc — abort the arm
                s.armed_slot = -1;
                s.push_log(astra::net_voice::cmd("cancelled."));
                return false;
            case '\t':                                // Tab — cycle active pipe
                if (!conn.empty())
                    s.active_pipe = (s.active_pipe + 1)
                                    % static_cast<int>(conn.size());
                return false;
            case ' ': case '\r': case '\n':           // confirm — execute
                return confirm_armed(game, s, conn);
            default:
                return false;                         // arm mode owns input
        }
    }

    // Phase 5 tactical combat (Slice 1): in COMBAT you are node-locked —
    // free movement is disabled (cast flow above still works). Movement
    // keys are inert (no world tick). CORE actions land in Slice 2.
    if (s.combat_mode == NetSession::NetCombatMode::Combat) {
        switch (key) {
            // Gate every locomotion binding, not just the arrow keycodes:
            // h/j/k/l are full movement aliases (see the move dispatch
            // below). '.' (wait) is intentionally NOT gated here — the
            // round model that replaces generic wait with CORE stances
            // is Slice 2; blocking it now would leave a program-less
            // player no way to pass a turn.
            case KEY_UP: case KEY_DOWN: case KEY_LEFT: case KEY_RIGHT:
            case 'k':    case 'j':      case 'h':      case 'l':
                return false;            // swallowed, no world advance
            case 'q': case 'w': case 'e': case 'r': {
                int idx = (key == 'q') ? 0 : (key == 'w') ? 1
                        : (key == 'e') ? 2 : 3;
                NetCoreAction act = s.core_actions[static_cast<size_t>(idx)];
                if (act == NetCoreAction::None) return false;
                if (act == NetCoreAction::Run) {
                    // Game-touching autopilot loop -- ticks tick_grid
                    // internally up to kRunAutopilotCap beats; returns
                    // having committed the player's "turn" as a single
                    // input event from this dispatcher's POV.
                    core_action_run(game);
                } else {
                    core_action_perform(s, idx);
                }
                return true;
            }
            default:
                break;
        }
    }

    // Telegraph eats input first when active.
    if (game.telegraph().active()) {
        s.committed_this_key = false;
        game.telegraph().handle_input(key, game);   // may run on_confirm -> fire_program
        if (!game.telegraph().active()) s.active_slot = -1;
        bool committed = s.committed_this_key;       // set only by a successful on_confirm
        s.committed_this_key = false;
        return committed;   // confirmed fire -> turn; re-aim / cancel -> free
    }

    // Log scrollback — free action (never consumes a world turn).
    // Page step = band height - 1 line of context overlap.
    constexpr int kLogPageStep = 7;
    if (key == KEY_PAGE_UP || key == KEY_PAGE_DOWN) {
        if (key == KEY_PAGE_UP) {
            s.log_scroll += kLogPageStep;
        } else {
            s.log_scroll -= kLogPageStep;
        }
        if (s.log_scroll < 0) s.log_scroll = 0;
        // Over-approximation upper bound: draw_log_pane re-clamps against the
        // true wrapped-line count, so pinning to log_lines.size() is safe.
        int hi = static_cast<int>(s.log_lines.size());
        if (s.log_scroll > hi) s.log_scroll = hi;
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
                warden.hp = std::max(0, warden.hp - kNetMeleeDamage);
                // Pay melee costs.
                s.ram = std::max(0, s.ram - kNetMeleeRamCost);
                if (kNetMeleeHeatCost > 0) {
                    auto* deck_slot = game.player().equipment.equipped_cyberdeck();
                    if (deck_slot && *deck_slot && (*deck_slot)->deck) {
                        cyberdeck_add_heat(*(*deck_slot)->deck, kNetMeleeHeatCost);
                    }
                }
                // Grant XP if the strike destroyed the Warden.
                if (warden.hp <= 0) {
                    int xp = (warden_color == IceColor::White) ? kXpIceWhite
                           : (warden_color == IceColor::Gray)  ? kXpIceGray
                           :                                     kXpIceBlack;
                    grant_net_xp(game, xp);
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
        case '.': {
            // Idle/stay: a committed turn that does nothing but let the
            // deck cool — passive +1 RAM (clamped at ram_max). Passive
            // trace decay is already applied turn-driven in tick_grid.
            // Storyboard /tmp/ui-mock-2 frame 5: "idle: RAM regen +1 -> 2".
            if (s.ram < s.ram_max) ++s.ram;
            s.push_log(astra::net_voice::cmd(
                "idle. deck cooling. RAM " + std::to_string(s.ram)
                + "/" + std::to_string(s.ram_max) + "."));
            return true;
        }

        case 'o': {
            // Observe — a FREE action (combat.md §Action Economy): inspect
            // the immediate surroundings; never advances the net clock.
            // Minimal honest verb for slice 2; tiered enemy-intent
            // telegraph content lands in slice 6.
            const auto& ns = s.netspace;
            NetTile here = ns.at(s.avatar_x, s.avatar_y);
            bool on_pipe = (here == NetTile::PipeH
                         || here == NetTile::PipeV
                         || here == NetTile::PipeJunc);
            std::string note = on_pipe
                ? "observe: standing on a data pipe. "
                : "observe: in open node. ";
            int best = -1;
            long bd = 0;
            for (size_t i = 0; i < s.ice.size(); ++i) {
                if (s.ice[i].hp <= 0) continue;
                long dx = s.ice[i].x - s.avatar_x;
                long dy = s.ice[i].y - s.avatar_y;
                long d = dx * dx + dy * dy;
                if (best < 0 || d < bd) { best = static_cast<int>(i); bd = d; }
            }
            if (best >= 0)
                note += display_name(s.ice[best].color) + " process nearby.";
            else
                note += "no hostile process in sight.";
            s.push_log(astra::net_voice::cmd(note));
            return false;   // FREE — net clock does not advance
        }

        case '1': return arm_slot(game, s, 0);
        case '2': return arm_slot(game, s, 1);
        case '3': return arm_slot(game, s, 2);
        case '4': return arm_slot(game, s, 3);
        case '5': return arm_slot(game, s, 4);
        case '6': return arm_slot(game, s, 5);
        case '7': return arm_slot(game, s, 6);
        case '8': return arm_slot(game, s, 7);

        case 'Q':
            game.hacking().jack_out(game, JackOutKind::HardJackOut);
            return false;
    }
    return false;
}

} // namespace astra::net_input
