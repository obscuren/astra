# Semantic UI Foundation — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the foundational types, layout system, renderer virtual methods, and terminal UI theme for semantic UI rendering. Everything compiles, nothing uses the new system yet.

**Architecture:** Game code will call immediate-mode UI methods on `UIContext` (panel, progress_bar, text, list, tab_bar, etc.) passing descriptor structs with `UITag` for semantic intent and optional `EntityRef` for entity-aware styling. `UIContext` delegates to new `Renderer` virtual methods. The terminal renderer implements them via a UI theme that maps UITag → colors/glyphs. Layout uses `rows()`/`columns()` with `fixed`/`fill`/`fraction` sizing in logical units.

**Tech Stack:** C++20, CMake

**Spec:** `docs/superpowers/specs/2026-03-31-semantic-ui-design.md`

---

## File Structure

| Action | Path | Responsibility |
|--------|------|----------------|
| Create | `include/astra/ui_types.h` | UITag enum, Size struct, EntityRef struct, all *Desc structs (PanelDesc, ProgressBarDesc, TextDesc, StyledTextDesc, ListDesc, TabBarDesc, SeparatorDesc, LabelValueDesc), TextSegment, ListItem, fixed/fill/fraction helpers |
| Create | `src/ui_layout.cpp` | rows() and columns() size resolution logic |
| Create | `src/terminal_renderer_ui.cpp` | TerminalRenderer UI method implementations (draw_panel, draw_progress_bar, etc.) |
| Create | `src/terminal_ui_theme.h` | UIStyle struct, resolve_ui_tag() declaration |
| Create | `src/terminal_ui_theme.cpp` | resolve_ui_tag() — maps UITag → Color for terminal, reproducing current visual style |
| Modify | `include/astra/ui.h` | Add layout methods (rows, columns) and semantic component methods (panel, progress_bar, text, styled_text, list, tab_bar, separator, label_value) to UIContext |
| Modify | `include/astra/renderer.h` | Add virtual methods for UI primitives (draw_panel, draw_progress_bar, draw_ui_text, etc.) |
| Modify | `include/astra/terminal_renderer.h` | Add override declarations for new UI virtual methods |
| Modify | `include/astra/sdl_renderer.h` | Add stub override declarations |
| Modify | `src/sdl_renderer.cpp` | Add stub implementations |
| Modify | `src/terminal_renderer_win.cpp` | Add UI method implementations (same as terminal_renderer_ui.cpp logic) |
| Modify | `CMakeLists.txt` | Add new source files |

---

## Task 1: Create ui_types.h — All Type Definitions

**Files:**
- Create: `include/astra/ui_types.h`

- [ ] **Step 1: Create the header with all types**

```cpp
// include/astra/ui_types.h
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace astra {

// ---------------------------------------------------------------------------
// UITag — semantic intent for UI elements. Renderer resolves to visual style.
// ---------------------------------------------------------------------------

enum class UITag : uint16_t {
    // Layout chrome
    Border, Separator, Title, Footer,

    // Data display
    HealthBar, XpBar, DurabilityBar, ChargeBar,
    HullBar, ShieldBar,

    // Text roles
    TextDefault, TextDim, TextBright,
    TextDanger, TextSuccess, TextWarning, TextAccent,

    // Interactive
    TabActive, TabInactive,
    OptionSelected, OptionNormal,

    // Game stats
    StatAttack, StatDefense, StatHealth, StatVision, StatSpeed,

    // Rarity
    RarityCommon, RarityUncommon, RarityRare, RarityEpic, RarityLegendary,
};

// ---------------------------------------------------------------------------
// EntityRef — lightweight reference to a game entity for renderer-resolved styling
// ---------------------------------------------------------------------------

struct EntityRef {
    enum class Kind : uint8_t { None, Npc, Item, Quest, Fixture } kind = Kind::None;
    uint16_t id = 0;       // NpcRole, item_def_id, quest_id, FixtureType
    uint8_t  seed = 0;     // race for NPCs, etc.

    bool has_value() const { return kind != Kind::None; }
};

// ---------------------------------------------------------------------------
// Layout sizing — logical units (terminal: 1 unit = 1 cell, SDL: renderer-defined)
// ---------------------------------------------------------------------------

struct Size {
    enum Kind { Fixed, Fill, Fraction };
    Kind kind;
    float value;
};

inline Size fixed(int units) { return {Size::Fixed, static_cast<float>(units)}; }
inline Size fill() { return {Size::Fill, 0.f}; }
inline Size fraction(float f) { return {Size::Fraction, f}; }

// ---------------------------------------------------------------------------
// UI Component Descriptors
// ---------------------------------------------------------------------------

struct PanelDesc {
    std::string title;
    std::string footer;
    UITag tag = UITag::Border;
};

struct ProgressBarDesc {
    int x = 0;
    int y = 0;
    int width = 10;
    int value = 0;
    int max = 100;
    UITag tag = UITag::HealthBar;
    std::string label;
    EntityRef entity;
};

struct TextDesc {
    int x = 0;
    int y = 0;
    std::string content;
    UITag tag = UITag::TextDefault;
};

struct TextSegment {
    std::string text;
    UITag tag = UITag::TextDefault;
    EntityRef entity;
};

struct StyledTextDesc {
    int x = 0;
    int y = 0;
    std::vector<TextSegment> segments;
};

struct ListItem {
    std::string label;
    UITag tag = UITag::OptionNormal;
    bool selected = false;
    EntityRef entity;
};

struct ListDesc {
    std::vector<ListItem> items;
    int scroll_offset = 0;
    UITag tag = UITag::OptionNormal;
    UITag selected_tag = UITag::OptionSelected;
};

struct TabBarDesc {
    std::vector<std::string> tabs;
    int active = 0;
    UITag active_tag = UITag::TabActive;
    UITag inactive_tag = UITag::TabInactive;
};

struct SeparatorDesc {
    UITag tag = UITag::Separator;
};

struct LabelValueDesc {
    int x = 0;
    int y = 0;
    std::string label;
    UITag label_tag = UITag::TextDim;
    std::string value;
    UITag value_tag = UITag::TextBright;
    EntityRef entity;
};

} // namespace astra
```

- [ ] **Step 2: Verify it compiles**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds (header not included anywhere yet).

- [ ] **Step 3: Commit**

```bash
git add include/astra/ui_types.h
git commit -m "Add ui_types.h — UITag, EntityRef, Size, and UI descriptor structs"
```

---

## Task 2: Add Renderer Virtual Methods for UI Primitives

**Files:**
- Modify: `include/astra/renderer.h`
- Modify: `include/astra/terminal_renderer.h`
- Modify: `include/astra/sdl_renderer.h`
- Modify: `src/sdl_renderer.cpp`

- [ ] **Step 1: Read renderer.h, terminal_renderer.h, sdl_renderer.h**

Understand current virtual method list and class structure.

- [ ] **Step 2: Forward-declare UI types in renderer.h**

Add before the Renderer class:
```cpp
// Forward declarations for semantic UI
struct Rect;
struct PanelDesc;
struct ProgressBarDesc;
struct TextDesc;
struct StyledTextDesc;
struct ListDesc;
struct TabBarDesc;
struct SeparatorDesc;
struct LabelValueDesc;
```

- [ ] **Step 3: Add virtual methods to Renderer class**

Add inside the Renderer class, after the existing `draw_animation` method:

```cpp
    // --- Semantic UI rendering ---
    // Each renderer implementation resolves descriptors to backend-specific visuals.

    // Draw a panel frame (borders, title, footer). Returns content rect (interior).
    virtual Rect draw_panel(const Rect& bounds, const PanelDesc& desc) = 0;

    // Draw a progress bar at position.
    virtual void draw_progress_bar(int x, int y, const ProgressBarDesc& desc) = 0;

    // Draw semantic text at position.
    virtual void draw_ui_text(int x, int y, const TextDesc& desc) = 0;

    // Draw styled text with mixed tags/entity refs.
    virtual void draw_styled_text(int x, int y, const StyledTextDesc& desc) = 0;

    // Draw a scrollable list within bounds.
    virtual void draw_list(const Rect& bounds, const ListDesc& desc) = 0;

    // Draw a tab bar within bounds.
    virtual void draw_tab_bar(const Rect& bounds, const TabBarDesc& desc) = 0;

    // Draw a horizontal separator within bounds.
    virtual void draw_separator(const Rect& bounds, const SeparatorDesc& desc) = 0;

    // Draw a label: value pair at position.
    virtual void draw_label_value(int x, int y, const LabelValueDesc& desc) = 0;
```

- [ ] **Step 4: Add override declarations to TerminalRenderer**

In `include/astra/terminal_renderer.h`, add to the public section:

```cpp
    // Semantic UI
    Rect draw_panel(const Rect& bounds, const PanelDesc& desc) override;
    void draw_progress_bar(int x, int y, const ProgressBarDesc& desc) override;
    void draw_ui_text(int x, int y, const TextDesc& desc) override;
    void draw_styled_text(int x, int y, const StyledTextDesc& desc) override;
    void draw_list(const Rect& bounds, const ListDesc& desc) override;
    void draw_tab_bar(const Rect& bounds, const TabBarDesc& desc) override;
    void draw_separator(const Rect& bounds, const SeparatorDesc& desc) override;
    void draw_label_value(int x, int y, const LabelValueDesc& desc) override;
```

- [ ] **Step 5: Add stub overrides to SdlRenderer**

Same declarations in `include/astra/sdl_renderer.h`. Stub implementations in `src/sdl_renderer.cpp` — no-op for now.

- [ ] **Step 6: Check terminal_renderer_win.cpp**

Determine if it needs the same implementations. If it's the same class compiled on Windows, the declarations in the shared header cover it — but the implementations need to be available. They'll come from `terminal_renderer_ui.cpp` which we compile on all platforms.

- [ ] **Step 7: Build — expect failure (implementations missing)**

This won't build yet because TerminalRenderer has no implementations. That's Task 3.

- [ ] **Step 8: Commit (interface only)**

```bash
git add include/astra/renderer.h include/astra/terminal_renderer.h include/astra/sdl_renderer.h src/sdl_renderer.cpp
git commit -m "Add renderer virtual methods for semantic UI primitives"
```

---

## Task 3: Create Terminal UI Theme

**Files:**
- Create: `src/terminal_ui_theme.h`
- Create: `src/terminal_ui_theme.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create terminal_ui_theme.h**

```cpp
// src/terminal_ui_theme.h
#pragma once

#include "astra/renderer.h"  // Color
#include "astra/ui_types.h"  // UITag, EntityRef

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
```

- [ ] **Step 2: Create terminal_ui_theme.cpp**

Map every UITag to the current hardcoded colors. Read the existing code to get exact values:

```cpp
// src/terminal_ui_theme.cpp
#include "terminal_ui_theme.h"

namespace astra {

UIStyle resolve_ui_tag(UITag tag) {
    switch (tag) {
        // Layout chrome
        case UITag::Border:     return {Color::White, Color::Default};
        case UITag::Separator:  return {Color::DarkGray, Color::Default};
        case UITag::Title:      return {Color::White, Color::Default};
        case UITag::Footer:     return {Color::DarkGray, Color::Default};

        // Data display
        case UITag::HealthBar:  return {Color::Green, Color::Default};
        case UITag::XpBar:      return {Color::Cyan, Color::Default};
        case UITag::DurabilityBar: return {Color::Green, Color::Default};
        case UITag::ChargeBar:  return {Color::Cyan, Color::Default};
        case UITag::HullBar:    return {Color::White, Color::Default};
        case UITag::ShieldBar:  return {Color::Cyan, Color::Default};

        // Text roles
        case UITag::TextDefault: return {Color::Default, Color::Default};
        case UITag::TextDim:     return {Color::DarkGray, Color::Default};
        case UITag::TextBright:  return {Color::White, Color::Default};
        case UITag::TextDanger:  return {Color::Red, Color::Default};
        case UITag::TextSuccess: return {Color::Green, Color::Default};
        case UITag::TextWarning: return {Color::Yellow, Color::Default};
        case UITag::TextAccent:  return {Color::Cyan, Color::Default};

        // Interactive
        case UITag::TabActive:      return {Color::Yellow, Color::Default};
        case UITag::TabInactive:    return {Color::DarkGray, Color::Default};
        case UITag::OptionSelected: return {Color::Yellow, Color::Default};
        case UITag::OptionNormal:   return {Color::Cyan, Color::Default};

        // Stats
        case UITag::StatAttack:  return {Color::Red, Color::Default};
        case UITag::StatDefense: return {Color::Blue, Color::Default};
        case UITag::StatHealth:  return {Color::Green, Color::Default};
        case UITag::StatVision:  return {Color::Cyan, Color::Default};
        case UITag::StatSpeed:   return {Color::Yellow, Color::Default};

        // Rarity
        case UITag::RarityCommon:    return {Color::White, Color::Default};
        case UITag::RarityUncommon:  return {Color::Green, Color::Default};
        case UITag::RarityRare:      return {Color::Blue, Color::Default};
        case UITag::RarityEpic:      return {Color::Magenta, Color::Default};
        case UITag::RarityLegendary: return {static_cast<Color>(208), Color::Default};
    }
    return {Color::Default, Color::Default};
}

UIStyle resolve_ui_tag(UITag tag, int value, int max) {
    // Value-aware variants
    if (tag == UITag::HealthBar && max > 0) {
        int pct = (value * 100) / max;
        if (pct <= 25) return {Color::Red, Color::Default};
        if (pct <= 50) return {Color::Yellow, Color::Default};
        return {Color::Green, Color::Default};
    }
    if (tag == UITag::DurabilityBar && max > 0) {
        int pct = (value * 100) / max;
        if (pct <= 25) return {Color::Red, Color::Default};
        if (pct <= 50) return {Color::Yellow, Color::Default};
        return {Color::Green, Color::Default};
    }
    return resolve_ui_tag(tag);
}

} // namespace astra
```

- [ ] **Step 3: Add to CMakeLists.txt**

Add `src/terminal_ui_theme.cpp` to `ASTRA_SOURCES`.

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`

- [ ] **Step 5: Commit**

```bash
git add src/terminal_ui_theme.h src/terminal_ui_theme.cpp CMakeLists.txt
git commit -m "Add terminal UI theme — resolve_ui_tag maps UITag to terminal colors"
```

---

## Task 4: Implement Terminal Renderer UI Methods

**Files:**
- Create: `src/terminal_renderer_ui.cpp`
- Modify: `CMakeLists.txt`

This is the largest task — implementing all 8 UI virtual methods for the terminal renderer using the Panel style (half-block borders ▐▀▌▄).

- [ ] **Step 1: Read the current Panel::draw() implementation in src/ui.cpp**

Understand the exact current half-block border drawing so we reproduce it identically.

- [ ] **Step 2: Read the current bar(), text(), text_rich(), label_value() in src/ui.cpp**

Get exact glyph choices and rendering patterns.

- [ ] **Step 3: Create terminal_renderer_ui.cpp**

This file implements all TerminalRenderer UI methods. It includes `terminal_ui_theme.h` for style resolution.

```cpp
// src/terminal_renderer_ui.cpp
#include "astra/terminal_renderer.h"
#include "astra/ui_types.h"
#include "terminal_ui_theme.h"

namespace astra {

Rect TerminalRenderer::draw_panel(const Rect& bounds, const PanelDesc& desc) {
    auto style = resolve_ui_tag(desc.tag);
    Color border_fg = style.fg;

    // Half-block borders: ▐▀▌ top, ▐ ▌ sides, ▐▄▌ bottom
    // Top border
    draw_glyph(bounds.x, bounds.y, "\xe2\x96\x90", border_fg);  // ▐
    for (int x = bounds.x + 1; x < bounds.x + bounds.w - 1; ++x)
        draw_glyph(x, bounds.y, "\xe2\x96\x80", border_fg);     // ▀
    draw_glyph(bounds.x + bounds.w - 1, bounds.y, "\xe2\x96\x8c", border_fg); // ▌

    // Bottom border
    draw_glyph(bounds.x, bounds.y + bounds.h - 1, "\xe2\x96\x90", border_fg);
    for (int x = bounds.x + 1; x < bounds.x + bounds.w - 1; ++x)
        draw_glyph(x, bounds.y + bounds.h - 1, "\xe2\x96\x84", border_fg); // ▄
    draw_glyph(bounds.x + bounds.w - 1, bounds.y + bounds.h - 1, "\xe2\x96\x8c", border_fg);

    // Side borders
    for (int y = bounds.y + 1; y < bounds.y + bounds.h - 1; ++y) {
        draw_glyph(bounds.x, y, "\xe2\x96\x90", border_fg);
        draw_glyph(bounds.x + bounds.w - 1, y, "\xe2\x96\x8c", border_fg);
    }

    // Title (centered, row 1)
    int content_top = bounds.y + 1;
    if (!desc.title.empty()) {
        auto title_style = resolve_ui_tag(UITag::Title);
        int tx = bounds.x + 1 + (bounds.w - 2 - static_cast<int>(desc.title.size())) / 2;
        for (int i = 0; i < static_cast<int>(desc.title.size()); ++i)
            draw_char(tx + i, content_top, desc.title[i], title_style.fg);
        content_top += 1;

        // Separator below title
        auto sep_style = resolve_ui_tag(UITag::Separator);
        for (int x = bounds.x + 1; x < bounds.x + bounds.w - 1; ++x)
            draw_glyph(x, content_top, "\xe2\x94\x80", sep_style.fg); // ─
        content_top += 1;
    }

    // Footer
    int content_bottom = bounds.y + bounds.h - 2;
    if (!desc.footer.empty()) {
        auto footer_style = resolve_ui_tag(UITag::Footer);
        // Separator above footer
        auto sep_style = resolve_ui_tag(UITag::Separator);
        for (int x = bounds.x + 1; x < bounds.x + bounds.w - 1; ++x)
            draw_glyph(x, content_bottom, "\xe2\x94\x80", sep_style.fg);
        content_bottom -= 1;

        // Footer text centered
        int fx = bounds.x + 1 + (bounds.w - 2 - static_cast<int>(desc.footer.size())) / 2;
        for (int i = 0; i < static_cast<int>(desc.footer.size()); ++i)
            draw_char(fx + i, bounds.y + bounds.h - 2, desc.footer[i], footer_style.fg);
    }

    // Content rect
    return {bounds.x + 1, content_top, bounds.w - 2, content_bottom - content_top + 1};
}

void TerminalRenderer::draw_progress_bar(int x, int y, const ProgressBarDesc& desc) {
    auto style = resolve_ui_tag(desc.tag, desc.value, desc.max);
    auto empty_style = resolve_ui_tag(UITag::TextDim);

    int filled = (desc.max > 0) ? (desc.value * desc.width) / desc.max : 0;
    if (filled > desc.width) filled = desc.width;

    draw_char(x, y, '[', empty_style.fg);
    for (int i = 0; i < desc.width; ++i) {
        if (i < filled)
            draw_glyph(x + 1 + i, y, "\xe2\x96\xb0", style.fg);   // ▰
        else
            draw_glyph(x + 1 + i, y, "\xe2\x96\xb1", empty_style.fg); // ▱
    }
    draw_char(x + desc.width + 1, y, ']', empty_style.fg);
}

void TerminalRenderer::draw_ui_text(int x, int y, const TextDesc& desc) {
    auto style = resolve_ui_tag(desc.tag);
    for (int i = 0; i < static_cast<int>(desc.content.size()); ++i)
        draw_char(x + i, y, desc.content[i], style.fg);
}

void TerminalRenderer::draw_styled_text(int x, int y, const StyledTextDesc& desc) {
    int cx = x;
    for (const auto& seg : desc.segments) {
        auto style = resolve_ui_tag(seg.tag);
        // TODO: entity-aware styling will be added when EntityRef resolution is built
        for (int i = 0; i < static_cast<int>(seg.text.size()); ++i)
            draw_char(cx + i, y, seg.text[i], style.fg);
        cx += static_cast<int>(seg.text.size());
    }
}

void TerminalRenderer::draw_list(const Rect& bounds, const ListDesc& desc) {
    int y = bounds.y;
    int visible = bounds.h;
    int start = desc.scroll_offset;
    int end = std::min(start + visible, static_cast<int>(desc.items.size()));

    for (int i = start; i < end && (y - bounds.y) < visible; ++i) {
        const auto& item = desc.items[i];
        UITag tag = item.selected ? desc.selected_tag : desc.tag;
        auto style = resolve_ui_tag(tag);

        // Cursor prefix
        if (item.selected) {
            draw_char(bounds.x, y, '>', style.fg);
            draw_char(bounds.x + 1, y, ' ', style.fg);
        } else {
            draw_char(bounds.x, y, ' ', style.fg);
            draw_char(bounds.x + 1, y, ' ', style.fg);
        }

        // Label
        int lx = bounds.x + 2;
        for (int c = 0; c < static_cast<int>(item.label.size()) && lx < bounds.x + bounds.w; ++c)
            draw_char(lx++, y, item.label[c], style.fg);

        ++y;
    }
}

void TerminalRenderer::draw_tab_bar(const Rect& bounds, const TabBarDesc& desc) {
    int x = bounds.x;
    for (int i = 0; i < static_cast<int>(desc.tabs.size()); ++i) {
        bool active = (i == desc.active);
        auto style = resolve_ui_tag(active ? desc.active_tag : desc.inactive_tag);

        if (active) draw_char(x++, bounds.y, '[', style.fg);
        else { draw_char(x++, bounds.y, ' ', style.fg); }

        for (char c : desc.tabs[i])
            draw_char(x++, bounds.y, c, style.fg);

        if (active) draw_char(x++, bounds.y, ']', style.fg);
        else { draw_char(x++, bounds.y, ' ', style.fg); }

        draw_char(x++, bounds.y, ' ', Color::Default); // spacing
    }
}

void TerminalRenderer::draw_separator(const Rect& bounds, const SeparatorDesc& desc) {
    auto style = resolve_ui_tag(desc.tag);
    for (int x = bounds.x; x < bounds.x + bounds.w; ++x)
        draw_glyph(x, bounds.y, "\xe2\x94\x80", style.fg);  // ─
}

void TerminalRenderer::draw_label_value(int x, int y, const LabelValueDesc& desc) {
    auto label_style = resolve_ui_tag(desc.label_tag);
    auto value_style = resolve_ui_tag(desc.value_tag);

    int cx = x;
    for (char c : desc.label)
        draw_char(cx++, y, c, label_style.fg);
    for (char c : desc.value)
        draw_char(cx++, y, c, value_style.fg);
}

} // namespace astra
```

- [ ] **Step 4: Add to CMakeLists.txt**

Add `src/terminal_renderer_ui.cpp` to `ASTRA_SOURCES`.

- [ ] **Step 5: Handle Windows renderer**

Check `src/terminal_renderer_win.cpp` — since it's the same `TerminalRenderer` class, it gets the implementations from `terminal_renderer_ui.cpp` which is compiled on all platforms. No additional changes needed.

- [ ] **Step 6: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -10`
Expected: Build succeeds. Nothing calls these methods yet.

- [ ] **Step 7: Commit**

```bash
git add src/terminal_renderer_ui.cpp CMakeLists.txt
git commit -m "Implement terminal renderer UI methods — panel, progress bar, text, list, tabs"
```

---

## Task 5: Add Layout System to UIContext

**Files:**
- Modify: `include/astra/ui.h`
- Create: `src/ui_layout.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Read include/astra/ui.h**

Understand the current UIContext class.

- [ ] **Step 2: Add rows() and columns() declarations to UIContext**

In `include/astra/ui.h`, add include at top:
```cpp
#include "astra/ui_types.h"  // Size
```

Add to UIContext public section:
```cpp
    // Layout — split this context into sub-regions
    std::vector<UIContext> rows(const std::vector<Size>& sizes) const;
    std::vector<UIContext> columns(const std::vector<Size>& sizes) const;
```

- [ ] **Step 3: Create src/ui_layout.cpp**

```cpp
// src/ui_layout.cpp
#include "astra/ui.h"
#include <algorithm>

namespace astra {

static std::vector<int> resolve_sizes(const std::vector<Size>& sizes, int total) {
    std::vector<int> result(sizes.size(), 0);
    int remaining = total;
    int fill_count = 0;

    // Pass 1: allocate fixed and fraction
    for (size_t i = 0; i < sizes.size(); ++i) {
        switch (sizes[i].kind) {
            case Size::Fixed:
                result[i] = static_cast<int>(sizes[i].value);
                remaining -= result[i];
                break;
            case Size::Fraction:
                result[i] = static_cast<int>(sizes[i].value * total);
                remaining -= result[i];
                break;
            case Size::Fill:
                fill_count++;
                break;
        }
    }

    // Pass 2: distribute remaining to fills
    if (fill_count > 0 && remaining > 0) {
        int per_fill = remaining / fill_count;
        int extra = remaining % fill_count;
        for (size_t i = 0; i < sizes.size(); ++i) {
            if (sizes[i].kind == Size::Fill) {
                result[i] = per_fill + (extra > 0 ? 1 : 0);
                if (extra > 0) extra--;
            }
        }
    }

    // Clamp negatives
    for (auto& s : result) s = std::max(s, 0);

    return result;
}

std::vector<UIContext> UIContext::rows(const std::vector<Size>& sizes) const {
    auto resolved = resolve_sizes(sizes, bounds_.h);
    std::vector<UIContext> result;
    int y = bounds_.y;
    for (size_t i = 0; i < sizes.size(); ++i) {
        result.emplace_back(renderer_, Rect{bounds_.x, y, bounds_.w, resolved[i]});
        y += resolved[i];
    }
    return result;
}

std::vector<UIContext> UIContext::columns(const std::vector<Size>& sizes) const {
    auto resolved = resolve_sizes(sizes, bounds_.w);
    std::vector<UIContext> result;
    int x = bounds_.x;
    for (size_t i = 0; i < sizes.size(); ++i) {
        result.emplace_back(renderer_, Rect{x, bounds_.y, resolved[i], bounds_.h});
        x += resolved[i];
    }
    return result;
}

} // namespace astra
```

- [ ] **Step 4: Add to CMakeLists.txt**

Add `src/ui_layout.cpp` to `ASTRA_SOURCES`.

- [ ] **Step 5: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`

- [ ] **Step 6: Commit**

```bash
git add include/astra/ui.h src/ui_layout.cpp CMakeLists.txt
git commit -m "Add layout system — rows() and columns() with fixed/fill/fraction sizing"
```

---

## Task 6: Add Semantic Component Methods to UIContext

**Files:**
- Modify: `include/astra/ui.h`
- Create: `src/ui_components.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add method declarations to UIContext in ui.h**

Add to UIContext public section:
```cpp
    // Semantic UI components — delegates to renderer
    UIContext panel(const PanelDesc& desc);
    void progress_bar(const ProgressBarDesc& desc);
    void text(const TextDesc& desc);
    void styled_text(const StyledTextDesc& desc);
    void list(const ListDesc& desc);
    void tab_bar(const TabBarDesc& desc);
    void separator(const SeparatorDesc& desc);
    void label_value(const LabelValueDesc& desc);
```

Note: `panel()` returns a new UIContext scoped to the content area. All others are void.

Note: The existing `text()` methods that take `(x, y, string_view, Color)` stay for backward compat during migration. The new `text(TextDesc)` overload is a different signature so they can coexist.

- [ ] **Step 2: Create src/ui_components.cpp**

Each method translates local coordinates to screen coordinates and delegates to the renderer:

```cpp
// src/ui_components.cpp
#include "astra/ui.h"

namespace astra {

UIContext UIContext::panel(const PanelDesc& desc) {
    Rect content_rect = renderer_->draw_panel(bounds_, desc);
    return UIContext(renderer_, content_rect);
}

void UIContext::progress_bar(const ProgressBarDesc& desc) {
    ProgressBarDesc abs = desc;
    abs.x += bounds_.x;
    abs.y += bounds_.y;
    renderer_->draw_progress_bar(abs.x, abs.y, abs);
}

void UIContext::text(const TextDesc& desc) {
    renderer_->draw_ui_text(bounds_.x + desc.x, bounds_.y + desc.y, desc);
}

void UIContext::styled_text(const StyledTextDesc& desc) {
    renderer_->draw_styled_text(bounds_.x + desc.x, bounds_.y + desc.y, desc);
}

void UIContext::list(const ListDesc& desc) {
    renderer_->draw_list(bounds_, desc);
}

void UIContext::tab_bar(const TabBarDesc& desc) {
    renderer_->draw_tab_bar(bounds_, desc);
}

void UIContext::separator(const SeparatorDesc& desc) {
    renderer_->draw_separator(bounds_, desc);
}

void UIContext::label_value(const LabelValueDesc& desc) {
    renderer_->draw_label_value(bounds_.x + desc.x, bounds_.y + desc.y, desc);
}

} // namespace astra
```

- [ ] **Step 3: Add to CMakeLists.txt**

Add `src/ui_components.cpp` to `ASTRA_SOURCES`.

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds. Nothing calls these yet.

- [ ] **Step 5: Commit**

```bash
git add include/astra/ui.h src/ui_components.cpp CMakeLists.txt
git commit -m "Add semantic UI component methods to UIContext — panel, text, list, tabs, etc."
```

---

## Task 7: Verify Full Build and Test

**Files:** None (verification only)

- [ ] **Step 1: Clean build**

Run: `rm -rf build && cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -10`
Expected: Build succeeds with zero warnings related to our changes.

- [ ] **Step 2: Run the game**

Run: `./build/astra-dev`
Expected: Game launches and plays identically. No visual changes — the new system exists but nothing uses it yet.

- [ ] **Step 3: Commit if any fixups needed**

---

## Summary

After this plan:
- `ui_types.h` defines UITag, EntityRef, Size, and all descriptor structs
- `Renderer` has 8 new virtual methods for UI primitives
- `TerminalRenderer` implements all 8 via `terminal_renderer_ui.cpp`
- Terminal UI theme maps UITag → colors (matching current visual style exactly)
- Layout system: `rows()` and `columns()` with `fixed`/`fill`/`fraction` sizing
- `UIContext` has semantic component methods that delegate to the renderer
- Everything compiles and runs — zero visual changes
- Old `UIContext` methods (put, text, box, etc.) still work for backward compat

**Next plan:** Migrate `render_play()` — the main game screen — to use the new semantic UI system.
