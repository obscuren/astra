// Plan 7 Phase B — Weaponized-tag commands: disarm / lockout /
// friendly_fire / targetlist.

#include "astra/device_shell.h"
#include "astra/faction.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/hackable.h"

#include <cstdio>
#include <string>

namespace astra {

namespace {

constexpr int kDisarmDuration = 30;

HackCommandResult exec_disarm(const ParsedArgs& a, Hackable& target,
                              DeviceShell& shell, Game& game) {
    if (a.has_flag("--__partial")) return {true, false, ""};
    if (a.has_flag("--__done")) {
        target.disarmed_ticks = kDisarmDuration;
        char buf[96];
        std::snprintf(buf, sizeof buf,
                      "[+] Turret disarmed for %d turns.", kDisarmDuration);
        return {true, false, buf};
    }
    const HackCommand* self = HackCommandRegistry::get().find("disarm");
    if (!self) return {false, false, "disarm: registry miss."};
    if (!shell.start_channel(*self, a, game)) return {false, false, ""};
    return {true, true, ""};
}

HackCommandResult exec_lockout(const ParsedArgs& a, Hackable& target,
                               DeviceShell& shell, Game& game) {
    if (a.has_flag("--__partial")) return {true, false, ""};
    if (a.has_flag("--__done")) {
        target.locked_out_to_player = true;
        return {true, false, "[+] Lockout engaged. Turret will only accept your inputs."};
    }
    const HackCommand* self = HackCommandRegistry::get().find("lockout");
    if (!self) return {false, false, "lockout: registry miss."};
    if (!shell.start_channel(*self, a, game)) return {false, false, ""};
    return {true, true, ""};
}

HackCommandResult exec_friendly_fire(const ParsedArgs& a, Hackable& target,
                                     DeviceShell& shell, Game& game) {
    if (a.has_flag("--__partial")) return {true, false, ""};
    if (a.has_flag("--__done")) {
        std::string fac(a.value_of("--target"));
        if (fac.empty()) fac = "Cartel"; // sensible default
        target.friendly_fire_target_faction = fac;
        // TODO(turret-AI): wire into NPC turret target selection (no turret
        // AI exists in v1; this is currently config-only).
        return {true, false, std::string("[+] Target priority reconfigured: ") +
                              fac + " is now hostile."};
    }
    const HackCommand* self = HackCommandRegistry::get().find("friendly_fire");
    if (!self) return {false, false, "friendly_fire: registry miss."};
    if (!shell.start_channel(*self, a, game)) return {false, false, ""};
    return {true, true, ""};
}

HackCommandResult exec_targetlist(const ParsedArgs& a, Hackable& target,
                                  DeviceShell& shell, Game&) {
    if (a.has_flag("--__done")) return {true, false, ""};
    shell.emit("[+] Target priority list:", UITag::TextDim);
    if (!target.friendly_fire_target_faction.empty()) {
        shell.emit(std::string("  1. ") + target.friendly_fire_target_faction +
                   " (override)", UITag::TextWarning);
    } else {
        shell.emit("  1. <hostile-to-owner>", UITag::TextDim);
    }
    shell.emit("  2. trespassers", UITag::TextDim);
    shell.emit("  3. unidentified IFF", UITag::TextDim);
    return {true, false, ""};
}

const HackCommand k_disarm{
    "disarm", "disarm", "turret disarmed for N turns",
    HackTag::Weaponized, /*requires_root=*/true,
    5, 4, 15, false, &exec_disarm,
};
const HackCommand k_lockout{
    "lockout", "lockout", "turret refuses inputs from anyone but you",
    HackTag::Weaponized, /*requires_root=*/true,
    3, 3, 12, false, &exec_lockout,
};
const HackCommand k_friendly_fire{
    "friendly_fire", "friendly_fire --target=<faction>",
    "reconfigure target priority by faction",
    HackTag::Weaponized, /*requires_root=*/true,
    10, 8, 25, false, &exec_friendly_fire,
};
const HackCommand k_targetlist{
    "targetlist", "targetlist", "print current target priority",
    HackTag::Weaponized, false, 0, 0, 0, false, &exec_targetlist,
};

struct AutoRegister {
    AutoRegister() {
        register_hack_command(&k_disarm);
        register_hack_command(&k_lockout);
        register_hack_command(&k_friendly_fire);
        register_hack_command(&k_targetlist);
    }
};
const AutoRegister k_auto;

} // namespace

void register_weaponized_commands_anchor() { (void)&k_auto; }

} // namespace astra
