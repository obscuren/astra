# Loot Table System — Design

**Status:** Design approved 2026-04-25. Implementation pending.

## Problem

Today's item-spawning surface is split across three uncoordinated mechanisms:

1. **`generate_loot_drop()` + `random_*` pickers** (`src/item_gen.cpp`, `src/item_defs.cpp`) — hardcoded if-ladders for category and item selection.
2. **Three merchant stock generators** (`generate_merchant_stock`, `generate_arms_dealer_stock`, `generate_food_merchant_stock`) — hand-curated `push_back` lists with reputation-gating.
3. **Hardcoded one-offs** — engine coil at `game_world.cpp:401`, quest items at `game_world.cpp:1113`.

Adding a new item means editing every relevant if-chain *and* re-balancing the percentages so they still sum correctly. Rarity is rolled independently from item identity, so a Void Lance and a Combat Knife both have the same 1% Legendary chance — "Legendary" only means "two affixes glued on," not "scarce item." The unused `LootTable`/`LootEntry` structs at `item.h:264` already gesture at this gap.

## Goals

- **Single source of truth** — every item appears once in a typed table, tagged with where it can drop and at what rarity.
- **Tag-driven queries** — pull pools by `(source, rarity, level)` rather than per-context if-ladders.
- **Per-source category mix** — chests can favor crafting materials, NPCs can favor weapons, without duplicating item lists.
- **Rarity affects stats** — flat multiplier, separate from affixes (which stay as flavor).
- **Per-item rarity gating** — junk items don't roll Legendary; Legendary-only items don't dilute Common pools.
- **Merchants share the table** — `assemble_stock(...)` pulls from the same pool, with `Always`-quantity entries for guaranteed stock (rations, batteries) and `Random` entries for variable inventory.

## Non-goals

- **Quest items** stay hardcoded (`game_world.cpp:1113`) — current placement model is at world-gen time, not on NPC death; no pain point yet.
- **Engine coil hardcoded placement** stays — single deterministic placement, not a pool roll.
- **Affix system rewrite** — affixes remain as today; only their stat values become "small additive flavor" relative to the new rarity multiplier.
- **External data files (JSON/TOML)** — explicitly chose C++ static tables for single-binary build and zero parser surface. Can revisit later.

## Design

### The three-roll model

Every loot drop runs through three independent rolls:

1. **Gate** — caller's responsibility. Today: `game_combat.cpp:652`'s 50% NPC-kill chance. The new system assumes "we already decided to drop something." Per-source gate tuning is a future refinement.
2. **Identity** — pool query + weighted pick from `s_loot_table`.
3. **Rarity** — independent 50/30/15/4/1 roll (Common/Uncommon/Rare/Epic/Legendary), filtered to entries whose `[min_rarity, max_rarity]` includes the rolled tier.

Filter-then-roll semantics: **rarity is rolled first**, the table is filtered to entries eligible at that tier, then the weighted item pick runs over the filtered set. Effect: a Legendary roll always produces a Legendary-eligible item; rare items are scarce because the high-tier pool is small, not because they got "demoted" by a clamp.

### Data model

```cpp
// loot_source.h
enum class LootSource : uint64_t {
    None              = 0,
    NpcDrop           = 1ULL << 0,
    Chest             = 1ULL << 1,
    MerchantGeneral   = 1ULL << 2,
    MerchantArms      = 1ULL << 3,
    MerchantFood      = 1ULL << 4,
    ScavMerchant      = 1ULL << 5,
    BlackMarket       = 1ULL << 6,
    MaintenanceTunnel = 1ULL << 7,
    // 56 bits available for future sources
};

enum class Theme : uint8_t {
    None, Military, Ancient, Alien, Scrap, Civilian, Tech
};

enum class Category : uint8_t {
    Weapon, Armor, Shield, Accessory, Consumable, Battery, Junk,
    CraftingMaterial, ShipComponent, Ingredient, Cookbook, EnergyMod, QuestItem
};
// Coarser than ItemType: MeleeWeapon + RangedWeapon both → Category::Weapon.
// Mapping helper lives next to the table.

// loot_table.h
struct LootEntry {
    uint16_t    item_def_id;                       // → build_by_def_id()
    std::string identifier;                        // stable string handle ("plasma_pistol")
                                                   // used by `give item` and any future tooling
    Rarity      min_rarity, max_rarity;            // inclusive
    int         default_weight;                    // applies when source not in source_weights
    std::map<LootSource, int> source_weights;      // optional per-source weight override
                                                   // Empty for now — populated when experimenting
    uint64_t    source_mask;                       // OR of LootSource bits
    Theme       theme;
    int         min_level = 1;
    Category    category;
};

struct StockManifestEntry {
    enum Mode { Always, Random };
    Mode      mode;
    uint16_t  item_def_id    = 0;     // Always: which item; Random: ignored
    Category  category       = Category::Junk;  // Random: filter; Always: ignored
    int       quantity       = 1;
    int       min_reputation = 0;     // gate; entry skipped if rep < this
};
```

### Pipeline — `roll_loot(source, level, rng, forced_category = std::nullopt)`

The optional `forced_category` parameter exists so `assemble_stock`'s `Random` entries can pin a specific category without going through the per-source category roll. When `nullopt`, the normal category roll runs.

1. `Rarity rarity = roll_rarity(rng)` — unchanged 50/30/15/4/1 split.
2. **Pick category:**
   - If `forced_category` is set, use it directly.
   - Otherwise look up `s_category_weights[source]`. If present, weighted-pick a `Category`. If absent (or empty for that source), no category filter applies — flat roll across all categories.
3. Filter `s_loot_table` to entries where:
   - `(entry.source_mask & source) != 0`
   - `rarity >= entry.min_rarity && rarity <= entry.max_rarity`
   - `level >= entry.min_level`
   - `entry.category == picked_category` (if step 2 picked one)
4. Weighted pick by `entry.source_weights[source]` (if present) else `entry.default_weight`. If filtered set is empty, return a sentinel "junk" item or skip — TBD at impl time, but should not crash.
5. `Item item = build_by_def_id(entry.item_def_id)` — universal constructor (expanded in commit 1).
6. `scale_item_to_rarity(item, rarity)` — new helper. Multipliers ×1.00 / ×1.10 / ×1.25 / ×1.45 / ×1.75 applied to: damage modifier, av, dv, max_hp, durability, energy capacity, buy/sell value. Tunable; starting values are gut-feel.
7. `scale_item_to_level(item, level)` — existing helper, unchanged.
8. `apply_rarity_affixes(item, rarity, rng)` — existing helper, unchanged. Affix stat values stay additive; they don't compound with the rarity multiplier.

### Pipeline — `assemble_stock(manifest, faction_rep, level, rng)`

For each `StockManifestEntry`:
- Skip if `faction_rep < entry.min_reputation`.
- **`Always`**: push `entry.quantity` copies of `build_by_def_id(entry.item_def_id)`. No rarity scaling — utility goods (rations, basic batteries) stay vanilla Common.
- **`Random`**: invoke `roll_loot(<calling source>, level, rng, /*forced_category=*/entry.category)` and push `entry.quantity` results. Rarity, level scaling, and affixes apply as normal.

Reputation-gating moves from if-chains into per-entry `min_reputation` fields. Cookbook unlocks at rep ≥60, scav-only items at rep ≥0, etc., become declarative.

### File layout

```
include/astra/loot_source.h    // enums (LootSource, Theme, Category)
include/astra/loot_table.h     // LootEntry, StockManifestEntry, public API
src/loot_table.cpp             // s_loot_table, s_category_weights, roll_loot, assemble_stock
src/item_gen.cpp               // adds scale_item_to_rarity (existing file)
src/item_defs.cpp              // build_by_def_id expanded to all items (existing file)
```

The loot table sits in one file because it's *the* index. Adding an item means appending one entry. Splitting per-category would re-introduce the "where does X go" friction we're removing.

### `give` dev command refactor

Replace the ~85-entry if-else chain at `dev_console.cpp:420+` with a lookup over the loot table:

```
give item                                  list all items (identifier, name, category, [min..max] rarity)
give item <identifier>                     spawn Common, level 1
give item <identifier> <rarity>            spawn at given rarity, level 1
give item <identifier> <rarity> <level>    fully specified
```

- `<identifier>` matches against `LootEntry::identifier` (case-insensitive).
- `<rarity>` accepts both short forms (`c/u/r/e/l`) and full names (`common`, `legendary`).
- **Args are positional, not auto-detected.** To spawn a level-5 Common item you type `give item plasma_pistol c 5` — rarity must be specified before level. Avoids ambiguity between "Is `5` a rarity index or a level?"
- Pipeline: `build_by_def_id()` → `scale_item_to_rarity()` → `scale_item_to_level()`. **No affix roll** — dev command stays deterministic.
- `give item` (no args) prints a paginated list grouped by category with rarity range per row.
- `give ship <component>` extends to use the same lookup since ship components live in the table.

## Migration plan

Six commits, ordered for safe revert. Each commit leaves the build green.

1. **Scaffolding** — Add `loot_source.h`, `loot_table.h` (struct + enum definitions, no data yet). Add `scale_item_to_rarity()` to `item_gen.cpp`. Expand `build_by_def_id()` dispatch in `item_defs.cpp` to cover all ~85 items. No call-site changes — pure additive. Build passes; nothing uses the new symbols yet.

2. **Populate the loot table** — Author `s_loot_table` in `src/loot_table.cpp` with one entry per existing item. Add `s_category_weights` per-source tables (`NpcDrop`, `Chest`, `MaintenanceTunnel` to start). Table sits unused; dead-code lint may complain — acceptable for one commit.

3. **Switch loot drops** — Replace `generate_loot_drop()` callers (`game_combat.cpp:652`, `:873`) with `roll_loot(LootSource::NpcDrop, level, rng)`. Delete `generate_loot_drop`, `generate_random_weapon`, `generate_random_armor`, `random_ranged_weapon`, `random_melee_weapon`, `random_armor`, `random_junk` (and `random_shield` if unused after merchant migration in commit 4 — defer if still referenced). Verify drops in dev mode.

4. **Switch merchants** — Convert the three stock generators to manifest declarations + `assemble_stock(...)` calls. Manifests live as `static const std::vector<StockManifestEntry>` next to each caller in `src/npcs/merchant.cpp`, `src/npcs/hub_npcs.cpp`, `src/npcs/scav_merchant.cpp`, `src/npcs/black_market_vendor.cpp`. Reputation gates move from if-chains into `min_reputation` fields. Verify shop UI still shows correct stock per rep tier.

5. **Refactor `give` dev command** — Replace the if-else chain at `dev_console.cpp:420+` with table-driven lookup. Add the no-args list mode. Extend rarity/level args. Update help text. Also extend `give ship`.

6. **Cleanup — no parallel item-creation paths remain.** This commit is a deliberate sweep, not just dead-struct removal. After it lands, the *only* sanctioned ways to create an `Item` are:
   - `build_by_def_id(def_id)` — the universal constructor, called via `roll_loot()`, `assemble_stock()`, or the dev console's `give item`.
   - The two explicit hardcoded one-offs called out in non-goals: engine coil at `game_world.cpp:401`, quest items at `game_world.cpp:1113`.

   Concrete actions:
   - Remove the unused `LootTable` / `LootEntry` structs from `item.h:264-267`.
   - Audit every `build_*()` call site outside `item_defs.cpp` — `grep -rn "build_[a-z_]*()" src/ include/`. (The `build_*()` functions themselves stay in `item_defs.cpp` because `build_by_def_id` dispatches to them; what we're hunting is *direct external callers*.) Each remaining external call must be one of the two sanctioned hardcoded one-offs, or it gets converted to a `build_by_def_id` call (or a `roll_loot`/`assemble_stock` query if it's procedural).
   - Audit `dev_console.cpp` end-to-end — no leftover `if (args[N] == "...") item = build_X()` chains anywhere; everything routes through the table-driven path from commit 5.
   - Audit `npcs/` directory — confirm no merchant or NPC file still constructs items directly outside the manifest path from commit 4.
   - Remove any `random_*` picker that didn't already get deleted in commit 3 (e.g., `random_shield` if it's now unused after merchant migration).
   - Verify `build_by_def_id`'s dispatch covers every item in `s_loot_table` (the test added in commit 1's risks should already enforce this).

   **No rename** — `docs/formulas.md` rename to `docs/mechanics.md` is a manual pre-merge step (saved to memory at `project_loot_table_premerge.md`).

## What stays untouched

- `game_combat.cpp:652` — 50% drop gate. Per-source gate tuning is a future refinement.
- `game_world.cpp:401` — engine coil hardcoded placement.
- `game_world.cpp:1113` — quest item placement.
- `item_gen.cpp:13-37` — affix prefix/suffix pools and `apply_rarity_affixes`. Affix stat values stay additive; no re-tuning needed for the new pipeline (rarity multiplier dominates, affixes are flavor).

## Open questions deferred to implementation time

- **Empty filtered-pool fallback** — what does `roll_loot` return when no entry matches the filter (e.g., no Legendary-eligible items in pool at level 1)? Options: return junk-tier sentinel, return nothing (caller deals with `std::optional`), or assert in dev mode. Pick at impl time once we see how often it triggers.
- **Rarity multiplier values** — starting at ×1.00/1.10/1.25/1.45/1.75. Tune in playtest.
- **`Battery` as separate Category vs folded into `Consumable`** — kept separate so chests can tilt toward batteries without flooding consumables. Revisit if Battery stays a one-item category in practice.
- **`source_weights` field** — present in struct, empty in all entries at first. Will populate when experimenting with per-source tuning.

## Risks

- **`build_by_def_id` expansion is mechanical but big** — covers ~85 items in one commit. Low risk per item but easy to typo. Add a unit test that loops every entry in `s_loot_table` and asserts `build_by_def_id(entry.item_def_id).item_def_id == entry.item_def_id` to catch missing dispatch arms.
- **Merchant determinism shift** — current merchant stocks include some random rolls (`random_ranged_weapon()`, etc.) that already produce different stock per restock. Manifest `Random` entries preserve this. Verify the seed-stability semantics match (whatever they are today) so save/load doesn't reshuffle stocks unexpectedly.
- **Save compatibility** — items save by `item_def_id`, which is preserved. No save-format change. Existing saves load fine.
