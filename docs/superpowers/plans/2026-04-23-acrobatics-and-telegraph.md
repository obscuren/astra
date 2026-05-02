# Acrobatics Tree & Telegraph System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild the Acrobatics skill tree with one category passive, two always-on passives, and two active abilities (one telegraphed, one self-cast), backed by a reusable telegraph system that future abilities (Suppressing Fire, grenades, Cleave refactor) can adopt. Also redesign the Character Screen skill detail panel so every skill/category lists its description + granted passives/actives Tinkering-style.

**Architecture:**
- A new `Telegraph` subsystem owns preview state, input routing, and tile-list computation for several shapes (Line, Ray, Cone, Burst, Adjacent). Abilities declare a `TelegraphSpec`; when invoked, the ability enters telegraph mode and `execute()` receives the confirmed tile list.
- Acrobatics passives emit auras/effects through the existing `make_*_ge()` + `rebuild_auras_from_sources()` pipeline. Context-dependent passives (Swiftness ranged-only, Sidestep adjacent-only) are applied inline at the two combat callsites that take player DV.
- Skill detail panel reads from a new `skill_detail()` helper that returns a structured description (passive list, active list, cooldown, gating).

**Tech Stack:** C++20, existing `Ability` + `Effect` + `Renderer` + `MenuState` framework. No new deps.

---

## File Structure

**Create:**
- `include/astra/telegraph.h` — `TelegraphSpec`, `Telegraph` class interface
- `src/telegraph.cpp` — shape tile-computation + input handling
- `tests/telegraph_test.cpp` — shape compute unit tests (if a test harness exists; otherwise defer)

**Modify:**
- `include/astra/skill_defs.h` — add Acrobatics skill IDs (Sidestep, SureFooted, AdrenalineRush) + `EffectId` entries
- `src/skill_defs.cpp` — Acrobatics category description (Tinkering-style) + 5-skill catalog
- `include/astra/effect.h` — new `EffectId`s (AcrobaticsAura, AdrenalineRush, CooldownTumble, CooldownAdrenaline), new `dv_vs_ranged` / `dv_vs_melee` field? No — keep simple: factories only
- `src/effect.cpp` — `make_acrobatics_aura_ge()`, `make_adrenaline_rush_ge()`, map new ids in `effect_for_id()`
- `include/astra/ability.h` — `Ability` gets `std::optional<TelegraphSpec> telegraph;` field
- `src/ability.cpp` — new `TumbleAbility` + `AdrenalineRushAbility`; register in catalog
- `src/game_combat.cpp` — apply `swiftness_bonus()` in `ranged_hit_player`; apply `sidestep_bonus()` in melee hit
- `include/astra/combat_system.h` / `src/game_combat.cpp` — reuse or parallel target-input model for telegraph
- `include/astra/game.h` + `src/game.cpp` — own `Telegraph telegraph_` member, call from input + render, call `apply_passive_skill_effects()` for Acrobatics category + Sidestep setup
- `src/game_input.cpp` — intercept input when `telegraph_.active()`; add ability-bar slot for Adrenaline Rush
- `src/game_rendering.cpp` — render telegraph preview overlay
- `src/character_screen.cpp` — new `build_skill_detail()` helper; redesign right-hand detail panel to show description + passive list + active list (Tinkering-style)
- `docs/roadmap.md` — mark Tumble as done (repurposed as active), add new Acrobatics entries
- `docs/formulas.md` — document Swiftness (+5 DV ranged), Sidestep (+2 DV adjacent), category +1 DV, Tumble (range 3, 25-tick CD), Adrenaline Rush (3-turn +2 DV, +1 quickness/tick, 40-tick CD)

---

## Phase 0 — Setup

### Task 0: Verify baseline build

**Files:** none

- [ ] **Step 1: Confirm clean build before any changes**

Run: `cmake -B build -DDEV=ON && cmake --build build -j`
Expected: Build succeeds.

- [ ] **Step 2: Commit nothing yet (baseline)** — purely a checkpoint.

---

## Phase 1 — Telegraph System

### Task 1: Define `TelegraphSpec` and core types

**Files:**
- Create: `include/astra/telegraph.h`

- [ ] **Step 1: Create the header**

```cpp
#pragma once

#include "astra/renderer.h"

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace astra {

class Game;

enum class TelegraphShape : uint8_t {
    Line,      // straight line from origin in one of 8 dirs, up to `range`
    Ray,       // like Line but stops at first wall/enemy
    Cone,      // widening arc from origin in a direction
    Burst,     // radius around a target tile
    Adjacent,  // 8 neighbors of origin (fixed)
};

struct TelegraphSpec {
    TelegraphShape shape = TelegraphShape::Line;
    int range = 3;            // length (line/ray), radius (burst), cone length
    int width = 1;            // cone half-width at tip
    bool diagonals = true;    // line/ray: allow 8-way dirs
    bool stop_at_wall = true;
    bool stop_at_enemy = false;
    bool require_walkable_dest = false; // Tumble needs this
};

struct TelegraphTile {
    int x;
    int y;
    bool blocked = false;     // rendered red, not a valid destination
};

struct TelegraphResult {
    std::vector<TelegraphTile> path;  // full preview
    int dest_x = -1;                  // final landing tile (for Line/Ray)
    int dest_y = -1;
};

class Telegraph {
public:
    using ConfirmFn = std::function<void(const TelegraphResult&)>;

    // Begin telegraphing. origin is the player's current tile. on_confirm
    // is called when Enter is pressed on a valid target.
    void begin(const TelegraphSpec& spec, int origin_x, int origin_y,
               ConfirmFn on_confirm);
    void cancel();

    bool active() const { return active_; }
    const TelegraphResult& preview() const { return preview_; }

    // Input: returns true if the key was consumed.
    bool handle_input(int key, Game& game);

    // Render preview tiles onto renderer. Called from game_rendering.
    void render(Renderer* renderer, int camera_x, int camera_y,
                int screen_w, int screen_h) const;

private:
    void recompute(const Game& game);

    bool active_ = false;
    TelegraphSpec spec_;
    int origin_x_ = 0;
    int origin_y_ = 0;

    // Line/Ray state
    int dir_dx_ = 0;          // 0 until first arrow press
    int dir_dy_ = 0;
    int length_ = 1;          // 1..spec.range

    // Burst state
    int cursor_x_ = 0;
    int cursor_y_ = 0;

    TelegraphResult preview_;
    ConfirmFn on_confirm_;
};

} // namespace astra
```

- [ ] **Step 2: Commit**

```bash
git add include/astra/telegraph.h
git commit -m "feat(telegraph): add TelegraphSpec/Shape/Result types + Telegraph skeleton"
```

### Task 2: Implement Line shape tile computation

**Files:**
- Create: `src/telegraph.cpp`

- [ ] **Step 1: Create `src/telegraph.cpp` with Line-only implementation**

```cpp
#include "astra/telegraph.h"

#include "astra/game.h"
#include "astra/renderer.h"
#include "astra/tilemap.h"

#include <algorithm>

namespace astra {

static bool tile_blocks_line(const Game& game, int x, int y, const TelegraphSpec& spec) {
    const auto& map = game.world().map();
    if (x < 0 || y < 0 || x >= map.width() || y >= map.height()) return true;
    if (spec.stop_at_wall && !map.passable(x, y)) return true;
    if (spec.stop_at_enemy) {
        for (const auto& npc : game.world().npcs()) {
            if (npc.alive() && npc.x == x && npc.y == y) return true;
        }
    }
    return false;
}

static void compute_line(const Game& game, int ox, int oy, int dx, int dy,
                         int length, const TelegraphSpec& spec,
                         TelegraphResult& out) {
    out.path.clear();
    out.dest_x = ox;
    out.dest_y = oy;
    for (int i = 1; i <= length; ++i) {
        int tx = ox + dx * i;
        int ty = oy + dy * i;
        bool blocked = tile_blocks_line(game, tx, ty, spec);
        out.path.push_back({tx, ty, blocked});
        if (blocked) break;
        out.dest_x = tx;
        out.dest_y = ty;
    }
}

void Telegraph::begin(const TelegraphSpec& spec, int origin_x, int origin_y,
                      ConfirmFn on_confirm) {
    spec_ = spec;
    origin_x_ = origin_x;
    origin_y_ = origin_y;
    on_confirm_ = std::move(on_confirm);
    dir_dx_ = 0;
    dir_dy_ = 0;
    length_ = 1;
    cursor_x_ = origin_x;
    cursor_y_ = origin_y;
    preview_ = {};
    active_ = true;
}

void Telegraph::cancel() {
    active_ = false;
    on_confirm_ = nullptr;
    preview_ = {};
}

void Telegraph::recompute(const Game& game) {
    preview_.path.clear();
    preview_.dest_x = origin_x_;
    preview_.dest_y = origin_y_;

    if (spec_.shape == TelegraphShape::Line ||
        spec_.shape == TelegraphShape::Ray) {
        if (dir_dx_ == 0 && dir_dy_ == 0) return; // waiting for first direction
        compute_line(game, origin_x_, origin_y_, dir_dx_, dir_dy_, length_, spec_, preview_);
    }
    // Ray/Cone/Burst/Adjacent implemented in later tasks.
}

} // namespace astra
```

- [ ] **Step 2: Add to CMakeLists.txt**

Open `CMakeLists.txt`, find the `set(ASTRA_SOURCES` list, add `src/telegraph.cpp` alongside `src/ability.cpp`.

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add include/astra/telegraph.h src/telegraph.cpp CMakeLists.txt
git commit -m "feat(telegraph): implement Line shape tile computation"
```

### Task 3: Telegraph input handling (direction picker for Line)

**Files:**
- Modify: `src/telegraph.cpp`

- [ ] **Step 1: Add `handle_input()` implementation**

Append to `src/telegraph.cpp`:

```cpp
bool Telegraph::handle_input(int key, Game& game) {
    if (!active_) return false;

    auto set_dir = [&](int ndx, int ndy) {
        // Perpendicular switch -> reset length. Same/opposite -> keep.
        bool same_axis = (ndx == dir_dx_ && ndy == dir_dy_) ||
                         (ndx == -dir_dx_ && ndy == -dir_dy_);
        if (!same_axis || (dir_dx_ == 0 && dir_dy_ == 0)) {
            dir_dx_ = ndx;
            dir_dy_ = ndy;
            length_ = 1;
            return;
        }
        // Same direction -> extend; opposite -> retract.
        if (ndx == dir_dx_ && ndy == dir_dy_) {
            if (length_ < spec_.range) ++length_;
        } else {
            if (length_ > 1) --length_;
        }
    };

    switch (key) {
        case 'k': case KEY_UP:    set_dir( 0, -1); break;
        case 'j': case KEY_DOWN:  set_dir( 0,  1); break;
        case 'h': case KEY_LEFT:  set_dir(-1,  0); break;
        case 'l': case KEY_RIGHT: set_dir( 1,  0); break;
        case 'y': set_dir(-1, -1); break;
        case 'u': set_dir( 1, -1); break;
        case 'b': set_dir(-1,  1); break;
        case 'n': set_dir( 1,  1); break;
        case '\n': case '\r': {
            recompute(game);
            // Valid if path non-empty, final tile not blocked, dest walkable if required.
            bool ok = !preview_.path.empty() &&
                      !preview_.path.back().blocked;
            if (ok && spec_.require_walkable_dest) {
                ok = game.world().map().passable(preview_.dest_x, preview_.dest_y);
            }
            if (!ok) {
                game.log("Invalid destination.");
                return true;
            }
            auto cb = std::move(on_confirm_);
            active_ = false;
            if (cb) cb(preview_);
            return true;
        }
        case '\033': // Esc
            cancel();
            game.log("Cancelled.");
            return true;
        default:
            return false;
    }

    recompute(game);
    return true;
}
```

- [ ] **Step 2: Build**

Run: `cmake --build build -j`
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add src/telegraph.cpp
git commit -m "feat(telegraph): direction-picker input with tap-to-extend"
```

### Task 4: Telegraph preview rendering

**Files:**
- Modify: `src/telegraph.cpp`
- Modify: `src/game_rendering.cpp`

- [ ] **Step 1: Implement `Telegraph::render()`**

Append to `src/telegraph.cpp`:

```cpp
void Telegraph::render(Renderer* r, int camera_x, int camera_y,
                       int screen_w, int screen_h) const {
    if (!active_ || !r) return;
    for (size_t i = 0; i < preview_.path.size(); ++i) {
        const auto& t = preview_.path[i];
        int sx = t.x - camera_x;
        int sy = t.y - camera_y;
        if (sx < 0 || sy < 0 || sx >= screen_w || sy >= screen_h) continue;
        Color col = t.blocked ? Color::Red : Color::Cyan;
        // Landing tile (last non-blocked tile) drawn brighter.
        bool is_dest = (!t.blocked &&
                        t.x == preview_.dest_x && t.y == preview_.dest_y);
        char glyph = is_dest ? 'X' : '+';
        r->put_overlay(sx, sy, glyph, col);
    }
}
```

- [ ] **Step 2: Confirm `put_overlay` exists; if not, use `put`**

Run: `grep -n "put_overlay\|void put(" include/astra/renderer.h`
If `put_overlay` is absent, replace with `r->put(sx, sy, glyph, col);`.

- [ ] **Step 3: Hook into main render path**

Open `src/game_rendering.cpp`, find where other overlays render (near combat targeting reticle — search for `combat_.targeting()`). After the map + NPC layer render, call:

```cpp
telegraph_.render(renderer_.get(), camera_x_, camera_y_, view_w, view_h);
```

Use the same camera/screen variables the existing reticle uses.

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add src/telegraph.cpp src/game_rendering.cpp
git commit -m "feat(telegraph): render preview overlay with highlighted landing tile"
```

### Task 5: Wire Telegraph into Game + input

**Files:**
- Modify: `include/astra/game.h`
- Modify: `src/game.cpp` (ctor init list if needed — `Telegraph` is default-constructible)
- Modify: `src/game_input.cpp`

- [ ] **Step 1: Add member to `Game`**

In `include/astra/game.h` near other subsystem members (e.g., after `CombatSystem combat_;`):

```cpp
#include "astra/telegraph.h"
// ...
Telegraph telegraph_;
```

Also add a public accessor:

```cpp
Telegraph& telegraph() { return telegraph_; }
```

- [ ] **Step 2: Intercept input**

In `src/game_input.cpp`, just above the existing `combat_.targeting()` block (line ~221):

```cpp
// Telegraph mode intercept
if (telegraph_.active()) {
    telegraph_.handle_input(key, *this);
    return;
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: Build succeeds (no callers yet — the system is dormant).

- [ ] **Step 4: Commit**

```bash
git add include/astra/game.h src/game_input.cpp
git commit -m "feat(telegraph): wire Telegraph subsystem into Game"
```

---

## Phase 2 — Acrobatics Skill Data

### Task 6: Add new SkillIds and EffectIds

**Files:**
- Modify: `include/astra/skill_defs.h`
- Modify: `include/astra/effect.h`

- [ ] **Step 1: Extend `SkillId`**

In `include/astra/skill_defs.h`, under the Acrobatics block:

```cpp
    // Acrobatics
    Swiftness = 100,
    Tumble = 101,
    Sidestep = 102,
    SureFooted = 103,
    AdrenalineRush = 104,
```

- [ ] **Step 2: Extend `EffectId`**

In `include/astra/effect.h`:

```cpp
    Acrobatics       = 12,   // always-on: +1 DV from Cat_Acrobatics
    AdrenalineRush   = 13,   // 3-turn active buff
    // ...ability cooldowns:
    CooldownTumble      = 106,
    CooldownAdrenaline  = 107,
```

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add include/astra/skill_defs.h include/astra/effect.h
git commit -m "feat(acrobatics): add SkillIds + EffectIds for new Acrobatics skills"
```

### Task 7: Acrobatics category passive effect (+1 DV)

**Files:**
- Modify: `include/astra/effect.h`
- Modify: `src/effect.cpp`
- Modify: `src/game.cpp`

- [ ] **Step 1: Factory**

In `include/astra/effect.h`, add near `make_thick_skin_ge()`:

```cpp
Effect make_acrobatics_ge();
Effect make_adrenaline_rush_ge(int duration);
```

In `src/effect.cpp`:

```cpp
Effect make_acrobatics_ge() {
    Effect e;
    e.id = EffectId::Acrobatics;
    e.name = "Acrobatics";
    e.color = Color::Cyan;
    e.duration = -1;
    e.remaining = -1;
    e.show_in_bar = false;
    e.modifiers.dv = 1;
    return e;
}

Effect make_adrenaline_rush_ge(int duration) {
    Effect e;
    e.id = EffectId::AdrenalineRush;
    e.name = "Adrenaline Rush";
    e.color = Color::Red;
    e.duration = duration;
    e.remaining = duration;
    e.show_in_bar = true;
    e.modifiers.dv = 2;
    e.modifiers.quickness = 25;  // +25% energy gain while active
    return e;
}
```

- [ ] **Step 2: Apply category passive**

In `src/game.cpp` `apply_passive_skill_effects()`:

```cpp
if (player_has_skill(player_, SkillId::Cat_Acrobatics) &&
    !has_effect(player_.effects, EffectId::Acrobatics)) {
    add_effect(player_.effects, make_acrobatics_ge());
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add include/astra/effect.h src/effect.cpp src/game.cpp
git commit -m "feat(acrobatics): category passive grants +1 DV via make_acrobatics_ge"
```

### Task 8: Inline context passives (Swiftness ranged, Sidestep adjacent)

**Files:**
- Modify: `src/game_combat.cpp`

- [ ] **Step 1: Helpers at the top of the anonymous namespace block**

Below the existing `weapon_skill_bonus()` function in `src/game_combat.cpp`:

```cpp
static int swiftness_bonus(const Player& player) {
    return player_has_skill(player, SkillId::Swiftness) ? 5 : 0;
}

static int sidestep_bonus(const Player& player, const Game& game) {
    if (!player_has_skill(player, SkillId::Sidestep)) return 0;
    int px = player.x, py = player.y;
    for (const auto& npc : game.world().npcs()) {
        if (!npc.alive()) continue;
        if (!is_hostile_to_player(npc.faction, player)) continue;
        int dx = std::abs(npc.x - px), dy = std::abs(npc.y - py);
        if (std::max(dx, dy) == 1) return 2;
    }
    return 0;
}
```

- [ ] **Step 2: Apply at ranged-hit-player site**

In `ranged_hit_player()` (line ~205), replace:

```cpp
    if (natural != 20 && attack_roll < game.player().effective_dv()) {
```

with:

```cpp
    int player_dv = game.player().effective_dv() + swiftness_bonus(game.player());
    if (natural != 20 && attack_roll < player_dv) {
```

Also update the log-line below to use `player_dv` where previously `effective_dv()` appeared (if any).

- [ ] **Step 3: Apply at melee-hit-player site**

In `process_npc_turn()`, at the melee branch (line ~397):

```cpp
    int player_dv = game.player().effective_dv() + sidestep_bonus(game.player(), game);
    if (natural != 20 && attack_roll < player_dv) {
```

- [ ] **Step 4: Build + smoke test**

Run: `cmake --build build -j`
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add src/game_combat.cpp
git commit -m "feat(acrobatics): apply Swiftness (ranged) and Sidestep (melee adjacent) at attack resolution"
```

### Task 9: SureFooted — faster movement

**Files:**
- Modify: `src/game_interaction.cpp`

**Design note:** No tile-level move-cost system exists in dungeons. Redefine Sure-Footed as **−10% dungeon move action cost** (player only), which fits the "agile movement" fantasy and has a concrete hook. Overworld travel is already cut by terrain lore; leave that alone.

- [ ] **Step 1: Compute effective move cost**

At the top of `src/game_interaction.cpp` (after includes):

```cpp
static int move_action_cost(const Player& player) {
    int cost = ActionCost::move;
    if (player_has_skill(player, SkillId::SureFooted)) {
        cost = (cost * 9) / 10;   // -10%
    }
    return cost;
}
```

(Requires `#include "astra/skill_defs.h"` if not already present.)

- [ ] **Step 2: Replace the four `advance_world(ActionCost::move)` calls in `try_move()`**

Swap each with `advance_world(move_action_cost(player_));` — lines ~110, ~122, ~164 in `src/game_interaction.cpp`. Check whether line ~478 in `src/game_rendering.cpp` is player movement; if so, swap there too.

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/game_interaction.cpp src/game_rendering.cpp
git commit -m "feat(acrobatics): SureFooted reduces dungeon move action cost by 10%"
```

### Task 10: Skill catalog — expand Acrobatics tree

**Files:**
- Modify: `src/skill_defs.cpp`

- [ ] **Step 1: Add helper for Acrobatics category description (Tinkering-style)**

Near `tinkering_category_description()`:

```cpp
static std::string acrobatics_category_description() {
    std::string s = "Mastery of agile movement and evasion in any environment.\n\n";
    s += colored("Passive:", Color::White);
    s += " +";
    s += colored("1 DV", Color::Cyan);
    s += " while this category is learned.";
    return s;
}
```

- [ ] **Step 2: Replace the Acrobatics block in `skill_catalog()`**

```cpp
{SkillId::Cat_Acrobatics, "Acrobatics",
 acrobatics_category_description(), 50, {
    {SkillId::Swiftness, "Swiftness",
     "You get +5 bonus to DV when attacked with ranged weapons.",
     true, 50, 0, nullptr},
    {SkillId::Sidestep, "Sidestep",
     "You get +2 DV while at least one hostile is adjacent. "
     "Stacks with Swiftness against adjacent ranged foes.",
     true, 75, 13, "Agility"},
    {SkillId::SureFooted, "Sure-Footed",
     "Your movement is lithe and efficient. Dungeon move "
     "actions cost 10% less time.",
     true, 75, 15, "Agility"},
    {SkillId::Tumble, "Tumble",
     "Dash up to 3 tiles in any direction, ignoring anything "
     "in between. Telegraphed. 25-tick cooldown.",
     false, 100, 17, "Agility"},
    {SkillId::AdrenalineRush, "Adrenaline Rush",
     "Self-cast surge: +2 DV and +25% quickness for 3 ticks. "
     "40-tick cooldown.",
     false, 150, 14, "Willpower"},
}},
```

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/skill_defs.cpp
git commit -m "feat(acrobatics): expand tree to 5 skills with Tinkering-style category passive block"
```

---

## Phase 3 — Tumble Ability (telegraphed)

### Task 11: `Ability` holds an optional `TelegraphSpec`

**Files:**
- Modify: `include/astra/ability.h`
- Modify: `src/ability.cpp` (no behavior change yet)

- [ ] **Step 1: Add field**

In `include/astra/ability.h`:

```cpp
#include "astra/telegraph.h"
// ...
#include <optional>
// ...
class Ability {
public:
    // ...existing members...
    std::optional<TelegraphSpec> telegraph;  // nullopt = instant, no preview

    // Called instead of execute() when telegraph is set. Default: no-op.
    virtual bool execute_telegraphed(Game&, const TelegraphResult&) { return false; }
};
```

- [ ] **Step 2: Build**

Run: `cmake --build build -j`
Expected: Build succeeds (no callers yet).

- [ ] **Step 3: Commit**

```bash
git add include/astra/ability.h
git commit -m "feat(ability): optional TelegraphSpec on Ability + execute_telegraphed hook"
```

### Task 12: Update `use_ability()` to route through Telegraph

**Files:**
- Modify: `src/ability.cpp`

- [ ] **Step 1: Rework the tail of `use_ability()`**

Replace the block that currently calls `ability->execute(...)` → cooldown → advance. New code:

```cpp
    auto finalize = [&]() {
        add_effect(game.player().effects, make_cooldown(
            ability->cooldown_effect, ability->name,
            ability->effective_cooldown(game.player())));
        game.advance_world(ability->action_cost);
    };

    if (ability->telegraph.has_value()) {
        game.telegraph().begin(
            *ability->telegraph,
            game.player().x, game.player().y,
            [ability, finalize, &game](const TelegraphResult& res) {
                if (!ability->execute_telegraphed(game, res)) {
                    game.log(ability->name + " failed.");
                    return;
                }
                finalize();
            });
        game.log(ability->name + ": pick a direction, Enter to dash, Esc to cancel.");
        return true;
    }

    if (!ability->execute(game, target)) {
        return false;
    }
    finalize();
    return true;
```

- [ ] **Step 2: Build**

Run: `cmake --build build -j`
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add src/ability.cpp
git commit -m "feat(ability): route telegraphed abilities through Telegraph subsystem"
```

### Task 13: Tumble ability definition

**Files:**
- Modify: `src/ability.cpp`

- [ ] **Step 1: New TumbleAbility class**

In `src/ability.cpp`, after `IntimidateAbility`:

```cpp
class TumbleAbility : public Ability {
public:
    TumbleAbility() {
        skill_id = SkillId::Tumble;
        name = "Tumble";
        description = "Dash up to 3 tiles, ignoring anything in between.";
        cooldown_ticks = 25;
        cooldown_effect = EffectId::CooldownTumble;
        needs_adjacent_target = false;
        required_weapon = WeaponClass::None;
        action_cost = 50;
        telegraph = TelegraphSpec{
            .shape = TelegraphShape::Line,
            .range = 3,
            .width = 1,
            .diagonals = true,
            .stop_at_wall = true,
            .stop_at_enemy = false,        // tumble flies over enemies
            .require_walkable_dest = true,
        };
    }

    bool execute(Game& /*game*/, Npc* /*target*/) override { return false; }

    bool execute_telegraphed(Game& game, const TelegraphResult& res) override {
        if (res.dest_x < 0 || res.dest_y < 0) return false;
        if (res.dest_x == game.player().x && res.dest_y == game.player().y) return false;
        // NPC swap not handled — tumble teleports over them; final tile must
        // be clear (TelegraphSpec::require_walkable_dest already ensures
        // passable; also block if an NPC occupies it).
        for (const auto& npc : game.world().npcs()) {
            if (npc.alive() && npc.x == res.dest_x && npc.y == res.dest_y) {
                game.log("Landing blocked by " + npc.label() + ".");
                return false;
            }
        }
        game.player().x = res.dest_x;
        game.player().y = res.dest_y;
        game.log("You tumble to (" + std::to_string(res.dest_x) + "," +
                 std::to_string(res.dest_y) + ").");
        return true;
    }
};
```

- [ ] **Step 2: Register in catalog**

```cpp
cat.push_back(std::make_unique<TumbleAbility>());
```

- [ ] **Step 3: Verify recompute_fov + compute_camera are reachable after tumble**

Tumble moves the player but the ability layer may not call FOV/camera refresh. After the player x/y assignment, add:

```cpp
        game.recompute_fov_and_camera();
```

If no such method exists, expose a thin wrapper on `Game` that wraps the two internals (`recompute_fov()` + `compute_camera()`) — both already exist from `try_move()`.

Run: `grep -n "recompute_fov\|compute_camera" include/astra/game.h` to confirm their access level.

If they are private, add a public helper:

```cpp
// include/astra/game.h (public section)
void refresh_view() { recompute_fov(); compute_camera(); }
```

And call `game.refresh_view()` after the tumble move.

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: Build succeeds.

- [ ] **Step 5: Manual smoke test**

Run: `./build/astra-dev --term`
Expected: From a fresh dev save, learn Cat_Acrobatics + Tumble; equip no weapon constraint; press ability slot bound to Tumble; preview appears; arrow keys pick direction; Enter teleports player.

- [ ] **Step 6: Commit**

```bash
git add src/ability.cpp include/astra/game.h
git commit -m "feat(acrobatics): Tumble active — 3-tile telegraphed dash, 25-tick cooldown"
```

---

## Phase 4 — Adrenaline Rush (self-cast)

### Task 14: AdrenalineRushAbility

**Files:**
- Modify: `src/ability.cpp`

- [ ] **Step 1: New class + register**

After `TumbleAbility`:

```cpp
class AdrenalineRushAbility : public Ability {
public:
    AdrenalineRushAbility() {
        skill_id = SkillId::AdrenalineRush;
        name = "Adrenaline Rush";
        description = "+2 DV and +25% quickness for 3 ticks.";
        cooldown_ticks = 40;
        cooldown_effect = EffectId::CooldownAdrenaline;
        needs_adjacent_target = false;
        required_weapon = WeaponClass::None;
        action_cost = 25;
    }

    bool execute(Game& game, Npc* /*target*/) override {
        add_effect(game.player().effects, make_adrenaline_rush_ge(3));
        game.log("Adrenaline floods your system!");
        return true;
    }
};
```

And:

```cpp
cat.push_back(std::make_unique<AdrenalineRushAbility>());
```

- [ ] **Step 2: Build**

Run: `cmake --build build -j`
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add src/ability.cpp
git commit -m "feat(acrobatics): Adrenaline Rush self-cast — 3-turn buff, 40-tick cooldown"
```

---

## Phase 5 — Skill Detail Panel Redesign

### Task 15: `skill_detail()` helper returns structured description

**Files:**
- Modify: `include/astra/skill_defs.h`
- Modify: `src/skill_defs.cpp`

- [ ] **Step 1: Add types + helper**

In `include/astra/skill_defs.h`:

```cpp
struct SkillDetail {
    std::string header;                    // name + [Passive]/[Active]
    std::string body;                      // main description
    std::vector<std::string> passives;     // "• +1 DV (category unlock)"
    std::vector<std::string> actives;      // "• Tumble — 25-tick CD"
    std::string cost_line;                 // "50 SP"
    std::string requirement_line;          // "Requires 15 Agility" or ""
};

// Build display details for a single skill OR a category (pass category->unlock_id).
SkillDetail skill_detail(SkillId id);
```

In `src/skill_defs.cpp` (after catalog):

```cpp
SkillDetail skill_detail(SkillId id) {
    SkillDetail d;
    const auto& cat_list = skill_catalog();

    // Is this a category unlock id?
    for (const auto& cat : cat_list) {
        if (cat.unlock_id == id) {
            d.header = cat.name + " [Category]";
            d.body = cat.description;
            d.cost_line = std::to_string(cat.sp_cost) + " SP";
            // List child skills grouped by passive/active.
            for (const auto& sk : cat.skills) {
                std::string line = "• " + sk.name + " — " + std::to_string(sk.sp_cost) + " SP";
                if (sk.passive) d.passives.push_back(line);
                else d.actives.push_back(line);
            }
            return d;
        }
    }

    // Individual skill
    const SkillDef* sk = find_skill(id);
    if (!sk) { d.header = "Unknown Skill"; return d; }
    d.header = sk->name + (sk->passive ? " [Passive]" : " [Active]");
    d.body = sk->description;
    d.cost_line = std::to_string(sk->sp_cost) + " SP";
    if (sk->attribute_req > 0 && sk->attribute_name) {
        d.requirement_line = "Requires " + std::to_string(sk->attribute_req) +
                             " " + sk->attribute_name;
    }
    return d;
}
```

- [ ] **Step 2: Build**

Run: `cmake --build build -j`
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add include/astra/skill_defs.h src/skill_defs.cpp
git commit -m "feat(skills): skill_detail() helper — structured description + child-skill list for categories"
```

### Task 16: Render structured detail in the Skills tab right pane

**Files:**
- Modify: `src/character_screen.cpp`

- [ ] **Step 1: Replace the detail-panel render block in `draw_skills()`**

Current code has separate `if (selected_cat_idx >= 0 && !selected_skill) { ... } else if (selected_skill) { ... }` branches (lines ~1717–1760). Replace both with a single structured render driven by `skill_detail()`:

```cpp
    SkillId detail_id = SkillId::Cat_Acrobatics;  // default no-op
    bool have = false;
    if (selected_cat_idx >= 0 && !selected_skill) {
        detail_id = catalog[selected_cat_idx].unlock_id;
        have = true;
    } else if (selected_skill) {
        detail_id = selected_skill->id;
        have = true;
    }
    if (!have) return;

    SkillDetail det = skill_detail(detail_id);
    int dy = 2;
    ctx.text({.x = rx, .y = dy++, .content = det.header, .tag = UITag::TextBright});
    if (!det.cost_line.empty())
        ctx.text({.x = rx, .y = dy++, .content = det.cost_line, .tag = UITag::TextWarning});
    if (!det.requirement_line.empty())
        ctx.text({.x = rx, .y = dy++, .content = det.requirement_line, .tag = UITag::TextDim});
    dy++;
    dy = wrap_text(dy, det.body, Color::DarkGray);
    dy++;

    if (!det.passives.empty()) {
        ctx.text({.x = rx, .y = dy++, .content = "Passives:", .tag = UITag::TextAccent});
        for (const auto& p : det.passives) {
            ctx.text({.x = rx, .y = dy++, .content = p, .tag = UITag::TextDefault});
        }
        dy++;
    }
    if (!det.actives.empty()) {
        ctx.text({.x = rx, .y = dy++, .content = "Actives:", .tag = UITag::TextAccent});
        for (const auto& a : det.actives) {
            ctx.text({.x = rx, .y = dy++, .content = a, .tag = UITag::TextDefault});
        }
    }
```

- [ ] **Step 2: Build + smoke test**

Run: `cmake --build build -j && ./build/astra-dev --term`
Expected: Skills tab — selecting Acrobatics category shows category description plus bullet lists of passive/active children. Selecting an individual skill shows its single detail.

- [ ] **Step 3: Commit**

```bash
git add src/character_screen.cpp
git commit -m "feat(skills): unified detail panel showing description + passive/active lists"
```

---

## Phase 6 — Docs & Roadmap

### Task 17: Update roadmap and formulas

**Files:**
- Modify: `docs/roadmap.md`
- Modify: `docs/formulas.md`

- [ ] **Step 1: Roadmap edits**

In `docs/roadmap.md`:

- Mark `Tumble` done (rewrite description: "active 3-tile telegraphed dash, 25-tick cooldown")
- Add new entries:
  - `[x] Swiftness — +5 DV vs ranged attacks`
  - `[x] Sidestep — +2 DV while adjacent to hostile`
  - `[x] Sure-Footed — −10% dungeon move cost`
  - `[x] Adrenaline Rush — self-cast +2 DV + 25% quickness, 3 ticks, 40-tick CD`

- [ ] **Step 2: Formulas edits**

Append to `docs/formulas.md` under the skill effects section:

```
### Acrobatics
- Cat_Acrobatics: +1 DV (always-on, via make_acrobatics_ge)
- Swiftness: +5 DV to ranged attack resolution only
- Sidestep: +2 DV when any adjacent tile holds a hostile NPC, melee resolution only
- Sure-Footed: dungeon move cost = floor(ActionCost::move * 9 / 10) = 45 (vs 50)
- Tumble: dash up to range 3 in one of 8 directions; ignores enemies in path; landing tile must be passable and unoccupied; cooldown 25 ticks
- Adrenaline Rush: +2 DV, +25 quickness for 3 ticks; cooldown 40 ticks; self-cast
```

- [ ] **Step 3: Commit**

```bash
git add docs/roadmap.md docs/formulas.md
git commit -m "docs(acrobatics): roadmap + formulas for new Acrobatics tree and telegraph system"
```

---

## Self-Review Checklist

- [ ] **Spec coverage** — every skill in the 5-skill list has an implementation task: Swiftness (Task 8), Sidestep (Task 8), Sure-Footed (Task 9), Tumble (Tasks 11–13), Adrenaline Rush (Task 14), category passive (Task 7). Telegraph system backs Tumble (Tasks 1–5). Detail panel covered (Tasks 15–16).
- [ ] **Placeholder scan** — no TBDs; every code step shows the change.
- [ ] **Type consistency** — `TelegraphSpec`, `TelegraphResult`, `SkillDetail` fields all reused verbatim across tasks. `make_acrobatics_ge()` name used in header and source and game.cpp.

## Notes on Deferred Work

- **Ray, Cone, Burst, Adjacent** telegraph shapes are declared in `TelegraphSpec` but not yet implemented in `Telegraph::recompute`. Next consumer (Suppressing Fire → Cone, grenades → Burst, Cleave refactor → Adjacent) will fill them in.
- Tumble's traversed tiles are purely cosmetic — no trap-triggering or reaction pass on the intermediate tiles. This matches the "commit to the move" feel.
- The existing `combat_` targeting path is NOT consolidated into Telegraph in this plan. It can be migrated in a follow-up once we have more telegraphed abilities using the new system.
