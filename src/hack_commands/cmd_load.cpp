// Plan 7 — `load` cyberdeck command. Loads a program from inventory into a
// deck slot, swapping out any existing occupant.

#include "astra/cyberdeck.h"
#include "astra/cyberdeck_shell_context.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/item.h"
#include "astra/item_defs.h"
#include "astra/player.h"
#include "astra/program.h"
#include "astra/shell_context.h"

#include <string>

namespace astra {

namespace {

HackCommandResult exec_load(const ParsedArgs& args, ShellContext& ctx, Game& game) {
    auto* deck = ctx.as_cyberdeck();
    if (!deck) return {};
    if (args.argv.size() < 3) {
        deck->emit("usage: load <slot> <id>", UITag::TextDim);
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
    int inv_idx = -1;
    for (size_t i = 0; i < game.player().inventory.items.size(); ++i) {
        const auto& it = game.player().inventory.items[i];
        if (it.type != ItemType::Program || !it.program) continue;
        const ProgramDef* def = find_program(it.program->id);
        if (def && std::string(def->filename) == args.argv[2]) {
            inv_idx = static_cast<int>(i);
            break;
        }
    }
    if (inv_idx < 0) {
        deck->emit("no such program in inventory.", UITag::TextDim);
        return {};
    }
    // No-op when the slot already holds the requested program.
    uint16_t inv_def_id = game.player().inventory.items[inv_idx].item_def_id;
    if (d.loaded[slot].program_def_id == inv_def_id) {
        deck->emit("slot " + std::to_string(slot) +
                   " already holds " + args.argv[2] + ".", UITag::TextDim);
        return {};
    }
    // Unload current occupant back to inventory.
    if (d.loaded[slot].program_def_id != 0) {
        Item old = build_by_def_id(d.loaded[slot].program_def_id);
        game.player().inventory.items.push_back(std::move(old));
        d.loaded[slot].program_def_id = 0;
    }
    d.loaded[slot].program_def_id = inv_def_id;
    game.player().inventory.items.erase(game.player().inventory.items.begin() + inv_idx);
    deck->emit("loaded " + args.argv[2] + " into slot " + std::to_string(slot) + ".",
               UITag::TextDefault);
    return {};
}

const HackCommand k_load{
    "load",
    "load <slot> <id>",
    "load a program into a slot",
    CommandScope::Cyberdeck,
    HackTag::None, false, 0, 0, 0, false,
    &exec_load,
};

struct AutoRegister {
    AutoRegister() { register_hack_command(&k_load); }
};
const AutoRegister k_auto;

} // namespace

void register_load_command_anchor() { (void)&k_auto; }

} // namespace astra
