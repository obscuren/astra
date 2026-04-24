# Ability Bar — Rows & Persistent Assignment

**Date:** 2026-04-24
**Status:** Accepted
**Related:** `include/astra/ability.h`, `src/ability.cpp`, `src/game_input.cpp`, `src/game_rendering.cpp`, `src/game.cpp`, `src/save_file.cpp`

## Goal

Replace the current single-row, catalog-derived ability bar (`'1'..'5'` only, layout re-computed every frame) with a persistent, row-paged hotbar. Abilities occupy explicit slots. Multiple rows exist when the slot count exceeds one row's worth; the player pages between rows with PageUp/PageDown. Auto-assign on learn, compact on remove. Data model is ready for a future manual-rebind UI without migration.

## Non-goals

- Manual rebinding UI (drag/swap abilities between slots) — deferred; the data model supports it.
- Per-row labels or ability categorization.
- Cross-session persistence of the visible row (resets to 0 on load by design).
- SDL renderer changes (SDL work is deferred globally per project policy).
- Backwards-compatible save migration (pre-ship policy: reject old saves on schema bumps).

## Architecture overview

Two concerns separate cleanly:

**Persistent state (on `Player`):** an ordered, flat list of `SkillId` representing the hotbar's contents. Auto-assign appends on learn, removal erases and compacts. Slot `N` in the flat list maps deterministically to `(row = N / kSlotsPerRow, col = N % kSlotsPerRow)` at query time.

**Transient state (on `Game`):** the currently visible row index. Not persisted. Resets to 0 on new game / load. PageUp/PageDown mutate it with wrap.

A new module `include/astra/ability_bar.h` + `src/ability_bar.cpp` owns the logic:

```cpp
namespace astra::ability_bar {

inline constexpr int kSlotsPerRow     = 4;
inline constexpr int kMaxSlotsPerRow  = 9;
inline constexpr int kMaxRows         = 9;          // 9x9 = 81 — unreachable in practice
static_assert(kSlotsPerRow <= kMaxSlotsPerRow);

int  row_count(const Player&);                                // >= 1
std::optional<SkillId> slot_at(const Player&, int row, int col);
bool assign_on_learn(Player&, SkillId);                       // append if absent
bool remove_and_compact(Player&, SkillId);                    // erase + clamp visible row
bool use_slot(Game&, int visible_row, int col);               // replaces use_ability()

void page_up  (int& visible_row, const Player&);              // wrap
void page_down(int& visible_row, const Player&);              // wrap

} // namespace astra::ability_bar
```

The existing `get_ability_bar(player)` is removed. `use_ability(slot, game)` in `src/ability.cpp` is either deleted or reduced to a thin forward — all new call sites use `ability_bar::use_slot`.

`ability_bar` is pure game logic: no platform includes, no renderer references. Render reads through `slot_at`; input calls `use_slot` and the paging helpers.

## Data model

### `Player` change

One field added:

```cpp
std::vector<SkillId> ability_slots;  // flat, compact, dense
```

**Invariants** (enforced by `ability_bar::`):

- No duplicate `SkillId`s.
- Every entry satisfies `player_has_skill(player, id) == true`.
- No sentinel / empty entries; "empty" slots are simply positions past `.size()`.

### Learn / remove plumbing

Every place that grants a skill to the player must route through a single helper so `ability_slots` stays in sync:

```cpp
void grant_skill (Player&, SkillId);   // records skill + ability_bar::assign_on_learn
void revoke_skill(Player&, SkillId);   // unrecords skill + ability_bar::remove_and_compact
```

The implementation audits every existing "now has skill" site (skill-tree unlocks, dev `give` commands, starting kit) and converts them to call `grant_skill`. Same for any revocation paths. If no revocation path exists today, `revoke_skill` still ships — dev commands and future data revisions will use it.

### Assign / remove semantics

**`assign_on_learn(player, id)`:**
1. If `id` is already in `ability_slots`, return `false` (no-op).
2. If `ability_slots.size() >= kMaxRows * kSlotsPerRow`, log warning and return `false`.
3. Otherwise `ability_slots.push_back(id)` and return `true`.

**`remove_and_compact(player, id)`:**
1. Find `id` in `ability_slots`; if absent, return `false`.
2. `erase` it.
3. The caller (Game) is responsible for clamping `ability_bar_row_` after this returns — `ability_bar::` doesn't hold that state, but a helper `clamp_visible_row(int& row, const Player&)` is provided.

## Paging & input

### Game state

```cpp
int ability_bar_row_ = 0;   // transient, not saved
```

### Input handler (`src/game_input.cpp`)

Replace the `'1'..'5'` handler (currently lines 449-457):

```cpp
if (key >= '1' && key <= ('0' + ability_bar::kSlotsPerRow) && !wait_focused) {
    ability_bar::use_slot(*this, ability_bar_row_, key - '1');
    break;
}
```

Add paging cases (outside the wait-focused branch, above the existing number-key block):

```cpp
case KEY_PAGE_UP:
    ability_bar::page_up(ability_bar_row_, player_);
    break;
case KEY_PAGE_DOWN:
    ability_bar::page_down(ability_bar_row_, player_);
    break;
```

Wrap semantics:

- `page_up`:   `row = (row - 1 + row_count) % row_count;`
- `page_down`: `row = (row + 1) % row_count;`
- Both guard against `row_count == 0` (no-op).

### `use_slot(game, visible_row, col)`

1. Translate `flat_index = visible_row * kSlotsPerRow + col`.
2. If out of range or past `ability_slots.size()`, log `"No ability in that slot."` and return `false`.
3. Otherwise run the existing cooldown / weapon-requirement / adjacent-target / telegraph pipeline (moved verbatim from `use_ability`).

### Collision check

`KEY_PAGE_UP` / `KEY_PAGE_DOWN` are currently consumed only by modal viewers (dev console, lore viewer, playback viewer, map editor). The game-input handler does not consume them today, so no existing binding is displaced.

### Telegraph interaction

When `game.telegraph().active()`, input is already routed to the telegraph system first. PgUp/PgDn presses during telegraph are naturally swallowed — the ability-bar handler never runs. No extra guard needed.

## Rendering

### Layout (`src/game.cpp` `compute_layout`)

Change `vrows[7]` from `fixed(1)` to `fixed(3)`. The `fill()` on `vrows[4]` (main content) absorbs the two extra rows automatically.

```cpp
auto vrows = root.rows({
    fixed(1),    // [0] stats bar
    fixed(1),    // [1] HP bar / tabs
    fixed(1),    // [2] shield bar
    fixed(1),    // [3] XP bar
    fill(),      // [4] main content
    fixed(1),    // [5] bottom separator
    fixed(1),    // [6] effects
    fixed(3),    // [7] abilities  ← was fixed(1)
});
```

### `render_abilities_bar` (`src/game_rendering.cpp`)

Rewrites to three rows. Target visual:

```
   (PgUp/PgDn)  ▲
   ABILITIES:  1  <1> Jab  <2> Cleave  <3> Quickdraw  <4> Intimidate
  Page 1 of 2   ▼
```

**Row 0 (top):** left-aligned `(PgUp/PgDn)` dim; a single `▲` glyph at the right edge (`\xe2\x96\xb2`). Deep-dim when `row_count == 1`, else normal `UITag::TextDim`.

**Row 1 (middle):** the functional hotbar.
- `ABILITIES:` dim.
- Row-number indicator (the `1` after `ABILITIES:`) shows `visible_row + 1` in normal text color.
- For each column `c` in `[0, kSlotsPerRow)`:
  - `slot = slot_at(player, visible_row, c)`.
  - If present: `<K> Name` where `K = c + 1`. On cooldown → append `(N)` where `N = remaining ticks`, and tag `UITag::TextDim`. Off cooldown → tag `UITag::TextWarning` (unchanged from today).
  - If empty: `<K> ---`, dim. Stable key positions aid muscle memory.
- Separator between slots: two spaces (matches existing spacing).

**Row 2 (bottom):** `Page N of M` left-aligned dim; `▼` at the right edge (`\xe2\x96\xbc`). Deep-dim when `row_count == 1`, else normal dim.

**Empty-player state** (`ability_slots.empty()`):
- Row 0: arrows present but deep-dim.
- Row 1: `ABILITIES: [none]` dim (matches today's empty behavior).
- Row 2: `Page 1 of 1` dim.

Bar is visually present at full height (3 rows) but inert.

## Save format

Bump `SAVE_FILE_VERSION` from `44` → `45` in `include/astra/save_file.h` with comment: `// v45: persisted ability bar slots`.

Old saves are rejected by the existing check at `src/save_file.cpp:2110` — no new reject-path code needed.

### Writer (alongside other player fields in `src/save_file.cpp`)

```cpp
w.write_i32(static_cast<int32_t>(player.ability_slots.size()));
for (SkillId id : player.ability_slots) {
    w.write_i32(static_cast<int32_t>(id));
}
```

### Reader (symmetric)

```cpp
int32_t count = r.read_i32();
player.ability_slots.clear();
player.ability_slots.reserve(count);
for (int32_t i = 0; i < count; ++i) {
    player.ability_slots.push_back(static_cast<SkillId>(r.read_i32()));
}
```

### Post-load validation

After reading, drop entries whose `SkillId` the player no longer has, and drop duplicates:

```cpp
std::erase_if(player.ability_slots, [&](SkillId id) {
    return !player_has_skill(player, id);
});
// dedupe preserving first occurrence
std::unordered_set<SkillId> seen;
std::erase_if(player.ability_slots, [&](SkillId id) {
    return !seen.insert(id).second;
});
```

Cheap, idempotent, defensive against data revisions.

## Edge cases

| Case | Behavior |
|---|---|
| Slots at cap (`kMaxRows * kSlotsPerRow`) and new skill learned | `assign_on_learn` logs warning + returns `false`; skill still granted. |
| Visible row stale after removal | `clamp_visible_row` sets `row = max(0, row_count - 1)`. |
| PgUp/PgDn with `row_count == 1` | No-op (wrap back to same row). |
| Telegraph active | Early-return in input handler; paging input swallowed. |
| Empty `ability_slots` at runtime | Bar renders inert (per "empty-player state" above). |
| Loaded save has `SkillId` not known to current build | Post-load validation drops it. |

## Testing

Terminal renderer only (per project policy). Manual + dev-console driven:

1. Fresh character, learn 4 skills one at a time → bar fills `<1>..<4>`, `Page 1 of 1`, arrows deep-dim.
2. Learn 5th skill → still on row 1 (visible), `Page 1 of 2`, arrows normal-dim.
3. PgDn → visible row 2, shows `<1> <5th ability>`, `<2>..<4> ---`, `Page 2 of 2`.
4. PgDn from row 2 → wraps to row 1.
5. PgUp from row 1 → wraps to row 2.
6. Press `2` while on page 2 → no-op (slot 2 of row 2 is empty), logs `"No ability in that slot."`.
7. Un-learn the only row-2 skill via dev command → compacts; `Page 1 of 1`; arrows deep-dim.
8. Un-learn an ability that's mid-cooldown → removed cleanly; later slots shift left.
9. Save with 5 abilities in custom learn-order, reload → order preserved exactly.
10. Cooldown indicator `(N)` still renders after paging away and back.

## Impacted files

**New:**
- `include/astra/ability_bar.h`
- `src/ability_bar.cpp`

**Modified:**
- `include/astra/player.h` — add `std::vector<SkillId> ability_slots;`
- `include/astra/ability.h` — remove `get_ability_bar`; optionally remove or thin-forward `use_ability`.
- `src/ability.cpp` — remove `get_ability_bar`; move body of `use_ability` into `ability_bar::use_slot`.
- `include/astra/game.h` — add `int ability_bar_row_ = 0;`.
- `src/game.cpp` — `compute_layout` changes `vrows[7]` to `fixed(3)`.
- `src/game_input.cpp` — replace the `'1'..'5'` handler; add `KEY_PAGE_UP` / `KEY_PAGE_DOWN` cases.
- `src/game_rendering.cpp` — rewrite `render_abilities_bar` to 3-row layout.
- `src/save_file.cpp` — read/write `ability_slots`; bump version.
- `include/astra/save_file.h` — bump `SAVE_FILE_VERSION` to 45.
- Anywhere a skill is granted/revoked (to be enumerated during implementation) — route through `grant_skill` / `revoke_skill`.

## Risks

- **Grant-path audit:** if a skill is granted somewhere that isn't converted to `grant_skill`, that skill won't show up on the bar until the player re-learns it through an audited path. Mitigation: at startup (or once per session), run an idempotent reconciliation — for every learned skill not in `ability_slots`, call `assign_on_learn`. Cheap insurance against missed audit sites.
- **Row-count drift in visible row:** addressed by `clamp_visible_row`, but must be called after every learn/remove that could change `row_count()`. The reconciliation step above doubles as protection here.
