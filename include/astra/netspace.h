#pragma once

// Netspace — the transient per-jack-in micro-dungeon a Drifter walks
// inside the cyberdeck overlay. One Netspace is generated each time the
// player jacks into a hackable target. Replaces the legacy single-Site /
// GridSector / GridNetwork geography.
//
// See docs/design/netspace.md for the full design. Phase 0 ships the
// abstraction and a blank-room stub generator; per-target grammars land
// in Phase 1+.

#include "astra/net_ice.h"
#include "astra/net_room.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace astra {

// What the Drifter is hacking. Drives which grammar generates the
// netspace and the visual / audio tone the overlay takes on.
enum class NetspaceTargetKind : uint8_t {
    Empty,          // Phase 0 stub — nothing in particular
    Door,
    VendingMachine,
    Camera,
    Atm,
    Turret,
    Elevator,
    TrafficLight,
    Corpse,         // dead NPC's cooling implant
    NpcHead,        // living NPC's neural jack
    Mainframe,
    BlackwallTear,  // endgame; rules break
    CombatArena,    // dev-only — :jack combat test bench (no in-world fixture)
};

// Per-jack-in target descriptor. Built by the caller of jack_in(); fed
// to the netspace generator. Per-phase, this struct grows (fixture id,
// npc id, faction tag, etc.).
struct TargetDescriptor {
    NetspaceTargetKind kind = NetspaceTargetKind::Empty;
    int                tier  = 1;      // 1..5 difficulty band
    uint32_t           seed  = 0;      // deterministic generation
    int                src_x = -1;    // meatworld source NPC cell; -1 = none (used by Turret)
    int                src_y = -1;
};

// Tiles inside the netspace. Vocabulary mirrors the design doc's
// "Visual Language Reference" — wall density gradient, box-drawing
// borders per threat tier, animated pipe segments, plus the basic
// floor/jack-in/exit set.
//
// Walls are impassable from `passable()`'s perspective regardless of
// density tier; the density variants exist for visual signalling
// (lock progression, ICE degradation per Phase 2).
enum class NetTile : uint8_t {
    Void,       // not part of the room — renders as nothing
    Floor,      // walkable interior
    JackIn,     // entry tile (avatar spawns here)
    Exit,       // step here -> jack_out

    // Wall density gradient (impassable).
    WallDot,    // · faint trace
    WallLight,  // ░ tier 1
    WallMed,    // ▒ tier 2
    WallHeavy,  // ▓ tier 3
    WallSolid,  // █ impassable / max (also: legacy default wall)

    // Box-drawing borders for boxed nodes (NetRoom). The renderer
    // resolves the per-cell glyph (corner / edge / junction) from the
    // 4-neighbour mask of like-typed tiles.
    BoxThin,    // ─ │ ┌ ┐ └ ┘ — civic, low-stakes
    BoxDouble,  // ═ ║ ╔ ╗ ╚ ╝ — corporate, mid-stakes
    BoxBlock,   // ▓ █ heavy block — military / boss

    // Animated data pipes (NetPipe overlays write these).
    PipeH,      // ─ horizontal segment
    PipeV,      // │ vertical segment
    PipeJunc,   // ┼ ┬ ┴ ├ ┤ junction
    PipePortV,  // box-border cell that accepts a vertical pipe — renders as
                // ╨ (thin horizontal + double-line up). Passable; used at
                // box-top intersections so the pipe terminates visibly at
                // the box border without overwriting it as PipeJunc's heavy ╬.
    PipePortCornerTR,  // top-right box corner that accepts a vertical pipe
                       // from above — renders as ╜ (double-up + thin-left).
                       // Passable; used where a pipe drops onto the very
                       // top-right corner of a thin-bordered box.
    PipePortDownD,     // double-box bottom-edge cell that emits a vertical
                       // pipe downward — renders as ╦ (double horizontal +
                       // double down). Passable; used where a pipe exits
                       // the bottom of a BoxDouble node (vending shelves).

    Glyph,      // tile carries an inline glyph; renderer reads it from
                // Netspace::glyph_overrides keyed by (x, y).

    Breakwall,  // breakable barrier — density lives on its BreakwallGroup
                // (looked up via Netspace::breakwall_lookup); impassable
                // until current_density == 0.
};

// Ambient animation overlay scaffold — grammars opt in by setting
// `Netspace::ambient`. Currently no variants ship; previous ScanLines
// effect was dropped. Kept so later phases (Blackwall drift, trace
// corruption) can re-introduce ambients without re-plumbing.
enum class NetspaceAmbient : uint8_t {
    None,
};

// Render mode of the in-net overlay. The overlay mutates its own rules
// based on this state — borders crawl, vitals lie, glyphs glitch. The
// underlying simulation stays honest; the *window rendering it* becomes
// unreliable as the Drifter's trace climbs.
//
// Phase 0 only uses Stable. Phase 3 brings the rest online.
enum class WindowState : uint8_t {
    Opening,           // jack-in ritual playing
    Stable,            // default rendering
    Stressed,          // trace 40-70%
    Hunted,            // trace 70-95%; readout begins to lie
    Critical,          // trace 95%+ or Black ICE inbound
    BlackIceTakeover,  // full-window override; ~1s
    Blackwall,         // wrong-rendering mode
    Closing,           // jack-out ritual playing
};

// One logical "breakable barrier" — may be one tile or many. Tiles in a
// group share density and demote together when any one is Breach-targeted.
// Grammars declare groups at gen time via NetspaceBuilder helpers; Netspace
// owns the state; renderer reads tiles only.
struct BreakwallGroup {
    uint8_t                          current_density = 0;   // 0=cleared, 1=·, 2=░, 3=▒, 4=▓, 5=█
    std::vector<std::pair<int, int>> tiles;
};

// Phase 4: declarative action nodes — interactable points of interest that
// grammars stamp into a netspace and the input layer resolves when the
// avatar steps on them.
enum class NetNodeKind : uint8_t { None = 0, TurretDisarm, TurretFlip, GhostTalk, Stash, VaultGrab };
struct NetNode {
    int x = 0;
    int y = 0;
    NetNodeKind kind = NetNodeKind::None;
    std::string label;  // render hint — unused until a later renderer pass
    uint32_t payload = 0;
    bool consumed = false;
};

// Phase 4: grammar triggers — one-shot ICE spawn events gated on trace
// level or turn count. Declared by grammars; evaluated each tick_grid.
enum class NetTriggerCond : uint8_t { TraceAtLeast, TurnCountAtLeast };
struct NetSpawnSpec {
    IceColor color = IceColor::Gray;
    int hp = 1;
    int count = 1;
    std::vector<std::pair<int,int>> cells;
    // Phase 5 S7c.2: typed daemon kind for the spawned Ice. Default
    // Watchdog preserves pre-S7c.2 trigger semantics (anonymous Gray).
    DaemonKind kind = DaemonKind::Watchdog;
};
struct NetTrigger {
    NetTriggerCond cond = NetTriggerCond::TraceAtLeast;
    int threshold = 0;
    NetSpawnSpec spawn;
    bool fired = false;
};

// The netspace itself. A flat row-major tile grid plus the bookkeeping
// the renderer / input layer reads. ICE, payloads-in-flight, ghost
// nodes, etc. are added per phase.
struct Netspace {
    TargetDescriptor     target;
    int                  w = 0;
    int                  h = 0;
    std::vector<NetTile> tiles;        // size = w * h, row-major
    enum class CombatStatus : uint8_t { Combat, Open };
    std::string          title;        // chrome line (e.g. "MAGLOCK :: DOOR_47B :: TIER 1")
    std::string          title_subtitle;  // optional second line (e.g. vending's quote)
    CombatStatus         combat_status = CombatStatus::Combat;
    int                  time_dilation = 1;  // meatworld ticks per net tick; 1 = no dilation
    NetspaceAmbient      ambient = NetspaceAmbient::None;
    int                  jack_in_x = 0;
    int                  jack_in_y = 0;
    int                  exit_x = 0;
    int                  exit_y = 0;
    WindowState          window_state = WindowState::Stable;

    // Composition primitives. The grammars stamp these into the tile
    // grid at gen time so passable()/Telegraph work without special
    // cases; the renderer overlays the room labels/content and animates
    // the pipes on top of the tile layer.
    std::vector<NetRoom> rooms;
    std::vector<NetPipe> pipes;

    // Phase 2: breakable barrier state. `breakwalls` is the source of truth
    // (persisted); `breakwall_lookup` is an index from tile position to
    // breakwalls[idx], recomputed from `breakwalls` on load (not persisted).
    std::vector<BreakwallGroup>          breakwalls;
    std::map<std::pair<int,int>, size_t> breakwall_lookup;

    // Per-cell passability override. Cells listed here are walkable by
    // the avatar (and pass Telegraph LoS) regardless of the underlying
    // tile. The renderer is unchanged — visually the cell still draws
    // as its tile (typically a BoxThin/Double/Block border), but the
    // avatar can step through it. NetspaceBuilder uses this to open
    // ports where pipes attach to room borders so room interiors stay
    // visually sealed while traversal works.
    std::set<std::pair<int, int>> passable_overrides;

    // Per-cell glyph overrides. Tiles typed NetTile::Glyph render the
    // UTF-8 string stored here keyed by (x, y). Also used as a spawn
    // pool by grammar triggers when NetSpawnSpec::cells is empty.
    std::map<std::pair<int,int>, std::string> glyph_overrides;

    // Optional per-cell color for NetTile::Glyph overrides. If a cell has
    // no entry here, the renderer falls back to a neutral default.
    std::map<std::pair<int,int>, Color> glyph_color_overrides;

    // Phase 4: declarative non-geometry lists. Grammars populate these at
    // gen time; jack_in seeds initial_ice into the session; tick_grid
    // evaluates triggers; net_input resolves action_nodes on avatar step.
    std::vector<Ice>        initial_ice;
    std::vector<NetNode>    action_nodes;
    std::vector<NetTrigger> triggers;

    // Trace-tick rate hint set by the grammar. jack_in reads this after
    // gen_for_target() and assigns it to NetSession::trace_tick_per_turn.
    // Default 1 = unchanged for existing grammars.
    int trace_tick_hint = 1;

    // Elevator grammar: number of floors (0 = not an elevator).
    int floor_count     = 0;
    // Elevator grammar: trace penalty added per floor on voluntary jack-out
    // (0 = none; non-elevator grammars leave this at 0).
    int press_luck_step = 0;

    NetTile at(int x, int y) const {
        if (x < 0 || y < 0 || x >= w || y >= h) return NetTile::Void;
        return tiles[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
    }
    void set(int x, int y, NetTile t) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        tiles[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = t;
    }
    bool in_bounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < w && y < h;
    }
    bool passable(int x, int y) const {
        if (passable_overrides.count({x, y})) return true;
        const NetTile t = at(x, y);
        // Floor + JackIn + Exit walkable. Pipes carry payloads but the
        // avatar can step on them too (telegraph LoS check is separate).
        return t == NetTile::Floor
            || t == NetTile::JackIn
            || t == NetTile::Exit
            || t == NetTile::PipeH
            || t == NetTile::PipeV
            || t == NetTile::PipeJunc
            || t == NetTile::PipePortV
            || t == NetTile::PipePortCornerTR
            || t == NetTile::PipePortDownD;
    }
    bool is_wall(int x, int y) const {
        const NetTile t = at(x, y);
        return t == NetTile::WallDot
            || t == NetTile::WallLight
            || t == NetTile::WallMed
            || t == NetTile::WallHeavy
            || t == NetTile::WallSolid
            || t == NetTile::Breakwall;
    }

    // Returns the index of the first non-consumed action node at (x, y),
    // or -1 if none. Used by net_input to dispatch on avatar step.
    int action_node_at(int x, int y) const {
        for (size_t i = 0; i < action_nodes.size(); ++i) {
            const auto& n = action_nodes[i];
            if (!n.consumed && n.x == x && n.y == y) return static_cast<int>(i);
        }
        return -1;
    }
};

// Map a room border style to the corresponding tile kind.
inline NetTile box_tile_for(NetRoom::Border b) {
    switch (b) {
        case NetRoom::Border::Thin:   return NetTile::BoxThin;
        case NetRoom::Border::Double: return NetTile::BoxDouble;
        case NetRoom::Border::Block:  return NetTile::BoxBlock;
    }
    return NetTile::BoxThin;
}

// Recompute Netspace::breakwall_lookup from Netspace::breakwalls. Call after
// load (where breakwall_lookup is not persisted) and after any operation that
// mutates breakwalls[i].tiles.
void recompute_breakwall_lookup(Netspace& ns);

// Re-stamp every tile in `g` with NetTile::Breakwall (current_density > 0)
// or NetTile::Floor (current_density == 0). Call after gen-time setup and
// after each Breach demote.
void restamp_breakwall_group(Netspace& ns, const BreakwallGroup& g);

}  // namespace astra
