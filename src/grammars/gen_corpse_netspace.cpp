#include "astra/grammars/gen_corpse_netspace.h"

#include "astra/net_room.h"
#include "astra/net_theme.h"
#include "astra/netspace_layout.h"

#include <string>

namespace astra {

namespace {

// Baked, art-directed zalgo strings (exact glyphs supplied by design).
// Combining marks are written AFTER their base char; string-literal
// concatenation keeps each \xHH escape from swallowing the next hex digit.
// Marks used: U+0334 ̴=\xcc\xb4  U+0336 ̶=\xcc\xb6  U+0337 ̷=\xcc\xb7  U+0338 ̸=\xcc\xb8

// "M̷E̷M̶O̴R̷Y̶"
const char* kMemoryLabel =
    "M" "\xcc\xb7" "E" "\xcc\xb7" "M" "\xcc\xb6"
    "O" "\xcc\xb4" "R" "\xcc\xb7" "Y" "\xcc\xb6";

// "G̷H̷O̴S̴T̷/"
const char* kGhostLabel =
    "G" "\xcc\xb7" "H" "\xcc\xb7" "O" "\xcc\xb4"
    "S" "\xcc\xb4" "T" "\xcc\xb7" "/";

// " ̴a̷v̷a̴t̸a̷r̶"  (leading space carries the first mark)
const char* kGhostAvatar =
    " " "\xcc\xb4" "a" "\xcc\xb7" "v" "\xcc\xb7"
    "a" "\xcc\xb4" "t" "\xcc\xb8" "a" "\xcc\xb7" "r" "\xcc\xb6";

// Title: "UNKNOWN DECK :: OWNER: K̴̴.̷ ̶R̴E̸N̸̷N̷E̷R̴ :: STATUS: DEAD"
std::string build_title() {
    const std::string owner =
        "K" "\xcc\xb4\xcc\xb4" "." "\xcc\xb7" " " "\xcc\xb6"
        "R" "\xcc\xb4" "E" "\xcc\xb8" "N" "\xcc\xb8\xcc\xb7"
        "N" "\xcc\xb7" "E" "\xcc\xb7" "R" "\xcc\xb4";
    return "UNKNOWN DECK :: OWNER: " + owner + " :: STATUS: DEAD";
}

const char* kNoiseRow = "\xe2\x96\x93 \xe2\x96\x92 \xe2\x96\x91";  // ▓ ▒ ░
const char* kRow7 = "\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91"; // ░░░░░░░

// Layout — canonical §Corpse. Stash/Ghost centres are aligned to the
// JACK/PROG centre columns so the vertical spines run dead straight and
// attach at room centres (never corners). MEMORY is a solid ▓ blob with
// NO box-drawing border and NO pipes inside it.
constexpr int kMargX = 3;
constexpr int kMargY = 2;
constexpr int kJackW  = 9,  kJackH  = 5;
constexpr int kMemW   = 15, kMemH   = 5;
constexpr int kProgW  = 11, kProgH  = 5;
constexpr int kHubW   = 11, kHubH   = 5;
constexpr int kStashW = 9,  kStashH = 5;   // W=9 → centre aligns with JACK spine
constexpr int kGhostW = 11, kGhostH = 6;   // W=11 → centre aligns with PROG spine; H+1 for ░▒▓
constexpr int kHGap   = 4;
constexpr int kVGap   = 5;

}  // namespace

Netspace gen_corpse_netspace(const TargetDescriptor& desc) {
    const int col0_x = kMargX;
    const int col1_x = col0_x + kJackW + kHGap;     // MEMORY / LAST RUN
    const int col2_x = col1_x + kMemW  + kHGap;     // PROG / GHOST
    const int row0_y = kMargY;
    const int row1_y = row0_y + kJackH + kVGap;     // LAST RUN
    const int row2_y = row1_y + kHubH  + kVGap;     // STASH / GHOST
    const int width  = col2_x + kProgW + kMargX;
    const int height = row2_y + kGhostH + kMargY + 2;

    NetspaceBuilder b(width, height, NetTile::Void);
    b.set_target(desc);
    b.set_title(build_title());
    b.ns.rooms.reserve(6);

    // ── Rooms ───────────────────────────────────────────────────────────
    NetRoom& jack = b.add_room(col0_x, row0_y, kJackW, kJackH, "JACK",
                               NetRoom::Border::Thin);
    jack.label_color    = net_theme::box_thin_color;
    jack.bottom_content = "";                 // set_jack_in writes @ here
    jack.bottom_color   = net_theme::avatar;
    b.set_jack_in(jack);

    // MEMORY — registered only for the label/noise overlay; every tile
    // (border + interior) is overwritten with ▓ at the end so it reads as
    // a solid corruption blob (no box corners) and is unenterable.
    NetRoom& memory = b.add_room(col1_x, row0_y, kMemW, kMemH, kMemoryLabel,
                                 NetRoom::Border::Thin);
    memory.label_color    = net_theme::wall_heavy;
    memory.top_content    = kNoiseRow;
    memory.top_color      = net_theme::wall_heavy;
    memory.bottom_content = kNoiseRow;
    memory.bottom_color   = net_theme::wall_heavy;

    NetRoom& prog = b.add_room(col2_x, row0_y, kProgW, kProgH, "\xc2\xa7\xc2\xa7\xc2\xa7",
                               NetRoom::Border::Thin);
    prog.label_color    = net_theme::data_node;
    prog.top_content    = "\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91";  // ░░░░░
    prog.top_color      = net_theme::data_node;
    prog.bottom_content = "PROG";
    prog.bottom_color   = net_theme::box_thin_color;

    NetRoom& hub = b.add_room(col1_x, row1_y, kHubW, kHubH, "LAST RUN",
                              NetRoom::Border::Thin);
    hub.label_color    = net_theme::box_thin_color;
    hub.top_content    = kRow7;
    hub.top_color      = net_theme::box_thin_color;
    hub.bottom_content = kRow7;
    hub.bottom_color   = net_theme::box_thin_color;

    NetRoom& stash = b.add_room(col0_x, row2_y, kStashW, kStashH, "STASH?",
                                NetRoom::Border::Thin);
    stash.label_color    = net_theme::box_thin_color;
    stash.bottom_content = "\xe2\x97\x8a\xe2\x97\x8a\xe2\x97\x8a";  // ◊◊◊
    stash.bottom_color   = Color::Yellow;

    NetRoom& ghost = b.add_room(col2_x, row2_y, kGhostW, kGhostH, kGhostLabel,
                                NetRoom::Border::Thin);
    ghost.label_color    = net_theme::box_thin_color;
    ghost.top_content    = "\xe2\x96\x91 \xe2\x96\x92 \xe2\x96\x93";  // ░ ▒ ▓
    ghost.top_color      = Color::DarkGray;
    ghost.bottom_content = kGhostAvatar;
    ghost.bottom_color   = net_theme::box_thin_color;

    // ── Hand-stamped pipe network (full control; pipe glyphs are uniform
    //    so routing — not Style — is what matters) ───────────────────────
    const int jx       = jack.x + kJackW / 2;          // col0 spine column
    const int px        = prog.x + kProgW / 2;          // col2 spine column
    const int top_y     = jack.y + kJackH / 2;          // JACK═══MEMORY═══PROG row
    const int hub_mid   = hub.y + kHubH / 2;            // LAST RUN tee row
    const int jack_bot  = jack.y + kJackH - 1;
    const int prog_bot  = prog.y + kProgH - 1;
    const int stash_top = stash.y;
    const int ghost_top = ghost.y;
    const int hub_l     = hub.x;
    const int hub_r     = hub.x + kHubW - 1;

    auto hrun = [&](int y, int x0, int x1) {
        for (int x = x0; x <= x1; ++x) {
            NetTile cur = b.ns.at(x, y);
            b.ns.set(x, y, cur == NetTile::PipeV ? NetTile::PipeJunc
                                                 : NetTile::PipeH);
        }
    };
    auto vrun = [&](int x, int y0, int y1) {
        for (int y = y0; y <= y1; ++y) {
            NetTile cur = b.ns.at(x, y);
            b.ns.set(x, y, cur == NetTile::PipeH ? NetTile::PipeJunc
                                                 : NetTile::PipeV);
        }
    };

    // (1) Top row: pipe only in the GAPS — never inside MEMORY.
    b.make_passable(jack.x + kJackW - 1, top_y);          // JACK right port
    hrun(top_y, jack.x + kJackW, memory.x - 1);            // JACK ═══ MEMORY
    hrun(top_y, memory.x + kMemW, prog.x - 1);             // MEMORY ═══ PROG
    b.make_passable(prog.x, top_y);                        // PROG left port

    // (9) col0 spine: JACK bottom-centre → STASH top-centre (straight).
    b.make_passable(jx, jack_bot);
    vrun(jx, jack_bot + 1, stash_top - 1);
    b.make_passable(jx, stash_top);
    // col2 spine: PROG bottom-centre → GHOST top-centre (straight).
    b.make_passable(px, prog_bot);
    vrun(px, prog_bot + 1, ghost_top - 1);
    b.make_passable(px, ghost_top);

    // (2) LAST RUN tees: full-length horizontals from each spine into the
    //     hub's left/right border (these used to be a 2-cell stub).
    hrun(hub_mid, jx, hub_l);                              // col0 spine ─── hub left
    b.make_passable(hub_l, hub_mid);
    hrun(hub_mid, hub_r, px);                              // hub right ─── col2 spine
    b.make_passable(hub_r, hub_mid);

    // ── Jack-out stub off JACK's LEFT wall ──────────────────────────────
    {
        const int port_y = jack.y + kJackH / 2;
        b.make_passable(jack.x, port_y);
        b.ns.set(jack.x - 1, port_y, NetTile::PipeH);
        b.ns.set(jack.x - 2, port_y, NetTile::Exit);
        b.ns.exit_x = jack.x - 2;
        b.ns.exit_y = port_y;
        jack.is_exit = true;
    }

    // ── MEMORY → solid ▓ blob (overwrites border + any pipe that strayed
    //    into its rect; clears box corners; keeps it unenterable) ────────
    for (int yy = memory.y; yy < memory.y + memory.h; ++yy)
        for (int xx = memory.x; xx < memory.x + memory.w; ++xx)
            b.ns.set(xx, yy, NetTile::WallHeavy);

    // ── Action nodes ────────────────────────────────────────────────────
    {
        NetNode nd;
        nd.x = stash.x + kStashW / 2;
        nd.y = stash.y + kStashH / 2;
        nd.kind    = NetNodeKind::Stash;
        nd.label   = "\xe2\x97\x8a\xe2\x97\x8a\xe2\x97\x8a";  // ◊◊◊
        nd.payload = desc.seed & 0xFFFFu;
        b.ns.action_nodes.push_back(nd);
    }
    {
        NetNode nd;
        nd.x = ghost.x + kGhostW / 2;
        nd.y = ghost.y + kGhostH / 2;
        nd.kind    = NetNodeKind::GhostTalk;
        nd.label   = kGhostLabel;
        nd.payload = desc.seed;
        b.ns.action_nodes.push_back(nd);
    }

    return b.finalize();
}

}  // namespace astra
