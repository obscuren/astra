#include "astra/netspace_layout.h"

#include <algorithm>
#include <cstdlib>

namespace astra {

NetspaceBuilder::NetspaceBuilder(int w, int h, NetTile bg) {
    ns.w = w;
    ns.h = h;
    ns.tiles.assign(static_cast<size_t>(w) * static_cast<size_t>(h), bg);
}

void NetspaceBuilder::fill(NetTile t) {
    std::fill(ns.tiles.begin(), ns.tiles.end(), t);
}

NetRoom& NetspaceBuilder::add_room(int x, int y, int w, int h,
                                   std::string label, NetRoom::Border border) {
    NetRoom room;
    room.x = x;
    room.y = y;
    room.w = w;
    room.h = h;
    room.border = border;
    room.label = std::move(label);

    // Stamp border + interior into the tile grid.
    const NetTile box = box_tile_for(border);
    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            const int tx = x + dx;
            const int ty = y + dy;
            if (!ns.in_bounds(tx, ty)) continue;
            const bool is_border = (dx == 0 || dx == w - 1 ||
                                    dy == 0 || dy == h - 1);
            ns.set(tx, ty, is_border ? box : NetTile::Floor);
        }
    }

    ns.rooms.push_back(std::move(room));
    return ns.rooms.back();
}

void NetspaceBuilder::stamp_h(int y, int x0, int x1) {
    if (x0 > x1) std::swap(x0, x1);
    for (int x = x0; x <= x1; ++x) {
        if (!ns.in_bounds(x, y)) continue;
        const NetTile cur = ns.at(x, y);
        // Don't overwrite room borders; cross PipeV → PipeJunc.
        if (cur == NetTile::BoxThin || cur == NetTile::BoxDouble ||
            cur == NetTile::BoxBlock) continue;
        if (cur == NetTile::PipeV)   ns.set(x, y, NetTile::PipeJunc);
        else                         ns.set(x, y, NetTile::PipeH);
    }
}

void NetspaceBuilder::stamp_v(int x, int y0, int y1) {
    if (y0 > y1) std::swap(y0, y1);
    for (int y = y0; y <= y1; ++y) {
        if (!ns.in_bounds(x, y)) continue;
        const NetTile cur = ns.at(x, y);
        if (cur == NetTile::BoxThin || cur == NetTile::BoxDouble ||
            cur == NetTile::BoxBlock) continue;
        if (cur == NetTile::PipeH)   ns.set(x, y, NetTile::PipeJunc);
        else                         ns.set(x, y, NetTile::PipeV);
    }
}

NetPipe& NetspaceBuilder::connect(const NetRoom& a, const NetRoom& b,
                                  NetPipe::Style style) {
    // Pick the dominant axis. Horizontal-dominant connections anchor
    // on the rooms' facing left/right edges at their first interior
    // row (y+1). Vertical-dominant connections anchor on the rooms'
    // facing top/bottom edges at the first interior column (x+1).
    //
    // First-interior anchoring matches the design-doc samples: door
    // pipes run through the ◄── / ░░░ / ▒▒▒ / ▓▓▓ header content row;
    // camera lens pipes through the (o) content row; vending shelf →
    // dispense vertical pipes through each shelf's column.
    const int ax_c = a.x + a.w / 2;
    const int ay_c = a.y + a.h / 2;
    const int bx_c = b.x + b.w / 2;
    const int by_c = b.y + b.h / 2;
    const int dx = bx_c - ax_c;
    const int dy = by_c - ay_c;

    NetPipe p;
    p.style = style;

    if (std::abs(dx) >= std::abs(dy)) {
        // Horizontal-dominant L: H at a.y+1, then V at b's near-x to b.y+1.
        const int ay = a.y + 1;
        const int by = b.y + 1;
        const int ax = (dx >= 0) ? a.x + a.w - 1 : a.x;
        const int bx = (dx >= 0) ? b.x : b.x + b.w - 1;
        p.x0 = ax; p.y0 = ay; p.x1 = bx; p.y1 = by;
        stamp_h(ay, ax, bx);
        if (ay != by) stamp_v(bx, ay, by);
    } else {
        // Vertical-dominant L: V at a's near-y to b.x+1's column, then H.
        const int ax = a.x + a.w / 2;
        const int bx = b.x + b.w / 2;
        const int ay = (dy >= 0) ? a.y + a.h - 1 : a.y;
        const int by = (dy >= 0) ? b.y : b.y + b.h - 1;
        p.x0 = ax; p.y0 = ay; p.x1 = bx; p.y1 = by;
        stamp_v(ax, ay, by);
        if (ax != bx) stamp_h(by, ax, bx);
    }

    ns.pipes.push_back(p);
    return ns.pipes.back();
}

// For 5-tall rooms the design-doc samples place the avatar @ on the
// bottom_content row (y+3). For 4-tall rooms the bottom_content row is
// y+2. In both cases that's room.y + room.h - 2 — the last interior
// row. For 3-tall rooms (lens nodes), the only interior row is y+1.
static int interior_focus_y(const NetRoom& r) {
    if (r.h <= 3) return r.y + 1;
    return r.y + r.h - 2;
}

void NetspaceBuilder::set_jack_in(NetRoom& r) {
    r.is_jack_in = true;
    const int cx = r.x + r.w / 2;
    const int cy = interior_focus_y(r);
    ns.jack_in_x = cx;
    ns.jack_in_y = cy;
    if (ns.in_bounds(cx, cy)) {
        ns.set(cx, cy, NetTile::JackIn);
    }
}

void NetspaceBuilder::set_exit(NetRoom& r) {
    r.is_exit = true;
    const int cx = r.x + r.w / 2;
    const int cy = interior_focus_y(r);
    ns.exit_x = cx;
    ns.exit_y = cy;
    if (ns.in_bounds(cx, cy)) {
        ns.set(cx, cy, NetTile::Exit);
    }
}

}  // namespace astra
