#include "astra/grammars/gen_combat_netspace.h"

#include "astra/net_pipe_path.h"
#include "astra/net_room.h"
#include "astra/net_theme.h"
#include "astra/netspace_layout.h"

#include <algorithm>
#include <cstdio>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace astra {

namespace {

constexpr int kRoomW  = 7;
constexpr int kRoomH  = 5;
constexpr int kMargin = 3;

// Pipe-length tuning knobs. Stations sit at these gaps from JACK;
// connect()/connect_vertical stamps that many pipe cells (raw == gap+2),
// and Slice-4 clamp_seg_len maps the drawn length into [2,6]. The bench
// MUST span both clamp bounds — selftest `s4ar-seg-span` (Task 3) asserts
// min==2 && max==6. Tuned to the design table {2,4,6,4}: gap+2 -> raw,
// then clamp [2,6]. clamp_seg_len's floor of 2 makes the short pipe read
// 2 even if a gap-0 pipe is degenerate. If `s4ar-seg-span` ever fails,
// widen kGapLongS / shrink kGapShortE and rebuild.
constexpr int kGapShortE = 0;    // JACK -> WHITE (east)  : raw 2 -> seg 2 (clamp floor)
constexpr int kGapMidN   = 2;    // JACK -> GRAY  (north) : raw 4 -> seg 4
constexpr int kGapLongS  = 9;    // JACK -> BLACK (south) : raw 11 -> seg 6 (clamp ceiling)
constexpr int kGapWallW  = 2;    // JACK -> VAULT (west)  : raw 4 -> seg 4 + breakwall

uint8_t wall_density(int tier) { return tier <= 1 ? 3 : 4; }

// Tier-scaled roster (see .claude/specs/netspace-combat-arena-spec.md). Bidirectional since S3: the GRAY station shoots back.
int gray_pack(int tier)    { return 2 + tier / 2; }            // t1=2 t2=3 t3=3 t4=4 t5=4
int white_count(int tier)  { return tier >= 2 ? 2 : 1; }
int black_count(int tier)  { return tier >= 3 ? 2 : 1; }
int white_hp(int tier)     { return 1 + (tier >= 4 ? 1 : 0); }
int gray_hp(int tier)      { return 2 + tier / 2; }
int black_hp(int tier)     { return 4 + tier; }

// Far-node (Impact) cell of pipe idx from JACK. Player payloads resolve here (radius-0 hits the seeded ICE);
// since S3 a Gray seeded here also casts back down this pipe at JACK. {-1,-1} if no path.
std::pair<int,int> pipe_far_cell(NetspaceBuilder& b, int idx) {
    auto p = pipe_path_cells(b.ns, idx, b.ns.jack_in_x, b.ns.jack_in_y);
    if (p.empty()) return {-1, -1};
    return p.back();
}

// Anchor `count` ICE on/around a station's Impact cell (cx,cy): ICE #0
// sits EXACTLY on it (radius-0 programs landing there hit), the rest
// spill outward in Chebyshev rings (within RELAY range) so AOE/chain
// programs sweep the pack too. Deterministic; seed jitters intra-ring
// order only. Never doubles a cell or goes out of bounds.
//
// NOTE: since S3 this bench is bidirectional — a Gray seeded on a JACK-connected node casts back
// (net_combat.cpp ice_cast_tick). The anchor-on-Impact-cell placement still lets a radius-0 player program hit it.
void seed_ice(NetspaceBuilder& b, int cx, int cy, IceColor color,
              int count, int hp, uint32_t salt) {
    if (count <= 0 || cx < 0 || cy < 0) return;
    std::vector<std::pair<int,int>> cand;
    cand.emplace_back(cx, cy);                         // anchor first
    for (int r = 1; r <= 3 && static_cast<int>(cand.size()) < count; ++r) {
        std::vector<std::pair<int,int>> ring;
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                int ax = dx < 0 ? -dx : dx;
                int ay = dy < 0 ? -dy : dy;
                if ((ax > ay ? ax : ay) != r) continue;   // ring edge only
                int x = cx + dx, y = cy + dy;
                if (b.ns.in_bounds(x, y)) ring.emplace_back(x, y);
            }
        std::mt19937 rng(b.ns.target.seed ^ salt
                         ^ static_cast<uint32_t>(r));
        std::shuffle(ring.begin(), ring.end(), rng);
        for (auto& c : ring) cand.push_back(c);
    }
    int placed = 0;
    for (auto& c : cand) {
        if (placed >= count) break;
        bool taken = false;
        for (auto& ic : b.ns.initial_ice)
            if (ic.x == c.first && ic.y == c.second) { taken = true; break; }
        if (taken) continue;
        Ice ic;
        ic.x = c.first;
        ic.y = c.second;
        ic.color = color;
        ic.hp = hp;
        b.ns.initial_ice.push_back(ic);
        ++placed;
    }
}

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
    const int white_idx = static_cast<int>(b.ns.pipes.size());
    b.connect(jack, white, NetPipe::Style::Double);            // SHORT (east)
    const int gray_idx  = static_cast<int>(b.ns.pipes.size());
    b.connect_vertical(jack, gray, NetPipe::Style::Double);     // MID   (north)
    const int black_idx = static_cast<int>(b.ns.pipes.size());
    b.connect_vertical(jack, black, NetPipe::Style::Double);    // LONG  (south)
    const int wall_idx  = static_cast<int>(b.ns.pipes.size());
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

    // Tier-scaled ICE roster, anchored on each station pipe's Slice-4
    // Impact cell so a radius-0 program landing there actually hits (the
    // bench's whole point). NOTE: bidirectional since S3 — the GRAY pack both takes player payloads here AND casts back at JACK.
    auto wf = pipe_far_cell(b, white_idx);
    auto gf = pipe_far_cell(b, gray_idx);
    auto bf = pipe_far_cell(b, black_idx);
    seed_ice(b, wf.first, wf.second, IceColor::White,
             white_count(tier), white_hp(tier), 0x5711u);
    seed_ice(b, gf.first, gf.second, IceColor::Gray,
             gray_pack(tier),  gray_hp(tier),  0x6712u);
    seed_ice(b, bf.first, bf.second, IceColor::Black,
             black_count(tier), black_hp(tier), 0x6713u);

    return b.finalize();
}

}  // namespace astra
