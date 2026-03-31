# Semantic UI Architecture

**Date:** 2026-03-31
**Status:** Approved
**Branch:** TBD (feature branch off main)

## Problem

UI code currently specifies *how* things look — colors, box-drawing characters, and layout math are hardcoded across 11+ files (~787 draw calls, ~1,175 color references). The renderer receives terminal-specific instructions (`draw_char('┌', Color::DarkGray)`) and can't interpret them differently for SDL or other backends.

This is the same problem we solved for game-world entities with `RenderDescriptor` → `draw_entity()`. The UI needs the same treatment: game code describes *what* UI elements exist, and each renderer decides *how* to present them.

## Architecture

### Immediate Mode Command Model

Game code emits semantic UI commands each frame. No retained state — the UI description is rebuilt from game state every frame, matching the existing `clear → render → present` loop.

Each command is a struct carrying:
- **What** — the component type (panel, progress bar, text, list, tab bar, etc.)
- **Data** — the values to display (HP 45/100, item names, tab labels)
- **Intent** — a `UITag` that tells the renderer the semantic purpose

```cpp
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
```

`UITag` is the bridge between game intent and renderer style. The game says "this is a health bar", the terminal renderer picks green fill with red-when-critical, SDL picks a gradient bar with a heart icon.

### Layout System

Game code describes structure with sizing hints. The layout system resolves to concrete rects.

```cpp
struct Size {
    enum Kind { Fixed, Fill, Fraction };
    Kind kind;
    float value;
};

inline Size fixed(int cells) { return {Size::Fixed, float(cells)}; }
inline Size fill() { return {Size::Fill, 0}; }
inline Size fraction(float f) { return {Size::Fraction, f}; }
```

**Splitting:**

```cpp
// Vertical split
auto [stats, hp, xp, main, sep, effects, abilities] = ctx.rows({
    fixed(1), fixed(1), fixed(1), fill(), fixed(1), fixed(1), fixed(1),
});

// Horizontal split within main
auto [map, vsep, panel] = main.columns({
    fill(), fixed(1), fraction(0.35),
});
```

**Logical units:** `fixed(n)` means n *logical units*, not cells or pixels. Each renderer defines what a unit means — terminal: 1 unit = 1 cell. SDL: 1 unit = font-height pixels (or a configurable grid scale). This keeps layout code renderer-agnostic while both backends use a grid-based model.

**Resolution rules:**
1. `fixed(n)` — exactly n logical units, honored first
2. `fraction(f)` — f * total available, rounded to nearest unit
3. `fill()` — gets remaining space. Multiple fills split evenly.
4. If total exceeds available — fixed honored, fractions shrink, fills get 0.

`rows()` and `columns()` return an array of `UIContext` objects, each scoped to its resolved rect. Nesting works naturally — a column can be split into rows, which can contain panels.

The world renderer receives its context from the layout and uses `width()`/`height()` as iteration bounds — clipping is built in. No wasted draw calls for off-screen tiles.

### UI Component Primitives

Each component is an immediate-mode method on `UIContext` taking a descriptor struct.

**Panel** — framed container with optional title/footer:
```cpp
struct PanelDesc {
    std::string title;
    std::string footer;
    UITag tag = UITag::Border;
};

auto content = ctx.panel({.title = "Inventory", .footer = "[Esc] Close"});
```
Returns a `UIContext` scoped to the content interior. The renderer decides border style (half-blocks on terminal, rounded corners on SDL).

**Progress Bar:**
```cpp
struct ProgressBarDesc {
    int x, y, width;
    int value, max;
    UITag tag;
    std::string label;  // optional
    EntityRef entity;   // optional — renderer may use for context-aware styling
};

ctx.progress_bar({.x=0, .y=0, .width=20, .value=hp, .max=max_hp, .tag=UITag::HealthBar, .label="HP"});
```

**Text:**
```cpp
struct TextDesc {
    int x, y;
    std::string content;
    UITag tag = UITag::TextDefault;
};

ctx.text({.x=0, .y=0, .content="System online", .tag=UITag::TextSuccess});
```

### Entity References

UI components often reference game entities (NPCs, items, quests). Instead of hardcoding visual data, they carry a lightweight entity reference. The renderer resolves styling from its own theme.

```cpp
struct EntityRef {
    enum class Kind : uint8_t { None, Npc, Item, Quest, Fixture } kind = Kind::None;
    uint16_t id = 0;       // NpcRole, item_def_id, quest_id, FixtureType
    uint8_t  seed = 0;     // race for NPCs, rarity for items, etc.
};
```

This is separate from `RenderDescriptor` (which carries map-specific fields like Biome, visibility flags). `EntityRef` is the UI's way of saying "this references a game entity" — the renderer looks it up in its own theme to determine styling.

Example: a text segment with `EntityRef{Npc, Nova, Stellari}` — the terminal renderer styles it in Nova's purple, SDL might add a portrait icon. An item ref with `EntityRef{Item, ITEM_PILE_OF_LEAVES}` — the terminal renderer alternates brown colors, SDL might use a leaf texture.

### Display Name Helpers

A family of `display_name()` overloads that return `std::vector<TextSegment>` for any named game entity. Each knows the naming convention for its entity type. Game code calls them instead of manually composing styled text.

```cpp
std::vector<TextSegment> display_name(const Npc& npc);
// → [{"Nova", UITag::TextBright}, {" (", UITag::TextDim}, {"N", entity=EntityRef{Npc, Nova, Stellari}}, {")", UITag::TextDim}]

std::vector<TextSegment> display_name(const Item& item);
// → [{"Plasma Pistol", entity=EntityRef{Item, ITEM_PLASMA_PISTOL}}]

std::vector<TextSegment> display_name(const Quest& quest);
// → [{"!", UITag::QuestMarker}, {" Missing Hauler", UITag::TextBright}]
```

The renderer uses the `EntityRef` to resolve per-character or per-segment styling from its own definitions. The game code never specifies colors.

### UI Component Primitives (continued)

**Styled Text** — mixed tags in one line, with optional entity references:
```cpp
struct TextSegment {
    std::string text;
    UITag tag = UITag::TextDefault;
    EntityRef entity;  // renderer resolves styling from entity identity when kind != None
};

struct StyledTextDesc {
    int x, y;
    std::vector<TextSegment> segments;
};

ctx.styled_text({.x=0, .y=0, .segments={
    {"HP:", UITag::TextDim},
    {"45/100", UITag::StatHealth},
}});

// Or with entity reference:
ctx.styled_text({.x=0, .y=0, .segments=display_name(npc)});
```

**List:**
```cpp
struct ListItem {
    std::string label;
    UITag tag;
    bool selected;
    EntityRef entity;  // renderer uses for glyph/color styling
};

struct ListDesc {
    std::vector<ListItem> items;
    int scroll_offset;
    UITag tag = UITag::OptionNormal;
    UITag selected_tag = UITag::OptionSelected;
};

ctx.list({.items=entries, .scroll_offset=scroll, .tag=UITag::OptionNormal});
```

**Tab Bar:**
```cpp
struct TabBarDesc {
    std::vector<std::string> tabs;
    int active;
    UITag active_tag = UITag::TabActive;
    UITag inactive_tag = UITag::TabInactive;
};

ctx.tab_bar({.tabs={"Messages", "Equipment", "Ship"}, .active=current_tab});
```

**Separator:**
```cpp
struct SeparatorDesc {
    UITag tag = UITag::Separator;
};

ctx.separator({.tag=UITag::Separator});
```

**Label-Value:**
```cpp
struct LabelValueDesc {
    int x, y;
    std::string label;
    UITag label_tag;
    std::string value;
    UITag value_tag;
    EntityRef entity;  // optional — renderer may use for value styling
};

ctx.label_value({.x=0, .y=0, .label="LVL:", .label_tag=UITag::TextDim, .value="5", .value_tag=UITag::TextBright});
```

### Renderer Integration

`UIContext` translates coordinates and delegates to the renderer. The `Renderer` interface gains virtual methods for each UI primitive:

```cpp
// New virtual methods on Renderer
virtual void draw_panel(const Rect& bounds, const PanelDesc& desc) = 0;
virtual Rect panel_content_rect(const Rect& bounds, const PanelDesc& desc) = 0;
virtual void draw_progress_bar(int x, int y, const ProgressBarDesc& desc) = 0;
virtual void draw_ui_text(int x, int y, const TextDesc& desc) = 0;
virtual void draw_styled_text(int x, int y, const StyledTextDesc& desc) = 0;
virtual void draw_list(const Rect& bounds, const ListDesc& desc) = 0;
virtual void draw_tab_bar(const Rect& bounds, const TabBarDesc& desc) = 0;
virtual void draw_separator(const Rect& bounds, const SeparatorDesc& desc) = 0;
virtual void draw_label_value(int x, int y, const LabelValueDesc& desc) = 0;
```

`panel_content_rect()` returns the interior rect after the renderer draws borders — the renderer knows how thick its borders are (1 cell for terminal half-blocks, maybe more for SDL).

Each renderer implements all methods. The terminal renderer resolves `UITag` → colors/glyphs via its UI theme. SDL resolves to sprites/rectangles/fonts.

### Terminal UI Theme

Internal to the terminal renderer. Maps `UITag` → terminal-specific visual properties:

```cpp
// src/terminal_ui_theme.h (terminal-internal)
struct UIStyle {
    Color fg = Color::Default;
    Color bg = Color::Default;
};

UIStyle resolve_ui_tag(UITag tag);
```

All current hardcoded colors move here. The existing visual look is preserved exactly:

- `UITag::HealthBar` → green fill (`Color::Green`), red when value < 25%
- `UITag::TextDanger` → `Color::Red`
- `UITag::TabActive` → `Color::Yellow`
- `UITag::Border` → `Color::White` with half-block chars (▐▀▌▄)
- `UITag::StatAttack` → `Color::Red`
- `UITag::RarityEpic` → `Color::Magenta`
- `UITag::Separator` → `Color::DarkGray` with `─`

### File Structure

Split by responsibility to keep files focused:

**Shared (renderer-agnostic):**
```
include/astra/ui_types.h         — UITag, Size, *Desc structs, fixed/fill/fraction helpers
include/astra/ui_context.h       — UIContext class declaration
src/ui_context.cpp               — UIContext core methods (put, text, sub, bounds)
src/ui_layout.cpp                — rows(), columns(), size resolution
src/ui_components.cpp            — panel(), list(), tab_bar() — delegates to renderer
```

**Terminal renderer:**
```
src/terminal_renderer.cpp        — core: init, shutdown, clear, present, input, cell buffer
src/terminal_renderer_world.cpp  — draw_entity(), draw_animation()
src/terminal_theme.cpp           — resolve() for world entities (exists)
src/terminal_renderer_ui.cpp     — draw_panel(), draw_progress_bar(), draw_ui_text(), etc.
src/terminal_ui_theme.cpp        — resolve_ui_tag(), UI style lookups
```

**SDL renderer (future):**
```
src/sdl_renderer_ui.cpp          — SDL implementations of same virtual methods
src/sdl_ui_theme.cpp             — SDL-specific styling
```

## Migration Strategy

Incremental — each game screen migrates independently. Old `UIContext` methods (`put()`, `text()`, `box()`, etc.) stay available during transition via the backward-compat `DrawContext` alias. Each screen converts from raw draw calls to semantic commands.

**Order:**
1. Add `ui_types.h` with `UITag`, `Size`, descriptor structs
2. Add layout system (`rows()`, `columns()`)
3. Add renderer virtual methods + terminal implementations
4. Migrate `render_play()` — the main game screen (biggest win: layout + stats + bars)
5. Migrate panel-based screens (trade, repair, help, character) one at a time
6. Migrate character creation, main menu
7. Remove old `put()`/`text()`/`box()` methods when no longer called

Each step is a working commit. The game looks identical throughout.

## Out of Scope

- SDL renderer UI implementation — created when SDL work begins
- Custom widget types beyond the primitives listed
- Animation/transition system for UI elements
- Accessibility features (screen reader, high contrast)
- Input handling redesign — current key-based input stays as-is
