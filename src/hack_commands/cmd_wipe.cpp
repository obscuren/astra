// Plan 7 Phase B — `wipe` privileged long-channel atomic.
//
// On success: appends the wiped path into Hackable.wiped_paths and rebuilds
// the DeviceFsView so the file disappears from subsequent `ls`/`cat`/`grep`.
// Persists across saves (see save_file.cpp v64). Atomic: aborted runs apply
// no partial state.

#include "astra/device_fs.h"
#include "astra/device_shell.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/hackable.h"

#include <algorithm>

namespace astra {

namespace {

HackCommandResult exec_wipe(const ParsedArgs& a, Hackable& target,
                            DeviceShell& shell, Game& game) {
    if (a.has_flag("--__partial")) return {true, false, ""}; // atomic: no partial
    if (a.has_flag("--__done")) {
        // Apply the wipe.
        std::string path;
        for (size_t i = 1; i < a.argv.size(); ++i) {
            const auto& v = a.argv[i];
            if (!v.empty() && v[0] != '-') { path = v; break; }
        }
        if (path.empty()) return {true, false, "wipe: missing path."};
        if (std::find(target.wiped_paths.begin(), target.wiped_paths.end(), path) ==
            target.wiped_paths.end()) {
            target.wiped_paths.push_back(path);
        }
        shell.rebuild_fs_view();
        return {true, false, std::string("[+] ") + path + " wiped. Persisted across saves."};
    }
    if (a.argv.size() < 2) return {false, false, "wipe: usage: wipe <path>"};
    const std::string& path = a.argv[1];
    if (!shell.fs_view().exists(path)) {
        return {true, false, std::string("wipe: ") + path + ": no such file"};
    }
    const HackCommand* self = HackCommandRegistry::get().find("wipe");
    if (!self) return {false, false, "wipe: registry miss."};
    if (!shell.start_channel(*self, a, game)) return {false, false, ""};
    return {true, true, ""};
}

const HackCommand k_wipe{
    "wipe", "wipe <path>",
    "permanently remove a file from the device (privileged)",
    HackTag::DataStore, /*requires_root=*/true,
    /*base_turns=*/4, /*base_heat=*/2, /*base_detection=*/10,
    /*allow_partial=*/false,
    &exec_wipe,
};

struct AutoRegister {
    AutoRegister() { register_hack_command(&k_wipe); }
};
const AutoRegister k_auto;

} // namespace

void register_wipe_command_anchor() { (void)&k_auto; }

} // namespace astra
