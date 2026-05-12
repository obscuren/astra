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

#include <cstdint>
#include <string>

namespace astra {

// A boxed labelled node in a netspace.
//
// Interior layout (per design-doc samples — door, camera, vending):
//
//   ┌─────┐   ← top border (y+0)
//   │ ◄── │   ← top_content     (y+1)
//   │JACK │   ← label           (y+2)
//   │ @   │   ← bottom_content  (y+3)
//   └─────┘   ← bottom border   (y+4)
//
// For shorter rooms the slots collapse:
// - h=4: label at y+1, bottom_content at y+2.
// - h=3: a single content row at y+1 (label preferred over content).
struct NetRoom {
    int                 x = 0;
    int                 y = 0;
    int                 w = 0;
    int                 h = 0;

    enum class Border : uint8_t { Thin, Double, Block };
    Border              border = Border::Thin;

    // Interior text rows. Each is rendered centered within the
    // interior width (w-2). Empty strings are skipped.
    std::string         top_content;    // e.g. "◄──", "░░░", "(o)", "▓▓▓"
    std::string         label;          // e.g. "JACK", "LOCK", "BOLT", "FEED"
    std::string         bottom_content; // e.g. "1", "◊", "►──"

    Color               top_color    = Color::Cyan;
    Color               label_color  = Color::Cyan;
    Color               bottom_color = Color::Yellow;

    // Generator hints used by the builder DSL + dispatch.
    bool                is_jack_in = false;
    bool                is_exit    = false;
};

// An animated data path between two rooms (or between two points).
// The renderer paints pipe tiles each turn with a pulse cycle keyed
// off (world_tick + pulse_offset). Pipes carry payloads in Phase 5;
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
