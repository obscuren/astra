# Decoration System Redesign — Renderer-Owned Environmental Decorations

## Context

The semantic rendering migration routes all fixtures through `RenderDescriptor` → `terminal_theme.cpp`. Environmental decorations (wildflowers, boulders, ferns, crystals, ~70 variants) are all `FixtureType::Debris` with per-instance glyph/color overrides. The theme resolves Debris as a single `,` DarkGray — **all overworld detail decorations are currently broken** (visually identical grey commas). This must be fixed before we can remove visual fields from `FixtureData` and before continuing with NPC/Item/Animation migrations.

The design decision: functional fixtures (terminals, doors, torches) stay as map data; environmental decorations become renderer-owned. Passable decorations (grass, flowers) don't need game state at all. Impassable decorations (boulders, stumps) need only a minimal obstacle marker.

## Approach

### New FixtureTypes

Add two entries to `FixtureType` enum:
- `NaturalObstacle` — impassable natural feature (boulder, stump, crystal, mushroom). Renderer resolves visual from biome + seed.
- `SettlementProp` — impassable settlement prop (antenna, bench, lamp post). Renderer resolves visual from seed.

Keep `Debris` for save backward compat (maps to `NaturalObstacle` on load if impassable, removed if passable).

### Passable Decorations → Enriched Floor Scatter

Passable decorations (wildflowers, tall grass, ferns, pebbles, etc.) are removed from the fixture system entirely. The renderer's `resolve_floor()` gets a second tier of biome-specific decoration scatter with colored glyphs — richer than the current dim `,"'` scatter. No game state needed.

### Generator Changes

`detail_map_generator.cpp` scatter palettes shrink to impassable entries only using `natural_obstacle()` / `settlement_prop()` helpers (no glyph/color args). The `deco()` helper is removed.

### Save Format

Bump version 16 → 17. Stop writing glyph/color. Old saves: read and discard 2 bytes, migrate Debris → NaturalObstacle (impassable) or remove (passable).

---

## Files to Modify

| File | Change |
|------|--------|
| `include/astra/tilemap.h` | Add `NaturalObstacle`, `SettlementProp` to FixtureType enum |
| `src/tilemap.cpp` | Add `make_fixture()` cases for new types |
| `src/terminal_theme.cpp` | Add NaturalObstacle/SettlementProp resolution (biome+seed → visual). Enrich `resolve_floor()` with decoration scatter tiers. Add `fixture_glyph()` entries. |
| `src/generators/detail_map_generator.cpp` | Replace `deco()` with `natural_obstacle()`/`settlement_prop()`. Remove passable entries from palettes. |
| `src/map_renderer.cpp:138` | Add `desc.seed = position_seed(mx, my)` to fixture descriptor path |
| `src/map_editor.cpp` | Add new types to editor fixture palette + name mapping |
| `src/save_file.cpp` | Bump to v17. Write path: drop glyph/color. Read path: skip 2 bytes for v3-v16, migrate Debris. |
| `include/astra/save_file.h:42` | `version = 17` |

## Implementation Steps

### Step 1: Add NaturalObstacle and SettlementProp (additive, no breakage)
- Add enum entries to `FixtureType` in `tilemap.h`
- Add `make_fixture()` cases in `tilemap.cpp` (passable=false, interactable=false)
- Add `fixture_glyph()` and `fixture_name()` entries
- Add resolver cases in `terminal_theme.cpp` with biome-aware visuals for NaturalObstacle and seed-varied visuals for SettlementProp
- Add `desc.seed = position_seed(mx, my)` to fixture descriptor in `map_renderer.cpp` (line ~138) and `map_editor.cpp`
- Commit

### Step 2: Enrich floor scatter (additive, visual improvement)
- Expand `resolve_floor()` in `terminal_theme.cpp` with decoration tiers:
  - Tier 1 (existing): basic scatter chars in dim color (~15-22%)
  - Tier 2 (new): biome-specific decoration glyphs with color (~5-10%): wildflowers `*` Yellow (Grassland), ferns `"` Green (Forest), ice shards `'` Cyan (Ice), etc.
  - Tier 3 (new): rare decorations (~1-2%): rare flowers, exotic plants
- Verify visual density looks right
- Commit

### Step 3: Convert generator to semantic obstacles
- Replace `deco(glyph, utf8, color, passable)` calls:
  - Impassable → `natural_obstacle()` or `settlement_prop()` (no visual args)
  - Passable → remove from palette (now handled by floor scatter)
- Remove the `deco()` helper function
- Reduce scatter attempt counts (fewer entries to place)
- Update settlement scatter similarly
- Commit

### Step 4: Save format migration
- Bump `SaveData::version` to 17 in `save_file.h`
- Write path: stop writing glyph/color bytes
- Read path (v3-v16): read and discard 2 bytes; migrate `Debris` → `NaturalObstacle` if impassable, convert to plain Floor if passable
- Add post-load migration pass for passable Debris: clear fixture_id, reset tile to Floor
- Commit

### Step 5: Remove visual fields from FixtureData
- Remove `glyph`, `utf8_glyph`, `color` from `FixtureData` struct in `tilemap.h`
- Remove visual assignments from `make_fixture()` in `tilemap.cpp`
- Remove door glyph updates in `dialog_manager.cpp` (door visual now from RF_Open flag)
- Fix any remaining compile errors
- Commit

### Step 6: Cleanup and verification
- Clean build
- Run game: verify overworld detail zones have varied decorations, boulders look correct per biome, settlements have props, floor scatter is richer
- Verify old saves load correctly (if available)
- Commit any fixups

## Verification

1. **Clean build**: `rm -rf build && cmake -B build -DDEV=ON && cmake --build build`
2. **Visual check**: Run `.worktrees/semantic-rendering/build/astra-dev`
   - Enter a detail zone on the overworld — should see biome-appropriate obstacles (boulders, stumps, crystals)
   - Floor tiles should show richer scatter (colored wildflowers, grass, ferns mixed in)
   - Settlement areas should have props (antennas, benches, lamp posts)
   - Station: functional fixtures (consoles, doors, torches) render correctly with animations
3. **Regression check**: All functional fixtures still work (doors open/close, heal pods, terminals interact)
4. **Save compat**: If old save files exist, verify they load without crashes
