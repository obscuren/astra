#pragma once

#include "astra/renderer.h"  // Color

namespace astra {

struct RenderDescriptor;
enum class AnimationType : uint8_t;

struct ResolvedVisual {
    char glyph         = '?';
    const char* utf8   = nullptr;  // nullptr = use glyph
    Color fg           = Color::Magenta;
    Color bg           = Color::Default;
};

// Resolve a game-world descriptor to terminal visuals.
// Returns fallback '?' / Magenta for unhandled categories (stub).
ResolvedVisual resolve(const RenderDescriptor& desc);

// Resolve an animation frame to terminal visuals.
// Returns fallback '*' / Magenta for unhandled types (stub).
ResolvedVisual resolve_animation(AnimationType type, int frame_index);

} // namespace astra
