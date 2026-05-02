#include "astra/grid_input.h"

#include "astra/cyberdeck.h"
#include "astra/game.h"
#include "astra/grid_constants.h"
#include "astra/grid_network.h"
#include "astra/grid_persistence.h"
#include "astra/grid_session.h"
#include "astra/hacking_system.h"
#include "astra/item.h"
#include "astra/item_defs.h"
#include "astra/lan.h"
#include "astra/program.h"
#include "astra/program_effects.h"
#include "astra/renderer.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <random>
#include <string>

namespace astra::grid_input {

namespace {

bool try_move(GridSession& s, int dx, int dy) {
    int nx = s.avatar_x + dx;
    int ny = s.avatar_y + dy;
    if (!s.sector.passable(nx, ny)) return false;
    s.avatar_x = nx;
    s.avatar_y = ny;
    return true;
}

// Look up the edge from `from` to `to` (LAN root edges fan out from the
// LAN root, so we also accept that). Returns nullptr if no such edge.
const GridEdge* find_outbound_edge(const GridNetwork& net,
                                   GridNodeId from,
                                   GridNodeId to) {
    for (const auto& e : net.edges()) {
        if (e.to == to && (e.from == from)) return &e;
    }
    return nullptr;
}

// Step onto a ⌬ Gateway or ⊕ DeepGridGateway. Resolves the target node from
// the tile's gateway_target map, checks the corresponding edge's cracked
// state (tier 0 always open), and traverses if open.
void traverse_via_gateway(Game& game, GridSession& s) {
    auto it = s.sector.gateway_target.find(
        std::pair<int,int>{s.avatar_x, s.avatar_y});
    if (it == s.sector.gateway_target.end()) {
        game.log("Gateway has no destination wired.");
        return;
    }
    GridNodeId tgt = it->second;
    if (!tgt.valid()) {
        game.log("Gateway: host unreachable.");
        return;
    }

    auto& net = game.world().grid_network();
    const auto& meta = game.world().lan_metadata();

    // Edge can fan out from current_node, OR from the LAN root (LAN→Subnet
    // edges live on the root, but the player triggers them from inside the
    // LAN sector — same node).
    const GridEdge* e = find_outbound_edge(net, s.current_node, tgt);
    if (!e && meta.lan_root.valid() && s.current_node == meta.lan_root) {
        // already covered by the previous lookup; fallthrough.
    }
    if (!e) {
        game.log("Gateway has no edge to that target.");
        return;
    }
    bool open = (e->gateway_tier == 0) || e->cracked;
    if (!open && s.skill_deepgrid_navigator) {
        // 50/50 passive crack — preserves prior DeepGridNavigator behaviour.
        std::uniform_int_distribution<int> coin(0, 1);
        if (coin(game.world().rng()) == 0) {
            for (auto& em : net.edges_mut()) {
                if (em.to == tgt && em.from == e->from) {
                    em.cracked = true;
                    open = true;
                    s.trace = std::min(kTraceMax, s.trace + 5);
                    game.log("DeepGridNavigator: gateway cracked. Trace +5.");
                    break;
                }
            }
        }
    }
    if (!open) {
        game.log("Gateway locked. Try breach.exe.");
        return;
    }
    if (!game.hacking().traverse_to(game, tgt)) {
        game.log("Gateway traversal failed.");
        return;
    }
    game.log("You slip through the gateway.");
}

void on_step(Game& game, GridSession& s) {
    GridTile here = s.sector.at(s.avatar_x, s.avatar_y);
    switch (here) {
        case GridTile::ExitNode: {
            // If we're inside a sub-sector reached via traversal, the ⊙ tile
            // bounces back to the LAN instead of jacking out. This keeps the
            // session alive across multi-sector exploration.
            if (s.return_node.valid()) {
                GridNodeId back = s.return_node;
                if (game.hacking().traverse_to(game, back)) {
                    game.log("Returning to host LAN.");
                    return;
                }
            }
            game.log("Disconnect channel...");
            game.hacking().jack_out(game, JackOutKind::Voluntary);
            return;
        }
        case GridTile::DataNode: {
            int credits = 5 + 5 * s.trace_tick_per_turn;
            s.loot.credits += credits;
            s.sector.set(s.avatar_x, s.avatar_y, GridTile::Floor);
            record_sector_mutation(game, s.avatar_x, s.avatar_y, GridTile::Floor);
            game.log("Data node ripped: +" + std::to_string(credits) + " credits.");
            return;
        }
        case GridTile::EncryptedFile:
            game.log("Encrypted file. Run decrypt.exe to read.");
            return;
        case GridTile::Gateway:
        case GridTile::DeepGridGateway:
            traverse_via_gateway(game, s);
            return;
        default:
            return;
    }
}

void show_help(Game& game) {
    game.log("Grid: hjkl/arrows move, '.' wait, 'f' fire program,");
    game.log("walk to ⊙ for safe disconnect, Shift+Q hard jack-out.");
}

void open_program_picker(Game& game, GridSession& s) {
    auto* slot = game.player().equipment.equipped_cyberdeck();
    if (!slot || !*slot || !(*slot)->deck) {
        game.log("No deck.");
        return;
    }
    auto& cd = *(*slot)->deck;

    int eff_slots = std::min(kCyberdeckMaxSlots,
                             cd.stats.slots + (s.skill_daemon_mastery ? 1 : 0));
    for (int i = 0; i < eff_slots; ++i) {
        if (cd.loaded[i].program_def_id == 0) continue;
        // Slot stores item_def_id; resolve to a ProgramId via the item registry.
        Item probe = build_by_def_id(cd.loaded[i].program_def_id);
        if (!probe.program) continue;
        ProgramId pid = probe.program->id;
        const auto* def = find_program(pid);
        if (!def) continue;
        if (def->kind == ProgramKind::Qh) continue;

        if (s.ram < def->ram_cost) {
            game.log("Not enough RAM for " + std::string(def->filename) +
                     " (" + std::to_string(def->ram_cost) + " required, " +
                     std::to_string(s.ram) + " available).");
            return;
        }
        s.ram -= def->ram_cost;

        int heat = def->heat_cost;
        if (s.skill_ghost_protocol && !s.ghost_protocol_used) {
            heat = 0;
            s.ghost_protocol_used = true;
        }
        cyberdeck_add_heat(cd, heat);

        GridProgramContext ctx{game, s, -1, -1};
        std::string msg = apply_program_in_grid(pid, ctx);
        game.log("[" + std::string(def->filename) + "] " + msg);
        return;
    }
    game.log("No Grid programs loaded. Slot a .exe in the PDA Hacking tab.");
}

} // namespace

bool handle(Game& game, int key) {
    auto* sess = game.hacking().session();
    if (!sess) return false;
    auto& s = *sess;

    auto move_with_step = [&](int dx, int dy) -> bool {
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
        case 'f': case 'F':       open_program_picker(game, s); return false;
        case 'Q':
            game.hacking().jack_out(game, JackOutKind::HardJackOut);
            return false;
        case 'q':
            if (s.sector.at(s.avatar_x, s.avatar_y) == GridTile::ExitNode) {
                game.log("You are on the exit node. Walk onto it to disconnect.");
            } else {
                game.log("(Shift+Q for hard jack-out; walk to ⊙ for safe exit.)");
            }
            return false;
        case '?':
            show_help(game);
            return false;
    }
    return false;
}

} // namespace astra::grid_input
