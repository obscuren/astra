# Semantic Rendering Foundation — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Lay the foundation for semantic rendering — add `RenderDescriptor`, split `DrawContext` into `WorldContext` and `UIContext`, add `draw_entity()` to `Renderer`, create stub `terminal_theme`. Everything compiles and behaves identically.

**Architecture:** `RenderDescriptor` is a flat struct carrying semantic game data (category, type_id, seed, flags, biome, rarity). `WorldContext` passes descriptors to `Renderer::draw_entity()`. `UIContext` is the existing `DrawContext` renamed. `terminal_theme.cpp` owns visual resolution for the terminal backend. This step introduces the plumbing — no visual logic moves yet.

**Tech Stack:** C++20, CMake

**Spec:** `docs/superpowers/specs/2026-03-31-semantic-rendering-design.md`

---

## File Structure

| Action | Path | Responsibility |
|--------|------|----------------|
| Create | `include/astra/render_descriptor.h` | `RenderDescriptor` struct, `RenderCategory` enum, `RenderFlag` enum, `position_seed()` |
| Create | `src/terminal_theme.h` | `ResolvedVisual` struct, `resolve()` declaration, `resolve_animation()` declaration (terminal-internal) |
| Create | `src/terminal_theme.cpp` | Stub `resolve()` and `resolve_animation()` — returns fallback `'?' / Magenta` for now |
| Create | `include/astra/world_context.h` | `WorldContext` class — takes `Renderer*` + `Rect`, delegates to `draw_entity()` |
| Modify | `include/astra/ui.h` | Rename `DrawContext` → `UIContext`. Add `using DrawContext = UIContext;` alias for backward compat |
| Modify | `include/astra/renderer.h` | Add `virtual void draw_entity(int x, int y, const RenderDescriptor& desc) = 0;` and `virtual void draw_animation(int x, int y, AnimationType type, int frame_index) = 0;` |
| Modify | `include/astra/terminal_renderer.h` | Add `draw_entity()` and `draw_animation()` override declarations |
| Modify | `src/terminal_renderer.cpp` | Implement `draw_entity()` — calls `resolve()` from terminal_theme, writes to cell buffer |
| Modify | `include/astra/sdl_renderer.h` | Add `draw_entity()` and `draw_animation()` stub override declarations |
| Modify | `src/sdl_renderer.cpp` | Stub `draw_entity()` and `draw_animation()` — no-op for now |
| Modify | `src/terminal_renderer_win.cpp` | Add `draw_entity()` and `draw_animation()` override if Windows renderer is a separate class (check first) |
| Modify | `CMakeLists.txt` | Add `src/terminal_theme.cpp` to ASTRA_SOURCES |
| Modify | `include/astra/map_renderer.h` | Change `#include "ui.h"` to `#include "world_context.h"` |
| Modify | `src/map_renderer.cpp` | Change `DrawContext` → `WorldContext` for the map rendering context only |
| Modify | `src/map_editor.cpp` | Change viewport `DrawContext` → `WorldContext` for the tile-drawing viewport only; UI parts stay `UIContext`/`DrawContext` |

---

## Task 1: Create RenderDescriptor

**Files:**
- Create: `include/astra/render_descriptor.h`

- [ ] **Step 1: Create the header file**

```cpp
// include/astra/render_descriptor.h
#pragma once

#include <cstdint>
#include "astra/tilemap.h"  // Biome
#include "astra/item.h"     // Rarity

namespace astra {

enum class RenderCategory : uint8_t {
    Tile,
    Fixture,
    Npc,
    Item,
    Effect,
    Player,
};

enum RenderFlag : uint8_t {
    RF_None         = 0,
    RF_Remembered   = 1 << 0,
    RF_Hostile      = 1 << 1,
    RF_Damaged      = 1 << 2,
    RF_Lit          = 1 << 3,
    RF_Interactable = 1 << 4,
    RF_Equipped     = 1 << 5,
};

struct RenderDescriptor {
    RenderCategory category = RenderCategory::Tile;
    uint16_t type_id  = 0;
    uint8_t  seed     = 0;
    uint8_t  flags    = RF_None;
    Biome    biome    = Biome::Station;
    Rarity   rarity   = Rarity::Common;
};

enum class AnimationType : uint8_t {
    ConsoleBlink,
    WaterShimmer,
    TorchFlicker,
    DamageFlash,
    HealPulse,
    Projectile,
    LevelUp,
};

// Deterministic position hash for visual variation.
// Same (x,y) always produces the same seed.
inline uint8_t position_seed(int x, int y) {
    uint32_t h = static_cast<uint32_t>(x * 374761393 + y * 668265263);
    h = (h ^ (h >> 13)) * 1274126177;
    return static_cast<uint8_t>(h >> 24);
}

} // namespace astra
```

- [ ] **Step 2: Verify it compiles**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds (header not included anywhere yet, but syntax should be valid)

- [ ] **Step 3: Commit**

```bash
git add include/astra/render_descriptor.h
git commit -m "Add RenderDescriptor — semantic rendering data structure"
```

---

## Task 2: Add draw_entity() and draw_animation() to Renderer Interface

**Files:**
- Modify: `include/astra/renderer.h`
- Modify: `include/astra/terminal_renderer.h`
- Modify: `src/terminal_renderer.cpp`
- Modify: `include/astra/sdl_renderer.h`
- Modify: `src/sdl_renderer.cpp`

- [ ] **Step 1: Forward-declare RenderDescriptor and AnimationType in renderer.h**

Add before the `Renderer` class definition in `include/astra/renderer.h` (after the `Color` enum and key codes, before `class Renderer`):

```cpp
// Forward declarations for semantic rendering
struct RenderDescriptor;
enum class AnimationType : uint8_t;
```

We forward-declare rather than `#include "render_descriptor.h"` to keep `renderer.h` lightweight — it's included by nearly every file. The full include goes in the `.cpp` files that need the definition.

- [ ] **Step 2: Add virtual methods to Renderer class**

Add inside the `Renderer` class in `include/astra/renderer.h`, after the `read_cell` method:

```cpp
    // Semantic rendering — render a game-world entity from its descriptor.
    // Each renderer implementation resolves the descriptor to backend-specific visuals.
    virtual void draw_entity(int x, int y, const RenderDescriptor& desc) = 0;

    // Semantic animation — render an animation frame at a position.
    virtual void draw_animation(int x, int y, AnimationType type, int frame_index) = 0;
```

- [ ] **Step 3: Add overrides to TerminalRenderer**

In `include/astra/terminal_renderer.h`, add to the public section:

```cpp
    void draw_entity(int x, int y, const RenderDescriptor& desc) override;
    void draw_animation(int x, int y, AnimationType type, int frame_index) override;
```

- [ ] **Step 4: Implement stubs in terminal_renderer.cpp**

Add at the end of `src/terminal_renderer.cpp`:

```cpp
#include "astra/render_descriptor.h"
#include "terminal_theme.h"

void TerminalRenderer::draw_entity(int x, int y, const RenderDescriptor& desc) {
    auto visual = resolve(desc);
    if (visual.utf8)
        draw_glyph(x, y, visual.utf8, visual.fg);
    else if (visual.bg != Color::Default)
        draw_char(x, y, visual.glyph, visual.fg, visual.bg);
    else
        draw_char(x, y, visual.glyph, visual.fg);
}

void TerminalRenderer::draw_animation(int x, int y, AnimationType type, int frame_index) {
    auto visual = resolve_animation(type, frame_index);
    if (visual.utf8)
        draw_glyph(x, y, visual.utf8, visual.fg);
    else
        draw_char(x, y, visual.glyph, visual.fg);
}
```

- [ ] **Step 5: Add overrides to SdlRenderer**

In `include/astra/sdl_renderer.h`, add to the public section:

```cpp
    void draw_entity(int x, int y, const RenderDescriptor& desc) override;
    void draw_animation(int x, int y, AnimationType type, int frame_index) override;
```

In `src/sdl_renderer.cpp`, add stubs:

```cpp
#include "astra/render_descriptor.h"

void SdlRenderer::draw_entity(int x, int y, const RenderDescriptor& desc) {
    (void)desc;
    draw_char(x, y, '?');  // stub
}

void SdlRenderer::draw_animation(int x, int y, AnimationType type, int frame_index) {
    (void)type; (void)frame_index;
    draw_char(x, y, '*');  // stub
}
```

- [ ] **Step 6: Check if terminal_renderer_win.cpp defines a separate class**

Read `src/terminal_renderer_win.cpp` to determine if it defines its own class or is conditionally compiled as the same `TerminalRenderer`. If it's a separate class, add the same overrides. If it's the same class with `#ifdef` guards, the declarations in the shared header cover it.

- [ ] **Step 7: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -10`
Expected: Build succeeds. No functionality changes.

- [ ] **Step 8: Commit**

```bash
git add include/astra/renderer.h include/astra/terminal_renderer.h src/terminal_renderer.cpp
git add include/astra/sdl_renderer.h src/sdl_renderer.cpp
git commit -m "Add draw_entity() and draw_animation() to Renderer interface"
```

---

## Task 3: Create Terminal Theme (Stub)

**Files:**
- Create: `src/terminal_theme.h`
- Create: `src/terminal_theme.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create terminal_theme.h**

```cpp
// src/terminal_theme.h
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
```

- [ ] **Step 2: Create terminal_theme.cpp**

```cpp
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
```

- [ ] **Step 3: Add terminal_theme.cpp to CMakeLists.txt**

In `CMakeLists.txt`, add `src/terminal_theme.cpp` to the `ASTRA_SOURCES` list, near the other renderer-adjacent files (after `src/map_renderer.cpp` is a good spot):

```cmake
    src/terminal_theme.cpp
```

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add src/terminal_theme.h src/terminal_theme.cpp CMakeLists.txt
git commit -m "Add terminal theme stub — resolve() returns fallback visuals"
```

---

## Task 4: Rename DrawContext to UIContext

**Files:**
- Modify: `include/astra/ui.h`
- Modify: `src/ui.cpp`

This is the most careful step — we rename the class but add a `using` alias so all existing code continues to compile without changes.

- [ ] **Step 1: Rename the class in ui.h**

In `include/astra/ui.h`, rename `class DrawContext` to `class UIContext`. Then add a backward-compatibility alias after the class definition:

Replace:
```cpp
class DrawContext {
```
with:
```cpp
class UIContext {
```

After the closing `};` of the class, add:

```cpp
// Backward-compatibility alias — remove after UI semantic redesign
using DrawContext = UIContext;
```

- [ ] **Step 2: Rename in ui.cpp**

In `src/ui.cpp`, rename all `DrawContext::` method prefixes to `UIContext::`. This is a find-and-replace of `DrawContext::` → `UIContext::` across the file.

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds. The `using DrawContext = UIContext;` alias means zero changes needed in any consumer file.

- [ ] **Step 4: Commit**

```bash
git add include/astra/ui.h src/ui.cpp
git commit -m "Rename DrawContext to UIContext — alias preserves backward compat"
```

---

## Task 5: Create WorldContext

**Files:**
- Create: `include/astra/world_context.h`

- [ ] **Step 1: Create world_context.h**

```cpp
// include/astra/world_context.h
#pragma once

#include "astra/renderer.h"
#include "astra/ui.h"              // Rect
#include "astra/render_descriptor.h"

namespace astra {

// Rendering context for game-world entities.
// Translates local coordinates to screen coordinates and delegates
// to Renderer::draw_entity(). Has no knowledge of glyphs or colors.
class WorldContext {
public:
    WorldContext(Renderer* r, Rect bounds)
        : renderer_(r), bounds_(bounds) {}

    void put(int x, int y, const RenderDescriptor& desc) {
        int sx = bounds_.x + x;
        int sy = bounds_.y + y;
        if (sx < bounds_.x || sx >= bounds_.x + bounds_.w) return;
        if (sy < bounds_.y || sy >= bounds_.y + bounds_.h) return;
        renderer_->draw_entity(sx, sy, desc);
    }

    void put_animation(int x, int y, AnimationType type, int frame_index) {
        int sx = bounds_.x + x;
        int sy = bounds_.y + y;
        if (sx < bounds_.x || sx >= bounds_.x + bounds_.w) return;
        if (sy < bounds_.y || sy >= bounds_.y + bounds_.h) return;
        renderer_->draw_animation(sx, sy, type, frame_index);
    }

    const Rect& bounds() const { return bounds_; }
    int width() const { return bounds_.w; }
    int height() const { return bounds_.h; }

private:
    Renderer* renderer_;
    Rect bounds_;
};

} // namespace astra
```

- [ ] **Step 2: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds (header not included anywhere yet).

- [ ] **Step 3: Commit**

```bash
git add include/astra/world_context.h
git commit -m "Add WorldContext — semantic coordinate translator for game world"
```

---

## Task 6: Migrate map_renderer to WorldContext

**Files:**
- Modify: `include/astra/map_renderer.h`
- Modify: `src/map_renderer.cpp`

This step changes `map_renderer` to create a `WorldContext` instead of a `DrawContext` for the map viewport. Since `WorldContext::put()` only accepts `RenderDescriptor` (not raw glyphs), and we haven't migrated tile resolution yet, we keep the existing `DrawContext` for now but **also** create a `WorldContext` alongside it. The actual migration of `ctx.put()` calls from glyph-based to descriptor-based happens in the Tiles plan (Step 2 of the overall migration).

- [ ] **Step 1: Add world_context.h include to map_renderer.h**

In `include/astra/map_renderer.h`, add after the existing includes:

```cpp
#include "astra/world_context.h"
```

- [ ] **Step 2: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds. No functional changes — `WorldContext` is included but not used yet. The migration of actual `ctx.put()` calls happens in the Tiles implementation plan.

- [ ] **Step 3: Commit**

```bash
git add include/astra/map_renderer.h
git commit -m "Include WorldContext in map_renderer — ready for tile migration"
```

---

## Task 7: Verify Full Build and Test

**Files:** None (verification only)

- [ ] **Step 1: Clean build**

Run: `rm -rf build && cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -10`
Expected: Build succeeds with zero warnings related to our changes.

- [ ] **Step 2: Run the game**

Run: `./build/astra-dev`
Expected: Game launches, main menu displays, gameplay works identically. No visual changes.

- [ ] **Step 3: Verify no regressions**

Play briefly: navigate menus, enter gameplay, move around, check that tiles/NPCs/items render correctly. The foundation changes are purely additive — nothing should look or behave differently.

- [ ] **Step 4: Final commit (if any fixups needed)**

If any fixes were needed in previous steps, commit them here. Otherwise skip.

---

## Summary

After this plan is complete:
- `RenderDescriptor` exists and is available to all game code
- `Renderer` has `draw_entity()` and `draw_animation()` virtual methods
- `TerminalRenderer` implements both via `terminal_theme.cpp` (stub returns `'?'` / Magenta)
- `SdlRenderer` has stub implementations
- `DrawContext` is renamed to `UIContext` with backward-compat alias
- `WorldContext` exists for game-world rendering via descriptors
- `map_renderer.h` includes `WorldContext` ready for the Tiles migration
- Everything compiles and runs identically — zero visual changes

**Next plan:** Tiles migration — move tile glyph/color resolution into `terminal_theme.cpp`, change `map_renderer.cpp` to build `RenderDescriptor` and use `WorldContext`.
