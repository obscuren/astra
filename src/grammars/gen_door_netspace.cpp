#include "astra/grammars/gen_door_netspace.h"

#include "astra/net_room.h"
#include "astra/net_theme.h"
#include "astra/netspace_layout.h"

#include <cstdio>
#include <random>
#include <string>

namespace astra {

namespace {

// Layout constants — matched to the design-doc sample (rooms 7w × 5h
// with a 4-tile gap that the ════ pipe fills).
constexpr int kRoomW   = 7;
constexpr int kRoomH   = 5;
constexpr int kGap     = 4;
constexpr int kMargX   = 3;
constexpr int kMargY   = 2;

// LOCK count is seed-derived and tier-gated:
//   tier 1 → 2-3 locks
//   tier 2 → 2-4 locks
//   tier 3+ → 3-4 locks
int lock_count(uint32_t seed, int tier) {
    std::mt19937 rng(seed ^ 0xD00Du);
    const int lo = (tier >= 3) ? 3 : 2;
    const int hi = (tier >= 2) ? 4 : 3;
    std::uniform_int_distribution<int> d(lo, hi);
    return d(rng);
}

// "DOOR_47B"-style id from the seed. Three decimal digits + one letter.
std::string door_id(uint32_t seed) {
    char buf[16];
    const int  n      = static_cast<int>(seed % 1000);
    const char suffix = static_cast<char>('A' + ((seed / 1000) % 26));
    std::snprintf(buf, sizeof buf, "DOOR_%03d%c", n, suffix);
    return buf;
}

}  // namespace

Netspace gen_door_netspace(const TargetDescriptor& desc) {
    const int n_locks    = lock_count(desc.seed, desc.tier);
    // Rooms: JACK + n_locks × LOCK + BOLT + OUT.
    const int total      = 1 + n_locks + 1 + 1;
    const int width      = kMargX * 2 + total * kRoomW + (total - 1) * kGap;
    const int height     = kMargY * 2 + kRoomH;

    NetspaceBuilder b(width, height, NetTile::Void);
    b.set_target(desc);

    // Title bar.
    char title_buf[64];
    std::snprintf(title_buf, sizeof title_buf,
                  "MAGLOCK :: %s :: TIER %d",
                  door_id(desc.seed).c_str(), desc.tier);
    b.set_title(title_buf);

    // Reserve to keep NetRoom references stable across add_room calls.
    b.ns.rooms.reserve(static_cast<size_t>(total));

    int x       = kMargX;
    const int y = kMargY;

    // ── JACK ───────────────────────────────────────────────────────
    NetRoom& jack = b.add_room(x, y, kRoomW, kRoomH, "JACK",
                               NetRoom::Border::Thin);
    jack.top_content    = "\xe2\x97\x84\xe2\x94\x80\xe2\x94\x80";  // ◄──
    jack.top_color      = net_theme::pipe_color;
    jack.label_color    = net_theme::box_thin_color;
    jack.bottom_content = "";   // avatar overlays here via set_jack_in
    b.set_jack_in(jack);
    x += kRoomW + kGap;

    // ── LOCK 1..n_locks ────────────────────────────────────────────
    // Density per lock — earlier locks lighter, later locks heavier.
    // With n_locks = 2 → ░ ▒; n_locks = 3 → ░ ▒ ▓; n_locks = 4 → · ░ ▒ ▓.
    NetRoom* prev = &jack;
    for (int i = 0; i < n_locks; ++i) {
        int den_idx = i + (4 - n_locks);
        if (den_idx < 0) den_idx = 0;
        if (den_idx > 3) den_idx = 3;

        NetRoom& lock = b.add_room(x, y, kRoomW, kRoomH, "LOCK",
                                   NetRoom::Border::Thin);
        const uint8_t density = static_cast<uint8_t>(den_idx + 1);  // 0..3 -> 1..4 (·/░/▒/▓)
        b.fill_top_row_with_breakwall(lock, density);
        lock.top_content    = "";
        lock.label_color    = net_theme::box_thin_color;
        lock.bottom_content = std::to_string(i + 1);
        lock.bottom_color   = net_theme::box_thin_color;

        b.connect(*prev, lock, NetPipe::Style::Double);
        prev = &lock;
        x += kRoomW + kGap;
    }

    // ── BOLT ───────────────────────────────────────────────────────
    NetRoom& bolt = b.add_room(x, y, kRoomW, kRoomH, "BOLT",
                               NetRoom::Border::Thin);
    b.fill_top_row_with_breakwall(bolt, /*density=*/4);   // ▓▓▓▓▓ — Breakwall heavy density
    bolt.top_content    = "";
    bolt.label_color    = net_theme::box_thin_color;
    bolt.bottom_content = net_theme::glyph_loot;     // ◊
    bolt.bottom_color   = Color::Yellow;
    b.connect(*prev, bolt, NetPipe::Style::Double);
    x += kRoomW + kGap;

    // ── OUT ────────────────────────────────────────────────────────
    NetRoom& out = b.add_room(x, y, kRoomW, kRoomH, "OUT",
                              NetRoom::Border::Thin);
    out.top_content    = "";
    out.label_color    = net_theme::box_thin_color;
    out.bottom_content = "\xe2\x96\xba\xe2\x94\x80\xe2\x94\x80";  // ►──
    out.bottom_color   = net_theme::pipe_color;
    b.connect(bolt, out, NetPipe::Style::Double);
    b.set_exit(out);

    return b.finalize();
}

}  // namespace astra
