#include "astra/net_pipe_path.h"

#include "astra/net_room.h"
#include "astra/netspace.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace astra {

// ---------------------------------------------------------------------------
// is_pipe_cell
// ---------------------------------------------------------------------------
bool is_pipe_cell(const Netspace& ns, int x, int y) {
    if (!ns.in_bounds(x, y)) return false;
    switch (ns.at(x, y)) {
        case NetTile::PipeH:
        case NetTile::PipeV:
        case NetTile::PipeJunc:
        case NetTile::PipePortV:
        case NetTile::PipePortCornerTR:
        case NetTile::PipePortDownD:
            return true;
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// room_index_at
// ---------------------------------------------------------------------------

// Chebyshev distance from point (px,py) to the nearest edge of the axis-
// aligned rect [rx, rx+rw) x [ry, ry+rh). Returns 0 if the point is inside.
static int chebyshev_to_rect(int px, int py, int rx, int ry, int rw, int rh) {
    // Clamp point to rect, then take the Chebyshev of the delta.
    const int cx = px < rx ? rx : (px >= rx + rw ? rx + rw - 1 : px);
    const int cy = py < ry ? ry : (py >= ry + rh ? ry + rh - 1 : py);
    const int dx = std::abs(px - cx);
    const int dy = std::abs(py - cy);
    return dx > dy ? dx : dy;
}

int room_index_at_strict(const Netspace& ns, int x, int y) {
    // Strict INTERIOR test (excludes the wall ring at room.x / x+w-1 /
    // y / y+h-1). Wall cells are pipe-port positions where the avatar
    // is still visually inside a pipe; engagement shouldn't trigger
    // until the avatar steps off the wall onto an interior floor cell.
    //
    // Phase 5 S7g: iterate in REVERSE registration order so later-
    // registered (typically nested / smaller) rooms win the
    // containment check over outer wrapper frames. VENDING's
    // add_room_outline registers an OUTER decorative frame at
    // rooms[0] whose rect strictly contains all interior rooms;
    // forward iteration shadowed the inner rooms. All shipped
    // grammars except VENDING have disjoint rooms (no nesting),
    // so this is a no-op for them.
    for (int i = static_cast<int>(ns.rooms.size()) - 1; i >= 0; --i) {
        const NetRoom& r = ns.rooms[i];
        if (x > r.x && x < r.x + r.w - 1 &&
            y > r.y && y < r.y + r.h - 1)
            return i;
    }
    return -1;
}

int room_index_at(const Netspace& ns, int x, int y) {
    if (ns.rooms.empty()) return -1;

    // First pass: containment check.
    // Phase 5 S7g: iterate in REVERSE registration order so later-
    // registered (nested / smaller) rooms win containment over outer
    // wrapper frames. See room_index_at_strict for full rationale.
    for (int i = static_cast<int>(ns.rooms.size()) - 1; i >= 0; --i) {
        const NetRoom& r = ns.rooms[i];
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
            return i;
    }

    // Second pass: nearest rect by Chebyshev distance.
    int best_i    = 0;
    int best_dist = chebyshev_to_rect(x, y,
                                       ns.rooms[0].x, ns.rooms[0].y,
                                       ns.rooms[0].w, ns.rooms[0].h);
    for (int i = 1; i < static_cast<int>(ns.rooms.size()); ++i) {
        const NetRoom& r = ns.rooms[i];
        const int d = chebyshev_to_rect(x, y, r.x, r.y, r.w, r.h);
        if (d < best_dist) {
            best_dist = d;
            best_i    = i;
        }
    }
    return best_i;
}

// ---------------------------------------------------------------------------
// connected_pipe_indices
// ---------------------------------------------------------------------------
std::vector<int> connected_pipe_indices(const Netspace& ns, int ax, int ay) {
    const int r = room_index_at(ns, ax, ay);
    if (r < 0) return {};

    std::vector<int> result;
    for (int i = 0; i < static_cast<int>(ns.pipes.size()); ++i) {
        const NetPipe& p = ns.pipes[i];
        if (room_index_at(ns, p.x0, p.y0) == r ||
            room_index_at(ns, p.x1, p.y1) == r) {
            result.push_back(i);
        }
    }
    // Already ascending because we iterate i = 0..n-1.
    return result;
}

// ---------------------------------------------------------------------------
// pipe_path_cells  (exact recorded cells, oriented near→far)
// ---------------------------------------------------------------------------

std::vector<std::pair<int,int>> pipe_path_cells(const Netspace& ns,
                                                 int pipe_idx, int ax, int ay) {
    if (pipe_idx < 0 || pipe_idx >= static_cast<int>(ns.pipes.size()))
        return {};

    const NetPipe& p = ns.pipes[pipe_idx];
    if (p.cells.empty()) return {};

    const int rA  = room_index_at(ns, p.x0, p.y0);
    const int rB  = room_index_at(ns, p.x1, p.y1);
    const int rav = room_index_at(ns, ax, ay);

    if (rA == rB && rA == rav) {
        // Degenerate: both endpoints map to the same room (current grammars
        // never produce this, but guard anyway).
        return p.cells;
    }

    if (rav == rA) {
        // Avatar at A end — cells already stored A→B (near→far).
        return p.cells;
    } else if (rav == rB) {
        // Avatar at B end — reverse to get B→A (near→far).
        auto rev = p.cells;
        std::reverse(rev.begin(), rev.end());
        return rev;
    }

    // Avatar room is neither endpoint room — pipe not reachable from here.
    return {};
}

}  // namespace astra
