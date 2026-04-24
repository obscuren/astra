# Ability Bar — Rows & Persistent Assignment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the single-row, catalog-derived ability bar with a persistent, row-paged hotbar backed by a flat `Player::ability_slots` vector. Auto-assign on learn, compact on remove, PageUp/PageDown paging with wrap.

**Architecture:** A new `ability_bar` module owns paging math and the `use_slot` pipeline. `Player` gains an `ability_slots` vector persisted via save bump (v44 → v45, old saves rejected). Skill-grant sites are funneled through a single `grant_skill` helper that keeps the bar in sync.

**Tech Stack:** C++20, existing `Renderer` + `UIContext` + `MenuState` framework, existing save-file schema. No new deps.

**Testing convention:** This project has no automated test harness. Every task's verification step is a manual smoke test: build with `-DDEV=ON`, run `./build/astra`, start a Dev Commander character (main menu → "Dev Commander"), and drive the dev console to observe the behavior. The dev console opens with backtick.

**Spec:** `docs/superpowers/specs/2026-04-24-ability-bar-rows-design.md`

---

## File Structure

**Create:**
- `include/astra/ability_bar.h` — paging math, `use_slot`, grant/revoke hooks, constants
- `src/ability_bar.cpp` — implementation
- `include/astra/skill_grant.h` — `grant_skill` / `revoke_skill` helpers (single "skill now granted" entry point)
- `src/skill_grant.cpp` — implementation

**Modify:**
- `include/astra/player.h` — add `std::vector<SkillId> ability_slots;`
- `include/astra/ability.h` — remove `get_ability_bar` declaration; remove or thin-forward `use_ability`
- `src/ability.cpp` — move the body of `use_ability` into `ability_bar::use_slot`; drop `get_ability_bar`
- `include/astra/game.h` — add `int ability_bar_row_ = 0;`
- `src/game.cpp` — `compute_layout` changes `vrows[7]` to `fixed(3)`; dev-commander skill grant uses `grant_skill`; character creation uses `grant_skill` per starting skill
- `src/character_screen.cpp` — category-unlock + skill-learn sites call `grant_skill`
- `src/game_input.cpp` — replace `'1'..'5'` handler with `'1'..('0' + kSlotsPerRow)`; add `KEY_PAGE_UP` / `KEY_PAGE_DOWN` cases
- `src/game_rendering.cpp` — rewrite `render_abilities_bar` to 3-row layout
- `src/save_file.cpp` — read/write `ability_slots`
- `include/astra/save_file.h` — bump `SAVE_FILE_VERSION` to 45

---

## Phase 0 — Baseline

### Task 0: Verify clean build

**Files:** none

- [ ] **Step 1: Confirm clean build before changes**

Run:
```bash
cmake -B build -DDEV=ON && cmake --build build -j
```
Expected: Build succeeds with no warnings or errors.

- [ ] **Step 2: Confirm baseline behavior**

Run: `./build/astra`, start a Dev Commander character. Verify the bottom of the screen shows the current single-line abilities bar with `<1> Jab  <2> Cleave  <3> Quickdraw  <4> Intimidate  <5> Tumble` (or similar — first 5 catalog skills the dev character has). Press `1` to use an ability — confirm it fires (adjacent-target message, or effect). Quit.

---

## Phase 1 — Constants & module scaffolding

### Task 1: Create `ability_bar` module with constants and row-count math

**Files:**
- Create: `include/astra/ability_bar.h`
- Create: `src/ability_bar.cpp`

This first task builds the pure-data part of the module — constants and read-only helpers that don't touch `Game` or `Player::ability_slots` yet (we add that field in Phase 2). We have it reference the field via forward contract, and implement it reading a `const std::vector<SkillId>&` passed explicitly so the file compiles before Phase 2 lands.

- [ ] **Step 1: Create the header**

Write `include/astra/ability_bar.h`:

```cpp
#pragma once

#include "astra/skill_defs.h"

#include <optional>
#include <vector>

namespace astra {

class Game;
struct Player;

namespace ability_bar {

inline constexpr int kSlotsPerRow    = 4;
inline constexpr int kMaxSlotsPerRow = 9;
inline constexpr int kMaxRows        = 9;
static_assert(kSlotsPerRow >= 1 && kSlotsPerRow <= kMaxSlotsPerRow,
              "kSlotsPerRow must be in [1, kMaxSlotsPerRow]");

// Read-only helpers operating on a slot list.
int  row_count(const std::vector<SkillId>& slots);   // always >= 1
std::optional<SkillId> slot_at(const std::vector<SkillId>& slots, int row, int col);

// Read-only helpers operating on a Player.
int  row_count(const Player& player);
std::optional<SkillId> slot_at(const Player& player, int row, int col);

// Mutators — defined here, no-op / minimal bodies until Phase 2 wires
// ability_slots onto Player. Once wired, assign pushes / remove erases.
bool assign_on_learn   (Player& player, SkillId id);
bool remove_and_compact(Player& player, SkillId id);

// Paging: wrap around.
void page_up  (int& visible_row, const Player& player);
void page_down(int& visible_row, const Player& player);

// Clamp a visible-row index after a learn/remove that could change row_count.
void clamp_visible_row(int& visible_row, const Player& player);

// Execute the ability in (visible_row, col) of the player's bar.
bool use_slot(Game& game, int visible_row, int col);

} // namespace ability_bar
} // namespace astra
```

- [ ] **Step 2: Create the implementation with the read-only helpers working**

Write `src/ability_bar.cpp`:

```cpp
#include "astra/ability_bar.h"

#include "astra/player.h"

#include <algorithm>

namespace astra::ability_bar {

int row_count(const std::vector<SkillId>& slots) {
    if (slots.empty()) return 1;
    const int n = static_cast<int>(slots.size());
    return (n + kSlotsPerRow - 1) / kSlotsPerRow;
}

std::optional<SkillId> slot_at(const std::vector<SkillId>& slots, int row, int col) {
    if (row < 0 || col < 0 || col >= kSlotsPerRow) return std::nullopt;
    const int idx = row * kSlotsPerRow + col;
    if (idx < 0 || idx >= static_cast<int>(slots.size())) return std::nullopt;
    return slots[idx];
}

// Player-based overloads (stubbed until Phase 2 adds ability_slots).
// Phase 2 replaces these bodies with the ability_slots reads.
int row_count(const Player& /*player*/) {
    return 1;
}

std::optional<SkillId> slot_at(const Player& /*player*/, int /*row*/, int /*col*/) {
    return std::nullopt;
}

bool assign_on_learn(Player& /*player*/, SkillId /*id*/) {
    return false;
}

bool remove_and_compact(Player& /*player*/, SkillId /*id*/) {
    return false;
}

void page_up(int& visible_row, const Player& player) {
    const int n = row_count(player);
    if (n <= 1) return;
    visible_row = (visible_row - 1 + n) % n;
}

void page_down(int& visible_row, const Player& player) {
    const int n = row_count(player);
    if (n <= 1) return;
    visible_row = (visible_row + 1) % n;
}

void clamp_visible_row(int& visible_row, const Player& player) {
    const int n = row_count(player);
    if (visible_row < 0) visible_row = 0;
    if (visible_row >= n) visible_row = std::max(0, n - 1);
}

bool use_slot(Game& /*game*/, int /*visible_row*/, int /*col*/) {
    // Phase 4 replaces this body with the full cooldown/weapon/target/telegraph pipeline.
    return false;
}

} // namespace astra::ability_bar
```

- [ ] **Step 3: Add the new files to the CMake source list**

Check `CMakeLists.txt` for where `src/ability.cpp` is listed and add `src/ability_bar.cpp` alongside it.

Run: `grep -n 'ability.cpp' CMakeLists.txt`
Expected: at least one hit. Insert `src/ability_bar.cpp` on the next line (alphabetical-ish order is preserved elsewhere in the file — match that).

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: Build succeeds. No functional change yet — `ability_bar` exists but nothing calls it.

- [ ] **Step 5: Commit**

```bash
git add include/astra/ability_bar.h src/ability_bar.cpp CMakeLists.txt
git commit -m "feat(ability_bar): module scaffold with paging math (no game wiring yet)"
```

---

## Phase 2 — Player field, save format bump

### Task 2: Add `Player::ability_slots` and bump save version

**Files:**
- Modify: `include/astra/player.h`
- Modify: `include/astra/save_file.h`
- Modify: `src/save_file.cpp`
- Modify: `src/ability_bar.cpp`

- [ ] **Step 1: Add the field to `Player`**

In `include/astra/player.h`, find the existing `std::vector<SkillId> learned_skills;` (around line 100). Add immediately below it:

```cpp
    // Ability hotbar slot assignments. Flat, dense, compact — no sentinels,
    // no duplicates. Flat index N maps to (row, col) via ability_bar::kSlotsPerRow.
    std::vector<SkillId> ability_slots;
```

- [ ] **Step 2: Bump the save schema version**

In `include/astra/save_file.h`, find `SAVE_FILE_VERSION` (line 28). Change from `44` to `45`:

```cpp
inline constexpr uint32_t SAVE_FILE_VERSION = 45;   // v45: persisted ability bar slots
```

- [ ] **Step 3: Add the writer**

In `src/save_file.cpp`, find the block starting at line 607 that writes `learned_skills`:

```cpp
    w.write_u32(static_cast<uint32_t>(p.learned_skills.size()));
    for (const auto& sid : p.learned_skills) {
        w.write_u32(static_cast<uint32_t>(sid));
    }
```

Immediately after this block, add:

```cpp
    // v45: ability bar slot assignments (flat, compact)
    w.write_u32(static_cast<uint32_t>(p.ability_slots.size()));
    for (const auto& sid : p.ability_slots) {
        w.write_u32(static_cast<uint32_t>(sid));
    }
```

- [ ] **Step 4: Add the reader**

In `src/save_file.cpp`, find the matching reader block starting around line 1455:

```cpp
    uint32_t skill_count = r.read_u32();
    p.learned_skills.resize(skill_count);
    for (uint32_t i = 0; i < skill_count; ++i) {
        p.learned_skills[i] = static_cast<SkillId>(r.read_u32());
    }
```

Immediately after this block, add:

```cpp
    // v45: ability bar slot assignments
    uint32_t ability_count = r.read_u32();
    p.ability_slots.resize(ability_count);
    for (uint32_t i = 0; i < ability_count; ++i) {
        p.ability_slots[i] = static_cast<SkillId>(r.read_u32());
    }
```

- [ ] **Step 5: Wire the Player overloads in `ability_bar.cpp`**

In `src/ability_bar.cpp`, replace the four stubbed Player-overloads (added in Task 1) with real implementations:

```cpp
int row_count(const Player& player) {
    return row_count(player.ability_slots);
}

std::optional<SkillId> slot_at(const Player& player, int row, int col) {
    return slot_at(player.ability_slots, row, col);
}

bool assign_on_learn(Player& player, SkillId id) {
    auto& slots = player.ability_slots;
    if (std::find(slots.begin(), slots.end(), id) != slots.end()) {
        return false; // already present
    }
    if (static_cast<int>(slots.size()) >= kMaxRows * kSlotsPerRow) {
        return false; // bar full
    }
    slots.push_back(id);
    return true;
}

bool remove_and_compact(Player& player, SkillId id) {
    auto& slots = player.ability_slots;
    auto it = std::find(slots.begin(), slots.end(), id);
    if (it == slots.end()) return false;
    slots.erase(it);
    return true;
}
```

- [ ] **Step 6: Build**

Run: `cmake --build build -j`
Expected: Build succeeds.

- [ ] **Step 7: Manual smoke test — old saves rejected**

If there are any saves under `~/.local/share/astra/` or the equivalent, loading them should now fail gracefully. Run `./build/astra`, select Load Game — saves list should either be empty or show "no saves available" after the version bump rejects all v44 saves.

Expected stderr when attempting to load an old save: `astra: rejecting save '<name>': schema version 44, expected 45.`

- [ ] **Step 8: Commit**

```bash
git add include/astra/player.h include/astra/save_file.h src/save_file.cpp src/ability_bar.cpp
git commit -m "feat(ability_bar): persist ability_slots on Player (save v44→v45)"
```

---

## Phase 3 — Skill grant/revoke helpers; audit grant sites

### Task 3: Create the `skill_grant` module

**Files:**
- Create: `include/astra/skill_grant.h`
- Create: `src/skill_grant.cpp`

Centralizes the "player now has (or no longer has) this skill" operation. Every current grant site will be converted to route through this helper so `ability_slots` stays in sync.

- [ ] **Step 1: Write the header**

Write `include/astra/skill_grant.h`:

```cpp
#pragma once

#include "astra/skill_defs.h"

namespace astra {

struct Player;

// Record the player as having `id` (if not already), and append it to the
// ability bar via ability_bar::assign_on_learn. Returns true if this was a
// new grant (player didn't already have it), false otherwise.
bool grant_skill(Player& player, SkillId id);

// Record the player as no longer having `id`, and remove it from the bar
// via ability_bar::remove_and_compact. Returns true if the player had it.
bool revoke_skill(Player& player, SkillId id);

} // namespace astra
```

- [ ] **Step 2: Write the implementation**

Write `src/skill_grant.cpp`:

```cpp
#include "astra/skill_grant.h"

#include "astra/ability_bar.h"
#include "astra/player.h"

#include <algorithm>

namespace astra {

bool grant_skill(Player& player, SkillId id) {
    auto& ls = player.learned_skills;
    if (std::find(ls.begin(), ls.end(), id) != ls.end()) {
        // Already learned — still ensure the bar reflects it (defensive;
        // also used by the reconciliation pass in Task 7).
        ability_bar::assign_on_learn(player, id);
        return false;
    }
    ls.push_back(id);
    ability_bar::assign_on_learn(player, id);
    return true;
}

bool revoke_skill(Player& player, SkillId id) {
    auto& ls = player.learned_skills;
    auto it = std::find(ls.begin(), ls.end(), id);
    if (it == ls.end()) {
        // Not learned — still try bar cleanup in case of state drift.
        ability_bar::remove_and_compact(player, id);
        return false;
    }
    ls.erase(it);
    ability_bar::remove_and_compact(player, id);
    return true;
}

} // namespace astra
```

- [ ] **Step 3: Add to CMake**

Add `src/skill_grant.cpp` to `CMakeLists.txt` next to `src/ability_bar.cpp`.

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add include/astra/skill_grant.h src/skill_grant.cpp CMakeLists.txt
git commit -m "feat(skill_grant): centralize skill learn/revoke with ability bar sync"
```

### Task 4: Route dev-commander bulk grant through `grant_skill`

**Files:**
- Modify: `src/game.cpp` (around lines 815-821)

- [ ] **Step 1: Convert the dev-commander grant site**

In `src/game.cpp`, find the block that starts `player_.learned_skills.clear();` around line 815. Replace:

```cpp
        // Dev Commander learns every skill and category for testing.
        player_.learned_skills.clear();
        for (const auto& cat : skill_catalog()) {
            player_.learned_skills.push_back(cat.unlock_id);
            for (const auto& sk : cat.skills) {
                player_.learned_skills.push_back(sk.id);
            }
        }
```

with:

```cpp
        // Dev Commander learns every skill and category for testing.
        player_.learned_skills.clear();
        player_.ability_slots.clear();
        for (const auto& cat : skill_catalog()) {
            grant_skill(player_, cat.unlock_id);
            for (const auto& sk : cat.skills) {
                grant_skill(player_, sk.id);
            }
        }
```

- [ ] **Step 2: Add the `skill_grant.h` include**

At the top of `src/game.cpp`, add to the existing includes:

```cpp
#include "astra/skill_grant.h"
```

(Place it alphabetically with the other `astra/...` includes.)

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: Build succeeds.

- [ ] **Step 4: Manual smoke test**

Run: `./build/astra`. Start Dev Commander. The old single-row abilities bar should still show up (we haven't rewritten the renderer yet), but — since Dev Commander now populates `ability_slots` via `grant_skill` — `get_ability_bar` (the old derived scan) and the new flat list agree on the same first-5 order.

Open the dev console (backtick). Expected: no crash; can close console and play as normal.

- [ ] **Step 5: Commit**

```bash
git add src/game.cpp
git commit -m "refactor(game): dev-commander grants via grant_skill helper"
```

### Task 5: Route character-creation starting skills through `grant_skill`

**Files:**
- Modify: `src/game.cpp` (around line 1139)

- [ ] **Step 1: Convert the starting-skills assignment**

In `src/game.cpp`, find:

```cpp
    player_.learned_skills = tmpl.starting_skills;
```

Replace with:

```cpp
    player_.learned_skills.clear();
    player_.ability_slots.clear();
    for (SkillId id : tmpl.starting_skills) {
        grant_skill(player_, id);
    }
```

- [ ] **Step 2: Build**

Run: `cmake --build build -j`
Expected: Build succeeds.

- [ ] **Step 3: Manual smoke test**

Run: `./build/astra`. Go through full character creation (not Dev Commander) picking any class. Start the game. Abilities bar should show the class's starting abilities.

- [ ] **Step 4: Commit**

```bash
git add src/game.cpp
git commit -m "refactor(game): character creation grants starting skills via grant_skill"
```

### Task 6: Route character-screen learn paths through `grant_skill`

**Files:**
- Modify: `src/character_screen.cpp` (around lines 393-438)

- [ ] **Step 1: Add the include**

Top of `src/character_screen.cpp`, add to the existing `astra/...` includes:

```cpp
#include "astra/skill_grant.h"
```

- [ ] **Step 2: Convert category unlock**

Find (around line 395):

```cpp
                        player_->skill_points -= cat.sp_cost;
                        player_->learned_skills.push_back(cat.unlock_id);
                        skill_cat_expanded_[v.ci] = true;
```

Replace `player_->learned_skills.push_back(cat.unlock_id);` with:

```cpp
                        grant_skill(*player_, cat.unlock_id);
```

- [ ] **Step 3: Convert skill learn**

Find (around line 427):

```cpp
                        if (meets_req) {
                            player_->skill_points -= sk.sp_cost;
                            player_->learned_skills.push_back(sk.id);
                            if (sk.id == SkillId::Haggle) {
```

Replace `player_->learned_skills.push_back(sk.id);` with:

```cpp
                            grant_skill(*player_, sk.id);
```

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: Build succeeds.

- [ ] **Step 5: Manual smoke test**

Run: `./build/astra`. Start a Dev Commander (has skill points). Open character screen (`c`), navigate to an un-learned skill with arrow keys, press `l` to learn. Confirm the skill appears in `learned_skills` (visible on the character tab) and — since Dev Commander already had every skill slotted — no change to the bar. Better test with a non-Dev-Commander class: reach level-up, gain a skill point, learn a new ability via 'l', and verify it appears on the abilities bar at the next empty slot.

- [ ] **Step 6: Commit**

```bash
git add src/character_screen.cpp
git commit -m "refactor(character_screen): skill learn routes through grant_skill"
```

---

## Phase 4 — Replace `use_ability` with `ability_bar::use_slot`

### Task 7: Move the ability-execution pipeline into `ability_bar::use_slot`

**Files:**
- Modify: `src/ability_bar.cpp`
- Modify: `include/astra/ability.h`
- Modify: `src/ability.cpp`

- [ ] **Step 1: Copy the execution pipeline into `ability_bar.cpp`**

The body currently lives in `src/ability.cpp:328-406` (function `use_ability(int slot, Game& game)`). The new version in `ability_bar::use_slot` is identical except the slot resolution step.

Add to `src/ability_bar.cpp` (new includes at top as needed):

```cpp
#include "astra/ability.h"
#include "astra/effect.h"
#include "astra/game.h"
#include "astra/telegraph.h"
```

And replace the stubbed `use_slot` implementation with (keeping the existing logic byte-for-byte — only the slot lookup at the top changes):

```cpp
bool use_slot(Game& game, int visible_row, int col) {
    auto slot = slot_at(game.player(), visible_row, col);
    if (!slot.has_value()) {
        game.log("No ability in that slot.");
        return false;
    }

    auto* ability = find_ability(*slot);
    if (!ability) return false;

    // Cooldown check
    if (has_effect(game.player().effects, ability->cooldown_effect)) {
        const auto* cd = find_effect(game.player().effects, ability->cooldown_effect);
        game.log(ability->name + " is on cooldown (" +
                 std::to_string(cd ? cd->remaining : 0) + " ticks).");
        return false;
    }

    // Weapon requirement
    if (ability->required_weapon != WeaponClass::None) {
        bool has_weapon = false;
        const auto& rh = game.player().equipment.right_hand;
        const auto& ms = game.player().equipment.missile;
        if (rh && rh->weapon_class == ability->required_weapon) has_weapon = true;
        if (ms && ms->weapon_class == ability->required_weapon) has_weapon = true;
        if (!has_weapon) {
            game.log(ability->name + " requires the right weapon equipped.");
            return false;
        }
    }

    // Adjacent target
    Npc* target = nullptr;
    if (ability->needs_adjacent_target) {
        int px = game.player().x, py = game.player().y;
        int best_dist = 999;
        for (auto& npc : game.world().npcs()) {
            if (!npc.alive() || !is_hostile_to_player(npc.faction, game.player())) continue;
            int dx = std::abs(npc.x - px), dy = std::abs(npc.y - py);
            int dist = std::max(dx, dy);
            if (dist <= 1 && dist < best_dist) {
                target = &npc;
                best_dist = dist;
            }
        }
        if (!target) {
            game.log("No adjacent enemy to target.");
            return false;
        }
    }

    // Cooldown factory (local — old one was file-local in ability.cpp)
    auto make_cd = [](EffectId id, const std::string& name, int duration) {
        Effect e;
        e.id = id;
        e.name = name + " CD";
        e.color = Color::DarkGray;
        e.duration = duration;
        e.remaining = duration;
        e.show_in_bar = false;
        return e;
    };

    auto finalize = [ability, &game, &make_cd]() {
        add_effect(game.player().effects, make_cd(
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
}
```

- [ ] **Step 2: Delete the old `use_ability` and `get_ability_bar` from `src/ability.cpp`**

In `src/ability.cpp`, delete:
- The static `make_cooldown` function (lines ~315-326) — moved into `use_slot` as a lambda.
- The entire `std::vector<SkillId> get_ability_bar(const Player& player)` function (lines ~302-311).
- The entire `bool use_ability(int slot, Game& game)` function (lines ~328-406).
- The separator comment `// ── Use ability ──────────────` that headed the removed section.

Leave `ability_catalog()`, `find_ability()`, and all concrete ability classes untouched.

- [ ] **Step 3: Remove the declarations from `include/astra/ability.h`**

In `include/astra/ability.h`, delete these two declarations (lines 52-56):

```cpp
// Get the player's equipped ability slots (up to 5)
std::vector<SkillId> get_ability_bar(const Player& player);

// Try to use ability in slot 0-4. Returns true if used.
bool use_ability(int slot, Game& game);
```

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: Compile fails in two places — `src/game_input.cpp` (still calls `use_ability`) and `src/game_rendering.cpp` (still calls `get_ability_bar`). That is expected; Tasks 8 and 10 fix them.

Do NOT commit yet — the tree is broken. Move to Task 8.

### Task 8: Wire input handler to `ability_bar::use_slot` + add PgUp/PgDn

**Files:**
- Modify: `include/astra/game.h`
- Modify: `src/game_input.cpp` (around lines 448-457)

- [ ] **Step 1: Add the visible-row state to Game**

In `include/astra/game.h`, find the private member section. Near other transient UI state (e.g. `menu_selection_`, `load_selection_`), add:

```cpp
    int ability_bar_row_ = 0;  // currently visible row of the ability bar (transient, not saved)
```

- [ ] **Step 2: Update the input handler**

At the top of `src/game_input.cpp`, replace `#include "astra/ability.h"` with:

```cpp
#include "astra/ability_bar.h"
```

(If `astra/ability.h` is needed elsewhere in the file, keep both; grep to confirm.)

Run: `grep -n 'find_ability\|ability_catalog' src/game_input.cpp`
Expected: no matches. If there are matches, keep `ability.h` too.

Then find the `'1'..'5'` handler at line 449. Replace:

```cpp
        case '\n': case '\r':
        case '1': case '2': case '3': case '4': case '5': case '6': {
            bool wait_focused = static_cast<Widget>(focused_widget_) == Widget::Wait
                                && widget_active(active_widgets_, Widget::Wait);
            // Number keys 1-5: abilities (unless Wait widget is focused)
            if (key >= '1' && key <= '5' && !wait_focused) {
                use_ability(key - '1', *this);
                break;
            }
            if (key == '6' && !wait_focused) break;
```

with:

```cpp
        case '\n': case '\r':
        case '1': case '2': case '3': case '4': case '5': case '6':
        case '7': case '8': case '9': {
            bool wait_focused = static_cast<Widget>(focused_widget_) == Widget::Wait
                                && widget_active(active_widgets_, Widget::Wait);
            // Number keys 1..kSlotsPerRow: abilities (unless Wait widget is focused)
            if (key >= '1' && key <= ('0' + ability_bar::kSlotsPerRow) && !wait_focused) {
                ability_bar::use_slot(*this, ability_bar_row_, key - '1');
                break;
            }
            // Digits above kSlotsPerRow fall through to the wait-widget handler below
            // (so e.g. the wait widget can still use 1-6). Non-wait presses of those
            // digits are no-ops.
            if (key > ('0' + ability_bar::kSlotsPerRow) && key <= '9' && !wait_focused) break;
            if (key == '6' && !wait_focused) break;
```

Note: the wait-widget presently honors digits `1..6`. Keeping `break` behavior for the non-ability digits matches today's semantics.

- [ ] **Step 3: Add paging handlers**

Find where other game-mode keys are handled (arrow keys, etc.). Find the existing `KEY_PAGE_UP` / `KEY_PAGE_DOWN` handlers if any:

Run: `grep -n 'KEY_PAGE_UP\|KEY_PAGE_DOWN' src/game_input.cpp`
Expected: no matches (PgUp/PgDn is not currently used in the playing-state input handler).

In the `switch (key)` block for play input, add two new cases (alongside other `KEY_*` cases like `KEY_UP`, `KEY_DOWN`):

```cpp
        case KEY_PAGE_UP:
            ability_bar::page_up(ability_bar_row_, player_);
            break;
        case KEY_PAGE_DOWN:
            ability_bar::page_down(ability_bar_row_, player_);
            break;
```

If there's no obvious place, adding them immediately before the `'1'..'9'` case block is fine — proximity helps readers.

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: `src/game_input.cpp` compiles. `src/game_rendering.cpp` still fails (next task).

Do NOT commit yet.

### Task 9: Rewrite `render_abilities_bar` (three rows, paging visuals)

**Files:**
- Modify: `src/game.cpp` (line 125 — vrows[7] fixed(1) → fixed(3))
- Modify: `src/game_rendering.cpp` (function `render_abilities_bar`, lines 1514-1542)

- [ ] **Step 1: Grow the layout row**

In `src/game.cpp`, find `compute_layout` (around line 109-126). Change:

```cpp
        fixed(1),    // [7] abilities
```

to:

```cpp
        fixed(3),    // [7] abilities (3 rows: arrows hint, hotbar, page indicator)
```

- [ ] **Step 2: Update `#include` in `src/game_rendering.cpp`**

Near the top, make sure `astra/ability_bar.h` is included. If `astra/ability.h` was only used for `get_ability_bar` / `use_ability` (both removed in Task 7), the include is still needed for `find_ability` and `Ability`. Keep both includes.

Run: `grep -n '#include "astra/ability' src/game_rendering.cpp`

- [ ] **Step 3: Rewrite the function**

Replace the entire `Game::render_abilities_bar` (lines 1514-1542) with:

```cpp
void Game::render_abilities_bar() {
    UIContext ctx(renderer_.get(), abilities_rect_);

    const int rows = ability_bar::row_count(player_);
    ability_bar::clamp_visible_row(ability_bar_row_, player_);

    const bool multi_row = (rows > 1);
    const UITag arrow_tag = multi_row ? UITag::TextDim : UITag::TextDim; // same for now;
    // If the theme ever gets a deeper-dim tag, use it here when !multi_row.

    // --- Row 0: hint + up arrow --------------------------------------
    {
        std::vector<TextSegment> top;
        top.push_back({"(PgUp/PgDn)", UITag::TextDim});
        ctx.styled_text({.x = 1, .y = 0, .segments = top});

        // Right-edge up arrow (▲ = \xe2\x96\xb2)
        int arrow_x = ctx.width() - 2;
        if (arrow_x < 0) arrow_x = 0;
        ctx.styled_text({.x = arrow_x, .y = 0,
                         .segments = {{"\xe2\x96\xb2", arrow_tag}}});
    }

    // --- Row 1: hotbar ------------------------------------------------
    {
        std::vector<TextSegment> mid;
        mid.push_back({"ABILITIES:  ", UITag::TextDim});
        mid.push_back({std::to_string(ability_bar_row_ + 1) + "  ", UITag::Text});

        if (player_.ability_slots.empty()) {
            mid.push_back({"[none]", UITag::TextDim});
        } else {
            for (int col = 0; col < ability_bar::kSlotsPerRow; ++col) {
                auto slot = ability_bar::slot_at(player_, ability_bar_row_, col);
                std::string key_tag = "<" + std::to_string(col + 1) + "> ";
                if (!slot.has_value()) {
                    mid.push_back({key_tag + "---  ", UITag::TextDim});
                    continue;
                }
                const auto* ab = find_ability(*slot);
                if (!ab) {
                    // Defensive — orphaned SkillId in the bar
                    mid.push_back({key_tag + "???  ", UITag::TextDim});
                    continue;
                }
                bool on_cd = has_effect(player_.effects, ab->cooldown_effect);
                const auto* cd_eff = find_effect(player_.effects, ab->cooldown_effect);

                std::string label = key_tag + ab->name;
                if (on_cd && cd_eff && cd_eff->remaining > 0) {
                    label += "(" + std::to_string(cd_eff->remaining) + ")";
                }
                label += "  ";

                mid.push_back({label, on_cd ? UITag::TextDim : UITag::TextWarning});
            }
        }
        ctx.styled_text({.x = 1, .y = 1, .segments = mid});
    }

    // --- Row 2: page indicator + down arrow --------------------------
    {
        std::string page = " Page " + std::to_string(ability_bar_row_ + 1)
                         + " of "   + std::to_string(rows);
        ctx.styled_text({.x = 1, .y = 2,
                         .segments = {{page, UITag::TextDim}}});

        int arrow_x = ctx.width() - 2;
        if (arrow_x < 0) arrow_x = 0;
        ctx.styled_text({.x = arrow_x, .y = 2,
                         .segments = {{"\xe2\x96\xbc", arrow_tag}}});
    }
}
```

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: Build succeeds cleanly.

- [ ] **Step 5: Manual smoke test**

Run: `./build/astra`. Start Dev Commander (has every ability).

Verify:
1. Bottom of the screen shows a 3-row abilities bar: `(PgUp/PgDn) ▲`, `ABILITIES: 1  <1>..<4>`, `Page 1 of N ▼`.
2. Press `1` through `4` — abilities fire (messages in log).
3. Press PgDn — visible row changes to 2, digits rebind to the page-2 abilities, `Page 2 of N`.
4. Press PgDn until row N, then PgDn once more — wraps back to row 1.
5. PgUp wraps the other way.
6. While on page 2, pressing `1` fires the 5th-indexed ability (first of row 2), not the 1st.
7. Use an ability — its slot on the current row shows `<K> Name(N)` in dim while on cooldown.
8. Press `c` to open the character screen, then `Esc` to close. The bar re-renders cleanly at 3 rows.

- [ ] **Step 6: Commit the combined Phase 4 changes**

Phase 4 was split into three tasks (7, 8, 9) to keep each diff focused, but the tree is only buildable after Task 9. Commit as one:

```bash
git add include/astra/ability.h include/astra/ability_bar.h include/astra/game.h \
        src/ability.cpp src/ability_bar.cpp src/game.cpp src/game_input.cpp src/game_rendering.cpp
git commit -m "feat(ability_bar): 3-row paged hotbar — use_slot + PgUp/PgDn + render"
```

---

## Phase 5 — Post-load validation + reconciliation

### Task 10: Post-load validation + defensive reconciliation

**Files:**
- Modify: `src/save_file.cpp` (in the player-read path, after reading `ability_slots`)
- Modify: `src/game.cpp` (startup / load path — one idempotent call)

- [ ] **Step 1: Add validation helper to `ability_bar`**

In `include/astra/ability_bar.h`, add to the namespace:

```cpp
// Drop any slots whose SkillId the player no longer has; drop duplicates.
// Idempotent. Safe to call any time.
void validate_and_dedupe(Player& player);

// For every learned skill not currently in the bar that is ability-eligible,
// append via assign_on_learn. Idempotent. Covers the case where a grant
// site was missed in the audit.
void reconcile_from_learned(Player& player);
```

In `src/ability_bar.cpp`, add:

```cpp
#include <unordered_set>
```

and implementations:

```cpp
void validate_and_dedupe(Player& player) {
    auto& slots = player.ability_slots;

    // Drop entries whose skill the player doesn't have.
    std::erase_if(slots, [&](SkillId id) {
        return !player_has_skill(player, id);
    });

    // Drop duplicates, preserving first occurrence.
    std::unordered_set<SkillId> seen;
    std::erase_if(slots, [&](SkillId id) {
        return !seen.insert(id).second;
    });
}

void reconcile_from_learned(Player& player) {
    // For each learned skill that is an ability (has a catalog entry),
    // ensure it's on the bar.
    for (const auto& a : ability_catalog()) {
        if (player_has_skill(player, a->skill_id)) {
            assign_on_learn(player, a->skill_id);  // no-op if already present
        }
    }
}
```

`player_has_skill` is already declared in the player/skill headers — verify the include is pulled in transitively; if not, add `#include "astra/player_skills.h"` or whichever header declares it.

Run: `grep -rn 'bool player_has_skill' include/astra/ src/`
Expected: at least one header declaration. Use that include.

- [ ] **Step 2: Call validation after save-load**

In `src/save_file.cpp`, find the reader block added in Task 2 (after `learned_skills` read, around line 1465 post-insert). Immediately after the `ability_slots` read loop:

```cpp
    // Post-load: drop stale or duplicate entries; the bar may reference
    // SkillIds removed by data revisions or duplicated by a bug in older
    // builds.
    ability_bar::validate_and_dedupe(p);
```

Add the `#include "astra/ability_bar.h"` at the top of `src/save_file.cpp` if not already present.

- [ ] **Step 3: Call reconcile at game-start**

In `src/game.cpp`, find where `new_game` or the equivalent function finishes setup. At the end of character creation (after line 1139 area, after starting skills are granted) AND at the end of Dev Commander setup (after line 821 area), ensure `ability_bar::reconcile_from_learned(player_)` is called. Even though `grant_skill` already assigns on learn, this is belt-and-suspenders.

Also call it once on load, in the load path just before transitioning to `GameState::Playing`. Find that path:

Run: `grep -n 'state_ = GameState::Playing' src/game.cpp src/save_file.cpp 2>/dev/null`

Add `ability_bar::reconcile_from_learned(player_);` immediately before the transition.

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: Build succeeds.

- [ ] **Step 5: Manual smoke test — save/load round-trip**

Run: `./build/astra`. Start Dev Commander. Press PgDn to page through rows — confirm all abilities present. Save via pause menu (not Dev Commander mode — or run a non-dev build for this test only).

Alternative test: learn a skill mid-game, save, load, confirm the skill is still on the bar in the same slot.

- [ ] **Step 6: Commit**

```bash
git add include/astra/ability_bar.h src/ability_bar.cpp src/save_file.cpp src/game.cpp
git commit -m "feat(ability_bar): post-load validation + reconcile_from_learned"
```

---

## Phase 6 — Documentation & sanity pass

### Task 11: Update roadmap and formulas docs

**Files:**
- Modify: `docs/roadmap.md`
- Modify: `docs/formulas.md` (only if a relevant section exists — check first)

- [ ] **Step 1: Find the relevant roadmap entry**

Run: `grep -n -i 'abilit\|hotbar\|ability bar' docs/roadmap.md`

- [ ] **Step 2: Add or update an entry**

If there's an existing section for UI / hotbar, tick a box or add a new line describing: "3-row paged ability bar with PgUp/PgDn, persistent slot assignments, v45 save format." If not, add under a UI / polish heading:

```markdown
- [x] Ability bar — 3-row paged hotbar, persistent slot assignments, auto-assign on learn, compact on remove
```

- [ ] **Step 3: Check formulas doc**

Run: `grep -n -i 'abilit\|hotbar' docs/formulas.md`

If there's a section on abilities (there may be cooldown/damage formulas), no changes are needed — this plan is UI/persistence, no formula changes. Skip modifying if no relevant section.

- [ ] **Step 4: Commit**

```bash
git add docs/roadmap.md docs/formulas.md
git commit -m "docs: ability bar rows + paging shipped"
```

### Task 12: Final full smoke test

**Files:** none

- [ ] **Step 1: Clean build**

Run: `cmake --build build -j`
Expected: No errors, no new warnings.

- [ ] **Step 2: Full smoke-test checklist**

Run: `./build/astra`. Start Dev Commander. Verify:

- [ ] Bar renders at 3 rows, bottom of the screen.
- [ ] All digits `1..4` fire an ability on page 1.
- [ ] PgDn advances to page 2; digits now fire page-2 abilities.
- [ ] On the final page, PgDn wraps to page 1. PgUp wraps the other way.
- [ ] On a page with fewer than 4 filled slots, empty slots render `<K> ---` dim.
- [ ] Cooldown readout `(N)` appears on the current-page slot of an ability just used; paging away and back preserves the countdown.
- [ ] Character screen opens/closes cleanly; abilities bar re-renders intact.
- [ ] Pause menu (Esc) opens/closes; bar intact.
- [ ] Help screen (`?`) opens/closes; bar intact.
- [ ] Dev console (backtick) opens/closes; bar intact.
- [ ] Telegraph-mode ability (e.g. Tumble): pressing the slot starts aim; PgUp/PgDn during aim do nothing (telegraph consumes input); Esc cancels.
- [ ] With no abilities learned (a character class that starts bare, or a freshly-created character before learning any ability): bar shows `(PgUp/PgDn) ▲` / `ABILITIES:  1  [none]` / `Page 1 of 1 ▼`.
- [ ] Save (from a non-dev build / non-Dev Commander character) → quit → load → abilities present in same slot order.

Any failure → open an issue task and fix before claiming the feature complete.

- [ ] **Step 3: Final commit (if documentation tweaks emerged)**

```bash
git status
# if clean: feature complete
# else: commit any stragglers
```

---

## Self-review notes

**Spec coverage:**
- Architecture overview (module, data/transient split) → Tasks 1, 8
- Player field + invariants → Task 2
- Grant/revoke plumbing + audit → Tasks 3, 4, 5, 6
- Assign / remove semantics → Task 2 Step 5
- Paging & input → Tasks 1 Step 2 (page_up/down), 8 Step 3
- `use_slot` pipeline → Task 7
- Layout + render → Task 9
- Save format → Task 2
- Post-load validation + reconcile → Task 10
- Edge cases (cap, stale row, PgUp/PgDn no-op, telegraph swallow, empty state, orphaned SkillId) → covered in Task 9 render, Task 10 validate, Task 8 input guards
- Risks → reconciliation pass lives in Task 10

**Type consistency:** `ability_bar::` namespace consistent across header and impl. `kSlotsPerRow`, `kMaxSlotsPerRow`, `kMaxRows` named identically in every task that uses them. `SkillId` / `Player` / `Game` references match existing code.

**No placeholders:** every code step shows the actual code. No "TBD", no "handle appropriately", no "similar to above".
