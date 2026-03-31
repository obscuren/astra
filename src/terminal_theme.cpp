// src/terminal_theme.cpp
#include "terminal_theme.h"
#include "astra/render_descriptor.h"

namespace astra {

ResolvedVisual resolve(const RenderDescriptor& desc) {
    (void)desc;
    // Stub — returns bright magenta '?' so unresolved entities are obvious.
    // Each migration step will add real resolution logic here.
    return {'?', nullptr, Color::Magenta, Color::Default};
}

ResolvedVisual resolve_animation(AnimationType type, int frame_index) {
    (void)type;
    (void)frame_index;
    // Stub — returns bright magenta '*' so unresolved animations are obvious.
    return {'*', nullptr, Color::Magenta, Color::Default};
}

} // namespace astra
