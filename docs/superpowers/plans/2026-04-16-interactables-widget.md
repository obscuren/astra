# Interactables Widget Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a side-panel widget listing NPCs, interactable fixtures, and ground items within the player's FOV, toggleable via F4.

**Architecture:** One new enum value (`Widget::Interactables = 3`), `widget_count` bumped to 4, F4 key added to the renderer key enum + both renderer backends. New `Game::render_interactables_widget` gathers entries in three passes (NPCs / fixtures / ground items), sorts by Chebyshev distance with NPC > Fixture > Item tie-breaker, and renders `<glyph> <name>` rows with an overflow counter. `fixture_type_name` is promoted from file-static to externally visible so the widget can reuse it.

**Tech Stack:** C++20, existing `UIContext` / `Widget` bitfield / `display_name` helpers. No new deps.

**Spec:** `docs/superpowers/specs/2026-04-16-interactables-widget-design.md`

**Worktree:** `.worktrees/interactables-widget`, branch `feat/interactables-widget`.

**No save-format changes.**

Build: `cmake --build build --target astra-dev`. Run: `./build/astra-dev --term`.

---

## File Structure

| File | Kind | Responsibility |
|---|---|---|
| `include/astra/renderer.h` | MODIFY | Add `KEY_F4` to the key enum |
| `src/terminal_renderer.cpp` | MODIFY | Map F4 escape sequences (`ESC[14~`, `ESC O S`) to `KEY_F4` |
| `src/sdl_renderer.cpp` | MODIFY | Map `SDLK_F4` to `KEY_F4` |
| `include/astra/game.h` | MODIFY | `Widget::Interactables`, `widget_count = 4`, F4 in `WidgetKeys`, decl `render_interactables_widget` |
| `include/astra/display_name.h` | MODIFY | Declare `fixture_type_name(FixtureType)` for cross-file reuse |
| `src/game_rendering.cpp` | MODIFY | Remove `static` from `fixture_type_name` definition; add `widget_desired_height` case; add `render_side_panel` dispatch; implement `render_interactables_widget` |

---

### Task 1: Add `KEY_F4` across renderer backends

**Files:**
- Modify: `include/astra/renderer.h:19-22` — key enum
- Modify: `src/terminal_renderer.cpp:474-485` — escape sequence mapping
- Modify: `src/sdl_renderer.cpp:133-135` — SDL mapping

- [ ] **Step 1: Add enum value**

In `include/astra/renderer.h`, find:

```cpp
    KEY_F1,
    KEY_F2,
    KEY_F3,
};
```

Change to:

```cpp
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
};
```

- [ ] **Step 2: Terminal renderer — `ESC[14~` form**

In `src/terminal_renderer.cpp`, find the block (around line 470):

```cpp
                case '1': {
                    char c3, tilde;
                    if (read(STDIN_FILENO, &c3, 1) == 1 &&
                        read(STDIN_FILENO, &tilde, 1) == 1 && tilde == '~') {
                        if (c3 == '1') return KEY_F1;
                        if (c3 == '2') return KEY_F2;
                        if (c3 == '3') return KEY_F3;
                    }
                    break;
                }
```

Insert after the `'3'` line:

```cpp
                        if (c3 == '4') return KEY_F4;
```

Repeat the same change for the identical block that appears later in the file (around line 539 — the blocking read path).

- [ ] **Step 3: Terminal renderer — `ESC O S` form**

In the same file, find:

```cpp
        if (seq[0] == 'O') {
            if (seq[1] == 'P') return KEY_F1;
            if (seq[1] == 'Q') return KEY_F2;
            if (seq[1] == 'R') return KEY_F3;
        }
```

Append inside the block:

```cpp
            if (seq[1] == 'S') return KEY_F4;
```

Do the same for the duplicate block further down if present.

- [ ] **Step 4: SDL renderer**

In `src/sdl_renderer.cpp`, find the case block mapping SDLK to KEY_F* (around line 133-135):

```cpp
                case SDLK_F1:      return KEY_F1;
                case SDLK_F2:      return KEY_F2;
                case SDLK_F3:      return KEY_F3;
```

Append:

```cpp
                case SDLK_F4:      return KEY_F4;
```

- [ ] **Step 5: Build**

Run: `cmake --build build --target astra-dev`
Expected: clean build.

- [ ] **Step 6: Commit**

```bash
git add include/astra/renderer.h src/terminal_renderer.cpp src/sdl_renderer.cpp
git commit -m "$(cat <<'EOF'
feat(renderer): add KEY_F4 to terminal and SDL backends

xterm-style ESC O S and rxvt-style ESC [14~ both resolve to KEY_F4.
Terminal renderer patches all duplicate escape-sequence blocks.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Extend `Widget` enum + `WidgetKeys`

**Files:**
- Modify: `include/astra/game.h:57-76`

- [ ] **Step 1: Add the enum value, bump count, add F4 key**

In `include/astra/game.h`, replace the widget block (around line 56-76) with:

```cpp
// Side-panel widgets — multiple can be active simultaneously (bitfield).
enum class Widget : uint8_t {
    Messages      = 0,
    Wait          = 1,
    Minimap       = 2,
    Interactables = 3,
};
static constexpr int widget_count = 4;
static constexpr uint8_t widget_default = (1 << static_cast<uint8_t>(Widget::Messages));

inline bool widget_active(uint8_t mask, Widget w) {
    return (mask >> static_cast<uint8_t>(w)) & 1;
}
inline void widget_toggle(uint8_t& mask, Widget w) {
    mask ^= (1 << static_cast<uint8_t>(w));
}

// Configurable toggle keys for each widget (indexed by Widget enum value).
struct WidgetKeys {
    int keys[widget_count] = { KEY_F1, KEY_F2, KEY_F3, KEY_F4 };
    const char* labels[widget_count] = { "F1", "F2", "F3", "F4" };
};
```

- [ ] **Step 2: Declare render function**

Still in `include/astra/game.h`, near the existing `render_minimap_widget` declaration (around line 222), add:

```cpp
    void render_interactables_widget(UIContext& ctx);
```

- [ ] **Step 3: Build**

Run: `cmake --build build --target astra-dev`

Expected: likely `-Wswitch` errors on `render_side_panel` and `widget_desired_height` — those switches need the new case. If the compiler flags them, leave them for Task 4 (they'll be handled together with the widget wiring). Build may fail; that's OK — commit the enum change and move on.

If the build does fail, add **temporary** no-op `case Widget::Interactables: return 0;` in `widget_desired_height` and `case Widget::Interactables: break;` in the `render_side_panel` switch so the build stays green. Task 4 replaces both.

- [ ] **Step 4: Commit**

```bash
git add include/astra/game.h src/game_rendering.cpp
git commit -m "$(cat <<'EOF'
feat(ui): Widget::Interactables enum value + F4 binding

Reserves the slot in the Widget bitfield and WidgetKeys table.
render_interactables_widget declaration added; implementation and
wiring land in the next commits.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Promote `fixture_type_name` to a header

**Files:**
- Modify: `include/astra/display_name.h` — declare
- Modify: `src/game_rendering.cpp:76-129` — drop `static`

- [ ] **Step 1: Declare in header**

In `include/astra/display_name.h`, add at the bottom of the file (inside `namespace astra {`):

```cpp
// Fixture — human-readable name for display.
const char* fixture_type_name(FixtureType type);
```

Include `#include "astra/tilemap.h"` if `FixtureType` isn't already in scope (tilemap.h is where the enum lives). If the header becomes circular, use a forward declaration instead:

```cpp
enum class FixtureType : uint8_t;
const char* fixture_type_name(FixtureType type);
```

- [ ] **Step 2: Remove `static` from the definition**

In `src/game_rendering.cpp`, around line 76:

```cpp
static const char* fixture_type_name(FixtureType type) {
```

Change to:

```cpp
const char* fixture_type_name(FixtureType type) {
```

- [ ] **Step 3: Build**

Run: `cmake --build build --target astra-dev`
Expected: clean build. Existing callers still resolve to the same function (they include either `display_name.h` or see the non-static definition in `game_rendering.cpp`).

- [ ] **Step 4: Commit**

```bash
git add include/astra/display_name.h src/game_rendering.cpp
git commit -m "$(cat <<'EOF'
refactor(ui): promote fixture_type_name to external linkage

Needed so the upcoming Interactables widget can reuse it. No
behavioral change; definition stays in game_rendering.cpp, just
drops the 'static' qualifier and gets a matching declaration in
display_name.h next to the other display helpers.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: Implement `render_interactables_widget`

**Files:**
- Modify: `src/game_rendering.cpp` — body of widget, size case, dispatch

- [ ] **Step 1: Add `widget_desired_height` case**

In `src/game_rendering.cpp` around line 1004, the `widget_desired_height` function:

```cpp
static int widget_desired_height(Widget w) {
    switch (w) {
        case Widget::Wait:    return 10;
        case Widget::Minimap: return 10;
        default:              return 0;
    }
}
```

If Task 2's temporary stub is present, replace it; otherwise add:

```cpp
        case Widget::Interactables: return 10;
```

Final function:

```cpp
static int widget_desired_height(Widget w) {
    switch (w) {
        case Widget::Wait:          return 10;
        case Widget::Minimap:       return 10;
        case Widget::Interactables: return 10;
        default:                    return 0;
    }
}
```

- [ ] **Step 2: Dispatch in `render_side_panel`**

Still in `src/game_rendering.cpp`, find the switch inside `render_side_panel` (around line 1050):

```cpp
        switch (active[i]) {
            case Widget::Messages: render_messages_widget(regions[region_idx]); break;
            case Widget::Wait:     render_wait_widget(regions[region_idx]); break;
            case Widget::Minimap:  render_minimap_widget(regions[region_idx]); break;
        }
```

Add a new case:

```cpp
            case Widget::Interactables: render_interactables_widget(regions[region_idx]); break;
```

- [ ] **Step 3: Add includes**

At the top of `src/game_rendering.cpp`, confirm these are present (add if missing):

```cpp
#include "astra/display_name.h"   // fixture_type_name, display_name overloads
```

- [ ] **Step 4: Implement the widget**

Append to `src/game_rendering.cpp` after `render_minimap_widget`:

```cpp
void Game::render_interactables_widget(UIContext& ctx) {
    struct Entry {
        int dist;
        int category;       // 0 NPC, 1 Fixture, 2 Item
        char glyph;
        Color color;
        std::string text;
    };

    const auto& map = world_.map();
    const auto& vis = world_.visibility();
    int px = player_.x;
    int py = player_.y;

    std::vector<Entry> entries;

    // NPC pass — alive + visible
    for (const auto& npc : world_.npcs()) {
        if (!npc.alive()) continue;
        if (vis.get(npc.x, npc.y) != Visibility::Visible) continue;
        int d = std::max(std::abs(npc.x - px), std::abs(npc.y - py));
        entries.push_back({d, 0, npc_glyph(npc.npc_role, npc.race),
                           Color::White, display_name(npc)});
    }

    // Fixture pass — interactable + visible, scanned within a 40x40 box
    int w = map.width();
    int h = map.height();
    int x0 = std::max(0, px - 20);
    int y0 = std::max(0, py - 20);
    int x1 = std::min(w, px + 21);
    int y1 = std::min(h, py + 21);
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            if (vis.get(x, y) != Visibility::Visible) continue;
            int fid = map.fixture_id(x, y);
            if (fid < 0) continue;
            const FixtureData& f = map.fixture(fid);
            if (!f.interactable) continue;
            int d = std::max(std::abs(x - px), std::abs(y - py));
            entries.push_back({d, 1, fixture_glyph(f.type),
                               Color::Cyan, fixture_type_name(f.type)});
        }
    }

    // Ground-item pass — visible
    for (const auto& gi : world_.ground_items()) {
        if (vis.get(gi.x, gi.y) != Visibility::Visible) continue;
        int d = std::max(std::abs(gi.x - px), std::abs(gi.y - py));
        entries.push_back({d, 2, '!', Color::Yellow,
                           display_name(gi.item)});
    }

    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b) {
                  if (a.dist != b.dist) return a.dist < b.dist;
                  return a.category < b.category;
              });

    int panel_h = ctx.height();
    if (panel_h <= 0) return;
    int visible_rows = panel_h;

    if (entries.empty()) {
        ctx.text(1, 0, "(nothing nearby)", Color::DarkGray);
        return;
    }

    int total = static_cast<int>(entries.size());
    int rows_used = std::min(total, visible_rows);
    bool overflow = total > visible_rows;
    if (overflow) --rows_used;   // reserve last row for "N more"

    for (int i = 0; i < rows_used; ++i) {
        const auto& e = entries[i];
        ctx.put(1, i, e.glyph, e.color);
        ctx.text_rich(3, i, e.text);
    }

    if (overflow) {
        std::string more = "... " + std::to_string(total - rows_used) + " more";
        ctx.text(1, visible_rows - 1, more, Color::DarkGray);
    }
}
```

**`npc_glyph` / `fixture_glyph`**: these should already be reachable via `#include "terminal_theme.h"` (see `src/game_rendering.cpp` existing usage). If the linker can't resolve them, either include the header or use inline lookup — the plan prefers the existing helpers.

**`text_rich`** is used because `display_name(npc)` / `display_name(item)` embed color markers. If `text_rich` is not available on `UIContext`, substitute `text(col, row, e.text, e.color)` — fall back to a single color for the name.

- [ ] **Step 5: Build**

Run: `cmake --build build --target astra-dev`
Expected: clean build.

If the widget's compilation chokes on `npc_glyph` / `fixture_glyph` / `text_rich`, adapt to whatever helpers the file's existing code uses (look at the look-popup or inspect-panel rendering paths for patterns).

- [ ] **Step 6: Commit**

```bash
git add src/game_rendering.cpp
git commit -m "$(cat <<'EOF'
feat(ui): Interactables widget — render NPCs / fixtures / items

Scans three categories inside the player's FOV and renders one
row per entry sorted by Chebyshev distance (NPC > Fixture > Item
on ties). Fixture pass is box-bounded to a 40x40 window so map
size isn't a factor. Overflow compresses into a "... N more"
footer; empty list shows "(nothing nearby)".

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: Smoke test

**Files:** none modified — manual verification.

- [ ] **Step 1: Launch**

```
./build/astra-dev --term
```

- [ ] **Step 2: Toggle the widget**

Start a new game on The Heavens Above. Press **F4**. The Interactables widget should appear in the side panel alongside Messages (and whatever else is on).

- [ ] **Step 3: Check NPC entries**

You should see the Station Keeper (`K`), any Merchants / Drifters visible, etc. Each one labelled with their race-colored name.

- [ ] **Step 4: Check fixture entries**

Stand near a Heal Pod / Food Terminal / Star Chart — they should appear in the list once within FOV. Walk away until they drop out of sight — watch them disappear from the list.

- [ ] **Step 5: Check ground items**

Drop an item (or spawn one via dev console if available). It should appear at its row.

- [ ] **Step 6: Check overflow**

Spawn enough NPCs (`spawn archon_remnant` a bunch of times if possible) to exceed the panel height. Confirm the last row reads `... N more`.

- [ ] **Step 7: Toggle off**

Press **F4** again. The widget disappears.

- [ ] **Step 8: No commit**

Smoke test only.

---

## Acceptance Criteria

- `cmake --build build --target astra-dev` clean at every commit.
- Pressing F4 toggles the Interactables widget on/off.
- Widget shows NPCs + interactable fixtures + ground items currently in FOV, sorted by Chebyshev distance.
- Empty list shows `(nothing nearby)`.
- Oversize list shows `... N more` on the last row.
- No regression on existing F1/F2/F3 widgets.

---

## Out of Scope (deferred)

- Cursor/selection in the widget.
- Cardinal direction column.
- Distance column.
- Stairs/hatches/doors in the list.
- Quest-marker highlights inside the widget.
- Custom quest-fixture display names (falls back to "Quest Fixture").
