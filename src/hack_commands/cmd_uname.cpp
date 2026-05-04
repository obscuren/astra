// Plan 7 — `uname` cyberdeck command. Prints OS identity. With `-a`,
// includes the equipped deck and operator name.

#include "astra/cyberdeck_shell_context.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/item.h"
#include "astra/player.h"
#include "astra/shell_context.h"

#include <string>

namespace astra {

namespace {

HackCommandResult exec_uname(const ParsedArgs& args, ShellContext& ctx, Game& game) {
    auto* deck = ctx.as_cyberdeck();
    if (!deck) return {};
    bool full = false;
    for (size_t i = 1; i < args.argv.size(); ++i) {
        if (!args.argv[i].empty() && args.argv[i][0] == '-' &&
            args.argv[i].find('a') != std::string::npos) {
            full = true; break;
        }
    }
    auto* slot = game.player().equipment.equipped_cyberdeck();
    std::string deck_name = (slot && *slot) ? (*slot)->name : "no-deck";
    if (full) {
        deck->emit("astra-os 1.0 // " + deck_name + " // operator: " + game.player().name);
    } else {
        deck->emit("astra-os");
    }
    return {};
}

const HackCommand k_uname{
    "uname",
    "uname [-a]",
    "system identity",
    CommandScope::Cyberdeck,
    HackTag::None, false, 0, 0, 0, false,
    &exec_uname,
};

struct AutoRegister {
    AutoRegister() { register_hack_command(&k_uname); }
};
const AutoRegister k_auto;

} // namespace

void register_uname_command_anchor() { (void)&k_auto; }

} // namespace astra
