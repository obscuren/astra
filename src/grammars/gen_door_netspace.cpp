#include "astra/grammars/gen_door_netspace.h"

#include "astra/net_ice.h"
#include "astra/net_room.h"
#include "astra/net_theme.h"
#include "astra/netspace_layout.h"
#include "astra/grammars/seed_daemon.h"

#include <algorithm>
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

// Phase 5 S7c.1: tier-scaled daemon stats for the door grammar.
// Table values follow the design doc § 2.3. Tier-1 ships the
// DaemonDef baseline; higher tiers multiply.
struct LockTierScale { int hp; int windup; };
constexpr LockTierScale kLockTiers[5] = {
    /* T1 */ { 4, 5 },
    /* T2 */ { 5, 5 },
    /* T3 */ { 5, 4 },
    /* T4 */ { 6, 4 },
    /* T5 */ { 6, 3 },
};
struct BoltTierScale { int hp; int windup; int cast_damage; };
constexpr BoltTierScale kBoltTiers[5] = {
    /* T1 */ { 12, 6, 2 },
    /* T2 */ { 16, 6, 2 },
    /* T3 */ { 20, 5, 3 },
    /* T4 */ { 28, 5, 3 },
    /* T5 */ { 36, 4, 4 },
};

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
    // Phase 5 S7c.1: each LOCK is a typed daemon whose HP drives the
    // wall-density visual via the RoomFill render path. HP + windup
    // scale by tier (kLockTiers[]); the static breakwall infrastructure
    // is no longer consulted for door netspaces.
    NetRoom* prev = &jack;
    const int t_lock = std::clamp(desc.tier - 1, 0, 4);
    for (int i = 0; i < n_locks; ++i) {
        NetRoom& lock = b.add_room(x, y, kRoomW, kRoomH, "LOCK",
                                   NetRoom::Border::Thin);
        lock.top_content    = "";
        lock.label_color    = net_theme::box_thin_color;
        lock.bottom_content = std::to_string(i + 1);
        lock.bottom_color   = net_theme::box_thin_color;

        b.connect(*prev, lock, NetPipe::Style::Double);
        seed_daemon(b, lock, DaemonKind::Lock,
                    kLockTiers[t_lock].hp,
                    kLockTiers[t_lock].windup,
                    /*cast_dmg_override*/ 0);   // Lock uses def baseline (1)
        prev = &lock;
        x += kRoomW + kGap;
    }

    // ── BOLT ───────────────────────────────────────────────────────
    // Phase 5 S7c.1: BOLT is a typed micro-boss daemon (Yellow ▣
    // glyph). Its bottom_content cell is left empty -- the daemon glyph
    // itself communicates the BOLT identity.
    NetRoom& bolt = b.add_room(x, y, kRoomW, kRoomH, "BOLT",
                               NetRoom::Border::Thin);
    bolt.top_content    = "";
    bolt.label_color    = net_theme::box_thin_color;
    bolt.bottom_content = "";                         // daemon glyph replaces it
    bolt.bottom_color   = Color::Yellow;
    b.connect(*prev, bolt, NetPipe::Style::Double);
    {
        const int t_bolt = std::clamp(desc.tier - 1, 0, 4);
        seed_daemon(b, bolt, DaemonKind::Bolt,
                    kBoltTiers[t_bolt].hp,
                    kBoltTiers[t_bolt].windup,
                    kBoltTiers[t_bolt].cast_damage);
    }
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
