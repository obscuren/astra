#pragma once
#include "astra/renderer.h"   // Color
#include <cstdint>
namespace astra {
enum class NetCoreAction : uint8_t { None, Sniff, Channel, Brace, Run };
inline const char* core_action_label(NetCoreAction a) {
    switch (a) {
        case NetCoreAction::Sniff:   return "SNIFF";
        case NetCoreAction::Channel: return "CHANNL";
        case NetCoreAction::Brace:   return "BRACE";
        case NetCoreAction::Run:     return "RUN";
        default:                     return "";
    }
}
inline const char* core_action_glyph(NetCoreAction a) {
    switch (a) {
        case NetCoreAction::Sniff:   return "\xe2\x89\x88";  // ≈ placeholder (single codepoint; was 2-cp ◜◞ — unrenderable in one cell). Final glyph TBD by owner.
        case NetCoreAction::Channel: return "\xe2\x86\xbb";
        case NetCoreAction::Brace:   return "\xe2\x96\xa3";
        case NetCoreAction::Run:     return "\xc2\xbb";
        default:                     return "\xe2\x96\xad";
    }
}
inline Color core_action_color(NetCoreAction a) {
    switch (a) {
        case NetCoreAction::Sniff:   return Color::Cyan;
        case NetCoreAction::Channel: return Color::Green;
        case NetCoreAction::Brace:   return Color::Yellow;
        case NetCoreAction::Run:     return Color::Magenta;
        default:                     return Color::DarkGray;
    }
}
}  // namespace astra
