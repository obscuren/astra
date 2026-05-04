// Plan 7 — `cat` cyberdeck command. Prints a program's full description from
// inventory, OR a decrypted lore archive (Plan 5 Cut 4).
//
// Cyberdeck-scope. The device-side `cat` (filesystem read) is registered in
// cmd_datastore_reads.cpp at Device-scope; the registry's find_for() picks
// the right entry by active context.

#include "astra/consciousness_save.h"
#include "astra/cyberdeck_shell_context.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/item.h"
#include "astra/player.h"
#include "astra/program.h"
#include "astra/shell_context.h"

#include <string>

namespace astra {

namespace {

HackCommandResult exec_cat_cyber(const ParsedArgs& args, ShellContext& ctx, Game& game) {
    auto* deck = ctx.as_cyberdeck();
    if (!deck) return {};
    if (args.argv.size() < 2) {
        deck->emit("usage: cat <filename-or-archive-id>", UITag::TextDim);
        return {};
    }
    const std::string& fname = args.argv[1];

    // Plan 5 Cut 4: try lore_archive first.
    {
        ConsciousnessSave cs;
        read_consciousness(cs);
        for (const auto& a : cs.lore_archive) {
            if (a.archive_id == fname) {
                deck->emit(">> archive: " + a.archive_id);
                deck->emit(">> origin: galaxy " + std::to_string(a.galaxy_seed_origin) +
                           ", tick " + std::to_string(a.world_tick_origin));
                deck->emit("");
                deck->emit("(lore body text — Plan 7)", UITag::TextDim);
                deck->emit("");
                deck->emit(">> end of archive.");
                return {};
            }
        }
    }

    // Existing logic (program inventory lookup).
    for (const auto& it : game.player().inventory.items) {
        if (it.type != ItemType::Program || !it.program) continue;
        const ProgramDef* def = find_program(it.program->id);
        if (!def || std::string(def->filename) != fname) continue;
        deck->emit(std::string(def->filename) + "  // " + def->name);
        deck->emit("  kind:      " + std::string(program_kind_name(def->kind)));
        deck->emit("  tier:      " + std::to_string(def->tier));
        deck->emit("  ram_cost:  " + std::to_string(def->ram_cost));
        if (def->kind == ProgramKind::Qh) {
            deck->emit("  detection: +" + std::to_string(def->detection_cost));
            if (!def->target_filter.empty()) {
                std::string targets;
                for (size_t i = 0; i < def->target_filter.size(); ++i) {
                    if (i > 0) targets += ", ";
                    targets += tag_set_describe(def->target_filter[i]);
                }
                deck->emit("  targets:   " + targets);
            }
        } else {
            deck->emit("  heat_cost: " + std::to_string(def->heat_cost));
        }
        deck->emit("");
        deck->emit(std::string(def->description), UITag::TextDim);
        return {};
    }
    deck->emit("cat: " + fname + ": no such file or archive.", UITag::TextDim);
    return {};
}

const HackCommand k_cat_cyber{
    "cat",
    "cat <filename-or-archive-id>",
    "show a program's description, or a lore archive",
    CommandScope::Cyberdeck,
    HackTag::None, false, 0, 0, 0, false,
    &exec_cat_cyber,
};

struct AutoRegister {
    AutoRegister() { register_hack_command(&k_cat_cyber); }
};
const AutoRegister k_auto;

} // namespace

void register_cat_cyber_command_anchor() { (void)&k_auto; }

} // namespace astra
