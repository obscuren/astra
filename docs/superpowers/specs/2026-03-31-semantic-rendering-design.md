# Semantic Rendering Model

**Date:** 2026-03-31
**Status:** Approved
**Branch:** `semantic-rendering` (feature branch off main)

## Problem

Game code currently specifies *how* things look — glyphs, UTF-8 characters, and colors are embedded in game-side files (`tilemap.cpp`, `map_renderer.cpp`, NPC/item builders, `animation.cpp`). The renderer receives pre-resolved visual data (`draw_char('▓', Color::Green)`) and acts as a dumb pixel-pusher.

This couples game logic to visual representation. Changing how a wall looks requires editing game code. Adding a new renderer (SDL) means the game-side visuals are terminal-biased. The game should describe *what* exists and the renderer should decide *how* to present it.

## Design

### RenderDescriptor

The core data structure. Game code builds these instead of specifying glyphs/colors.

```cpp
// include/astra/render_descriptor.h

enum class RenderCategory : uint8_t {
    Tile, Fixture, Npc, Item, Effect, Player
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
    RenderCategory category;
    uint16_t type_id;     // enum value cast: Tile::Wall, FixtureType::Door, NpcRole::Merchant, etc.
    uint8_t  seed;        // position-based variation (deterministic hash of x,y)
    uint8_t  flags;       // RenderFlag bitfield
    Biome    biome;       // contextual — renderer ignores when irrelevant for category
    Rarity   rarity;      // contextual — items only, ignored otherwise
};
```

`RenderDescriptor` is ~8 bytes, trivially copyable, no heap allocations. Unused fields for a given category are simply ignored by the resolver.

This struct covers game world entities only. UI rendering is out of scope (see Context Split below).

### Renderer Owns Resolution and Drawing

The abstract `Renderer` interface gains a new virtual method:

```cpp
// include/astra/renderer.h
virtual void draw_entity(int x, int y, const RenderDescriptor& desc) = 0;
```

Each renderer owns the full pipeline from descriptor to pixels. Resolution is internal to each renderer — not part of the shared interface.

**Terminal renderer** — resolves descriptors to glyphs/colors, writes to cell buffer:

```cpp
// src/terminal_theme.h (internal to terminal renderer)

struct ResolvedVisual {
    char glyph;
    const char* utf8;    // nullptr = use glyph
    Color fg;
    Color bg;            // Color::Default if none
};

ResolvedVisual resolve(const RenderDescriptor& desc);
```

`resolve()` and `ResolvedVisual` live in `terminal_theme.h/.cpp` — they are terminal-specific internals, not shared types. `TerminalRenderer::draw_entity()` calls `resolve()` then writes to its cell buffer.

Example resolution inside `terminal_theme.cpp`:

```cpp
case RenderCategory::Tile: {
    auto tile = static_cast<Tile>(desc.type_id);
    switch (tile) {
        case Tile::Wall: {
            auto colors = biome_palette(desc.biome);
            auto glyph = wall_variant(desc.seed);
            if (desc.flags & RF_Remembered)
                return {glyph.ch, glyph.utf8, colors.remembered, Color::Default};
            return {glyph.ch, glyph.utf8, colors.wall, Color::Default};
        }
    }
}
case RenderCategory::Npc: {
    auto role = static_cast<NpcRole>(desc.type_id);
    // terminal decides 'M'/Cyan for Merchant, 'X'/Red for Xytomorph, etc.
}
```

**SDL renderer** — resolves descriptors to sprite IDs / texture regions, renders images. No glyphs involved. `SdlRenderer::draw_entity()` calls its own resolution logic (e.g. `sdl_theme.cpp`) mapping the same descriptors to graphical assets.

### Context Split

`DrawContext` is split into two classes:

- **`WorldContext`** — new, accepts `RenderDescriptor`. Used by `map_renderer.cpp` and game world rendering. Calls `resolve()` internally to get visuals, then draws them.
- **`UIContext`** — the current `DrawContext` renamed. Keeps the existing glyph/color API (`put(x, y, char, Color)`). Used by all screen/panel code (character screen, trade window, help, menus, etc.).

This separation means:
- The game world refactor is fully isolated from UI code
- UI gets its own semantic redesign later as a separate project
- No risk of the two migrations conflicting

`WorldContext::put()` is a thin coordinate translator — it passes the descriptor straight to the renderer:

```cpp
void WorldContext::put(int x, int y, const RenderDescriptor& desc) {
    renderer_->draw_entity(ox_ + x, oy_ + y, desc);
}
```

`WorldContext` has no knowledge of glyphs, colors, or sprites. It only translates local coordinates to screen coordinates and delegates to the renderer.

### Animations

Animation frames become semantic. The `AnimationManager` tracks type and frame index; the renderer owns the visual representation of each frame.

```cpp
enum class AnimationType : uint8_t {
    ConsoleBlink, WaterShimmer, TorchFlicker,
    DamageFlash, HealPulse, Projectile, LevelUp
};

struct AnimationDef {
    AnimationType type;
    std::vector<int> frame_durations_ms;  // duration per frame in milliseconds
    bool looping;
};
```

Frame durations stay in `AnimationDef` (they're timing, not visuals — the game controls pacing, the renderer controls appearance). Frame count is implicit from `frame_durations_ms.size()`.

The renderer resolves animation visuals internally. The abstract `Renderer` interface gains:

```cpp
virtual void draw_animation(int x, int y, AnimationType type, int frame_index) = 0;
```

Each renderer owns the visual mapping — `TerminalRenderer` resolves to glyphs/colors via `terminal_theme.cpp`, `SdlRenderer` resolves to sprite frames. The priority chain is unchanged: effect animations > fixture animations > base tile descriptor.

### What Moves Where

| Current Location | What | Destination |
|---|---|---|
| `tilemap.h` | `tile_glyph()`, `overworld_glyph()` | `terminal_theme.cpp` |
| `tilemap.cpp` | `dungeon_wall_glyph()`, `dungeon_water_glyph()`, `biome_colors()` | `terminal_theme.cpp` |
| `tilemap.cpp` | `make_fixture()` glyph/color assignments | `terminal_theme.cpp` |
| `map_renderer.cpp` | `overworld_tile_color()`, `floor_scatter()` | `terminal_theme.cpp` |
| `map_renderer.cpp` | Hardcoded `'@'` / `Yellow` for player | `terminal_theme.cpp` |
| NPC builders | `npc.glyph`, `npc.color` assignments | `terminal_theme.cpp` |
| Item builders | `item.glyph`, `item.color` assignments | `terminal_theme.cpp` |
| `item.h` | `rarity_color()` | `terminal_theme.cpp` |
| `animation.cpp` | Frame glyph/color definitions | `terminal_theme.cpp` |

### What Gets Removed from Game-Side Structs

| Struct | Fields Removed |
|---|---|
| `Npc` | `glyph`, `color` |
| `Item` | `glyph`, `color` |
| `FixtureData` | `glyph`, `utf8_glyph`, `color` |
| `AnimationFrame` | `glyph`, `utf8`, `color` (struct replaced by `AnimationType` + frame index) |

### Seed-Based Variation

The game provides a deterministic seed for position-varied visuals. The renderer uses it to select glyph variants.

```cpp
uint8_t position_seed(int x, int y);  // deterministic hash, lives in game code
```

This ensures visual consistency across renderers — the same seed produces the same variant selection, but each renderer maps that variant to its own visual (terminal: UTF-8 glyph, SDL: sprite, etc.).

### Out of Scope

- **UI semantic rendering** — `UIContext` keeps current API, separate design later
- **Boot sequence** — stays as-is, direct renderer calls
- **SDL renderer theme** — `sdl_theme.cpp` created when SDL work begins
- **New render categories** — can be added to `RenderCategory` as needed

## Migration Plan

Incremental, one commit per step on the `semantic-rendering` feature branch. Each step leaves the game in a working state.

1. **Foundation** — Add `render_descriptor.h`, `terminal_theme.h/.cpp` with stub `resolve()`. Split `DrawContext` into `WorldContext` and `UIContext`. Migrate `map_renderer.cpp` to `WorldContext`, all screen code to `UIContext`. Everything compiles and works identically.

2. **Tiles** — Migrate dungeon and overworld tiles. Move all tile glyph/color functions into `terminal_theme.cpp`. `map_renderer.cpp` builds `RenderDescriptor` instead of calling `tile_glyph()` / `biome_colors()`. Remove old functions from `tilemap.h/cpp`.

3. **Fixtures** — Move fixture glyph/color out of `make_fixture()` into theme. Remove `glyph`, `utf8_glyph`, `color` from `FixtureData`. `map_renderer.cpp` builds fixture descriptors from `FixtureType`.

4. **NPCs** — Remove `glyph` and `color` from `Npc`. NPC builders stop setting visual fields. Role-to-visual mapping moves to theme.

5. **Items** — Remove `glyph` and `color` from `Item`. Type/rarity-to-visual mapping moves to theme.

6. **Player** — Remove hardcoded `'@'` / `Yellow`. Player gets `RenderCategory::Player` descriptor.

7. **Animations** — Replace `AnimationFrame` glyph/color with semantic `AnimationType` + frame index. Frame visuals move to theme via `resolve_animation()`.

8. **Cleanup** — Remove dead code, unused includes, verify no visual data in game-side files.

Each step gets its own detailed implementation plan before execution.
