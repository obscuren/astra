#pragma once

#include "astra/hackable.h"

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace astra {

// Plan 7 §11 — DeviceFsView
//
// Procedurally generated, in-memory "filesystem" for one device shell session.
// Built deterministically from (network_id, hackable_id-stable-bits) so a
// re-opened shell always shows the same files.
//
// Path/content lookups go through this object; commands `ls`, `cat`, `grep`,
// `find`, `dump`, `wipe` all consult it. Permission gate (guest vs root) is
// enforced by the view itself: anything outside the guest-allow list returns
// "permission denied" to a guest tier.
//
// Phase B: persists `wiped_paths` on the Hackable (vector<string>); a wipe
// command appends the path; subsequent ls/cat skip wiped entries.
class DeviceFsView {
public:
    // Build the view for `target`. `faction` selects the flavor pack used
    // for /home/<user>/notes.txt content. Empty = Civilian fallback.
    void build(const Hackable& target, std::string_view faction);

    // Sorted absolute paths present on the device. Wiped paths excluded.
    const std::vector<std::string>& all_paths() const { return paths_; }

    // List files under `dir` (e.g. "/var/log"). Returns basenames (or full
    // paths if `full=true`). `is_root` controls permission gating.
    std::vector<std::string> list_dir(std::string_view dir,
                                      bool is_root,
                                      bool full = false) const;

    // Read file content. Returns false if path doesn't exist.
    // If permission denied: out_content set to "permission denied" and
    // returns true with `out_denied=true`.
    bool read(std::string_view path,
              bool is_root,
              std::string& out_content,
              bool& out_denied) const;

    // Substring search across all readable files. Each match emitted as a
    // pair {path, line}.
    struct GrepHit {
        std::string path;
        std::string line;
    };
    std::vector<GrepHit> grep(std::string_view needle, bool is_root) const;

    // Glob-style path-pattern search. Supports `*` only.
    std::vector<std::string> find(std::string_view pattern, bool is_root) const;

    // Permission check (without reading). Public for cmd_dump / cmd_wipe.
    bool can_read(std::string_view path, bool is_root) const;

    // Encoded entry — used by cmd_wipe to know whether the file existed.
    bool exists(std::string_view path) const;

    // Access map directly (for test / dev console).
    const std::map<std::string, std::string>& entries() const { return entries_; }

private:
    std::map<std::string, std::string> entries_;   // path → content
    std::vector<std::string> paths_;               // sorted keys (post-wipe)
    HackTagMask tags_ = 0;
    bool built_ = false;
};

// Persisted on the Hackable: wiped path bitmask. We use a small string vector
// stored on the runtime side (not yet persisted to save — see plan note).
// For Phase B, the in-memory record lives on a `wiped_paths` field added to
// Hackable. See hackable.h.

} // namespace astra
