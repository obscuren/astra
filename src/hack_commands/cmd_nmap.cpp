// Plan 7 — `nmap [-l|-m|-h]` cyberdeck command.
//
//   no-arg / -l / --list : text list of LAN nodes (IP, hostname, edge state,
//                          per-device tier + lock state, OS tag).
//   -m / --map           : opens the visual map widget on the PDA.
//   -h / --help          : usage.

#include "astra/cyberdeck_shell_context.h"
#include "astra/game.h"
#include "astra/grid_network.h"
#include "astra/hack_command.h"
#include "astra/hackable.h"
#include "astra/ip.h"
#include "astra/lan.h"
#include "astra/pda_screen.h"
#include "astra/shell_context.h"
#include "astra/world_manager.h"

#include <cstdio>
#include <string>

namespace astra {

namespace {

void emit_usage(CyberdeckShellContext& deck) {
    deck.emit("usage: nmap [-l|--list] [-m|--map] [-h|--help]", UITag::TextDim);
    deck.emit("  (no args) list nodes on the current LAN (default)", UITag::TextDim);
    deck.emit("  -l   list nodes on the current LAN", UITag::TextDim);
    deck.emit("  -m   open the visual map widget", UITag::TextDim);
}

void exec_list(CyberdeckShellContext& deck, Game& game) {
    const auto& world = game.world();
    const auto& meta = world.lan_metadata();
    if (meta.nodes_total <= 0 || !meta.lan_root.valid()) {
        deck.emit("nmap: no LAN on this map.", UITag::TextDim);
        return;
    }

    char header[160];
    std::snprintf(header, sizeof header,
                  "LAN: %s   (%s/24)   %d nodes, %d cracked",
                  meta.display_name.c_str(),
                  format_ip(meta.subnet_base).c_str(),
                  meta.nodes_total, meta.nodes_cracked);
    deck.emit(header);
    deck.emit("");
    // Plan 7 §17 A4: per-device tier + lock state in a single canonical
    // "tier:N (locked|cracked|unlocked)" column.
    deck.emit("  IP            HOST                            EDGE      DEVICE                 OS");

    const auto& net = world.grid_network();
    for (const auto& e : net.edges()) {
        if (e.from != meta.lan_root) continue;
        const GridNode* n = net.find(e.to);
        if (!n || n->kind != GridNodeKind::Subnet) continue;

        std::string edge_status;
        if (e.gateway_tier == 0)        edge_status = "open";
        else if (e.cracked)             edge_status = "cracked";
        else                            edge_status = "locked." + std::to_string(e.gateway_tier);

        std::string host = n->label;  // fallback: raw IP if lookup misses
        const Hackable* h = nullptr;
        if (auto parsed = parse_ip(n->label)) {
            h = world.find_hackable_by_ip(*parsed);
            if (h) host = lan_hostname(*h, meta);
        }

        std::string device_state;
        std::string os_slot;
        if (h) {
            int dtier = h->security_tier;
            const char* lock_label;
            if (h->escalated)                            lock_label = "cracked";
            else if (has_tag(h->tags, HackTag::Locked))  lock_label = "locked";
            else                                         lock_label = "unlocked";
            char ds[64];
            std::snprintf(ds, sizeof ds, "tier:%d (%s)", dtier, lock_label);
            device_state = ds;
            if (has_tag(h->tags, HackTag::AlienTech)) {
                os_slot = "??? (unknown)";
            } else {
                os_slot = tag_summary(h->tags);
            }
        } else {
            char ds[64];
            std::snprintf(ds, sizeof ds, "tier:%d (?)", n->security_tier);
            device_state = ds;
            os_slot = "?";
        }

        char line[256];
        std::snprintf(line, sizeof line, "  %-13s %-31s %-9s %-22s %s",
                      n->label.c_str(), host.c_str(),
                      edge_status.c_str(),
                      device_state.c_str(),
                      os_slot.c_str());
        deck.emit(line);
    }
    // Deep-grid gateway intentionally omitted — nmap is a local-LAN tool.
}

HackCommandResult exec_nmap(const ParsedArgs& args, ShellContext& ctx, Game& game) {
    auto* deck = ctx.as_cyberdeck();
    if (!deck) return {};

    // No args -> list (matches `-l`).
    if (args.argv.size() < 2) {
        exec_list(*deck, game);
        return {};
    }
    const std::string& flag = args.argv[1];
    if (flag == "-h" || flag == "--help") {
        emit_usage(*deck);
        return {};
    }
    if (flag == "-l" || flag == "--list") {
        exec_list(*deck, game);
        return {};
    }
    if (flag == "-m" || flag == "--map") {
        game.pda_screen().hack_term_open_nmap_widget();
        return {};
    }
    deck->emit("nmap: unknown flag '" + flag + "'; try -l, -m, or -h.", UITag::TextDim);
    return {};
}

const HackCommand k_nmap{
    "nmap",
    "nmap [-l|-m]",
    "list or map LAN nodes",
    CommandScope::Cyberdeck,
    HackTag::None, false, 0, 0, 0, false,
    &exec_nmap,
};

struct AutoRegister {
    AutoRegister() { register_hack_command(&k_nmap); }
};
const AutoRegister k_auto;

} // namespace

void register_nmap_command_anchor() { (void)&k_auto; }

} // namespace astra
