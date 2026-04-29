# Smoke Clouds & Ground-Effect Framework Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the Smoke Grenade's placeholder Slow effect with a real, persistent vision-blocking cloud, built on a generalized per-tile ground-effect registry.

**Architecture:** New `astra/ground_effect.h` module (`GroundEffect`, `GroundEffectKind`, `GroundEffectDef`, stamp / tick / opacity helpers). New per-`MapState` `ground_effects` registry on `WorldManager` mirroring `traps_` / `noise_events_`. FOV refactored to take an `OpacityProbe` (TileMap pointer + optional opaque-overrides hash-set) so smoke tiles count as opaque without coupling `TileMap` to `WorldManager`. Renderer-side glyph/color tables; gameplay layer never imports them. Save schema bumps v50 → v51.

**Tech Stack:** C++20, CMake, existing astra modules (`TileMap`, `VisibilityMap`, `Renderer`, `Game`, `WorldManager`).

**Reference spec:** `docs/superpowers/specs/2026-04-29-smoke-clouds-design.md`.

**Project test convention:** No unit-test harness. Verification = `cmake --build build` clean + manual dev-console smoke. The "test" steps below are smoke tests run interactively in `--term --dev` mode.

**Build command (used everywhere):**
```bash
cmake -B build -DDEV=ON && cmake --build build
```

**Branch:** Create a feature branch `feature/smoke-clouds` before Task 1.1.

```bash
git checkout -b feature/smoke-clouds
```

Each Phase ends with **one squashed commit** to `feature/smoke-clouds`. The 5 phase commits land sequentially on the branch; final merge to `main` is a single squash of the branch.

---

## Phase 1 — Data model + storage + save bump

This phase compiles cleanly but produces no visible behavior. It only adds plumbing.

### Task 1.1: Create `include/astra/ground_effect.h`

**Files:**
- Create: `include/astra/ground_effect.h`

- [ ] **Step 1: Write the header**

```cpp
#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace astra {

class Game;

enum class GroundEffectKind : uint8_t {
    Smoke = 0,
    // Reserved: Acid, Ice, Fire — adding a kind requires only an enum value,
    // a row in the GroundEffectDef table, optional per-step gameplay hook,
    // and a render entry. No save schema change.
};

struct GroundEffect {
    GroundEffectKind kind = GroundEffectKind::Smoke;
    int x = 0;
    int y = 0;
    int ttl = 0;            // ticks remaining; entry erased when ttl <= 0
    uint16_t origin_id = 0; // optional grouping: which detonation produced this
};

struct GroundEffectDef {
    int  radius;          // Chebyshev half-width (5×5 = 2)
    int  center_ttl;      // initial TTL at impact tile
    int  ring_falloff;    // TTL subtracted per Chebyshev ring
    bool blocks_vision;   // smoke=true; future ice=false
};

const GroundEffectDef& ground_effect_def_for(GroundEffectKind k);

// Stamp a new ground-effect patch at (x, y). Walks (2*radius+1)² square,
// skips tiles whose Bresenham wall-LOS from impact is blocked. Per-tile
// TTL = max(1, center_ttl − ring * ring_falloff). On overlap with existing
// entry of same kind on the tile, TTL = max(new, existing).
void stamp_ground_effect(Game& game, GroundEffectKind kind, int x, int y);

// Run once per world tick: TTL--, erase expired.
void tick_ground_effects(Game& game);

// Build packed (x,y) hash-set of tiles whose active ground effect has
// blocks_vision=true. Used by FOV. Key = (uint64_t(uint32_t(x)) << 32) | uint32_t(y).
std::unordered_set<uint64_t> opaque_ground_effect_tiles(const Game& game);

} // namespace astra
```

- [ ] **Step 2: Build to confirm header compiles**

```bash
cmake --build build 2>&1 | tail -10
```

Expected: clean (header is unused so far; nothing else changes).

### Task 1.2: Stub `src/ground_effect.cpp`

**Files:**
- Create: `src/ground_effect.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write a minimal `.cpp` that links**

Initially we only need `ground_effect_def_for` to be linkable. The other three functions get real bodies in Phase 3; for Phase 1 they just abort.

```cpp
#include "astra/ground_effect.h"

#include "astra/game.h"

#include <cstdlib>

namespace astra {

namespace {
constexpr GroundEffectDef kDefs[] = {
    /* Smoke */ { /*radius*/2, /*center_ttl*/12, /*ring_falloff*/3, /*blocks_vision*/true },
};
} // namespace

const GroundEffectDef& ground_effect_def_for(GroundEffectKind k) {
    return kDefs[static_cast<int>(k)];
}

void stamp_ground_effect(Game&, GroundEffectKind, int, int) {
    // Phase 3 fills this in.
    std::abort();
}

void tick_ground_effects(Game&) {
    // Phase 3 fills this in.
}

std::unordered_set<uint64_t> opaque_ground_effect_tiles(const Game&) {
    // Phase 3 fills this in.
    return {};
}

} // namespace astra
```

- [ ] **Step 2: Add `src/ground_effect.cpp` to CMake sources**

Open `CMakeLists.txt`. Find the `add_executable(astra ...)` (or equivalent target) source list — search for an existing entry like `src/grenade.cpp` or `src/trap.cpp`. Add `src/ground_effect.cpp` next to it, alphabetically near `src/grenade.cpp`.

Example diff (the exact line nearby will vary; match the surrounding style):

```cmake
    src/game.cpp
    src/game_combat.cpp
    src/game_rendering.cpp
    src/game_world.cpp
+   src/ground_effect.cpp
    src/grenade.cpp
```

- [ ] **Step 3: Build clean**

```bash
cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -10
```

Expected: clean compile, no warnings.

### Task 1.3: Add `ground_effects_` to `WorldManager` and `LocationState`

**Files:**
- Modify: `include/astra/world_manager.h`

- [ ] **Step 1: Add `#include "astra/ground_effect.h"` to the includes block**

In `include/astra/world_manager.h`, near the existing `#include "astra/noise_event.h"` (around line 13):

```cpp
#include "astra/ground_effect.h"
#include "astra/noise_event.h"
```

(Alphabetical ordering, immediately above `noise_event.h`.)

- [ ] **Step 2: Add field to `LocationState`**

Inside `struct LocationState` (around line 29), immediately after the existing `std::vector<NoiseEvent> noise_events;` line, add:

```cpp
struct LocationState {
    TileMap map;
    VisibilityMap visibility;
    std::vector<Npc> npcs;
    std::vector<GroundItem> ground_items;
    std::vector<Trap> traps;
    std::vector<NoiseEvent> noise_events;
    std::vector<GroundEffect> ground_effects;   // NEW
    int player_x = 0;
    int player_y = 0;
};
```

- [ ] **Step 3: Add accessor and field to `WorldManager`**

In `class WorldManager`, immediately after the existing `noise_events()` accessor (around line 96–97), add:

```cpp
std::vector<GroundEffect>& ground_effects() { return ground_effects_; }
const std::vector<GroundEffect>& ground_effects() const { return ground_effects_; }
```

In the private members block (around line 209–210), immediately after `std::vector<NoiseEvent> noise_events_;`, add:

```cpp
std::vector<GroundEffect> ground_effects_;
```

- [ ] **Step 4: Build clean**

```bash
cmake --build build 2>&1 | tail -10
```

Expected: clean compile.

### Task 1.4: Add `ground_effects` to `MapState` and bump save schema

**Files:**
- Modify: `include/astra/save_file.h`
- Modify: `src/save_file.cpp`

- [ ] **Step 1: Bump version in `save_file.h`**

In `include/astra/save_file.h`, find:
```cpp
inline constexpr uint32_t SAVE_FILE_VERSION = 50;   // v50: trap registry + noise events
```
Replace with:
```cpp
inline constexpr uint32_t SAVE_FILE_VERSION = 51;   // v51: ground effects (smoke clouds)
```

- [ ] **Step 2: Forward-include and add field to `MapState`**

In `include/astra/save_file.h`, near the existing trap/noise-event includes, add:
```cpp
#include "astra/ground_effect.h"
```

Inside `struct MapState`, immediately after the existing `std::vector<NoiseEvent> noise_events;` line, add:
```cpp
std::vector<GroundEffect> ground_effects;  // v51
```

- [ ] **Step 3: Add write block in `src/save_file.cpp`**

Locate the `MapState` write block — search for `// v50: noise events` (around line 939). Insert the new block immediately after the closing `}` of the noise_events loop, before the `// Fixtures (v3+)` comment:

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

- [ ] **Step 4: Add read block (mirror)**

Find the matching read block — search for `// v50: noise events` in the read path (around line 1819). After the closing `}` of the noise_events read loop, before the `// Fixtures` block, add:

```cpp
    // v51: ground effects
    {
        uint32_t n_ge = r.read_u32();
        ms.ground_effects.resize(n_ge);
        for (auto& ge : ms.ground_effects) {
            ge.kind = static_cast<GroundEffectKind>(r.read_u8());
            ge.x = r.read_i32();
            ge.y = r.read_i32();
            ge.ttl = r.read_i32();
            ge.origin_id = r.read_u16();
        }
    }
```

- [ ] **Step 5: Build clean**

```bash
cmake --build build 2>&1 | tail -10
```

Expected: clean compile.

### Task 1.5: Mirror save_system + game_world + map_editor copy points

**Files:**
- Modify: `src/save_system.cpp`
- Modify: `src/game_world.cpp`
- Modify: `src/map_editor.cpp`

These three files copy live-world registries to/from `MapState` / `LocationState` for save, load, location swaps, and editor preserve/restore. Mirror every existing `noise_events` line with a parallel `ground_effects` line.

- [ ] **Step 1: `src/save_system.cpp` — save path**

Around line 69–70, immediately after:
```cpp
if (!dead) ms.noise_events = world.noise_events();
```
add:
```cpp
if (!dead) ms.ground_effects = world.ground_effects();
```

Around line 90–91, immediately after:
```cpp
cached.noise_events = state.noise_events;
```
add:
```cpp
cached.ground_effects = state.ground_effects;
```

- [ ] **Step 2: `src/save_system.cpp` — load path**

Around line 157–158, immediately after:
```cpp
world.noise_events() = ms.noise_events;
```
add:
```cpp
world.ground_effects() = ms.ground_effects;
```

Around line 189–190, immediately after:
```cpp
state.noise_events = std::move(cm.noise_events);
```
add:
```cpp
state.ground_effects = std::move(cm.ground_effects);
```

- [ ] **Step 3: `src/game_world.cpp` — location swap**

Around line 218, immediately after:
```cpp
state.noise_events = std::move(world_.noise_events());
```
add:
```cpp
state.ground_effects = std::move(world_.ground_effects());
```

Around line 232, immediately after:
```cpp
world_.noise_events() = std::move(state.noise_events);
```
add:
```cpp
world_.ground_effects() = std::move(state.ground_effects);
```

- [ ] **Step 4: `src/map_editor.cpp` — editor preserve/restore**

Around line 460, immediately after:
```cpp
state.noise_events = world_->noise_events();
```
add:
```cpp
state.ground_effects = world_->ground_effects();
```

Around line 474, immediately after:
```cpp
world_->noise_events() = state.noise_events;
```
add:
```cpp
world_->ground_effects() = state.ground_effects;
```

- [ ] **Step 5: Build clean**

```bash
cmake --build build 2>&1 | tail -10
```

Expected: clean compile, no warnings.

### Task 1.6: Phase 1 commit

- [ ] **Step 1: Stage and commit**

```bash
git add include/astra/ground_effect.h src/ground_effect.cpp \
        include/astra/world_manager.h \
        include/astra/save_file.h src/save_file.cpp \
        src/save_system.cpp src/game_world.cpp src/map_editor.cpp \
        CMakeLists.txt
git commit -m "feat(ground-effects): data model + v51 save schema (no behavior yet)"
```

Old v50 saves are now unreadable (rejected at load). This is intentional per the spec's "no backcompat pre-ship" rule. Document clearly in the commit message.

---

## Phase 2 — FOV refactor: introduce `OpacityProbe`

After this phase, FOV behavior is identical to before. We're just changing the function signature so a future opacity override can plug in. No smoke yet.

### Task 2.1: Add `OpacityProbe` and refactor FOV signatures

**Files:**
- Modify: `include/astra/fov.h`

- [ ] **Step 1: Replace the contents of `include/astra/fov.h`**

```cpp
#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace astra {

class TileMap;
class VisibilityMap;

// Pluggable opacity check for FOV. Holds a TileMap pointer (terrain + fixtures)
// and an optional set of "extra opaque" tiles (e.g., active smoke clouds).
// Built once per FOV call; passed by const ref into shadowcasting.
//
// Key encoding for extra_opaque: (uint64_t(uint32_t(x)) << 32) | uint32_t(y).
struct OpacityProbe {
    const TileMap* map = nullptr;
    const std::unordered_set<uint64_t>* extra_opaque = nullptr;

    bool opaque(int x, int y) const;
};

// Compute field of view using recursive shadowcasting.
// Clears current visibility, then marks tiles visible from (origin_x, origin_y)
// within the given radius. Walls (and any tile flagged opaque by the probe)
// block line of sight.
void compute_fov(const OpacityProbe& probe, VisibilityMap& vis,
                 int origin_x, int origin_y, int radius);

// A light source that extends the player's FOV in its direction.
struct LightSource {
    int x, y;
    int radius;
};

// Extend FOV from player toward light sources (additive — does not clear).
void compute_fov_lit(const OpacityProbe& probe, VisibilityMap& vis,
                     int player_x, int player_y,
                     const std::vector<LightSource>& lights);

} // namespace astra
```

### Task 2.2: Refactor `src/fov.cpp` to use `OpacityProbe`

**Files:**
- Modify: `src/fov.cpp`

- [ ] **Step 1: Replace the file contents**

```cpp
#include "astra/fov.h"
#include "astra/tilemap.h"
#include "astra/visibility_map.h"

#include <cmath>

namespace astra {

bool OpacityProbe::opaque(int x, int y) const {
    if (map && map->opaque(x, y)) return true;
    if (extra_opaque) {
        uint64_t key = (uint64_t(uint32_t(x)) << 32) | uint32_t(y);
        if (extra_opaque->count(key)) return true;
    }
    return false;
}

// Multipliers for transforming coordinates in each octant.
static constexpr int octant_transform[8][4] = {
    { 1,  0,  0,  1},
    { 0,  1,  1,  0},
    { 0, -1,  1,  0},
    {-1,  0,  0,  1},
    {-1,  0,  0, -1},
    { 0, -1, -1,  0},
    { 0,  1, -1,  0},
    { 1,  0,  0, -1},
};

static void cast_light(const OpacityProbe& probe, VisibilityMap& vis,
                        int ox, int oy, int radius,
                        int row, float start_slope, float end_slope,
                        int xx, int xy, int yx, int yy) {
    if (start_slope < end_slope) return;

    int radius_sq = radius * radius;

    for (int j = row; j <= radius; ++j) {
        int dx = -j - 1;
        int dy = -j;
        bool blocked = false;
        float new_start = start_slope;

        for (dx = dx + 1; dx <= 0; ++dx) {
            int map_x = ox + dx * xx + dy * xy;
            int map_y = oy + dx * yx + dy * yy;

            float l_slope = (dx - 0.5f) / (dy + 0.5f);
            float r_slope = (dx + 0.5f) / (dy - 0.5f);

            if (start_slope < r_slope) continue;
            if (end_slope > l_slope) break;

            if (dx * dx + dy * dy <= radius_sq) {
                vis.set_visible(map_x, map_y);
            }

            if (blocked) {
                if (probe.opaque(map_x, map_y)) {
                    new_start = r_slope;
                } else {
                    blocked = false;
                    start_slope = new_start;
                }
            } else {
                if (probe.opaque(map_x, map_y) && j < radius) {
                    blocked = true;
                    cast_light(probe, vis, ox, oy, radius,
                               j + 1, start_slope, l_slope,
                               xx, xy, yx, yy);
                    new_start = r_slope;
                }
            }
        }

        if (blocked) break;
    }
}

void compute_fov(const OpacityProbe& probe, VisibilityMap& vis,
                 int origin_x, int origin_y, int radius) {
    vis.clear_visible();
    vis.set_visible(origin_x, origin_y);

    for (int oct = 0; oct < 8; ++oct) {
        cast_light(probe, vis, origin_x, origin_y, radius,
                   1, 1.0f, 0.0f,
                   octant_transform[oct][0], octant_transform[oct][1],
                   octant_transform[oct][2], octant_transform[oct][3]);
    }
}

static void cast_light_lit(const OpacityProbe& probe, VisibilityMap& vis,
                            int ox, int oy, int radius,
                            int row, float start_slope, float end_slope,
                            int xx, int xy, int yx, int yy,
                            const std::vector<LightSource>& lights) {
    if (start_slope < end_slope) return;

    int radius_sq = radius * radius;

    for (int j = row; j <= radius; ++j) {
        int dx = -j - 1;
        int dy = -j;
        bool blocked = false;
        float new_start = start_slope;

        for (dx = dx + 1; dx <= 0; ++dx) {
            int map_x = ox + dx * xx + dy * xy;
            int map_y = oy + dx * yx + dy * yy;

            float l_slope = (dx - 0.5f) / (dy + 0.5f);
            float r_slope = (dx + 0.5f) / (dy - 0.5f);

            if (start_slope < r_slope) continue;
            if (end_slope > l_slope) break;

            if (dx * dx + dy * dy <= radius_sq) {
                for (const auto& ls : lights) {
                    int ldx = map_x - ls.x, ldy = map_y - ls.y;
                    if (ldx * ldx + ldy * ldy <= ls.radius * ls.radius) {
                        vis.set_visible(map_x, map_y);
                        break;
                    }
                }
            }

            if (blocked) {
                if (probe.opaque(map_x, map_y)) {
                    new_start = r_slope;
                } else {
                    blocked = false;
                    start_slope = new_start;
                }
            } else {
                if (probe.opaque(map_x, map_y) && j < radius) {
                    blocked = true;
                    cast_light_lit(probe, vis, ox, oy, radius,
                                    j + 1, start_slope, l_slope,
                                    xx, xy, yx, yy, lights);
                    new_start = r_slope;
                }
            }
        }

        if (blocked) break;
    }
}

void compute_fov_lit(const OpacityProbe& probe, VisibilityMap& vis,
                     int player_x, int player_y,
                     const std::vector<LightSource>& lights) {
    int max_radius = 0;
    for (const auto& ls : lights) {
        int dx = ls.x - player_x, dy = ls.y - player_y;
        int dist = static_cast<int>(std::sqrt(dx * dx + dy * dy)) + 1;
        int extended = dist + ls.radius;
        if (extended > max_radius) max_radius = extended;
    }

    for (int oct = 0; oct < 8; ++oct) {
        cast_light_lit(probe, vis, player_x, player_y, max_radius,
                       1, 1.0f, 0.0f,
                       octant_transform[oct][0], octant_transform[oct][1],
                       octant_transform[oct][2], octant_transform[oct][3],
                       lights);
    }
}

} // namespace astra
```

The diff vs. the original: every `const TileMap& map` parameter becomes `const OpacityProbe& probe`, and every `map.opaque(x, y)` call becomes `probe.opaque(x, y)`. The new `OpacityProbe::opaque` member at the top fuses the TileMap check with an optional override-set check.

### Task 2.3: Update `Game::recompute_fov` call sites

**Files:**
- Modify: `src/game_world.cpp`

The single direct caller of `compute_fov` / `compute_fov_lit` is `Game::recompute_fov()` (around line 2070, calls at line 2095 and 2111).

- [ ] **Step 1: Build the probe and pass it in**

Open `src/game_world.cpp` around line 2070. Find the body of `void Game::recompute_fov()`. Replace the single direct call to `compute_fov(world_.map(), ...)` (line 2095) with:

```cpp
    OpacityProbe probe{ &world_.map(), nullptr };
    compute_fov(probe, world_.visibility(), player_.x, player_.y, radius);
```

Replace the call to `compute_fov_lit(world_.map(), ...)` (line 2111) with:

```cpp
    compute_fov_lit(probe, world_.visibility(), player_.x, player_.y, lights);
```

(Reuse the same `probe`. The `extra_opaque` is `nullptr` for now — Phase 3 swaps in the smoke set.)

- [ ] **Step 2: Build clean**

```bash
cmake --build build 2>&1 | tail -10
```

Expected: clean compile.

### Task 2.4: Phase 2 smoke

- [ ] **Step 1: Run the game and confirm FOV unchanged**

```bash
./build/astra --term --dev
```

Walk around. Confirm:
- Player FOV looks identical to before this phase (same view radius, same wall shadowing).
- Light sources (campfires, lanterns, etc.) still illuminate correctly.
- Save and reload — visibility persists as before.

If FOV looks broken, almost certainly the `OpacityProbe::opaque` short-circuit is wrong. The probe should return true when *either* the underlying tile is opaque or the override-set contains it. Walls were rendering as see-through? Probably the `map &&` guard is failing — verify `&world_.map()` is not null.

### Task 2.5: Phase 2 commit

- [ ] **Step 1: Stage and commit**

```bash
git add include/astra/fov.h src/fov.cpp src/game_world.cpp
git commit -m "refactor(fov): take OpacityProbe instead of TileMap& (prep for smoke)"
```

---

## Phase 3 — Stamping + ticking + grenade rewiring

This is the phase where smoke actually appears. After this phase, throwing a smoke grenade lays a 5×5 cloud and FOV is blocked through it. Rendering still uses whatever the renderer falls back to for smoke tiles (likely nothing visible), so the cloud may be invisible — Phase 4 fixes that.

### Task 3.1: Implement `stamp_ground_effect`, `tick_ground_effects`, `opaque_ground_effect_tiles`

**Files:**
- Modify: `src/ground_effect.cpp`

- [ ] **Step 1: Replace the stub bodies with real implementations**

Open `src/ground_effect.cpp`. Replace the file with:

```cpp
#include "astra/ground_effect.h"

#include "astra/game.h"
#include "astra/tilemap.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <cstdlib>

namespace astra {

namespace {

constexpr GroundEffectDef kDefs[] = {
    /* Smoke */ { /*radius*/2, /*center_ttl*/12, /*ring_falloff*/3, /*blocks_vision*/true },
};

// Standard Bresenham line from (x0,y0) → (x1,y1). Returns false if any
// *intermediate* tile (not the endpoints) is opaque per TileMap::opaque
// (i.e. wall, StructuralWall, or vision-blocking fixture).
bool line_of_sight_walls_only(const TileMap& map, int x0, int y0, int x1, int y1) {
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    int x = x0, y = y0;
    while (x != x1 || y != y1) {
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx)  { err += dx; y += sy; }
        if (x == x1 && y == y1) break;
        if (map.opaque(x, y)) return false;
    }
    return true;
}

void upsert(std::vector<GroundEffect>& effects,
            GroundEffectKind kind, int x, int y, int ttl) {
    for (auto& ge : effects) {
        if (ge.kind == kind && ge.x == x && ge.y == y) {
            if (ttl > ge.ttl) ge.ttl = ttl;
            return;
        }
    }
    GroundEffect ge;
    ge.kind = kind;
    ge.x = x;
    ge.y = y;
    ge.ttl = ttl;
    ge.origin_id = 0;
    effects.push_back(ge);
}

} // namespace

const GroundEffectDef& ground_effect_def_for(GroundEffectKind k) {
    return kDefs[static_cast<int>(k)];
}

void stamp_ground_effect(Game& game, GroundEffectKind kind, int ix, int iy) {
    const GroundEffectDef& def = ground_effect_def_for(kind);
    auto& effects = game.world().ground_effects();
    const TileMap& map = game.world().map();

    for (int dy = -def.radius; dy <= def.radius; ++dy) {
        for (int dx = -def.radius; dx <= def.radius; ++dx) {
            int tx = ix + dx;
            int ty = iy + dy;
            if (tx < 0 || ty < 0 || tx >= map.width() || ty >= map.height()) continue;
            if (!(dx == 0 && dy == 0) &&
                !line_of_sight_walls_only(map, ix, iy, tx, ty)) continue;
            int ring = std::max(std::abs(dx), std::abs(dy));
            int ttl = def.center_ttl - ring * def.ring_falloff;
            if (ttl < 1) ttl = 1;
            upsert(effects, kind, tx, ty, ttl);
        }
    }
}

void tick_ground_effects(Game& game) {
    auto& effects = game.world().ground_effects();
    for (auto& ge : effects) ge.ttl -= 1;
    effects.erase(
        std::remove_if(effects.begin(), effects.end(),
                       [](const GroundEffect& ge) { return ge.ttl <= 0; }),
        effects.end());
}

std::unordered_set<uint64_t> opaque_ground_effect_tiles(const Game& game) {
    std::unordered_set<uint64_t> out;
    const auto& effects = game.world().ground_effects();
    out.reserve(effects.size());
    for (const auto& ge : effects) {
        const auto& def = ground_effect_def_for(ge.kind);
        if (!def.blocks_vision) continue;
        uint64_t key = (uint64_t(uint32_t(ge.x)) << 32) | uint32_t(ge.y);
        out.insert(key);
    }
    return out;
}

} // namespace astra
```

- [ ] **Step 2: Build clean**

```bash
cmake --build build 2>&1 | tail -10
```

Expected: clean compile.

### Task 3.2: Wire `tick_ground_effects` into the world tick

**Files:**
- Modify: `src/game_world.cpp`

- [ ] **Step 1: Add the call after `tick_noise_events`**

In `src/game_world.cpp`, find the line (around line 2218):

```cpp
tick_noise_events(*this);
```

Add `#include "astra/ground_effect.h"` to the file's include block if not already present, then immediately after the noise-events tick add:

```cpp
tick_noise_events(*this);
tick_ground_effects(*this);
```

- [ ] **Step 2: Build clean**

```bash
cmake --build build 2>&1 | tail -10
```

### Task 3.3: Wire smoke set into `Game::recompute_fov`

**Files:**
- Modify: `src/game_world.cpp`

- [ ] **Step 1: Replace the `OpacityProbe{ &world_.map(), nullptr }` line**

In `Game::recompute_fov()` (around line 2070), the line added in Task 2.3 currently reads:

```cpp
OpacityProbe probe{ &world_.map(), nullptr };
```

Replace it with:

```cpp
auto smoke_opaque = opaque_ground_effect_tiles(*this);
OpacityProbe probe{ &world_.map(), &smoke_opaque };
```

Make sure `#include "astra/ground_effect.h"` is in the file (added in Task 3.2).

`recompute_fov` is called every time the world advances or the player moves, so this rebuilds the smoke set on each FOV recompute. With a handful of clouds, this is tens of hash inserts per call — negligible.

- [ ] **Step 2: Build clean**

```bash
cmake --build build 2>&1 | tail -10
```

### Task 3.4: Rewire `detonate_grenade` for `GrenadeKind::Smoke`

**Files:**
- Modify: `src/grenade.cpp`

- [ ] **Step 1: Update smoke entry in `kGrenadeDefs`**

In `src/grenade.cpp` around line 27, find:
```cpp
/* Smoke      */ { 0,  1, static_cast<int>(EffectId::Slow),         4, 0 },
```
Replace with:
```cpp
/* Smoke      */ { 0,  2, 0,                                        0, 0 },
```

Notes:
- `burst_radius` is now `2` so the grenade-throw telegraph reticule shows a 5×5 footprint matching the actual smoke patch (WYSIWYG aim).
- `status` is `0` (no status); pure utility grenade.

- [ ] **Step 2: Add Smoke branch in `detonate_grenade`**

In `src/grenade.cpp`, find the `void detonate_grenade(...)` function (around line 131). At the very top of its body — *before* the headline log line `game.log("The " + display_name(kind) + " detonates!");` — add:

```cpp
void detonate_grenade(Game& game, GrenadeKind kind, int x, int y,
                      bool placer_is_player) {
    if (kind == GrenadeKind::Smoke) {
        game.log("The smoke grenade pops — a thick cloud billows out.");
        stamp_ground_effect(game, GroundEffectKind::Smoke, x, y);
        return;
    }
    const GrenadeDef& def = grenade_def_for(kind);
    // ... existing body unchanged below ...
```

Add `#include "astra/ground_effect.h"` to the file's include block.

- [ ] **Step 3: Build clean**

```bash
cmake --build build 2>&1 | tail -10
```

### Task 3.5: Phase 3 smoke test

- [ ] **Step 1: Throw a smoke grenade and verify FOV blocking**

```bash
./build/astra --term --dev
```

Steps inside the game:
1. Use the dev console / dev-spawn to put a Smoke Grenade in inventory (whatever command the project uses to spawn items — `dev-spawn smoke_grenade` or browse the dev item picker).
2. Equip thrown slot, press `[T]`, telegraph, throw at an open tile ~4 squares ahead with a clear path.
3. Watch the log: `"The smoke grenade pops — a thick cloud billows out."`
4. The 5×5 patch is **not yet rendered as glyphs** (Phase 4 wires that), but FOV should already be blocked. Walk forward into where the smoke is — your view should collapse to your tile + 1 ring as you cross the boundary.
5. Walk past the cloud at distance 6 — tiles "behind" the smoke (occluded by the invisible smoke patch) should not be visible, even though there's no wall there.
6. Wait 12 turns (press `.` repeatedly). FOV should fully restore as the cloud expires.

If FOV is *not* blocked: most likely cause is `tick_ground_effects` running before/instead of after the entries are read — but since recompute_fov runs after the world tick, that's fine. More likely: `opaque_ground_effect_tiles` returns empty. Add a printf in there to confirm entries exist, or check that `stamp_ground_effect` actually populated `world.ground_effects()`.

If the placer_is_player splash branch crashes for Smoke grenades: confirm the `if (kind == GrenadeKind::Smoke) return;` is at the very top of `detonate_grenade`, before any non-Smoke logic.

### Task 3.6: Phase 3 commit

- [ ] **Step 1: Stage and commit**

```bash
git add src/ground_effect.cpp src/game_world.cpp src/grenade.cpp
git commit -m "feat(ground-effects): stamp + tick + smoke grenade rewire (FOV blocking)"
```

---

## Phase 4 — Render pass + animation

After this phase, the smoke cloud is visible: `▓` / `▒` / `░` glyphs alternating per world tick.

### Task 4.1: Add render-side glyph and color helpers

**Files:**
- Modify: `src/map_renderer.cpp`

These helpers stay file-local — gameplay code never imports them. The astra terminal renderer uses UTF-8 byte strings via `UIContext::put(int, int, const char*, Color)` for multi-byte glyphs (see `terminal_theme.cpp` and `telegraph.cpp` for examples — `"\xe2\x96\x93"` is `▓`, `"\xe2\x96\x92"` is `▒`, `"\xe2\x96\x91"` is `░`).

- [ ] **Step 1: Add `#include "astra/ground_effect.h"` near the top**

Open `src/map_renderer.cpp`. Find the existing `#include "astra/trap.h"` (around line 12). Add immediately after:

```cpp
#include "astra/ground_effect.h"
```

- [ ] **Step 2: Add the helper block above `render_map`**

If `map_renderer.cpp` already has an anonymous `namespace { ... }` near the top, add the helpers inside it. Otherwise, add a fresh anonymous block immediately above `void render_map(const MapRenderContext& rc)`:

```cpp
namespace {

const char* ground_effect_glyph(GroundEffectKind kind, int ttl, int center_ttl, int phase) {
    if (kind == GroundEffectKind::Smoke) {
        int hi = (center_ttl * 2) / 3;   // 8 when center=12
        int lo = center_ttl / 3;         // 4 when center=12
        if (ttl >= hi) {
            // Densest tier: ▒ ↔ ▓
            return phase ? "\xe2\x96\x93" /*▓*/ : "\xe2\x96\x92" /*▒*/;
        } else if (ttl >= lo) {
            // Mid tier: ░ ↔ ▒
            return phase ? "\xe2\x96\x92" /*▒*/ : "\xe2\x96\x91" /*░*/;
        } else {
            // Thin wisps: . ↔ ░
            return phase ? "\xe2\x96\x91" /*░*/ : ".";
        }
    }
    return "?";
}

Color ground_effect_color(GroundEffectKind kind) {
    if (kind == GroundEffectKind::Smoke) return Color::DarkGray;
    return Color::White;
}

} // namespace
```

- [ ] **Step 3: Build clean**

```bash
cmake --build build 2>&1 | tail -10
```

### Task 4.2: Insert smoke render pass in `map_renderer.cpp`

**Files:**
- Modify: `src/map_renderer.cpp`

The existing trap pass starts around line 302 (`// Trap pass — draw player-visible traps`). The smoke pass goes **immediately before** the trap pass — smoke draws over floor/fixtures, traps draw over smoke.

- [ ] **Step 1: Insert the ground-effect pass**

Find the comment line `// Trap pass — draw player-visible traps (between fixtures and ground items)` (around line 302). Immediately above it, insert:

```cpp
    // Ground-effect pass — draw active smoke / future acid / ice. Sits between
    // fixtures and traps so traps overlay smoke (you can still see your own
    // mine glyphs through fog).
    {
        const auto& effects = rc.world.ground_effects();
        int phase = rc.world.world_tick() & 1;
        for (const GroundEffect& ge : effects) {
            if (rc.world.visibility().get(ge.x, ge.y) != Visibility::Visible) continue;
            int sx = ge.x - rc.camera_x;
            int sy = ge.y - rc.camera_y;
            if (sx < 0 || sx >= rc.map_rect.w || sy < 0 || sy >= rc.map_rect.h) continue;
            const auto& def = ground_effect_def_for(ge.kind);
            const char* glyph = ground_effect_glyph(ge.kind, ge.ttl, def.center_ttl, phase);
            ctx.put(sx, sy, glyph, ground_effect_color(ge.kind));
        }
    }
```

(Match the bracket style and indentation of the surrounding trap block — they should look identical except for the inner loop body.)

- [ ] **Step 2: Build clean**

```bash
cmake --build build 2>&1 | tail -10
```

### Task 4.3: Phase 4 smoke test

- [ ] **Step 1: Throw smoke and visually confirm**

```bash
./build/astra --term --dev
```

1. Spawn smoke grenade, throw to ~4 tiles ahead (same as Phase 3.5).
2. Confirm:
   - The 5×5 patch is rendered as gray block characters.
   - Center tiles show `▓`, ring 1 shows `▒`, ring 2 shows `░`.
   - Wait one turn (`.`): glyphs flip (`▓` → `▒`, `▒` → `░`, `░` → `.`). Wait again: glyphs flip back. The cloud should *churn* once per world tick.
   - Watch for 6+ turns: the outer ring (TTL 6) disappears first, then ring 1 (TTL 9), then center (TTL 12). The patch visibly shrinks.
3. Walk into the cloud — confirm FOV collapses (already verified in Phase 3, but re-confirm rendering lines up with the opaque tiles).
4. Throw smoke at a tile next to a wall — confirm cells whose Bresenham LOS from impact crosses the wall are NOT smoked. Patch shape is asymmetric.

If the glyphs don't render (you see `?` or empty cells), the encoding-vs-renderer mismatch from Task 4.1 Step 3 is the culprit. Inspect a tile glyph elsewhere in `map_renderer.cpp` that uses `▒` or `▓` and use the exact same approach.

If the alternation looks too fast (e.g., flickering at 60 fps), the `phase` is being computed off the render frame instead of `world_tick()`. Confirm `rc.world.world_tick() & 1` not a frame counter.

### Task 4.4: Phase 4 commit

- [ ] **Step 1: Stage and commit**

```bash
git add src/map_renderer.cpp
git commit -m "feat(ground-effects): smoke render pass + per-tick glyph alternation"
```

---

## Phase 5 — Verification, save round-trip, roadmap update

This phase has no new code. It runs the spec's Section 8 verification list end-to-end and updates docs.

### Task 5.1: Save / load round-trip test

- [ ] **Step 1: Save mid-cloud and reload**

```bash
./build/astra --term --dev
```

1. Throw a smoke grenade.
2. Wait 2 turns so TTLs are partially decayed (center 10, ring 1 = 7, ring 2 = 4).
3. Save the game.
4. Quit (`Q` from main menu, or whatever the save-and-quit flow is).
5. Relaunch `./build/astra` and load the save.
6. Confirm:
   - The smoke patch is still on the map.
   - Remaining TTL is consistent (decay continues from where it stopped).
   - FOV is still blocked through the cloud.

If the cloud disappears on reload: the save/load mirror in Task 1.5 missed a copy point. Re-grep `noise_events` against the codebase and confirm every line has a `ground_effects` sibling.

### Task 5.2: Full verification checklist (spec §8)

Run each item from the spec's Section 8 manually and tick when passed:

- [ ] **8.1 Spawn & throw** — 5×5 patch appears, glyphs differ by ring, alternate per tick.
- [ ] **8.2 Vision inside** — player at center sees own tile + 1 ring only.
- [ ] **8.3 Vision outside** — smoke tiles visible from outside, tiles behind not visible.
- [ ] **8.4 Wall LOS at stamp time** — throw next to a wall; wall-shadowed cells in the 5×5 are not smoked.
- [ ] **8.5 Refresh, no stack** — second throw onto fading patch refreshes TTL (max), no doubling.
- [ ] **8.6 Save / reload** — covered in 5.1.
- [ ] **8.7 Decay shape** — outer ring expires first, patch shrinks.
- [ ] **8.8 Multiple clouds** — two non-overlapping throws decay independently.
- [ ] **8.9 NPC pathing through smoke** — NPC loses target when player is behind smoke.
- [ ] **8.10 Schema reject** — load a v50 save (from before Phase 1); confirm rejection message.

To produce a pre-Phase-1 v50 save for 8.10, either pull one off `main` before this branch was created (`git stash` your save dir or copy `~/.local/share/astra/saves/<file>` aside before merging) or skip 8.10 if no v50 save is handy — the schema-reject path is well-tested by the existing trap/noise_event v49→v50 work.

### Task 5.3: Update `docs/roadmap.md`

**Files:**
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Find the smoke entry**

Around line 61 of `docs/roadmap.md`:
```markdown
  - [ ] **Smoke Grenade** — currently applies Slow as placeholder; needs a real vision-blocking cloud (FOV opacity field over a tile patch with TTL). Touches FOV / visibility map
```

- [ ] **Step 2: Replace with completed entry**

```markdown
  - [x] **Smoke Grenade** (v51) — generic per-tile `GroundEffect` registry on `WorldManager` (mirrors `traps_` / `noise_events_`), 5×5 Bresenham wall-LOS-clipped stamp on detonation, per-ring TTL falloff (12/9/6), `OpacityProbe` extends FOV opacity without coupling `TileMap` to game state, gray block-glyph render with per-world-tick churn animation. Reusable for future Acid / Ice / Fire ground effects (additive — no save schema bump needed).
```

### Task 5.4: Update `docs/mechanics.md` and `docs/items.md`

**Files:**
- Modify: `docs/mechanics.md`
- Modify: `docs/items.md`

- [ ] **Step 1: Mechanics — add a "Ground effects" subsection**

Find a logical spot in `docs/mechanics.md` (likely near other on-tile effects: traps, noise events). Add a short section:

```markdown
### Ground effects

Per-tile transient effects laid down on the active map. Stored on `WorldManager.ground_effects_`; persisted in `MapState.ground_effects` (v51). One entry per affected tile.

- **Smoke**: 5×5 Chebyshev stamp from impact, Bresenham wall-LOS clipped.
  Center TTL 12, ring 1 TTL 9, ring 2 TTL 6 (outer ring dissipates first).
  Blocks FOV via `OpacityProbe`. No direct damage. Refresh-on-overlap (max
  TTL, not stacking).
```

- [ ] **Step 2: Items — note Smoke Grenade behavior**

Find the existing Smoke Grenade row in `docs/items.md`. Update its description to reflect the new behavior — replace any "applies Slow" placeholder text with "lays a 5×5 vision-blocking smoke cloud for ~12 ticks (outer ring dissipates after 6)."

### Task 5.5: Phase 5 squashed-branch commit

- [ ] **Step 1: Commit doc updates**

```bash
git add docs/roadmap.md docs/mechanics.md docs/items.md
git commit -m "docs(ground-effects): roadmap + mechanics + items entries for smoke"
```

- [ ] **Step 2: Squash the branch into 5 phase commits before merging**

```bash
git log --oneline main..HEAD
```

Expected: 5 commits, one per phase. If the count is higher (fix-on-fix work during a phase), interactive-rebase to squash each phase's tail into its head:

```bash
git rebase -i main
```

In the editor, mark fix-up commits as `f` (fixup) under their phase head. Per project rule "Clean commit history — squash fix-upon-fix chains into logical units before merging."

- [ ] **Step 3: Final build + smoke**

```bash
cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -10
./build/astra --term --dev
```

Quick run-through: spawn smoke, throw, walk through, save, reload, throw a second one. Confirm everything still works after the squash.

- [ ] **Step 4: Hand off for review (do NOT merge)**

Per project rule "Wait before merge — never merge without user verifying the build first": stop here. Tell the user the branch is ready for them to verify, and only merge once they've confirmed.
