#pragma once
#include "astra/renderer.h"

#include <cstdint>

namespace astra {
// FixtureType is defined in tilemap.h; forward-declare to avoid the heavy include.
enum class FixtureType : uint8_t;
}

namespace astra::net_theme {

// Plan 5 Cut 2.6: pick the wall-mounted device-avatar glyph (UTF-8) for the
// FixtureType that a subnet sector mirrors. Used by the subnet renderer when
// drawing the GridTile::DeviceAvatar tile. All avatars render in BrightWhite.
const char* device_avatar_glyph(astra::FixtureType type);

// Color palette — Tron-style Cyan/Magenta family.
// Substitutions where the plan-named Color entry is absent in the enum,
// chosen so no two palette slots collide:
//   DarkBlue   -> Blue
//   Gray       -> DarkGray
//   BrightCyan -> Cyan           (Plan 5 Cut 2: DeepGridGateway; avoid collision with exit_node's BrightWhite)
//   BrightBlue -> Green          (avoid collision with floor's Blue)
constexpr Color floor       = Color::Blue;
constexpr Color firewall    = Color::Magenta;
constexpr Color door_open   = Color::Cyan;
constexpr Color door_locked = Color::Yellow;          // orange-ish in palette
constexpr Color avatar      = Color::Cyan;
constexpr Color white_ice   = Color::White;
constexpr Color gray_ice    = Color::DarkGray;
constexpr Color black_ice   = Color::Red;
constexpr Color data_node   = Color::Yellow;
constexpr Color gateway     = Color::BrightMagenta;
constexpr Color exit_node   = Color::BrightWhite;
constexpr Color encrypted   = Color::Green;
constexpr Color connector         = Color::DarkGray;
constexpr Color deep_grid_gateway = Color::Cyan;
constexpr Color warp_anchor       = Color::BrightWhite;

// Glyphs — UTF-8 strings per spec; rendered via Renderer::draw_glyph.
constexpr const char* door_open_glyph   = "+";
constexpr const char* door_locked_glyph = "\xe2\x96\xa3";  // ▣
constexpr const char* floor_glyph     = " ";              // (none) — design-doc empty space
constexpr const char* firewall_glyph  = "▓";
constexpr const char* avatar_glyph    = "@";
constexpr const char* white_ice_glyph = "▼";
constexpr const char* gray_ice_glyph  = "◇";
constexpr const char* black_ice_glyph = "▲";
constexpr const char* data_node_glyph = "$";
constexpr const char* gateway_glyph   = "⌬";
constexpr const char* exit_glyph      = "\xe2\x97\x84";    // ◄ design-doc exit arrow
constexpr const char* encrypted_glyph = "⊘";

// ─── Visual language reference (docs/design/netspace.md) ────────────
//
// Wall density gradient — used by the door grammar's lock progression
// and (Phase 2) by walls degrading under fire.
constexpr const char* wall_dot_glyph   = "\xc2\xb7";       // ·
constexpr const char* wall_light_glyph = "\xe2\x96\x91";   // ░
constexpr const char* wall_med_glyph   = "\xe2\x96\x92";   // ▒
constexpr const char* wall_heavy_glyph = "\xe2\x96\x93";   // ▓
constexpr const char* wall_solid_glyph = "\xe2\x96\x88";   // █

constexpr Color wall_dot   = Color::DarkGray;
constexpr Color wall_light = Color::DarkGray;
constexpr Color wall_med   = Color::Cyan;
constexpr Color wall_heavy = Color::BrightMagenta;
constexpr Color wall_solid = Color::White;

// Box-drawing — borders for NetRoom. The renderer resolves which
// specific glyph (corner, edge, junction) by 4-neighbour mask. These
// are the building blocks each style uses.
struct BoxGlyphs {
    const char* h;     // horizontal edge       ─ ═ ▀
    const char* v;     // vertical edge         │ ║ ▌
    const char* tl;    // top-left corner       ┌ ╔ ▛
    const char* tr;    // top-right corner      ┐ ╗ ▜
    const char* bl;    // bottom-left corner    └ ╚ ▙
    const char* br;    // bottom-right corner   ┘ ╝ ▟
};

inline constexpr BoxGlyphs box_thin   = {
    "\xe2\x94\x80", "\xe2\x94\x82",
    "\xe2\x94\x8c", "\xe2\x94\x90",
    "\xe2\x94\x94", "\xe2\x94\x98",
};
inline constexpr BoxGlyphs box_double = {
    "\xe2\x95\x90", "\xe2\x95\x91",
    "\xe2\x95\x94", "\xe2\x95\x97",
    "\xe2\x95\x9a", "\xe2\x95\x9d",
};
inline constexpr BoxGlyphs box_block  = {
    "\xe2\x96\x80", "\xe2\x96\x8c",
    "\xe2\x96\x9b", "\xe2\x96\x9c",
    "\xe2\x96\x99", "\xe2\x96\x9f",
};

// Border colors per threat tier.
constexpr Color box_thin_color   = Color::DarkGray;
constexpr Color box_double_color = Color::Cyan;
constexpr Color box_block_color  = Color::BrightMagenta;

// Animated data pipes — phase the renderer cycles each turn.
constexpr const char* pipe_h_frames[4] = {
    "\xe2\x95\x90",  // ═
    "\xe2\x94\x80",  // ─
    "\xe2\x95\x90",  // ═
    "\xe2\x94\x80",  // ─
};
constexpr const char* pipe_v_frames[4] = {
    "\xe2\x95\x91",  // ║
    "\xe2\x94\x82",  // │
    "\xe2\x95\x91",  // ║
    "\xe2\x94\x82",  // │
};
constexpr const char* pipe_junc_glyph  = "\xe2\x95\xac";  // ╬
constexpr const char* pipe_port_v_glyph = "\xe2\x95\xa8";  // ╨ — pipe enters box from top
constexpr const char* pipe_port_corner_tr_glyph = "\xe2\x95\x9c";  // ╜ — pipe enters box top-right corner
constexpr const char* pipe_port_down_d_glyph = "\xe2\x95\xa6";  // ╦ — pipe exits double-box bottom
constexpr Color       pipe_color = Color::Cyan;

// Fork-pipe T-junctions for box_thin — used by the S6.4 ICE telegraph
// tether to splice into a room's wall and the wrapping label box.
constexpr const char* tee_down  = "\xe2\x94\xac";  // ┬ U+252C — splits down from a horizontal edge
constexpr const char* tee_up    = "\xe2\x94\xb4";  // ┴ U+2534 — splits up   from a horizontal edge
constexpr const char* tee_right = "\xe2\x94\x9c";  // ├ U+251C — splits right from a vertical   edge
constexpr const char* tee_left  = "\xe2\x94\xa4";  // ┤ U+2524 — splits left  from a vertical   edge

// Mixed-weight tees for double<->single junctions. Useful when a double-
// bordered overlay (e.g. the S6 telegraph box) tethers into a single-
// bordered surface (a thin-bordered room). The "D" side is the double
// edge, the branch is single.
inline constexpr const char* tee_dh_down_s  = "\xe2\x95\xa5";  // ╥ — double horizontal, single down branch
inline constexpr const char* tee_dh_up_s    = "\xe2\x95\xa8";  // ╨ — double horizontal, single up branch
inline constexpr const char* tee_dv_left_s  = "\xe2\x95\xa2";  // ╢ — double vertical, single left branch
inline constexpr const char* tee_dv_right_s = "\xe2\x95\x9f";  // ╟ — double vertical, single right branch

// Canonical glyph vocabulary from the design doc.
constexpr const char* glyph_jack_in_arrow = "\xe2\x97\x84";  // ◄
constexpr const char* glyph_exit_arrow    = "\xe2\x96\xba";  // ►
constexpr const char* glyph_payload       = "\xc2\xa7";       // §
constexpr const char* glyph_enemy_payload = "\xc2\xa4";       // ¤
constexpr const char* glyph_loop          = "\xe2\x80\xa1";   // ‡
constexpr const char* glyph_break         = "\xce\xa9";        // Ω
constexpr const char* glyph_black_ice     = "\xce\x9b";        // Λ
constexpr const char* glyph_loot          = "\xe2\x97\x8a";    // ◊
constexpr const char* glyph_workbench     = "\xe2\x96\xa3";    // ▣
constexpr const char* glyph_dead_node     = "X";
constexpr const char* glyph_unknown       = "?";
constexpr const char* glyph_credit        = "$";
constexpr const char* glyph_eye           = "\xe2\x97\x8f";    // ●
constexpr const char* glyph_camera_lens   = "(o)";              // composed; rendered as a 3-cell content

// Pick a "glitch" UTF-8 glyph deterministically from a fixed pool using
// a (x, y, frame) hash so each (tile, frame) combination shows a
// different chaotic glyph. Pool excludes zalgo combining diacritics
// (those are reserved for Phase 8 Blackwall corruption).
const char* wall_glitch_glyph(int x, int y, int frame);

// Brightness-modulated shade of the density's normal wall color.
// Phase 2: returns the density's base color; brightness wobble is a
// deferred polish pass (the glyph chaos alone reads as a strong hit).
Color shade_for_density(uint8_t density, int frame);

} // namespace astra::net_theme
