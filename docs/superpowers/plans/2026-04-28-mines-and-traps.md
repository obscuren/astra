# Mines & Traps Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Activate the 5 inert mine items (Proximity, EMP, Incendiary, Decoy, Caltrops) by building a generalized trap/trigger framework that also supports pre-placed dungeon traps.

**Architecture:** Per-`MapState` `Trap` registry, telegraph-driven deployment, live faction-eval triggers, `Player.trap_detection`-keyed detection roll, minimal `NoiseEvent` queue for the Decoy. Save schema bumps to v50.

**Tech Stack:** C++20, CMake, existing astra modules (`Effect`, `Telegraph`, `Faction`, `TileMap`, `Renderer`).

**Reference spec:** `docs/superpowers/specs/2026-04-28-mines-and-traps-design.md`.

**Project test convention:** No unit-test harness. Verification = `cmake --build build` clean + manual dev-console smoke. The "test" steps below are smoke tests run interactively in `--term` mode.

**Build command (used everywhere):**
```bash
cmake -B build -DDEV=ON && cmake --build build
```

**Branch:** Create a feature branch `feature/mines-and-traps` before Task 1.1.

```bash
git checkout -b feature/mines-and-traps
```

Each Phase ends with **one squashed commit** to `feature/mines-and-traps`. The 6 phase commits match the spec's Section 11 phasing.

---

## Phase 1 — Trap data model + storage + save bump

### Task 1.1: Create `include/astra/trap.h`

**Files:**
- Create: `include/astra/trap.h`

- [ ] **Step 1: Write the header**

```cpp
#pragma once

#include <cstdint>
#include <string>

namespace astra {

class Game;
struct Player;
struct Npc;
class TileMap;

enum class TrapKind : uint8_t {
    ProximityMine = 0,
    EmpMine,
    IncendiaryMine,
    DecoyMine,
    Caltrops,
    DungeonGeneric,
};

enum class TrapTrigger : uint8_t {
    NonFriendlyToOwner = 0,
    AnyEntity,
    PlayerOnly,
};

struct Trap {
    TrapKind kind = TrapKind::ProximityMine;
    int x = 0;
    int y = 0;

    // Visibility / detection
    bool hidden = true;
    int reveal_radius = 2;             // Chebyshev
    int detection_dc = 12;
    bool was_in_player_radius = false; // debounce flag for detection roll

    // Trigger logic
    TrapTrigger trigger_mode = TrapTrigger::NonFriendlyToOwner;
    std::string owner_faction;         // "" if player- or dungeon-placed
    bool placer_is_player = false;
    int placer_npc_id = -1;            // for NPC-placed traps

    // State
    int activations_remaining = 1;
    int placed_tick = 0;
};

// Display helpers
const char* trap_kind_name(TrapKind k);
char trap_glyph(TrapKind k);
int  trap_color(TrapKind k);

// Player deploy (telegraph confirm calls into here)
void place_player_trap(Game& game, TrapKind kind, int dest_x, int dest_y, int item_index);

// Dungeon-placement helper — used by future generators
void place_dungeon_trap(TileMap& map, int x, int y, TrapKind kind,
                        TrapTrigger trigger = TrapTrigger::AnyEntity,
                        bool hidden = true,
                        int detection_dc = 14);

// Per-tick / per-event hooks
void on_entity_enters_tile(Game& game, int x, int y, bool is_player, int npc_id);
void update_trap_detection(Game& game);

// Item id ↔ TrapKind mapping (used by use_item dispatch)
TrapKind trap_kind_for_item_id(uint16_t item_id);

} // namespace astra
```

- [ ] **Step 2: Verify the file compiles standalone**

Run:
```bash
cmake --build build --target astra 2>&1 | head -40
```
Expected: no errors referencing `trap.h` (it's not yet `#include`d anywhere; build should still succeed).

### Task 1.2: Stub `src/trap.cpp`

**Files:**
- Create: `src/trap.cpp`
- Modify: `CMakeLists.txt` (add `src/trap.cpp`)

- [ ] **Step 1: Add the source file to CMake**

Find the source list in `CMakeLists.txt` (search for an existing `src/...cpp` entry such as `src/tinkering.cpp`) and add `src/trap.cpp` next to it, in alphabetical order if the list is alphabetical, otherwise next to other gameplay files.

- [ ] **Step 2: Write `src/trap.cpp` with stub bodies**

```cpp
#include "astra/trap.h"

#include "astra/game.h"
#include "astra/item_ids.h"
#include "astra/npc.h"
#include "astra/player.h"
#include "astra/tilemap.h"
#include "astra/world_manager.h"

#include <cmath>

namespace astra {

const char* trap_kind_name(TrapKind k) {
    switch (k) {
        case TrapKind::ProximityMine:  return "proximity mine";
        case TrapKind::EmpMine:        return "EMP mine";
        case TrapKind::IncendiaryMine: return "incendiary mine";
        case TrapKind::DecoyMine:      return "decoy mine";
        case TrapKind::Caltrops:       return "caltrops";
        case TrapKind::DungeonGeneric: return "trap";
    }
    return "trap";
}

char trap_glyph(TrapKind k) {
    switch (k) {
        case TrapKind::Caltrops: return '*';
        default:                  return '^';
    }
}

int trap_color(TrapKind k) {
    // Color::* enum values in renderer.h. Numeric placeholders here;
    // matched by terminal palette.
    switch (k) {
        case TrapKind::ProximityMine:  return 6;  // Cyan
        case TrapKind::EmpMine:        return 12; // BrightBlue
        case TrapKind::IncendiaryMine: return 11; // Orange / BrightYellow proxy
        case TrapKind::DecoyMine:      return 14; // Yellow
        case TrapKind::Caltrops:       return 15; // White
        case TrapKind::DungeonGeneric: return 9;  // Red
    }
    return 7;
}

TrapKind trap_kind_for_item_id(uint16_t id) {
    switch (id) {
        case ITEM_PROXIMITY_MINE:  return TrapKind::ProximityMine;
        case ITEM_EMP_MINE:        return TrapKind::EmpMine;
        case ITEM_INCENDIARY_MINE: return TrapKind::IncendiaryMine;
        case ITEM_DECOY_MINE:      return TrapKind::DecoyMine;
        case ITEM_CALTROPS:        return TrapKind::Caltrops;
    }
    return TrapKind::ProximityMine;
}

// --- stubs filled in later phases ---

void place_player_trap(Game&, TrapKind, int, int, int) {
    // Phase 2
}

void place_dungeon_trap(TileMap&, int, int, TrapKind,
                        TrapTrigger, bool, int) {
    // Phase 6
}

void on_entity_enters_tile(Game&, int, int, bool, int) {
    // Phase 3
}

void update_trap_detection(Game&) {
    // Phase 4
}

} // namespace astra
```

- [ ] **Step 3: Build and confirm clean compile**

```bash
cmake --build build 2>&1 | tail -20
```
Expected: `Linking target astra` and no errors. The new file builds and links because all functions have bodies (stubs).

- [ ] **Step 4: Verify renderer Color enum values match**

Open `include/astra/renderer.h` and confirm `Color::Cyan`, `Color::BrightBlue`, etc., match the numeric values used in `trap_color`. If the enum uses different names, replace numeric literals with `static_cast<int>(Color::Cyan)` etc. The point is the colors render correctly later; numeric or enum-cast — pick whichever the rest of the codebase uses for numeric color storage.

### Task 1.3: Add `MapState.traps` and bump save schema

**Files:**
- Modify: `include/astra/save_file.h`
- Modify: `src/save_file.cpp`

- [ ] **Step 1: Bump version in `save_file.h`**

Find the line:
```cpp
inline constexpr uint32_t SAVE_FILE_VERSION = 49;   // v49: tinkering expansion (learned_schematics + teaches_schematic_id)
```
Replace with:
```cpp
inline constexpr uint32_t SAVE_FILE_VERSION = 50;   // v50: trap registry + noise events
```

- [ ] **Step 2: Add `traps` to `MapState` and forward-include `trap.h`**

In `include/astra/save_file.h`, add at top of includes:
```cpp
#include "astra/trap.h"
```

Inside `struct MapState`, add right after `std::vector<GroundItem> ground_items;`:
```cpp
std::vector<Trap> traps;             // v50
// noise_events added in Phase 5
```

- [ ] **Step 3: Add serialization for `Trap` in `src/save_file.cpp`**

Locate the `MapState` write block (search for where `ground_items` is written; copy that pattern). Add after the ground_items block:

```cpp
// v50: traps
write_uint32(out, static_cast<uint32_t>(ms.traps.size()));
for (const Trap& t : ms.traps) {
    out.put(static_cast<char>(t.kind));
    write_int32(out, t.x);
    write_int32(out, t.y);
    out.put(t.hidden ? 1 : 0);
    write_int32(out, t.reveal_radius);
    write_int32(out, t.detection_dc);
    out.put(t.was_in_player_radius ? 1 : 0);
    out.put(static_cast<char>(t.trigger_mode));
    write_string(out, t.owner_faction);
    out.put(t.placer_is_player ? 1 : 0);
    write_int32(out, t.placer_npc_id);
    write_int32(out, t.activations_remaining);
    write_int32(out, t.placed_tick);
}
```

Then mirror the read block (find the `ground_items` read and add after):
```cpp
// v50: traps
{
    uint32_t n_traps = read_uint32(in);
    ms.traps.resize(n_traps);
    for (Trap& t : ms.traps) {
        t.kind = static_cast<TrapKind>(in.get());
        t.x = read_int32(in);
        t.y = read_int32(in);
        t.hidden = in.get() != 0;
        t.reveal_radius = read_int32(in);
        t.detection_dc = read_int32(in);
        t.was_in_player_radius = in.get() != 0;
        t.trigger_mode = static_cast<TrapTrigger>(in.get());
        t.owner_faction = read_string(in);
        t.placer_is_player = in.get() != 0;
        t.placer_npc_id = read_int32(in);
        t.activations_remaining = read_int32(in);
        t.placed_tick = read_int32(in);
    }
}
```

(Use the same `write_uint32`, `write_int32`, `write_string`, etc. helpers already present in `save_file.cpp` — copy the exact names from the surrounding code.)

- [ ] **Step 4: Build clean**

```bash
cmake --build build 2>&1 | tail -20
```
Expected: clean build, no errors.

- [ ] **Step 5: Smoke — start a fresh game and save**

Run `./build/astra`. Start a new game (existing v49 saves will be rejected on load — that's correct per project rule). Save the game, exit, reload. Confirm the save round-trips.

Expected:
- Old v49 saves show as `rejecting save '...': schema version 49, expected 50` on load.
- New v50 saves load cleanly with `traps` empty.

### Task 1.4: Phase 1 commit

- [ ] **Step 1: Stage and commit Phase 1**

```bash
git add include/astra/trap.h src/trap.cpp CMakeLists.txt include/astra/save_file.h src/save_file.cpp
git commit -m "$(cat <<'EOF'
feat(traps): trap data model + v50 save schema

Adds Trap, TrapKind, TrapTrigger types, MapState.traps registry,
trap.h/cpp module with stubbed lifecycle hooks, and serialization
through a v50 schema bump. No deploy or trigger logic yet — just
plumbing so traps can persist across save/load.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Phase 2 — Deploy pipeline

### Task 2.1: Verify / extend `TelegraphShape::Burst`

**Files:**
- Read: `include/astra/telegraph.h`
- Read/Modify: `src/telegraph.cpp` (only if Burst is unimplemented)

- [ ] **Step 1: Inspect telegraph.cpp for Burst handling**

```bash
grep -n "Burst\|TODO(burst)" /Users/jeffrey/dev/crawler/src/telegraph.cpp
```

Expected: either Burst is fully implemented (no TODO), or there's a `TODO(burst)` marker.

- [ ] **Step 2: If Burst is unimplemented, add a minimal implementation**

If the recompute path doesn't handle `TelegraphShape::Burst`, implement it as: "preview is a square of side `2*spec.width + 1` centered on the cursor, where the cursor moves freely within `spec.range` Chebyshev of origin." Use existing arrow-key cursor movement (look at how `Line` handles direction keys; for Burst the keys move the cursor x/y instead of rotating direction).

Pseudocode for `recompute()` Burst branch:
```cpp
case TelegraphShape::Burst: {
    preview_.path.clear();
    preview_.dest_x = cursor_x_;
    preview_.dest_y = cursor_y_;
    int w = spec_.width;
    for (int dy = -w; dy <= w; ++dy) {
        for (int dx = -w; dx <= w; ++dx) {
            TelegraphTile tile{cursor_x_ + dx, cursor_y_ + dy, false};
            if (spec_.require_walkable_dest) {
                tile.blocked = !game.world().map().passable(tile.x, tile.y);
            }
            preview_.path.push_back(tile);
        }
    }
    break;
}
```

For input handling, when `spec_.shape == Burst`:
- Arrow keys move `cursor_x_/cursor_y_` by ±1, clamped to range from origin (Chebyshev ≤ `spec_.range`).
- Enter confirms.
- Escape cancels.

`require_walkable_dest` should reject confirm if the centre cursor tile is not passable.

- [ ] **Step 3: Build clean**

```bash
cmake --build build 2>&1 | tail -10
```
Expected: clean build.

- [ ] **Step 4: Smoke — telegraph dev**

If there's an existing dev/console command that invokes a telegraph, use it. Otherwise, defer the Burst smoke until 2.4 (real mine deployment). Mark this step done if Burst was already implemented; otherwise note for verification at 2.4.

### Task 2.2: Implement `place_player_trap` and Caltrops scatter

**Files:**
- Modify: `src/trap.cpp`

- [ ] **Step 1: Implement scatter helper and `place_player_trap`**

Replace the Phase-2 stub in `src/trap.cpp` with:

```cpp
namespace {

constexpr int kCaltropsScatterCount = 4;

bool tile_passable(const Game& game, int x, int y) {
    return game.world().map().passable(x, y);
}

void scatter_caltrops(Game& game, int cx, int cy, bool placer_is_player) {
    auto& traps = game.world().map_state().traps;
    auto& rng = game.world().rng();

    // 9-tile candidate set in 3x3 around (cx, cy), excluding player's tile.
    struct P { int x, y; };
    std::vector<P> candidates;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int x = cx + dx, y = cy + dy;
            if (x == game.player().x && y == game.player().y) continue;
            if (!tile_passable(game, x, y)) continue;
            candidates.push_back({x, y});
        }
    }
    std::shuffle(candidates.begin(), candidates.end(), rng);
    int n = std::min<int>(kCaltropsScatterCount, candidates.size());
    for (int i = 0; i < n; ++i) {
        Trap t;
        t.kind = TrapKind::Caltrops;
        t.x = candidates[i].x;
        t.y = candidates[i].y;
        t.hidden = false;
        t.reveal_radius = 0;
        t.detection_dc = 0;
        t.activations_remaining = 3;
        t.placer_is_player = placer_is_player;
        t.owner_faction = "";
        t.placer_npc_id = -1;
        t.placed_tick = game.world().world_tick();
        traps.push_back(std::move(t));
    }
}

void place_single_trap(Game& game, TrapKind kind, int x, int y) {
    Trap t;
    t.kind = kind;
    t.x = x;
    t.y = y;
    // Per-kind defaults
    switch (kind) {
        case TrapKind::ProximityMine:  t.detection_dc = 12; t.hidden = true;  break;
        case TrapKind::EmpMine:        t.detection_dc = 13; t.hidden = true;  break;
        case TrapKind::IncendiaryMine: t.detection_dc = 11; t.hidden = true;  break;
        case TrapKind::DecoyMine:      t.detection_dc = 0;  t.hidden = false; break;
        default: break;
    }
    t.placer_is_player = true;
    t.owner_faction = "";
    t.placer_npc_id = -1;
    t.placed_tick = game.world().world_tick();
    game.world().map_state().traps.push_back(std::move(t));
}

} // namespace

void place_player_trap(Game& game, TrapKind kind, int dest_x, int dest_y, int item_index) {
    if (kind == TrapKind::Caltrops) {
        scatter_caltrops(game, dest_x, dest_y, /*placer_is_player=*/true);
    } else {
        place_single_trap(game, kind, dest_x, dest_y);
    }

    // Consume item
    auto& items = game.player().inventory.items;
    if (item_index >= 0 && item_index < static_cast<int>(items.size())) {
        Item& it = items[item_index];
        if (it.stackable && it.stack_count > 1) {
            --it.stack_count;
        } else {
            items.erase(items.begin() + item_index);
        }
    }
    game.advance_world(ActionCost::wait);
    game.log("You deploy the trap.");
}
```

Add `#include <algorithm>` at the top of `trap.cpp`.

- [ ] **Step 2: Confirm `Game::world().map_state()` accessor exists**

If it doesn't, find the equivalent — the active `MapState` accessor is whatever the rest of `world_manager` uses (often `world_.maps()[0]` or `world_.active_map_state()`). Replace `game.world().map_state()` with the correct call site. This is a one-symbol find-and-replace once you locate the right name.

- [ ] **Step 3: Build clean**

```bash
cmake --build build 2>&1 | tail -20
```
Expected: clean build.

### Task 2.3: Hook `ItemType::Mine` in `Game::use_item`

**Files:**
- Modify: `src/game_rendering.cpp` (the file owning `Game::use_item`)
- Modify: `include/astra/game.h` (add `Telegraph telegraph_;` if not already present, plus deploy callback)

- [ ] **Step 1: Verify Game already owns a `Telegraph` instance**

```bash
grep -n "Telegraph\|telegraph_" /Users/jeffrey/dev/crawler/include/astra/game.h /Users/jeffrey/dev/crawler/src/game.cpp | head -20
```
If `telegraph_` is already a member: skip to Step 2. If not: add `Telegraph telegraph_;` to `Game`'s private members in `game.h`.

- [ ] **Step 2: Replace the default `case` for `ItemType::Mine` with deploy logic**

In `src/game_rendering.cpp` `Game::use_item`, find:
```cpp
default:
    log("You can't use " + item.name + ".");
    return;
```

Add a new `case` *before* the default:
```cpp
case ItemType::Mine: {
    TrapKind kind = trap_kind_for_item_id(item.id);
    int range = (kind == TrapKind::Caltrops) ? 4 : 3;
    int width = (kind == TrapKind::Caltrops) ? 1 : 0;
    TelegraphSpec spec;
    spec.shape = TelegraphShape::Burst;
    spec.range = range;
    spec.width = width;
    spec.require_walkable_dest = true;
    int captured_index = index;
    telegraph_.begin(spec, player_.x, player_.y,
        [this, captured_index, kind](const TelegraphResult& r) {
            place_player_trap(*this, kind, r.dest_x, r.dest_y, captured_index);
        });
    return; // do NOT advance world or consume here — the telegraph callback handles both
}
```

Add `#include "astra/trap.h"` at the top of `game_rendering.cpp`.

- [ ] **Step 3: Confirm telegraph input is dispatched in the input loop**

```bash
grep -n "telegraph_\.\|telegraph_.handle_input\|telegraph_.active" /Users/jeffrey/dev/crawler/src/game_input.cpp /Users/jeffrey/dev/crawler/src/game.cpp | head -10
```
If telegraph input isn't already routed (e.g., `telegraph_.handle_input(key, *this)` in the input dispatch), find the input handler and add a check at the top: if telegraph is active, route the key to it and `return` early. This is the pattern any prior telegraph-using feature already follows; copy it.

- [ ] **Step 4: Build clean**

```bash
cmake --build build 2>&1 | tail -20
```
Expected: clean build.

### Task 2.4: Phase 2 smoke

- [ ] **Step 1: Spawn each mine via dev console**

Run `./build/astra`. Open the dev console (whichever key the project uses; check `dev_console.h`). Spawn each mine kind into the player's inventory:
```
spawn-item ITEM_PROXIMITY_MINE
spawn-item ITEM_EMP_MINE
spawn-item ITEM_INCENDIARY_MINE
spawn-item ITEM_DECOY_MINE
spawn-item ITEM_CALTROPS
```
(Use the actual dev-console syntax; consult `src/dev_console.cpp` for the spawn-item command name.)

- [ ] **Step 2: Deploy each mine and verify**

For each mine in inventory, press `a` (apply) to bring up the telegraph reticule. Move the cursor up to 3 (or 4 for Caltrops) tiles away, confirm placement.

Expected:
- Single-tile mines render as `^` at the chosen tile.
- Caltrops scatter 4 of 9 tiles in a 3x3 around the chosen centre as `*`.
- Inventory count decreases by 1 per deploy.
- Hidden mines (Proximity / EMP / Incendiary) render normally to the player because **placer_is_player == true** — owner override (Phase 4 will gate this for non-owner; Phase 2 just shows them since Phase 4 isn't done yet).

- [ ] **Step 3: Save / load round-trip**

Save, exit, reload. Confirm placed traps are still on the map with correct positions and kinds.

### Task 2.5: Phase 2 commit

- [ ] **Step 1: Squash and commit**

```bash
git add -A
git commit -m "$(cat <<'EOF'
feat(traps): telegraph-driven mine deployment

Wires ItemType::Mine into Game::use_item via TelegraphShape::Burst.
Single-tile mines deploy at range 3; Caltrops scatter 4 of 9 tiles
in a 3x3 reticule at range 4. Player-placed traps carry placer_is_player
and consume one stack count. No trigger logic yet (Phase 3).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Phase 3 — Trigger pipeline + per-kind effect resolution + EmpDisabled

### Task 3.1: Add `EmpDisabled` EffectId

**Files:**
- Modify: `include/astra/effect.h`
- Modify: `src/effect.cpp`

- [ ] **Step 1: Add the EffectId**

In `include/astra/effect.h`, add to the `EffectId` enum:
```cpp
// Debuffs (400+)
EmpDisabled = 400,
```

Add factory declaration near the other `make_*_ge` declarations:
```cpp
Effect make_emp_disabled_ge(int duration);
```

Extend `effect_for_id` mapping coverage (in the `.cpp`).

- [ ] **Step 2: Implement the factory**

In `src/effect.cpp`, add:
```cpp
Effect make_emp_disabled_ge(int duration) {
    Effect e;
    e.id = EffectId::EmpDisabled;
    e.name = "EMP-Disabled";
    e.color = Color::BrightBlue;
    e.duration = duration;
    e.remaining = duration;
    e.show_in_bar = true;
    return e;
}
```

And add a case to `effect_for_id`:
```cpp
case EffectId::EmpDisabled: return make_emp_disabled_ge(5);
```

- [ ] **Step 3: Gate energy weapon attacks on EmpDisabled**

```bash
grep -rn "energy\|fire_weapon\|ranged_attack\|missile" /Users/jeffrey/dev/crawler/src/game_combat.cpp | head -20
```

In the energy/ranged-weapon firing path, add an early-out before consuming energy or rolling damage:
```cpp
if (has_effect(player_.effects, EffectId::EmpDisabled)) {
    log("Your weapon is EMP-disabled.");
    return;
}
```

For NPC-side: in `src/npc.cpp`'s combat path (energy attacks), do the equivalent check on `npc.effects`.

- [ ] **Step 4: Gate ability cooldown advancement on EmpDisabled**

```bash
grep -rn "tick_cooldown\|cooldown\|advance_cooldown" /Users/jeffrey/dev/crawler/include/astra/ability.h /Users/jeffrey/dev/crawler/src/ability.cpp | head -10
```

Wherever ability cooldowns advance per tick, skip the advance for entities that have `EmpDisabled` active:
```cpp
if (has_effect(entity.effects, EffectId::EmpDisabled)) {
    // EMP freezes ability cooldowns — don't decrement
} else {
    // existing decrement logic
}
```

- [ ] **Step 5: Suppress aura emission on EmpDisabled**

```bash
grep -n "rebuild_auras_from_sources\|AuraSystem\|aura_system_\.tick" /Users/jeffrey/dev/crawler/src/aura_system.cpp /Users/jeffrey/dev/crawler/src/game.cpp | head -10
```

In `AuraSystem::tick`, when iterating emitter entities, skip emitters that have `EmpDisabled`:
```cpp
if (has_effect(entity.effects, EffectId::EmpDisabled)) continue;
```

- [ ] **Step 6: Build clean**

```bash
cmake --build build 2>&1 | tail -20
```
Expected: clean build.

### Task 3.2: Implement `should_trigger` and `resolve_trap`

**Files:**
- Modify: `src/trap.cpp`

- [ ] **Step 1: Add helpers and the trap-def table**

At the top of `src/trap.cpp` (after includes), add a per-kind defaults table:

```cpp
namespace {

struct TrapDef {
    int damage = 0;
    int burst_radius = 0;       // 0 = single tile only
    EffectId status = EffectId::Invulnerable; // sentinel "no status"
    int status_duration = 0;
    int status_tick_damage = 0;
};

constexpr TrapDef kTrapDefs[] = {
    /* ProximityMine  */ { 12, 1, EffectId::Invulnerable, 0, 0 },
    /* EmpMine        */ { 4,  1, EffectId::EmpDisabled,  5, 0 },
    /* IncendiaryMine */ { 8,  1, EffectId::Burn,         4, 2 },
    /* DecoyMine      */ { 0,  0, EffectId::Invulnerable, 0, 0 },
    /* Caltrops       */ { 3,  0, EffectId::Slow,         3, 0 },
    /* DungeonGeneric */ { 6,  0, EffectId::Invulnerable, 0, 0 },
};

const TrapDef& def_for(TrapKind k) {
    return kTrapDefs[static_cast<int>(k)];
}

int chebyshev(int ax, int ay, int bx, int by) {
    return std::max(std::abs(ax - bx), std::abs(ay - by));
}

} // namespace
```

Add `#include "astra/effect.h"`, `#include "astra/faction.h"`, and `#include <cstdlib>` at the top.

- [ ] **Step 2: Implement `should_trigger`**

```cpp
namespace {

bool should_trigger(const Trap& t, Game& game, bool stepper_is_player, int stepper_npc_id) {
    if (t.trigger_mode == TrapTrigger::AnyEntity)  return true;
    if (t.trigger_mode == TrapTrigger::PlayerOnly) return stepper_is_player;

    // NonFriendlyToOwner — live faction eval.
    if (t.placer_is_player) {
        if (stepper_is_player) return false;
        const Npc& npc = game.world().npc_by_id(stepper_npc_id);
        return is_hostile_to_player(npc.faction, game.player());
    }
    // NPC-placed
    if (stepper_is_player) {
        return is_hostile_to_player(t.owner_faction, game.player());
    }
    if (stepper_npc_id == t.placer_npc_id) return false;
    const Npc& stepper = game.world().npc_by_id(stepper_npc_id);
    return is_hostile(stepper.faction, t.owner_faction);
}

} // namespace
```

If `world().npc_by_id(int)` doesn't exist, find the existing accessor (e.g., `npcs()[id]` indexed lookup) and use that.

- [ ] **Step 3: Implement `resolve_trap`**

```cpp
namespace {

void apply_damage_and_status(Game& game, Player* player, Npc* npc,
                             const Trap& t, const TrapDef& def) {
    // Splash immunity for the placer
    if (player && t.placer_is_player) return;
    if (npc   && t.placer_npc_id == npc->id) return;

    int dmg = def.damage;
    if (player) {
        player->hp = std::max(0, player->hp - dmg);
    } else if (npc) {
        npc->hp = std::max(0, npc->hp - dmg);
    }

    if (def.status != EffectId::Invulnerable) {
        Effect e = effect_for_id(def.status);
        if (def.status == EffectId::Burn || def.status == EffectId::Poison) {
            e = make_burn_ge(def.status_duration, def.status_tick_damage);
        } else if (def.status == EffectId::Slow) {
            e.duration = e.remaining = def.status_duration;
        } else if (def.status == EffectId::EmpDisabled) {
            e = make_emp_disabled_ge(def.status_duration);
        }
        if (player) add_effect(player->effects, e);
        else if (npc) add_effect(npc->effects, e);
    }
}

void resolve_trap(Game& game, const Trap& t, int x, int y,
                  bool stepper_is_player, int stepper_npc_id) {
    const TrapDef& def = def_for(t.kind);

    // Decoy mine — emits noise (Phase 5). For now, just log + no damage.
    if (t.kind == TrapKind::DecoyMine) {
        game.log("The decoy beeps loudly!");
        // Phase 5 hook: emit_noise_event(game, t);
        return;
    }

    // Damage stepping entity
    if (stepper_is_player) {
        apply_damage_and_status(game, &game.player(), nullptr, t, def);
    } else {
        Npc& n = game.world().npc_by_id(stepper_npc_id);
        apply_damage_and_status(game, nullptr, &n, t, def);
    }

    // Splash to other entities in burst radius (skip stepper; they already took it)
    if (def.burst_radius > 0) {
        // Player splash
        if (!stepper_is_player &&
            chebyshev(game.player().x, game.player().y, x, y) <= def.burst_radius) {
            apply_damage_and_status(game, &game.player(), nullptr, t, def);
        }
        // NPC splash
        for (Npc& n : game.world().npcs()) {
            if (n.id == stepper_npc_id) continue;
            if (chebyshev(n.x, n.y, x, y) <= def.burst_radius) {
                apply_damage_and_status(game, nullptr, &n, t, def);
            }
        }
    }

    // Log
    switch (t.kind) {
        case TrapKind::ProximityMine:
        case TrapKind::EmpMine:
        case TrapKind::IncendiaryMine:
            game.log(std::string("The ") + trap_kind_name(t.kind) + " detonates!");
            break;
        case TrapKind::Caltrops:
            game.log(stepper_is_player
                ? std::string("You step on caltrops!")
                : std::string("The ") + game.world().npc_by_id(stepper_npc_id).name + " steps on caltrops!");
            break;
        default: break;
    }
}

} // namespace
```

- [ ] **Step 4: Implement `on_entity_enters_tile`**

Replace the Phase-3 stub:

```cpp
void on_entity_enters_tile(Game& game, int x, int y, bool is_player, int npc_id) {
    auto& traps = game.world().map_state().traps;
    for (auto it = traps.begin(); it != traps.end(); /* manual */) {
        if (it->x != x || it->y != y) { ++it; continue; }
        if (!should_trigger(*it, game, is_player, npc_id)) { ++it; continue; }

        resolve_trap(game, *it, x, y, is_player, npc_id);

        if (--it->activations_remaining <= 0) it = traps.erase(it);
        else ++it;
    }
}
```

- [ ] **Step 5: Build clean**

```bash
cmake --build build 2>&1 | tail -20
```
Expected: clean build. Fix any signature mismatches (most likely `npc_by_id` or `npcs()` accessor names — find the correct ones in `world_manager.h`).

### Task 3.3: Hook movement to call `on_entity_enters_tile`

**Files:**
- Modify: `src/game_world.cpp` (player movement)
- Modify: `src/npc.cpp` (NPC turn movement)

- [ ] **Step 1: Player move hook**

In `src/game_world.cpp`, find `Game::move_player` (or the function that updates `player_.x` / `player_.y` from a directional input). After the position is updated and before `advance_world` is called, insert:

```cpp
on_entity_enters_tile(*this, player_.x, player_.y, /*is_player=*/true, /*npc_id=*/-1);
```

Make sure to `#include "astra/trap.h"` at the top of `game_world.cpp`.

- [ ] **Step 2: NPC move hook**

In `src/npc.cpp`, find the per-NPC move path (the place where `npc.x` / `npc.y` is updated each turn). After the move:

```cpp
on_entity_enters_tile(game, npc.x, npc.y, /*is_player=*/false, npc.id);
```

If NPCs do not have a stable `id` field, add one. Search for any existing id-like field on `Npc` first (`uid`, `entity_id`, etc.).

- [ ] **Step 3: Build clean**

```bash
cmake --build build 2>&1 | tail -20
```

### Task 3.4: Phase 3 smoke

- [ ] **Step 1: Verify hostile NPC triggers each mine**

Run `./build/astra`. Spawn each mine, deploy on a tile, then dev-spawn a hostile NPC adjacent (e.g., a Feral) and let it walk in (or use AI step).

Expected:
- Proximity Mine: NPC takes 12 damage; if adjacent NPCs / player are in 3×3 radius and not the placer, they take splash.
- EMP Mine: NPC takes 4 damage and gains `EmpDisabled` for 5 ticks (visible in effect bar if NPCs render effects).
- Incendiary Mine: NPC takes 8 damage and `Burn` for 4 ticks at 2 dmg/tick.
- Caltrops: NPC takes 3 damage and `Slow` for 3 ticks; tile remains until 3 step-ons.
- Decoy: just logs "The decoy beeps loudly!" and self-destroys.

- [ ] **Step 2: Verify owner immunity**

Walk over your own placed Proximity Mine. Expected: no trigger (because `should_trigger` returns false for player on player-placed). Walk over your own Incendiary Mine while a hostile NPC also stands in the burst radius and triggers it from an adjacent tile. Expected: NPC takes splash; player does not.

- [ ] **Step 3: Verify friendly NPC does not trigger player-placed mine**

Dev-spawn a friendly (non-hostile) NPC and have it walk into the trap. Expected: no trigger.

### Task 3.5: Phase 3 commit

```bash
git add -A
git commit -m "$(cat <<'EOF'
feat(traps): trigger pipeline + EmpDisabled effect

on_entity_enters_tile fires from Game::move_player and NPC turn
movement, runs live faction-eval (is_hostile / is_hostile_to_player),
applies per-kind damage and status with placer-immune splash. Adds
EmpDisabled effect that gates energy weapons, ability cooldown
advance, and aura emission.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Phase 4 — Detection roll + rendering

### Task 4.1: Add `Player.trap_detection`

**Files:**
- Modify: `include/astra/player.h`
- Modify: `src/save_file.cpp`

- [ ] **Step 1: Add the field**

In `include/astra/player.h`'s `Player` struct, add (next to other simple int attributes):
```cpp
int trap_detection = 0;   // additive bonus to 1d20 detection rolls
```

- [ ] **Step 2: Serialize the field**

In `src/save_file.cpp`, find the Player write block. Add adjacent to other int writes:
```cpp
write_int32(out, p.trap_detection);
```
Mirror in the read block:
```cpp
p.trap_detection = read_int32(in);
```

- [ ] **Step 3: Build clean**

```bash
cmake --build build 2>&1 | tail -10
```

### Task 4.2: Implement `update_trap_detection`

**Files:**
- Modify: `src/trap.cpp`

- [ ] **Step 1: Add `relative_dir` helper**

In the anonymous namespace of `trap.cpp`:
```cpp
const char* relative_dir(int from_x, int from_y, int to_x, int to_y) {
    int dx = to_x - from_x, dy = to_y - from_y;
    if (dx == 0 && dy < 0)  return "to the north";
    if (dx == 0 && dy > 0)  return "to the south";
    if (dx > 0 && dy == 0)  return "to the east";
    if (dx < 0 && dy == 0)  return "to the west";
    if (dx > 0 && dy < 0)   return "to the northeast";
    if (dx < 0 && dy < 0)   return "to the northwest";
    if (dx > 0 && dy > 0)   return "to the southeast";
    if (dx < 0 && dy > 0)   return "to the southwest";
    return "nearby";
}
```

- [ ] **Step 2: Implement `update_trap_detection`**

Replace the Phase-4 stub:

```cpp
void update_trap_detection(Game& game) {
    auto& traps = game.world().map_state().traps;
    auto& rng = game.world().rng();
    const Player& p = game.player();

    for (Trap& t : traps) {
        if (!t.hidden || t.placer_is_player) continue;

        bool now_in = chebyshev(p.x, p.y, t.x, t.y) <= t.reveal_radius;
        if (!t.was_in_player_radius && now_in) {
            std::uniform_int_distribution<int> d20(1, 20);
            int roll = d20(rng);
            if (roll + p.trap_detection >= t.detection_dc) {
                t.hidden = false;
                game.log(std::string("You spot a ") + trap_kind_name(t.kind) + " " +
                         relative_dir(p.x, p.y, t.x, t.y) + "!");
            }
        }
        t.was_in_player_radius = now_in;
    }
}
```

- [ ] **Step 3: Call `update_trap_detection` after every player position change**

In `src/game_world.cpp`, find every place `player_.x` / `player_.y` is assigned (the `grep` from Task 1's exploration produced ~20 sites; the relevant ones are the move-player path, teleports, and zone transitions). After each, append:
```cpp
update_trap_detection(*this);
```

A pragmatic approach: wrap the existing `on_entity_enters_tile(*this, player_.x, player_.y, true, -1);` call from Phase 3 into a small helper and call `update_trap_detection` immediately after. For zone transitions and teleports, call the helper there as well.

- [ ] **Step 4: Build clean**

```bash
cmake --build build 2>&1 | tail -10
```

### Task 4.3: Render pass for traps

**Files:**
- Modify: `src/map_renderer.cpp`

- [ ] **Step 1: Locate the existing render passes**

```bash
grep -n "render\|draw\|fixture\|ground_item" /Users/jeffrey/dev/crawler/src/map_renderer.cpp | head -30
```

Identify where fixtures are drawn and where ground items are drawn. Insert a new pass between them.

- [ ] **Step 2: Add the trap render pass**

Add (between fixture and ground-item passes):

```cpp
// Trap pass — draw player-visible traps
{
    const auto& traps = world.map_state().traps;
    for (const Trap& t : traps) {
        bool visible = t.placer_is_player || !t.hidden;
        if (!visible) continue;
        if (!visibility.in_view(t.x, t.y)) continue;   // existing FOV check
        renderer.put(t.x - camera_x + offset_x,
                     t.y - camera_y + offset_y,
                     trap_glyph(t.kind),
                     trap_color(t.kind));
    }
}
```

(Match the existing renderer call shape — `put` / `draw_cell` / whatever the codebase uses; copy from the fixture pass right above.)

- [ ] **Step 3: Build clean**

```bash
cmake --build build 2>&1 | tail -10
```

### Task 4.4: Dev console `reveal_traps` toggle

**Files:**
- Modify: `src/dev_console.cpp`
- Modify: `include/astra/game.h` (add `bool reveal_traps_debug_ = false;`)

- [ ] **Step 1: Add the flag**

In `include/astra/game.h`, in the `Game` class private section:
```cpp
bool reveal_traps_debug_ = false;
public:
    bool reveal_traps_debug() const { return reveal_traps_debug_; }
    void toggle_reveal_traps() { reveal_traps_debug_ = !reveal_traps_debug_; }
```

- [ ] **Step 2: Hook the debug flag in the render pass**

In the trap render pass added in 4.3, change `bool visible = t.placer_is_player || !t.hidden;` to:
```cpp
bool visible = game.reveal_traps_debug() || t.placer_is_player || !t.hidden;
```

- [ ] **Step 3: Add the dev-console command**

In `src/dev_console.cpp`, register:
```cpp
register_command("reveal_traps", "Toggle render of all hidden traps", [](Game& g, const std::vector<std::string>&) {
    g.toggle_reveal_traps();
    g.log(g.reveal_traps_debug() ? "Trap debug ON" : "Trap debug OFF");
});
```
(Use the actual registration API in `dev_console.cpp` — copy a neighbouring registration's shape.)

- [ ] **Step 4: Build clean**

```bash
cmake --build build 2>&1 | tail -10
```

### Task 4.5: Phase 4 smoke

- [ ] **Step 1: Hidden trap test**

Dev-spawn a Proximity Mine, then dev-place it as a *non-player-placed* trap (use the dungeon helper once it lands in Phase 6, OR temporarily flip `placer_is_player = false` in dev mode). Walk toward it. With `trap_detection = 0`, a roll of 12+ on d20 is a 45% chance to spot at radius 2 (DC 12). Verify:
- On entry to radius 2: either you see "You spot a proximity mine to the east!" and it appears, or it stays hidden.
- Walking out and back in re-rolls.
- Setting `player.trap_detection = 20` via dev console makes detection guaranteed.

- [ ] **Step 2: Dev toggle test**

Hidden trap in view. Run `reveal_traps`. Confirm trap renders. Run again to toggle off.

- [ ] **Step 3: Save / load**

Save with detection state (some traps revealed, some still hidden, `was_in_player_radius` set). Reload. Verify state persists.

### Task 4.6: Phase 4 commit

```bash
git add -A
git commit -m "$(cat <<'EOF'
feat(traps): detection roll + rendering

Adds Player.trap_detection (additive 1d20 bonus), update_trap_detection
fires once per player move with debounce on radius enter/leave, and a
new render pass between fixtures and ground items. Owner sees own
traps regardless of hidden flag. Dev-console reveal_traps toggle for
debugging.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Phase 5 — Decoy noise events

### Task 5.1: Create `include/astra/noise_event.h`

**Files:**
- Create: `include/astra/noise_event.h`

- [ ] **Step 1: Write the header**

```cpp
#pragma once

#include <string>

namespace astra {

class Game;

struct NoiseEvent {
    int x = 0;
    int y = 0;
    int radius = 5;          // Chebyshev — 10-tile diameter for Decoy
    int ttl_ticks = 5;
    std::string emitter_owner_faction;
    bool emitter_is_player = false;
};

// Append a noise event to the active map.
void emit_noise_event(Game& game, NoiseEvent ev);

// Run every world tick: decrement ttl, erase expired.
void tick_noise_events(Game& game);

} // namespace astra
```

### Task 5.2: Wire `MapState.noise_events` and serialize

**Files:**
- Modify: `include/astra/save_file.h`
- Modify: `src/save_file.cpp`

- [ ] **Step 1: Add the field**

In `include/astra/save_file.h`, add `#include "astra/noise_event.h"` and inside `MapState`:
```cpp
std::vector<NoiseEvent> noise_events;  // v50
```

- [ ] **Step 2: Serialize**

In `src/save_file.cpp`, after the trap serialize block, write:
```cpp
write_uint32(out, static_cast<uint32_t>(ms.noise_events.size()));
for (const NoiseEvent& ev : ms.noise_events) {
    write_int32(out, ev.x);
    write_int32(out, ev.y);
    write_int32(out, ev.radius);
    write_int32(out, ev.ttl_ticks);
    write_string(out, ev.emitter_owner_faction);
    out.put(ev.emitter_is_player ? 1 : 0);
}
```

Mirror in the read block:
```cpp
{
    uint32_t n = read_uint32(in);
    ms.noise_events.resize(n);
    for (NoiseEvent& ev : ms.noise_events) {
        ev.x = read_int32(in);
        ev.y = read_int32(in);
        ev.radius = read_int32(in);
        ev.ttl_ticks = read_int32(in);
        ev.emitter_owner_faction = read_string(in);
        ev.emitter_is_player = in.get() != 0;
    }
}
```

- [ ] **Step 3: Build clean**

### Task 5.3: Implement `emit_noise_event`, `tick_noise_events`, hook Decoy

**Files:**
- Create: `src/noise_event.cpp`
- Modify: `src/trap.cpp` (Decoy emission)
- Modify: `src/game_world.cpp` (per-tick tick_noise_events call)
- Modify: `CMakeLists.txt` (add `src/noise_event.cpp`)

- [ ] **Step 1: Implement `noise_event.cpp`**

```cpp
#include "astra/noise_event.h"
#include "astra/game.h"
#include "astra/world_manager.h"

namespace astra {

void emit_noise_event(Game& game, NoiseEvent ev) {
    game.world().map_state().noise_events.push_back(std::move(ev));
}

void tick_noise_events(Game& game) {
    auto& events = game.world().map_state().noise_events;
    for (auto it = events.begin(); it != events.end(); ) {
        if (--it->ttl_ticks <= 0) it = events.erase(it);
        else ++it;
    }
}

} // namespace astra
```

Add to `CMakeLists.txt` source list.

- [ ] **Step 2: Hook Decoy emission in `resolve_trap`**

In `src/trap.cpp`, replace the Phase-3 placeholder Decoy branch:
```cpp
if (t.kind == TrapKind::DecoyMine) {
    game.log("The decoy beeps loudly!");
    NoiseEvent ev;
    ev.x = t.x;
    ev.y = t.y;
    ev.radius = 5;
    ev.ttl_ticks = 5;
    ev.emitter_is_player = t.placer_is_player;
    ev.emitter_owner_faction = t.owner_faction;
    emit_noise_event(game, std::move(ev));
    return;
}
```

Add `#include "astra/noise_event.h"` at the top.

- [ ] **Step 3: Hook `tick_noise_events` once per world tick**

In `src/game_world.cpp`, find `Game::advance_world` (or the per-tick loop). Add before existing per-tick effect tick:
```cpp
tick_noise_events(*this);
```

- [ ] **Step 4: Build clean**

```bash
cmake --build build 2>&1 | tail -20
```

### Task 5.4: NPC consumes noise events

**Files:**
- Modify: `src/npc.cpp`

- [ ] **Step 1: Add the consumer to the NPC turn**

In the NPC turn handler, **before** standard move planning runs, add:

```cpp
// Decoy / noise-event reaction (Idle / Wandering only)
if (npc.state == NpcState::Idle || npc.state == NpcState::Wandering) {
    const auto& events = world.map_state().noise_events;
    for (const NoiseEvent& ev : events) {
        if (chebyshev(npc.x, npc.y, ev.x, ev.y) > ev.radius) continue;
        bool hostile = ev.emitter_is_player
            ? is_hostile_to_player(npc.faction, player)
            : is_hostile(npc.faction, ev.emitter_owner_faction);
        if (!hostile) continue;
        npc.move_target_x = ev.x;
        npc.move_target_y = ev.y;
        npc.move_target_ttl = ev.ttl_ticks;
        break;
    }
}
```

If `Npc` doesn't have `move_target_x/y/ttl` fields, add them:
```cpp
int move_target_x = -1, move_target_y = -1, move_target_ttl = 0;
```
And also wire them through save_file.cpp serialization (alongside other Npc fields). The pathing logic should already prefer `move_target_*` if set; if not, find the existing wander pathing call and add a "if move_target_ttl > 0, path toward (move_target_x, move_target_y) and decrement ttl" branch.

- [ ] **Step 2: Build clean**

```bash
cmake --build build 2>&1 | tail -10
```

### Task 5.5: Phase 5 smoke

- [ ] **Step 1: Decoy pulls hostile NPCs**

Spawn a hostile NPC at radius 3-5 from the player. Deploy a Decoy Mine. Have the NPC step on it (or trigger it manually via dev console). Verify nearby idle/wandering hostile NPCs walk toward the decoy's position over the next 5 ticks.

- [ ] **Step 2: Decoy ignored by friendly NPCs and player**

Spawn a friendly NPC near a decoy. Confirm friendly does not retarget. Confirm player movement is unaffected (no log spam, no auto-pathing).

- [ ] **Step 3: Save / load**

Save with active noise events. Reload. Verify events persist and continue ticking down.

### Task 5.6: Phase 5 commit

```bash
git add -A
git commit -m "$(cat <<'EOF'
feat(traps): decoy mine noise event system

NoiseEvent queue on MapState; Decoy Mine emits a 5-radius / 5-ttl
event on trigger. NPCs in Idle/Wandering retarget to the noise
location for the event's TTL if the emitter is hostile-to-them.
Player ignores noise events.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Phase 6 — Dungeon API + smoke commands

### Task 6.1: Implement `place_dungeon_trap`

**Files:**
- Modify: `src/trap.cpp`

- [ ] **Step 1: Replace the Phase-6 stub**

```cpp
void place_dungeon_trap(TileMap& /*map*/, int x, int y, TrapKind kind,
                        TrapTrigger trigger, bool hidden, int detection_dc) {
    // Note: TileMap doesn't own the trap registry; the active MapState does.
    // Generators that call this currently are populating the active map
    // during world build. We append to the active map's traps via Game.
    // For now, the dungeon helper has the same shape as the design and is
    // called from generator contexts that have access to the active MapState.
    Trap t;
    t.kind = kind;
    t.x = x;
    t.y = y;
    t.hidden = hidden;
    t.reveal_radius = 2;
    t.detection_dc = detection_dc;
    t.trigger_mode = trigger;
    t.placer_is_player = false;
    t.placer_npc_id = -1;
    t.owner_faction = "";
    t.activations_remaining = 1;
    t.placed_tick = 0;
    // Caller is responsible for pushing into the correct MapState.traps
    // — see the dev-console smoke command below for usage.
    // (Future generators get the active MapState via Game; this overload
    // is a placeholder for the API shape.)
    (void)t;
}
```

The cleanest route: change the signature to accept `MapState&` (or `Game&`) so the caller passes the registry directly.

```cpp
void place_dungeon_trap(MapState& ms, int x, int y, TrapKind kind,
                        TrapTrigger trigger, bool hidden, int detection_dc) {
    Trap t;
    t.kind = kind;
    t.x = x;
    t.y = y;
    t.hidden = hidden;
    t.reveal_radius = 2;
    t.detection_dc = detection_dc;
    t.trigger_mode = trigger;
    t.placer_is_player = false;
    t.placer_npc_id = -1;
    t.owner_faction = "";
    t.activations_remaining = 1;
    t.placed_tick = 0;
    ms.traps.push_back(std::move(t));
}
```

Update the declaration in `include/astra/trap.h` accordingly:
```cpp
struct MapState; // forward
void place_dungeon_trap(MapState& ms, int x, int y, TrapKind kind,
                        TrapTrigger trigger = TrapTrigger::AnyEntity,
                        bool hidden = true,
                        int detection_dc = 14);
```

- [ ] **Step 2: Build clean**

```bash
cmake --build build 2>&1 | tail -10
```

### Task 6.2: Dev console `spawn-trap <kind>` command

**Files:**
- Modify: `src/dev_console.cpp`

- [ ] **Step 1: Register the command**

```cpp
register_command("spawn-trap", "Spawn a dungeon-trap at player feet (kind: prox | emp | incendiary | decoy | caltrops | dungeon)",
    [](Game& g, const std::vector<std::string>& args) {
        if (args.empty()) { g.log("usage: spawn-trap <kind>"); return; }
        const std::string& s = args[0];
        TrapKind k;
        if (s == "prox")        k = TrapKind::ProximityMine;
        else if (s == "emp")    k = TrapKind::EmpMine;
        else if (s == "incendiary") k = TrapKind::IncendiaryMine;
        else if (s == "decoy")  k = TrapKind::DecoyMine;
        else if (s == "caltrops") k = TrapKind::Caltrops;
        else if (s == "dungeon") k = TrapKind::DungeonGeneric;
        else { g.log("unknown trap kind"); return; }
        place_dungeon_trap(g.world().map_state(), g.player().x, g.player().y, k);
        g.log(std::string("Spawned ") + trap_kind_name(k));
    });
```

(Match the actual registration API; copy from a neighbour.)

- [ ] **Step 2: Build clean**

### Task 6.3: Final smoke pass

- [ ] **Step 1: Run the full Section 8 smoke checklist from the spec**

Execute each of the 11 smoke items:
1. Build clean (`cmake -B build -DDEV=ON && cmake --build build`).
2. Launch in `--term` mode.
3. Dev-spawn each mine kind into inventory; deploy each via `a` + telegraph.
4. Dev-spawn a hostile NPC; confirm it triggers each mine kind correctly.
5. Dev-spawn a friendly NPC; confirm it does **not** trigger player-placed mines.
6. Walk own placed mine — confirm no trigger.
7. Walk past hidden mine at distance 2 — confirm detection roll fires once; toggle `trap_detection` to verify math.
8. Decoy: confirm an idle hostile NPC walks toward the decoy on next turn.
9. Caltrops: confirm 4 of 9 tiles scatter, each persists for 3 step-ons, then disappears.
10. Save / load round-trip: traps + noise events + `was_in_player_radius` survive.
11. `place_dungeon_trap` smoke via `spawn-trap dungeon`: confirm `AnyEntity` mode triggers on player.

- [ ] **Step 2: Update docs**

In `docs/items.md`, replace each mine's description "(Inert.)" with the active behaviour summary. In `docs/mechanics.md`, add a "Traps" section describing trigger rules, detection roll, and the noise-event signal.

In `docs/roadmap.md`, check off the line "Consumable use code (stims/grenades/mines)" insofar as it covers mines (stims and grenades remain).

### Task 6.4: Phase 6 commit and merge prep

- [ ] **Step 1: Commit**

```bash
git add -A
git commit -m "$(cat <<'EOF'
feat(traps): dungeon-trap API + dev-console smoke command

Generators can call place_dungeon_trap(map_state, x, y, kind) to
seed pre-placed traps. Dev console spawn-trap <kind> command for
manual smoke. Closes the mines slice of the consumable-use roadmap
item; stims and grenades remain.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 2: Verify linear, squashable history**

```bash
git log --oneline main..HEAD
```
Expected: 6 commits, one per Phase, in order.

- [ ] **Step 3: Prepare merge**

Per project memory rule, do **not** auto-push or auto-merge. Notify the user: "All 6 phases done, build is clean, smoke passes. Ready for review and merge — say the word and I'll merge to main."

---

## Self-Review Checklist (run after writing this plan)

- [x] **Spec coverage**: Every section of the spec maps to one or more tasks. Section 1 (data model) → Phase 1; Section 2 (deploy) → Phase 2; Section 3 (trigger) → Phase 3; Section 4 (decoy noise) → Phase 5; Section 5 (visibility/detection) → Phase 4; Section 6 (save/load) → Phase 1 + addenda in Phase 4 (`trap_detection`) and Phase 5 (`noise_events`); Section 7 (dungeon-placement) → Phase 6; Section 8 (smoke) → Phase 6 final; Section 11 (phasing) → 6 phase commits.
- [x] **Placeholder scan**: No `TBD`, `TODO`, `implement later`. Every code block contains complete, paste-able code (modulo a few "find the right accessor name" notes that are unavoidable for unfamiliar codebase APIs — those are explicitly flagged with grep commands so the engineer locates them deterministically).
- [x] **Type consistency**: `Trap`, `TrapKind`, `TrapTrigger`, `NoiseEvent`, `EmpDisabled`, `place_player_trap`, `place_dungeon_trap`, `on_entity_enters_tile`, `update_trap_detection`, `tick_noise_events`, `trap_kind_for_item_id`, `trap_kind_name`, `trap_glyph`, `trap_color` are consistent across all phases.
- **Known unknowns** the engineer must resolve from the surrounding code (each flagged inline with a `grep` recipe):
  - Exact name of the active `MapState` accessor on `World` (`map_state()` vs `active_map_state()` vs `maps()[0]`).
  - Exact name of NPC accessor by id (`npc_by_id` vs indexed lookup).
  - Whether `Telegraph::Burst` is fully implemented (Task 2.1 covers extension).
  - Exact `dev_console.cpp` registration API.
  - Exact `save_file.cpp` write/read helper names (`write_int32`/`read_int32` etc.).
