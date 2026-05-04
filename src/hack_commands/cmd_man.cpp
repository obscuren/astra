// Plan 7 — `man` cyberdeck command. Shows the manual page for a command.
//
// Pages are static text, indexed by command name. The cyberdeck-side `man`
// is informational only; the device-side per-command help is rendered via
// `<cmd> --help` (see DeviceShell::render_help_for_).

#include "astra/cyberdeck_shell_context.h"
#include "astra/hack_command.h"
#include "astra/shell_context.h"

#include <string>

namespace astra {

namespace {

HackCommandResult exec_man(const ParsedArgs& args, ShellContext& ctx, Game&) {
    auto* deck = ctx.as_cyberdeck();
    if (!deck) return {};
    if (args.argv.size() < 2) {
        deck->emit("usage: man <command>", UITag::TextDim);
        return {};
    }
    const std::string& topic = args.argv[1];
    struct Page { const char* cmd; const char* lines[6]; };
    static const Page pages[] = {
        {"help",   {"NAME", "  help — list available commands.", "", "SEE ALSO", "  man <command>", nullptr}},
        {"man",    {"NAME", "  man — show the manual page for a command.", "", "USAGE", "  man <command>", nullptr}},
        {"deck",   {"NAME", "  deck info — print the equipped deck's stats.", "", "ENVIRONMENT", "  Requires an equipped cyberdeck.", nullptr}},
        {"ps",     {"NAME", "  ps — list programs loaded into deck slots.", "", "FLAGS", "  -a, aux : extended ps-style listing", nullptr}},
        {"ls",     {"NAME", "  ls — list programs in inventory.", "", "FLAGS", "  -l : long format (one row per program)", nullptr}},
        {"load",   {"NAME", "  load — load a program from inventory into a deck slot.", "", "USAGE", "  load <slot> <filename>", nullptr}},
        {"unload", {"NAME", "  unload — remove a program from a deck slot.", "", "USAGE", "  unload <slot>", nullptr}},
        {"cat",    {"NAME", "  cat — print a program's full description.", "", "USAGE", "  cat <filename>", nullptr}},
        {"echo",   {"NAME", "  echo — print arguments to the terminal.", "", "USAGE", "  echo <text...>", nullptr}},
        {"uname",  {"NAME", "  uname — print system identification.", "", "FLAGS", "  -a : full identity (deck + version + operator)", nullptr}},
        {"whoami", {"NAME", "  whoami — print the current operator handle.", nullptr, nullptr, nullptr, nullptr}},
        {"ping",   {"NAME", "  ping — probe a node (free recon).", "", "USAGE", "  ping <ip>", nullptr}},
        {"nmap",   {"NAME", "  nmap — list or map nodes on the current LAN.", "", "FLAGS", "  -l/--list : text list   -m/--map : visual widget", nullptr}},
        {"jack",   {"NAME", "  jack — jack into a node.", "", "USAGE", "  jack <ip>", nullptr}},
        {"lore",   {"NAME", "  lore — list decrypted lore archives.", "", "OUTPUT", "  archive ids + origin tick. Use 'cat <archive-id>' to read.", nullptr}},
        {"clear",  {"NAME", "  clear — wipe the scrollback and re-greet.", nullptr, nullptr, nullptr, nullptr}},
        {"history",{"NAME", "  history — replay this session's command history.", nullptr, nullptr, nullptr, nullptr}},
    };
    for (const auto& p : pages) {
        if (topic == p.cmd) {
            for (auto* s : p.lines) {
                if (!s) break;
                deck->emit(s);
            }
            return {};
        }
    }
    deck->emit("No manual entry for '" + topic + "'.", UITag::TextDim);
    return {};
}

const HackCommand k_man{
    "man",
    "man <command>",
    "manual page for a command",
    CommandScope::Cyberdeck,
    HackTag::None, false, 0, 0, 0, false,
    &exec_man,
};

struct AutoRegister {
    AutoRegister() { register_hack_command(&k_man); }
};
const AutoRegister k_auto;

} // namespace

void register_man_command_anchor() { (void)&k_auto; }

} // namespace astra
