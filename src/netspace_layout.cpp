#include "astra/netspace_layout.h"

#include <algorithm>

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
    // Route between the two rooms' nearest-edge midpoints.
    // Horizontal-first L route: pick the row halfway between the two
    // rooms' vertical centers, run horizontally, then drop vertically.
    const int ax_mid = a.x + a.w / 2;
    const int ay_mid = a.y + a.h / 2;
    const int bx_mid = b.x + b.w / 2;
    const int by_mid = b.y + b.h / 2;

    // Anchor on the room edge facing the other room.
    int ax = (bx_mid >= ax_mid) ? a.x + a.w - 1 : a.x;
    int bx = (bx_mid >= ax_mid) ? b.x : b.x + b.w - 1;

    NetPipe p;
    p.x0 = ax;
    p.y0 = ay_mid;
    p.x1 = bx;
    p.y1 = by_mid;
    p.style = style;

    // Stamp: walk horizontally at ay_mid from ax → bx, then vertically
    // at bx from ay_mid → by_mid. Aligned rooms collapse to one segment.
    stamp_h(ay_mid, ax, bx);
    if (ay_mid != by_mid) stamp_v(bx, ay_mid, by_mid);

    ns.pipes.push_back(p);
    return ns.pipes.back();
}

void NetspaceBuilder::set_jack_in(NetRoom& r) {
    r.is_jack_in = true;
    const int cx = r.x + r.w / 2;
    const int cy = r.y + r.h / 2;
    ns.jack_in_x = cx;
    ns.jack_in_y = cy;
    if (ns.in_bounds(cx, cy)) {
        ns.set(cx, cy, NetTile::JackIn);
    }
}

void NetspaceBuilder::set_exit(NetRoom& r) {
    r.is_exit = true;
    const int cx = r.x + r.w / 2;
    const int cy = r.y + r.h / 2;
    ns.exit_x = cx;
    ns.exit_y = cy;
    if (ns.in_bounds(cx, cy)) {
        ns.set(cx, cy, NetTile::Exit);
    }
}

}  // namespace astra
