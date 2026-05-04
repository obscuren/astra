// Plan 7 Phase B — PowerNode-tag commands: surge / kill / reroute / dim.

#include "astra/device_shell.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/hackable.h"
#include "astra/tilemap.h"
#include "astra/world_manager.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace astra {

namespace {

constexpr int kSurgeDuration = 8;
constexpr int kKillDuration  = 30;
constexpr int kDimDuration   = 30;

// Locate fixture coords for `target`. Returns (-1,-1) if not found.
std::pair<int,int> target_xy(Game& game, const Hackable& target) {
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
    return {-1, -1};
}

// Apply `apply` to every Electronic-tagged neighbour within `radius` (Chebyshev).
template <class F>
void for_neighbour_devices(Game& game, const Hackable& src, int radius, F&& apply) {
    auto [sx, sy] = target_xy(game, src);
    if (sx < 0) return;
    auto& m = game.world().map();
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int x = sx + dx, y = sy + dy;
            if (x < 0 || y < 0 || x >= m.width() || y >= m.height()) continue;
            if (m.get(x, y) != Tile::Fixture) continue;
            int fid = m.fixture_id(x, y);
            if (fid < 0) continue;
            FixtureData& fd = m.fixture_mut(fid);
            if (!fd.cyber) continue;
            if (!has_tag(fd.cyber->tags, HackTag::Electronic)) continue;
            apply(*fd.cyber);
        }
    }
}

// Apply to every device on the same network_id as `src`.
template <class F>
void for_network_devices(Game& game, const Hackable& src, F&& apply) {
    auto& m = game.world().map();
    for (int i = 0; i < m.fixture_count(); ++i) {
        FixtureData& fd = m.fixture_mut(i);
        if (!fd.cyber) continue;
        if (&*fd.cyber == &src) continue;
        if (fd.cyber->network_id == 0) continue;
        if (fd.cyber->network_id != src.network_id) continue;
        apply(*fd.cyber);
    }
}

HackCommandResult exec_surge(const ParsedArgs& a, Hackable& target,
                             DeviceShell& shell, Game& game) {
    if (a.has_flag("--__partial")) return {true, false, ""};
    if (a.has_flag("--__done")) {
        int n = 0;
        for_neighbour_devices(game, target, /*radius=*/2, [&](Hackable& h) {
            h.surged_ticks = std::max(h.surged_ticks, kSurgeDuration);
            ++n;
        });
        char buf[96];
        std::snprintf(buf, sizeof buf, "[+] Surge applied to %d adjacent device(s).", n);
        return {true, false, buf};
    }
    const HackCommand* self = HackCommandRegistry::get().find("surge");
    if (!self) return {false, false, "surge: registry miss."};
    if (!shell.start_channel(*self, a, game)) return {false, false, ""};
    return {true, true, ""};
}

HackCommandResult exec_kill(const ParsedArgs& a, Hackable& target,
                            DeviceShell& shell, Game& game) {
    if (a.has_flag("--__partial")) return {true, false, ""};
    if (a.has_flag("--__done")) {
        int n = 0;
        if (target.network_id != 0) {
            for_network_devices(game, target, [&](Hackable& h) {
                h.power_off_ticks = std::max(h.power_off_ticks, kKillDuration);
                ++n;
            });
        } else {
            for_neighbour_devices(game, target, /*radius=*/3, [&](Hackable& h) {
                h.power_off_ticks = std::max(h.power_off_ticks, kKillDuration);
                ++n;
            });
        }
        char buf[96];
        std::snprintf(buf, sizeof buf,
                      "[+] Power cut to %d downstream device(s) for %d turns.",
                      n, kKillDuration);
        return {true, false, buf};
    }
    const HackCommand* self = HackCommandRegistry::get().find("kill");
    if (!self) return {false, false, "kill: registry miss."};
    if (!shell.start_channel(*self, a, game)) return {false, false, ""};
    return {true, true, ""};
}

HackCommandResult exec_reroute(const ParsedArgs& a, Hackable& /*target*/,
                               DeviceShell& shell, Game& game) {
    if (a.has_flag("--__partial")) return {true, false, ""};
    if (a.has_flag("--__done")) {
        // Cosmetic v1: no downstream-set model yet.
        return {true, false, "[+] Downstream set switched. Topology re-cached."};
    }
    const HackCommand* self = HackCommandRegistry::get().find("reroute");
    if (!self) return {false, false, "reroute: registry miss."};
    if (!shell.start_channel(*self, a, game)) return {false, false, ""};
    return {true, true, ""};
}

HackCommandResult exec_dim(const ParsedArgs& a, Hackable& target,
                           DeviceShell& shell, Game& game) {
    if (a.has_flag("--__partial")) return {true, false, ""};
    if (a.has_flag("--__done")) {
        int n = 0;
        auto apply = [&](Hackable& h) {
            if (!has_tag(h.tags, HackTag::HasOptics)) return;
            h.optics_dim_ticks = std::max(h.optics_dim_ticks, kDimDuration);
            ++n;
        };
        if (target.network_id != 0) {
            for_network_devices(game, target, apply);
        } else {
            for_neighbour_devices(game, target, /*radius=*/3, apply);
        }
        char buf[96];
        std::snprintf(buf, sizeof buf,
                      "[+] Dimmed %d downstream optics device(s).", n);
        return {true, false, buf};
    }
    const HackCommand* self = HackCommandRegistry::get().find("dim");
    if (!self) return {false, false, "dim: registry miss."};
    if (!shell.start_channel(*self, a, game)) return {false, false, ""};
    return {true, true, ""};
}

const HackCommand k_surge{
    "surge", "surge", "briefly power up adjacent devices",
    HackTag::PowerNode, /*requires_root=*/true,
    4, 4, 10, false, &exec_surge,
};
const HackCommand k_kill{
    "kill", "kill", "cut power to downstream devices for N turns",
    HackTag::PowerNode, /*requires_root=*/true,
    5, 5, 15, false, &exec_kill,
};
const HackCommand k_reroute{
    "reroute", "reroute", "switch downstream device set",
    HackTag::PowerNode, /*requires_root=*/true,
    8, 6, 18, false, &exec_reroute,
};
const HackCommand k_dim{
    "dim", "dim", "reduce vision-cone radius of downstream optics",
    HackTag::PowerNode, /*requires_root=*/true,
    2, 2, 5, false, &exec_dim,
};

struct AutoRegister {
    AutoRegister() {
        register_hack_command(&k_surge);
        register_hack_command(&k_kill);
        register_hack_command(&k_reroute);
        register_hack_command(&k_dim);
    }
};
const AutoRegister k_auto;

} // namespace

void register_powernode_commands_anchor() { (void)&k_auto; }

} // namespace astra
