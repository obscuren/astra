# Semantic Rendering Migration Report

**Branch:** `semantic-rendering`
**Date:** 2026-03-31
**Commits:** 37

## Motivation

The renderer was acting as a dumb pixel-pusher — game code specified exactly how things look (glyphs, UTF-8 characters, colors) and the renderer just drew what it was told. This meant:

- Changing how a wall looks required editing game code
- Visual data (glyph chars, color values) was scattered across tilemap.h, map_renderer.cpp, NPC builders, item builders, and animation definitions
- Adding a second renderer (SDL) would inherit terminal-biased visuals — the game was telling the renderer to draw `▓` in green, which is meaningless to a sprite-based renderer
- The game couldn't describe *what* exists without also saying *how it looks*

The goal: game code describes **what** exists semantically, and each renderer decides **how** to present it. A wall is `{Tile::Wall, Biome::Volcanic}` — the terminal renderer draws `▓` in red, SDL could draw a lava-rock sprite.

## Architecture

### RenderDescriptor

A flat 8-byte struct that carries semantic data:

```cpp
struct RenderDescriptor {
    RenderCategory category;  // Tile, Fixture, Npc, Item, Effect, Player
    uint16_t type_id;         // enum value (Tile::Wall, FixtureType::Door, NpcRole::Merchant, etc.)
    uint8_t  seed;            // position-based variation (deterministic hash)
    uint8_t  flags;           // RF_Remembered, RF_Lit, RF_Hostile, RF_Open, etc.
    Biome    biome;           // contextual
    Rarity   rarity;          // contextual (items)
};
```

Game code builds these. The renderer consumes them. Game code never specifies a glyph or color.

### Renderer Interface

Two new virtual methods on `Renderer`:

- `draw_entity(int x, int y, const RenderDescriptor& desc)` — each renderer resolves the descriptor to backend-specific visuals
- `draw_animation(int x, int y, AnimationType type, int frame_index)` — each renderer resolves animation frame visuals

The terminal renderer resolves to glyphs+colors via `terminal_theme.cpp`. SDL would resolve to sprites.

### Context Split

`DrawContext` was renamed to `UIContext` (with backward-compat alias). A new `WorldContext` was created for game-world rendering — it accepts `RenderDescriptor` and delegates to `draw_entity()`. UI rendering stays on `UIContext` with raw glyphs/colors until a separate UI semantic redesign.

### Terminal Theme

`terminal_theme.cpp` is the terminal renderer's visual brain. It contains:

- `resolve(RenderDescriptor)` — maps any game entity to a glyph+color
- `resolve_animation(AnimationType, frame_index)` — maps animation frames to visuals
- All biome color palettes, wall glyph variants, floor scatter, fixture visuals, NPC glyphs, item visuals
- Position-seed-based variation for visual variety (deterministic: same position always looks the same)

## What Changed Per Entity

### Tiles

All tile glyph/color functions moved from `tilemap.h`/`tilemap.cpp`/`map_renderer.cpp` into `terminal_theme.cpp`:

- `tile_glyph()`, `overworld_glyph()`, `dungeon_wall_glyph()`, `dungeon_water_glyph()`, `dungeon_portal_glyph()` — removed from tilemap.h
- `biome_colors()`, `floor_scatter()`, `star_at()`, `overworld_tile_color()` — moved to theme
- `map_renderer.cpp` and `map_editor.cpp` build `RenderDescriptor` for each tile

Seed-based variation: the game provides `position_seed(x, y)` (deterministic 8-bit hash), the renderer picks a glyph variant from it. Ensures visual consistency across renderers.

### Fixtures

Functional fixtures (terminals, doors, torches, heal pods, etc.) resolve visuals from `FixtureType` via the theme. The `glyph`, `utf8_glyph`, and `color` fields were removed from `FixtureData`.

Door open/close state is communicated via the `RF_Open` flag on the descriptor — the renderer decides what an open door looks like.

### Environmental Decorations

This was the biggest design discovery. The old system placed ~70 unique decorative fixtures (wildflowers, boulders, ferns, crystals) as `FixtureType::Debris` with per-instance glyph/color overrides. This fundamentally conflicted with semantic rendering.

The solution split decorations into two categories:

**Passable decorations** (flowers, grass, ferns, pebbles) — removed from game state entirely. They don't affect gameplay (no collision, no interaction). The renderer generates them procedurally as enriched floor scatter with three tiers:
- Tier 1 (~10%): basic dim scatter chars
- Tier 2 (~4%): biome-specific colored decorations (wildflowers, ferns, ice shards, etc.)
- Tier 3 (~1%): rare decorations

Each biome has its own scatter palette with multiple variants and colors.

**Impassable decorations** (boulders, tree stumps, thickets) — replaced with two new semantic fixture types:
- `NaturalObstacle` — renderer resolves visual from biome + seed (boulder in Rocky, tree stump in Forest, crystal in Crystal, etc.)
- `SettlementProp` — renderer resolves from seed (antenna, bench, lamp post, etc.)

The generator's scatter palettes were simplified from ~70 entries with per-instance visuals to a handful of semantic obstacle placements.

### NPCs

`NpcRole` enum expanded from 4 to 12 entries to cover all NPC types (StationKeeper, Merchant, Nova, Civilian, etc.). The `glyph` and `color` fields were removed from the `Npc` struct. Civilian NPCs vary by `Race` — the seed field carries the race for the renderer.

### Items

Each item definition got a `uint16_t item_def_id` — a numeric registry ID. The theme has a lookup table mapping ID to glyph+color. The `glyph` and `color` fields were removed from the `Item` struct. Numeric IDs were chosen over an enum because the game will have hundreds of items and potentially randomly generated ones.

All UI screens (inventory, trade, character screen, repair bench) were updated to use the theme helper `item_visual()` instead of reading `item.glyph`/`item.color` directly.

### Player

The hardcoded `'@'` Yellow was replaced with `RenderCategory::Player` in the descriptor. The theme resolves it.

### Animations

`AnimationFrame` was stripped to just `duration_ms`. The `AnimationDef` carries an `AnimationType` enum. Frame visual data (glyphs, UTF-8 characters, colors) moved to `resolve_animation()` in the theme. The `AnimationManager` now returns `AnimQueryResult{type, frame_index}` instead of raw frame pointers.

## Save Format

Three version bumps for backward compatibility:

| Version | Change |
|---------|--------|
| v17 | Fixture glyph/color no longer written. Debris migrated to NaturalObstacle. |
| v18 | NPC glyph/color replaced with NpcRole. Old saves reconstruct role from name string. |
| v19 | Item glyph/color replaced with item_def_id. Old saves reconstruct ID from item name. |

## What Remains on Old Path

These still use `UIContext` with raw glyphs/colors:
- Targeting line and reticule (`*` and `+` in green/red)
- Look cursor (`[X]` brackets)
- Overworld stamp glyph overrides (custom per-cell glyphs)
- Boot sequence (cinematic, stays as-is by design)
- All UI chrome (panels, borders, text, progress bars)

These will be addressed in a future UI semantic redesign.

## Files Added

| File | Purpose |
|------|---------|
| `include/astra/render_descriptor.h` | RenderDescriptor, RenderCategory, RenderFlag, AnimationType, position_seed() |
| `include/astra/world_context.h` | WorldContext — coordinate translator for game-world rendering |
| `include/astra/item_ids.h` | Item definition ID constants |
| `src/terminal_theme.h` | ResolvedVisual, resolve/resolve_animation declarations, UI helpers |
| `src/terminal_theme.cpp` | Full visual resolution for all entity types |

## Key Design Decisions

1. **Flat struct over variant** — `RenderDescriptor` is a simple flat struct (~8 bytes) rather than a `std::variant`. Unused fields for a given category are ignored. Simpler, no template machinery.

2. **Renderer owns the full pipeline** — `draw_entity()` is a virtual method. Each renderer resolves AND draws. `ResolvedVisual` is terminal-internal, not a shared type. SDL never sees glyphs.

3. **Seed-based variation** — deterministic `position_seed(x, y)` ensures visual consistency across renderers and save/load cycles. The same boulder always looks the same at the same position.

4. **Passable decorations are renderer-owned** — no game state for purely visual elements. Shrinks map data and ensures biome-consistent visuals.

5. **Numeric item IDs over enum** — scales to hundreds of items and future random generation without touching an enum.

6. **Incremental migration** — each entity type migrated independently with its own commit sequence. The old and new paths coexisted during transition.
