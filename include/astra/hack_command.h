#pragma once

#include "astra/hackable.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace astra {

class DeviceShell;
class Game;
struct Player;
class ShellContext;

// Parsed shell command tokens.
struct ParsedArgs {
    std::string raw;                 // full original line (less trailing whitespace)
    std::vector<std::string> argv;   // tokens; argv[0] is the command name
    bool wants_help = false;         // any of {-h, --help}

    bool has_flag(std::string_view name) const;
    std::string_view value_of(std::string_view name) const; // --name=value or --name value
};

ParsedArgs parse_command_line(const std::string& line);

// Result of executing a HackCommand.
struct HackCommandResult {
    bool ok = true;          // false = command rejected (bad args, perm, etc.)
    bool started_channel = false; // true if a long-channel was opened (DeviceShell tracks state)
    std::string message;     // single-line summary printed to the shell
};

// Scope filters which contexts a command is valid in. The cyberdeck shell
// (pda> ...) and the per-device ssh shell (TUR-OS:root# ...) share the
// registry; scope is what keeps `nmap` from showing in a device shell and
// `hashcat` from showing in pda>.
enum class CommandScope : std::uint8_t {
    Cyberdeck,    // valid only when active ctx is CyberdeckShellContext
    Device,       // valid only when active ctx is DeviceShellContext
    Universal,    // valid in both (clear, help, history, exit)
};

// Forward — defined in hack_command.cpp + cmd_*.cpp.
struct HackCommand {
    const char* name              = "";   // "hashcat", "ssh", ...
    const char* synopsis          = "";   // one-line usage string
    const char* description       = "";   // multi-line for `<cmd> --help`
    CommandScope scope            = CommandScope::Device;  // Device by default — most existing cmds.
    HackTag     required_tag      = HackTag::None; // None = universal (Device only)
    bool        requires_root     = false;          // Device only
    int         base_turns        = 0;    // long-channel duration; 0 = instant
    int         base_heat         = 0;
    int         base_detection    = 0;
    bool        allow_partial     = false;

    // Execute fn. May start a long-channel on a DeviceShellContext.
    // Cyberdeck-scope commands cast ctx.as_cyberdeck(); device-scope cast
    // ctx.as_device(); universal commands inspect ctx.as_*() to branch.
    HackCommandResult (*execute)(const ParsedArgs& args,
                                 ShellContext& ctx,
                                 Game& game) = nullptr;
};

// Scaled cost — single source of truth for runtime + `--help` rendering.
// Formula:
//   scaled_turns      = max(1, base_turns × (1 - 0.05*INT_mod) × skill_factor)
//   scaled_heat       = max(0, round(base_heat × (1 - 0.04*INT_mod) × skill_factor))
//   scaled_detection  = max(0, round(base_detection × cold_hands_factor))
struct ScaledCost {
    int turns = 0;
    int heat = 0;
    int detection = 0;
};
ScaledCost scaled_cost(const HackCommand& cmd, const Player& player);

// Static-init friendly registry. Each cmd_*.cpp constructs a HackCommand
// and calls register_hack_command() in a static initializer.
class HackCommandRegistry {
public:
    static HackCommandRegistry& get();

    void add(const HackCommand* cmd);
    const HackCommand* find(std::string_view name) const;

    // Scope-aware lookup. With `ls` and `clear` registered both as
    // CommandScope::Device and CommandScope::Cyberdeck (different bodies),
    // a plain `find(name)` would always return the first match. Callers
    // that know the active ShellContext should use this overload so the
    // dispatch picks the right entry. Universal commands match any context.
    const HackCommand* find_for(std::string_view name, const ShellContext& ctx) const;

    // Device-shell filter: returns commands whose scope is Device or
    // Universal, required_tag is satisfied by `tags`, and requires_root is
    // satisfiable at the current tier. HackTag::None tags are always
    // included.
    std::vector<const HackCommand*> commands_for(HackTagMask tags, bool is_root) const;

    // Universals only (HackTag::None).
    std::vector<const HackCommand*> universals(bool is_root) const;

    // Polymorphic filter — returns the commands valid in the current
    // ShellContext (scope + tag/tier checks for Device contexts). Used by
    // `help`, dispatch, and tab-completion.
    std::vector<const HackCommand*> commands_for(const ShellContext& ctx) const;

    // Cyberdeck-scope (or Universal) commands only. Used by the cyberdeck
    // shell's help and dispatch.
    std::vector<const HackCommand*> cyberdeck_commands() const;

private:
    std::vector<const HackCommand*> commands_;
};

void register_hack_command(const HackCommand* cmd);

// Helper — used by executes that want to know "is this player root on this device?"
bool is_player_root(const Hackable& target, bool shell_tier_root);

} // namespace astra
