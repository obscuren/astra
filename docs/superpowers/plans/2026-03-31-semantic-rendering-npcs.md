# NPC Semantic Rendering Migration — Implementation Plan

## Context

NPCs currently store `glyph` (char) and `color` (Color) directly on the `Npc` struct, set by builder functions. The renderer reads these fields to draw NPCs. To complete the semantic rendering migration, NPC visuals should be resolved by the theme from semantic data (role, race, disposition), not stored per-instance.

**Complication:** The `NpcRole` enum only has 4 entries (StationKeeper, Merchant, Drifter, Xytomorph) but there are 11+ distinct NPC types built by separate functions, plus 6 civilian race variants. The enum needs expanding.

## Approach

### Expand NpcRole enum

Add all NPC types to `NpcRole`:
- `FoodMerchant`, `Medic`, `Commander`, `ArmsDealer`, `Astronomer`, `Engineer`, `Nova`, `Civilian`

The theme resolves visual from `NpcRole` + `Race` (civilians vary by race).

### Theme resolution

Add `RenderCategory::Npc` handling in `terminal_theme.cpp`. The descriptor carries `type_id = NpcRole`, `seed` for race (civilian variant selection), and flags for disposition (hostile = red tint).

### Save format

Bump v17 → v18. Stop writing glyph/color for NPCs. Old saves: read and discard 2 bytes.

---

## Files to Modify

| File | Change |
|------|--------|
| `include/astra/npc.h` | Expand `NpcRole` enum, remove `glyph`/`color` from `Npc` struct |
| `src/npcs/*.cpp` | Remove glyph/color assignments from all builders, set `NpcRole` instead of string role where missing |
| `src/terminal_theme.cpp` | Add `RenderCategory::Npc` resolution — NpcRole+Race → glyph+color |
| `src/map_renderer.cpp` | Build NPC descriptor, use `wctx.put()` |
| `src/map_editor.cpp` | Same for editor NPC rendering |
| `src/save_file.cpp` | Bump to v18, stop writing NPC glyph/color, backward compat read |
| `include/astra/save_file.h` | `version = 18` |
| `include/astra/render_descriptor.h` | Ensure NpcRole can be forward-declared or included |

## Implementation Steps

### Step 1: Expand NpcRole enum (additive, no breakage)
- Add `FoodMerchant`, `Medic`, `Commander`, `ArmsDealer`, `Astronomer`, `Engineer`, `Nova`, `Civilian` to `NpcRole` in `npc.h`
- Add a `NpcRole npc_role` field to `Npc` struct (alongside existing `role` string for display)
- Update all builders to set `npc.npc_role` appropriately
- Commit

### Step 2: Add NPC theme resolution
- Add `resolve_npc()` in `terminal_theme.cpp` — switches on `NpcRole`, with `Race` for civilian variants
- Wire into `resolve()` for `RenderCategory::Npc`
- Add `npc_glyph(NpcRole, Race)` helper for UI display
- Commit

### Step 3: Migrate NPC rendering in map_renderer and map_editor
- Build `RenderDescriptor` for NPCs with `category = Npc`, `type_id = npc_role`, seed encodes race
- Use `wctx.put()` for NPC rendering
- Animation overrides stay on old DrawContext path
- Commit

### Step 4: Save format migration
- Bump version to 18
- Stop writing NPC glyph/color
- Read path: skip 2 bytes for old saves, reconstruct from npc_role
- Commit

### Step 5: Remove glyph/color from Npc struct
- Remove fields from struct
- Remove assignments from all builders
- Fix any remaining references
- Commit

### Step 6: Verification
- Clean build
- Visual check: all NPC types render with correct glyphs/colors
- Verify old saves load correctly

## Verification

1. `rm -rf build && cmake -B build -DDEV=ON && cmake --build build`
2. Run `.worktrees/semantic-rendering/build/astra-dev`
   - Station: see K (green keeper), M (cyan merchant), etc.
   - Hub NPCs: F (yellow food), D (green medic), C (white commander), etc.
   - Monsters: X (red xytomorph)
   - Civilians: race-appropriate glyphs and colors
3. Save and reload — NPCs retain correct appearance
