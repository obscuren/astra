# Smoke Clouds & Ground-Effect Framework

**Date:** 2026-04-29
**Status:** Design

## Goals

1. Replace the Smoke Grenade's placeholder Slow effect with a real vision-blocking cloud that lingers on the map for several world ticks.
2. Introduce a generalized **ground effect** framework — a single struct + enum + dispatch shape that can later support acid pools, ice patches, fire pools, etc., without further save schema work.
3. Integrate ground-effect opacity into the FOV raycaster cleanly, without coupling `TileMap` to game-state registries.
4. Persist ground effects across save / load.

## Non-Goals

- AI awareness of smoke. NPCs in v1 lose sight through smoke the same way the player does and do not pathfind around it.
- Light interaction with day/night cycle.
- Animated radial growth from impact (cloud is a single stamp on detonation, then decays in place).
- Acid / ice / fire ground effects — framework supports them, but only `Smoke` is wired in this spec.
- Save migration. v50 → v51 bump rejects v50 saves per the project's "no backcompat pre-ship" rule.
- Flashbang's true Stun (separate roadmap entry).

## High-Level Architecture

```
detonate_grenade(GrenadeKind::Smoke, x, y)
        │
        ▼
stamp_ground_effect(game, GroundEffectKind::Smoke, x, y)
        │
        │ reads GroundEffectDef[Smoke] = { radius:2, center_ttl:12, ring_falloff:3, blocks_vision:true }
        │ Bresenham wall-LOS test from (x,y) per tile in 5×5 square
        │ ttl = center_ttl − ring * ring_falloff (clamped ≥ 1)
        │ upsert into WorldManager.ground_effects_ (max(new_ttl, existing_ttl))
        ▼
WorldManager.ground_effects_   ◀── new vector<GroundEffect>
        │
        ▼ each world tick
tick_ground_effects(game) — TTL--, erase entries with ttl ≤ 0
        │
        ▼ FOV computation
auto smoke = opaque_ground_effect_tiles(game);
OpacityProbe probe{ &world.map(), &smoke };
compute_fov(probe, vis, player.x, player.y, radius);
        │
        ▼ render pass (between fixtures and traps)
for each GroundEffect ge in world.ground_effects():
    glyph = ground_effect_glyph(ge.kind, ge.ttl, def.center_ttl, world_tick & 1)
    ctx.draw(ge.x, ge.y, glyph, ground_effect_color(ge.kind))
```

**New module: `astra/ground_effect.h` + `src/ground_effect.cpp`** — owns gameplay-side data and pipeline (`GroundEffect`, `GroundEffectKind`, `GroundEffectDef`, `stamp_ground_effect`, `tick_ground_effects`, `opaque_ground_effect_tiles`).

**Render-side helper** in the existing map-render code (next to the trap render pass) — owns glyph and color tables. Pure presentation; gameplay does not import them.

**FOV refactor** — `compute_fov` and `compute_fov_lit` accept an `OpacityProbe` (TileMap pointer + optional opaque-overrides hash-set) instead of a raw `TileMap&`. The four `map.opaque(x, y)` call sites in `src/fov.cpp` become `probe.opaque(x, y)`.

`MapState` gains `std::vector<GroundEffect> ground_effects;` as a v51 tail-write.

## Section 1 — Data model

### 1.1 `astra/ground_effect.h`

```cpp
#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace astra {

class Game;

enum class GroundEffectKind : uint8_t {
    Smoke = 0,
    // Reserved for future kinds: Acid, Ice, Fire.
    // Adding a kind = enum value + GroundEffectDef table row + render entry +
    // (optional) per-step gameplay hook. No save schema change required.
};

struct GroundEffect {
    GroundEffectKind kind = GroundEffectKind::Smoke;
    int x = 0;
    int y = 0;
    int ttl = 0;          // ticks remaining; tile is erased when ttl <= 0
    uint16_t origin_id = 0; // optional grouping: which detonation produced this
};

struct GroundEffectDef {
    int  radius;          // Chebyshev half-width (5×5 → radius=2)
    int  center_ttl;      // initial TTL at impact tile
    int  ring_falloff;    // TTL subtracted per Chebyshev ring
    bool blocks_vision;   // smoke=true; future ice=false
};

const GroundEffectDef& ground_effect_def_for(GroundEffectKind k);

// Stamp a new ground-effect patch at (ix, iy). Walks a (2*radius+1)² square,
// keeps only tiles whose Bresenham line-of-sight from (ix, iy) is unblocked
// by walls / StructuralWall / opaque fixtures (impact tile itself is always
// kept). Per-tile TTL = max(1, center_ttl − ring * ring_falloff). On overlap
// with an existing ground-effect entry of the same kind on that tile, the
// new TTL is max(new, existing); no stacking.
void stamp_ground_effect(Game& game, GroundEffectKind kind, int x, int y);

// One pass per world tick — decrement TTL on every entry, erase expired.
void tick_ground_effects(Game& game);

// Build a hash-set of packed (x, y) coords for tiles whose active ground
// effect has blocks_vision=true. Used by FOV to extend opacity. Key encoding:
// (uint64_t(uint32_t(x)) << 32) | uint32_t(y).
std::unordered_set<uint64_t> opaque_ground_effect_tiles(const Game& game);

} // namespace astra
```

### 1.2 `GroundEffectDef` table (in `ground_effect.cpp`)

```cpp
namespace {
constexpr GroundEffectDef kDefs[] = {
    /* Smoke */ { /*radius*/2, /*center_ttl*/12, /*ring_falloff*/3, /*blocks_vision*/true },
};
} // namespace
```

Initial smoke profile produces:
- Center tile (ring 0): TTL 12.
- Ring 1 (8 tiles): TTL 9.
- Ring 2 (16 tiles): TTL 6.

Outer ring expires after 6 world ticks; the patch visibly shrinks for the remaining 6 ticks before the center fades.

### 1.3 Render-side helpers

Live next to the existing trap render pass (in `map_renderer.cpp` or a sibling file — match the pattern already in place). Not exposed in `ground_effect.h`:

```cpp
char ground_effect_glyph(GroundEffectKind kind, int ttl, int center_ttl, int phase);
int  ground_effect_color(GroundEffectKind kind);
```

Smoke glyph mapping (TTL bucket × phase):
- TTL ≥ 2/3 of `center_ttl` (≥ 8): phase 0 → `▓`, phase 1 → `█`
- TTL ≥ 1/3 of `center_ttl` (4..7): phase 0 → `▒`, phase 1 → `▓`
- TTL < 1/3 of `center_ttl` (1..3): phase 0 → `░`, phase 1 → `▒`

Color: `Color::DarkGray`. `phase = world_tick() & 1` so the cloud churns once per world tick. Animation freezes when the world is paused, matching every other world-tick-driven visual.

### 1.4 `WorldManager` additions

`include/astra/world_manager.h`:

```cpp
#include "astra/ground_effect.h"
...
class WorldManager {
public:
    ...
    std::vector<GroundEffect>& ground_effects() { return ground_effects_; }
    const std::vector<GroundEffect>& ground_effects() const { return ground_effects_; }
private:
    ...
    std::vector<GroundEffect> ground_effects_;
};
```

And in `LocationState` (the per-location cache structure in the same header):

```cpp
struct LocationState {
    ...
    std::vector<GroundEffect> ground_effects;
};
```

### 1.5 `MapState` additions (save schema)

`include/astra/save_file.h`:

```cpp
inline constexpr uint32_t SAVE_FILE_VERSION = 51;   // v51: ground_effects (smoke clouds)
...
struct MapState {
    ...
    std::vector<NoiseEvent> noise_events;    // v50
    std::vector<GroundEffect> ground_effects; // v51
    ...
};
```

`src/save_file.cpp` write path (after the noise-events tail-write):

```cpp
// v51: ground effects
w.write_u32(static_cast<uint32_t>(ms.ground_effects.size()));
for (const auto& ge : ms.ground_effects) {
    w.write_u8(static_cast<uint8_t>(ge.kind));
    w.write_i32(ge.x);
    w.write_i32(ge.y);
    w.write_i32(ge.ttl);
    w.write_u16(ge.origin_id);
}
```

Read path — symmetric, after the noise-events read.

## Section 2 — FOV integration

### 2.1 `OpacityProbe`

`include/astra/fov.h`:

```cpp
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace astra {

class TileMap;
class VisibilityMap;

struct OpacityProbe {
    const TileMap* map = nullptr;
    const std::unordered_set<uint64_t>* extra_opaque = nullptr;

    bool opaque(int x, int y) const;  // map->opaque(x,y) || (extra_opaque && contains(x,y))
};

void compute_fov(const OpacityProbe& probe, VisibilityMap& vis,
                 int origin_x, int origin_y, int radius);

void compute_fov_lit(const OpacityProbe& probe, VisibilityMap& vis,
                     int player_x, int player_y,
                     const std::vector<LightSource>& lights);

} // namespace astra
```

### 2.2 `src/fov.cpp` refactor

The four `map.opaque(map_x, map_y)` calls (lines 55, 62, 129, 136) become `probe.opaque(map_x, map_y)`. `cast_light` and `cast_light_lit` take `const OpacityProbe& probe` instead of `const TileMap& map`.

`OpacityProbe::opaque` is implemented in `fov.cpp`:

```cpp
bool OpacityProbe::opaque(int x, int y) const {
    if (map && map->opaque(x, y)) return true;
    if (extra_opaque) {
        uint64_t key = (uint64_t(uint32_t(x)) << 32) | uint32_t(y);
        if (extra_opaque->count(key)) return true;
    }
    return false;
}
```

### 2.3 Call-site changes

There is exactly one direct caller of `compute_fov` / `compute_fov_lit`: `Game::recompute_fov()` in `src/game_world.cpp` (around line 2070). All gameplay code that needs the FOV recomputed funnels through `recompute_fov()`, so the refactor is a single edit:

```cpp
void Game::recompute_fov() {
    ...
    auto smoke = opaque_ground_effect_tiles(*this);
    OpacityProbe probe{ &world_.map(), &smoke };
    compute_fov(probe, world_.visibility(), player_.x, player_.y, radius);
    ...
    if (/* lights present */) {
        compute_fov_lit(probe, world_.visibility(), player_.x, player_.y, lights);
    }
}
```

The `OpacityProbe` is built once and reused for both `compute_fov` and `compute_fov_lit`. The old `compute_fov(const TileMap&, ...)` and `compute_fov_lit(const TileMap&, ...)` overloads are removed in favor of the single probe-based signature.

### 2.4 Vision behavior consequences (intentional, no extra logic needed)

The standard recursive shadowcaster gives the desired smoke behavior for free, because each octant's `cast_light` marks a tile visible *before* testing its opacity for the next iteration:

- **Player inside smoke (center of 5×5):** origin always visible, the 8 immediate neighbors get marked visible (they are smoke too), then the next iteration sees them as opaque and halts. Effective vision = the player tile + 1 ring. The player can see the wisps at their feet but nothing past them.
- **Player on the border of a smoke patch:** asymmetric — clear directions get a normal FOV ring, smoke directions are blocked at the next step.
- **Player outside smoke:** smoke tiles themselves are visible (marked before opacity test). Tiles behind smoke are not visible. Standard wall behavior.

## Section 3 — Stamping algorithm

```
stamp_ground_effect(game, kind, ix, iy):
    def = ground_effect_def_for(kind)
    auto& effects = game.world().ground_effects()
    for dy in -def.radius..+def.radius:
      for dx in -def.radius..+def.radius:
        tx, ty = ix+dx, iy+dy
        if !map.in_bounds(tx, ty): continue
        if !(dx == 0 && dy == 0) && !line_of_sight_walls_only(map, ix, iy, tx, ty):
            continue
        ring = max(|dx|, |dy|)
        ttl  = max(1, def.center_ttl - ring * def.ring_falloff)
        upsert_ground_effect(effects, kind, tx, ty, ttl)
```

`upsert_ground_effect`: if an entry of the same `kind` already exists at `(tx, ty)`, set its TTL to `max(existing.ttl, ttl)`. Otherwise push a new entry. Linear scan of `effects` is fine (tens of entries, not thousands).

`line_of_sight_walls_only`: private helper in `ground_effect.cpp`. Standard Bresenham from `(ix, iy)` to `(tx, ty)`. Returns false if any *intermediate* tile (excluding endpoints) is opaque per `TileMap::opaque` (which already covers Wall, StructuralWall, opaque fixtures). The endpoints themselves do not block.

## Section 4 — Tick & expiration

`tick_ground_effects(game)`:

```cpp
auto& effects = game.world().ground_effects();
for (auto& ge : effects) ge.ttl -= 1;
effects.erase(
    std::remove_if(effects.begin(), effects.end(),
                   [](const GroundEffect& ge) { return ge.ttl <= 0; }),
    effects.end());
```

Wired into `game_world.cpp::advance_world_tick`, immediately after `tick_noise_events(*this);`:

```cpp
tick_noise_events(*this);
tick_ground_effects(*this);   // NEW
```

## Section 5 — Detonation rewiring

`src/grenade.cpp`:

1. Update `kGrenadeDefs[Smoke]` to `{ /*damage*/0, /*burst_radius*/2, /*status*/0, /*status_duration*/0, /*status_tick_damage*/0 }`. The 5×5 telegraph reticule now matches the actual cloud footprint (WYSIWYG).

2. In `detonate_grenade`, branch on `kind == GrenadeKind::Smoke` *before* the apply-to-entities loop:

   ```cpp
   if (kind == GrenadeKind::Smoke) {
       game.log("The smoke grenade pops — a thick cloud billows out.");
       stamp_ground_effect(game, GroundEffectKind::Smoke, x, y);
       return;
   }
   // existing damage + status loop for other kinds...
   ```

3. Remove the `EffectId::Slow` row from the smoke entry. `short_status` does not need to handle Smoke specifically.

## Section 6 — Save / load

`SAVE_FILE_VERSION` bumps to `51`. v50 saves are rejected at the existing `h.version != SAVE_FILE_VERSION` gate — no migration shim. Expected: any in-progress save from before this change must be discarded.

`MapState` write/read order: the new `ground_effects` block sits immediately after the v50 `noise_events` block. Same per-entry encoding shown in §1.5.

`world_manager.cpp` save / load — the active map's `ground_effects_` is copied to `MapState.ground_effects` on save, and restored on load. The location-cache path (cached non-active maps) handles `ground_effects` in `LocationState` the same way it already handles `traps` and `noise_events`.

## Section 7 — Render pass

In the existing map-render flow, after fixtures and before traps (or wherever the trap pass currently sits — match the project's render order so smoke draws *over* floor / fixtures but *under* trap glyphs and entities):

```cpp
for (const auto& ge : world.ground_effects()) {
    if (!visibility.is_visible(ge.x, ge.y)) continue;
    const auto& def = ground_effect_def_for(ge.kind);
    int phase = world.world_tick() & 1;
    char glyph = ground_effect_glyph(ge.kind, ge.ttl, def.center_ttl, phase);
    int color  = ground_effect_color(ge.kind);
    ctx.draw_cell(ge.x, ge.y, glyph, color);
}
```

Smoke is only drawn on visible tiles. Tiles outside FOV that happen to be smoked stay un-rendered (consistent with the project's existing fog-of-war behavior — explored-but-not-visible tiles render terrain in dim color, but transient effects like smoke are not memorized).

## Section 8 — Verification (manual smoke test)

No unit-test harness — all checks are dev-mode `--term` runs. Build with `cmake -B build -DDEV=ON && cmake --build build`.

1. **Spawn & throw.** `dev-spawn` a smoke grenade. Equip thrown, telegraph, throw at a far open tile. Expect: 5×5 patch appears, glyphs differ by ring, alternation visible across world ticks.
2. **Vision inside.** Move the player into the cloud center. Expect: FOV collapses to player tile + 1 ring of smoke.
3. **Vision outside.** Stand 4 tiles from the patch with a clear line. Expect: smoke tiles visible, nothing behind them visible.
4. **Wall LOS at stamp time.** Throw smoke at a tile adjacent to a wall such that some 5×5 cells are wall-shadowed from the impact tile. Expect: those wall-shadowed cells are NOT smoked. The patch shape is asymmetric.
5. **Refresh, no stack.** Throw a second smoke onto a fading patch. Expect: TTLs refresh up to the new max, no doubled durations, no double-render.
6. **Save / reload.** Mid-cloud, save and reload. Expect: cloud persists, TTLs continue from where they were.
7. **Decay shape.** Wait through the full duration. Expect: outer ring (TTL 6) vanishes after 6 ticks, ring 1 (TTL 9) vanishes after 9, center (TTL 12) vanishes after 12. Patch visibly shrinks.
8. **Multiple clouds.** Two non-overlapping smoke throws. Expect: both render, both decay independently.
9. **NPC pathing through smoke.** Hostile NPC with line-of-sight loses sight when the player ducks behind smoke. Expect: NPC AI loses target, falls back to wandering / last-known-position behavior (whatever the existing AI already does when LOS breaks; smoke is not special-cased here).
10. **Schema reject.** Load an old (v50) save. Expect: rejected with the standard schema-mismatch message.

## Section 9 — Roadmap follow-ups (out of scope)

- Acid pools (per-step damage + stack tile), ice patches (slow + slip), fire pools (Burn re-application). Each is a `GroundEffectKind` enum value + a row in `kDefs[]` + an optional per-step entity hook + a render entry. No further save / FOV work.
- AI awareness of smoke (path around, hold fire when LOS broken).
- Render of "remembered smoke" in fog-of-war. Currently smoke draws only on visible tiles.
- Animated radial growth on detonation (per-tick expansion vs single-frame stamp).
- Flashbang true Stun.
