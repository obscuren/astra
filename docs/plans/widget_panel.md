# Side Panel Widget System Redesign

## Context

The current side panel uses a tab bar where only one tab is visible at a time (Messages, Equipment, Ship, Wait). The user wants to replace this with a **widget-based system** where multiple widgets can be toggled on/off simultaneously and stack vertically. Equipment is removed from the side panel (accessible via character screen `c`). Ship tab is also removed (placeholder). A Minimap widget is added as a stub for future implementation.

## Widget System

### Widgets
1. **Messages** — message log with scrollback (existing code, extracted)
2. **Wait** — wait/rest menu (existing code, extracted)
3. **Minimap** — stub for now, renders placeholder text

### Data Model

Replace `PanelTab` enum + `active_tab_` with:

```cpp
enum class Widget : uint8_t {
    Messages = 0,
    Wait     = 1,
    Minimap  = 2,
};
static constexpr int widget_count = 3;

// In Game:
uint8_t active_widgets_ = 1;      // bitfield, Messages on by default
int focused_widget_ = 0;           // index of widget receiving +/- input
bool panel_visible_ = true;        // Ctrl+H still toggles entire panel
```

### Input (configurable keys)

- **F1** toggle Messages, **F2** toggle Wait, **F3** toggle Minimap
- Keys stored in a struct/array so they can be changed later
- **Tab** / **Shift+Tab** cycles focus among active widgets
- **+/-** routes to focused widget (message scroll or wait cursor)
- **Enter/1-6** on Wait widget only when Wait is focused
- **Ctrl+H** toggles entire panel visibility (unchanged)
- When a focused widget is toggled off, focus moves to next active widget

### Top Bar Rendering

```
[F1]Messages  F2 Wait  [F3]Minimap
```
- Bracketed `[Fn]Name` = active, plain `Fn Name` = inactive
- Focused widget gets highlight/underline to distinguish from merely-active

### Vertical Stacking

Active widgets stack in `side_panel_rect_`. Messages always at bottom. Space division:
- 1 widget: gets all space
- 2 widgets: top gets ~40%, Messages gets ~60%
- 3 widgets: top two get ~25% each, Messages gets ~50%
- Horizontal separator between stacked widgets

## Files to Modify

### Step 1: Add KEY_F3 to renderer
- `include/astra/renderer.h` — add `KEY_F3` to virtual keycodes
- `src/terminal_renderer.cpp` — parse F3 escape sequence
- `src/sdl_renderer.cpp` — map SDL F3 keycode

### Step 2: Widget enum + Game members
- `include/astra/game.h`
  - Remove `PanelTab` enum (line 51-58), `panel_tab_count` (line 58)
  - Add `Widget` enum, helpers (`widget_active`, `widget_toggle`)
  - Replace `active_tab_` → `active_widgets_`, add `focused_widget_`
  - Add configurable key array for widget toggles
  - Update accessors: remove `active_tab()`/`set_active_tab()`, add widget equivalents
  - Add declarations: `render_messages_widget`, `render_wait_widget`, `render_minimap_widget`

### Step 3: Widget bar in semantic UI
- `include/astra/ui_types.h` — add `WidgetBarDesc` struct
- `include/astra/renderer.h` — add `virtual void draw_widget_bar(const Rect&, const WidgetBarDesc&) = 0`
- `include/astra/terminal_renderer.h` — declare override
- `src/terminal_renderer_ui.cpp` — implement (adapted from `draw_tab_bar`)
- `include/astra/sdl_renderer.h` — declare override
- `src/sdl_renderer.cpp` — implement
- `include/astra/ui.h` — add `void widget_bar(const WidgetBarDesc&)`
- `src/ui_components.cpp` — delegate to renderer

### Step 4: Refactor rendering
- `src/game_rendering.cpp`
  - Remove `tab_names[]` array
  - Rewrite `render_tabs()` → `render_widget_bar()` using new `WidgetBarDesc`
  - Extract message rendering (lines 916-1013) → `render_messages_widget(UIContext&)`
  - Extract wait rendering (lines 1079-1122) → `render_wait_widget(UIContext&)`
  - Add `render_minimap_widget(UIContext&)` stub
  - Delete Equipment rendering (lines 1015-1073) and Ship rendering (lines 1075-1078)
  - Rewrite `render_side_panel()` to stack active widgets vertically with separators

### Step 5: Refactor input
- `src/game_input.cpp`
  - Add F1/F2/F3 handlers to toggle widget bits (using configurable key array)
  - Tab/Shift+Tab cycles `focused_widget_` among active widgets
  - +/- routes to focused widget
  - Enter/1-6 checks focused widget == Wait
  - When toggling off focused widget, auto-advance focus

### Step 6: Save/load migration
- `include/astra/save_file.h` — replace `int active_tab` with `uint8_t active_widgets` + `uint8_t focused_widget`
- `src/save_file.cpp`
  - Bump save version (19→20)
  - Write: `write_u8(active_widgets)`, `write_u8(focused_widget)`
  - Read v20+: `read_u8` for both
  - Read v19: migrate `read_i32(old_tab)` → bitfield (Messages always on, Wait on if old tab was 3)
- `src/save_system.cpp` — update `build_save_data` and `load` to use new accessors

### Step 7: Update help screen
- `src/help_screen.cpp` — update Controls section: F1-F3 widget toggles, Tab focus cycling

## Verification

1. `cmake -B build -DDEV=ON && cmake --build build`
2. Run `./build/astra --term`, verify:
   - Widget bar shows `[F1]Messages  F2 Wait  F3 Minimap` on start
   - F1 toggles Messages off/on (brackets appear/disappear)
   - F2 toggles Wait, F3 toggles Minimap stub
   - Multiple widgets stack vertically when active
   - Tab cycles focus highlight among active widgets
   - +/- scrolls messages when Messages focused, moves cursor when Wait focused
   - Ctrl+H hides/shows entire panel
   - Wait actions (Enter/1-6) only fire when Wait is focused
   - Abilities 1-5 still work when Wait is not focused
3. Save game, reload — widget state preserved
4. Load old save (v19) — migrates gracefully (Messages active)
