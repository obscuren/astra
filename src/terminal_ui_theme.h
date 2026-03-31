// src/terminal_ui_theme.h
#pragma once

#include "astra/renderer.h"
#include "astra/ui_types.h"

namespace astra {

struct UIStyle {
    Color fg = Color::Default;
    Color bg = Color::Default;
};

// Resolve a UITag to terminal-specific colors.
UIStyle resolve_ui_tag(UITag tag);

// Resolve a UITag with value context (e.g. health bar changes color at low %).
UIStyle resolve_ui_tag(UITag tag, int value, int max);

} // namespace astra
