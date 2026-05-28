#include "astra/grammars/gen_corpse_netspace.h"

#include "astra/grammars/seed_daemon.h"
#include "astra/net_room.h"
#include "astra/net_theme.h"
#include "astra/netspace_layout.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

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

// Phase 5 S7c.2: tier-scaled stats for MEMRY.kex.
struct MemryKexTier { int hp; int windup; };
constexpr MemryKexTier kMemryKexTiers[5] = {
    /*T1*/{6,5}, /*T2*/{7,5}, /*T3*/{7,4}, /*T4*/{8,4}, /*T5*/{8,3}
};

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

    // Phase 5 S7e: hrun/vrun gain an optional `cells` outparam so the
    // grammar can collect the painted tiles into a NetPipe.cells
    // vector while still painting them. Existing call sites without
    // the outparam pass nullptr (default) and behave identically.
    auto hrun = [&](int y, int x0, int x1,
                    std::vector<std::pair<int,int>>* cells = nullptr) {
        for (int x = x0; x <= x1; ++x) {
            NetTile cur = b.ns.at(x, y);
            b.ns.set(x, y, cur == NetTile::PipeV ? NetTile::PipeJunc
                                                 : NetTile::PipeH);
            if (cells) cells->emplace_back(x, y);
        }
    };
    auto vrun = [&](int x, int y0, int y1,
                    std::vector<std::pair<int,int>>* cells = nullptr) {
        for (int y = y0; y <= y1; ++y) {
            NetTile cur = b.ns.at(x, y);
            b.ns.set(x, y, cur == NetTile::PipeH ? NetTile::PipeJunc
                                                 : NetTile::PipeV);
            if (cells) cells->emplace_back(x, y);
        }
    };

    // (1) Top row: pipe only in the GAPS — never inside MEMORY.
    b.make_passable(jack.x + kJackW - 1, top_y);          // JACK right port
    std::vector<std::pair<int,int>> jack_memory_cells;
    hrun(top_y, jack.x + kJackW, memory.x - 1, &jack_memory_cells);
    {
        // Phase 5 S7e: register JACK↔MEMORY pipe. MEMORY is a
        // WallHeavy blob with no Floor interior, but room_index_at
        // resolves cells inside MEMORY's rect to MEMORY's index --
        // so node-scoped Impact at the far-end engages MEMRY.kex.
        NetPipe p;
        p.x0 = jack.x + kJackW - 1; p.y0 = top_y;
        p.x1 = memory.x;            p.y1 = top_y;
        p.style = NetPipe::Style::Thin;
        p.cells = std::move(jack_memory_cells);
        b.ns.pipes.push_back(std::move(p));
    }
    std::vector<std::pair<int,int>> memory_prog_cells;
    hrun(top_y, memory.x + kMemW, prog.x - 1, &memory_prog_cells);
    b.make_passable(prog.x, top_y);                        // PROG left port
    {
        // Phase 5 S7e: register MEMORY↔PROG pipe (mirror of the
        // JACK↔MEMORY pipe — engages MEMRY.kex from the PROG side).
        NetPipe p;
        p.x0 = memory.x + kMemW - 1; p.y0 = top_y;
        p.x1 = prog.x;               p.y1 = top_y;
        p.style = NetPipe::Style::Thin;
        p.cells = std::move(memory_prog_cells);
        b.ns.pipes.push_back(std::move(p));
    }

    // (9) col0 + col2 spines and the LAST RUN tees — Phase 5 S7f.
    // Visual: still a full vertical spine in each column with a
    // horizontal tee going into the hub at hub_mid (unchanged tile-
    // paint). Engine: each spine SPLITS into two NetPipes that
    // route THROUGH the hub junction, so the avatar in LAST RUN
    // sees 4 cast paths (JACK / STASH via col0; PROG / GHOST via
    // col2). Each spine's lower half is a separate pipe from its
    // upper half; the tee cells are shared between the two pipes
    // on that spine (a payload's `pipe_index` keys per-pipe so
    // they don't collide on shared cells).

    // Col0 spine: paint, collecting cells into separate upper/lower
    // half lists. Spine vrun + tee hrun + junction stamp.
    b.make_passable(jx, jack_bot);
    b.make_passable(jx, stash_top);
    b.make_passable(hub_l, hub_mid);
    std::vector<std::pair<int,int>> col0_up_cells;     // jack_bot+1 .. hub_mid-1
    std::vector<std::pair<int,int>> col0_down_cells;   // hub_mid+1 .. stash_top-1
    vrun(jx, jack_bot + 1, hub_mid - 1, &col0_up_cells);
    vrun(jx, hub_mid + 1, stash_top - 1, &col0_down_cells);
    std::vector<std::pair<int,int>> col0_tee_cells;    // jx+1 .. hub_l (going east into hub port)
    hrun(hub_mid, jx + 1, hub_l, &col0_tee_cells);
    // Junction at (jx, hub_mid): hrun didn't touch it (range starts
    // at jx+1); vrun didn't touch it either (gaps at hub_mid-1 and
    // hub_mid+1). Stamp it explicitly as PipeJunc.
    b.ns.set(jx, hub_mid, NetTile::PipeJunc);
    const std::pair<int,int> col0_junction = {jx, hub_mid};

    // Col2 spine: same shape, mirror direction (tee goes WEST from
    // hub right port at hub_r into the col2 spine).
    b.make_passable(px, prog_bot);
    b.make_passable(px, ghost_top);
    b.make_passable(hub_r, hub_mid);
    std::vector<std::pair<int,int>> col2_up_cells;
    std::vector<std::pair<int,int>> col2_down_cells;
    vrun(px, prog_bot + 1, hub_mid - 1, &col2_up_cells);
    vrun(px, hub_mid + 1, ghost_top - 1, &col2_down_cells);
    std::vector<std::pair<int,int>> col2_tee_cells;    // hub_r .. px-1 (going east from hub port into spine)
    hrun(hub_mid, hub_r, px - 1, &col2_tee_cells);
    b.ns.set(px, hub_mid, NetTile::PipeJunc);
    const std::pair<int,int> col2_junction = {px, hub_mid};

    // ── Pipe 3a: JACK ↔ LAST_RUN (col0 upper half + tee, JACK→hub) ─
    {
        std::vector<std::pair<int,int>> cells;
        // From JACK (jack_bot) going down the col0 upper-half spine:
        // col0_up_cells is ordered (jx, jack_bot+1), (jx, jack_bot+2), ..., (jx, hub_mid-1).
        cells.insert(cells.end(), col0_up_cells.begin(), col0_up_cells.end());
        cells.push_back(col0_junction);
        // Tee cells: col0_tee_cells is ordered (jx+1, hub_mid), ..., (hub_l, hub_mid).
        cells.insert(cells.end(), col0_tee_cells.begin(), col0_tee_cells.end());
        NetPipe p;
        p.x0 = jx;     p.y0 = jack_bot;
        p.x1 = hub_l;  p.y1 = hub_mid;
        p.style = NetPipe::Style::Thin;
        p.cells = std::move(cells);
        b.ns.pipes.push_back(std::move(p));
    }

    // ── Pipe 3b: LAST_RUN ↔ STASH (tee + col0 lower half, hub→STASH) ─
    {
        std::vector<std::pair<int,int>> cells;
        // From hub_l going west along the tee to the junction:
        // col0_tee_cells is ordered (jx+1, hub_mid), ..., (hub_l, hub_mid).
        // We want hub→junction, so reverse-iterate the tee cells.
        for (auto it = col0_tee_cells.rbegin(); it != col0_tee_cells.rend(); ++it)
            cells.push_back(*it);
        cells.push_back(col0_junction);
        // From junction continuing down: col0_down_cells is
        // (jx, hub_mid+1), ..., (jx, stash_top-1).
        cells.insert(cells.end(), col0_down_cells.begin(), col0_down_cells.end());
        NetPipe p;
        p.x0 = hub_l;  p.y0 = hub_mid;
        p.x1 = jx;     p.y1 = stash_top;
        p.style = NetPipe::Style::Thin;
        p.cells = std::move(cells);
        b.ns.pipes.push_back(std::move(p));
    }

    // ── Pipe 4a: PROG ↔ LAST_RUN (col2 upper half + tee, PROG→hub) ─
    {
        std::vector<std::pair<int,int>> cells;
        // From PROG (prog_bot) going down col2 upper-half spine:
        // col2_up_cells ordered (px, prog_bot+1), ..., (px, hub_mid-1).
        cells.insert(cells.end(), col2_up_cells.begin(), col2_up_cells.end());
        cells.push_back(col2_junction);
        // Tee cells: col2_tee_cells ordered (hub_r, hub_mid), ..., (px-1, hub_mid).
        // We want PROG→hub direction, which means junction→hub_r — reverse-iterate.
        for (auto it = col2_tee_cells.rbegin(); it != col2_tee_cells.rend(); ++it)
            cells.push_back(*it);
        NetPipe p;
        p.x0 = px;     p.y0 = prog_bot;
        p.x1 = hub_r;  p.y1 = hub_mid;
        p.style = NetPipe::Style::Thin;
        p.cells = std::move(cells);
        b.ns.pipes.push_back(std::move(p));
    }

    // ── Pipe 4b: LAST_RUN ↔ GHOST (tee + col2 lower half, hub→GHOST) ─
    {
        std::vector<std::pair<int,int>> cells;
        // From hub_r going east along the tee to the junction:
        // col2_tee_cells ordered (hub_r, hub_mid), ..., (px-1, hub_mid).
        cells.insert(cells.end(), col2_tee_cells.begin(), col2_tee_cells.end());
        cells.push_back(col2_junction);
        // From junction continuing down: col2_down_cells is
        // (px, hub_mid+1), ..., (px, ghost_top-1).
        cells.insert(cells.end(), col2_down_cells.begin(), col2_down_cells.end());
        NetPipe p;
        p.x0 = hub_r;  p.y0 = hub_mid;
        p.x1 = px;     p.y1 = ghost_top;
        p.style = NetPipe::Style::Thin;
        p.cells = std::move(cells);
        b.ns.pipes.push_back(std::move(p));
    }

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

    // Phase 5 S7c.2: MEMRY.kex daemon — the corruption blob becomes
    // attackable. The RoomFill renderer overdraws the WallHeavy tiles
    // with a density gradient sourced from daemon HP, so the visual
    // matches the existing ▓ blob until the player damages it.
    {
        const int t = std::clamp(desc.tier - 1, 0, 4);
        seed_daemon(b, memory, DaemonKind::MemryKex,
                    kMemryKexTiers[t].hp,
                    kMemryKexTiers[t].windup,
                    /*cast_dmg_override*/ 0);
    }

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
