// Plan 7 Phase B — `dump` privileged long-channel.
//
// Leeches a file's contents to the player's inventory as a `data fragment`
// item. Partial-state: on abort, accumulate dumped_bytes. The data-fragment
// item type does not yet exist in v1; we print the "[+] Dumped N bytes from
// <path>" success line and leave a TODO for a future plan to add the item
// to the item database.

#include "astra/device_fs.h"
#include "astra/device_shell.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/hackable.h"
#include "astra/shell_context.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace astra {

namespace {

HackCommandResult exec_dump(const ParsedArgs& a, ShellContext& ctx, Game& game) {
    auto* dev = ctx.as_device();
    if (!dev || !dev->target()) return {false, false, ""};
    Hackable& target = *dev->target();
    DeviceShell& shell = *dev;
    if (a.has_flag("--__partial")) {
        // Find the path arg (positional, non-flag).
        std::string path;
        for (size_t i = 1; i < a.argv.size(); ++i) {
            const auto& v = a.argv[i];
            if (!v.empty() && v[0] != '-') { path = v; break; }
        }
        // Pull progress percent from the synthetic flag.
        std::string pf(a.value_of("--__partial"));
        int pct = 0;
        try { pct = std::stoi(pf); } catch (...) { pct = 0; }
        // Accumulate bytes proportionally.
        uint32_t total = 0;
        if (!path.empty()) {
            std::string content;
            bool denied = false;
            if (shell.fs_view().read(path, /*is_root=*/true, content, denied) && !denied) {
                total = static_cast<uint32_t>(content.size());
            }
        }
        uint32_t accrued = (total * static_cast<uint32_t>(std::clamp(pct, 0, 100))) / 100;
        target.dumped_bytes += accrued;
        char buf[80];
        std::snprintf(buf, sizeof buf,
                      "[+] Salvaged %u bytes (running total %u).",
                      static_cast<unsigned>(accrued),
                      static_cast<unsigned>(target.dumped_bytes));
        shell.emit(buf, UITag::TextDim);
        return {true, false, ""};
    }
    if (a.has_flag("--__done")) {
        // Channel completion path: apply the success.
        std::string path;
        for (size_t i = 1; i < a.argv.size(); ++i) {
            const auto& v = a.argv[i];
            if (!v.empty() && v[0] != '-') { path = v; break; }
        }
        if (path.empty()) return {true, false, "dump: missing path on completion."};
        std::string content;
        bool denied = false;
        bool ok = shell.fs_view().read(path, /*is_root=*/true, content, denied);
        if (!ok || denied) {
            return {true, false, std::string("dump: ") + path + ": vanished mid-channel."};
        }
        uint32_t bytes = static_cast<uint32_t>(content.size());
        target.dumped_bytes += bytes;
        char buf[160];
        std::snprintf(buf, sizeof buf,
                      "[+] Dumped %u bytes from %s.",
                      static_cast<unsigned>(bytes), path.c_str());
        shell.emit(buf, UITag::TextSuccess);
        // TODO(plan-N): add a `data fragment` item to player inventory and
        // attribute its contents here. Phase B leaves it as a log line.
        return {true, false, ""};
    }
    if (a.argv.size() < 2) return {false, false, "dump: usage: dump <path>"};
    const std::string& path = a.argv[1];
    if (!shell.fs_view().exists(path)) {
        return {true, false, std::string("dump: ") + path + ": no such file"};
    }
    if (!shell.fs_view().can_read(path, /*is_root=*/true)) {
        return {true, false, std::string("dump: ") + path + ": kernel-only"};
    }
    const HackCommand* self = HackCommandRegistry::get().find("dump");
    if (!self) return {false, false, "dump: registry miss."};
    if (!shell.start_channel(*self, a, game)) return {false, false, ""};
    return {true, true, ""};
}

const HackCommand k_dump{
    "dump", "dump <path>",
    "leech file contents to inventory (privileged)",
    CommandScope::Device,
    HackTag::DataStore, /*requires_root=*/true,
    /*base_turns=*/6, /*base_heat=*/3, /*base_detection=*/8,
    /*allow_partial=*/true,
    &exec_dump,
};

struct AutoRegister {
    AutoRegister() { register_hack_command(&k_dump); }
};
const AutoRegister k_auto;

} // namespace

void register_dump_command_anchor() { (void)&k_auto; }

} // namespace astra
