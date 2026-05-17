#include "astra/grammars/gen_turret_netspace.h"

#include "astra/net_constants.h"
#include "astra/net_ice.h"
#include "astra/net_room.h"
#include "astra/net_theme.h"
#include "astra/netspace_layout.h"

#include <cstdio>
#include <string>

namespace astra {

// ---------------------------------------------------------------------------
// Layout constants — matched to the design-doc sample (§ "Turret — hostile
// from frame one"). The outer arena uses add_room_outline with a BoxBlock
// (▓) border; JACK, AMMO, FRIEND are a vertical spine inside.
//
// Arena: 38w × 20h (outer), with 2-cell margins giving ~34w × 16h interior.
// Spine rooms: 7w × 5h, centered horizontally, stacked vertically.
// ---------------------------------------------------------------------------
namespace {

constexpr int kCanvasW   = 54;
constexpr int kCanvasH   = 27;

// Outer █-walled arena. Height sized so the JACK/AMMO/FRIEND spine
// (3 rooms × 5h + 2-row gaps) fits with margin off the top/bottom walls.
constexpr int kArenaX    = 3;
constexpr int kArenaY    = 1;
constexpr int kArenaW    = 40;
constexpr int kArenaH    = 24;

// Spine rooms: 7w × 5h.
constexpr int kRoomW     = 7;
constexpr int kRoomH     = 5;

// Horizontal start of the spine inside the arena — inset well off the
// left █ wall so the rooms have breathing room (gap = 5 cells).
constexpr int kSpineX    = kArenaX + 5;

// Vertical positions of each spine room (top edge). +2 leaves a 1-row
// interior gap below the top █ wall.
constexpr int kJackY     = kArenaY + 2;
constexpr int kAmmoY     = kJackY  + kRoomH + 2;
constexpr int kFriendY   = kAmmoY  + kRoomH + 2;

// "TURRET_07"-style id from the seed.
std::string turret_id(uint32_t seed) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "TURRET_%02d",
                  static_cast<int>(seed % 100u));
    return buf;
}

} // namespace

Netspace gen_turret_netspace(const TargetDescriptor& desc) {
    NetspaceBuilder b(kCanvasW, kCanvasH, NetTile::Void);
    b.set_target(desc);

    // Title bar.
    char title_buf[64];
    std::snprintf(title_buf, sizeof title_buf,
                  "KANG-TAO AUTO :: %s :: ARMED",
                  turret_id(desc.seed).c_str());
    b.set_title(title_buf);

    // Three rooms: JACK + AMMO + FRIEND.  (Arena is a stamped █ frame;
    // Exit is a raw tile.) Reserve to keep NetRoom references stable.
    b.ns.rooms.reserve(static_cast<size_t>(3));

    // ── Outer arena ── full-block █ frame (NetTile::Glyph + override, in
    // magenta). A flat █ avoids the half-block box-corner artifacts of
    // Border::Block. 1-cell thick; interior stays Void so the avatar walks
    // only through the spine pipe corridors. Glyph tiles are impassable.
    {
        const char* kFull = "\xe2\x96\x88";  // █
        const Color  wall = net_theme::wall_heavy;  // BrightMagenta
        const int x0 = kArenaX, y0 = kArenaY;
        const int x1 = kArenaX + kArenaW - 1, y1 = kArenaY + kArenaH - 1;
        auto put = [&](int x, int y) {
            b.ns.set(x, y, NetTile::Glyph);
            b.ns.glyph_overrides[{x, y}]       = kFull;
            b.ns.glyph_color_overrides[{x, y}] = wall;
        };
        for (int x = x0; x <= x1; ++x) { put(x, y0); put(x, y1); }
        for (int y = y0 + 1; y < y1; ++y) { put(x0, y); put(x1, y); }
    }

    // ── JACK room (jack-in) ────────────────────────────────────────────
    NetRoom& jack = b.add_room(kSpineX, kJackY, kRoomW, kRoomH, "JACK",
                               NetRoom::Border::Thin);
    jack.label_color    = net_theme::box_thin_color;
    jack.bottom_content = "";   // avatar overlays via set_jack_in
    b.set_jack_in(jack);

    // Jack-in coords (center of jack room interior, bottom_content row).
    const int jack_cx = kSpineX + kRoomW / 2;
    const int jack_cy = kJackY  + kRoomH - 2; // bottom_content row

    // ── AMMO room (TurretDisarm node) ─────────────────────────────────
    NetRoom& ammo = b.add_room(kSpineX, kAmmoY, kRoomW, kRoomH, "AMMO",
                               NetRoom::Border::Thin);
    ammo.label_color    = net_theme::box_thin_color;
    ammo.bottom_content = "\xc2\xa7\xc2\xa7\xc2\xa7";  // §§§ — disarm glyph
    ammo.bottom_color   = Color::Yellow;

    b.connect_vertical(jack, ammo, NetPipe::Style::Double);

    // ── FRIEND room (TurretFlip node) ─────────────────────────────────
    NetRoom& frnd = b.add_room(kSpineX, kFriendY, kRoomW, kRoomH, "FRIEND",
                               NetRoom::Border::Thin);
    frnd.label_color    = net_theme::box_thin_color;
    frnd.bottom_content = "/OE";
    frnd.bottom_color   = Color::Cyan;

    b.connect_vertical(ammo, frnd, NetPipe::Style::Double);

    // ── Exit ── route a horizontal pipe from FRIEND's right wall to the
    // right side of the arena. The arena interior is Void, so the pipe
    // is the only walkable path to Exit.
    const int frnd_right = kSpineX + kRoomW - 1;
    const int exit_y     = kFriendY + kRoomH / 2;   // vertical center of FRIEND
    const int exit_x     = kArenaX  + kArenaW - 2;  // last interior col of arena

    // Open port through FRIEND's right wall and lay the pipe.
    b.make_passable(frnd_right, exit_y);
    for (int x = frnd_right + 1; x < exit_x; ++x) {
        b.ns.set(x, exit_y, NetTile::PipeH);
    }
    b.ns.set(exit_x, exit_y, NetTile::Exit);
    b.ns.exit_x = exit_x;
    b.ns.exit_y = exit_y;

    // ── Gray ICE — hostile from frame 1 ──────────────────────────────
    // Place n_ice Gray ICE in the arena interior, each within
    // kIceVisionRange (Manhattan) of the jack-in tile, on a passable
    // tile, not on the jack-in tile, not overlapping each other.
    // The spine pipe cells between JACK and AMMO are passable Floor/Pipe.
    // We place ICE in those corridors so they immediately engage.
    //
    // Passable cells near jack_cy in the pipe corridor (between JACK bottom
    // edge and AMMO top edge). The pipe center column is jack_cx; cells
    // at (jack_cx, jack_cy + 1) up to (jack_cx, kAmmoY - 1) are PipeV.
    // All are within kIceVisionRange = 4 of (jack_cx, jack_cy).
    int n_ice = 1 + desc.tier / 2;
    {
        // Shared placement helper: attempt to place one Gray ICE at (jack_cx, iy).
        // Returns true and appends to initial_ice if the cell passes all guards;
        // returns false (no-op) otherwise.
        auto try_place = [&](int iy) -> bool {
            const int ix = jack_cx;
            if (iy == kJackY + kRoomH - 1) return false; // JACK bottom border — BoxThin, not passable
            if (iy == kAmmoY)               return false; // AMMO top border — passable but visually bad
            if (!b.ns.in_bounds(ix, iy)) return false;
            if (ix == jack_cx && iy == jack_cy) return false; // jack-in tile itself
            for (const auto& prev : b.ns.initial_ice)
                if (prev.x == ix && prev.y == iy) return false; // already occupied
            Ice g;
            g.color = IceColor::Gray;
            g.hp    = 2;
            g.x     = ix;
            g.y     = iy;
            b.ns.initial_ice.push_back(g);
            return true;
        };

        // Forward scan: downward from jack-in into the pipe corridor.
        int placed = 0;
        for (int d = 1; placed < n_ice && d <= kIceVisionRange; ++d)
            if (try_place(jack_cy + d)) ++placed;

        // Fallback: upward scan into the JACK interior above jack-in.
        for (int d = 1; placed < n_ice && d <= kIceVisionRange; ++d) {
            int iy = jack_cy - d;
            if (iy == kJackY) continue;  // top border of JACK room
            if (try_place(iy)) ++placed;
        }
    }

    // ── Action nodes ───────────────────────────────────────────────────
    // AMMO interior center (label row = kAmmoY + 2, bottom row = kAmmoY + 3).
    {
        NetNode dis;
        dis.kind  = NetNodeKind::TurretDisarm;
        dis.label = "\xc2\xa7\xc2\xa7\xc2\xa7";  // §§§
        dis.x     = kSpineX + kRoomW / 2;         // center col
        dis.y     = kAmmoY  + kRoomH - 2;         // bottom_content row
        b.ns.action_nodes.push_back(dis);
    }
    {
        NetNode flip;
        flip.kind  = NetNodeKind::TurretFlip;
        flip.label = "/OE";
        flip.x     = kSpineX + kRoomW / 2;
        flip.y     = kFriendY + kRoomH - 2;       // bottom_content row
        b.ns.action_nodes.push_back(flip);
    }

    return b.finalize();
}

} // namespace astra
