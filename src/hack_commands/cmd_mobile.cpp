// Plan 7 Phase B — Mobile-tag commands: halt / redirect / gps.

#include "astra/device_shell.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/hackable.h"
#include "astra/npc.h"
#include "astra/shell_context.h"
#include "astra/tilemap.h"
#include "astra/world_manager.h"

#include <cstdio>
#include <string>

namespace astra {

namespace {

constexpr int kHaltDuration = 20;

// Find the (x, y) of the entity carrying `target` — searches both fixtures
// and live NPCs (Mobile fixtures are usually NPC implants).
std::pair<int,int> locate_target(Game& game, const Hackable& target) {
    auto& m = game.world().map();
    for (int y = 0; y < m.height(); ++y) {
        for (int x = 0; x < m.width(); ++x) {
            if (m.get(x, y) != Tile::Fixture) continue;
            int fid = m.fixture_id(x, y);
            if (fid < 0) continue;
            if (m.fixture(fid).cyber && &*m.fixture(fid).cyber == &target)
                return {x, y};
        }
    }
    for (const auto& npc : game.world().npcs()) {
        if (npc.cyber && &*npc.cyber == &target) return {npc.x, npc.y};
    }
    return {-1, -1};
}

HackCommandResult exec_halt(const ParsedArgs& a, ShellContext& ctx, Game& game) {
    auto* dev = ctx.as_device();
    if (!dev || !dev->target()) return {false, false, ""};
    Hackable& target = *dev->target();
    DeviceShell& shell = *dev;
    if (a.has_flag("--__partial")) return {true, false, ""};
    if (a.has_flag("--__done")) {
        target.halt_ticks = kHaltDuration;
        char buf[96];
        std::snprintf(buf, sizeof buf,
                      "[+] Locomotion locked for %d turns.", kHaltDuration);
        return {true, false, buf};
    }
    const HackCommand* self = HackCommandRegistry::get().find("halt");
    if (!self) return {false, false, "halt: registry miss."};
    if (!shell.start_channel(*self, a, game)) return {false, false, ""};
    return {true, true, ""};
}

HackCommandResult exec_redirect(const ParsedArgs& a, ShellContext& ctx, Game& game) {
    auto* dev = ctx.as_device();
    if (!dev) return {false, false, ""};
    DeviceShell& shell = *dev;
    if (a.has_flag("--__partial")) return {true, false, ""};
    if (a.has_flag("--__done")) {
        std::string to(a.value_of("--to"));
        if (to.empty()) to = "0,0";
        // Cosmetic v1: print a "queued" line; AI doesn't yet consume this.
        return {true, false, std::string("[+] Redirect queued: (") + to + ")."};
    }
    const HackCommand* self = HackCommandRegistry::get().find("redirect");
    if (!self) return {false, false, "redirect: registry miss."};
    if (!shell.start_channel(*self, a, game)) return {false, false, ""};
    return {true, true, ""};
}

HackCommandResult exec_gps(const ParsedArgs& a, ShellContext& ctx, Game& game) {
    auto* dev = ctx.as_device();
    if (!dev || !dev->target()) return {false, false, ""};
    Hackable& target = *dev->target();
    DeviceShell& shell = *dev;
    if (a.has_flag("--__done")) return {true, false, ""};
    auto [x, y] = locate_target(game, target);
    if (x < 0) return {true, false, "gps: location unknown (target offline)."};
    char buf[120];
    std::snprintf(buf, sizeof buf, "[+] Location: (%d, %d), zone %d/%d.",
                  x, y,
                  game.world().zone_x(), game.world().zone_y());
    shell.emit(buf, UITag::TextDim);
    return {true, false, ""};
}

const HackCommand k_halt{
    "halt", "halt", "stop the mobile fixture for N turns",
    CommandScope::Device,
    HackTag::Mobile, /*requires_root=*/true,
    3, 3, 8, false, &exec_halt,
};
const HackCommand k_redirect{
    "redirect", "redirect --to=<x,y>", "set a new patrol target (cosmetic v1)",
    CommandScope::Device,
    HackTag::Mobile, /*requires_root=*/true,
    6, 4, 12, false, &exec_redirect,
};
const HackCommand k_gps{
    "gps", "gps", "print current location",
    CommandScope::Device,
    HackTag::Mobile, false, 0, 0, 0, false, &exec_gps,
};

struct AutoRegister {
    AutoRegister() {
        register_hack_command(&k_halt);
        register_hack_command(&k_redirect);
        register_hack_command(&k_gps);
    }
};
const AutoRegister k_auto;

} // namespace

void register_mobile_commands_anchor() { (void)&k_auto; }

} // namespace astra
