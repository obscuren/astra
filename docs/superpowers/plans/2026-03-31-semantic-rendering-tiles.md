# Semantic Rendering: Tiles Migration — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move all tile glyph and color resolution from game code into `terminal_theme.cpp`. `map_renderer.cpp` and `map_editor.cpp` build `RenderDescriptor` structs and use `WorldContext` for tile rendering. No visual changes.

**Architecture:** Game code builds a `RenderDescriptor` per tile with `{category: Tile, type_id: tile_enum, seed: position_seed(x,y), flags: visibility_flags, biome: map_biome}`. `WorldContext::put()` passes this to `Renderer::draw_entity()`. `TerminalRenderer::draw_entity()` calls `resolve()` in `terminal_theme.cpp` which returns the glyph + color. All existing glyph/color functions move into `terminal_theme.cpp`.

**Tech Stack:** C++20, CMake

**Spec:** `docs/superpowers/specs/2026-03-31-semantic-rendering-design.md`

**Branch:** `semantic-rendering` (worktree at `.worktrees/semantic-rendering/`)

**Scope:** Tiles only — fixtures, NPCs, items, player, targeting, and look cursor stay on the old `DrawContext`/glyph path for now. The map renderer will use both `WorldContext` (for tiles) and `DrawContext` (for everything else) during this transitional step.

---

## File Structure

| Action | Path | Responsibility |
|--------|------|----------------|
| Modify | `src/terminal_theme.h` | Add helper function declarations |
| Modify | `src/terminal_theme.cpp` | Full tile resolution logic — all glyph/color functions move here |
| Modify | `src/map_renderer.cpp` | Build `RenderDescriptor` for tiles, use `WorldContext` for tile rendering |
| Modify | `src/map_editor.cpp` | Build `RenderDescriptor` for tiles in editor viewport |
| Modify | `include/astra/tilemap.h` | Remove `tile_glyph()`, `overworld_glyph()`, `dungeon_wall_glyph()`, `dungeon_water_glyph()`, `dungeon_portal_glyph()` |
| Modify | `src/tilemap.cpp` | Remove `biome_colors()` (or keep for non-rendering uses — check first) |
| Modify | `include/astra/render_descriptor.h` | Add tile-specific flags if needed |

---

## Task 1: Populate resolve() with Tile Resolution

This is the largest task — moving all tile visual logic into `terminal_theme.cpp`.

**Files:**
- Modify: `src/terminal_theme.h`
- Modify: `src/terminal_theme.cpp`

- [ ] **Step 1: Read the current terminal_theme files**

Read `src/terminal_theme.h` and `src/terminal_theme.cpp` to understand current state.

- [ ] **Step 2: Add RenderFlag extensions to render_descriptor.h**

In `include/astra/render_descriptor.h`, add a new flag for starfield background and structural wall material encoding. Add after the existing `RenderFlag` enum:

```cpp
    RF_Starfield    = 1 << 6,  // tile is in a starfield zone (Station Empty tiles)
```

Also add a helper to encode structural wall material in the seed field (material is 0-3, seed is 0-255 — we use the top 2 bits for material, bottom 6 for variation):

```cpp
// Encode structural wall material (0-3) into the seed's top 2 bits.
// Bottom 6 bits are position variation.
inline uint8_t encode_wall_seed(uint8_t material, int x, int y) {
    uint8_t base = position_seed(x, y) & 0x3F;  // bottom 6 bits
    return static_cast<uint8_t>((material << 6) | base);
}

inline uint8_t decode_wall_material(uint8_t seed) {
    return seed >> 6;
}
```

- [ ] **Step 3: Add helper declarations to terminal_theme.h**

Add after the existing `resolve()` and `resolve_animation()` declarations, still inside namespace astra:

```cpp
// Internal helpers used by resolve() — exposed in header for testability
struct BiomeColors {
    Color wall;
    Color floor;
    Color water;
    Color remembered;
};

BiomeColors biome_palette(Biome biome);
```

- [ ] **Step 4: Implement full tile resolution in terminal_theme.cpp**

Replace the stub `resolve()` with real logic. The function handles `RenderCategory::Tile` and falls through to the stub for other categories.

```cpp
#include "terminal_theme.h"
#include "astra/render_descriptor.h"
#include "astra/tilemap.h"  // Tile enum

namespace astra {

// ---------- Internal helpers ----------

// Position-varied glyph selection using seed
static const char* select_variant(const char* const* glyphs, int count, uint8_t seed) {
    return glyphs[seed % count];
}

BiomeColors biome_palette(Biome biome) {
    switch (biome) {
        case Biome::Station:   return {Color::White, Color::Default, Color::Blue, Color::Blue};
        case Biome::Rocky:     return {Color::White, Color::DarkGray, Color::Blue, Color::Blue};
        case Biome::Volcanic:  return {Color::Red, static_cast<Color>(52), Color::Red, static_cast<Color>(52)};
        case Biome::Ice:       return {Color::Cyan, Color::White, static_cast<Color>(39), Color::Blue};
        case Biome::Sandy:     return {Color::Yellow, static_cast<Color>(180), Color::Blue, static_cast<Color>(58)};
        case Biome::Aquatic:   return {static_cast<Color>(30), static_cast<Color>(24), Color::Blue, static_cast<Color>(24)};
        case Biome::Fungal:    return {Color::Green, static_cast<Color>(22), Color::Green, static_cast<Color>(22)};
        case Biome::Crystal:   return {Color::BrightMagenta, Color::Magenta, Color::Magenta, static_cast<Color>(54)};
        case Biome::Corroded:  return {static_cast<Color>(142), static_cast<Color>(58), static_cast<Color>(148), static_cast<Color>(58)};
        case Biome::Forest:    return {Color::Green, static_cast<Color>(58), Color::Blue, static_cast<Color>(22)};
        case Biome::Grassland: return {Color::DarkGray, Color::Green, Color::Blue, static_cast<Color>(22)};
        case Biome::Jungle:    return {static_cast<Color>(22), static_cast<Color>(22), static_cast<Color>(30), static_cast<Color>(22)};
    }
    return {Color::White, Color::Default, Color::Blue, Color::Blue};
}

// --- Starfield ---
// Note: the original star_at() used a full 32-bit hash. Our seed is 8 bits
// (0-255), so we need to reproduce the same distribution. We use the seed
// for density (3% threshold) and star type selection from separate bit ranges.
static ResolvedVisual resolve_starfield(uint8_t seed) {
    // 3% of 256 values ≈ 8 values. Use threshold of 8 out of 256.
    if (seed >= 8) return {' ', nullptr, Color::Default, Color::Default};
    // Star type: use the seed value itself for variety (0-7)
    if (seed < 5) return {'.', nullptr, Color::Cyan, Color::Default};
    if (seed < 7) return {'*', nullptr, Color::White, Color::Default};
    return {'+', nullptr, Color::White, Color::Default};
}

// --- Overworld glyphs ---
static ResolvedVisual resolve_overworld_tile(Tile tile, Biome biome, uint8_t seed, uint8_t flags) {
    Color c = Color::White;
    const char* utf8 = nullptr;
    char glyph = ' ';

    // Color by tile type with biome-aware plains
    switch (tile) {
        case Tile::OW_Plains:
            switch (biome) {
                case Biome::Ice:   c = Color::White; break;
                case Biome::Rocky: c = Color::DarkGray; break;
                case Biome::Sandy: c = Color::Yellow; break;
                default:           c = Color::Green; break;
            }
            break;
        case Tile::OW_Mountains:   c = Color::White; break;
        case Tile::OW_Crater:      c = Color::DarkGray; break;
        case Tile::OW_IceField:    c = Color::Cyan; break;
        case Tile::OW_LavaFlow:    c = Color::Red; break;
        case Tile::OW_Desert:      c = Color::Yellow; break;
        case Tile::OW_Fungal:      c = Color::Green; break;
        case Tile::OW_Forest:      c = Color::Green; break;
        case Tile::OW_River:       c = Color::Blue; break;
        case Tile::OW_Lake:        c = Color::Cyan; break;
        case Tile::OW_Swamp:       c = static_cast<Color>(58); break;
        case Tile::OW_CaveEntrance:c = Color::Magenta; break;
        case Tile::OW_Ruins:       c = Color::BrightMagenta; break;
        case Tile::OW_Settlement:  c = Color::Yellow; break;
        case Tile::OW_CrashedShip: c = Color::Cyan; break;
        case Tile::OW_Outpost:     c = Color::Green; break;
        case Tile::OW_Landing:     c = static_cast<Color>(14); break;
        default:                   c = Color::White; break;
    }

    // UTF-8 glyph by tile type with seed-based variation
    switch (tile) {
        case Tile::OW_Mountains: {
            static const char* g[] = {"\xe2\x96\xb2", "\xe2\x88\xa9", "^", "\xce\x93", "\xe2\x96\xb2"};
            utf8 = select_variant(g, 5, seed);
            break;
        }
        case Tile::OW_Forest: {
            static const char* g[] = {"\xe2\x99\xa0", "\xce\xa6", "\xc6\x92"};
            utf8 = select_variant(g, 3, seed);
            break;
        }
        case Tile::OW_Plains: {
            static const char* g[] = {"\xc2\xb7", ".", ","};
            utf8 = select_variant(g, 3, seed);
            break;
        }
        case Tile::OW_Desert: {
            static const char* g[] = {"\xe2\x96\x91", "\xc2\xb7", "."};
            utf8 = select_variant(g, 3, seed);
            break;
        }
        case Tile::OW_Lake:
            utf8 = "\xe2\x89\x88";
            break;
        case Tile::OW_River: {
            static const char* g[] = {"\xe2\x89\x88", "~"};
            utf8 = select_variant(g, 2, seed);
            break;
        }
        case Tile::OW_Swamp: {
            static const char* g[] = {"\xcf\x84", "\"", ","};
            utf8 = select_variant(g, 3, seed);
            break;
        }
        case Tile::OW_Fungal: {
            static const char* g[] = {"\xce\xa6", "\"", "\xcf\x84"};
            utf8 = select_variant(g, 3, seed);
            break;
        }
        case Tile::OW_IceField: {
            static const char* g[] = {"\xe2\x96\x91", "\xc2\xb7", "'"};
            utf8 = select_variant(g, 3, seed);
            break;
        }
        case Tile::OW_LavaFlow: {
            static const char* g[] = {"\xe2\x89\x88", "~"};
            utf8 = select_variant(g, 2, seed);
            break;
        }
        case Tile::OW_Crater: {
            static const char* g[] = {"o", "\xc2\xb0"};
            utf8 = select_variant(g, 2, seed);
            break;
        }
        case Tile::OW_CaveEntrance: {
            static const char* g[] = {"\xe2\x96\xbc", "\xce\x98"};
            utf8 = select_variant(g, 2, seed);
            break;
        }
        case Tile::OW_Ruins: {
            static const char* g[] = {"\xcf\x80", "\xce\xa9", "\xc2\xa7", "\xce\xa3"};
            utf8 = select_variant(g, 4, seed);
            break;
        }
        case Tile::OW_Settlement:  utf8 = "\xe2\x99\xa6"; break;
        case Tile::OW_CrashedShip: {
            static const char* g[] = {"%", "\xc2\xa4"};
            utf8 = select_variant(g, 2, seed);
            break;
        }
        case Tile::OW_Outpost:     utf8 = "+"; break;
        case Tile::OW_Landing:     utf8 = "\xe2\x89\xa1"; break;
        default:                   utf8 = " "; break;
    }

    // Interactable flag overrides color (custom detail zones in editor)
    if (flags & RF_Interactable) c = Color::BrightYellow;

    return {glyph, utf8, c, Color::Default};
}

// --- Dungeon wall glyphs ---
static const char* wall_glyph_for_biome(Biome biome, uint8_t seed) {
    switch (biome) {
        case Biome::Station:
            return "\xe2\x96\x88";  // █
        case Biome::Rocky: {
            static const char* g[] = {"\xe2\x96\x91", "\xe2\x96\x91", "#"};
            return select_variant(g, 3, seed);
        }
        case Biome::Volcanic: {
            static const char* g[] = {"\xe2\x96\x93", "\xe2\x96\x93", "\xe2\x96\x92"};
            return select_variant(g, 3, seed);
        }
        case Biome::Ice: {
            static const char* g[] = {"\xe2\x96\x91", "\xe2\x96\x91", "#"};
            return select_variant(g, 3, seed);
        }
        case Biome::Sandy: {
            static const char* g[] = {"\xe2\x96\x92", "\xe2\x96\x91", "#"};
            return select_variant(g, 3, seed);
        }
        case Biome::Aquatic: {
            static const char* g[] = {"\xe2\x96\x93", "\xe2\x96\x92"};
            return select_variant(g, 2, seed);
        }
        case Biome::Fungal: {
            static const char* g[] = {"\xe2\x96\x93", "\xe2\x96\x92", "#"};
            return select_variant(g, 3, seed);
        }
        case Biome::Crystal: {
            static const char* g[] = {"\xe2\x97\x86", "\xe2\x97\x87"};
            return select_variant(g, 2, seed);
        }
        case Biome::Corroded: {
            static const char* g[] = {"\xe2\x96\x91", "#", "\xe2\x96\x92"};
            return select_variant(g, 3, seed);
        }
        case Biome::Forest: {
            static const char* g[] = {"\xe2\x96\x93", "\xe2\x96\x92", "#"};
            return select_variant(g, 3, seed);
        }
        case Biome::Grassland: {
            static const char* g[] = {"\xe2\x96\x91", "\xe2\x96\x91", "#"};
            return select_variant(g, 3, seed);
        }
        case Biome::Jungle: {
            static const char* g[] = {"\xe2\x96\x93", "\xe2\x96\x93", "\xe2\x96\x92"};
            return select_variant(g, 3, seed);
        }
        default:
            return "#";
    }
}

// --- Dungeon water glyphs ---
static const char* water_glyph(uint8_t seed) {
    static const char* g[] = {"\xe2\x89\x88", "\xe2\x89\x88", "~"};
    return select_variant(g, 3, seed);
}

// --- Floor scatter ---
static ResolvedVisual resolve_floor(Biome biome, uint8_t seed, bool remembered) {
    auto bc = biome_palette(biome);
    Color c = remembered ? bc.remembered : bc.floor;

    if (biome == Biome::Station) return {'.', nullptr, c, Color::Default};

    int roll = static_cast<int>(seed % 100);
    struct ScatterSet { int threshold; const char* glyphs; int count; };
    ScatterSet s;
    switch (biome) {
        case Biome::Rocky:     s = {15, ",:`",  3}; break;
        case Biome::Volcanic:  s = {20, ",';" , 3}; break;
        case Biome::Ice:       s = {12, "'`,",  3}; break;
        case Biome::Sandy:     s = {20, ",`:",  3}; break;
        case Biome::Aquatic:   s = {10, ",:",   2}; break;
        case Biome::Fungal:    s = {18, "\",'", 3}; break;
        case Biome::Crystal:   s = {15, "*'`",  3}; break;
        case Biome::Corroded:  s = {20, ",:;",  3}; break;
        case Biome::Forest:    s = {18, "\",'", 3}; break;
        case Biome::Grassland: s = {15, ",`.",  3}; break;
        case Biome::Jungle:    s = {22, "\",'", 3}; break;
        default: return {'.', nullptr, c, Color::Default};
    }
    // Use different bits of seed for glyph selection vs threshold check
    char g = (roll >= s.threshold) ? '.' : s.glyphs[(seed >> 4) % s.count];
    return {g, nullptr, c, Color::Default};
}

// --- Structural wall ---
static ResolvedVisual resolve_structural_wall(uint8_t seed, bool remembered) {
    uint8_t mat = decode_wall_material(seed);
    const char* utf8 = nullptr;
    Color c = Color::White;

    switch (mat) {
        case 1:  utf8 = "\xe2\x96\x93"; c = Color::DarkGray; break;    // ▓ concrete
        case 2:  utf8 = "\xe2\x96\x92"; c = static_cast<Color>(137); break; // ▒ wood
        case 3:  utf8 = "\xe2\x96\x91"; c = static_cast<Color>(142); break; // ░ salvage
        default: utf8 = "\xe2\x96\x88"; c = Color::White; break;       // █ metal
    }

    if (remembered) c = Color::DarkGray;
    return {'#', utf8, c, Color::Default};
}

// --- Main resolve() ---
ResolvedVisual resolve(const RenderDescriptor& desc) {
    if (desc.category != RenderCategory::Tile) {
        // Stub for non-tile categories — will be filled in by later migration steps
        return {'?', nullptr, Color::Magenta, Color::Default};
    }

    auto tile = static_cast<Tile>(desc.type_id);
    bool remembered = (desc.flags & RF_Remembered) != 0;

    // Starfield (Station empty tiles)
    if (desc.flags & RF_Starfield) {
        return resolve_starfield(desc.seed);
    }

    // Empty tiles
    if (tile == Tile::Empty) {
        return {' ', nullptr, Color::Default, Color::Default};
    }

    // Overworld tiles
    if (tile >= Tile::OW_Plains) {
        return resolve_overworld_tile(tile, desc.biome, desc.seed, desc.flags);
    }

    // Dungeon/Station tiles
    auto bc = biome_palette(desc.biome);

    switch (tile) {
        case Tile::StructuralWall:
            return resolve_structural_wall(desc.seed, remembered);

        case Tile::Wall: {
            const char* utf8 = wall_glyph_for_biome(desc.biome, desc.seed);
            Color c = remembered ? bc.remembered : bc.wall;
            return {'#', utf8, c, Color::Default};
        }

        case Tile::Portal: {
            const char* utf8 = "\xe2\x96\xbc";  // ▼
            Color c = remembered ? bc.remembered : Color::Magenta;
            return {'>', utf8, c, Color::Default};
        }

        case Tile::Water: {
            const char* utf8 = water_glyph(desc.seed);
            Color c = remembered ? bc.remembered : bc.water;
            return {'~', utf8, c, Color::Default};
        }

        case Tile::Ice: {
            Color c = remembered ? bc.remembered : Color::Cyan;
            return {'~', nullptr, c, Color::Default};
        }

        case Tile::IndoorFloor: {
            const char* utf8 = "\xe2\x96\xaa";  // ▪
            Color c = remembered ? bc.remembered : static_cast<Color>(137);
            return {'.', utf8, c, Color::Default};
        }

        case Tile::Floor:
            return resolve_floor(desc.biome, desc.seed, remembered);

        default:
            return {' ', nullptr, Color::Default, Color::Default};
    }
}

ResolvedVisual resolve_animation(AnimationType type, int frame_index) {
    (void)type;
    (void)frame_index;
    return {'*', nullptr, Color::Magenta, Color::Default};
}

} // namespace astra
```

- [ ] **Step 5: Build and verify theme compiles**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds. Nothing calls the new resolve logic yet.

- [ ] **Step 6: Commit**

```bash
git add src/terminal_theme.h src/terminal_theme.cpp include/astra/render_descriptor.h
git commit -m "Populate terminal theme with full tile resolution logic"
```

---

## Task 2: Migrate map_renderer.cpp Tile Rendering to WorldContext

This task changes `render_map()` to build `RenderDescriptor` for tiles and use `WorldContext`. Non-tile rendering (items, NPCs, player, targeting, look cursor) stays on the old `DrawContext` path.

**Files:**
- Modify: `src/map_renderer.cpp`

- [ ] **Step 1: Read current map_renderer.cpp**

Read the full file to understand the current rendering flow.

- [ ] **Step 2: Add includes and create both contexts**

At the top of `map_renderer.cpp`, add:

```cpp
#include "astra/world_context.h"
#include "astra/render_descriptor.h"
```

In `render_map()`, alongside the existing `DrawContext ctx(rc.renderer, rc.map_rect)`, create a `WorldContext`:

```cpp
WorldContext wctx(rc.renderer, rc.map_rect);
DrawContext ctx(rc.renderer, rc.map_rect);  // kept for non-tile rendering (NPCs, items, etc.)
```

- [ ] **Step 3: Replace tile rendering with RenderDescriptor + WorldContext**

Replace the tile rendering sections (starfield, overworld, dungeon visible, dungeon remembered) with descriptor-based rendering. The non-tile rendering (ground items, NPCs, player, targeting line, look cursor) stays using `ctx.put()` with raw glyphs.

**Starfield** (Empty tiles in Station biome):
```cpp
if (rc.world.map().biome() == Biome::Station && tile_at == Tile::Empty) {
    RenderDescriptor desc;
    desc.category = RenderCategory::Tile;
    desc.type_id = static_cast<uint16_t>(Tile::Empty);
    desc.seed = position_seed(mx, my);
    desc.flags = RF_Starfield;
    desc.biome = Biome::Station;
    wctx.put(sx, sy, desc);
    continue;  // or skip based on resolve returning space
}
```

**Overworld tiles:**
```cpp
if (rc.world.map().map_type() == MapType::Overworld) {
    RenderDescriptor desc;
    desc.category = RenderCategory::Tile;
    desc.type_id = static_cast<uint16_t>(tile_at);
    desc.seed = position_seed(mx, my);
    desc.biome = rc.world.map().biome();
    // Glyph override for stamps/quest markers
    uint8_t gov = rc.world.map().glyph_override(mx, my);
    if (gov == SG_QuestMarker) desc.flags |= RF_Interactable;
    // Note: non-quest glyph overrides (stamps) need special handling — see Step 4

    // Animation override check stays on old path for now (animations not migrated yet)
    if (rc.animations) {
        if (auto* frame = rc.animations->query(mx, my)) {
            // Still use old DrawContext for animation overrides until animation migration
            if (frame->utf8) ctx.put(sx, sy, frame->utf8, frame->color);
            else ctx.put(sx, sy, frame->glyph, frame->color);
            continue;
        }
    }

    wctx.put(sx, sy, desc);
    continue;
}
```

**Dungeon visible tiles:**
Build a `RenderDescriptor` for each visible tile. For `Tile::Fixture`, continue using old path (fixture migration is a later plan). For all other tiles, use `wctx.put()`.

```cpp
if (v == Visibility::Visible) {
    // Fixtures stay on old path (not migrated yet)
    if (tile_at == Tile::Fixture) {
        // ... existing fixture rendering code unchanged, using ctx ...
    } else {
        RenderDescriptor desc;
        desc.category = RenderCategory::Tile;
        desc.type_id = static_cast<uint16_t>(tile_at);
        desc.biome = biome;
        desc.flags = RF_Lit;

        if (tile_at == Tile::StructuralWall) {
            uint8_t mat = rc.world.map().glyph_override(mx, my);
            desc.seed = encode_wall_seed(mat, mx, my);
        } else {
            desc.seed = position_seed(mx, my);
        }

        // Animation override check
        if (rc.animations) {
            if (auto* frame = rc.animations->query(mx, my)) {
                if (frame->utf8) ctx.put(sx, sy, frame->utf8, frame->color);
                else ctx.put(sx, sy, frame->glyph, frame->color);
                continue;
            }
        }

        wctx.put(sx, sy, desc);
    }
}
```

**Remembered tiles:**
```cpp
else {
    // Remembered — same descriptor but with RF_Remembered flag
    if (tile_at == Tile::Fixture) {
        // ... existing remembered fixture rendering, using ctx ...
    } else {
        RenderDescriptor desc;
        desc.category = RenderCategory::Tile;
        desc.type_id = static_cast<uint16_t>(tile_at);
        desc.biome = biome;
        desc.flags = RF_Remembered;

        if (tile_at == Tile::StructuralWall) {
            uint8_t mat = rc.world.map().glyph_override(mx, my);
            desc.seed = encode_wall_seed(mat, mx, my);
        } else {
            desc.seed = position_seed(mx, my);
        }

        wctx.put(sx, sy, desc);
    }
}
```

- [ ] **Step 4: Handle overworld glyph overrides (stamps)**

Overworld maps have `glyph_override()` for stamp glyphs and quest markers. Quest markers map to `RF_Interactable`. Non-quest stamp glyphs (from `stamp_glyph()`) are a special case — they're custom per-cell overrides that don't fit the descriptor model cleanly.

For now, stamp glyph overrides (non-quest) stay on the old `DrawContext` path:

```cpp
uint8_t gov = rc.world.map().glyph_override(mx, my);
if (gov != 0 && gov != SG_QuestMarker) {
    // Custom stamp glyph — stays on old path
    Color c = overworld_tile_color(tile_at, rc.world.map().biome());
    ctx.put(sx, sy, stamp_glyph(gov), c);
    continue;
}
```

Wait — `overworld_tile_color()` is being removed. We need to keep it temporarily for this edge case, OR resolve stamps through the descriptor by adding a glyph override mechanism. The simpler approach: keep `overworld_tile_color()` as a static function in `map_renderer.cpp` for the stamp edge case until stamps are migrated. Move it from its current position but keep it as a local static.

- [ ] **Step 5: Remove old tile glyph/color imports**

Remove or comment out the includes/calls to `tile_glyph()`, `overworld_glyph()`, `dungeon_wall_glyph()`, `dungeon_water_glyph()`, `dungeon_portal_glyph()`, `biome_colors()` from `map_renderer.cpp` for tile rendering paths. Keep `floor_scatter()` and `star_at()` as local statics until verify they're no longer called.

Remove the local `floor_scatter()` and `star_at()` functions since their logic is now in `terminal_theme.cpp`.

Keep `overworld_tile_color()` as a local static for the stamp glyph edge case (Step 4).

Keep `chebyshev_dist()` — it's used by targeting, not tile rendering.

- [ ] **Step 6: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -10`
Expected: Build succeeds.

- [ ] **Step 7: Run the game and visually verify**

Run: `./build/astra-dev`
Expected: Tiles render identically to before. Check:
- Overworld: terrain variety, biome colors, quest markers
- Dungeon: wall glyphs vary by biome, floor scatter, water glyphs
- Station: starfield backdrop, structural walls
- Remembered tiles: dimmed colors
- Fixtures still render (old path)
- NPCs, items, player still render (old path)

- [ ] **Step 8: Commit**

```bash
git add src/map_renderer.cpp
git commit -m "Migrate map_renderer tile rendering to WorldContext + RenderDescriptor"
```

---

## Task 3: Migrate map_editor.cpp Tile Rendering

**Files:**
- Modify: `src/map_editor.cpp`

- [ ] **Step 1: Read draw_viewport() in map_editor.cpp**

Read the `draw_viewport()` function (around line 1058).

- [ ] **Step 2: Add includes**

At the top of `map_editor.cpp`, add:

```cpp
#include "astra/world_context.h"
#include "astra/render_descriptor.h"
```

- [ ] **Step 3: Create WorldContext for viewport**

In `draw_viewport()`, the parameter is currently `DrawContext& ctx`. Change the function to create a `WorldContext` internally from the renderer:

Since `draw_viewport` receives a `DrawContext&`, and we need a `WorldContext` for tiles but still need `DrawContext` for the cursor overlay, create the `WorldContext` from the renderer and bounds:

```cpp
void MapEditor::draw_viewport(DrawContext& ctx) {
    WorldContext wctx(renderer_, ctx.bounds());
    // ... rest of function
```

Wait — `draw_viewport` gets a `DrawContext` parameter, not direct renderer access. But `MapEditor` has `renderer_` as a member. So we can create a `WorldContext` directly. But we need the bounds from the DrawContext. Since `DrawContext` (now `UIContext`) exposes `bounds()`, this works.

- [ ] **Step 4: Replace tile rendering with descriptors**

Replace the overworld and dungeon tile rendering in `draw_viewport()` with `RenderDescriptor` + `wctx.put()`. The cursor rendering stays on `ctx.put()`.

**Overworld:**
```cpp
if (is_ow) {
    RenderDescriptor desc;
    desc.category = RenderCategory::Tile;
    desc.type_id = static_cast<uint16_t>(t);
    desc.seed = position_seed(mx, my);
    desc.biome = map.biome();
    if (map.custom_detail(mx, my)) desc.flags |= RF_Interactable;
    wctx.put(sx, sy, desc);
}
```

**Dungeon (non-fixture tiles):**
```cpp
else {
    if (t == Tile::Fixture) {
        // Fixtures stay on old path
        // ... existing fixture code using ctx ...
    } else {
        RenderDescriptor desc;
        desc.category = RenderCategory::Tile;
        desc.type_id = static_cast<uint16_t>(t);
        desc.biome = map.biome();
        desc.flags = RF_Lit;

        if (t == Tile::StructuralWall) {
            // Editor doesn't use glyph_override for material — use default (0)
            desc.seed = encode_wall_seed(0, mx, my);
        } else {
            desc.seed = position_seed(mx, my);
        }

        wctx.put(sx, sy, desc);
    }
}
```

Cursor rendering stays on `ctx.put()` (UI overlay, not game-world tile).

- [ ] **Step 5: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds.

- [ ] **Step 6: Commit**

```bash
git add src/map_editor.cpp
git commit -m "Migrate map editor viewport to WorldContext + RenderDescriptor"
```

---

## Task 4: Remove Old Tile Glyph/Color Functions

Now that both `map_renderer.cpp` and `map_editor.cpp` use `terminal_theme.cpp` for tile resolution, remove the old functions.

**Files:**
- Modify: `include/astra/tilemap.h`
- Modify: `src/tilemap.cpp`

- [ ] **Step 1: Check for remaining callers**

Search the entire codebase for calls to:
- `tile_glyph(`
- `overworld_glyph(`
- `dungeon_wall_glyph(`
- `dungeon_water_glyph(`
- `dungeon_portal_glyph(`
- `biome_colors(`
- `floor_scatter(`
- `star_at(`
- `overworld_tile_color(`

If any callers remain outside `terminal_theme.cpp`, they need to be migrated or the function needs to stay.

- [ ] **Step 2: Remove functions from tilemap.h**

Remove these inline functions from `include/astra/tilemap.h`:
- `tile_glyph()` (lines ~45-73)
- `overworld_glyph()` (lines ~77-195)
- `dungeon_wall_glyph()` (lines ~234-332)
- `dungeon_water_glyph()` (lines ~335-348)
- `dungeon_portal_glyph()` (lines ~351-353)

Keep the `Tile` enum, `Biome` enum, `FixtureType` enum, `FixtureData` struct, and all non-visual functions.

- [ ] **Step 3: Remove biome_colors() from tilemap.cpp**

Remove `biome_colors()` from `src/tilemap.cpp` and its declaration/`BiomeColors` struct from `tilemap.h`. The equivalent now lives in `terminal_theme.cpp` as `biome_palette()`.

If `BiomeColors` is used elsewhere (check first), keep the struct definition but remove the function.

- [ ] **Step 4: Remove local functions from map_renderer.cpp**

Remove from `src/map_renderer.cpp`:
- `star_at()` (if no longer called)
- `floor_scatter()` (if no longer called)
- `overworld_tile_color()` (if no longer called — may still be needed for stamp edge case)

- [ ] **Step 5: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -10`
Expected: Build succeeds. Any remaining callers will cause compile errors — fix them.

- [ ] **Step 6: Run the game and visually verify**

Run: `./build/astra-dev`
Expected: Everything renders identically.

- [ ] **Step 7: Commit**

```bash
git add include/astra/tilemap.h src/tilemap.cpp src/map_renderer.cpp
git commit -m "Remove old tile glyph/color functions — now in terminal_theme"
```

---

## Task 5: Final Verification

**Files:** None (verification only)

- [ ] **Step 1: Clean build**

Run: `rm -rf build && cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -10`
Expected: Clean build, no warnings related to our changes.

- [ ] **Step 2: Run and verify**

Run: `./build/astra-dev`
Verify:
- Station: starfield, structural walls (metal/concrete/wood/salvage variants), floors
- Dungeon: biome-specific wall glyphs, floor scatter, water, portals, ice
- Overworld: all terrain types with UTF-8 variants, biome-aware colors, quest markers
- Remembered tiles: dimmed colors
- Map editor: tiles render correctly in both overworld and dungeon modes
- Fixtures, NPCs, items, player, targeting: all still work (old path)

- [ ] **Step 3: Commit if any fixups needed**

---

## Summary

After this plan:
- All tile visual resolution lives in `terminal_theme.cpp`
- `map_renderer.cpp` builds `RenderDescriptor` for tiles, uses `WorldContext`
- `map_editor.cpp` does the same for its viewport
- Old glyph/color functions removed from `tilemap.h/cpp` and `map_renderer.cpp`
- Fixtures, NPCs, items, player still on old `DrawContext` path (next plans)
- Zero visual changes

**Next plan:** Fixtures migration — move fixture glyph/color out of `make_fixture()` into `terminal_theme.cpp`.
