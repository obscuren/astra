// Plan 7 — `ssh [<user>@]<ip>` cyberdeck command. Opens a per-device shell.
//
// Manual ssh strict semantics (spec §4): root@locked-unescalated rejects with
// permission-denied + try-guest hint and DOES NOT open. guest@ always succeeds.
// Spec §16: AlienTech-tagged devices reject with "protocol not understood
// (alien tech)" — no shell opens.

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

#include <string>

namespace astra {

namespace {

HackCommandResult exec_ssh(const ParsedArgs& args, ShellContext& ctx, Game& game) {
    auto* deck = ctx.as_cyberdeck();
    if (!deck) return {};
    if (args.argv.size() < 2) {
        deck->emit("usage: ssh [<user>@]<ip>", UITag::TextDim);
        return {};
    }
    if (!player_has_skill(game.player(), SkillId::Cat_Hacking)) {
        deck->emit("ssh: requires Cat_Hacking skill.", UITag::TextDim);
        return {};
    }

    // Parse `[user@]ip`.
    std::string user = "root";
    std::string ip_str = args.argv[1];
    if (auto at = ip_str.find('@'); at != std::string::npos) {
        user   = ip_str.substr(0, at);
        ip_str = ip_str.substr(at + 1);
    }
    auto parsed = parse_ip(ip_str);
    if (!parsed) {
        deck->emit("ssh: invalid IP '" + ip_str + "'", UITag::TextDim);
        return {};
    }
    const auto* h = game.world().find_hackable_by_ip(*parsed);
    if (!h) {
        deck->emit("ssh: " + format_ip(*parsed) + ": host unreachable", UITag::TextDim);
        return {};
    }

    // Plan 7 §16 — AlienTech opt-out.
    if (has_tag(h->tags, HackTag::AlienTech)) {
        deck->emit("ssh: " + format_ip(*parsed) +
                   ": protocol not understood (alien tech).",
                   UITag::TextDim);
        return {};
    }

    bool wants_root = (user == "root");
    bool locked = has_tag(h->tags, HackTag::Locked);
    if (wants_root && locked && !h->escalated) {
        // Strict reject: permission-denied + try-guest hint. No shell opens.
        deck->emit("ssh: " + format_ip(*parsed) +
                   ": permission denied (root login disabled).",
                   UITag::TextDim);
        deck->emit("      try: ssh guest@" + format_ip(*parsed),
                   UITag::TextDim);
        return {};
    }

    // Queue the shell-open request for game_input.cpp to consume.
    game.pda_screen().hack_term_set_ssh_request(
        static_cast<uint32_t>(*parsed), wants_root);
    deck->emit("ssh: connecting to " + format_ip(*parsed) + " as " + user + "...",
               UITag::TextDim);
    return {};
}

const HackCommand k_ssh{
    "ssh",
    "ssh [<user>@]<ip>",
    "open a device shell (default user: root)",
    CommandScope::Cyberdeck,
    HackTag::None, false, 0, 0, 0, false,
    &exec_ssh,
};

struct AutoRegister {
    AutoRegister() { register_hack_command(&k_ssh); }
};
const AutoRegister k_auto;

} // namespace

void register_ssh_command_anchor() { (void)&k_auto; }

} // namespace astra
