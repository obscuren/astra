#pragma once
#include "astra/renderer.h"

namespace astra::grid_theme {

// Color palette — Tron-style Cyan/Magenta family.
// Substitutions where the plan-named Color entry is absent in the enum,
// chosen so no two palette slots collide:
//   DarkBlue   -> Blue
//   Gray       -> DarkGray
//   BrightCyan -> BrightWhite  (avoid collision with avatar's Cyan)
//   BrightBlue -> Green        (avoid collision with floor's Blue)
constexpr Color floor       = Color::Blue;
constexpr Color firewall    = Color::Magenta;
constexpr Color avatar      = Color::Cyan;
constexpr Color white_ice   = Color::White;
constexpr Color gray_ice    = Color::DarkGray;
constexpr Color black_ice   = Color::Red;
constexpr Color data_node   = Color::Yellow;
constexpr Color gateway     = Color::BrightMagenta;
constexpr Color exit_node   = Color::BrightWhite;
constexpr Color encrypted   = Color::Green;

// Glyphs — ASCII only (Cell is char).
constexpr char floor_glyph     = '.';
constexpr char firewall_glyph  = '#';
constexpr char avatar_glyph    = '@';
constexpr char white_ice_glyph = 'w';
constexpr char gray_ice_glyph  = 'G';
constexpr char black_ice_glyph = 'B';
constexpr char data_node_glyph = '$';
constexpr char gateway_glyph   = '*';
constexpr char exit_glyph      = 'O';
constexpr char encrypted_glyph = '?';

} // namespace astra::grid_theme
