#pragma once

// NetspaceBuilder — fluent layout helpers for the per-target netspace
// grammars. Each grammar composes its Netspace by sequencing builder
// calls. The builder stamps room borders and pipe segments into the
// underlying NetTile grid so passable()/Telegraph work without
// special-casing the overlay primitives.

#include "astra/netspace.h"

#include <string>

namespace astra {

struct NetspaceBuilder {
    Netspace ns;

    // Construct an empty canvas of the given dimensions. The tile grid
    // is filled with `bg` (default: Void). Grammars typically build
    // their netspace on a Void background and stamp Floor inside rooms.
    explicit NetspaceBuilder(int w, int h, NetTile bg = NetTile::Void);

    void set_title(std::string t)            { ns.title = std::move(t); }
    void set_target(TargetDescriptor d)      { ns.target = d; }

    // Fill the entire tile grid with one tile. Useful for solid-wall
    // grammars that carve rooms out of a filled block.
    void fill(NetTile t);

    // Add a room. Stamps the border tiles (using the room's chosen
    // border style) and fills the interior with Floor in the tile grid.
    // Returns a reference to the stored NetRoom for further tweaks.
    NetRoom& add_room(int x, int y, int w, int h, std::string label,
                      NetRoom::Border border = NetRoom::Border::Thin);

    // Connect two rooms with a pipe along an L-shaped route (horizontal
    // then vertical or vice versa, picked deterministically). Stamps
    // PipeH / PipeV / PipeJunc tiles where they cross. Returns the
    // stored NetPipe for tweaks (style, color, pulse_offset).
    NetPipe& connect(const NetRoom& a, const NetRoom& b,
                     NetPipe::Style style = NetPipe::Style::Double);

    // Mark a room as the jack-in point. Writes JackIn tile at the room
    // center, sets ns.jack_in_x/y, sets room.is_jack_in.
    void set_jack_in(NetRoom& r);

    // Mark a room as the exit point. Writes Exit tile at the room
    // center, sets ns.exit_x/y, sets room.is_exit.
    void set_exit(NetRoom& r);

    // Finalize: return the populated Netspace by move.
    Netspace finalize() { return std::move(ns); }

private:
    // Stamp a horizontal pipe run between two x coordinates at row y.
    // Cells that are currently Void or Floor are overwritten with PipeH.
    void stamp_h(int y, int x0, int x1);
    // Stamp a vertical pipe run between two y coordinates at column x.
    void stamp_v(int x, int y0, int y1);
};

}  // namespace astra
