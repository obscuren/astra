#include "astra/device_fs.h"

#include "astra/fixture_os_id.h"
#include "astra/hack_flavor.h"

#include <algorithm>
#include <cstdio>
#include <random>
#include <sstream>
#include <unordered_set>

namespace astra {

namespace {

// Hash for the per-device deterministic seed.
uint64_t mix_seed(uint32_t a, uint32_t b) {
    uint64_t s = 0xC0DEC0DEu;
    s ^= a + 0x9E3779B97F4A7C15ull + (s << 6) + (s >> 2);
    s ^= b + 0x9E3779B97F4A7C15ull + (s << 6) + (s >> 2);
    return s;
}

// Pick deterministically from a span.
template <class T>
const T& pick(std::span<const T> pool, std::mt19937_64& rng, const T& fallback) {
    if (pool.empty()) return fallback;
    return pool[rng() % pool.size()];
}

const char* pick_str(std::span<const char* const> pool, std::mt19937_64& rng) {
    if (pool.empty()) return "";
    return pool[rng() % pool.size()];
}

bool starts_with(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() &&
           std::memcmp(s.data(), prefix.data(), prefix.size()) == 0;
}

bool path_in_dir(std::string_view path, std::string_view dir) {
    // dir is "/var/log" -> match "/var/log/anything" (no trailing slash).
    std::string norm(dir);
    while (!norm.empty() && norm.back() == '/') norm.pop_back();
    if (norm.empty()) {
        // root: match top-level (/foo with no further /)
        if (path.size() < 2 || path[0] != '/') return false;
        size_t next = path.find('/', 1);
        return next == std::string_view::npos;
    }
    if (!starts_with(path, norm)) return false;
    if (path.size() == norm.size()) return false;
    return path[norm.size()] == '/';
}

bool simple_glob(std::string_view pat, std::string_view s) {
    // Minimal glob: only `*` matches any-length, no ?/brackets.
    size_t pi = 0, si = 0, star = std::string_view::npos, sm = 0;
    while (si < s.size()) {
        if (pi < pat.size() && pat[pi] == '*') {
            star = pi++;
            sm = si;
        } else if (pi < pat.size() && pat[pi] == s[si]) {
            ++pi; ++si;
        } else if (star != std::string_view::npos) {
            pi = star + 1;
            si = ++sm;
        } else {
            return false;
        }
    }
    while (pi < pat.size() && pat[pi] == '*') ++pi;
    return pi == pat.size();
}

bool guest_readable(std::string_view path) {
    return path == "/etc/version" ||
           path == "/etc/motd" ||
           path == "/var/log/auth.log";
}

bool is_kernel_only(std::string_view path) {
    // Reserved for plot-gated reads; nothing here in v1.
    (void)path;
    return false;
}

// Build /var/log/auth.log content from faction templates.
std::string build_auth_log(const HackFlavorPack& fp, std::mt19937_64& rng) {
    int n = 5 + (rng() % 6); // 5..10
    std::string out;
    for (int i = 0; i < n; ++i) {
        out += pick_str(fp.log_lines, rng);
        out += '\n';
    }
    return out;
}

std::string build_system_log(const HackFlavorPack& fp, std::mt19937_64& rng,
                             const char* system) {
    int n = 3 + (rng() % 5);
    std::string out;
    for (int i = 0; i < n; ++i) {
        out += "[";
        out += system;
        out += "] ";
        out += pick_str(fp.log_lines, rng);
        out += '\n';
    }
    return out;
}

std::string build_firmware_blob(const char* tag, std::mt19937_64& rng) {
    std::string out = "# firmware blob (";
    out += tag;
    out += ")\n";
    char hex[24];
    for (int i = 0; i < 8; ++i) {
        std::snprintf(hex, sizeof hex, "%016llx",
                      static_cast<unsigned long long>(rng()));
        out += hex;
        out += '\n';
    }
    return out;
}

} // namespace

void DeviceFsView::build(const Hackable& target, std::string_view faction) {
    entries_.clear();
    paths_.clear();
    tags_ = target.tags;
    built_ = true;

    const HackFlavorPack& fp = flavor_for(faction);
    const FixtureOsId& os = os_id_for(target.source_type);

    uint64_t seed = mix_seed(target.network_id ^ target.ip,
                             static_cast<uint32_t>(target.source_type) * 0x9E37u);
    std::mt19937_64 rng(seed);

    // Always-present paths.
    {
        std::string ver = std::string(os.os_name) + " " + os.version + "\n";
        entries_["/etc/version"] = ver;
    }
    {
        std::string motd = std::string("# ") + pick_str(fp.motd_lines, rng) + "\n";
        entries_["/etc/motd"] = motd;
    }
    entries_["/var/log/auth.log"] = build_auth_log(fp, rng);

    // Tag-conditional system logs and firmware blobs.
    auto add_for_tag = [&](HackTag t, const char* sysname, const char* fwname) {
        if (!has_tag(target.tags, t)) return;
        std::string log_path = std::string("/var/log/") + sysname + ".log";
        entries_[log_path] = build_system_log(fp, rng, sysname);
        std::string fw_path = std::string("/firmware/") + fwname + ".fw";
        entries_[fw_path] = build_firmware_blob(fwname, rng);
    };
    add_for_tag(HackTag::HasOptics,  "optics",  "optics");
    add_for_tag(HackTag::PowerNode,  "power",   "power");
    add_for_tag(HackTag::Weaponized, "weapons", "weapons");
    add_for_tag(HackTag::Mobile,     "motion",  "motion");
    add_for_tag(HackTag::DataStore,  "data",    "data");
    add_for_tag(HackTag::Locked,     "auth",    "auth");

    // /home/<user>/notes.txt|todo.txt — one user, two files.
    if (!fp.user_names.empty()) {
        const char* user = pick_str(fp.user_names, rng);
        std::string base = std::string("/home/") + user + "/";
        entries_[base + "notes.txt"] = std::string(pick_str(fp.file_contents, rng)) + "\n";
        entries_[base + "todo.txt"] = std::string(pick_str(fp.file_contents, rng)) + "\n";
    }

    // /data/* — DataStore only.
    if (has_tag(target.tags, HackTag::DataStore)) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "/data/archive-%04x.frag",
                      static_cast<unsigned>(rng() & 0xFFFF));
        entries_[buf] = std::string(pick_str(fp.file_contents, rng)) + "\n";
        std::snprintf(buf, sizeof buf, "/data/balance-%04x.txt",
                      static_cast<unsigned>(rng() & 0xFFFF));
        entries_[buf] = std::string("balance: ") +
                        std::to_string(rng() % 5000) + " credits\n";
    }

    // Apply persisted wipes.
    std::unordered_set<std::string> wiped(target.wiped_paths.begin(),
                                          target.wiped_paths.end());
    for (auto it = entries_.begin(); it != entries_.end(); ) {
        if (wiped.count(it->first)) it = entries_.erase(it);
        else ++it;
    }

    paths_.reserve(entries_.size());
    for (const auto& [k, _] : entries_) paths_.push_back(k);
    std::sort(paths_.begin(), paths_.end());
}

bool DeviceFsView::can_read(std::string_view path, bool is_root) const {
    if (!exists(path)) return false;
    if (is_kernel_only(path)) return false;
    if (is_root) return true;
    return guest_readable(path);
}

bool DeviceFsView::exists(std::string_view path) const {
    return entries_.find(std::string(path)) != entries_.end();
}

std::vector<std::string>
DeviceFsView::list_dir(std::string_view dir, bool is_root, bool full) const {
    std::vector<std::string> out;
    std::string norm(dir);
    while (norm.size() > 1 && norm.back() == '/') norm.pop_back();
    for (const auto& p : paths_) {
        if (!path_in_dir(p, norm)) continue;
        if (!is_root && !guest_readable(p)) continue;
        if (full) {
            out.push_back(p);
        } else {
            size_t pos = norm.empty() ? 1 : norm.size() + 1;
            out.push_back(p.substr(pos));
        }
    }
    return out;
}

bool DeviceFsView::read(std::string_view path,
                        bool is_root,
                        std::string& out_content,
                        bool& out_denied) const {
    out_denied = false;
    auto it = entries_.find(std::string(path));
    if (it == entries_.end()) return false;
    if (!is_root && !guest_readable(path)) {
        out_denied = true;
        out_content = "permission denied";
        return true;
    }
    if (is_kernel_only(path)) {
        out_denied = true;
        out_content = "permission denied (kernel-only)";
        return true;
    }
    out_content = it->second;
    return true;
}

std::vector<DeviceFsView::GrepHit>
DeviceFsView::grep(std::string_view needle, bool is_root) const {
    std::vector<GrepHit> out;
    if (needle.empty()) return out;
    for (const auto& [path, content] : entries_) {
        if (!is_root && !guest_readable(path)) continue;
        std::stringstream ss(content);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.find(needle) != std::string::npos) {
                out.push_back({path, line});
            }
        }
    }
    return out;
}

std::vector<std::string>
DeviceFsView::find(std::string_view pattern, bool is_root) const {
    std::vector<std::string> out;
    for (const auto& p : paths_) {
        if (!is_root && !guest_readable(p)) continue;
        // Match pattern against full path AND basename.
        bool m = simple_glob(pattern, p);
        if (!m) {
            size_t slash = p.find_last_of('/');
            std::string_view base =
                slash == std::string::npos ? std::string_view(p)
                                           : std::string_view(p).substr(slash + 1);
            m = simple_glob(pattern, base);
        }
        if (m) out.push_back(p);
    }
    return out;
}

} // namespace astra
