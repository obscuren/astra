// Plan 7 Phase A — universal hack commands.
//
// One file because each is a tiny stub. Phase B will split per-command if any
// outgrow ~30 lines or need additional state. Each command self-registers via
// a static initializer; we anchor them with a single registration helper at
// the bottom so the linker can't dead-strip them.

#include "astra/device_shell.h"
#include "astra/fixture_os_id.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/hackable.h"

namespace astra {

namespace {

HackCommandResult exec_help(const ParsedArgs&, Hackable& target,
                            DeviceShell& shell, Game&) {
    auto& reg = HackCommandRegistry::get();
    bool tier_root = (shell.tier() == ShellTier::Root);
    bool is_root = is_player_root(target, tier_root);

    shell.emit("Commands available on this device:", UITag::TextDim);
    auto cmds = reg.commands_for(target.tags, is_root);
    for (const auto* c : cmds) {
        std::string row = std::string("  ") + c->name + " - " + c->description;
        shell.emit(row, UITag::TextDim);
    }
    shell.emit("Tip: <cmd> --help shows scaled cost.", UITag::TextDim);
    return {};
}

HackCommandResult exec_whoami(const ParsedArgs&, Hackable& target,
                              DeviceShell& shell, Game&) {
    bool tier_root = (shell.tier() == ShellTier::Root);
    bool is_root = is_player_root(target, tier_root);
    const FixtureOsId& os = os_id_for(target.source_type);
    std::string user = is_root ? "root" : "guest";
    std::string out = user + "@" + os.os_name + "-" + os.version;
    shell.emit(out, UITag::TextDefault);
    return {};
}

HackCommandResult exec_clear(const ParsedArgs&, Hackable&,
                             DeviceShell& shell, Game&) {
    shell.clear_scroll();
    return {};
}

HackCommandResult exec_history(const ParsedArgs&, Hackable&,
                               DeviceShell& shell, Game&) {
    int i = 1;
    for (const auto& h : shell.history()) {
        char buf[16];
        std::snprintf(buf, sizeof buf, "%4d  ", i++);
        shell.emit(std::string(buf) + h, UITag::TextDim);
    }
    return {};
}

HackCommandResult exec_exit(const ParsedArgs&, Hackable&,
                            DeviceShell& shell, Game& game) {
    shell.close(game);
    return {};
}

const HackCommand k_help{
    "help",
    "help",
    "list commands available here",
    HackTag::None, false, 0, 0, 0, false,
    &exec_help,
};
const HackCommand k_whoami{
    "whoami",
    "whoami",
    "print current user@host",
    HackTag::None, false, 0, 0, 0, false,
    &exec_whoami,
};
const HackCommand k_clear{
    "clear",
    "clear",
    "clear the visible scroll",
    HackTag::None, false, 0, 0, 0, false,
    &exec_clear,
};
const HackCommand k_history{
    "history",
    "history",
    "print this session's command history",
    HackTag::None, false, 0, 0, 0, false,
    &exec_history,
};
const HackCommand k_exit{
    "exit",
    "exit",
    "close the shell (yank cable in real-world)",
    HackTag::None, false, 0, 0, 0, false,
    &exec_exit,
};

struct AutoRegister {
    AutoRegister() {
        register_hack_command(&k_help);
        register_hack_command(&k_whoami);
        register_hack_command(&k_clear);
        register_hack_command(&k_history);
        register_hack_command(&k_exit);
    }
};
const AutoRegister k_auto_register;

} // namespace

// Linker anchor — referenced from a TU we know is always linked, to keep the
// static initializer above from being elided in a release build.
void register_universal_hack_commands() {
    (void)&k_auto_register;
}

} // namespace astra
