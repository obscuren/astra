// Plan 7 — `ls` cyberdeck command. Lists program (.qh / .ru / .ev) files in
// inventory.
//
// The device-side `ls` (filesystem read) is registered in
// cmd_datastore_reads.cpp at Device-scope; the registry's find_for() picks
// the right entry by active context, so both can share the name "ls".

#include "astra/cyberdeck_shell_context.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/item.h"
#include "astra/player.h"
#include "astra/program.h"
#include "astra/shell_context.h"

#include <algorithm>
#include <string>
#include <vector>

namespace astra {

namespace {

HackCommandResult exec_ls_cyber(const ParsedArgs& args, ShellContext& ctx, Game& game) {
    auto* deck = ctx.as_cyberdeck();
    if (!deck) return {};
    // Collect all program defs from inventory (preserve insertion order).
    std::vector<const ProgramDef*> defs;
    for (const auto& it : game.player().inventory.items) {
        if (it.type != ItemType::Program || !it.program) continue;
        const ProgramDef* def = find_program(it.program->id);
        if (def) defs.push_back(def);
    }
    if (defs.empty()) {
        deck->emit("  (no programs in inventory)", UITag::TextDim);
        return {};
    }

    // Long format if any arg looks like a flag containing 'l' (covers
    // -l, -la, -al, -ahl, etc. — same loose match real ls accepts).
    bool long_fmt = false;
    for (size_t i = 1; i < args.argv.size(); ++i) {
        if (!args.argv[i].empty() && args.argv[i][0] == '-' &&
            args.argv[i].find('l') != std::string::npos) {
            long_fmt = true;
            break;
        }
    }

    if (long_fmt) {
        for (const auto* def : defs) {
            std::string row = "  " + std::string(def->filename) + "  " +
                              program_kind_short(def->kind) + "  " +
                              std::to_string(def->ram_cost) + " RAM";
            if (def->kind == ProgramKind::Qh) {
                row += ", +" + std::to_string(def->detection_cost) + " Det";
            } else {
                row += ", " + std::to_string(def->heat_cost) + " Heat";
            }
            deck->emit(row);
        }
        return {};
    }

    // Default: column-grid layout.
    size_t col_w = 0;
    for (const auto* def : defs) {
        col_w = std::max(col_w, std::string(def->filename).size());
    }
    col_w += 2;  // gutter

    constexpr size_t kAssumedTermWidth = 70;
    size_t cols = std::max<size_t>(1, kAssumedTermWidth / col_w);

    std::string row;
    size_t in_row = 0;
    for (size_t i = 0; i < defs.size(); ++i) {
        std::string cell = defs[i]->filename;
        if (cell.size() < col_w) cell.append(col_w - cell.size(), ' ');
        row += cell;
        ++in_row;
        if (in_row >= cols || i + 1 == defs.size()) {
            while (!row.empty() && row.back() == ' ') row.pop_back();
            deck->emit(row);
            row.clear();
            in_row = 0;
        }
    }
    return {};
}

const HackCommand k_ls_cyber{
    "ls",
    "ls [-l]",
    "list programs in inventory",
    CommandScope::Cyberdeck,
    HackTag::None, false, 0, 0, 0, false,
    &exec_ls_cyber,
};

struct AutoRegister {
    AutoRegister() { register_hack_command(&k_ls_cyber); }
};
const AutoRegister k_auto;

} // namespace

void register_ls_cyber_command_anchor() { (void)&k_auto; }

} // namespace astra
