#pragma once

// Netspace — the transient per-jack-in micro-dungeon a Drifter walks
// inside the cyberdeck overlay. One Netspace is generated each time the
// player jacks into a hackable target. Replaces the legacy single-Site /
// GridSector / GridNetwork geography.
//
// See docs/design/netspace.md for the full design. Phase 0 ships the
// abstraction and a blank-room stub generator; per-target grammars land
// in Phase 1+.

#include <cstddef>
#include <cstdint>
#include <string>
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
};

// Per-jack-in target descriptor. Built by the caller of jack_in(); fed
// to the netspace generator. Per-phase, this struct grows (fixture id,
// npc id, faction tag, etc.).
struct TargetDescriptor {
    NetspaceTargetKind kind = NetspaceTargetKind::Empty;
    int                tier = 1;       // 1..5 difficulty band
    uint32_t           seed = 0;       // deterministic generation
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

    Glyph,      // tile carries an inline glyph; renderer reads it from
                // Netspace::glyph_overrides keyed by (x, y).
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

// The netspace itself. A flat row-major tile grid plus the bookkeeping
// the renderer / input layer reads. ICE, payloads-in-flight, ghost
// nodes, etc. are added per phase.
struct Netspace {
    TargetDescriptor     target;
    int                  w = 0;
    int                  h = 0;
    std::vector<NetTile> tiles;        // size = w * h, row-major
    std::string          title;        // chrome line (e.g. "MAGLOCK :: DOOR_47B :: TIER 1")
    int                  jack_in_x = 0;
    int                  jack_in_y = 0;
    int                  exit_x = 0;
    int                  exit_y = 0;
    WindowState          window_state = WindowState::Stable;

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
        const NetTile t = at(x, y);
        // Floor + JackIn + Exit walkable. Pipes carry payloads but the
        // avatar can step on them too (telegraph LoS check is separate).
        return t == NetTile::Floor
            || t == NetTile::JackIn
            || t == NetTile::Exit
            || t == NetTile::PipeH
            || t == NetTile::PipeV
            || t == NetTile::PipeJunc;
    }
    bool is_wall(int x, int y) const {
        const NetTile t = at(x, y);
        return t == NetTile::WallDot
            || t == NetTile::WallLight
            || t == NetTile::WallMed
            || t == NetTile::WallHeavy
            || t == NetTile::WallSolid;
    }
};

}  // namespace astra
