// Plan 7 — `deck info` cyberdeck command. Prints the equipped deck's stats.
//
// Cyberdeck-scope. The user types `deck info` (two tokens); we register the
// command name as `deck` and check the second arg ourselves so the registry
// dispatch stays one-key-one-cmd.

#include "astra/cyberdeck.h"
#include "astra/cyberdeck_shell_context.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/item.h"
#include "astra/player.h"
#include "astra/shell_context.h"

#include <string>

namespace astra {

namespace {

HackCommandResult exec_deck(const ParsedArgs& args, ShellContext& ctx, Game& game) {
    auto* deck_ctx = ctx.as_cyberdeck();
    if (!deck_ctx) return {};
    if (args.argv.size() < 2 || args.argv[1] != "info") {
        deck_ctx->emit("usage: deck info", UITag::TextDim);
        return {};
    }
    auto* slot = game.player().equipment.equipped_cyberdeck();
    if (!slot || !*slot || !(*slot)->deck) {
        deck_ctx->emit("no deck equipped.", UITag::TextDim);
        return {};
    }
    auto& d = *(*slot)->deck;
    deck_ctx->emit("Deck: " + (*slot)->name);
    deck_ctx->emit("  RAM " + std::to_string(d.ram_current) + "/" + std::to_string(d.stats.ram_max));
    deck_ctx->emit("  CPU " + std::to_string(d.stats.cpu));
    deck_ctx->emit("  SLOTS " + std::to_string(d.stats.slots));
    deck_ctx->emit("  STEALTH +" + std::to_string(d.stats.stealth));
    deck_ctx->emit("  COOLING " + std::to_string(d.stats.cooling_rate) + "/turn");
    deck_ctx->emit("  HEAT_CAP " + std::to_string(d.stats.heat_cap));
    return {};
}

const HackCommand k_deck{
    "deck",
    "deck info",
    "deck stats",
    CommandScope::Cyberdeck,
    HackTag::None, false, 0, 0, 0, false,
    &exec_deck,
};

struct AutoRegister {
    AutoRegister() { register_hack_command(&k_deck); }
};
const AutoRegister k_auto;

} // namespace

void register_deck_info_command_anchor() { (void)&k_auto; }

} // namespace astra
