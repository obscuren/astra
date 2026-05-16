#include "astra/net_theme.h"

#include <cstddef>
#include <cstdint>

namespace astra::net_theme {

namespace {

// ~30 weird UTF-8 glyphs. Math/Greek, typographic marks, heavy-block
// fragments, diamonds, crossed box-drawing. NO combining diacritics
// (reserved for Phase 8 Blackwall).
constexpr const char* kGlitchPool[] = {
    "\xce\xa8", "\xce\xa3", "\xce\x9b", "\xce\x9e", "\xce\xa9",     // Ψ Σ Λ Ξ Ω
    "\xe2\x88\x86", "\xe2\x88\x87", "\xe2\x88\x82", "\xe2\x88\xab", // ∆ ∇ ∂ ∫
    "\xe2\x88\x91", "\xe2\x88\x8f", "\xe2\x88\x9e",                 // ∑ ∏ ∞
    "\xc2\xa7", "\xc2\xa4", "\xe2\x80\xa1", "\xe2\x80\xa0",         // § ¤ ‡ †
    "\xc2\xb6", "\xe2\x80\x96", "\xe2\x80\xbb", "\xe2\x88\x8e",     // ¶ ‖ ※ ∎
    "\xe2\x96\x9a", "\xe2\x96\x9e", "\xe2\x96\x9f",                 // ▚ ▞ ▟
    "\xe2\x96\x99", "\xe2\x96\x9b", "\xe2\x96\x9c",                 // ▙ ▛ ▜
    "\xe2\x97\x8a", "\xe2\x97\x87", "\xe2\x97\x88",                 // ◊ ◇ ◈
    "\xe2\x97\x89", "\xe2\x97\x8e",                                 // ◉ ◎
    "\xe2\x95\xb3", "\xe2\x95\x8b", "\xe2\x95\xac",                 // ╳ ╋ ╬
    "\xe2\x95\xaa", "\xe2\x95\xab",                                 // ╪ ╫
};
constexpr size_t kGlitchPoolSize = sizeof(kGlitchPool) / sizeof(kGlitchPool[0]);

// Cheap deterministic 3-int mix (FNV-1a-style).
uint32_t hash3(int x, int y, int frame) {
    uint32_t h = 2166136261u;
    auto mix = [&](uint32_t v) { h ^= v; h *= 16777619u; };
    mix(static_cast<uint32_t>(x));
    mix(static_cast<uint32_t>(y));
    mix(static_cast<uint32_t>(frame));
    return h;
}

}  // namespace

const char* wall_glitch_glyph(int x, int y, int frame) {
    return kGlitchPool[hash3(x, y, frame) % kGlitchPoolSize];
}

Color shade_for_density(uint8_t density, int frame) {
    (void)frame;  // brightness wobble deferred
    switch (density) {
        case 1: return wall_dot;
        case 2: return wall_light;
        case 3: return wall_med;
        case 4: return wall_heavy;
        case 5: return wall_solid;
        default: return Color::White;
    }
}

}  // namespace astra::net_theme
