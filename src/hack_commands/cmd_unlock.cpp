// Plan 7 Phase B — `unlock` (Locked tag, root-only single-tick).
//
// Doors: cycles the FixtureData.locked flag (and fd.open if currently shut).
// Other Locked devices: sets escalated=true (no further effect; cosmetic).

#include "astra/device_shell.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/hackable.h"
#include "astra/player.h"
#include "astra/shell_context.h"
#include "astra/tilemap.h"
#include "astra/world_manager.h"

namespace astra {

namespace {

FixtureData* find_fixture_for_target(Game& game, Hackable& target) {
    auto& m = game.world().map();
    for (int i = 0; i < m.fixture_count(); ++i) {
        FixtureData& fd = m.fixture_mut(i);
        if (fd.cyber && &*fd.cyber == &target) return &fd;
    }
    return nullptr;
}

HackCommandResult exec_unlock(const ParsedArgs& a, ShellContext& ctx, Game& game) {
    auto* dev = ctx.as_device();
    if (!dev || !dev->target()) return {false, false, ""};
    Hackable& target = *dev->target();
    DeviceShell& shell = *dev;
    if (a.has_flag("--__done")) return {true, false, ""};
    if (!has_tag(target.tags, HackTag::Locked)) {
        return {false, false, "unlock: target is not locked."};
    }

    FixtureData* fd = find_fixture_for_target(game, target);
    if (fd && fd->type == FixtureType::Door) {
        fd->locked = false;
        shell.emit("[+] Door cycle requested.", UITag::TextSuccess);
        shell.emit("[+] Lock disengaged.", UITag::TextSuccess);
        return {true, false, ""};
    }
    if (fd && fd->type == FixtureType::Gate) {
        fd->locked = false;
        shell.emit("[+] Gate disengaged.", UITag::TextSuccess);
        return {true, false, ""};
    }

    // Non-door Locked devices: equivalent to "I have admin rights now".
    target.escalated = true;
    shell.emit("[+] Authentication elevated. Subsequent commands run as root.",
               UITag::TextSuccess);
    return {true, false, ""};
}

const HackCommand k_unlock{
    "unlock",
    "unlock",
    "request open / disengage lock",
    CommandScope::Device,
    HackTag::Locked, /*requires_root=*/true,
    0, 0, 0, false,
    &exec_unlock,
};

struct AutoRegister {
    AutoRegister() { register_hack_command(&k_unlock); }
};
const AutoRegister k_auto;

} // namespace

void register_unlock_command_anchor() { (void)&k_auto; }

} // namespace astra
