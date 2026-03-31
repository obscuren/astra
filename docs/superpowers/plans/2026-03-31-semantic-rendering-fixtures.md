# Semantic Rendering: Fixtures Migration — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move fixture glyph/color resolution from `FixtureData` and `make_fixture()` into `terminal_theme.cpp`. Remove `glyph`, `utf8_glyph`, and `color` from `FixtureData`. Fixture rendering in map_renderer and map_editor uses `RenderDescriptor` via `WorldContext`.

**Architecture:** Fixtures become semantic — `FixtureData` carries `FixtureType` plus gameplay state (passable, interactable, cooldown, etc.) but no visual data. The renderer resolves `{category: Fixture, type_id: FixtureType, flags: ...}` to glyph+color. Save files need a format version bump since `glyph` and `color` are currently serialized.

**Tech Stack:** C++20, CMake

**Spec:** `docs/superpowers/specs/2026-03-31-semantic-rendering-design.md`

**Branch:** `semantic-rendering` (worktree at `.worktrees/semantic-rendering/`)

---

## File Structure

| Action | Path | Responsibility |
|--------|------|----------------|
| Modify | `src/terminal_theme.cpp` | Add `RenderCategory::Fixture` resolution — FixtureType → glyph/color |
| Modify | `include/astra/tilemap.h` | Remove `glyph`, `utf8_glyph`, `color` from `FixtureData` |
| Modify | `src/tilemap.cpp` | Remove glyph/color assignments from `make_fixture()` |
| Modify | `src/map_renderer.cpp` | Fixture rendering uses `RenderDescriptor` + `WorldContext` |
| Modify | `src/map_editor.cpp` | Fixture rendering in viewport uses `RenderDescriptor` + `WorldContext` |
| Modify | `src/map_editor.cpp` | Fixture palette display uses theme resolution for glyph |
| Modify | `src/save_file.cpp` | Bump save version, stop writing glyph/color, handle old format on read |
| Modify | `include/astra/render_descriptor.h` | Add `RF_Open` flag for door state |

---

## Task 1: Add Fixture Resolution to Terminal Theme

**Files:**
- Modify: `src/terminal_theme.cpp`
- Modify: `include/astra/render_descriptor.h`

- [ ] **Step 1: Add RF_Open flag to render_descriptor.h**

Doors have open/closed state that affects their visual. Add to `RenderFlag`:

```cpp
    RF_Open         = 1 << 7,  // door is open
```

Note: this uses the last bit of a uint8_t. If we need more flags later, we'll widen to uint16_t.

- [ ] **Step 2: Read make_fixture() in tilemap.cpp**

Read the full `make_fixture()` function to understand all 27 fixture type → glyph/color mappings.

- [ ] **Step 3: Add fixture resolution to resolve() in terminal_theme.cpp**

Add a `resolve_fixture()` static function and wire it into the main `resolve()`:

```cpp
static ResolvedVisual resolve_fixture(uint16_t type_id, uint8_t flags) {
    auto type = static_cast<FixtureType>(type_id);
    bool remembered = (flags & RF_Remembered) != 0;

    char glyph = '?';
    const char* utf8 = nullptr;
    Color color = Color::White;

    switch (type) {
        case FixtureType::Door:
            if (flags & RF_Open) {
                glyph = '\''; utf8 = nullptr; color = static_cast<Color>(137);
            } else {
                glyph = '+'; utf8 = nullptr; color = static_cast<Color>(137);
            }
            break;
        case FixtureType::Window:
            glyph = 'O'; utf8 = nullptr; color = Color::Cyan; break;
        case FixtureType::Table:
            glyph = 'o'; utf8 = "\xc2\xa4"; color = Color::DarkGray; break;
        case FixtureType::Console:
            glyph = '#'; utf8 = "\xe2\x95\xac"; color = Color::Cyan; break;
        case FixtureType::Crate:
            glyph = '='; utf8 = "\xe2\x96\xa0"; color = Color::Yellow; break;
        case FixtureType::Bunk:
            glyph = '='; utf8 = "\xe2\x89\xa1"; color = Color::DarkGray; break;
        case FixtureType::Rack:
            glyph = '|'; utf8 = "\xe2\x95\x8f"; color = Color::DarkGray; break;
        case FixtureType::Conduit:
            glyph = '%'; utf8 = "\xe2\x95\xa3"; color = Color::DarkGray; break;
        case FixtureType::ShuttleClamp:
            glyph = '='; utf8 = "\xe2\x95\xa4"; color = Color::White; break;
        case FixtureType::Shelf:
            glyph = '['; utf8 = "\xe2\x95\x94"; color = Color::DarkGray; break;
        case FixtureType::Viewport:
            glyph = '"'; utf8 = "\xe2\x96\x91"; color = Color::Cyan; break;
        case FixtureType::Torch:
            glyph = '*'; utf8 = nullptr; color = Color::Yellow; break;
        case FixtureType::Stool:
            glyph = 'o'; utf8 = "\xc2\xb7"; color = Color::DarkGray; break;
        case FixtureType::Debris:
            glyph = ','; utf8 = nullptr; color = Color::DarkGray; break;
        case FixtureType::HealPod:
            glyph = '+'; utf8 = "\xe2\x9c\x9a"; color = Color::Green; break;
        case FixtureType::FoodTerminal:
            glyph = '$'; utf8 = nullptr; color = Color::Yellow; break;
        case FixtureType::WeaponDisplay:
            glyph = '/'; utf8 = "\xe2\x80\xa0"; color = Color::Red; break;
        case FixtureType::RepairBench:
            glyph = '%'; utf8 = "\xe2\x95\xaa"; color = Color::Cyan; break;
        case FixtureType::SupplyLocker:
            glyph = '&'; utf8 = "\xe2\x96\xaa"; color = Color::Yellow; break;
        case FixtureType::StarChart:
            glyph = '*'; utf8 = nullptr; color = Color::Cyan; break;
        case FixtureType::RestPod:
            glyph = '='; utf8 = "\xe2\x88\xa9"; color = Color::Green; break;
        case FixtureType::ShipTerminal:
            glyph = '>'; utf8 = "\xc2\xbb"; color = Color::Yellow; break;
        case FixtureType::CommandTerminal:
            glyph = '#'; utf8 = "\xe2\x96\xa3"; color = Color::Cyan; break;
        case FixtureType::DungeonHatch:
            glyph = 'v'; utf8 = "\xe2\x96\xbc"; color = Color::Yellow; break;
        case FixtureType::StairsUp:
            glyph = '<'; utf8 = "\xe2\x96\xb2"; color = Color::White; break;
    }

    if (remembered) {
        // Use biome remembered color — but fixtures don't carry biome in descriptor.
        // Use a generic dim color for remembered fixtures.
        color = Color::DarkGray;
    }

    return {glyph, utf8, color, Color::Default};
}
```

Wire into `resolve()`:
```cpp
if (desc.category == RenderCategory::Fixture) {
    return resolve_fixture(desc.type_id, desc.flags);
}
```

Note about remembered fixtures: currently `map_renderer.cpp` uses `biome_colors(biome).remembered` for remembered fixture color. The descriptor carries `biome`, so we can use `biome_palette(desc.biome).remembered` instead of generic DarkGray. Update the function:

```cpp
if (remembered) {
    color = biome_palette(desc.biome).remembered;
}
```

This requires the function signature to accept biome: `resolve_fixture(uint16_t type_id, uint8_t flags, Biome biome)`.

- [ ] **Step 4: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`

- [ ] **Step 5: Commit**

```bash
git add src/terminal_theme.cpp include/astra/render_descriptor.h
git commit -m "Add fixture resolution to terminal theme"
```

---

## Task 2: Migrate Fixture Rendering in map_renderer.cpp

**Files:**
- Modify: `src/map_renderer.cpp`

- [ ] **Step 1: Read current fixture rendering code**

Read the fixture rendering sections in `render_map()` — both visible and remembered paths.

- [ ] **Step 2: Replace fixture rendering with descriptors**

For **visible fixtures**:
```cpp
if (tile_at == Tile::Fixture) {
    int fid = rc.world.map().fixture_id(mx, my);
    if (fid >= 0 && fid < rc.world.map().fixture_count()) {
        const auto& f = rc.world.map().fixture(fid);

        // Animation override stays on old path (animations not migrated yet)
        if (rc.animations) {
            if (auto* frame = rc.animations->query(mx, my)) {
                if (frame->utf8) ctx.put(sx, sy, frame->utf8, frame->color);
                else ctx.put(sx, sy, frame->glyph, frame->color);
                continue;
            }
        }

        RenderDescriptor desc;
        desc.category = RenderCategory::Fixture;
        desc.type_id = static_cast<uint16_t>(f.type);
        desc.biome = biome;
        desc.flags = RF_Lit;
        if (f.open) desc.flags |= RF_Open;
        wctx.put(sx, sy, desc);
    } else {
        ctx.put(sx, sy, '?', Color::Red);  // invalid fixture
    }
    continue;
}
```

For **remembered fixtures**:
```cpp
if (tile_at == Tile::Fixture) {
    int fid = rc.world.map().fixture_id(mx, my);
    if (fid >= 0 && fid < rc.world.map().fixture_count()) {
        const auto& f = rc.world.map().fixture(fid);
        RenderDescriptor desc;
        desc.category = RenderCategory::Fixture;
        desc.type_id = static_cast<uint16_t>(f.type);
        desc.biome = biome;
        desc.flags = RF_Remembered;
        if (f.open) desc.flags |= RF_Open;
        wctx.put(sx, sy, desc);
    }
    continue;
}
```

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`

- [ ] **Step 4: Commit**

```bash
git add src/map_renderer.cpp
git commit -m "Migrate fixture rendering to RenderDescriptor in map_renderer"
```

---

## Task 3: Migrate Fixture Rendering in map_editor.cpp

**Files:**
- Modify: `src/map_editor.cpp`

- [ ] **Step 1: Read current fixture rendering in draw_viewport()**

- [ ] **Step 2: Replace fixture viewport rendering with descriptors**

```cpp
} else if (t == Tile::Fixture) {
    int fid = map.fixture_id(mx, my);
    if (fid >= 0) {
        auto& f = map.fixture(fid);
        RenderDescriptor desc;
        desc.category = RenderCategory::Fixture;
        desc.type_id = static_cast<uint16_t>(f.type);
        desc.biome = map.biome();
        desc.flags = RF_Lit;
        if (f.open) desc.flags |= RF_Open;
        wctx.put(sx, sy, desc);
    }
}
```

- [ ] **Step 3: Update fixture palette display**

The palette currently does `make_fixture(type)` and reads `fd.glyph` for display. Since we're removing visual fields from FixtureData, the palette needs another way to get the display glyph. Two options:

a) Call the theme's resolve function directly — but it's terminal-internal and the editor shouldn't depend on it.
b) Keep using `make_fixture()` but the glyph field will be gone.

The cleanest approach: the palette is UI (not game-world rendering), so it stays on `UIContext`/`DrawContext`. We can add a small helper that resolves a fixture type to its ASCII glyph for UI display. Add to `terminal_theme.h`:

```cpp
// UI helper — returns the ASCII glyph for a fixture type (for palette display etc.)
char fixture_glyph(FixtureType type);
```

Implement in `terminal_theme.cpp` — simple switch returning the `glyph` field from `resolve_fixture`.

Update the palette code:
```cpp
#include "terminal_theme.h"  // fixture_glyph()

// In palette rendering:
char g = fixture_glyph(fixture_palette_[i]);
std::string line = std::string(1, marker) + " " + g + " " + fixture_name(fixture_palette_[i]);
```

Wait — including `terminal_theme.h` in map_editor breaks the separation (terminal_theme is terminal-internal). Better approach: the Renderer interface should provide this. But that's overengineering for a dev tool.

Pragmatic solution: keep including `terminal_theme.h` in map_editor for now. The editor is already terminal-specific (dev mode only). Add a TODO comment.

- [ ] **Step 4: Build and verify**

- [ ] **Step 5: Commit**

```bash
git add src/map_editor.cpp
git commit -m "Migrate fixture rendering in map editor to RenderDescriptor"
```

---

## Task 4: Remove Visual Fields from FixtureData

**Files:**
- Modify: `include/astra/tilemap.h`
- Modify: `src/tilemap.cpp`
- Modify: `src/save_file.cpp`

- [ ] **Step 1: Search for remaining readers of fixture visual fields**

Grep for `\.glyph`, `\.utf8_glyph`, `\.color` on fixture objects. After Tasks 2 and 3, there should be no readers of visual fields except:
- `save_file.cpp` (serialization)
- `make_fixture()` (assignment)
- Possibly animation code

If any unexpected readers remain, handle them.

- [ ] **Step 2: Remove visual fields from FixtureData**

In `include/astra/tilemap.h`, remove from `FixtureData`:
```cpp
    // REMOVE these:
    char glyph = '?';
    const char* utf8_glyph = nullptr;
    Color color = Color::White;
```

- [ ] **Step 3: Remove visual assignments from make_fixture()**

In `src/tilemap.cpp`, remove all `fd.glyph = ...`, `fd.utf8_glyph = ...`, `fd.color = ...` lines from `make_fixture()`. The function now only sets gameplay properties (type, passable, interactable, cooldown, light_radius, etc.).

- [ ] **Step 4: Update save_file.cpp**

The save format currently writes `glyph` and `color` for each fixture. We need to:

1. Bump the save format version (check current version number first).
2. In the **write** path: stop writing `glyph` and `color`.
3. In the **read** path: for old versions, read and discard `glyph` and `color`. For new version, don't expect them.

Read the current save format version handling first to understand how versioning works.

**Write path change:**
```cpp
// New version: only write gameplay fields
w.write_u8(static_cast<uint8_t>(f.type));
// REMOVED: w.write_u8(static_cast<uint8_t>(f.glyph));
// REMOVED: w.write_u8(static_cast<uint8_t>(f.color));
w.write_u8(f.passable ? 1 : 0);
w.write_u8(f.interactable ? 1 : 0);
w.write_i32(f.cooldown);
w.write_i32(f.last_used_tick);
```

**Read path change (backward compat):**
```cpp
f.type = static_cast<FixtureType>(r.read_u8());
if (version < NEW_VERSION) {
    r.read_u8();  // skip legacy glyph
    r.read_u8();  // skip legacy color
}
f.passable = r.read_u8() != 0;
f.interactable = r.read_u8() != 0;
f.cooldown = r.read_i32();
f.last_used_tick = r.read_i32();
```

Also need to reconstruct non-visual fields that `make_fixture()` used to set. After removing visual assignments, `make_fixture()` still sets gameplay fields. On load, we need to ensure fields like `light_radius` are set. Currently they're NOT serialized — they come from `make_fixture()`. Check if there's a post-load reconstruction step, or if fixtures loaded from save rely on the struct defaults.

- [ ] **Step 5: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -10`
Fix any remaining references to removed fields.

- [ ] **Step 6: Commit**

```bash
git add include/astra/tilemap.h src/tilemap.cpp src/save_file.cpp
git commit -m "Remove visual fields from FixtureData, bump save format"
```

---

## Task 5: Final Verification

- [ ] **Step 1: Clean build**

Run: `rm -rf build && cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -10`

- [ ] **Step 2: Run and verify**

Run: `./build/astra-dev`
Verify:
- Fixtures render with correct glyphs and colors (consoles ╬, tables ¤, doors +, etc.)
- Remembered fixtures dimmed correctly
- Animated fixtures (Console blink, Torch flicker) still work
- Map editor shows fixtures correctly
- Map editor palette shows fixture glyphs

- [ ] **Step 3: Commit if any fixups needed**

---

## Summary

After this plan:
- Fixture visual resolution lives entirely in `terminal_theme.cpp`
- `FixtureData` has no `glyph`, `utf8_glyph`, or `color` fields
- `make_fixture()` only sets gameplay properties
- Save format bumped, backward-compatible with old saves
- Map renderer and editor use `RenderDescriptor` for fixtures
- Animation overrides still work (on old DrawContext path until animation migration)

**Next plan:** NPCs migration — remove `glyph`/`color` from `Npc`, role→visual mapping to theme.
