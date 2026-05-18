#include "astra/grammars/gen_combat_netspace.h"

#include "astra/net_pipe_path.h"
#include "astra/net_room.h"
#include "astra/net_theme.h"
#include "astra/netspace_layout.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace astra {

namespace {

constexpr int kRoomW  = 7;
constexpr int kRoomH  = 5;
constexpr int kMargin = 3;

// Pipe-length tuning knobs. Stations sit at these gaps from JACK;
// connect()/connect_vertical stamps that many pipe cells, and Slice-4
// clamp_seg_len maps the drawn length into [2,6]. The bench MUST span
// both clamp bounds — selftest `s4ar-seg-span` (Task 3) asserts
// min==2 && max==6. If that fails after a build, widen kGapLongS until
// the longest pipe clamps to 6 and/or narrow kGapShortE until the
// shortest clamps to 2.
constexpr int kGapShortE = 2;    // JACK -> WHITE (east)  : short -> seg 2
constexpr int kGapMidN   = 4;    // JACK -> GRAY  (north) : mid   -> seg 4
constexpr int kGapLongS  = 9;    // JACK -> BLACK (south) : long  -> seg 6 (ceiling)
constexpr int kGapWallW  = 4;    // JACK -> VAULT (west)  : mid   -> seg ~4 + breakwall

uint8_t wall_density(int tier) { return tier <= 1 ? 3 : 4; }

}  // namespace

Netspace gen_combat_netspace(const TargetDescriptor& desc) {
    const int tier = desc.tier;

    // Horizontal:  VAULT(W) | JACK | WHITE(E).
    // Vertical:    GRAY(N) / JACK / BLACK(S, long).
    const int jack_x  = kMargin + kRoomW + kGapWallW;       // leave room for VAULT west
    const int gray_y  = kMargin;
    const int jack_y  = gray_y + kRoomH + kGapMidN;
    const int black_y = jack_y + kRoomH + kGapLongS;
    const int width   = jack_x + kRoomW + kGapShortE + kRoomW + kMargin;
    const int height  = black_y + kRoomH + kMargin;

    NetspaceBuilder b(width, height, NetTile::Void);
    b.set_target(desc);

    char title[64];
    std::snprintf(title, sizeof title, "COMBAT BENCH :: TIER %d", tier);
    b.set_title(title);

    b.ns.rooms.reserve(5);

    NetRoom& jack = b.add_room(jack_x, jack_y, kRoomW, kRoomH, "JACK",
                               NetRoom::Border::Thin);
    jack.label_color = net_theme::box_thin_color;
    b.set_jack_in(jack);                       // sets ns.jack_in_x/y

    NetRoom& white = b.add_room(jack_x + kRoomW + kGapShortE, jack_y,
                                kRoomW, kRoomH, "WHITE", NetRoom::Border::Thin);
    white.label_color = net_theme::box_thin_color;

    NetRoom& gray = b.add_room(jack_x, gray_y, kRoomW, kRoomH, "GRAY",
                               NetRoom::Border::Thin);
    gray.label_color = net_theme::box_thin_color;

    NetRoom& black = b.add_room(jack_x, black_y, kRoomW, kRoomH, "BLACK",
                                NetRoom::Border::Thin);
    black.label_color = net_theme::box_thin_color;

    NetRoom& vault = b.add_room(kMargin, jack_y, kRoomW, kRoomH, "VAULT",
                                NetRoom::Border::Thin);
    vault.label_color = net_theme::box_thin_color;

    // All four pipes attach to JACK -> connected_pipe_indices(jack)==4.
    b.connect(jack, white, NetPipe::Style::Double);            // SHORT (east)
    b.connect_vertical(jack, gray, NetPipe::Style::Double);     // MID   (north)
    b.connect_vertical(jack, black, NetPipe::Style::Double);    // LONG  (south)
    const int wall_idx = static_cast<int>(b.ns.pipes.size());
    b.connect(jack, vault, NetPipe::Style::Double);             // WALL  (west)

    // Breakwall at EXACTLY the Slice-4 Impact cell of the WALL pipe:
    // confirm_armed targets pipe_path_cells(...).back(); impact_resolve
    // demotes only when breakwall_lookup.count({tx,ty}) is true. Placing
    // the tile on that cell makes each WALL-pipe Impact degrade the wall.
    auto wpath = pipe_path_cells(b.ns, wall_idx,
                                 b.ns.jack_in_x, b.ns.jack_in_y);
    if (!wpath.empty()) {
        b.add_breakwall_tile(wpath.back().first, wpath.back().second,
                             wall_density(tier));
    }

    return b.finalize();
}

}  // namespace astra
