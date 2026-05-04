#pragma once

#include "astra/hackable.h"

#include <string>
#include <string_view>
#include <vector>

namespace astra {

class DeviceShell;
class Game;
struct Player;

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

// Forward — defined in hack_command.cpp + cmd_*.cpp.
struct HackCommand {
    const char* name              = "";   // "hashcat", "ssh", ...
    const char* synopsis          = "";   // one-line usage string
    const char* description       = "";   // multi-line for `<cmd> --help`
    HackTag     required_tag      = HackTag::None; // None = universal
    bool        requires_root     = false;
    int         base_turns        = 0;    // long-channel duration; 0 = instant
    int         base_heat         = 0;
    int         base_detection    = 0;
    bool        allow_partial     = false;

    // Execute fn. May start a long-channel via shell.start_channel(...).
    HackCommandResult (*execute)(const ParsedArgs& args,
                                 Hackable& target,
                                 DeviceShell& shell,
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

    // Returns commands whose required_tag is satisfied by `tags` AND whose
    // requires_root is satisfiable at the current tier. HackTag::None tags
    // are always included (universal).
    std::vector<const HackCommand*> commands_for(HackTagMask tags, bool is_root) const;

    // Universals only (HackTag::None).
    std::vector<const HackCommand*> universals(bool is_root) const;

private:
    std::vector<const HackCommand*> commands_;
};

void register_hack_command(const HackCommand* cmd);

// Helper — used by executes that want to know "is this player root on this device?"
bool is_player_root(const Hackable& target, bool shell_tier_root);

} // namespace astra
