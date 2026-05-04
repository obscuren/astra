// Plan 7 — `lore` cyberdeck command. Lists decrypted lore archives.
//
// Cyberdeck-scope. Reads the consciousness save (Plan 5 Cut 4 lore_archive).
// Pure-instant — no channel, no world tick.

#include "astra/consciousness_save.h"
#include "astra/cyberdeck_shell_context.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/shell_context.h"

#include <cstdio>

namespace astra {

namespace {

HackCommandResult exec_lore(const ParsedArgs&, ShellContext& ctx, Game&) {
    auto* deck = ctx.as_cyberdeck();
    if (!deck) return {};

    ConsciousnessSave cs;
    read_consciousness(cs);

    if (cs.lore_archive.empty()) {
        deck->emit("no decrypted archives.", UITag::TextDim);
        return {};
    }

    deck->emit("decrypted archives:");
    for (const auto& a : cs.lore_archive) {
        char line[160];
        std::snprintf(line, sizeof line, "  %-24s  (origin: tick %d)",
                      a.archive_id.c_str(),
                      static_cast<int>(a.world_tick_origin));
        deck->emit(line);
    }
    deck->emit("");
    deck->emit("use:  cat <archive-id>", UITag::TextDim);
    return {};
}

const HackCommand k_lore{
    "lore",
    "lore",
    "list decrypted lore archives",
    CommandScope::Cyberdeck,
    HackTag::None, false, 0, 0, 0, false,
    &exec_lore,
};

struct AutoRegister {
    AutoRegister() { register_hack_command(&k_lore); }
};
const AutoRegister k_auto;

} // namespace

void register_lore_command_anchor() { (void)&k_auto; }

} // namespace astra
