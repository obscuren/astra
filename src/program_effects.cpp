#include "astra/program_effects.h"

#include "astra/cyberdeck.h"
#include "astra/effect.h"
#include "astra/game.h"
#include "astra/grid_constants.h"
#include "astra/grid_ice.h"
#include "astra/grid_network.h"
#include "astra/grid_session.h"
#include "astra/hackable.h"
#include "astra/npc.h"
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
    // Friendly Fire only works on weaponized targets (turrets).
    if (!has_tag(target.tags, HackTag::Weaponized)) {
        game.log("Friendly Fire only works on turrets.");
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

std::string apply_icebreaker_lite_grid(GridProgramContext c) {
    GridIce* tgt = nullptr;
    int best = INT_MAX;
    for (auto& i : c.session.ice) {
        int d = std::abs(i.x - c.session.avatar_x) + std::abs(i.y - c.session.avatar_y);
        if (d <= kIceVisionRange && d < best) { tgt = &i; best = d; }
    }
    if (!tgt) return "icebreaker_lite: no target in range.";

    std::uniform_int_distribution<int> roll(1, 4);
    int dmg = 1 + roll(c.game.world().rng())
            + (c.session.skill_icebreaking ? 1 : 0);

    grid_ice::damage(c.session, *tgt, dmg);
    grid_ice::kill_if_dead(c.session, *tgt);
    return "icebreaker_lite: " + std::to_string(dmg) + " damage to ICE.";
}

std::string apply_ghost_trace_grid(GridProgramContext c) {
    c.session.trace = std::max(0, c.session.trace - 3);
    add_effect(c.game.player().effects, make_ghost_cloak_ge(3));
    return "ghost_trace: invisible to white ICE for 3 turns. Trace -3.";
}

std::string apply_cooldown_grid(GridProgramContext c) {
    auto* slot = c.game.player().equipment.equipped_cyberdeck();
    if (!slot || !*slot || !(*slot)->deck) return "cooldown: no deck.";
    auto& cd = *(*slot)->deck;
    cd.heat_current = std::max(0, cd.heat_current - 4);
    return "cooldown: heat -4.";
}

std::string apply_breach_grid(GridProgramContext c) {
    static constexpr int dx[4] = { 0, 0, -1, 1 };
    static constexpr int dy[4] = { -1, 1, 0, 0 };
    for (int d = 0; d < 4; ++d) {
        int nx = c.session.avatar_x + dx[d];
        int ny = c.session.avatar_y + dy[d];
        GridTile t = c.session.sector.at(nx, ny);
        if (t == GridTile::Firewall) {
            c.session.sector.set(nx, ny, GridTile::Floor);
            c.session.trace = std::min(kTraceMax, c.session.trace + 5);
            return "breach: firewall down. Trace +5.";
        }
        if (t == GridTile::Gateway) {
            auto& net = c.game.world().grid_network();
            for (auto& e : net.edges_mut()) {
                if ((e.from == c.session.current_node ||
                     e.to == c.session.current_node) && !e.cracked) {
                    e.cracked = true;
                    c.session.trace = std::min(kTraceMax, c.session.trace + 5);
                    return "breach: gateway cracked. Trace +5.";
                }
            }
            return "breach: no locked gateway here.";
        }
    }
    return "breach: nothing adjacent to break.";
}

std::string apply_decrypt_grid(GridProgramContext c) {
    static constexpr int dx[5] = { 0, 0, 0, -1, 1 };
    static constexpr int dy[5] = { 0, -1, 1, 0, 0 };
    for (int d = 0; d < 5; ++d) {
        int nx = c.session.avatar_x + dx[d];
        int ny = c.session.avatar_y + dy[d];
        if (c.session.sector.at(nx, ny) == GridTile::EncryptedFile) {
            c.session.sector.set(nx, ny, GridTile::Floor);
            c.session.loot.lore_unlocked.push_back(
                "ARCH-" + std::to_string(c.session.entry_node.value) +
                "-" + std::to_string(d));
            return "decrypt: archive read.";
        }
    }
    return "decrypt: no encrypted file in range.";
}

std::string apply_pulse_hammer_grid(GridProgramContext c) {
    // AoE 1d6 damage to all ICE in Chebyshev radius-1 around the nearest ICE.
    // Use the same nearest-ICE targeting as IcebreakerLite, then hit all ICE
    // adjacent (including diagonal) to that tile.
    GridIce* tgt = nullptr;
    int best = INT_MAX;
    for (auto& i : c.session.ice) {
        int d = std::abs(i.x - c.session.avatar_x) + std::abs(i.y - c.session.avatar_y);
        if (d <= kIceVisionRange && d < best) { tgt = &i; best = d; }
    }
    if (!tgt) return "pulse_hammer: no target in range.";

    int tx = tgt->x;
    int ty = tgt->y;
    int hit = 0;
    std::uniform_int_distribution<int> roll(1, 6);
    for (auto& ice : c.session.ice) {
        int dx = std::abs(ice.x - tx);
        int dy = std::abs(ice.y - ty);
        if (dx <= 1 && dy <= 1 && (dx + dy) > 0) {
            int dmg = roll(c.game.world().rng());
            grid_ice::damage(c.session, ice, dmg);
            ++hit;
        }
    }
    // Also damage the primary target.
    {
        int dmg = roll(c.game.world().rng());
        grid_ice::damage(c.session, *tgt, dmg);
        ++hit;
    }
    // Prune dead ICE.
    for (auto& ice : c.session.ice) grid_ice::kill_if_dead(c.session, ice);
    return "pulse_hammer: hit " + std::to_string(hit) + " ICE.";
}

std::string apply_daemon_hijack_grid(GridProgramContext c) {
    // Charm the nearest visible ICE for 3 turns.
    GridIce* tgt = nullptr;
    int best = INT_MAX;
    for (auto& i : c.session.ice) {
        int d = std::abs(i.x - c.session.avatar_x) + std::abs(i.y - c.session.avatar_y);
        if (d <= kIceVisionRange && d < best) { tgt = &i; best = d; }
    }
    if (!tgt) return "daemon_hijack: no target in range.";
    tgt->charmed_turns_left = 3;
    return "daemon_hijack: ICE hijacked for 3 turns.";
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
