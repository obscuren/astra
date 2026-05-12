#include "astra/program_effects.h"

#include "astra/combat_system.h"
#include "astra/cyberdeck.h"
#include "astra/display_name.h"
#include "astra/effect.h"
#include "astra/game.h"
#include "astra/grid_combat.h"
#include "astra/grid_constants.h"
#include "astra/grid_display.h"
#include "astra/grid_ice.h"
#include "astra/grid_network.h"
#include "astra/grid_persistence.h"
#include "astra/grid_session.h"
#include "astra/hackable.h"
#include "astra/lan.h"
#include "astra/npc.h"
#include "astra/vulnerability.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <random>
#include <string>

namespace astra {

namespace {

// ── Real-world quickhack helpers ────────────────────────────────────

void apply_reboot_optics(Game& game, Hackable& target, int /*tx*/, int /*ty*/) {
    target.state = HackState::Compromised;
    target.state_ticks_left = 4;
    game.log("The " + std::string(tag_summary(target.tags)) +
             " judders and flickers offline.");
}

void apply_friendly_fire(Game& game, Hackable& target, int tx, int ty) {
    // Friendly Fire's tag filter is {Weaponized | Mobile} — mobile weapon
    // platforms (drones, sentries with cybernetic implants). The in-effect
    // guard mirrors the filter's most distinctive bit.
    if (!has_tag(target.tags, HackTag::Weaponized)) {
        game.log("Friendly Fire only works on weapon platforms.");
        return;
    }
    target.state = HackState::Compromised;
    target.state_ticks_left = 2;
    for (auto& npc : game.world().npcs()) {
        if (npc.x == tx && npc.y == ty && npc.alive()) {
            if (npc.pre_hijack_faction.empty()) {
                npc.pre_hijack_faction = npc.faction;
            }
            npc.faction = "Hijacked";
            add_effect(npc.effects, make_hijacked_ge(2));
            game.log("The turret rotates onto its allies.");
            return;
        }
    }
    game.log("The turret's targeting matrix scrambles, but no allies are in range.");
}

void apply_data_leech(Game& game, Hackable& target, int /*tx*/, int /*ty*/) {
    target.state = HackState::Compromised;
    target.state_ticks_left = 1;
    int credits = 5 + (target.security_tier * 5);
    game.player().money += credits;
    game.log("Data leeched: +" + std::to_string(credits) +
             " credits skimmed off the bus.");
}

// ── Grid-.exe helpers ────────────────────────────────────────────────

// Records the kill against the active LAN's runtime state when the ICE
// reached HP <= 0. Returns true if the ICE was just killed.
bool kill_and_persist(Game& game, GridSession& s, GridIce& ice) {
    if (ice.hp > 0) return false;
    int kx = ice.x;
    int ky = ice.y;
    IceColor col = ice.color;
    bool killed = grid_ice::kill_if_dead(s, ice);
    if (killed) {
        record_killed_ice(game, kx, ky);
        int xp = (col == IceColor::White) ? kXpIceWhite
               : (col == IceColor::Gray)  ? kXpIceGray
               :                            kXpIceBlack;
        grant_grid_xp(game, xp);
    }
    return killed;
}

std::string apply_icebreaker_lite_grid(GridProgramContext c) {
    // Plan 6: target tile supplied by Telegraph; valid_target predicate
    // already guaranteed an ICE is at (tx, ty).
    GridIce* tgt = nullptr;
    for (auto& i : c.session.ice) {
        if (i.hp > 0 && i.x == c.target_x && i.y == c.target_y) {
            tgt = &i; break;
        }
    }
    const std::string prefix = display_name(ProgramId::IcebreakerLite) + ": ";
    if (!tgt) return prefix + "target lost.";

    std::uniform_int_distribution<int> roll(1, 4);
    int dmg = 1 + roll(c.game.world().rng())
            + (c.session.skill_icebreaking ? 1 : 0);

    IceColor col = tgt->color;
    grid_ice::damage(c.session, *tgt, dmg);
    kill_and_persist(c.game, c.session, *tgt);
    return prefix + std::to_string(dmg) + " damage to " + display_name(col) + ".";
}

std::string apply_ghost_trace_grid(GridProgramContext c) {
    c.session.trace = std::max(0, c.session.trace - 3);
    add_effect(c.game.player().effects, make_ghost_cloak_ge(3));
    return display_name(ProgramId::GhostTrace)
         + ": invisible to " + display_name(IceColor::White) + " for 3 turns. Trace -3.";
}

std::string apply_cooldown_grid(GridProgramContext c) {
    const std::string prefix = display_name(ProgramId::Cooldown) + ": ";
    auto* slot = c.game.player().equipment.equipped_cyberdeck();
    if (!slot || !*slot || !(*slot)->deck) return prefix + "no deck.";
    auto& cd = *(*slot)->deck;
    cd.heat_current = std::max(0, cd.heat_current - 4);
    return prefix + "heat -4.";
}

// Try to crack the edge from (the LAN root or `from_node`) -> `target`.
// Sets `cracked = true` on the matching edge if found. Returns true on hit.
bool crack_gateway_edge(GridNetwork& net,
                        GridNodeId from_node,
                        GridNodeId lan_root,
                        GridNodeId target) {
    for (auto& e : net.edges_mut()) {
        if (e.to == target &&
            (e.from == from_node || (lan_root.valid() && e.from == lan_root))) {
            e.cracked = true;
            return true;
        }
    }
    return false;
}

std::string apply_breach_grid(GridProgramContext c) {
    // Plan 6: target tile supplied by Telegraph; valid_target predicate
    // already guaranteed (tx, ty) is a Firewall, Gateway, or DeepGridGateway.
    GridSession& s = c.session;
    const std::string prefix = display_name(ProgramId::Breach) + ": ";
    int x = c.target_x, y = c.target_y;
    if (!s.sector.in_bounds(x, y)) return prefix + "target out of bounds.";
    GridTile t = s.sector.at(x, y);

    if (t == GridTile::Firewall) {
        s.sector.set(x, y, GridTile::Floor);
        s.gain_trace(5);
        record_sector_mutation(c.game, x, y, GridTile::Floor);
        return prefix + display_name(GridTile::Firewall) + " down. Trace +5.";
    }
    if (t == GridTile::DeepGridGateway) {
        const std::string gw = display_name(t);
        GridNodeId tgt = s.sector.deep_grid_destination;
        if (!tgt.valid()) {
            return prefix + gw + " has no target node.";
        }
        auto& net = c.game.world().grid_network();
        const auto& meta = c.game.world().lan_metadata();
        if (!crack_gateway_edge(net, s.current_node, meta.lan_root, tgt)) {
            return prefix + "edge not found for " + gw + " target.";
        }
        s.gain_trace(5);

        // First-time crack of a connected LAN's ⊕ registers an Atlas
        // WarpAnchor in the consciousness save and stamps a ◉ tile in the
        // deep-Grid Atlas region.
        register_deep_grid_warp_anchor(c.game.world(), s.current_node);
        return prefix + gw + " cracked. Trace +5.";
    }
    // Plan 8 (Cut 3): Door branch for v2 generator locked bridges. Valid_target
    // predicate already ensured this is a locked Door. Unlock in-memory; no tile
    // change needed — renderer reads locked_doors set. v1 Firewall/Gateway
    // branches above stay alive through Cut 6; removed in Cut 7.
    if (t == GridTile::Door) {
        if (!s.sector.is_locked_door(x, y)) {
            return prefix + "door already open.";
        }
        s.sector.unlock_door(x, y);
        record_cracked_door(c.game, x, y);
        s.gain_trace(5);
        return prefix + "lock cracked — door open. Trace +5.";
    }
    return prefix + "nothing to break here.";
}

std::string apply_decrypt_grid(GridProgramContext c) {
    // Plan 6: target tile supplied by Telegraph; valid_target predicate
    // already guaranteed (tx, ty) is an EncryptedFile.
    const std::string prefix = display_name(ProgramId::Decrypt) + ": ";
    int x = c.target_x, y = c.target_y;
    if (!c.session.sector.in_bounds(x, y) ||
        c.session.sector.at(x, y) != GridTile::EncryptedFile) {
        return prefix + "target lost.";
    }
    c.session.sector.set(x, y, GridTile::Floor);
    record_sector_mutation(c.game, x, y, GridTile::Floor);
    c.session.loot.lore_unlocked.push_back(
        "ARCH-" + std::to_string(c.session.entry_node.value) +
        "-" + std::to_string(x * 1000 + y));
    return prefix + "archive read.";
}

std::string apply_pulse_hammer_grid(GridProgramContext c) {
    // Plan 6: AoE 3×3 centered on the Telegraph-supplied tile. Hits any ICE
    // in the footprint (1d6 each).
    const std::string prefix = display_name(ProgramId::PulseHammer) + ": ";
    int tx = c.target_x, ty = c.target_y;
    int hit = 0;
    std::uniform_int_distribution<int> roll(1, 6);
    for (auto& ice : c.session.ice) {
        if (ice.hp <= 0) continue;
        int dx = std::abs(ice.x - tx);
        int dy = std::abs(ice.y - ty);
        if (dx <= 1 && dy <= 1) {
            int dmg = roll(c.game.world().rng());
            grid_ice::damage(c.session, ice, dmg);
            ++hit;
        }
    }
    for (auto& ice : c.session.ice) kill_and_persist(c.game, c.session, ice);
    if (hit == 0) return prefix + "no ICE in blast radius.";
    return prefix + "hit " + std::to_string(hit) + " ICE.";
}

std::string apply_daemon_hijack_grid(GridProgramContext c) {
    // Plan 6: target tile supplied by Telegraph; predicate already verified
    // a live ICE sits at (tx, ty). Hand the player control of that ICE for
    // N turns. The ICE's own AI is suppressed via charmed_turns_left; the
    // session-level fields route movement input to the puppet.
    GridSession& s = c.session;
    int best_idx = -1;
    GridIce* tgt = nullptr;
    for (size_t i = 0; i < s.ice.size(); ++i) {
        auto& ice = s.ice[i];
        if (ice.hp > 0 && ice.x == c.target_x && ice.y == c.target_y) {
            best_idx = static_cast<int>(i);
            tgt = &ice;
            break;
        }
    }
    const std::string prefix = display_name(ProgramId::DaemonHijack) + ": ";
    if (!tgt) return prefix + "target lost.";

    constexpr int kHijackTurns = 3;
    tgt->charmed_turns_left = kHijackTurns;
    s.hijacked_ice_idx      = best_idx;
    s.hijacked_turns_left   = kHijackTurns;
    return prefix + display_name(tgt->color) + " puppeteered for " +
           std::to_string(kHijackTurns) +
           " turns. Movement keys drive it. Avatar holds.";
}

} // namespace

void apply_program_effect(ProgramId id, Game& game, Hackable& target, int tx, int ty) {
    switch (id) {
        case ProgramId::RebootOptics: apply_reboot_optics(game, target, tx, ty); break;
        case ProgramId::FriendlyFire: apply_friendly_fire(game, target, tx, ty); break;
        case ProgramId::DataLeech:    apply_data_leech(game, target, tx, ty); break;
        default: break;
    }
}

std::string apply_program_in_grid(ProgramId id, GridProgramContext ctx) {
    switch (id) {
        case ProgramId::IcebreakerLite: return apply_icebreaker_lite_grid(ctx);
        case ProgramId::GhostTrace:     return apply_ghost_trace_grid(ctx);
        case ProgramId::Cooldown:       return apply_cooldown_grid(ctx);
        case ProgramId::Breach:         return apply_breach_grid(ctx);
        case ProgramId::Decrypt:        return apply_decrypt_grid(ctx);
        case ProgramId::PulseHammer:    return apply_pulse_hammer_grid(ctx);
        case ProgramId::DaemonHijack:   return apply_daemon_hijack_grid(ctx);

        default:                        return "Program is not Grid-side.";
    }
}

} // namespace astra
