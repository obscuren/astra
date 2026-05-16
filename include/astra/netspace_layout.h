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

    // Like add_room, but stamps ONLY the border — the interior is left as
    // the canvas background tile (Void by default). Use for "outer frame"
    // containers where the inside is supposed to hold other rooms and the
    // space between them must NOT be walkable. Returns a NetRoom registered
    // with Netspace for ownership tracking, same as add_room.
    NetRoom& add_room_outline(int x, int y, int w, int h,
                              std::string label, NetRoom::Border border);

    // Connect two rooms with a pipe along an L-shaped route (horizontal
    // then vertical or vice versa, picked deterministically). Stamps
    // PipeH / PipeV / PipeJunc tiles where they cross. Returns the
    // stored NetPipe for tweaks (style, color, pulse_offset).
    NetPipe& connect(const NetRoom& a, const NetRoom& b,
                     NetPipe::Style style = NetPipe::Style::Double);

    // Like connect(), but always routes a straight vertical drop from a's
    // facing edge to b's, regardless of the dx/dy ratio. Use when the
    // grammar intends a clean top-to-bottom pipe and must not let the
    // auto-router flip to a horizontal L (e.g. vending shelf → DISPENSE
    // when a shelf sits far from DISPENSE's center).
    NetPipe& connect_vertical(const NetRoom& a, const NetRoom& b,
                              NetPipe::Style style = NetPipe::Style::Double);

    // Mark a room as the jack-in point. Writes JackIn tile at the room
    // center, sets ns.jack_in_x/y, sets room.is_jack_in.
    void set_jack_in(NetRoom& r);

    // Mark a room as the exit point. Writes Exit tile at the room
    // center, sets ns.exit_x/y, sets room.is_exit.
    void set_exit(NetRoom& r);

    // Mark a single cell as passable regardless of its underlying tile.
    // Use for grammar-specific doorways through walls — e.g. an exit
    // corridor through a room border — where the visual wall should
    // stay intact but the avatar needs a walkable port. connect()
    // calls this automatically for pipe-room attach points.
    void make_passable(int x, int y) {
        if (ns.in_bounds(x, y)) ns.passable_overrides.insert({x, y});
    }

    // Add a horizontal contiguous run of breakwall tiles at row y from x0..x1
    // inclusive. Stamps tiles AND registers a single BreakwallGroup.
    BreakwallGroup& add_breakwall_row(int x0, int x1, int y, uint8_t density);

    // Add a single isolated breakwall tile as a one-tile BreakwallGroup.
    BreakwallGroup& add_breakwall_tile(int x, int y, uint8_t density);

    // Add an arbitrary set of tiles as one BreakwallGroup.
    BreakwallGroup& add_breakwall_blob(std::vector<std::pair<int, int>> tiles, uint8_t density);

    // Convenience: fill the top interior row of `room` with a breakwall group
    // at the given density. Used by the door grammar's LOCK / BOLT rooms.
    BreakwallGroup& fill_top_row_with_breakwall(const NetRoom& room, uint8_t density);

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
