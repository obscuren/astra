// Plan 7 Phase B — DataStore-tag instant reads: ls / cat / grep / find.
//
// All four are instant (paused world). Permission gating goes through the
// shell's DeviceFsView. Any guest sees only /etc/version, /etc/motd,
// /var/log/auth.log; root sees everything except kernel-only paths.

#include "astra/device_fs.h"
#include "astra/device_shell.h"
#include "astra/game.h"
#include "astra/hack_command.h"
#include "astra/hackable.h"
#include "astra/shell_context.h"

#include <sstream>
#include <string>

namespace astra {

namespace {

bool is_root_now(const Hackable& t, const DeviceShell& s) {
    return is_player_root(t, s.tier() == ShellTier::Root);
}

HackCommandResult exec_ls(const ParsedArgs& a, ShellContext& ctx, Game&) {
    auto* dev = ctx.as_device();
    if (!dev || !dev->target()) return {false, false, ""};
    Hackable& target = *dev->target();
    DeviceShell& shell = *dev;
    if (a.has_flag("--__done")) return {true, false, ""};
    std::string dir = "/";
    if (a.argv.size() > 1) dir = a.argv[1];
    bool root = is_root_now(target, shell);
    auto entries = shell.fs_view().list_dir(dir, root);
    if (entries.empty()) {
        // Distinguish "permission denied" from "empty dir" by re-checking.
        // If guest and any entry exists at root, say denied.
        auto root_entries = shell.fs_view().list_dir(dir, /*root=*/true);
        if (!root && !root_entries.empty()) {
            return {true, false, "ls: permission denied."};
        }
        return {true, false, "ls: (empty)"};
    }
    for (const auto& e : entries) shell.emit("  " + e, UITag::TextDim);
    return {true, false, ""};
}

HackCommandResult exec_cat(const ParsedArgs& a, ShellContext& ctx, Game&) {
    auto* dev = ctx.as_device();
    if (!dev || !dev->target()) return {false, false, ""};
    Hackable& target = *dev->target();
    DeviceShell& shell = *dev;
    if (a.has_flag("--__done")) return {true, false, ""};
    if (a.argv.size() < 2) return {false, false, "cat: usage: cat <path>"};
    bool root = is_root_now(target, shell);
    std::string content;
    bool denied = false;
    bool found = shell.fs_view().read(a.argv[1], root, content, denied);
    if (!found) return {true, false, std::string("cat: ") + a.argv[1] + ": no such file"};
    if (denied) return {true, false, std::string("cat: ") + a.argv[1] + ": permission denied"};
    std::stringstream ss(content);
    std::string line;
    while (std::getline(ss, line)) shell.emit(line, UITag::TextDefault);
    return {true, false, ""};
}

HackCommandResult exec_grep(const ParsedArgs& a, ShellContext& ctx, Game&) {
    auto* dev = ctx.as_device();
    if (!dev || !dev->target()) return {false, false, ""};
    Hackable& target = *dev->target();
    DeviceShell& shell = *dev;
    if (a.has_flag("--__done")) return {true, false, ""};
    if (a.argv.size() < 2) return {false, false, "grep: usage: grep <pattern>"};
    bool root = is_root_now(target, shell);
    auto hits = shell.fs_view().grep(a.argv[1], root);
    if (hits.empty()) return {true, false, "grep: no matches."};
    for (const auto& h : hits) {
        shell.emit(h.path + ": " + h.line, UITag::TextDim);
    }
    return {true, false, ""};
}

HackCommandResult exec_find(const ParsedArgs& a, ShellContext& ctx, Game&) {
    auto* dev = ctx.as_device();
    if (!dev || !dev->target()) return {false, false, ""};
    Hackable& target = *dev->target();
    DeviceShell& shell = *dev;
    if (a.has_flag("--__done")) return {true, false, ""};
    if (a.argv.size() < 2) return {false, false, "find: usage: find <pattern>"};
    bool root = is_root_now(target, shell);
    auto hits = shell.fs_view().find(a.argv[1], root);
    if (hits.empty()) return {true, false, "find: no matches."};
    for (const auto& p : hits) shell.emit("  " + p, UITag::TextDim);
    return {true, false, ""};
}

const HackCommand k_ls{
    "ls", "ls [<dir>]", "list files at path",
    CommandScope::Device,
    HackTag::DataStore, false, 0, 0, 0, false, &exec_ls,
};
const HackCommand k_cat{
    "cat", "cat <path>", "print file contents",
    CommandScope::Device,
    HackTag::DataStore, false, 0, 0, 0, false, &exec_cat,
};
const HackCommand k_grep{
    "grep", "grep <pattern>", "substring search across the device",
    CommandScope::Device,
    HackTag::DataStore, false, 0, 0, 0, false, &exec_grep,
};
const HackCommand k_find{
    "find", "find <glob>", "path-pattern search (supports *)",
    CommandScope::Device,
    HackTag::DataStore, false, 0, 0, 0, false, &exec_find,
};

struct AutoRegister {
    AutoRegister() {
        register_hack_command(&k_ls);
        register_hack_command(&k_cat);
        register_hack_command(&k_grep);
        register_hack_command(&k_find);
    }
};
const AutoRegister k_auto;

} // namespace

void register_datastore_read_commands_anchor() { (void)&k_auto; }

} // namespace astra
