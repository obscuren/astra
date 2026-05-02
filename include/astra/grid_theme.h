#pragma once
#include "astra/renderer.h"

namespace astra::grid_theme {

// Color palette — Tron-style Cyan/Magenta family.
// Substitutions where the plan-named Color entry is absent in the enum,
// chosen so no two palette slots collide:
//   DarkBlue   -> Blue
//   Gray       -> DarkGray
//   BrightCyan -> Cyan           (Plan 5 Cut 2: DeepGridGateway; avoid collision with exit_node's BrightWhite)
//   BrightBlue -> Green          (avoid collision with floor's Blue)
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
constexpr Color connector         = Color::DarkGray;
constexpr Color deep_grid_gateway = Color::Cyan;
constexpr Color warp_anchor       = Color::BrightWhite;

// Glyphs — UTF-8 strings per spec; rendered via Renderer::draw_glyph.
constexpr const char* floor_glyph     = "░";
constexpr const char* firewall_glyph  = "▓";
constexpr const char* avatar_glyph    = "@";
constexpr const char* white_ice_glyph = "▼";
constexpr const char* gray_ice_glyph  = "◇";
constexpr const char* black_ice_glyph = "▲";
constexpr const char* data_node_glyph = "$";
constexpr const char* gateway_glyph   = "⌬";
constexpr const char* exit_glyph      = "⊙";
constexpr const char* encrypted_glyph = "⊘";
constexpr const char* connector_glyph         = "\xe2\x95\x90";   // ═ (default; renderer picks neighbour-resolved variant in Task 19)
constexpr const char* deep_grid_gateway_glyph = "\xe2\x8a\x95";   // ⊕
constexpr const char* warp_anchor_glyph       = "\xe2\x97\x89";   // ◉

} // namespace astra::grid_theme
