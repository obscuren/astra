# Semantic Rendering: Items Migration — Implementation Plan

**Goal:** Move item glyph/color resolution from `Item` struct into `terminal_theme.cpp`. Items identified by `uint16_t item_def_id` — a numeric index into a definition registry. Renderer resolves visual from the ID.

**Branch:** `semantic-rendering` (worktree at `.worktrees/semantic-rendering/`)

---

## Approach

Each item definition registers with a central registry and gets a `uint16_t item_def_id`. The `Item` struct carries this ID instead of glyph/color. The theme has a lookup table indexed by the same ID. New items get the next number — no enum to maintain. Random/generated items will get their own ID range in the future.

---

## Steps

### Step 1: Create item definition registry (additive)
- Add `item_def_id` field (`uint16_t`) to `Item` struct in `item.h`
- Create a simple registry: each `build_*` function gets a fixed numeric ID (0-39ish)
- Define IDs as constants in a header (e.g. `item_ids.h`) or at the top of `item_defs.cpp`
- Update all builders to set `item.item_def_id`
- Update `item_gen.cpp` to set `item_def_id` for generated items
- Commit

### Step 2: Add Item theme resolution
- Add `resolve_item()` in `terminal_theme.cpp` — `item_def_id` → glyph+color lookup table
- Wire into `resolve()` for `RenderCategory::Item`
- Add `item_visual(uint16_t item_def_id)` helper for UI screens
- Commit

### Step 3: Migrate item rendering
- Ground items in `map_renderer.cpp` → RenderDescriptor + WorldContext
- Map editor item rendering → same
- UI screens (inventory, trade, character screen, repair bench) → use theme helper
- Replace `rarity_color()` calls with theme resolution where appropriate
- Commit

### Step 4: Save format + field removal
- Bump v18 → v19, write `item_def_id` instead of glyph/color
- Backward compat: old saves read+discard glyph/color, reconstruct item_def_id from item name
- Remove `glyph`/`color` from Item struct
- Remove assignments from all builders
- Commit

### Step 5: Verification
- Clean build
- Visual: ground items, inventory, trade, character screen all correct
- Save/load test

---

## Files to Modify

| File | Change |
|------|--------|
| `include/astra/item.h` | Add `uint16_t item_def_id` field, eventually remove `glyph`/`color` |
| `src/item_defs.cpp` | Assign fixed numeric IDs to all builders |
| `src/item_gen.cpp` | Set `item_def_id` for generated items |
| `src/terminal_theme.cpp` | Add `resolve_item()` — ID → glyph+color lookup table |
| `src/terminal_theme.h` | Add `item_visual(uint16_t)` helper |
| `src/map_renderer.cpp` | Ground item rendering → descriptor |
| `src/map_editor.cpp` | Editor item rendering |
| `src/ui.cpp` | Inventory display → theme helper |
| `src/trade_window.cpp` | Trade display → theme helper |
| `src/character_screen.cpp` | Character screen → theme helper |
| `src/repair_bench.cpp` | Repair display → theme helper |
| `src/game_rendering.cpp` | Ground item look → theme helper |
| `src/tinkering.cpp` | Crafting result → set item_def_id |
| `src/save_file.cpp` | Bump v19, serialize item_def_id |
| `include/astra/save_file.h` | `version = 19` |
