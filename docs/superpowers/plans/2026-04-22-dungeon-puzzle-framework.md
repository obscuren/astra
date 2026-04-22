# Dungeon Puzzle Framework Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a reusable puzzle framework as dungeon pipeline layer 7, plus fixture-level proximity triggers. First puzzle: `SealedStairsDown` on Archive L1 (seals terminal room, places unlock button, violet-tints stairs post-unlock).

**Architecture:** New `apply_puzzles(...)` layer in `src/dungeon/pipeline.cpp` mirrors the existing `apply_fixtures(...)` pattern. Puzzle state lives on `TileMap::puzzles_` with kind-tagged payload, persisted via `save_file.cpp`. New `FixtureType::PrecursorButton` and `FixtureType::StairsDownPrecursor` join the fixture catalogue. `FixtureData` gains three fields (`puzzle_id`, `proximity_message`, `proximity_radius`). A small per-tick proximity pass in `Game::update()` maintains an ephemeral "currently in radius" set and emits flavor log lines.

**Tech Stack:** C++20, `namespace astra` / `namespace astra::dungeon`. Build: `cmake -B build -DDEV=ON && cmake --build build -j`. Smoke validation: `./build/astra :dungen precursor_ruin Precursor` on L1 + Archive quest playthrough via Io entry.

**Validation model (no unit-test framework):** Each task ends in a **build + smoke check + commit**. Build command: `cmake --build build -j`. Smoke checks are specific to each task and listed inline.

**Reference spec:** `docs/superpowers/specs/2026-04-22-dungeon-puzzle-framework-design.md`

---

## Task 1: Extend `FixtureData` with puzzle + proximity fields

**Files:**
- Modify: `include/astra/tilemap.h:422-433`

- [ ] **Step 1: Add three fields to `FixtureData`**

Open `include/astra/tilemap.h` and replace the `FixtureData` struct (lines 422–433) with:

```cpp
struct FixtureData {
    FixtureType type = FixtureType::Table;
    bool passable = false;
    bool interactable = false;
    int cooldown = 0;
    int last_used_tick = -1;
    bool locked = false;
    bool open = false;
    int light_radius = 0;
    bool blocks_vision = false;
    std::string quest_fixture_id;

    // Puzzle framework (layer 7)
    uint16_t    puzzle_id         = 0;   // 0 = not linked to any puzzle
    std::string proximity_message;       // empty = no proximity trigger
    uint8_t     proximity_radius  = 0;   // 0 = disabled; Chebyshev distance from player
};
```

- [ ] **Step 2: Build**

Run: `cmake --build build -j`
Expected: clean build, zero warnings. (The new fields have defaults so no call sites break.)

- [ ] **Step 3: Commit**

```bash
git add include/astra/tilemap.h
git commit -m "feat(tilemap): add puzzle_id + proximity fields to FixtureData"
```

---

## Task 2: Add `FixtureType::PrecursorButton` — enum, factory, theme, name, editor

**Files:**
- Modify: `include/astra/tilemap.h:419` (end of `FixtureType` enum, before `};`)
- Modify: `src/tilemap.cpp` (`make_fixture` switch, near the other interactable cases ~line 497)
- Modify: `src/terminal_theme.cpp` (after existing fixture visuals block)
- Modify: `src/game_rendering.cpp` (fixture name + description function — find `fixture_name` and `fixture_description`, or equivalent)
- Modify: `src/map_editor.cpp:~299` (editor palette lists)

- [ ] **Step 1: Add enum entry**

In `include/astra/tilemap.h`, add `PrecursorButton` as the last entry in `FixtureType` (just before `QuestFixture`, so `QuestFixture` stays last if it's the catch-all, or right after depending on existing style — check local convention):

```cpp
    PrecursorButton,   // '◘' — gold Precursor stud, unlocks a linked puzzle
    QuestFixture,
};
```

- [ ] **Step 2: Add factory defaults**

In `src/tilemap.cpp`, add a case in `make_fixture`:

```cpp
case FixtureType::PrecursorButton:
    fd.passable = false;
    fd.interactable = true;
    fd.proximity_message = "A button flashes faintly.";
    fd.proximity_radius = 4;
    break;
```

- [ ] **Step 3: Add terminal theme row**

In `src/terminal_theme.cpp`, add alongside the other fixture cases (the reconnaissance found `StairsDown` at line ~631 — add near there or with other Precursor items):

```cpp
case FixtureType::PrecursorButton:
    vis = {'o', "\xe2\x97\x98", Color::BrightYellow, Color::Default}; break;  // ◘ gold
```

(`"\xe2\x97\x98"` is UTF-8 for `◘`. `Color::BrightYellow` is the gold palette entry used elsewhere for Precursor items — verify by grepping `BrightYellow` in `terminal_theme.cpp` and matching the existing Precursor visual vocabulary.)

- [ ] **Step 4: Add name + description**

Find `fixture_name(FixtureType)` and `fixture_description(FixtureType)` in `src/game_rendering.cpp` (or wherever fixture text lives). Add cases:

```cpp
case FixtureType::PrecursorButton:
    return "Precursor stud";
// description:
case FixtureType::PrecursorButton:
    return "A recessed Precursor stud, dimly lit.";
```

- [ ] **Step 5: Add editor palette entry**

In `src/map_editor.cpp`, find the fixture palette/enum-iteration block (around line 299 per the reconnaissance). Add `PrecursorButton` to whatever palette vector or switch exists so it's selectable in dev-mode editing.

- [ ] **Step 6: Build + smoke**

Run: `cmake --build build -j`
Expected: clean build.

Smoke check (optional at this stage): launch dev mode, enter the map editor, verify `PrecursorButton` appears in the fixture palette.

- [ ] **Step 7: Commit**

```bash
git add include/astra/tilemap.h src/tilemap.cpp src/terminal_theme.cpp src/game_rendering.cpp src/map_editor.cpp
git commit -m "feat(fixtures): add PrecursorButton fixture type"
```

---

## Task 3: Add `FixtureType::StairsDownPrecursor` — enum, factory, theme, aliasing

**Files:**
- Modify: `include/astra/tilemap.h` (FixtureType enum)
- Modify: `src/tilemap.cpp` (`make_fixture`)
- Modify: `src/terminal_theme.cpp` (theme row)
- Modify: `src/game_rendering.cpp` (name/description)
- Modify: **all 13 sites** that special-case `FixtureType::StairsDown` (listed below)

**Sites that must treat `StairsDownPrecursor` equivalently to `StairsDown`:**
- `src/main.cpp:82`
- `src/tilemap.cpp:444`
- `src/generators/dungeon_level.cpp:54`
- `src/dialog_manager.cpp:426`
- `src/dev_console.cpp:933`, `src/dev_console.cpp:1006`
- `src/map_editor.cpp:299`
- `src/game.cpp:701`
- `src/game_input.cpp:403`
- `src/dungeon/fixtures.cpp:289`, `src/dungeon/fixtures.cpp:319`

(The numbers are reconnaissance-era; confirm via `rg 'FixtureType::StairsDown\b' src/ include/` at task time.)

- [ ] **Step 1: Add enum entry**

In `include/astra/tilemap.h`, add `StairsDownPrecursor` immediately after `StairsDown` in the `FixtureType` enum so the two sit together:

```cpp
    StairsDown,             // '>' descend
    StairsDownPrecursor,    // '>' descend, Nova violet — post-unlock Precursor variant
```

- [ ] **Step 2: Add factory defaults**

In `src/tilemap.cpp` `make_fixture`, add right after the `StairsDown` case:

```cpp
case FixtureType::StairsDownPrecursor:
    fd.passable = false;
    fd.interactable = true;
    fd.proximity_message = "The stairs pulse faintly with a familiar violet light. Not Conclave work.";
    fd.proximity_radius = 4;
    break;
```

- [ ] **Step 3: Add terminal theme row**

In `src/terminal_theme.cpp` (next to `StairsDown` at line ~631):

```cpp
case FixtureType::StairsDownPrecursor:
    vis = {'>', "\xe2\x96\xbc", Color::BrightMagenta, Color::Default}; break;  // ▼ violet
```

- [ ] **Step 4: Add name + description**

In `src/game_rendering.cpp`:

```cpp
case FixtureType::StairsDownPrecursor:
    return "stairs down";   // same player-facing name as StairsDown — the colour is the tell
// description:
case FixtureType::StairsDownPrecursor:
    return "Stairs down. They pulse with a familiar violet light.";
```

- [ ] **Step 5: Alias `StairsDownPrecursor` to `StairsDown` behavior at every site**

For each of the 13 call sites listed above, locate the code and update. The pattern:

```cpp
// BEFORE
if (f.type == FixtureType::StairsDown) { ... }

// AFTER
if (f.type == FixtureType::StairsDown ||
    f.type == FixtureType::StairsDownPrecursor) { ... }
```

Or equivalently in switch statements, add a fallthrough:

```cpp
case FixtureType::StairsDown:
case FixtureType::StairsDownPrecursor:
    /* shared behavior */ break;
```

Specific site guidance:

- **`src/dialog_manager.cpp:426`** — the `case FixtureType::StairsDown:` block that calls `game.descend_stairs(xy)`. Add a fallthrough from `StairsDownPrecursor`.
- **`src/game_input.cpp:403`** — descent trigger. Accept both.
- **`src/generators/dungeon_level.cpp:54`** — condition check during level generation. Accept both.
- **`src/dungeon/fixtures.cpp:289`, `:319`** — stair placement helpers. The placer places `StairsDown` (not `StairsDownPrecursor`) — no change needed for placement, but any lookup of "where is the down-stair?" should accept both.
- **`src/game.cpp:701`** — glyph/render lookup. The theme already handles both types; if this line is a hard-coded "if StairsDown then glyph `>`", extend to accept `StairsDownPrecursor` too.
- **`src/dev_console.cpp:933, :1006`** — dumpmap fixture labels. Accept both.
- **`src/map_editor.cpp:299`** — editor fixture name display. Accept both.
- **`src/main.cpp:82`** — legacy glyph handler. Accept both.
- **`src/tilemap.cpp:444`** — existing make_fixture case. No action (the new case was added in step 2).

- [ ] **Step 6: Build + smoke**

Run: `cmake --build build -j`
Expected: clean build.

Smoke check: launch game, use dev tools to spawn a `StairsDownPrecursor` fixture, verify:
1. It renders as a violet `>`.
2. Stepping onto it and pressing the descend key descends normally.

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "feat(fixtures): add StairsDownPrecursor variant + alias to StairsDown behavior"
```

---

## Task 4: Define `PuzzleKind`, `RequiredPuzzle`, `PuzzleState`

**Files:**
- Create: `include/astra/dungeon/puzzles.h` (new file, headers only for now)

- [ ] **Step 1: Write the puzzle types header**

Create `include/astra/dungeon/puzzles.h`:

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace astra::dungeon {

enum class PuzzleKind : uint8_t {
    SealedStairsDown = 0,
    // Future: SealedChamber, PressurePlateSequence, RuneAlignment, ...
};

struct RequiredPuzzle {
    PuzzleKind kind;
    uint32_t   depth_mask;   // which depths this puzzle runs on (depth_mask_bit(N))
};

// Per-level, per-puzzle runtime state. Persisted on TileMap.
// Kind-specific payload lives inline; future kinds may convert to std::variant.
struct PuzzleState {
    uint16_t   id         = 0;     // unique within the level (1-based; 0 = unlinked)
    PuzzleKind kind       = PuzzleKind::SealedStairsDown;
    bool       solved     = false;

    // SealedStairsDown payload:
    std::vector<std::pair<int,int>> sealed_tiles;           // floor → StructuralWall positions
    std::pair<int,int>              button_pos  { -1, -1 }; // where the unlock button was placed
    std::pair<int,int>              stairs_pos  { -1, -1 }; // cached stairs_dn position
};

} // namespace astra::dungeon
```

- [ ] **Step 2: Build**

Run: `cmake --build build -j`
Expected: clean build. (Header is self-contained; no consumers yet.)

- [ ] **Step 3: Commit**

```bash
git add include/astra/dungeon/puzzles.h
git commit -m "feat(dungeon): add PuzzleKind, RequiredPuzzle, PuzzleState types"
```

---

## Task 5: Add `required_puzzles` to `DungeonStyle`

**Files:**
- Modify: `include/astra/dungeon/dungeon_style.h:77-87`

- [ ] **Step 1: Include the new header + add the field**

At the top of `include/astra/dungeon/dungeon_style.h`:

```cpp
#include "astra/dungeon/puzzles.h"
```

Then extend the `DungeonStyle` struct:

```cpp
struct DungeonStyle {
    StyleId                      id;
    const char*                  debug_name;
    std::string                  backdrop_material;
    LayoutKind                   layout;
    StairsStrategy               stairs_strategy;
    std::vector<OverlayKind>     allowed_overlays;
    std::string                  decoration_pack;
    bool                         connectivity_required;
    std::vector<RequiredFixture> required_fixtures;
    std::vector<RequiredPuzzle>  required_puzzles;   // layer 7 catalog
};
```

If `dungeon_style.h` already includes a forward declaration for puzzle types or if there's a circular-include risk with `puzzles.h`, prefer a forward declaration + include in the .cpp instead. Check with a build after the edit.

- [ ] **Step 2: Build**

Run: `cmake --build build -j`
Expected: clean build. All existing style configs (`kPrecursorRuin` etc.) continue to compile; `required_puzzles` defaults to empty vector.

- [ ] **Step 3: Commit**

```bash
git add include/astra/dungeon/dungeon_style.h
git commit -m "feat(dungeon): add required_puzzles to DungeonStyle"
```

---

## Task 6: Add `puzzles_` storage + accessors on `TileMap`

**Files:**
- Modify: `include/astra/tilemap.h` (around the fixtures accessors at lines 536–544 and the private members at 626–627)
- Modify: `src/tilemap.cpp` (implementation of accessors if out-of-line; inline otherwise)

- [ ] **Step 1: Include puzzles.h in tilemap.h**

Near the other `#include` lines at the top of `include/astra/tilemap.h`:

```cpp
#include "astra/dungeon/puzzles.h"
```

- [ ] **Step 2: Add accessors to the `TileMap` public section**

Near the existing fixture accessors (around line 536):

```cpp
// Puzzle accessors
int puzzle_count() const { return static_cast<int>(puzzles_.size()); }
const astra::dungeon::PuzzleState& puzzle(int idx) const { return puzzles_[idx]; }
astra::dungeon::PuzzleState& puzzle_mut(int idx) { return puzzles_[idx]; }
const std::vector<astra::dungeon::PuzzleState>& puzzles_vec() const { return puzzles_; }
int add_puzzle(astra::dungeon::PuzzleState ps);   // returns index
void load_puzzles(std::vector<astra::dungeon::PuzzleState> ps);
astra::dungeon::PuzzleState* find_puzzle_by_id(uint16_t id);
```

- [ ] **Step 3: Add private member**

Near the existing `fixtures_` member (around line 626):

```cpp
std::vector<astra::dungeon::PuzzleState> puzzles_;
```

- [ ] **Step 4: Implement accessors in `src/tilemap.cpp`**

```cpp
int TileMap::add_puzzle(astra::dungeon::PuzzleState ps) {
    puzzles_.push_back(std::move(ps));
    return static_cast<int>(puzzles_.size() - 1);
}

void TileMap::load_puzzles(std::vector<astra::dungeon::PuzzleState> ps) {
    puzzles_ = std::move(ps);
}

astra::dungeon::PuzzleState* TileMap::find_puzzle_by_id(uint16_t id) {
    for (auto& p : puzzles_) {
        if (p.id == id) return &p;
    }
    return nullptr;
}
```

- [ ] **Step 5: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 6: Commit**

```bash
git add include/astra/tilemap.h src/tilemap.cpp
git commit -m "feat(tilemap): add puzzle storage + accessors"
```

---

## Task 7: Bump `SAVE_FILE_VERSION` and serialize new fields + puzzles

**Files:**
- Modify: `include/astra/save_file.h:28`
- Modify: `src/save_file.cpp:708-722` (fixture write), `src/save_file.cpp:1481-1500` (fixture read), plus new puzzle write/read blocks

- [ ] **Step 1: Bump version**

In `include/astra/save_file.h`:

```cpp
inline constexpr uint32_t SAVE_FILE_VERSION = 41;   // puzzle framework + FixtureData extensions
```

- [ ] **Step 2: Extend fixture write block**

In `src/save_file.cpp` (~line 708), within the existing `for (const auto& f : fixtures)` loop, after `w.write_string(f.quest_fixture_id);`, add:

```cpp
w.write_u16(f.puzzle_id);
w.write_string(f.proximity_message);
w.write_u8(f.proximity_radius);
```

- [ ] **Step 3: Extend fixture read block**

In `src/save_file.cpp` (~line 1481), within the fixture read loop, after `f.quest_fixture_id = r.read_string();`, add:

```cpp
f.puzzle_id = r.read_u16();
f.proximity_message = r.read_string();
f.proximity_radius = r.read_u8();
```

- [ ] **Step 4: Add puzzle write block**

Immediately after the fixture write block in `src/save_file.cpp`, add:

```cpp
// Puzzles (v41+)
const auto& puzzles = tm.puzzles_vec();
w.write_u32(static_cast<uint32_t>(puzzles.size()));
for (const auto& p : puzzles) {
    w.write_u16(p.id);
    w.write_u8(static_cast<uint8_t>(p.kind));
    w.write_u8(p.solved ? 1 : 0);
    w.write_u32(static_cast<uint32_t>(p.sealed_tiles.size()));
    for (const auto& [x, y] : p.sealed_tiles) {
        w.write_i32(x);
        w.write_i32(y);
    }
    w.write_i32(p.button_pos.first);
    w.write_i32(p.button_pos.second);
    w.write_i32(p.stairs_pos.first);
    w.write_i32(p.stairs_pos.second);
}
```

- [ ] **Step 5: Add puzzle read block**

Immediately after the fixture read block in `src/save_file.cpp`, add:

```cpp
// Puzzles (v41+)
{
    uint32_t puzzle_count = r.read_u32();
    std::vector<astra::dungeon::PuzzleState> puzzles(puzzle_count);
    for (auto& p : puzzles) {
        p.id = r.read_u16();
        p.kind = static_cast<astra::dungeon::PuzzleKind>(r.read_u8());
        p.solved = r.read_u8() != 0;
        uint32_t n = r.read_u32();
        p.sealed_tiles.resize(n);
        for (auto& [x, y] : p.sealed_tiles) {
            x = r.read_i32();
            y = r.read_i32();
        }
        p.button_pos.first  = r.read_i32();
        p.button_pos.second = r.read_i32();
        p.stairs_pos.first  = r.read_i32();
        p.stairs_pos.second = r.read_i32();
    }
    ms.tilemap.load_puzzles(std::move(puzzles));
}
```

- [ ] **Step 6: Verify existing old-save rejection path still fires**

`feedback_no_backcompat_pre_ship` is in force. Confirm that the save file header-version check in `src/save_file.cpp` rejects any save with `version != SAVE_FILE_VERSION`. If the current check is `version < SAVE_FILE_VERSION`, tighten it to `!=` (or at minimum, ensure v40 saves are rejected). Grep for `SAVE_FILE_VERSION` in `save_file.cpp` to locate.

- [ ] **Step 7: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 8: Smoke — save/load roundtrip**

Start a new game, save, reload. Confirm it loads successfully (empty `puzzles_` vector serializes as zero count and round-trips cleanly). Any pre-existing v40 save must be rejected with the usual version-mismatch error.

- [ ] **Step 9: Commit**

```bash
git add include/astra/save_file.h src/save_file.cpp
git commit -m "feat(save): bump SAVE_FILE_VERSION to 41, serialize puzzles + FixtureData ext"
```

---

## Task 8: Write `apply_puzzles` layer — skeleton + `SealedStairsDown` resolver (generation side)

**Files:**
- Create: `src/dungeon/puzzles.cpp`
- Modify: `include/astra/dungeon/puzzles.h` — add the `apply_puzzles` declaration

- [ ] **Step 1: Declare the layer entry point**

Append to `include/astra/dungeon/puzzles.h`:

```cpp
#include <random>

namespace astra {
class TileMap;
}

namespace astra::dungeon {

struct DungeonStyle;
struct LevelContext;

void apply_puzzles(astra::TileMap& map, const DungeonStyle& style,
                   LevelContext& ctx, std::mt19937& rng);

} // namespace astra::dungeon
```

(Adjust forward-decls to match the codebase's conventions — if `DungeonStyle` and `LevelContext` are already pulled in via `dungeon_style.h`, prefer the direct include.)

- [ ] **Step 2: Create `src/dungeon/puzzles.cpp` with the layer skeleton + `SealedStairsDown` resolver**

```cpp
#include "astra/dungeon/puzzles.h"

#include "astra/dungeon/dungeon_style.h"
#include "astra/dungeon/fixtures.h"
#include "astra/dungeon/level_context.h"
#include "astra/tilemap.h"

#include <algorithm>
#include <random>

namespace astra::dungeon {

namespace {

// Helpers -------------------------------------------------------------------

constexpr uint32_t depth_to_bit(int depth) {
    return (depth >= 1 && depth <= 32) ? (1u << (depth - 1)) : 0u;
}

bool puzzle_applies_at_depth(const RequiredPuzzle& rp, int depth) {
    return (rp.depth_mask & depth_to_bit(depth)) != 0u;
}

// Find walkable tiles inside `box` whose 4-neighbour includes at least one
// walkable tile outside `box`. Returns the OUTSIDE tile (the one we'll seal),
// de-duplicated. This is the doorway-detection step.
std::vector<std::pair<int,int>> find_exterior_doorway_tiles(
    const astra::TileMap& m, const LevelContext::Box& box)
{
    std::vector<std::pair<int,int>> out;
    static const int dxs[4] = { 1,-1, 0, 0 };
    static const int dys[4] = { 0, 0, 1,-1 };
    for (int y = box.y0; y <= box.y1; ++y) {
        for (int x = box.x0; x <= box.x1; ++x) {
            if (!m.passable(x, y)) continue;
            for (int i = 0; i < 4; ++i) {
                int nx = x + dxs[i], ny = y + dys[i];
                if (nx < 0 || ny < 0 || nx >= m.width() || ny >= m.height()) continue;
                if (box.contains(nx, ny)) continue;
                if (!m.passable(nx, ny)) continue;
                out.emplace_back(nx, ny);
            }
        }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// Choose a wall-attached cell for the button.
// Scope: regions that are not the entry region and not the sanctum region.
// Falls back to any wall-attached cell outside those, then to any open floor
// outside those — this last fallback guarantees we never fail generation.
std::pair<int,int> pick_button_position(
    const astra::TileMap& m, const LevelContext& ctx, std::mt19937& rng)
{
    std::vector<std::pair<int,int>> candidates;

    // First: wall-attached cells in side-rooms (regions != entry, != sanctum).
    for (int rid = 0; rid < m.region_count(); ++rid) {
        if (rid == ctx.entry_region_id) continue;
        if (rid == ctx.sanctum_region_id) continue;
        auto wa = region_wall_attached(m, rid);   // exposed from src/dungeon/fixtures.cpp
        candidates.insert(candidates.end(), wa.begin(), wa.end());
    }

    if (candidates.empty()) {
        // Fallback 1: any wall-attached cell outside entry+sanctum.
        for (int rid = 0; rid < m.region_count(); ++rid) {
            if (rid == ctx.entry_region_id) continue;
            if (rid == ctx.sanctum_region_id) continue;
            auto wa = region_wall_attached(m, rid);
            candidates.insert(candidates.end(), wa.begin(), wa.end());
        }
    }

    if (candidates.empty()) {
        // Fallback 2: any open floor outside entry+sanctum.
        for (int y = 0; y < m.height(); ++y) {
            for (int x = 0; x < m.width(); ++x) {
                if (!m.passable(x, y)) continue;
                int rid = m.region_id(x, y);
                if (rid == ctx.entry_region_id) continue;
                if (rid == ctx.sanctum_region_id) continue;
                candidates.emplace_back(x, y);
            }
        }
    }

    if (candidates.empty()) return { -1, -1 };   // catastrophic — map has no open cells
    std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
    return candidates[pick(rng)];
}

// SealedStairsDown resolver -------------------------------------------------

void resolve_sealed_stairs_down(
    astra::TileMap& map, const LevelContext& ctx, std::mt19937& rng,
    uint16_t puzzle_id)
{
    // 1. Identify terminal chamber via sanctum_box (set by PrecursorVault layout).
    if (ctx.sanctum_box.x0 < 0 || ctx.stairs_dn.first < 0) return;   // nothing to seal

    // 2. Find doorway seams + seal the outside tiles.
    auto seams = find_exterior_doorway_tiles(map, ctx.sanctum_box);
    if (seams.empty()) return;   // no doorway detected — defensive skip

    PuzzleState ps;
    ps.id         = puzzle_id;
    ps.kind       = PuzzleKind::SealedStairsDown;
    ps.solved     = false;
    ps.stairs_pos = ctx.stairs_dn;
    for (const auto& [sx, sy] : seams) {
        map.set_tile(sx, sy, Tile::StructuralWall);
        ps.sealed_tiles.emplace_back(sx, sy);
    }

    // 3. Place the button somewhere outside entry + sanctum.
    auto [bx, by] = pick_button_position(map, ctx, rng);
    if (bx < 0) {
        // Catastrophic fallback: revert the seal so the level stays solvable.
        for (const auto& [sx, sy] : ps.sealed_tiles) {
            map.set_tile(sx, sy, Tile::Floor);
        }
        return;
    }

    FixtureData button = make_fixture(FixtureType::PrecursorButton);
    button.puzzle_id = puzzle_id;
    map.add_fixture(bx, by, std::move(button));
    ps.button_pos = { bx, by };

    // 4. Record the puzzle.
    map.add_puzzle(std::move(ps));
}

}  // anonymous namespace

void apply_puzzles(astra::TileMap& map, const DungeonStyle& style,
                   LevelContext& ctx, std::mt19937& rng) {
    uint16_t next_id = 1;
    for (const auto& rp : style.required_puzzles) {
        if (!puzzle_applies_at_depth(rp, ctx.depth)) continue;
        const uint16_t id = next_id++;
        switch (rp.kind) {
            case PuzzleKind::SealedStairsDown:
                resolve_sealed_stairs_down(map, ctx, rng, id);
                break;
        }
    }
}

}  // namespace astra::dungeon
```

- [ ] **Step 3: Expose `region_wall_attached` if it's currently static**

The reconnaissance showed `region_wall_attached` at `src/dungeon/fixtures.cpp:122-140`. If it's in an anonymous namespace, move its declaration into `include/astra/dungeon/fixtures.h` and remove the anonymous wrapping. Sketch:

```cpp
// include/astra/dungeon/fixtures.h (add near the apply_fixtures declaration):
std::vector<std::pair<int,int>> region_wall_attached(const astra::TileMap& m, int rid);
```

Then in `src/dungeon/fixtures.cpp`, ensure the definition is outside the anonymous namespace.

- [ ] **Step 4: Add `src/dungeon/puzzles.cpp` to `CMakeLists.txt`**

Find the `astra` executable's source list in `CMakeLists.txt` and add:

```cmake
src/dungeon/puzzles.cpp
```

near the other `src/dungeon/*.cpp` entries.

- [ ] **Step 5: Build**

Run: `cmake --build build -j`
Expected: clean build. The layer exists but nothing calls it yet, so there's no behavior change.

- [ ] **Step 6: Commit**

```bash
git add include/astra/dungeon/puzzles.h src/dungeon/puzzles.cpp include/astra/dungeon/fixtures.h src/dungeon/fixtures.cpp CMakeLists.txt
git commit -m "feat(dungeon): apply_puzzles layer + SealedStairsDown resolver"
```

---

## Task 9: Wire layer 7 into `dungeon::run`

**Files:**
- Modify: `src/dungeon/pipeline.cpp:16-31`

- [ ] **Step 1: Include puzzles header**

Add to the top of `src/dungeon/pipeline.cpp`:

```cpp
#include "astra/dungeon/puzzles.h"
```

- [ ] **Step 2: Add RNG sub-seed + call after fixtures**

Replace the body of `run(...)` with:

```cpp
void run(TileMap& map, const DungeonStyle& style, const CivConfig& civ,
         const DungeonLevelSpec& spec, LevelContext& ctx) {
    auto rng_back  = sub(ctx.seed, 0xBDBDBDBDu);
    auto rng_lay   = sub(ctx.seed, 0x1A1A1A1Au);
    auto rng_con   = sub(ctx.seed, 0xC0FFEE00u);
    auto rng_ovl   = sub(ctx.seed, 0x0FEB0FEBu);
    auto rng_dec   = sub(ctx.seed, 0xDEC02011u);
    auto rng_fix   = sub(ctx.seed, 0xF12F12F1u);
    auto rng_puz   = sub(ctx.seed, 0x5EA1EDEDu);   // "sealeded"

    apply_backdrop    (map, style, civ,            rng_back);
    apply_layout      (map, style, civ, ctx,       rng_lay);
    apply_connectivity(map, style,      ctx,       rng_con);
    apply_overlays    (map, style, spec,           rng_ovl);
    apply_decoration  (map, style, civ, spec,      rng_dec);
    apply_fixtures    (map, style, civ, spec, ctx, rng_fix);
    apply_puzzles     (map, style,                 ctx, rng_puz);
}
```

- [ ] **Step 3: Build + smoke**

Run: `cmake --build build -j`
Expected: clean build.

Smoke: `./build/astra` and `:dungen precursor_ruin Precursor` on L1. Since `kPrecursorRuin.required_puzzles` is still empty (next task wires it), there should be **no visible change yet** — the layer runs but does nothing.

- [ ] **Step 4: Commit**

```bash
git add src/dungeon/pipeline.cpp
git commit -m "feat(dungeon): wire apply_puzzles as layer 7"
```

---

## Task 10: Register `SealedStairsDown` on `kPrecursorRuin` L1

**Files:**
- Modify: `src/dungeon/style_configs.cpp:25-59`

- [ ] **Step 1: Add the puzzles catalog**

Near `kPrecursorRuinRequiredFixtures` in `src/dungeon/style_configs.cpp`:

```cpp
const std::vector<RequiredPuzzle> kPrecursorRuinRequiredPuzzles = {
    { PuzzleKind::SealedStairsDown, depth_mask_bit(1) },
};
```

Inside the `kPrecursorRuin` initializer lambda, after `s.required_fixtures = kPrecursorRuinRequiredFixtures;`:

```cpp
    s.required_puzzles = kPrecursorRuinRequiredPuzzles;
```

- [ ] **Step 2: Build + smoke**

Run: `cmake --build build -j`
Expected: clean build.

Smoke: `./build/astra` in dev mode. Run `:dungen precursor_ruin Precursor` with depth 1. Expected generator output:

1. The terminal room (containing stairs_dn) has its doorway tile(s) replaced with `StructuralWall`. It should be visually sealed — the stairs_dn is unreachable.
2. A `PrecursorButton` fixture (gold `◘`) appears somewhere on the level outside the entry room and outside the sealed terminal room.
3. No crash, no error logs.

If the map editor / dumpmap command is available, run `:dumpmap` and confirm there's one puzzle record with `kind=0 (SealedStairsDown)`, `solved=false`, a `button_pos`, and a non-empty `sealed_tiles` list.

- [ ] **Step 3: Commit**

```bash
git add src/dungeon/style_configs.cpp
git commit -m "feat(archive): register SealedStairsDown puzzle on Precursor L1"
```

---

## Task 11: Implement `on_button_pressed` runtime unlock path

**Files:**
- Modify: `include/astra/dungeon/puzzles.h` — declaration
- Modify: `src/dungeon/puzzles.cpp` — implementation

- [ ] **Step 1: Declare the runtime entry point**

Append to `include/astra/dungeon/puzzles.h`:

```cpp
namespace astra { class Game; }

namespace astra::dungeon {

// Runtime unlock entry point. Invoked from dialog_manager when the player
// interacts with a PrecursorButton. `puzzle_id` comes from the fixture's
// `puzzle_id` field. Safe to call on unknown / already-solved ids (no-op).
void on_button_pressed(astra::Game& game, uint16_t puzzle_id);

}
```

- [ ] **Step 2: Implement**

Append to `src/dungeon/puzzles.cpp`:

```cpp
#include "astra/game.h"
#include "astra/world.h"   // or wherever TileMap is reached via Game

namespace astra::dungeon {

namespace {

void unlock_sealed_stairs_down(astra::Game& game, PuzzleState& ps) {
    auto& map = game.world().map();

    // 1. Unseal the doorway tiles.
    for (const auto& [x, y] : ps.sealed_tiles) {
        map.set_tile(x, y, Tile::Floor);
    }

    // 2. Swap the stairs fixture from StairsDown to StairsDownPrecursor.
    const auto [sx, sy] = ps.stairs_pos;
    if (sx >= 0 && sy >= 0) {
        int fid = map.fixture_id(sx, sy);
        if (fid >= 0) {
            map.remove_fixture(sx, sy);
        }
        FixtureData stairs = make_fixture(FixtureType::StairsDownPrecursor);
        map.add_fixture(sx, sy, std::move(stairs));
    }

    // 3. Flavor log.
    game.log("You hear a faint rumbling in the distance, rock scraping against rock. "
             "With a sudden thud it stops, the floor shakes slightly.");

    ps.solved = true;
}

}  // anonymous namespace

void on_button_pressed(astra::Game& game, uint16_t puzzle_id) {
    if (puzzle_id == 0) return;
    auto& map = game.world().map();
    auto* ps = map.find_puzzle_by_id(puzzle_id);
    if (!ps || ps->solved) return;

    switch (ps->kind) {
        case PuzzleKind::SealedStairsDown:
            unlock_sealed_stairs_down(game, *ps);
            break;
    }
}

}  // namespace astra::dungeon
```

Adjust the `game.world().map()` expression and `game.log(...)` call to match the actual `Game` API (the reconnaissance showed `game.world().map()` and log conventions; verify against `include/astra/game.h`).

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add include/astra/dungeon/puzzles.h src/dungeon/puzzles.cpp
git commit -m "feat(dungeon): on_button_pressed runtime unlock for SealedStairsDown"
```

---

## Task 12: Wire interaction — `dialog_manager.cpp` dispatch case

**Files:**
- Modify: `src/dialog_manager.cpp:310-458` (the `interact_fixture` function)

- [ ] **Step 1: Include puzzles header**

At the top of `src/dialog_manager.cpp`:

```cpp
#include "astra/dungeon/puzzles.h"
```

- [ ] **Step 2: Add interaction case**

Inside the `interact_fixture(int fid, Game& game)` switch on fixture type, add:

```cpp
case FixtureType::PrecursorButton: {
    const auto& f = game.world().map().fixture(fid);
    astra::dungeon::on_button_pressed(game, f.puzzle_id);
    break;
}
```

- [ ] **Step 3: Build + smoke (end-to-end unlock)**

Run: `cmake --build build -j`

Launch `./build/astra`, `:dungen precursor_ruin Precursor` L1. Find the button (dev-mode reveal map if needed), walk onto or adjacent to it, interact (`e` key or equivalent).

Expected:
1. Log line: *"You hear a faint rumbling in the distance, rock scraping against rock. With a sudden thud it stops, the floor shakes slightly."*
2. The sealed doorway tiles revert to floor (visible if map is revealed).
3. The stairs_dn fixture renders as **violet `>`** instead of white.
4. Walking to the stairs and pressing descend succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/dialog_manager.cpp
git commit -m "feat(dialog): route PrecursorButton interaction to puzzle unlock"
```

---

## Task 13: Add proximity-trigger pass in `Game::update`

**Files:**
- Modify: `include/astra/game.h` (~line 288, member variable block)
- Modify: `src/game_rendering.cpp:668-688` (the `Game::update()` method) — or wherever `update()` actually lives; the reconnaissance pointed here but confirm with `rg 'void Game::update'`.

- [ ] **Step 1: Add the ephemeral set member**

In `include/astra/game.h`, near other `Game` member variables:

```cpp
// Proximity-trigger state: fixture ids whose radius the player is
// currently inside. Ephemeral (not serialised).
std::unordered_set<int> proximity_fixtures_in_range_;
```

Ensure `#include <unordered_set>` is present.

- [ ] **Step 2: Add the proximity pass to `Game::update()`**

Inside `Game::update()` (after the quest journal sync, before the popup drain):

```cpp
if (state_ == GameState::Playing) {
    // ...existing quest_manager.update_quest_journals + popup drain...

    // Proximity-trigger pass: iterate fixtures near the player, fire
    // messages on entering a radius, evict on exit.
    const auto& map = world_.map();
    const int px = player_.x;
    const int py = player_.y;
    std::unordered_set<int> still_in_range;

    // Bounding box sized for the maximum proximity_radius we expect (~8).
    // Walk fixture_ids around the player rather than all fixtures.
    constexpr int kMaxRadius = 16;   // conservative upper bound
    for (int dy = -kMaxRadius; dy <= kMaxRadius; ++dy) {
        int y = py + dy;
        if (y < 0 || y >= map.height()) continue;
        for (int dx = -kMaxRadius; dx <= kMaxRadius; ++dx) {
            int x = px + dx;
            if (x < 0 || x >= map.width()) continue;
            int fid = map.fixture_id(x, y);
            if (fid < 0) continue;
            const auto& f = map.fixture(fid);
            if (f.proximity_radius == 0 || f.proximity_message.empty()) continue;
            int cheby = std::max(std::abs(dx), std::abs(dy));
            if (cheby > static_cast<int>(f.proximity_radius)) continue;
            still_in_range.insert(fid);
            if (proximity_fixtures_in_range_.find(fid) == proximity_fixtures_in_range_.end()) {
                log(f.proximity_message);
            }
        }
    }
    proximity_fixtures_in_range_ = std::move(still_in_range);
}
```

Match the `player_.x`, `world_.map()`, `log(...)`, and `state_` names to the actual `Game` class — check by grepping once.

- [ ] **Step 3: Reset the set on level transitions**

Find where `Game` handles descend/ascend/level-swap (grep `descend_stairs`, `ascend_stairs`). In each of those paths, after the new level loads, add:

```cpp
proximity_fixtures_in_range_.clear();
```

This prevents false "leaving range" behavior when the player descends and the stale fixture ids from the previous level would otherwise persist.

- [ ] **Step 4: Build + smoke**

Run: `cmake --build build -j`

Launch, `:dungen precursor_ruin Precursor` L1. Approach the button (walk within 4 tiles).

Expected: log line *"A button flashes faintly."* fires once. Walking away and back within 4 tiles fires it again. After pressing the button, walking within 4 tiles of the post-unlock stairs fires *"The stairs pulse faintly with a familiar violet light. Not Conclave work."*

- [ ] **Step 5: Commit**

```bash
git add include/astra/game.h src/game_rendering.cpp src/game.cpp
git commit -m "feat(game): fixture proximity-trigger pass in Game::update"
```

---

## Task 14: `:solve` dev command + `dumpmap` puzzle output

**Files:**
- Modify: `src/dev_console.cpp:113-186` (command dispatch) and the `dev_command_dumpmap` function (the reconnaissance pointed to `src/game.cpp:744-760`)

- [ ] **Step 1: Add `:solve` command handler**

In `src/dev_console.cpp` inside `execute_command`, add a branch near `heal`:

```cpp
else if (verb == "solve") {
    auto& map = game.world().map();
    int solved = 0;
    for (int i = 0; i < map.puzzle_count(); ++i) {
        auto& ps = map.puzzle_mut(i);
        if (ps.solved) continue;
        astra::dungeon::on_button_pressed(game, ps.id);
        ++solved;
    }
    log("Solved " + std::to_string(solved) + " puzzle(s).");
}
```

Add `#include "astra/dungeon/puzzles.h"` at the top of the file.

- [ ] **Step 2: Extend dumpmap output**

In `src/game.cpp` (around line 760, after the fixtures dump block), add:

```cpp
out << "\n--- puzzles list ---\n";
for (int i = 0; i < m.puzzle_count(); ++i) {
    const auto& p = m.puzzle(i);
    out << "  [" << i << "] id=" << p.id
        << " kind=" << static_cast<int>(p.kind)
        << " solved=" << (p.solved ? "y" : "n")
        << " button=(" << p.button_pos.first << "," << p.button_pos.second << ")"
        << " stairs=(" << p.stairs_pos.first << "," << p.stairs_pos.second << ")"
        << " sealed_tiles=" << p.sealed_tiles.size()
        << '\n';
}
```

- [ ] **Step 3: Build + smoke**

Run: `cmake --build build -j`

Launch, `:dungen precursor_ruin Precursor` L1. Try `:solve` — expect the seal to break and log line to fire. `:dumpmap` should show the puzzle with `solved=y` afterward.

- [ ] **Step 4: Commit**

```bash
git add src/dev_console.cpp src/game.cpp
git commit -m "feat(dev): :solve command + dumpmap puzzle output"
```

---

## Task 15: End-to-end validation — Archive L1 via quest entry + save/load

**Files:** none (validation task)

- [ ] **Step 1: Build**

Run: `cmake --build build -DDEV=ON -j`
Expected: zero new warnings.

- [ ] **Step 2: `:dungen` smoke**

Launch `./build/astra`, `:dungen precursor_ruin Precursor` on depths 1, 2, 3.

Expected:
- **L1:** sealed terminal room + button on the map. Press button → unlock + violet stairs.
- **L2:** unchanged (no puzzle registered for L2 via `depth_mask_bit(2)`).
- **L3:** unchanged.

- [ ] **Step 3: Archive quest playthrough**

Start a new game. Travel to Io (or whatever entry point triggers the Archive quest per current repo state). Enter the Archive, reach L1.

Expected: same behavior as the `:dungen` smoke test. Stairs to L2 are sealed until the button is pressed.

- [ ] **Step 4: Save/load mid-L1**

While on L1 with the puzzle unsolved: save. Quit. Reload.

Expected: the sealed doorway tiles remain sealed, the button remains placed, pressing it unlocks normally. Save again after solving, reload — expect stairs still violet, seal still open.

- [ ] **Step 5: Old save rejection**

Attempt to load a v40 save (if one exists from before this branch).

Expected: clean rejection with a version mismatch error.

- [ ] **Step 6: Proximity smoke**

Walk towards the button from different angles; confirm the "button flashes faintly" line fires on first enter into radius 4 and re-fires after leaving + re-entering. Repeat for the post-unlock stairs.

- [ ] **Step 7: Update `docs/roadmap.md`**

Check off the puzzle framework entry (add the line if it doesn't exist yet).

- [ ] **Step 8: Commit the roadmap update**

```bash
git add docs/roadmap.md
git commit -m "docs(roadmap): check off dungeon puzzle framework"
```

---

## Self-review notes (from plan author)

- **Spec coverage:** every numbered spec section and Definition-of-Done line has a task. The spec's "LevelContext: no new fields" is preserved (no `LevelContext` changes in any task). The spec's RNG sub-seed constant `0x5EA1EDEDu` is used in Task 9.
- **Stairs aliasing:** Task 3 requires changing 13 call sites. If any site is missed, the symptom is "post-unlock stairs don't descend" or "violet stairs disappear in some render path." Grep is the safety net.
- **`region_wall_attached` exposure:** Task 8 surfaces a previously-static helper. If this breaks something (e.g. an anonymous-namespace collision) the fix is local to `fixtures.cpp`.
- **`Game::update` location:** the reconnaissance put `Game::update()` in `src/game_rendering.cpp` — that felt surprising. Task 13 instructs the implementer to grep to confirm before editing.
- **No backcompat:** Task 7 tightens the save version check to reject non-matching versions, per `feedback_no_backcompat_pre_ship`.

---

## Definition of done (from spec)

- Layer 7 runs in `src/dungeon/puzzles.cpp`, dispatched from `pipeline.cpp`. ✓ Tasks 8, 9.
- `DungeonStyle::required_puzzles` exists; `kPrecursorRuin` registers `SealedStairsDown` on L1. ✓ Tasks 5, 10.
- `FixtureType::PrecursorButton` wired end-to-end. ✓ Task 2.
- `FixtureType::StairsDownPrecursor` wired and aliased to StairsDown behavior. ✓ Task 3.
- `:dungen precursor_ruin Precursor` L1 → sealed + buttoned; press → unlock + violet stairs + log. ✓ Tasks 10, 12.
- Archive quest playthrough reproduces. ✓ Task 15.
- Save/load mid-L1 preserves puzzle state. ✓ Tasks 7, 15.
- `SAVE_FILE_VERSION` 41; old saves rejected. ✓ Task 7.
- `:solve` + `dumpmap` extension. ✓ Task 14.
- Proximity triggers on button + stairs. ✓ Tasks 2, 3, 13.
- Zero new warnings. ✓ Task 15.
- `docs/roadmap.md` updated. ✓ Task 15.
