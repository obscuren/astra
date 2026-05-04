// Plan 7 — `ps` cyberdeck command. Lists programs loaded into deck slots.
//
// `-a` / `aux` enable an extended ps-style listing. Default is one row per
// slot with filename + RAM/heat costs.

#include "astra/cyberdeck.h"
#include "astra/cyberdeck_shell_context.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/item.h"
#include "astra/item_defs.h"
#include "astra/player.h"
#include "astra/program.h"
#include "astra/shell_context.h"

#include <algorithm>
#include <string>

namespace astra {

namespace {

HackCommandResult exec_ps(const ParsedArgs& args, ShellContext& ctx, Game& game) {
    auto* deck = ctx.as_cyberdeck();
    if (!deck) return {};
    auto* slot = game.player().equipment.equipped_cyberdeck();
    if (!slot || !*slot || !(*slot)->deck) {
        deck->emit("no deck equipped.", UITag::TextDim);
        return {};
    }
    auto& d = *(*slot)->deck;

    bool extended = false;
    for (size_t i = 1; i < args.argv.size(); ++i) {
        if (args.argv[i] == "aux" ||
            (args.argv[i].size() >= 2 && args.argv[i][0] == '-' &&
             args.argv[i].find('a') != std::string::npos)) {
            extended = true; break;
        }
    }
    if (extended) {
        deck->emit("USER  PID  STAT  RAM   HEAT  PROGRAM", UITag::TextDim);
    }
    int active = 0;
    for (int i = 0; i < d.stats.slots; ++i) {
        if (d.loaded[i].program_def_id == 0) {
            if (extended) {
                deck->emit("op    --   ----  --    --    [" + std::to_string(i) + "] (empty)",
                           UITag::TextDim);
            } else {
                deck->emit("  [" + std::to_string(i) + "] (empty)");
            }
            continue;
        }
        Item probe = build_by_def_id(d.loaded[i].program_def_id);
        const ProgramDef* def = probe.program ? find_program(probe.program->id) : nullptr;
        if (!def) {
            deck->emit("  [" + std::to_string(i) + "] ???");
            continue;
        }
        ++active;
        if (extended) {
            std::string pid = std::string(2 - std::min<size_t>(2, std::to_string(i).size()), '0') +
                              std::to_string(i);
            std::string row = "op    " + pid + "   S     " +
                              std::to_string(def->ram_cost) + "     " +
                              std::to_string(def->heat_cost) + "     " +
                              def->filename + "  " + program_kind_short(def->kind);
            deck->emit(row);
        } else {
            std::string row = "  [" + std::to_string(i) + "] " + def->filename + "  " +
                              program_kind_short(def->kind) + "  " +
                              std::to_string(def->ram_cost) + " RAM, " +
                              std::to_string(def->heat_cost) + " Heat";
            deck->emit(row);
        }
    }
    if (extended) {
        deck->emit("--", UITag::TextDim);
        deck->emit(std::to_string(active) + " active / " +
                   std::to_string(d.stats.slots) + " slots // RAM " +
                   std::to_string(d.ram_current) + "/" +
                   std::to_string(d.stats.ram_max), UITag::TextDim);
    }
    return {};
}

const HackCommand k_ps{
    "ps",
    "ps [-a]",
    "list loaded programs (running in RAM)",
    CommandScope::Cyberdeck,
    HackTag::None, false, 0, 0, 0, false,
    &exec_ps,
};

struct AutoRegister {
    AutoRegister() { register_hack_command(&k_ps); }
};
const AutoRegister k_auto;

} // namespace

void register_ps_command_anchor() { (void)&k_auto; }

} // namespace astra
