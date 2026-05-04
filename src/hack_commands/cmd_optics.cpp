// Plan 7 Phase B — HasOptics-tag commands: blind / feed / restream / purge.
//
// `blind`     — privileged long-channel; sets optics_blind_ticks for N turns.
// `feed`      — root long-channel; ASCII-renders a snapshot of the camera's
//               nearby visible tiles into the shell scroll.
// `restream`  — root long-channel; loops a recorded frame, sets
//               optics_restream_ticks (longer effective blind window).
// `purge`     — root long-channel; cosmetic v1 ("buffer purged").

#include "astra/device_shell.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/hackable.h"
#include "astra/shell_context.h"
#include "astra/tilemap.h"
#include "astra/world_manager.h"

#include <cstdio>
#include <string>

namespace astra {

namespace {

constexpr int kBlindDuration    = 30;
constexpr int kRestreamDuration = 60;

FixtureData* find_fixture_for_target(Game& game, Hackable& target) {
    auto& m = game.world().map();
    for (int i = 0; i < m.fixture_count(); ++i) {
        FixtureData& fd = m.fixture_mut(i);
        if (fd.cyber && &*fd.cyber == &target) return &fd;
    }
    return nullptr;
}

HackCommandResult exec_blind(const ParsedArgs& a, ShellContext& ctx, Game& game) {
    auto* dev = ctx.as_device();
    if (!dev || !dev->target()) return {false, false, ""};
    Hackable& target = *dev->target();
    DeviceShell& shell = *dev;
    if (a.has_flag("--__partial")) return {true, false, ""};
    if (a.has_flag("--__done")) {
        target.optics_blind_ticks = kBlindDuration;
        char buf[96];
        std::snprintf(buf, sizeof buf,
                      "[+] Vision cone disabled for %d turns.", kBlindDuration);
        return {true, false, buf};
    }
    const HackCommand* self = HackCommandRegistry::get().find("blind");
    if (!self) return {false, false, "blind: registry miss."};
    if (!shell.start_channel(*self, a, game)) return {false, false, ""};
    return {true, true, ""};
}

HackCommandResult exec_feed(const ParsedArgs& a, ShellContext& ctx, Game& game) {
    auto* dev = ctx.as_device();
    if (!dev || !dev->target()) return {false, false, ""};
    Hackable& target = *dev->target();
    DeviceShell& shell = *dev;
    if (a.has_flag("--__partial")) return {true, false, ""};
    if (a.has_flag("--__done")) {
        FixtureData* fd = find_fixture_for_target(game, target);
        int cx = -1, cy = -1;
        if (fd) {
            // Locate the fixture's tile by scanning fixture_id grid.
            const auto& m = game.world().map();
            for (int y = 0; y < m.height(); ++y) {
                for (int x = 0; x < m.width(); ++x) {
                    if (m.get(x, y) == Tile::Fixture && m.fixture_id(x, y) >= 0 &&
                        &m.fixture(m.fixture_id(x, y)) == fd) { cx = x; cy = y; }
                }
            }
        }
        if (cx < 0) {
            return {true, false, "[+] Feed: <signal lost — camera offline>"};
        }
        // Render a 7x7 ASCII snapshot around the camera.
        const int R = 3;
        shell.emit("[+] Live feed snapshot:", UITag::TextDim);
        const auto& m = game.world().map();
        for (int dy = -R; dy <= R; ++dy) {
            std::string row = "  | ";
            for (int dx = -R; dx <= R; ++dx) {
                int x = cx + dx, y = cy + dy;
                if (x < 0 || y < 0 || x >= m.width() || y >= m.height()) {
                    row += '?'; continue;
                }
                Tile t = m.get(x, y);
                char ch = '.';
                switch (t) {
                    case Tile::Wall:
                    case Tile::StructuralWall: ch = '#'; break;
                    case Tile::Fixture:        ch = '+'; break;
                    case Tile::Floor:
                    case Tile::IndoorFloor:
                    case Tile::Path:           ch = '.'; break;
                    default:                   ch = '?'; break;
                }
                if (dx == 0 && dy == 0) ch = '@'; // camera location
                row += ch;
            }
            row += " |";
            shell.emit(row, UITag::TextDim);
        }
        return {true, false, ""};
    }
    const HackCommand* self = HackCommandRegistry::get().find("feed");
    if (!self) return {false, false, "feed: registry miss."};
    if (!shell.start_channel(*self, a, game)) return {false, false, ""};
    return {true, true, ""};
}

HackCommandResult exec_restream(const ParsedArgs& a, ShellContext& ctx, Game& game) {
    auto* dev = ctx.as_device();
    if (!dev || !dev->target()) return {false, false, ""};
    Hackable& target = *dev->target();
    DeviceShell& shell = *dev;
    if (a.has_flag("--__partial")) return {true, false, ""};
    if (a.has_flag("--__done")) {
        target.optics_restream_ticks = kRestreamDuration;
        target.optics_blind_ticks = kRestreamDuration; // also blind for the loop
        char buf[96];
        std::snprintf(buf, sizeof buf,
                      "[+] Recording loop active. Camera reports nothing for %d turns.",
                      kRestreamDuration);
        return {true, false, buf};
    }
    const HackCommand* self = HackCommandRegistry::get().find("restream");
    if (!self) return {false, false, "restream: registry miss."};
    if (!shell.start_channel(*self, a, game)) return {false, false, ""};
    return {true, true, ""};
}

HackCommandResult exec_purge(const ParsedArgs& a, ShellContext& ctx, Game& game) {
    auto* dev = ctx.as_device();
    if (!dev) return {false, false, ""};
    DeviceShell& shell = *dev;
    if (a.has_flag("--__partial")) return {true, false, ""};
    if (a.has_flag("--__done")) {
        // Cosmetic: there's no recording buffer in v1. Print and move on.
        return {true, false, "[+] Buffer purged."};
    }
    const HackCommand* self = HackCommandRegistry::get().find("purge");
    if (!self) return {false, false, "purge: registry miss."};
    if (!shell.start_channel(*self, a, game)) return {false, false, ""};
    return {true, true, ""};
}

const HackCommand k_blind{
    "blind", "blind", "disable vision cone for N turns",
    CommandScope::Device,
    HackTag::HasOptics, /*requires_root=*/true,
    /*turns=*/4, /*heat=*/4, /*det=*/12, false, &exec_blind,
};
const HackCommand k_feed{
    "feed", "feed", "render snapshot of camera vision (root)",
    CommandScope::Device,
    HackTag::HasOptics, /*requires_root=*/true,
    2, 2, 4, false, &exec_feed,
};
const HackCommand k_restream{
    "restream", "restream", "loop a recorded frame (camera reports nothing)",
    CommandScope::Device,
    HackTag::HasOptics, /*requires_root=*/true,
    8, 6, 18, false, &exec_restream,
};
const HackCommand k_purge{
    "purge", "purge", "wipe the camera's recording buffer",
    CommandScope::Device,
    HackTag::HasOptics, /*requires_root=*/true,
    3, 3, 6, false, &exec_purge,
};

struct AutoRegister {
    AutoRegister() {
        register_hack_command(&k_blind);
        register_hack_command(&k_feed);
        register_hack_command(&k_restream);
        register_hack_command(&k_purge);
    }
};
const AutoRegister k_auto;

} // namespace

void register_optics_commands_anchor() { (void)&k_auto; }

} // namespace astra
