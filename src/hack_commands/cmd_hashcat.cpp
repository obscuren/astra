// Plan 7 Phase B — `hashcat` escalation long-channel.
//
// Long-channel that flips Hackable.escalated to true on success. Partial
// state on abort: increments cracked_digits by 1..2 (resumes next attempt).
// Skill check formula (per plan-7):
//   1d100 <= 50 + 5*INT_mod + 10*Cat_Hacking_rank + 15*RootKit_rank
// RootKit doesn't exist in the skill tree yet (Phase B-2 lands it); we
// hard-code its rank to 0.

#include "astra/device_shell.h"
#include "astra/dice.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/hackable.h"
#include "astra/player.h"
#include "astra/skill_defs.h"

#include <cstdio>
#include <random>

namespace astra {

namespace {

int int_mod(const Player& p) { return (p.attributes.intelligence - 10) / 2; }

int cat_hacking_rank(const Player& p) {
    return player_has_skill(p, SkillId::Cat_Hacking) ? 1 : 0;
}

int rootkit_rank(const Player&) {
    // RootKit ships in Phase B-2. Treat as 0 for now.
    return 0;
}

bool roll_success(const Player& p, std::mt19937_64& rng) {
    int dc = 50 + 5 * int_mod(p)
                + 10 * cat_hacking_rank(p)
                + 15 * rootkit_rank(p);
    if (dc < 5)  dc = 5;
    if (dc > 95) dc = 95;
    int roll = static_cast<int>((rng() % 100) + 1);
    return roll <= dc;
}

HackCommandResult exec_hashcat(const ParsedArgs& args, Hackable& target,
                               DeviceShell& shell, Game& game) {
    if (!has_tag(target.tags, HackTag::Locked)) {
        return {false, false, "hashcat: target is not locked."};
    }
    if (target.escalated) {
        return {false, false, "hashcat: already root."};
    }

    // Show partial-state preview if any digits cracked.
    if (target.cracked_digits > 0) {
        char buf[64];
        // Render N digits revealed out of 11 (cosmetic length).
        std::string mask;
        const int total = 11;
        for (int i = 0; i < total; ++i) {
            if (i < target.cracked_digits) mask += "0123456789ABC"[i % 13];
            else mask += '*';
        }
        std::snprintf(buf, sizeof buf, "[+] Recovered: %s", mask.c_str());
        shell.emit(buf, UITag::TextSuccess);
    }

    // Find the registered HackCommand for ourselves so start_channel sees the
    // full descriptor (cost numbers).
    const HackCommand* self = HackCommandRegistry::get().find("hashcat");
    if (!self) return {false, false, "hashcat: registry miss."};
    if (!shell.start_channel(*self, args, game)) {
        return {false, false, ""};
    }
    return {true, true, ""};
}

// Channel completion / abort handler. Phase A's tick_world routes completion
// to execute() — but for hashcat we need an "on success" path that runs at
// 100%. So our execute() above handles "start"; the world-tick completion
// path will re-call execute() at 100% which we detect via channel state.
//
// To avoid double-channel, we route completion logic via a separate function
// that DeviceShell calls when the channel finishes. Phase A's design routes
// completion through cmd->execute() with the same args — so we need a marker.
// We use args.has_flag("--complete") added by the channel runtime? No — keep
// it simple: detect completion by checking active-channel == nullptr at top.
//
// Actually: the channel runtime in DeviceShell::tick_world() runs:
//   channel_ = HackChannel{}; cmd->execute(args, target, *this, game)
// So when execute() is called from the completion path, channel is already
// cleared. We can detect "we are running as the completion handler" by
// inspecting shell.channel_active() == false AND the args being the same.
// Simpler: provide a `--__complete` synthetic flag set by the runtime. Since
// runtime doesn't do that, we use a sentinel in the registry.
//
// Cleanest: add an `on_complete` hook to HackCommand. Out of scope for this
// task budget. Instead: in execute(), if there's no active channel (we just
// finished one), apply the success roll.

HackCommandResult exec_hashcat_complete(Hackable& target, DeviceShell& shell,
                                        Game& game) {
    std::mt19937_64 rng(static_cast<uint64_t>(game.world().world_tick()) ^
                        static_cast<uint64_t>(target.ip) * 0x9E37u ^
                        static_cast<uint64_t>(target.cracked_digits) * 0xBF58u);
    bool ok = roll_success(game.player(), rng);
    if (ok) {
        target.escalated = true;
        target.cracked_digits = 11;
        shell.emit("[+] Recovered: full passphrase.", UITag::TextSuccess);
        shell.emit("[+] Authentication elevated. Reconnect or run `whoami`.",
                   UITag::TextSuccess);
        // Note: we don't auto-promote shell tier mid-session; spec §4 requires
        // a re-issue of `ssh root@` for that. Tier escalation in-session is
        // deferred to Phase C polish.
        return {true, false, ""};
    }
    // Fail: bump cracked_digits 1..2 (partial state).
    int bump = 1 + static_cast<int>(rng() % 2);
    target.cracked_digits = static_cast<uint8_t>(
        std::min<int>(10, target.cracked_digits + bump));
    shell.emit("[-] Failed. Captured partial signature.", UITag::TextWarning);
    char buf[80];
    std::snprintf(buf, sizeof buf, "[+] Cracked digits so far: %u/11",
                  static_cast<unsigned>(target.cracked_digits));
    shell.emit(buf, UITag::TextDim);
    return {true, false, ""};
}

const HackCommand k_hashcat{
    "hashcat",
    "hashcat [--fast]",
    "attempt password recovery (escalation)",
    HackTag::Locked, false,
    /*base_turns=*/10, /*base_heat=*/6, /*base_detection=*/15,
    /*allow_partial=*/true,
    // execute() handles both "start" and the post-channel completion. The
    // runtime calls us once on submit (channel_active=false→start) and again
    // when the channel completes (channel_active=false again — but we just
    // started one, so it's been cleared). We disambiguate by re-checking
    // whether we still hold the start state: at completion the channel's
    // already-NULL-cmd condition triggers.
    [](const ParsedArgs& a, Hackable& t, DeviceShell& s, Game& g) -> HackCommandResult {
        // Heuristic: if we're called and there is NO active channel AND
        // cracked_digits indicates progress was already started (i.e. we got
        // here through completion path), apply completion logic. Otherwise
        // begin a new channel.
        //
        // The Phase A runtime calls execute() exactly TWICE per channel:
        //   1) on submit_input (start) - we call shell.start_channel() here
        //   2) after channel_.scaled_turns ticks - the channel was already
        //      cleared by tick_world before our re-entry; we then run the
        //      success roll.
        //
        // Distinguish: a freshly-issued command has args.argv[0]=="hashcat"
        // and the shell isn't busy. After completion the same applies. So we
        // use a static-init-friendly trick: stash a "expecting completion"
        // bool on the shell? No global state. The cleanest discriminator is
        // shell.channel_active() — but tick_world clears the channel BEFORE
        // calling execute(), so it's always false here.
        //
        // Pragmatic solution: cmd_hashcat carries a marker on the args via a
        // private synthetic flag. The runtime doesn't add one, so we add a
        // "completion" path explicitly — see device_shell completion patch
        // below where we set args.argv to {"hashcat","--__done"} when the
        // tick reaches scaled_turns.
        if (a.has_flag("--__done")) {
            return exec_hashcat_complete(t, s, g);
        }
        if (a.has_flag("--__partial")) {
            // Aborted mid-channel: bump cracked_digits 1..2 (partial state).
            std::mt19937_64 rng(static_cast<uint64_t>(g.world().world_tick()) ^
                                static_cast<uint64_t>(t.ip) * 0x4E55u);
            int bump = 1 + static_cast<int>(rng() % 2);
            t.cracked_digits = static_cast<uint8_t>(
                std::min<int>(10, t.cracked_digits + bump));
            char buf[80];
            std::snprintf(buf, sizeof buf,
                          "[+] Captured partial signature. Cracked: %u/11",
                          static_cast<unsigned>(t.cracked_digits));
            s.emit(buf, UITag::TextDim);
            return {true, false, ""};
        }
        return exec_hashcat(a, t, s, g);
    },
};

struct AutoRegister {
    AutoRegister() { register_hack_command(&k_hashcat); }
};
const AutoRegister k_auto;

} // namespace

void register_hashcat_command_anchor() { (void)&k_auto; }

} // namespace astra
