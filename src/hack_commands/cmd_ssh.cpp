// Plan 7 — `ssh [<user>@]<ip>` cyberdeck command (deprecated).
//
// Spec 1 §11 Plan 7 dormancy: the ssh path no longer opens a device shell.
// The device-shell layer is dormant. Sigils are fired in the Grid; couple via
// a JackInPort fixture. This stub emits a deprecation message and returns.

#include "astra/cyberdeck_shell_context.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/hackable.h"
#include "astra/ip.h"
#include "astra/pda_screen.h"
#include "astra/player.h"
#include "astra/shell_context.h"
#include "astra/skill_defs.h"
#include "astra/world_manager.h"

#include <string>

namespace astra {

namespace {

HackCommandResult exec_ssh(const ParsedArgs& args, ShellContext& ctx, Game& game) {
    auto* deck = ctx.as_cyberdeck();
    if (!deck) return {};

    // Spec 1 §11: Plan 7 device-shell layer is dormant. The ssh path no
    // longer opens a shell. Sigils are fired in the Grid; couple via a
    // JackInPort fixture.
    deck->emit("ssh: deprecated. Sigils are fired in the Grid; couple via a JackInPort fixture.",
               UITag::TextDim);
    return {};
}

const HackCommand k_ssh{
    "ssh",
    "ssh [<user>@]<ip>",
    "open a device shell (default user: root)",
    CommandScope::Cyberdeck,
    HackTag::None, false, 0, 0, 0, false,
    &exec_ssh,
};

struct AutoRegister {
    AutoRegister() { register_hack_command(&k_ssh); }
};
const AutoRegister k_auto;

} // namespace

void register_ssh_command_anchor() { (void)&k_auto; }

} // namespace astra
