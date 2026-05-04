// Plan 7 — `ping` cyberdeck command. Free recon: probes a node by IP and
// prints latency, tier, and tag summary.

#include "astra/cyberdeck_shell_context.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/hackable.h"
#include "astra/ip.h"
#include "astra/shell_context.h"
#include "astra/world_manager.h"

#include <cstdio>
#include <string>

namespace astra {

namespace {

HackCommandResult exec_ping(const ParsedArgs& args, ShellContext& ctx, Game& game) {
    auto* deck = ctx.as_cyberdeck();
    if (!deck) return {};
    if (args.argv.size() < 2) {
        deck->emit("usage: ping <ip>", UITag::TextDim);
        return {};
    }
    auto parsed = parse_ip(args.argv[1]);
    if (!parsed) {
        deck->emit("ping: invalid IP '" + args.argv[1] + "'", UITag::TextDim);
        return {};
    }
    auto* h = game.world().find_hackable_by_ip(*parsed);
    if (!h) {
        deck->emit("ping: " + format_ip(*parsed) + ": host unreachable", UITag::TextDim);
        return {};
    }

    char line1[160], line2[160], line3[160];
    std::snprintf(line1, sizeof line1, "PING %s (%s):",
                  format_ip(*parsed).c_str(), tag_summary(h->tags));
    int latency = 1 + (static_cast<int>(*parsed) & 7);   // deterministic, cosmetic
    std::snprintf(line2, sizeof line2, "  64 bytes from %s: time=%dms",
                  format_ip(*parsed).c_str(), latency);
    std::snprintf(line3, sizeof line3, "  tier:    %d (%s)",
                  h->security_tier,
                  h->state == HackState::Compromised ? "compromised"
                  : h->state == HackState::Alarmed   ? "alarmed"
                                                    : "clean");
    deck->emit(line1);
    deck->emit(line2);
    deck->emit(line3);

    std::string tags_line = "  tags:    ";
    tags_line += tag_set_describe(h->tags);
    deck->emit(tags_line);
    return {};
}

const HackCommand k_ping{
    "ping",
    "ping <ip>",
    "probe a node (free recon)",
    CommandScope::Cyberdeck,
    HackTag::None, false, 0, 0, 0, false,
    &exec_ping,
};

struct AutoRegister {
    AutoRegister() { register_hack_command(&k_ping); }
};
const AutoRegister k_auto;

} // namespace

void register_ping_command_anchor() { (void)&k_auto; }

} // namespace astra
