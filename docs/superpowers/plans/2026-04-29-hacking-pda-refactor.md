# Hacking — PDA Refactor + Hacking Tab Placeholder Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rename `character_screen` to `pda_screen`, extract each existing tab's draw function into its own translation unit, and add a placeholder `Hacking` tab that displays a locked-state splash. This is the **foundation plan** for the broader Hacking & The Grid feature — it ships working software (a renamed PDA with all existing tabs unchanged + a visible-but-empty Hacking tab) and unblocks every subsequent layer.

**Architecture:** Pure refactor with one cosmetic addition. The existing `CharacterScreen` class is renamed to `PdaScreen` and its monolithic 4347-line `.cpp` is split into one `.cpp` per tab (each containing the tab's `draw_X` function and any tab-specific helper member functions, all still defined as member functions of `PdaScreen`). Per-tab state remains on `PdaScreen` for now; full state encapsulation is deferred. A new `Hacking` enum value is added with a stub draw that shows a locked splash. **Visual parity for existing tabs is a hard constraint** — verified manually after each commit.

**Tech Stack:** C++20, CMake, terminal renderer. No new dependencies. No tests (no test infra in repo); verification is build + run + manual tab inspection.

**Out of scope (future plans):**
- All hacking gameplay (cyberdeck items, programs, `Hackable` component, quickhacks, Grid sessions, persistence). The Hacking tab is intentionally a stub.
- Per-tab state encapsulation (each tab becoming its own class). Tabs share `PdaScreen` state for now — deferred.
- Per-tab input handler extraction. The main `handle_input` keeps its `if (active_tab_ == ...)` branches; only `draw_X` and direct helper methods move out.

**Spec reference:** `docs/superpowers/specs/2026-04-29-hacking-design.md` §4 (UX) and §6 (v1 scope).

**Project conventions to follow:**
- Build with dev mode: `cmake -B build -DDEV=ON && cmake --build build`. (Per `feedback_dev_mode.md`.)
- Headers: `#pragma once`, in `include/astra/`.
- Namespace: `astra`.
- Member variables: `snake_case_` trailing underscore.
- Classes: PascalCase.
- Don't auto-push (user pushes when ready).
- Update `docs/roadmap.md` for shipped features.

---

## File map

**Files renamed:**
- `include/astra/character_screen.h` → `include/astra/pda_screen.h`
- `src/character_screen.cpp` → `src/pda_screen.cpp` (will shrink dramatically as content moves to per-tab files)

**Files modified:**
- `include/astra/game.h` — rename member `character_screen_` → `pda_screen_`, update include
- `src/game_input.cpp` — rename member access in 12+ call sites, update include
- `src/game_rendering.cpp` — rename member access in 1 call site, update include
- `CMakeLists.txt` — rename source list entry, add per-tab files
- `docs/roadmap.md` — add line under UI section: PDA rename + Hacking tab placeholder

**Files created (one per tab):**
- `src/pda_skills_tab.cpp`
- `src/pda_attributes_tab.cpp`
- `src/pda_equipment_tab.cpp`
- `src/pda_tinkering_tab.cpp`
- `src/pda_cooking_tab.cpp`
- `src/pda_journal_tab.cpp`
- `src/pda_quests_tab.cpp`
- `src/pda_reputation_tab.cpp`
- `src/pda_ship_tab.cpp`
- `src/pda_hacking_tab.cpp` (new — locked-splash stub)

After this plan completes, `src/pda_screen.cpp` should contain only: constructor, `is_open`, `open`, `close`, `handle_input` (with input dispatch branches still inline — extraction deferred), `draw` (top-level frame + dispatch), `draw_context_menu`, `draw_look_overlay`, `draw_tab_help`, `draw_stat_box`, `draw_section_header`, `draw_stub`, `consume_*` accessors, and tab metadata (`tab_names[]`, `tab_help_title`).

---

## Task 1 — Rename `character_screen` → `pda_screen`

**Files:**
- Rename: `include/astra/character_screen.h` → `include/astra/pda_screen.h`
- Rename: `src/character_screen.cpp` → `src/pda_screen.cpp`
- Modify: `include/astra/game.h`
- Modify: `src/game_input.cpp`
- Modify: `src/game_rendering.cpp`
- Modify: `CMakeLists.txt`

This task is purely mechanical: rename file + class + enum + member, no behavior change.

- [ ] **Step 1: Rename header and source files**

```bash
cd /Users/jeffrey/dev/crawler/.worktrees/hacking
git mv include/astra/character_screen.h include/astra/pda_screen.h
git mv src/character_screen.cpp src/pda_screen.cpp
```

- [ ] **Step 2: Update include guard / pragma — N/A (uses `#pragma once`)**

Confirm `include/astra/pda_screen.h` line 1 is still `#pragma once`. Nothing else needed.

- [ ] **Step 3: In `include/astra/pda_screen.h`, rename class and enum**

Replace `class CharacterScreen` → `class PdaScreen`. Replace all references to `CharacterScreen::` inside the file (none expected; just the class declaration). Replace `enum class CharTab` → `enum class PdaTab` and rename all `CharTab::` references inside the header to `PdaTab::`.

Use sed (mechanical, header-only):

```bash
sed -i '' \
  -e 's/\bCharacterScreen\b/PdaScreen/g' \
  -e 's/\bCharTab\b/PdaTab/g' \
  include/astra/pda_screen.h
```

Verify no other identifiers were affected:

```bash
grep -n "Character\|CharTab" include/astra/pda_screen.h
```

Expected output: empty (no remaining occurrences).

- [ ] **Step 4: In `src/pda_screen.cpp`, update include and rename**

```bash
sed -i '' \
  -e 's|#include "astra/character_screen.h"|#include "astra/pda_screen.h"|g' \
  -e 's/\bCharacterScreen\b/PdaScreen/g' \
  -e 's/\bCharTab\b/PdaTab/g' \
  src/pda_screen.cpp
```

Verify:

```bash
grep -n "character_screen\|CharacterScreen\|CharTab" src/pda_screen.cpp
```

Expected output: empty.

- [ ] **Step 5: Update `include/astra/game.h`**

Replace include and member declaration. Open the file and locate the lines:

```cpp
#include "astra/character_screen.h"
```

Change to:

```cpp
#include "astra/pda_screen.h"
```

And:

```cpp
CharacterScreen character_screen_;
```

Change to:

```cpp
PdaScreen pda_screen_;
```

- [ ] **Step 6: Update `src/game_input.cpp`**

Replace include (if present) and rename all `character_screen_.` accesses to `pda_screen_.`, plus `CharacterScreen` → `PdaScreen`, `CharTab` → `PdaTab` if any appear.

```bash
sed -i '' \
  -e 's|#include "astra/character_screen.h"|#include "astra/pda_screen.h"|g' \
  -e 's/\bcharacter_screen_\b/pda_screen_/g' \
  -e 's/\bCharacterScreen\b/PdaScreen/g' \
  -e 's/\bCharTab\b/PdaTab/g' \
  src/game_input.cpp
```

Verify:

```bash
grep -n "character_screen\|CharacterScreen\|CharTab" src/game_input.cpp
```

Expected output: empty.

- [ ] **Step 7: Update `src/game_rendering.cpp`**

```bash
sed -i '' \
  -e 's|#include "astra/character_screen.h"|#include "astra/pda_screen.h"|g' \
  -e 's/\bcharacter_screen_\b/pda_screen_/g' \
  -e 's/\bCharacterScreen\b/PdaScreen/g' \
  -e 's/\bCharTab\b/PdaTab/g' \
  src/game_rendering.cpp
```

Verify:

```bash
grep -n "character_screen\|CharacterScreen\|CharTab" src/game_rendering.cpp
```

Expected output: empty.

- [ ] **Step 8: Sweep the rest of the codebase for stray references**

```bash
grep -rn "character_screen\|CharacterScreen\|CharTab" include/ src/ 2>/dev/null
```

Expected output: empty. If anything remains (e.g. `include/astra/player.h` was reported in the survey), apply the same sed swap to it. Re-run grep until clean.

- [ ] **Step 9: Update `CMakeLists.txt` source list**

Open `CMakeLists.txt`, find the line `src/character_screen.cpp` (around line 138), change it to `src/pda_screen.cpp`.

```bash
sed -i '' 's|src/character_screen.cpp|src/pda_screen.cpp|g' CMakeLists.txt
grep -n "character_screen\|pda_screen" CMakeLists.txt
```

Expected: one line, `src/pda_screen.cpp`.

- [ ] **Step 10: Build**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

Expected: clean build, zero errors. If errors, they are 99% renamed-identifier misses — re-run grep step 8 with broader scope (`grep -rn "CharacterScreen\|character_screen\|CharTab"`).

- [ ] **Step 11: Smoke test**

Run the game (`./build/astra`), open the PDA (the existing character-screen hotkey), and tab through every tab (Skills, Attributes, Inventory & Equipment, Tinkering, Cooking, Journal, Quests, Reputation, Ship). Confirm each renders identically to before the rename. Close PDA, reopen — verify "remember last tab" still works.

- [ ] **Step 12: Commit**

```bash
git add include/astra/pda_screen.h src/pda_screen.cpp \
        include/astra/game.h src/game_input.cpp src/game_rendering.cpp \
        CMakeLists.txt
git commit -m "$(cat <<'EOF'
refactor(ui): rename character_screen → pda_screen

Pure rename. CharacterScreen → PdaScreen, CharTab → PdaTab,
character_screen_ → pda_screen_. Visual output unchanged.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2 — Add "PDA" frame title (cosmetic chrome)

**Files:**
- Modify: `src/pda_screen.cpp` (panel construction in `PdaScreen::draw`)

This is the only intentional visual change in Plan 1: the panel chrome gains a "PDA" title. Per the spec §4: "frame restyle scope: cosmetic only and limited to the outer chrome (title bar text). The interior of every existing tab stays pixel-identical."

- [ ] **Step 1: Inspect the panel construction site**

In `src/pda_screen.cpp`, locate the panel construction (around what was line 1334 before the rename). The current code is:

```cpp
auto ctx = outer.panel({.footer = footer_text});
```

- [ ] **Step 2: Add a `.title` field**

Check the `panel()` API to confirm `.title` is supported. Open `include/astra/ui_components.h` (or wherever `panel` is declared) and confirm the `PanelOptions` struct has a `title` field. If it does, change the call to:

```cpp
auto ctx = outer.panel({.title = "PDA", .footer = footer_text});
```

If `PanelOptions` does not have a `title` field, **stop and report**. The plan assumes it does (other panels in the codebase use titled panels — confirm by `grep -rn "\.title" include/astra/ui_components.h src/ui_components.cpp` before assuming a fix is required). If absent, add the field by following the existing field pattern (out-of-scope addition; keep the change tight).

- [ ] **Step 3: Build**

```bash
cmake --build build
```

Expected: clean.

- [ ] **Step 4: Smoke test**

Run `./build/astra`, open PDA, verify the title "PDA" appears at the top of the outer panel chrome and that *no tab interior is altered* (cycle through every tab and compare to your memory of step 11 of Task 1).

- [ ] **Step 5: Commit**

```bash
git add src/pda_screen.cpp
git commit -m "$(cat <<'EOF'
ui(pda): add 'PDA' title to outer panel chrome

Cosmetic-only chrome change. Tab interiors untouched.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3 — Add `PdaTab::Hacking` enum + tab strip entry + locked placeholder

**Files:**
- Modify: `include/astra/pda_screen.h`
- Modify: `src/pda_screen.cpp`

Adds the new tab as a locked placeholder. No real hacking functionality yet — that's future plans. The placeholder draws a centered message indicating the feature requires the (not-yet-implemented) `Cat_Hacking` skill.

- [ ] **Step 1: Add enum entry**

Open `include/astra/pda_screen.h`. Locate the `enum class PdaTab` definition (formerly `CharTab`, around line 18). It currently reads:

```cpp
enum class PdaTab : uint8_t {
    Skills,
    Attributes,
    Equipment,
    Tinkering,
    Cooking,
    Journal,
    Quests,
    Reputation,
    Ship,
};

static constexpr int char_tab_count = 9;
```

Add a `Hacking` entry at the end and bump the count. Also rename the count constant for consistency with the new naming:

```cpp
enum class PdaTab : uint8_t {
    Skills,
    Attributes,
    Equipment,
    Tinkering,
    Cooking,
    Journal,
    Quests,
    Reputation,
    Ship,
    Hacking,
};

static constexpr int pda_tab_count = 10;
```

If `char_tab_count` is referenced elsewhere in code, update those references too:

```bash
grep -rn "char_tab_count" include/ src/
```

Replace each hit with `pda_tab_count`.

- [ ] **Step 2: Add the placeholder draw method declaration**

In `include/astra/pda_screen.h`, in the `private:` section near the other `draw_X(UIContext&)` declarations (around line 213 — the `draw_attributes`, `draw_skills`, etc. block), add:

```cpp
    void draw_hacking(UIContext& ctx);
```

- [ ] **Step 3: Add the Hacking entry to the `tab_names[]` array**

In `src/pda_screen.cpp`, near the top (around line 27), update the array:

```cpp
static const char* tab_names[] = {
    "Skills", "Attributes", "Inventory & Equipment", "Tinkering",
    "Cooking", "Journal", "Quests", "Reputation", "Ship", "Hacking",
};
```

Confirm the array length is now 10 entries (matches `pda_tab_count`).

- [ ] **Step 4: Add `Hacking` to `tab_help_title` switch**

In `src/pda_screen.cpp`, locate the `tab_help_title` function (around line 4248). Add the case before the closing brace:

```cpp
        case PdaTab::Hacking:    return "Hacking";
```

- [ ] **Step 5: Add the dispatch case in `PdaScreen::draw`**

In `src/pda_screen.cpp`, locate the `switch (active_tab_)` at the end of `PdaScreen::draw()` (around what was line 1349). Add the new case before the closing brace:

```cpp
        case PdaTab::Hacking:    draw_hacking(content); break;
```

- [ ] **Step 6: Implement the placeholder `draw_hacking`**

Append this function to `src/pda_screen.cpp` (place it next to other small `draw_X` functions, e.g. just after `draw_reputation`):

```cpp
void PdaScreen::draw_hacking(UIContext& ctx) {
    // Placeholder until cyberdeck + Cat_Hacking skill are implemented in
    // future plans. The deck/skill state cannot be checked yet because
    // neither exists, so we always render the locked splash for now.
    draw_stub(ctx,
        "HACKING\n\n"
        "Requires a cyberdeck and the Hacking skill.\n"
        "(Feature in development.)");
}
```

- [ ] **Step 7: Handle input on the Hacking tab**

The existing `handle_input` dispatch chain (the `if (active_tab_ == PdaTab::Skills) { ... } else if (active_tab_ == PdaTab::Tinkering) { ... }` chain in `PdaScreen::handle_input`) does not have a branch for `Hacking`. Without one, the tab will simply consume left/right arrow navigation (which is fine — handled by the outer tab switch logic) and do nothing else. Confirm this by reading the existing dispatch — the outer tab switch occurs *before* the active-tab-specific branches (around lines 147–158). If the outer switch already covers tab navigation regardless of active tab, no further change is needed.

If the existing code requires a per-tab fall-through, add an explicit no-op branch:

```cpp
    } else if (active_tab_ == PdaTab::Hacking) {
        // No tab-specific input yet — placeholder.
    }
```

Add this at the end of the existing `else if` chain (just before the final `else` or closing brace of the function).

- [ ] **Step 8: Build**

```bash
cmake --build build
```

Expected: clean.

- [ ] **Step 9: Smoke test**

Run `./build/astra`, open the PDA, navigate to the rightmost tab using right-arrow several times. Confirm:

1. The `Hacking` tab appears in the tab strip (rightmost position).
2. Selecting it shows the locked splash text.
3. Pressing left-arrow returns to `Ship`; pressing right-arrow from `Ship` reaches `Hacking`.
4. ESC closes the screen normally.
5. Reopening PDA on the Hacking tab still shows the splash (last-tab persistence works).
6. All other tabs still render identically.

- [ ] **Step 10: Commit**

```bash
git add include/astra/pda_screen.h src/pda_screen.cpp
git commit -m "$(cat <<'EOF'
feat(pda): add Hacking tab placeholder

Adds PdaTab::Hacking with a locked-state splash. Real hacking
functionality (cyberdeck, programs, jack-in) follows in future plans.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Tasks 4–13 — Per-tab `draw` extraction

**Pattern (identical for every tab):**

For each tab `<X>` in {skills, attributes, equipment, tinkering, cooking, journal, quests, reputation, ship, hacking}:

1. Create `src/pda_<X>_tab.cpp`.
2. Move `void PdaScreen::draw_<X>(UIContext& ctx) { ... }` into it.
3. Move any `<X>`-specific helper member function definitions into it (private methods declared in the header).
4. Add `#include "astra/pda_screen.h"` plus any other headers the moved code uses.
5. Wrap definitions in `namespace astra { ... }`.
6. Remove the moved bodies from `src/pda_screen.cpp` (the **declarations** stay in the header; only **definitions** move).
7. Add `src/pda_<X>_tab.cpp` to `CMakeLists.txt` source list.
8. Build, smoke test the affected tab and one neighbor for parity, commit.

**Helper-function inventory by tab** (definitions to move alongside `draw_<X>`):

| Tab | Member function definitions to move |
|---|---|
| skills | `draw_skills`, `build_skill_vis` |
| attributes | `draw_attributes`, `has_pending`, `total_pending`, `commit_pending` |
| equipment | `draw_equipment` |
| tinkering | `draw_tinkering` (only — tinkering helpers are mostly local lambdas) |
| cooking | `draw_cooking`, `handle_cooking_key`, `handle_cooking_picker_key`, `handle_cooking_qty_prompt_key`, `cooking_open_picker_for_slot`, `cooking_picker_confirm`, `cooking_commit_qty_prompt`, `cooking_clear_slot`, `cooking_toggle_recipe`, `cooking_attempt_cook` |
| journal | `draw_journal` |
| quests | `draw_quests`, `build_quest_vis` |
| reputation | `draw_reputation` |
| ship | `draw_ship` |
| hacking | `draw_hacking` (the stub from Task 3) |

For each task below, verify the helper list against the actual file before moving — the inventory above is accurate as of plan-write time but may have drifted. Use:

```bash
grep -n "^void PdaScreen::\|^bool PdaScreen::\|^int PdaScreen::\|^std::vector.*PdaScreen::" src/pda_screen.cpp
```

to enumerate all member definitions in `pda_screen.cpp` before starting.

---

### Task 4 — Extract `skills` tab

**Files:**
- Create: `src/pda_skills_tab.cpp`
- Modify: `src/pda_screen.cpp` (remove moved definitions)
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Locate definitions to move**

```bash
grep -n "^void PdaScreen::draw_skills\|^.*PdaScreen::build_skill_vis" src/pda_screen.cpp
```

Note the line ranges of both definitions (each runs from its `{` to its matching `}`).

- [ ] **Step 2: Read both definitions in full**

Use `sed -n '<start>,<end>p' src/pda_screen.cpp` for each range, or open the file in an editor. Identify which standard library / project headers the bodies depend on (look at any `astra::*` types referenced and any `#include` already used by `draw_skills` indirectly).

- [ ] **Step 3: Create `src/pda_skills_tab.cpp`**

Write the file with this skeleton:

```cpp
#include "astra/pda_screen.h"

// Add any additional includes that the moved definitions need.
// Common ones for skills: <algorithm>, <string>, "astra/skill_defs.h",
// "astra/player.h", "astra/ui_components.h", "astra/ui_layout.h".

namespace astra {

// === Definitions moved from pda_screen.cpp ===

std::vector<PdaScreen::SkillVisItem> PdaScreen::build_skill_vis() const {
    // (paste body verbatim)
}

void PdaScreen::draw_skills(UIContext& ctx) {
    // (paste body verbatim)
}

} // namespace astra
```

- [ ] **Step 4: Remove the moved definitions from `src/pda_screen.cpp`**

Delete the exact line ranges identified in Step 1. Do not delete anything else. Do not delete the **declarations** in `include/astra/pda_screen.h` — those stay.

- [ ] **Step 5: Add `src/pda_skills_tab.cpp` to `CMakeLists.txt`**

Locate the line `src/pda_screen.cpp` in `CMakeLists.txt` and add the new file directly after it:

```cmake
    src/pda_screen.cpp
    src/pda_skills_tab.cpp
```

- [ ] **Step 6: Build**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

Expected: clean. If you get "undefined reference" for `build_skill_vis` or `draw_skills`, you missed adding the .cpp to CMakeLists. If you get "incomplete type" or missing-header errors, add the relevant include to `pda_skills_tab.cpp`. If you get duplicate-symbol errors, the original definition wasn't fully removed from `pda_screen.cpp`.

- [ ] **Step 7: Smoke test**

Run `./build/astra`, open PDA, switch to Skills tab. Verify:
- Tab renders identically to before.
- Cursor movement works (up/down).
- Skill category expand/collapse (Space) works.
- Learning a skill (`l`) works.

Also visit one neighboring tab (Attributes) to confirm the unaffected tabs still work.

- [ ] **Step 8: Commit**

```bash
git add src/pda_skills_tab.cpp src/pda_screen.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
refactor(pda): extract skills tab into pda_skills_tab.cpp

Pure code move. Member definitions of draw_skills and build_skill_vis
relocated; declarations remain in pda_screen.h. Behavior unchanged.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 5 — Extract `attributes` tab

**Files:**
- Create: `src/pda_attributes_tab.cpp`
- Modify: `src/pda_screen.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Locate definitions to move**

```bash
grep -n "^void PdaScreen::draw_attributes\|^bool PdaScreen::has_pending\|^int PdaScreen::total_pending\|^void PdaScreen::commit_pending" src/pda_screen.cpp
```

- [ ] **Step 2: Read all four definitions in full**

Note their line ranges.

- [ ] **Step 3: Create `src/pda_attributes_tab.cpp`**

```cpp
#include "astra/pda_screen.h"

// Likely needed includes: "astra/player.h", "astra/ui_components.h",
// "astra/ui_layout.h", <string>, <cstdio>.

namespace astra {

bool PdaScreen::has_pending() const {
    // (paste body verbatim)
}

int PdaScreen::total_pending() const {
    // (paste body verbatim)
}

void PdaScreen::commit_pending() {
    // (paste body verbatim)
}

void PdaScreen::draw_attributes(UIContext& ctx) {
    // (paste body verbatim)
}

} // namespace astra
```

- [ ] **Step 4: Remove the moved definitions from `src/pda_screen.cpp`**

- [ ] **Step 5: Add `src/pda_attributes_tab.cpp` to `CMakeLists.txt`**

```cmake
    src/pda_skills_tab.cpp
    src/pda_attributes_tab.cpp
```

- [ ] **Step 6: Build**

```bash
cmake --build build
```

Expected: clean.

- [ ] **Step 7: Smoke test**

Open PDA, switch to Attributes tab. Verify:
- Layout is identical (main attrs, secondary attrs, resistances).
- Cursor moves between attribute boxes.
- `+`/`-` adjusts pending allocations (when points are available).
- `Space` commits pending changes.
- Switch to a neighboring tab (Skills, Equipment) and back — state is preserved.

- [ ] **Step 8: Commit**

```bash
git add src/pda_attributes_tab.cpp src/pda_screen.cpp CMakeLists.txt
git commit -m "refactor(pda): extract attributes tab into pda_attributes_tab.cpp

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 6 — Extract `equipment` tab

**Files:**
- Create: `src/pda_equipment_tab.cpp`
- Modify: `src/pda_screen.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Locate `draw_equipment`**

```bash
grep -n "^void PdaScreen::draw_equipment" src/pda_screen.cpp
```

- [ ] **Step 2: Create `src/pda_equipment_tab.cpp`**

```cpp
#include "astra/pda_screen.h"

// Likely needed: "astra/player.h", "astra/item.h", "astra/display_name.h",
// "astra/ui_components.h", <string>.

namespace astra {

void PdaScreen::draw_equipment(UIContext& ctx) {
    // (paste body verbatim)
}

} // namespace astra
```

- [ ] **Step 3: Remove from `src/pda_screen.cpp`**

- [ ] **Step 4: Add to CMakeLists.txt**

```cmake
    src/pda_attributes_tab.cpp
    src/pda_equipment_tab.cpp
```

- [ ] **Step 5: Build**

```bash
cmake --build build
```

- [ ] **Step 6: Smoke test**

Open PDA → Inventory & Equipment. Verify paper-doll renders, inventory list renders, cursor movement, `Tab` switches focus between paper-doll and inventory, `Space` interacts, `g` toggles, `l` look. Drop item / use item / recharge interactions still work.

- [ ] **Step 7: Commit**

```bash
git add src/pda_equipment_tab.cpp src/pda_screen.cpp CMakeLists.txt
git commit -m "refactor(pda): extract equipment tab into pda_equipment_tab.cpp

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 7 — Extract `tinkering` tab

**Files:**
- Create: `src/pda_tinkering_tab.cpp`
- Modify: `src/pda_screen.cpp`
- Modify: `CMakeLists.txt`

This is the largest tab (~530 lines). The tinkering helpers are mostly file-local lambdas inside `draw_tinkering`, so only `draw_tinkering` itself needs to move. Verify with grep first.

- [ ] **Step 1: Locate `draw_tinkering` and verify no helper member functions exist**

```bash
grep -n "^void PdaScreen::draw_tinkering\|^.*PdaScreen::.*tinker\|^.*PdaScreen::.*synth\|^.*PdaScreen::.*workbench\|^.*PdaScreen::.*catalog" src/pda_screen.cpp
```

If only `draw_tinkering` is listed, proceed. If other tinkering-related member functions appear, move them too (add them to the new file in the same task).

- [ ] **Step 2: Create `src/pda_tinkering_tab.cpp`**

```cpp
#include "astra/pda_screen.h"

// Likely needed: "astra/player.h", "astra/item.h", "astra/tinkering.h"
// (or similar — check existing includes at top of pda_screen.cpp),
// "astra/display_name.h", "astra/ui_components.h", <string>, <vector>.

namespace astra {

void PdaScreen::draw_tinkering(UIContext& ctx) {
    // (paste body verbatim — large function)
}

// (any other tinkering member definitions found in step 1)

} // namespace astra
```

- [ ] **Step 3: Remove from `src/pda_screen.cpp`**

- [ ] **Step 4: Add to CMakeLists.txt**

```cmake
    src/pda_equipment_tab.cpp
    src/pda_tinkering_tab.cpp
```

- [ ] **Step 5: Build**

```bash
cmake --build build
```

- [ ] **Step 6: Smoke test**

Open PDA → Tinkering. Verify:
- Workbench renders, slots render, synthesizer renders, materials list, catalog tab.
- Tab cycles through focus regions.
- All tinkering hotkeys work (`r`, `a`, `s`, `f`, `x`, `y`, `R`, `C`).
- Both Blueprints and Schematics catalog tabs work (`←/→` switches).

This is the most complex tab — be thorough.

- [ ] **Step 7: Commit**

```bash
git add src/pda_tinkering_tab.cpp src/pda_screen.cpp CMakeLists.txt
git commit -m "refactor(pda): extract tinkering tab into pda_tinkering_tab.cpp

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 8 — Extract `cooking` tab

**Files:**
- Create: `src/pda_cooking_tab.cpp`
- Modify: `src/pda_screen.cpp`
- Modify: `CMakeLists.txt`

Cooking has a substantial helper inventory. All ten functions move together.

- [ ] **Step 1: Locate all cooking member definitions**

```bash
grep -n "^.*PdaScreen::.*[Cc]ooking" src/pda_screen.cpp
```

Expected matches include:
- `draw_cooking`
- `handle_cooking_key`
- `handle_cooking_picker_key`
- `handle_cooking_qty_prompt_key`
- `cooking_open_picker_for_slot`
- `cooking_picker_confirm`
- `cooking_commit_qty_prompt`
- `cooking_clear_slot`
- `cooking_toggle_recipe`
- `cooking_attempt_cook`

- [ ] **Step 2: Create `src/pda_cooking_tab.cpp`**

```cpp
#include "astra/pda_screen.h"

// Likely needed: "astra/player.h", "astra/item.h", "astra/cooking.h"
// (or whatever cooking module is named — check imports of pda_screen.cpp),
// "astra/ui_components.h", <string>, <vector>, <unordered_set>.

namespace astra {

// Order: callees first, callers last (for forward-decl friendliness),
// or simply paste in source order. Compilation order doesn't matter for
// member definitions in the same TU.

void PdaScreen::cooking_clear_slot(int idx) { /* ... */ }
void PdaScreen::cooking_toggle_recipe(uint16_t recipe_id) { /* ... */ }
void PdaScreen::cooking_open_picker_for_slot(int slot_idx) { /* ... */ }
void PdaScreen::cooking_picker_confirm() { /* ... */ }
void PdaScreen::cooking_commit_qty_prompt() { /* ... */ }
void PdaScreen::cooking_attempt_cook() { /* ... */ }
void PdaScreen::handle_cooking_qty_prompt_key(int key) { /* ... */ }
void PdaScreen::handle_cooking_picker_key(int key) { /* ... */ }
void PdaScreen::handle_cooking_key(int key) { /* ... */ }
void PdaScreen::draw_cooking(UIContext& ctx) { /* ... */ }

} // namespace astra
```

Replace each `/* ... */` with the verbatim body from `src/pda_screen.cpp`.

- [ ] **Step 3: Remove all ten definitions from `src/pda_screen.cpp`**

- [ ] **Step 4: Add to CMakeLists.txt**

```cmake
    src/pda_tinkering_tab.cpp
    src/pda_cooking_tab.cpp
```

- [ ] **Step 5: Build**

```bash
cmake --build build
```

If "undefined reference" errors appear for any cooking method, check that all ten are in the new file and removed from the old. If the build complains about `cooking_picker_active_` or similar member access, the includes in `pda_cooking_tab.cpp` are insufficient — but these are member accesses on `PdaScreen` itself, so the existing `#include "astra/pda_screen.h"` should suffice. The likely issue would be missing includes for types like `Item` or external cooking-related headers — add them.

- [ ] **Step 6: Smoke test**

Open PDA → Cooking. Verify:
- Pot slots render, cookbook renders.
- `Tab` switches focus between slots and cookbook.
- `←/→` navigates between slots.
- `Space` opens the ingredient picker; picker filters and selects work.
- Quantity prompt accepts digits and `Enter` commits.
- `x` clears a slot.
- `c` triggers cook.

Cooking has the most stateful sub-modes — exercise all of them.

- [ ] **Step 7: Commit**

```bash
git add src/pda_cooking_tab.cpp src/pda_screen.cpp CMakeLists.txt
git commit -m "refactor(pda): extract cooking tab into pda_cooking_tab.cpp

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 9 — Extract `journal` tab

**Files:**
- Create: `src/pda_journal_tab.cpp`
- Modify: `src/pda_screen.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Locate `draw_journal`**

```bash
grep -n "^void PdaScreen::draw_journal" src/pda_screen.cpp
```

- [ ] **Step 2: Create `src/pda_journal_tab.cpp`**

```cpp
#include "astra/pda_screen.h"

// Likely needed: "astra/player.h", "astra/journal.h", "astra/ui_components.h",
// <string>.

namespace astra {

void PdaScreen::draw_journal(UIContext& ctx) {
    // (paste body verbatim)
}

} // namespace astra
```

- [ ] **Step 3: Remove from `src/pda_screen.cpp`**

- [ ] **Step 4: Add to CMakeLists.txt**

```cmake
    src/pda_cooking_tab.cpp
    src/pda_journal_tab.cpp
```

- [ ] **Step 5: Build**

```bash
cmake --build build
```

- [ ] **Step 6: Smoke test**

Open PDA → Journal. Verify entries list (or empty-state stub if no entries), scroll up/down works.

- [ ] **Step 7: Commit**

```bash
git add src/pda_journal_tab.cpp src/pda_screen.cpp CMakeLists.txt
git commit -m "refactor(pda): extract journal tab into pda_journal_tab.cpp

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 10 — Extract `quests` tab

**Files:**
- Create: `src/pda_quests_tab.cpp`
- Modify: `src/pda_screen.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Locate definitions**

```bash
grep -n "^void PdaScreen::draw_quests\|^std::vector.*PdaScreen::build_quest_vis" src/pda_screen.cpp
```

- [ ] **Step 2: Create `src/pda_quests_tab.cpp`**

```cpp
#include "astra/pda_screen.h"

// Likely needed: "astra/player.h", "astra/quest_manager.h" (or similar),
// "astra/ui_components.h", <string>, <vector>, <unordered_set>.

namespace astra {

std::vector<PdaScreen::QuestVisItem> PdaScreen::build_quest_vis() const {
    // (paste body verbatim)
}

void PdaScreen::draw_quests(UIContext& ctx) {
    // (paste body verbatim)
}

} // namespace astra
```

- [ ] **Step 3: Remove from `src/pda_screen.cpp`**

- [ ] **Step 4: Add to CMakeLists.txt**

```cmake
    src/pda_journal_tab.cpp
    src/pda_quests_tab.cpp
```

- [ ] **Step 5: Build**

```bash
cmake --build build
```

- [ ] **Step 6: Smoke test**

Open PDA → Quests. Verify:
- Categories (Main / Bounties / Contracts) render.
- Arc headers and quest items render.
- Cursor movement, expand/collapse work.
- Left/right focus switching between left list and right reward area.

- [ ] **Step 7: Commit**

```bash
git add src/pda_quests_tab.cpp src/pda_screen.cpp CMakeLists.txt
git commit -m "refactor(pda): extract quests tab into pda_quests_tab.cpp

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 11 — Extract `reputation` tab

**Files:**
- Create: `src/pda_reputation_tab.cpp`
- Modify: `src/pda_screen.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Locate `draw_reputation`**

```bash
grep -n "^void PdaScreen::draw_reputation" src/pda_screen.cpp
```

- [ ] **Step 2: Create `src/pda_reputation_tab.cpp`**

```cpp
#include "astra/pda_screen.h"

// Likely needed: "astra/player.h", "astra/faction.h", "astra/ui_components.h",
// <string>.

namespace astra {

void PdaScreen::draw_reputation(UIContext& ctx) {
    // (paste body verbatim)
}

} // namespace astra
```

- [ ] **Step 3: Remove from `src/pda_screen.cpp`**

- [ ] **Step 4: Add to CMakeLists.txt**

```cmake
    src/pda_quests_tab.cpp
    src/pda_reputation_tab.cpp
```

- [ ] **Step 5: Build**

```bash
cmake --build build
```

- [ ] **Step 6: Smoke test**

Open PDA → Reputation. Verify faction list renders with current reputation values and colors.

- [ ] **Step 7: Commit**

```bash
git add src/pda_reputation_tab.cpp src/pda_screen.cpp CMakeLists.txt
git commit -m "refactor(pda): extract reputation tab into pda_reputation_tab.cpp

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 12 — Extract `ship` tab

**Files:**
- Create: `src/pda_ship_tab.cpp`
- Modify: `src/pda_screen.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Locate `draw_ship`**

```bash
grep -n "^void PdaScreen::draw_ship" src/pda_screen.cpp
```

- [ ] **Step 2: Create `src/pda_ship_tab.cpp`**

```cpp
#include "astra/pda_screen.h"

// Likely needed: "astra/player.h", "astra/world_manager.h" (or wherever ship
// state lives — check existing pda_screen.cpp includes), "astra/ui_components.h",
// <string>.

namespace astra {

void PdaScreen::draw_ship(UIContext& ctx) {
    // (paste body verbatim)
}

} // namespace astra
```

- [ ] **Step 3: Remove from `src/pda_screen.cpp`**

- [ ] **Step 4: Add to CMakeLists.txt**

```cmake
    src/pda_reputation_tab.cpp
    src/pda_ship_tab.cpp
```

- [ ] **Step 5: Build**

```bash
cmake --build build
```

- [ ] **Step 6: Smoke test**

Open PDA → Ship. Verify:
- Actions / Components / Diagnostics sections render.
- `Tab` cycles focus.
- "Board Ship" action appears when `can_board_ship` is true (test from a station context if available).
- Component install flow works.

- [ ] **Step 7: Commit**

```bash
git add src/pda_ship_tab.cpp src/pda_screen.cpp CMakeLists.txt
git commit -m "refactor(pda): extract ship tab into pda_ship_tab.cpp

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 13 — Extract `hacking` tab stub

**Files:**
- Create: `src/pda_hacking_tab.cpp`
- Modify: `src/pda_screen.cpp`
- Modify: `CMakeLists.txt`

This finalizes the per-tab structure: every `draw_X` lives in its own file. The Hacking stub from Task 3 moves into a dedicated file ready for future plans to fill in real functionality.

- [ ] **Step 1: Locate `draw_hacking`**

```bash
grep -n "^void PdaScreen::draw_hacking" src/pda_screen.cpp
```

- [ ] **Step 2: Create `src/pda_hacking_tab.cpp`**

```cpp
#include "astra/pda_screen.h"

#include "astra/ui_components.h"

namespace astra {

void PdaScreen::draw_hacking(UIContext& ctx) {
    // Placeholder until cyberdeck + Cat_Hacking skill are implemented in
    // future plans. The deck/skill state cannot be checked yet because
    // neither exists, so we always render the locked splash for now.
    draw_stub(ctx,
        "HACKING\n\n"
        "Requires a cyberdeck and the Hacking skill.\n"
        "(Feature in development.)");
}

} // namespace astra
```

- [ ] **Step 3: Remove from `src/pda_screen.cpp`**

- [ ] **Step 4: Add to CMakeLists.txt**

```cmake
    src/pda_ship_tab.cpp
    src/pda_hacking_tab.cpp
```

- [ ] **Step 5: Build**

```bash
cmake --build build
```

- [ ] **Step 6: Smoke test**

Open PDA → Hacking. Verify the locked splash text appears. Confirm tab nav still works.

- [ ] **Step 7: Verify final state of `pda_screen.cpp`**

```bash
grep -n "^void PdaScreen::draw_\|^.*PdaScreen::build_\|^.*PdaScreen::has_pending\|^.*PdaScreen::handle_cooking" src/pda_screen.cpp
```

Expected: only entries that **should** remain in the dispatcher file (e.g. `draw`, `draw_context_menu`, `draw_look_overlay`, `draw_tab_help`, `draw_stat_box`, `draw_section_header`, `draw_stub`). No `draw_skills`, `draw_attributes`, `draw_equipment`, `draw_tinkering`, `draw_cooking`, `draw_journal`, `draw_quests`, `draw_reputation`, `draw_ship`, or `draw_hacking` — those are now in their per-tab files.

```bash
wc -l src/pda_screen.cpp
```

Expected: significantly smaller than the original 4347 (likely 600-900 lines, depending on how much was per-tab vs shared).

- [ ] **Step 8: Commit**

```bash
git add src/pda_hacking_tab.cpp src/pda_screen.cpp CMakeLists.txt
git commit -m "refactor(pda): extract hacking tab stub into pda_hacking_tab.cpp

Completes per-tab extraction. pda_screen.cpp is now a thin dispatcher
plus shared chrome (context menu, look overlay, tab help).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 14 — Roadmap update

**Files:**
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Add a roadmap line under the relevant UI section**

Open `docs/roadmap.md`. Find an appropriate UI/QoL section (or add at the end of the most recent section). Add:

```markdown
- [x] **PDA refactor** — `character_screen` renamed to `pda_screen`; per-tab modules; new (placeholder) Hacking tab. Foundation for the Hacking & The Grid feature spec'd in `docs/superpowers/specs/2026-04-29-hacking-design.md`.
```

- [ ] **Step 2: Commit**

```bash
git add docs/roadmap.md
git commit -m "docs(roadmap): PDA refactor + Hacking tab placeholder

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Final verification

After all tasks complete:

- [ ] **Full build from clean state**

```bash
rm -rf build && cmake -B build -DDEV=ON && cmake --build build
```

Expected: clean build, zero warnings introduced by this plan.

- [ ] **Comprehensive smoke test**

Run `./build/astra`, open PDA, exhaustively visit every tab in order:

1. Skills — cursor, expand/collapse, learn
2. Attributes — boxes, allocate, commit
3. Inventory & Equipment — paper-doll, inventory, drop, use, recharge
4. Tinkering — workbench, slots, synthesizer, materials, both catalog tabs
5. Cooking — slots, picker, qty prompt, cook
6. Journal — list, scroll
7. Quests — categories, expand, focus switching
8. Reputation — list rendering
9. Ship — actions, components, diagnostics, install (if applicable)
10. Hacking — locked splash

Confirm:
- Every tab renders and behaves identically to pre-plan baseline (except Hacking, which is new).
- "Last tab" persistence still works (close PDA on Tinkering, reopen, lands on Tinkering).
- Tab help overlay still works on every tab.
- Context menu still works on Inventory.
- ESC closes from any tab.

- [ ] **Branch state**

```bash
git log --oneline main..HEAD
```

Expected: 14 commits (one per task), each scoped to a single file or small set of related files.

---

## Self-review notes (kept here for the executor's awareness, not as work)

**Spec coverage** — This plan implements only §4's PDA refactor + §6's Hacking tab placeholder line. All other spec sections (Hackable component, cyberdeck, programs, Grid session, persistence) are explicitly out of scope and become future plans.

**Risks flagged in spec §6 addressed by this plan:**
- "PDA tab refactor regresses existing screens" — mitigated by per-tab commits + smoke tests between commits + visual-parity constraint.

**Potential gotchas the executor should know about:**
- `sed -i ''` is the macOS form (empty-string suffix). On Linux, drop the `''`.
- The `panel().title` field assumed in Task 2 may not exist — Step 2 of that task tells the executor to verify before proceeding; if it's absent, that's a small extra change and should be done in Task 2's commit.
- Cooking tab has the most state — Task 8 is the riskiest. Test all sub-modes (picker, quantity prompt, cooking attempt) before committing.
- Tinkering tab is the longest function but has the simplest extraction (no helper member functions, only file-local lambdas). It should move cleanly.
- `char_tab_count` in the original header may be referenced from outside `pda_screen.h`; Task 3 Step 1 includes a grep to find any external uses.
- This plan does NOT split tab-specific input handling out of `handle_input`. That `else if` chain stays in `pda_screen.cpp`. Future plans may extract per-tab `handle_X(int key)` methods if the chain becomes unwieldy.
