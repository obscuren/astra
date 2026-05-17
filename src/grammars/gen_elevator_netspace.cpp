#include "astra/grammars/gen_elevator_netspace.h"

#include "astra/net_ice.h"
#include "astra/net_room.h"
#include "astra/net_theme.h"
#include "astra/netspace_layout.h"

#include <cstdio>
#include <string>

namespace astra {

namespace {

// Layout constants — vertical stack of full-width floor rooms separated by
// a 2-cell vertical gap through which the spine pipe runs.
constexpr int kFloorW  = 28;
constexpr int kFloorH  = 5;
constexpr int kGapV    = 2;
constexpr int kMargX   = 2;
constexpr int kMargY   = 2;

// Spine column sits at the horizontal centre of every floor room.
constexpr int kSpineX  = kMargX + kFloorW / 2;  // = 16

// Total canvas width: left margin + floor room + right margin + 1 exit cell.
// Exit is placed one cell outside the right wall of the LOBBY, no PipeH run.
constexpr int kCanvasW = kMargX + kFloorW + kMargX + 1;

// y-top of floor k (0 = LOBBY at bottom, n_floors-1 = PENTHOUSE at top).
// Floors are laid out so that floor 0 has the highest y value (bottom of
// canvas) and floor n_floors-1 has the lowest y value (top of canvas).
int floor_top_y(int n_floors, int k) {
    return kMargY + (n_floors - 1 - k) * (kFloorH + kGapV);
}

// Canvas height needed for n_floors stacked floors.
int canvas_height(int n_floors) {
    return kMargY + n_floors * kFloorH + (n_floors - 1) * kGapV + kMargY;
}

// "LIFT_03"-style id — two decimal digits from seed.
std::string lift_id(uint32_t seed) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "LIFT_%02d", static_cast<int>(seed % 100u));
    return buf;
}

// Floor label names indexed from 0 (LOBBY) upward.
const char* floor_label(int k, int n_floors) {
    // LOBBY always 0, PENTHOUSE always top.
    if (k == 0)            return "LOBBY";
    if (k == n_floors - 1) return "PENTHOUSE";
    // k == mid → SECURITY; k == mid+1 (if present) → OFFICE.
    const int mid = n_floors / 2;
    if (k == mid)          return "SECURITY";
    if (k == mid + 1)      return "OFFICE";
    return "FLOOR";
}

}  // namespace (anonymous)

// ---------------------------------------------------------------------------
// elevator_floor_for_y
// ---------------------------------------------------------------------------
// Given ns and a y-row, return the floor index (0 = LOBBY at bottom,
// increasing upward). Uses the same floor_top_y math. For y below the LOBBY
// we return 0. For y above the PENTHOUSE top border (the top margin) we
// clamp to n_floors-1.
//
// Gap rows (the 2-cell vertical gaps between floor rooms through which the
// spine pipe runs) resolve to the nearest floor ABOVE (the ascent destination),
// because the search walks k from 0 upward and breaks at the first floor whose
// top is at or below y — which for a gap cell is the upper floor (smaller k
// wrapping around to a lower fy than the gap).

int elevator_floor_for_y(const Netspace& ns, int y) {
    const int n = ns.floor_count;
    if (n <= 0) return 0;
    // Search from floor 0 (LOBBY, largest fy) upward. floor_top_y decreases
    // as k increases. Break at the first k whose top y <= the query y; that
    // is the highest-indexed floor still at or below y.
    int best = 0;
    for (int k = 0; k < n; ++k) {
        const int fy = floor_top_y(n, k);
        if (y >= fy) {
            best = k;
            break;    // floor k is the highest floor whose top is <= y
        }
    }
    // y in the top margin (above PENTHOUSE's top border) matched no floor;
    // clamp to the top floor so callers get a sane in-range index.
    if (best == 0 && n > 1 && y < floor_top_y(n, n - 1)) best = n - 1;
    return best;
}

// ---------------------------------------------------------------------------
// gen_elevator_netspace
// ---------------------------------------------------------------------------

Netspace gen_elevator_netspace(const TargetDescriptor& desc) {
    const int n_floors = 4 + (desc.tier >= 3 ? 2 : 0);  // 4 or 6
    const int mid      = n_floors / 2;                   // SECURITY gate floor index
    const int height   = canvas_height(n_floors);

    NetspaceBuilder b(kCanvasW, height, NetTile::Void);
    b.set_target(desc);

    char title_buf[64];
    // TODO: parameterize corp per fixture/location in the lore pass.
    std::snprintf(title_buf, sizeof title_buf,
                  "KONPEKI PLAZA :: %s :: FLOOR ?", lift_id(desc.seed).c_str());
    b.set_title(title_buf);

    // Reserve stable references across add_room calls.
    b.ns.rooms.reserve(static_cast<size_t>(n_floors));

    b.ns.floor_count      = n_floors;
    b.ns.press_luck_step  = 8;

    // ── Build floor rooms bottom→top ────────────────────────────────────
    // We must build from bottom (k=0) to top (k=n_floors-1) and connect
    // each consecutive pair via the spine (upward direction = decreasing y).
    for (int k = 0; k < n_floors; ++k) {
        const int fy = floor_top_y(n_floors, k);
        const char* lbl = floor_label(k, n_floors);
        NetRoom& room = b.add_room(kMargX, fy, kFloorW, kFloorH, lbl,
                                   NetRoom::Border::Thin);
        room.label_color = net_theme::box_thin_color;

        if (k == 0) {
            // LOBBY — jack-in, avatar spawns at interior focus (bottom row).
            b.set_jack_in(room);

            // LOBBY exit (canonical): one cell outside the right wall at the
            // jack-in row.  make_passable opens the border cell so the avatar
            // can step out without a PipeH run.  floor==0 ⟹ no trace cost.
            const int exit_y = b.ns.jack_in_y;
            const int right_wall_x = kMargX + kFloorW - 1;
            const int exit_x       = kMargX + kFloorW;    // immediately right of wall
            b.make_passable(right_wall_x, exit_y);
            b.ns.set(exit_x, exit_y, NetTile::Exit);
            b.ns.exit_x = exit_x;
            b.ns.exit_y = exit_y;
        } else {
            // Upper-floor exit — press-your-luck side-step.
            //
            // Placement: same right-wall idiom as the LOBBY, but at this
            // floor's interior focus row (fy + kFloorH - 2).  The spine
            // traversal column is kSpineX=16; the exit is at x=kMargX+kFloorW
            // (outside the right wall at x=kMargX+kFloorW-1=29), more than
            // 13 cells away from the spine — unreachable without a deliberate
            // rightward walk.
            //
            // The vertical spine sweep for connect_vertical(floor_k, floor_{k+1})
            // runs from the upper room's bottom border (fy_{k+1}+4) to the lower
            // room's top border (fy_k), both of which are BoxThin and skipped.
            // Only the two gap cells at fy_{k+1}+5 and fy_{k+1}+6 receive PipeV.
            // The floor interior rows fy_k+1..fy_k+3 are NOT in that sweep range
            // and stay as plain Floor.  The exit cell at (right_wall_x, fy_k+3)
            // is 13 columns right of the spine and is never visited during normal
            // ascend/descend traversal.
            //
            // No PipeH is used: make_passable + adjacent Exit, same as LOBBY.
            const int focus_y      = fy + kFloorH - 2;    // interior focus (bottom interior row)
            const int right_wall_x = kMargX + kFloorW - 1; // right border of the floor room
            const int upper_exit_x = kMargX + kFloorW;     // one cell outside right wall
            b.make_passable(right_wall_x, focus_y);
            b.ns.set(upper_exit_x, focus_y, NetTile::Exit);
            // ns.exit_x/exit_y remain the canonical LOBBY exit (floor 0); the
            // upper-floor Exits are additional jack-out triggers only.
        }
    }

    // ── Spine: connect consecutive floor pairs via vertical pipes ────────
    // connect_vertical(lower, upper): lower room has higher y, upper has lower y.
    // stamp_v runs along kSpineX through the gap between them (pure PipeV).
    for (int k = 0; k < n_floors - 1; ++k) {
        NetRoom& lower = b.ns.rooms[static_cast<size_t>(k)];
        NetRoom& upper = b.ns.rooms[static_cast<size_t>(k + 1)];
        b.connect_vertical(lower, upper, NetPipe::Style::Double);
    }

    // ── SECURITY breakwall: gate ascent past floor `mid` ────────────────
    // Place a breakwall tile on the spine column, one cell above the
    // SECURITY room's top border (i.e. in the gap between SECURITY and
    // the floor above). This blocks upward traversal along the spine
    // without affecting the SECURITY room's interior or any lower floor.
    {
        const int security_top_y = floor_top_y(n_floors, mid);
        // The gap cell directly above SECURITY's top border:
        const int bw_y = security_top_y - 1;
        b.add_breakwall_tile(kSpineX, bw_y, /*density=*/3);  // ▒ = mid threat
    }

    // ── ICE + Stash nodes for floors 1..n_floors-1 ──────────────────────
    for (int k = 1; k < n_floors; ++k) {
        const int fy       = floor_top_y(n_floors, k);
        const int mid_y    = fy + kFloorH / 2;      // middle interior row (fy+2)
        const int ice_n    = k / 2;  // floor 1 → 0 ICE intentional: easing-in floor, not an off-by-one
        const int interior_left  = kMargX + 2;      // well inside left border
        const int interior_right = kMargX + kFloorW - 3;  // well inside right border

        // ICE: spread across the middle row, left of spine.
        for (int i = 0; i < ice_n; ++i) {
            Ice g;
            g.color = IceColor::Gray;
            g.hp    = 2;
            g.x     = interior_left + i * 3;
            g.y     = mid_y;
            b.ns.initial_ice.push_back(g);
        }

        // Stash node: right side of the floor interior, middle row.
        NetNode loot;
        loot.kind    = NetNodeKind::Stash;
        loot.payload = static_cast<uint32_t>(20 + k * 25);
        loot.label   = "$";
        loot.x       = interior_right;
        loot.y       = mid_y;
        b.ns.action_nodes.push_back(loot);
    }

    return b.finalize();
}

}  // namespace astra
