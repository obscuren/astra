// Plan 7 — `whoami` cyberdeck command. Prints the player's name (the
// operator handle).
//
// Distinct from the device-side `whoami` registered in cmd_universals.cpp,
// which prints `user@host`. The registry uses scope-aware find_for() so the
// right entry runs for the active context.

#include "astra/cyberdeck_shell_context.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/player.h"
#include "astra/shell_context.h"

namespace astra {

namespace {

HackCommandResult exec_whoami_cyber(const ParsedArgs&, ShellContext& ctx, Game& game) {
    auto* deck = ctx.as_cyberdeck();
    if (!deck) return {};
    deck->emit(game.player().name);
    return {};
}

const HackCommand k_whoami_cyber{
    "whoami",
    "whoami",
    "operator handle",
    CommandScope::Cyberdeck,
    HackTag::None, false, 0, 0, 0, false,
    &exec_whoami_cyber,
};

struct AutoRegister {
    AutoRegister() { register_hack_command(&k_whoami_cyber); }
};
const AutoRegister k_auto;

} // namespace

void register_whoami_cyber_command_anchor() { (void)&k_auto; }

} // namespace astra
