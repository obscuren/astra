#include "astra/grammars/gen_vending_netspace.h"

#include "astra/net_room.h"
#include "astra/net_theme.h"
#include "astra/netspace_layout.h"

#include <cstdio>
#include <string>

namespace astra {

namespace {

// Canvas + outer-machine sizing — matched to the design-doc sample.
// The outer "machine" room frames the netspace; shelves and DISPENSE are
// nested inside its Floor interior.
constexpr int kCanvasW   = 54;
constexpr int kCanvasH   = 14;
constexpr int kOuterX    = 19;
constexpr int kOuterY    = 1;
constexpr int kOuterW    = 17;
constexpr int kOuterH    = 12;

// Shelf rooms — 3w × 3h BoxDouble nodes, three across, in a row near the
// top of the outer interior. With h=3, the single interior row renders
// the label centered, which is where the density glyph sits.
constexpr int kShelfY    = kOuterY + 2;
constexpr int kShelfW    = 3;
constexpr int kShelfH    = 3;
constexpr int kShelf1X   = kOuterX + 3;
constexpr int kShelf2X   = kOuterX + 7;
constexpr int kShelf3X   = kOuterX + 11;

// DISPENSE — 12w × 4h BoxThin box near the bottom of the outer interior.
// h=4 places label at y+1 and bottom_content at y+2; jack-in writes
// the avatar tile onto bottom_content (the design-doc avatar row).
constexpr int kDispenseX = kOuterX + 2;
constexpr int kDispenseY = kOuterY + 7;
constexpr int kDispenseW = 12;
constexpr int kDispenseH = 4;

// "VEND_NN" id from the seed — two decimal digits, like the design's VEND_09.
std::string vend_id(uint32_t seed) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "VEND_%02d",
                  static_cast<int>(seed % 100u));
    return buf;
}

}  // namespace

Netspace gen_vending_netspace(const TargetDescriptor& desc) {
    NetspaceBuilder b(kCanvasW, kCanvasH, NetTile::Void);
    b.set_target(desc);

    // Title bar — short corporate brand + unit id. The subtitle carries
    // the joke flavor line.
    char title_buf[64];
    std::snprintf(title_buf, sizeof title_buf,
                  "STIM-O-MATIC :: %s", vend_id(desc.seed).c_str());
    b.set_title(title_buf);
    b.ns.title_subtitle = "have a nice day :)";

    // Five rooms total: outer machine + 3 shelves + dispense. Reserve to
    // keep NetRoom references stable across add_room calls.
    b.ns.rooms.reserve(5);

    // ── Outer machine ─────────────────────────────────────────────────
    // BoxThin frame with no label or content — its Floor interior is
    // what the shelves and DISPENSE are stamped into.
    NetRoom& outer = b.add_room(kOuterX, kOuterY, kOuterW, kOuterH, "",
                                NetRoom::Border::Thin);
    outer.label_color = net_theme::box_thin_color;

    // ── Three shelf nodes ─────────────────────────────────────────────
    // Per the design-doc sample, the trio reads ▓ ░ ▒ left-to-right.
    NetRoom& shelf1 = b.add_room(kShelf1X, kShelfY, kShelfW, kShelfH, "",
                                 NetRoom::Border::Double);
    shelf1.label        = net_theme::wall_heavy_glyph;   // ▓
    shelf1.label_color  = net_theme::wall_heavy;

    NetRoom& shelf2 = b.add_room(kShelf2X, kShelfY, kShelfW, kShelfH, "",
                                 NetRoom::Border::Double);
    shelf2.label        = net_theme::wall_light_glyph;   // ░
    shelf2.label_color  = net_theme::wall_light;

    NetRoom& shelf3 = b.add_room(kShelf3X, kShelfY, kShelfW, kShelfH, "",
                                 NetRoom::Border::Double);
    shelf3.label        = net_theme::wall_med_glyph;     // ▒
    shelf3.label_color  = net_theme::wall_med;

    // ── DISPENSE ──────────────────────────────────────────────────────
    NetRoom& dispense = b.add_room(kDispenseX, kDispenseY,
                                   kDispenseW, kDispenseH, "DISPENSE",
                                   NetRoom::Border::Thin);
    dispense.label_color    = net_theme::box_thin_color;
    dispense.bottom_content = "";   // jack-in writes the @ tile here
    dispense.bottom_color   = net_theme::avatar;

    // ── Pipes: each shelf drops straight down into DISPENSE's top edge.
    // connect() picks vertical-dominant routing (centers differ more in
    // y than x) and stamp_v skips Box* cells, so each pipe runs cleanly
    // through the two Floor rows between shelf bottom and DISPENSE top.
    b.connect(shelf1, dispense, NetPipe::Style::Double);
    b.connect(shelf2, dispense, NetPipe::Style::Double);
    b.connect(shelf3, dispense, NetPipe::Style::Double);

    // Avatar spawns inside DISPENSE on its bottom_content row.
    b.set_jack_in(dispense);

    // ── Junctions where pipes meet DISPENSE top ───────────────────────
    // stamp_v leaves the BoxThin top row intact (so its border renders
    // cleanly), but we want the pipes to *terminate* visibly at the box
    // and — crucially — for those cells to be passable so the avatar can
    // walk between DISPENSE and the shelves via the pipes.
    const int dispense_top = kDispenseY;
    b.ns.set(kShelf1X + kShelfW / 2, dispense_top, NetTile::PipeJunc);
    b.ns.set(kShelf2X + kShelfW / 2, dispense_top, NetTile::PipeJunc);
    b.ns.set(kShelf3X + kShelfW / 2, dispense_top, NetTile::PipeJunc);

    // ── Exit ──────────────────────────────────────────────────────────
    // Open a port through DISPENSE's right wall at the avatar row and
    // place the Exit tile two steps further right on outer floor. The
    // wall stays visually intact (BoxThin │ at that cell);
    // make_passable lets the avatar phase through that one cell.
    const int avatar_y       = kDispenseY + kDispenseH - 2;  // bottom_content row
    const int dispense_right = kDispenseX + kDispenseW - 1;
    b.make_passable(dispense_right, avatar_y);

    // Place the exit on outer floor, one cell shy of the outer right wall
    // so it stays inside the machine frame.
    const int exit_x = kOuterX + kOuterW - 2;
    const int exit_y = avatar_y;
    b.ns.set(exit_x, exit_y, NetTile::Exit);
    b.ns.exit_x = exit_x;
    b.ns.exit_y = exit_y;

    return b.finalize();
}

}  // namespace astra
