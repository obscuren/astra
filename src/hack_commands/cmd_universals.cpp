// Plan 7 Phase A — universal hack commands.
//
// One file because each is a tiny stub. Phase B will split per-command if any
// outgrow ~30 lines or need additional state. Each command self-registers via
// a static initializer; we anchor them with a single registration helper at
// the bottom so the linker can't dead-strip them.

#include "astra/cyberdeck_shell_context.h"
#include "astra/device_shell.h"
#include "astra/fixture_os_id.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/hackable.h"
#include "astra/hacking_system.h"
#include "astra/pda_screen.h"
#include "astra/shell_context.h"

namespace astra {

// Forward — implemented in cmd_cyberdeck_help.cpp / cmd_cyberdeck_exit.cpp
// once the cyberdeck-side migration is on. For now `help` and `exit` from the
// cyberdeck shell stay in pda_hacking_tab.cpp; this universals TU only owns
// the device-shell-facing universals + scope-aware fallbacks.

namespace {

HackCommandResult exec_help(const ParsedArgs&, ShellContext& ctx, Game&) {
    auto& reg = HackCommandRegistry::get();

    if (auto* dev = ctx.as_device()) {
        if (!dev->target()) return {};
        Hackable& target = *dev->target();
        DeviceShell& shell = *dev;
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

    if (auto* deck = ctx.as_cyberdeck()) {
        deck->emit("Commands:", UITag::TextDim);
        auto cmds = reg.commands_for(ctx);
        for (const auto* c : cmds) {
            std::string row = std::string("  ") + c->name + " — " + c->description;
            deck->emit(row, UITag::TextDim);
        }
        return {};
    }
    return {};
}

HackCommandResult exec_whoami(const ParsedArgs&, ShellContext& ctx, Game&) {
    auto* dev = ctx.as_device();
    if (!dev || !dev->target()) return {};
    Hackable& target = *dev->target();
    DeviceShell& shell = *dev;
    bool tier_root = (shell.tier() == ShellTier::Root);
    bool is_root = is_player_root(target, tier_root);
    const FixtureOsId& os = os_id_for(target.source_type);
    std::string user = is_root ? "root" : "guest";
    std::string out = user + "@" + os.os_name + "-" + os.version;
    shell.emit(out, UITag::TextDefault);
    return {};
}

HackCommandResult exec_clear(const ParsedArgs&, ShellContext& ctx, Game& game) {
    if (auto* dev = ctx.as_device()) {
        dev->clear_scroll();
        return {};
    }
    if (ctx.as_cyberdeck()) {
        // Cyberdeck-side: wipe scroll, then re-greet the equipped deck so
        // the player sees the banner again (the latched greeter would
        // otherwise stay quiet on subsequent draws).
        game.pda_screen().hack_term_clear_lines();
        game.pda_screen().hack_term_re_greet();
    }
    return {};
}

HackCommandResult exec_history(const ParsedArgs&, ShellContext& ctx, Game&) {
    if (auto* dev = ctx.as_device()) {
        int i = 1;
        for (const auto& h : dev->history()) {
            char buf[16];
            std::snprintf(buf, sizeof buf, "%4d  ", i++);
            dev->emit(std::string(buf) + h, UITag::TextDim);
        }
        return {};
    }
    if (auto* deck = ctx.as_cyberdeck()) {
        // The history Vec on ShellContext appends BEFORE the dispatch, so
        // the in-flight `history` line is already in the buffer — print it
        // verbatim. Format mirrors a real shell: leading index column.
        for (size_t i = 0; i < deck->history().size(); ++i) {
            deck->emit("  " + std::to_string(i) + "  " + deck->history()[i]);
        }
    }
    return {};
}

HackCommandResult exec_exit(const ParsedArgs&, ShellContext& ctx, Game& game) {
    if (ctx.as_device()) {
        // Pops the stack (which calls on_pop → close()), printing the logout
        // pair and (in real-world) yanking the cable. The caller path also
        // advances the world by one tick to charge for shell time.
        game.hacking().close_device_shell(game);
        return {};
    }
    if (ctx.as_cyberdeck()) {
        // Cyberdeck-side: clear the scroll AND close the PDA. The cyberdeck
        // context itself stays on the stack (it persists across PDA
        // open/close). Mirrors what Esc does on the PDA.
        game.pda_screen().hack_term_clear_lines();
        game.pda_screen().hack_term_close_pda();
    }
    return {};
}

const HackCommand k_help{
    "help",
    "help",
    "list commands available here",
    CommandScope::Universal,
    HackTag::None, false, 0, 0, 0, false,
    &exec_help,
};
const HackCommand k_whoami{
    "whoami",
    "whoami",
    "print current user@host",
    CommandScope::Device,    // device-only (cyberdeck has its own whoami in pda_hacking_tab)
    HackTag::None, false, 0, 0, 0, false,
    &exec_whoami,
};
const HackCommand k_clear{
    "clear",
    "clear",
    "clear the visible scroll",
    CommandScope::Universal,
    HackTag::None, false, 0, 0, 0, false,
    &exec_clear,
};
const HackCommand k_history{
    "history",
    "history",
    "print this session's command history",
    CommandScope::Universal,
    HackTag::None, false, 0, 0, 0, false,
    &exec_history,
};
const HackCommand k_exit{
    "exit",
    "exit",
    "close the shell (yank cable in real-world)",
    CommandScope::Universal,
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
