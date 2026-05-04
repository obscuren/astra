// Plan 7 — `unload` cyberdeck command. Removes a program from a deck slot,
// returning it to inventory.

#include "astra/cyberdeck.h"
#include "astra/cyberdeck_shell_context.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/item.h"
#include "astra/item_defs.h"
#include "astra/player.h"
#include "astra/shell_context.h"

#include <string>

namespace astra {

namespace {

HackCommandResult exec_unload(const ParsedArgs& args, ShellContext& ctx, Game& game) {
    auto* deck = ctx.as_cyberdeck();
    if (!deck) return {};
    if (args.argv.size() < 2) {
        deck->emit("usage: unload <slot>", UITag::TextDim);
        return {};
    }
    auto* slot_p = game.player().equipment.equipped_cyberdeck();
    if (!slot_p || !*slot_p || !(*slot_p)->deck) {
        deck->emit("no deck equipped.", UITag::TextDim);
        return {};
    }
    auto& d = *(*slot_p)->deck;
    int slot = -1;
    try { slot = std::stoi(args.argv[1]); } catch (...) {}
    if (slot < 0 || slot >= d.stats.slots) {
        deck->emit("bad slot.", UITag::TextDim);
        return {};
    }
    if (d.loaded[slot].program_def_id == 0) {
        deck->emit("slot already empty.", UITag::TextDim);
        return {};
    }
    Item old = build_by_def_id(d.loaded[slot].program_def_id);
    game.player().inventory.items.push_back(std::move(old));
    d.loaded[slot].program_def_id = 0;
    deck->emit("unloaded slot " + std::to_string(slot) + ".", UITag::TextDefault);
    return {};
}

const HackCommand k_unload{
    "unload",
    "unload <slot>",
    "unload a slot",
    CommandScope::Cyberdeck,
    HackTag::None, false, 0, 0, 0, false,
    &exec_unload,
};

struct AutoRegister {
    AutoRegister() { register_hack_command(&k_unload); }
};
const AutoRegister k_auto;

} // namespace

void register_unload_command_anchor() { (void)&k_auto; }

} // namespace astra
