// Plan 7 — `jack <ip>` cyberdeck command. Queues a jack-in request that
// game_input.cpp pops to trigger the avatar upload.
//
// Plan 7 §15 / §17 A1: mod-gated. Without the Wireless Jack-In Module the
// command refuses. The §16 lock-check path is gone — mod gate is the only
// error before reachability.

#include "astra/cyberdeck_mods.h"
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

namespace astra {

namespace {

HackCommandResult exec_jack(const ParsedArgs& args, ShellContext& ctx, Game& game) {
    auto* deck = ctx.as_cyberdeck();
    if (!deck) return {};
    if (args.argv.size() < 2) {
        deck->emit("usage: jack <ip>", UITag::TextDim);
        return {};
    }
    auto parsed = parse_ip(args.argv[1]);
    if (!parsed) {
        deck->emit("jack: invalid IP '" + args.argv[1] + "'", UITag::TextDim);
        return {};
    }
    if (!player_has_skill(game.player(), SkillId::Cat_Hacking)) {
        deck->emit("jack: requires Cat_Hacking skill.", UITag::TextDim);
        return {};
    }
    // Plan 7 §15 / §17 A1: mod gate.
    if (!CyberdeckMods::wireless_jackin_installed(game.player())) {
        deck->emit("jack: no wireless jack-in device installed.", UITag::TextDim);
        deck->emit("       (requires Wireless Jack-In Module.)", UITag::TextDim);
        return {};
    }
    auto* h = game.world().find_hackable_by_ip(*parsed);
    if (!h) {
        deck->emit("jack: " + format_ip(*parsed) + ": host unreachable", UITag::TextDim);
        return {};
    }
    if (h->jack_in_node_id <= 0) {
        deck->emit("jack: target has no node id (not yet registered)", UITag::TextDim);
        return {};
    }
    game.pda_screen().hack_term_set_jack_in_request(
        static_cast<uint32_t>(h->jack_in_node_id));
    deck->emit(">> uploading consciousness... <<");
    return {};
}

const HackCommand k_jack{
    "jack",
    "jack <ip>",
    "jack into a node",
    CommandScope::Cyberdeck,
    HackTag::None, false, 0, 0, 0, false,
    &exec_jack,
};

struct AutoRegister {
    AutoRegister() { register_hack_command(&k_jack); }
};
const AutoRegister k_auto;

} // namespace

void register_jack_command_anchor() { (void)&k_auto; }

} // namespace astra
