#pragma once

// NetRoom + NetPipe — composition primitives that the per-target netspace
// grammars stamp into a Netspace. The renderer overlays them on top of
// the source-of-truth tile grid, so movement / passable() / Telegraph
// LoS still work against the underlying NetTile values written at
// generation time.
//
// See docs/design/netspace.md § "Visual Language Reference" for the
// styles + glyphs each kind picks from.

#include "astra/renderer.h"
#include "astra/net_theme.h"

#include <cstdint>
#include <string>

namespace astra {

// A boxed labelled node in a netspace. NetRoom carries layout +
// presentation metadata; the actual border tiles are stamped into the
// tile grid by NetspaceBuilder at gen time so collision + Telegraph
// work without special-casing rooms.
struct NetRoom {
    int                 x = 0;
    int                 y = 0;
    int                 w = 0;
    int                 h = 0;

    enum class Border : uint8_t { Thin, Double, Block };
    Border              border = Border::Thin;

    // Top-row label, e.g. "JACK", "LOCK 1", "BOLT", "DISPENSE", "FEED".
    // Rendered centered inside the top border row.
    std::string         label;

    // Optional second row (e.g. vending shelf glyph cluster).
    std::string         subtitle;

    // Inline glyph cluster drawn centered in the room's interior:
    // "◊", "@", "§§§", "$$$$$", "▓▓▓", etc.
    std::string         content;

    Color               label_color   = Color::Cyan;
    Color               subtitle_color = Color::Cyan;
    Color               content_color = Color::Yellow;

    // Generator hints used by builder helpers + dispatch.
    bool                is_jack_in = false;
    bool                is_exit    = false;
};

// An animated data path between two rooms (or between two points).
// The renderer paints pipe tiles each turn with a pulse cycle keyed
// off (world_tick + pulse_offset). Pipes carry payloads (Phase 5);
// for now they're cosmetic + telegraph-passable.
struct NetPipe {
    int                 x0 = 0;
    int                 y0 = 0;
    int                 x1 = 0;
    int                 y1 = 0;

    enum class Style : uint8_t { Thin, Double, Heavy, Dashed };
    Style               style = Style::Double;

    Color               color = Color::Cyan;
    int                 pulse_offset = 0;
};

}  // namespace astra
