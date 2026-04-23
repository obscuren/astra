# Cooking System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship a cooking system where the player combines ingredients in a 3-slot "pot" inside a new Cooking character-screen tab, gated on proximity to a cooking-source fixture via the existing aura system. Known, unknown (experimentation), and miss (Burnt Slop) paths all resolve through one `[c] cook` action.

**Architecture:**
- Capability layer: new `EffectId::CookingFireAura`. Emitted by any `FixtureTag::CookingSource` fixture via the aura registry. Cook code queries `has_effect(...)` — zero proximity logic at call sites.
- Data layer: recipes are first-class (`Recipe` struct, `recipe_catalog()` registry). Ingredients get their own `ItemType::Ingredient`. Cooked dishes hang a `DishOutput` off the Food item def. Cookbooks are `ItemType::Cookbook` with a `teaches_recipe_id`.
- Skill layer: new `Cat_Cooking` category gates the tab. `AdvancedFireMaking` sub-skill cross-references `CampMaking` and reduces its cooldown by 40%.
- UI layer: new `CharTab::Cooking` rendered by `draw_cooking`. Left half = 3 pot slots + ingredients list. Right half = collapsible cookbook catalog mirroring blueprints.

**Tech Stack:** C++20, `namespace astra`. Build: `cmake -B build -DDEV=ON && cmake --build build -j`. No unit-test framework — each task ends with a build + smoke check + commit.

**Reference spec:** `docs/superpowers/specs/2026-04-23-cooking-system-design.md`

---

## Task 1: Add cooking-related Effects

**Files:**
- Modify: `include/astra/effect.h`
- Modify: `src/effect.cpp`

- [ ] **Step 1: Add EffectId values**

In `include/astra/effect.h` add four enum values to `EffectId`:

```cpp
enum class EffectId : uint32_t {
    // ...existing values...
    CookingFireAura,
    WarmMeal,
    WellFed,
    Hearty,
};
```

Keep them grouped near other aura/food-adjacent entries if any exist.

- [ ] **Step 2: Declare factories**

In `include/astra/effect.h`:

```cpp
Effect make_cooking_fire_aura_ge();
Effect make_warm_meal_ge();
Effect make_well_fed_ge();
Effect make_hearty_ge();
```

- [ ] **Step 3: Implement factories**

In `src/effect.cpp`, follow the existing `make_*_ge` pattern (see `make_cozy_ge` for reference):

```cpp
Effect make_cooking_fire_aura_ge() {
    Effect e;
    e.id = EffectId::CookingFireAura;
    e.name = "Cooking Fire";
    e.description = "Near a cooking source.";
    e.duration = 1;           // refreshed every tick by the aura system
    e.modifiers = {};
    e.hidden_from_hud = true; // capability effect, not player-facing
    return e;
}

Effect make_warm_meal_ge() {
    Effect e;
    e.id = EffectId::WarmMeal;
    e.name = "Warm Meal";
    e.description = "A full stomach brings gentle healing.";
    e.duration = 100;
    e.modifiers = {};
    e.regen_bonus = 1;  // +1 HP per regen tick — adjust to existing field name if different
    return e;
}

Effect make_well_fed_ge() {
    Effect e;
    e.id = EffectId::WellFed;
    e.name = "Well Fed";
    e.description = "Sustained nourishment speeds recovery.";
    e.duration = 300;
    e.regen_bonus = 1;
    return e;
}

Effect make_hearty_ge() {
    Effect e;
    e.id = EffectId::Hearty;
    e.name = "Hearty";
    e.description = "The hero's feast girds you for battle.";
    e.duration = 500;
    e.modifiers.av = 1;
    e.modifiers.max_hp = 5;
    return e;
}
```

If the `Effect` struct does not have `hidden_from_hud` or `regen_bonus`, fall back to the closest equivalents (e.g., attach regen via existing modifier fields, or introduce `hidden_from_hud` as a new `bool` — this is a one-line addition if needed). Mirror whatever the existing `make_cozy_ge` or `make_regen_ge` does.

- [ ] **Step 4: Build**

```bash
cmake --build build -j
```

Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add include/astra/effect.h src/effect.cpp
git commit -m "feat(effect): add cooking-related GEs (cooking fire, warm meal, well fed, hearty)"
```

---

## Task 2: Register the CookingFireAura on FixtureTag::CookingSource

**Files:**
- Modify: `src/aura.cpp` (the `tag_auras()` function, around line 19)

- [ ] **Step 1: Add the registry entry**

In `src/aura.cpp`, inside `tag_auras()`:

```cpp
static const std::vector<std::pair<FixtureTag, Aura>>& tag_auras() {
    static const std::vector<std::pair<FixtureTag, Aura>> auras = {
        // ...existing entries...
        { FixtureTag::CookingSource, Aura{
            .template_effect = make_cooking_fire_aura_ge(),
            .radius = 2,
            .target_mask = TargetMask::Player,
            .source = AuraSource::Fixture,
        }},
    };
    return auras;
}
```

Field names must match the real `Aura` struct layout — see `include/astra/aura.h`. If the registry constructor used by other entries takes positional args, follow that form. If a designated-initializer style is used, keep it.

- [ ] **Step 2: Include `effect.h` if not already included**

Ensure `#include "astra/effect.h"` is present at the top of `src/aura.cpp`.

- [ ] **Step 3: Build**

```bash
cmake --build build -j
```

Expected: clean build.

- [ ] **Step 4: Smoke check**

Run `./build/astra` with dev mode. In the dev console (`~`), spawn or travel to a location with a Campfire (or place one via `CampMaking`). Step within 2 tiles. Open the character screen, navigate to the Effects/Status panel — `Cooking Fire` should appear briefly (or inspect `player_.effects` via the dev console if a dump command exists). Step away 3 tiles and wait one tick — it should disappear within a tick or two.

If there's no effects dump, add a temporary `log("has cooking: " + std::to_string(has_effect(player_.effects, EffectId::CookingFireAura)))` to the game loop for this smoke check and remove it before commit.

- [ ] **Step 5: Commit**

```bash
git add src/aura.cpp
git commit -m "feat(aura): emit CookingFireAura from FixtureTag::CookingSource"
```

---

## Task 3: Extend the item model — Ingredient, Cookbook, DishOutput

**Files:**
- Modify: `include/astra/item.h`
- Modify: `src/item.cpp` (extend `item_type_name`)

- [ ] **Step 1: Add new ItemType values**

In `include/astra/item.h`, inside `enum class ItemType`:

```cpp
enum class ItemType : uint8_t {
    // ...existing values, unchanged order...
    Ingredient,
    Cookbook,
};
```

Append at the end — order matters for save compatibility with any future versions, but we're bumping save version anyway.

- [ ] **Step 2: Add `DishOutput`**

In `include/astra/item.h`, above `struct Item`:

```cpp
struct DishOutput {
    int hunger_shift = 0;                // negative moves toward Satiated
    int hp_restore = 0;                  // instant heal on consume, clamped
    std::vector<EffectId> granted;       // GEs applied via add_effect
};
```

Include `"astra/effect.h"` at the top of `item.h` if not already present (check first — may be transitively included).

- [ ] **Step 3: Add `dish` and `teaches_recipe_id` to `Item`**

```cpp
struct Item {
    // ...existing fields unchanged...
    std::optional<DishOutput> dish;
    uint16_t teaches_recipe_id = 0;      // non-zero only when type == Cookbook
};
```

- [ ] **Step 4: Extend `item_type_name`**

In `src/item.cpp`:

```cpp
case ItemType::Ingredient: return "Ingredient";
case ItemType::Cookbook:   return "Cookbook";
```

- [ ] **Step 5: Build**

```bash
cmake --build build -j
```

Expected: clean build.

- [ ] **Step 6: Commit**

```bash
git add include/astra/item.h src/item.cpp
git commit -m "feat(item): add Ingredient/Cookbook types, DishOutput, dish and teaches_recipe_id fields"
```

---

## Task 4: Create the Recipe data model

**Files:**
- Create: `include/astra/recipe.h`
- Create: `src/recipe_defs.cpp`
- Modify: `CMakeLists.txt` (add `src/recipe_defs.cpp`)

- [ ] **Step 1: Write `include/astra/recipe.h`**

```cpp
#pragma once

#include "astra/effect.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace astra {

enum class RecipeCategory : uint8_t {
    Basic,
    Hearty,
    Gourmet,
    Survival,
};

const char* recipe_category_name(RecipeCategory c);

struct RecipeIngredient {
    uint16_t item_def_id = 0;
    int qty = 0;
};

struct Recipe {
    uint16_t id = 0;
    std::string name;
    std::string description;
    RecipeCategory category = RecipeCategory::Basic;
    std::vector<RecipeIngredient> ingredients;  // size 1..3
    uint16_t result_item_def_id = 0;
    int result_qty = 1;
};

struct PotSlot {
    uint16_t item_def_id = 0;   // 0 = empty
    int qty = 0;
};

const std::vector<Recipe>& recipe_catalog();
const Recipe* find_recipe(uint16_t id);

// Order-independent, quantity-exact. Returns nullptr if no match.
const Recipe* match_recipe(const std::array<PotSlot, 3>& slots);

} // namespace astra
```

- [ ] **Step 2: Write `src/recipe_defs.cpp` (skeleton with empty catalog)**

```cpp
#include "astra/recipe.h"

#include <algorithm>

namespace astra {

const char* recipe_category_name(RecipeCategory c) {
    switch (c) {
        case RecipeCategory::Basic:    return "Basic";
        case RecipeCategory::Hearty:   return "Hearty";
        case RecipeCategory::Gourmet:  return "Gourmet";
        case RecipeCategory::Survival: return "Survival";
    }
    return "Unknown";
}

const std::vector<Recipe>& recipe_catalog() {
    static const std::vector<Recipe> catalog = {
        // filled in Task 7
    };
    return catalog;
}

const Recipe* find_recipe(uint16_t id) {
    for (const auto& r : recipe_catalog()) if (r.id == id) return &r;
    return nullptr;
}

const Recipe* match_recipe(const std::array<PotSlot, 3>& slots) {
    // Canonicalise: collect (id, qty) for non-empty slots, sort by id.
    std::vector<RecipeIngredient> bag;
    for (const auto& s : slots) {
        if (s.item_def_id == 0 || s.qty <= 0) continue;
        bag.push_back({ s.item_def_id, s.qty });
    }
    if (bag.empty()) return nullptr;
    std::sort(bag.begin(), bag.end(), [](const RecipeIngredient& a, const RecipeIngredient& b){
        return a.item_def_id < b.item_def_id;
    });

    for (const auto& r : recipe_catalog()) {
        if (r.ingredients.size() != bag.size()) continue;
        std::vector<RecipeIngredient> ri(r.ingredients.begin(), r.ingredients.end());
        std::sort(ri.begin(), ri.end(), [](const RecipeIngredient& a, const RecipeIngredient& b){
            return a.item_def_id < b.item_def_id;
        });
        bool match = true;
        for (size_t i = 0; i < bag.size(); ++i) {
            if (ri[i].item_def_id != bag[i].item_def_id || ri[i].qty != bag[i].qty) {
                match = false;
                break;
            }
        }
        if (match) return &r;
    }
    return nullptr;
}

} // namespace astra
```

- [ ] **Step 3: Add `src/recipe_defs.cpp` to CMakeLists.txt**

Find the source list and add `src/recipe_defs.cpp` alongside other game sources.

- [ ] **Step 4: Build**

```bash
cmake -B build -DDEV=ON && cmake --build build -j
```

Expected: clean build with empty catalog.

- [ ] **Step 5: Commit**

```bash
git add include/astra/recipe.h src/recipe_defs.cpp CMakeLists.txt
git commit -m "feat(recipe): recipe data model, catalog scaffold, match_recipe"
```

---

## Task 5: Ingredient item defs

**Files:**
- Modify: `include/astra/item_defs.h`
- Modify: `src/item_defs.cpp`

- [ ] **Step 1: Declare builders**

In `include/astra/item_defs.h`, under a new section header `// --- Ingredients ---`:

```cpp
// --- Ingredients ---
Item build_raw_meat();
Item build_carrot();
Item build_flour();
Item build_herbs();
Item build_synth_protein();
```

Also declare assigned item_def_ids as constants (or rely on the numbering convention used by existing defs — check how `build_ration_pack` sets `item_def_id` and follow the same pattern). If item_def_ids are assigned by enum or constexpr constant, add:

```cpp
// Ingredient item_def_ids
constexpr uint16_t ITEM_DEF_RAW_MEAT      = /* next free id */;
constexpr uint16_t ITEM_DEF_CARROT        = /* next free id */;
constexpr uint16_t ITEM_DEF_FLOUR         = /* next free id */;
constexpr uint16_t ITEM_DEF_HERBS         = /* next free id */;
constexpr uint16_t ITEM_DEF_SYNTH_PROTEIN = /* next free id */;
```

If item_def_ids are assigned inline in the `build_*` function and there's no central constants table, skip this block and assign unique integers directly inside each `build_*`. Use `grep -rn "item_def_id = " src/item_defs.cpp` to see the current max and pick the next range.

- [ ] **Step 2: Implement builders**

In `src/item_defs.cpp`, mirror `build_ration_pack`'s shape. Example for raw meat:

```cpp
Item build_raw_meat() {
    Item it;
    it.name = "Raw Meat";
    it.description = "A cut of uncooked meat. Needs cooking to be edible.";
    it.type = ItemType::Ingredient;
    it.item_def_id = ITEM_DEF_RAW_MEAT;
    it.rarity = Rarity::Common;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 4;
    it.sell_value = 1;
    return it;
}
```

Repeat for the other four with appropriate names/descriptions/values:
- Carrot — "A crunchy root vegetable." weight 1, buy 2
- Flour — "A sack of milled grain." weight 1, buy 2
- Herbs — "A bundle of aromatic herbs." weight 1, buy 3
- Synth-Protein — "A brick of station-processed protein." weight 1, buy 5

- [ ] **Step 3: Build**

```bash
cmake --build build -j
```

Expected: clean build.

- [ ] **Step 4: Smoke check**

In dev console, spawn one of each ingredient (via `spawn <name>` or similar dev command — check `src/dev_console.cpp` for the exact syntax). Verify they appear in inventory with correct name, stack properly, and have the "Ingredient" type label.

- [ ] **Step 5: Commit**

```bash
git add include/astra/item_defs.h src/item_defs.cpp
git commit -m "feat(items): add ingredient item defs (raw meat, carrot, flour, herbs, synth-protein)"
```

---

## Task 6: Cooked dish, Burnt Slop, and cookbook item defs

**Files:**
- Modify: `include/astra/item_defs.h`
- Modify: `src/item_defs.cpp`

- [ ] **Step 1: Declare builders**

In `include/astra/item_defs.h`:

```cpp
// --- Cooked dishes ---
Item build_cooked_meat();
Item build_bowl_of_broth();
Item build_flatbread();
Item build_hearty_stew();
Item build_protein_bake();
Item build_heros_feast();
Item build_burnt_slop();

// --- Cookbooks ---
Item build_cookbook_hearty_stew();
Item build_cookbook_protein_bake();
Item build_cookbook_heros_feast();
```

Assign `item_def_id` constants analogous to Task 5.

- [ ] **Step 2: Implement cooked dish builders**

Each cooked dish populates `ItemType::Food`, `usable = true`, and a `DishOutput`. Example:

```cpp
Item build_hearty_stew() {
    Item it;
    it.name = "Hearty Stew";
    it.description = "A thick, meaty stew. Restores stamina and fortifies you.";
    it.type = ItemType::Food;
    it.item_def_id = ITEM_DEF_HEARTY_STEW;
    it.rarity = Rarity::Uncommon;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 20;
    it.sell_value = 5;
    it.usable = true;
    it.dish = DishOutput{
        .hunger_shift = -2,
        .hp_restore = 5,
        .granted = { EffectId::WellFed },
    };
    return it;
}
```

Fill in the remaining dishes per the spec's recipe table. Burnt Slop:

```cpp
Item build_burnt_slop() {
    Item it;
    it.name = "Burnt Slop";
    it.description = "A charred, inedible-looking mass. Better than nothing.";
    it.type = ItemType::Food;
    it.item_def_id = ITEM_DEF_BURNT_SLOP;
    it.rarity = Rarity::Common;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = 0;
    it.sell_value = 0;
    it.usable = true;
    it.dish = DishOutput{ .hunger_shift = -1, .hp_restore = 0, .granted = {} };
    return it;
}
```

Also update `build_ration_pack` to populate a `DishOutput`:

```cpp
it.dish = DishOutput{ .hunger_shift = -1, .hp_restore = 5, .granted = {} };
```

(Remove any ration-specific hardcoded hunger/HP handling that would otherwise double-apply — see Task 10 for consumption rewrite.)

- [ ] **Step 3: Implement cookbook builders**

```cpp
Item build_cookbook_hearty_stew() {
    Item it;
    it.name = "Cookbook: Hearty Stew";
    it.description = "A well-thumbed recipe card for a hearty stew.";
    it.type = ItemType::Cookbook;
    it.item_def_id = ITEM_DEF_COOKBOOK_HEARTY_STEW;
    it.rarity = Rarity::Uncommon;
    it.weight = 1;
    it.stackable = false;
    it.stack_count = 1;
    it.buy_value = 40;
    it.sell_value = 10;
    it.usable = true;
    it.teaches_recipe_id = 10;  // matches Recipe::id in Task 7
    return it;
}
```

Same pattern for Protein Bake (recipe 11, buy 35) and Hero's Feast (recipe 12, buy 80, rarity Rare).

- [ ] **Step 4: Build**

```bash
cmake --build build -j
```

Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add include/astra/item_defs.h src/item_defs.cpp
git commit -m "feat(items): add cooked dishes, Burnt Slop, and cookbook items"
```

---

## Task 7: Populate the recipe catalog

**Files:**
- Modify: `src/recipe_defs.cpp`

- [ ] **Step 1: Fill the catalog**

Replace the empty catalog in `recipe_catalog()`:

```cpp
const std::vector<Recipe>& recipe_catalog() {
    static const std::vector<Recipe> catalog = {
        {
            .id = 1,
            .name = "Cooked Meat",
            .description = "Meat cooked over a fire. Simple and satisfying.",
            .category = RecipeCategory::Basic,
            .ingredients = { { ITEM_DEF_RAW_MEAT, 1 } },
            .result_item_def_id = ITEM_DEF_COOKED_MEAT,
            .result_qty = 1,
        },
        {
            .id = 2,
            .name = "Warm Broth",
            .description = "A simple vegetable broth. Warms the bones.",
            .category = RecipeCategory::Basic,
            .ingredients = { { ITEM_DEF_CARROT, 1 }, { ITEM_DEF_HERBS, 1 } },
            .result_item_def_id = ITEM_DEF_BOWL_OF_BROTH,
            .result_qty = 1,
        },
        {
            .id = 3,
            .name = "Campfire Bread",
            .description = "Flatbread cooked on a hot stone.",
            .category = RecipeCategory::Basic,
            .ingredients = { { ITEM_DEF_FLOUR, 2 } },
            .result_item_def_id = ITEM_DEF_FLATBREAD,
            .result_qty = 1,
        },
        {
            .id = 10,
            .name = "Hearty Stew",
            .description = "Meat, roots, and herbs simmered into a thick stew.",
            .category = RecipeCategory::Hearty,
            .ingredients = {
                { ITEM_DEF_RAW_MEAT, 2 },
                { ITEM_DEF_CARROT, 1 },
                { ITEM_DEF_HERBS, 1 },
            },
            .result_item_def_id = ITEM_DEF_HEARTY_STEW,
            .result_qty = 1,
        },
        {
            .id = 11,
            .name = "Protein Bake",
            .description = "Synth-protein baked into a filling loaf.",
            .category = RecipeCategory::Hearty,
            .ingredients = {
                { ITEM_DEF_SYNTH_PROTEIN, 2 },
                { ITEM_DEF_FLOUR, 1 },
            },
            .result_item_def_id = ITEM_DEF_PROTEIN_BAKE,
            .result_qty = 1,
        },
        {
            .id = 12,
            .name = "Hero's Feast",
            .description = "A legendary dish said to steel a warrior for any trial.",
            .category = RecipeCategory::Gourmet,
            .ingredients = {
                { ITEM_DEF_RAW_MEAT, 1 },
                { ITEM_DEF_HERBS, 1 },
                { ITEM_DEF_SYNTH_PROTEIN, 1 },
            },
            .result_item_def_id = ITEM_DEF_HEROS_FEAST,
            .result_qty = 1,
        },
    };
    return catalog;
}
```

- [ ] **Step 2: Build**

```bash
cmake --build build -j
```

Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add src/recipe_defs.cpp
git commit -m "feat(recipe): populate catalog with 6 starting + cookbook recipes"
```

---

## Task 8: Player state — known_recipes and cooking_slots

**Files:**
- Modify: `include/astra/player.h`
- Modify: character-creation code (search for where `learned_skills` is seeded — likely `src/game.cpp` or a character-setup file)

- [ ] **Step 1: Add fields to `Player`**

In `include/astra/player.h`:

```cpp
#include "astra/recipe.h"  // for PotSlot

struct Player {
    // ...existing fields...
    std::vector<uint16_t> known_recipes;
    std::array<PotSlot, 3> cooking_slots;
};
```

- [ ] **Step 2: Seed starting recipes at character creation**

Find the location where `learned_skills` or `inventory` is initialised for a new game (search `grep -rn "learned_skills" src/`). In the same place, seed:

```cpp
player_.known_recipes = { 1, 2, 3 };  // Cooked Meat, Warm Broth, Campfire Bread
```

For the DevCommander class, seed all six IDs for testing convenience:

```cpp
if (player_.player_class == PlayerClass::DevCommander) {
    player_.known_recipes = { 1, 2, 3, 10, 11, 12 };
}
```

- [ ] **Step 3: Build**

```bash
cmake --build build -j
```

Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add include/astra/player.h src/game.cpp  # and any other files touched
git commit -m "feat(player): add known_recipes, cooking_slots; seed starting recipes"
```

---

## Task 9: Cat_Cooking skill category and AdvancedFireMaking

**Files:**
- Modify: `include/astra/skill_defs.h`
- Modify: `src/skill_defs.cpp`
- Modify: `src/ability.cpp` (CampMakingAbility cooldown)
- Modify: skill-purchase UI (wherever `Cat_Tinkering` purchase is validated — likely `src/character_screen.cpp`)

- [ ] **Step 1: Add SkillIds**

In `include/astra/skill_defs.h`:

```cpp
enum class SkillId : uint32_t {
    // ...existing values...
    Cat_Cooking         = 11,
    AdvancedFireMaking  = 1100,
};
```

- [ ] **Step 2: Register the category**

In `src/skill_defs.cpp`, add to the catalog builder alongside the existing categories:

```cpp
{ SkillCategory{
    .unlock_id = SkillId::Cat_Cooking,
    .name = "Cooking",
    .description = "Prepare food to restore hunger, heal, and grant meal buffs.",
    .sp_cost = 50,
    .skills = {
        SkillDef{
            .id = SkillId::AdvancedFireMaking,
            .name = "Advanced Fire Making",
            .description = "Requires Camp Making. Reduces the Camp Making cooldown by 40%.",
            .passive = true,
            .sp_cost = 75,
        },
    },
}},
```

- [ ] **Step 3: Cross-category prereq enforcement**

Find where the skill-tree UI spends a skill point. It's likely a function in `src/character_screen.cpp` that handles "purchase skill" input. Before confirming a purchase of `AdvancedFireMaking`, check:

```cpp
if (skill_id == SkillId::AdvancedFireMaking &&
    !player_has_skill(*player_, SkillId::CampMaking)) {
    log_or_toast("Requires Camp Making skill.");
    return;
}
```

Also reject loads where `AdvancedFireMaking` is learned without `CampMaking` — add a validator in save-load (optional, nice-to-have).

- [ ] **Step 4: Apply cooldown reduction**

In `src/ability.cpp`, `CampMakingAbility` reads `cooldown_ticks = world::camp_making_cooldown_ticks` (line ~159). Change it to compute at use-time rather than construct-time — add a virtual or override that consults the player:

```cpp
int effective_cooldown(const Player& player) const {
    int base = world::camp_making_cooldown_ticks;
    if (player_has_skill(player, SkillId::AdvancedFireMaking)) {
        base = (base * 60) / 100;  // -40%
    }
    return base;
}
```

Then wherever the ability applies its cooldown effect, use `effective_cooldown(player_)` instead of the static field. If `cooldown_ticks` is a stored field read by generic ability-dispatch code, prefer recomputing it in `tick()` or in the use-ability path. Adjust to match the existing cooldown plumbing — do not introduce a parallel system.

- [ ] **Step 5: Build**

```bash
cmake --build build -j
```

Expected: clean build.

- [ ] **Step 6: Smoke check**

Start a new game. In the skill screen, verify the Cooking category appears. Purchase `Cat_Cooking`, then attempt to purchase `AdvancedFireMaking` without having `CampMaking` — should be blocked with the message above. Purchase `Cat_Wayfinding → CampMaking`, then `AdvancedFireMaking` — should succeed. Use the camp-making ability and confirm the cooldown is shorter than baseline.

- [ ] **Step 7: Commit**

```bash
git add include/astra/skill_defs.h src/skill_defs.cpp src/ability.cpp src/character_screen.cpp
git commit -m "feat(skills): add Cat_Cooking category and AdvancedFireMaking (reduces CampMaking cooldown)"
```

---

## Task 10: Extend `Game::use_item` for DishOutput and Cookbooks

**Files:**
- Modify: `src/game_rendering.cpp` (the `use_item` function around line 494)

- [ ] **Step 1: Replace the Food branch with DishOutput-aware consumption**

Current Food branch (around line 500):

```cpp
case ItemType::Food: {
    int heal = item.buy_value * 5;
    if (heal < 5) heal = 5;
    player_.hp = std::min(player_.hp + heal, player_.max_hp);
    if (player_.hunger > HungerState::Satiated)
        player_.hunger = static_cast<HungerState>(
            static_cast<uint8_t>(player_.hunger) - 1);
    log("You eat the " + item.name + ". (+" +
        std::to_string(heal) + " HP)");
    break;
}
```

Replace with:

```cpp
case ItemType::Food: {
    if (!item.dish) {
        // Fallback for any Food without a DishOutput (shouldn't happen post-migration).
        log("You can't eat " + item.name + ".");
        return;
    }
    const auto& d = *item.dish;
    // Hunger shift (negative moves toward Satiated).
    int h = static_cast<int>(player_.hunger) + d.hunger_shift;
    h = std::clamp(h, static_cast<int>(HungerState::Satiated),
                      static_cast<int>(HungerState::Starving));
    player_.hunger = static_cast<HungerState>(h);
    // HP restore.
    if (d.hp_restore > 0) {
        int max = player_.effective_max_hp();
        player_.hp = std::min(player_.hp + d.hp_restore, max);
    }
    // Granted GEs.
    for (EffectId eid : d.granted) {
        add_effect(player_.effects, effect_for_id(eid));
    }
    log("You eat the " + item.name + ".");
    break;
}
```

`effect_for_id` is a dispatcher that returns the appropriate `make_*_ge()` result for an `EffectId`. If no such helper exists, write a small one in `src/effect.cpp`:

```cpp
Effect effect_for_id(EffectId id) {
    switch (id) {
        case EffectId::WarmMeal:        return make_warm_meal_ge();
        case EffectId::WellFed:         return make_well_fed_ge();
        case EffectId::Hearty:          return make_hearty_ge();
        case EffectId::CookingFireAura: return make_cooking_fire_aura_ge();
        default: return Effect{};
    }
}
```

Declare in `include/astra/effect.h`.

- [ ] **Step 2: Add the Cookbook branch**

```cpp
case ItemType::Cookbook: {
    uint16_t rid = item.teaches_recipe_id;
    if (rid == 0) {
        log("This cookbook is blank.");
        return;
    }
    auto& known = player_.known_recipes;
    if (std::find(known.begin(), known.end(), rid) != known.end()) {
        log("You already know this recipe.");
        return;  // do not consume
    }
    known.push_back(rid);
    const Recipe* r = find_recipe(rid);
    std::string name = r ? r->name : "a new recipe";
    log("*You learn to cook " + name + ".*");
    break;
}
```

Add `#include "astra/recipe.h"` and `#include <algorithm>` at the top if not already present.

- [ ] **Step 3: Build**

```bash
cmake --build build -j
```

Expected: clean build.

- [ ] **Step 4: Smoke check**

Spawn a `ration_pack` — use it. Hunger should shift one step, HP should go up by 5. Spawn `Cookbook: Hearty Stew` — use it. Log should show "*You learn to cook Hearty Stew.*", book is consumed, `player_.known_recipes` contains `10`. Use another copy — should say "You already know this recipe." and not consume.

- [ ] **Step 5: Commit**

```bash
git add src/game_rendering.cpp include/astra/effect.h src/effect.cpp
git commit -m "feat(consume): DishOutput-driven Food, Cookbook learns recipes"
```

---

## Task 11: CharTab::Cooking — stub + gating

**Files:**
- Modify: `include/astra/character_screen.h`
- Modify: `src/character_screen.cpp`

- [ ] **Step 1: Add the tab**

In `include/astra/character_screen.h`, extend the `CharTab` enum (after the existing entries):

```cpp
enum class CharTab : uint8_t {
    // ...existing...
    Cooking,
};
```

Also add state fields for the cooking UI (used in Task 12):

```cpp
enum class CookingFocus : uint8_t { Slots, Ingredients, Cookbook };

// Inside CharacterScreen class:
CookingFocus cooking_focus_ = CookingFocus::Slots;
int cooking_slot_cursor_ = 0;       // 0..2
int cooking_ingredient_cursor_ = 0;
int cooking_cookbook_cursor_ = 0;   // flattened category+recipe index
bool cooking_qty_prompt_active_ = false;
int  cooking_qty_prompt_value_ = 1;
uint16_t cooking_qty_prompt_item_def_id_ = 0;
std::vector<RecipeCategory> cooking_expanded_categories_;  // tracks ▾ state
```

- [ ] **Step 2: Register tab in the tab list**

Find the tab list / labels in `character_screen.cpp` (search for where `CharTab::Tinkering` is added) and append `CharTab::Cooking` with label `"Cooking"`.

- [ ] **Step 3: Wire up the draw dispatch**

In `character_screen.cpp` around line 1127:

```cpp
case CharTab::Cooking: draw_cooking(full); break;
```

Declare `void draw_cooking(UIContext& ctx);` on the class.

- [ ] **Step 4: Implement stub**

```cpp
void CharacterScreen::draw_cooking(UIContext& ctx) {
    if (!player_has_skill(*player_, SkillId::Cat_Cooking)) {
        draw_stub(ctx, "Kitchen unavailable.");
        ctx.text({.x = ctx.width() / 2 - 22, .y = ctx.height() / 2 + 1,
                  .content = "Learn the Cooking skill to use the kitchen.",
                  .tag = UITag::TextDim});
        return;
    }
    // Full UI comes in Task 12 — placeholder for now.
    draw_section_header(ctx, 0, "COOKING");
    ctx.text({.x = 3, .y = 3, .content = "(kitchen UI — implemented in next task)",
              .tag = UITag::TextDim});
}
```

- [ ] **Step 5: Build**

```bash
cmake --build build -j
```

Expected: clean build.

- [ ] **Step 6: Smoke check**

Open character screen, tab to Cooking. Without `Cat_Cooking`, stub prompt appears. Learn `Cat_Cooking` via dev console, reopen — should show the "COOKING" header and placeholder line.

- [ ] **Step 7: Commit**

```bash
git add include/astra/character_screen.h src/character_screen.cpp
git commit -m "feat(ui): CharTab::Cooking scaffold with skill gate"
```

---

## Task 12: Cooking tab — full UI rendering

**Files:**
- Modify: `src/character_screen.cpp` (replace the placeholder in `draw_cooking`)

This task renders the full layout. Input handling comes in Task 13.

- [ ] **Step 1: Layout constants + section headers**

Replace the placeholder body of `draw_cooking`:

```cpp
void CharacterScreen::draw_cooking(UIContext& ctx) {
    if (!player_has_skill(*player_, SkillId::Cat_Cooking)) {
        draw_stub(ctx, "Kitchen unavailable.");
        ctx.text({.x = ctx.width() / 2 - 22, .y = ctx.height() / 2 + 1,
                  .content = "Learn the Cooking skill to use the kitchen.",
                  .tag = UITag::TextDim});
        return;
    }

    int w = ctx.width();
    int half = w / 2;

    // ── Left half ──
    draw_section_header(ctx, 0, "COOKING POT");
    draw_pot_slots(ctx, half);                // Step 2
    draw_aura_indicator(ctx, half, 7);        // Step 3

    draw_section_header(ctx, 10, "INGREDIENTS");
    draw_ingredient_list(ctx, half, 12);      // Step 4

    // ── Right half ──
    int right_x = half + 2;
    draw_section_header_at(ctx, right_x, 0, "COOKBOOK");
    draw_cookbook(ctx, right_x, 2, w - right_x - 2);  // Step 5

    // ── Quantity prompt overlay ──
    if (cooking_qty_prompt_active_) {
        draw_qty_prompt(ctx, half);           // Step 6
    }
}
```

`draw_section_header_at` is a new variant that takes an x-offset — if the existing `draw_section_header` already accepts one, use that; otherwise add a small helper. If both a single- and dual-column version are needed, duplicate the existing body and parametrise.

- [ ] **Step 2: Implement `draw_pot_slots`**

```cpp
void CharacterScreen::draw_pot_slots(UIContext& ctx, int half) {
    int slot_w = 13;
    int slot_gap = 2;
    int total = slot_w * 3 + slot_gap * 2;
    int start_x = (half - total) / 2;
    int y = 2;

    for (int i = 0; i < 3; ++i) {
        int sx = start_x + i * (slot_w + slot_gap);
        bool selected = (cooking_focus_ == CookingFocus::Slots && cooking_slot_cursor_ == i);
        Color border = selected ? Color::Yellow : Color::DarkGray;

        // Box (top/mid/bottom rows).
        ctx.put(sx, y, BoxDraw::TL, border);
        for (int k = 1; k < slot_w - 1; ++k) ctx.put(sx + k, y, BoxDraw::H, border);
        ctx.put(sx + slot_w - 1, y, BoxDraw::TR, border);
        ctx.put(sx, y + 1, BoxDraw::V, border);
        ctx.put(sx + slot_w - 1, y + 1, BoxDraw::V, border);
        ctx.put(sx, y + 2, BoxDraw::BL, border);
        for (int k = 1; k < slot_w - 1; ++k) ctx.put(sx + k, y + 2, BoxDraw::H, border);
        ctx.put(sx + slot_w - 1, y + 2, BoxDraw::BR, border);

        // Label below.
        std::string label = "SLOT " + std::to_string(i + 1);
        int lx = sx + (slot_w - static_cast<int>(label.size())) / 2;
        ctx.text({.x = lx, .y = y + 3, .content = label,
                  .tag = selected ? UITag::TextWarning : UITag::TextDim});

        // Contents.
        const auto& slot = player_->cooking_slots[i];
        if (slot.item_def_id == 0) {
            std::string empty = "empty";
            int ex = sx + (slot_w - static_cast<int>(empty.size())) / 2;
            ctx.text({.x = ex, .y = y + 1, .content = empty, .tag = UITag::TextDim});
        } else {
            std::string name = short_item_name(slot.item_def_id, slot_w - 4);
            std::string display = name + " x" + std::to_string(slot.qty);
            int dx = sx + (slot_w - static_cast<int>(display.size())) / 2;
            ctx.text({.x = dx, .y = y + 1, .content = display});
        }
    }
}
```

`short_item_name(item_def_id, max_len)` resolves the item def's name and truncates. If no central def-lookup exists, build one inline via `item_visual()` + a name map, or scan `item_defs` — whatever the existing code uses to resolve def_ids to names.

- [ ] **Step 3: Implement `draw_aura_indicator`**

```cpp
void CharacterScreen::draw_aura_indicator(UIContext& ctx, int half, int y) {
    bool near_fire = has_effect(player_->effects, EffectId::CookingFireAura);
    std::string text = near_fire ? "● Near a cooking fire"
                                 : "○ No cooking source nearby";
    UITag tag = near_fire ? UITag::TextSuccess : UITag::TextDim;
    int x = (half - static_cast<int>(text.size())) / 2;
    ctx.text({.x = x, .y = y, .content = text, .tag = tag});
}
```

- [ ] **Step 4: Implement `draw_ingredient_list`**

```cpp
void CharacterScreen::draw_ingredient_list(UIContext& ctx, int half, int y) {
    // Collect all ItemType::Ingredient stacks from inventory.
    std::vector<const Item*> list;
    for (const auto& it : player_->inventory.items)
        if (it.type == ItemType::Ingredient) list.push_back(&it);

    if (list.empty()) {
        ctx.text({.x = 3, .y = y, .content = "(no ingredients)", .tag = UITag::TextDim});
        return;
    }

    for (size_t i = 0; i < list.size(); ++i) {
        bool selected = (cooking_focus_ == CookingFocus::Ingredients &&
                         cooking_ingredient_cursor_ == static_cast<int>(i));
        std::string cursor = selected ? "▸ " : "  ";
        std::string line = cursor + list[i]->name;
        ctx.text({.x = 3, .y = y + static_cast<int>(i), .content = line,
                  .tag = selected ? UITag::TextWarning : UITag::TextDefault});
        std::string qty = "x" + std::to_string(list[i]->stack_count);
        ctx.text({.x = half - 2 - static_cast<int>(qty.size()),
                  .y = y + static_cast<int>(i), .content = qty,
                  .tag = UITag::TextDim});
    }
}
```

- [ ] **Step 5: Implement `draw_cookbook`**

```cpp
void CharacterScreen::draw_cookbook(UIContext& ctx, int x, int y, int w) {
    // Group known recipes by category, in the fixed order Basic/Hearty/Gourmet/Survival.
    const RecipeCategory order[] = {
        RecipeCategory::Basic, RecipeCategory::Hearty,
        RecipeCategory::Gourmet, RecipeCategory::Survival,
    };

    int row = 0;
    int flat_index = 0;  // matches cooking_cookbook_cursor_

    for (RecipeCategory cat : order) {
        // Collect recipes in this category the player knows.
        std::vector<const Recipe*> recipes;
        for (uint16_t rid : player_->known_recipes) {
            const Recipe* r = find_recipe(rid);
            if (r && r->category == cat) recipes.push_back(r);
        }
        if (recipes.empty()) continue;

        bool expanded = std::find(cooking_expanded_categories_.begin(),
                                  cooking_expanded_categories_.end(),
                                  cat) != cooking_expanded_categories_.end();
        bool cat_selected = (cooking_focus_ == CookingFocus::Cookbook &&
                             cooking_cookbook_cursor_ == flat_index);

        std::string marker = expanded ? "▾ " : "▸ ";
        std::string header = marker + recipe_category_name(cat);
        ctx.text({.x = x, .y = y + row, .content = header,
                  .tag = cat_selected ? UITag::TextWarning : UITag::TextHeader});
        ++row; ++flat_index;

        if (!expanded) continue;

        for (const Recipe* r : recipes) {
            bool sel = (cooking_focus_ == CookingFocus::Cookbook &&
                        cooking_cookbook_cursor_ == flat_index);
            std::string prefix = sel ? "  ▸ " : "    ";
            ctx.text({.x = x, .y = y + row, .content = prefix + r->name,
                      .tag = sel ? UITag::TextWarning : UITag::TextDefault});
            ++row;

            if (sel) {
                // Expanded detail: ingredients, then produces, then description.
                for (const auto& ing : r->ingredients) {
                    std::string line = "      + " + std::to_string(ing.qty) + "x " +
                                       short_item_name(ing.item_def_id, w - 12);
                    ctx.text({.x = x, .y = y + row, .content = line,
                              .tag = UITag::TextDim});
                    ++row;
                }
                ++row;  // blank line
                std::string produces = "      Creates: " +
                                       short_item_name(r->result_item_def_id, w - 16);
                ctx.text({.x = x, .y = y + row, .content = produces,
                          .tag = UITag::TextDefault});
                ++row;

                // Dish GEs summary.
                const Item* dish_def_proto = nullptr;
                // A lightweight way: call the build_* by def_id via a registry.
                // If no registry lookup exists, render just the description.
                if (!r->description.empty()) {
                    ++row;
                    ctx.text({.x = x, .y = y + row, .content = "      " + r->description,
                              .tag = UITag::TextDim});
                    ++row;
                }
            }
            ++flat_index;
        }
    }

    if (row == 0) {
        ctx.text({.x = x, .y = y, .content = "(no recipes learned)",
                  .tag = UITag::TextDim});
    }
}
```

- [ ] **Step 6: Implement `draw_qty_prompt`**

```cpp
void CharacterScreen::draw_qty_prompt(UIContext& ctx, int half) {
    int y = 20;  // near the bottom of the left pane — adjust to ctx height if needed.
    int available = inventory_qty_for_def(*player_, cooking_qty_prompt_item_def_id_);
    std::string name = short_item_name(cooking_qty_prompt_item_def_id_, 16);
    std::string line = "How many " + name + "? [ " +
                       std::to_string(cooking_qty_prompt_value_) +
                       " ]   (1-" + std::to_string(available) +
                       ")   Enter=confirm  Esc=cancel";
    ctx.text({.x = 2, .y = y, .content = line, .tag = UITag::TextWarning});
}
```

Helper:

```cpp
int inventory_qty_for_def(const Player& p, uint16_t def_id) {
    int total = 0;
    for (const auto& it : p.inventory.items)
        if (it.item_def_id == def_id) total += it.stack_count;
    return total;
}
```

- [ ] **Step 7: Build**

```bash
cmake --build build -j
```

Expected: clean build.

- [ ] **Step 8: Smoke check**

Open Cooking tab. Header "COOKING POT", three empty slots, aura indicator (○ or ● depending on location), INGREDIENTS section. Right half shows COOKBOOK with starting recipes grouped under "▸ Basic". No input works yet — that's Task 13.

- [ ] **Step 9: Commit**

```bash
git add include/astra/character_screen.h src/character_screen.cpp
git commit -m "feat(ui): render cooking tab (pot slots, ingredients, collapsible cookbook)"
```

---

## Task 13: Cooking tab — input handling and cook resolution

**Files:**
- Modify: `src/character_screen.cpp` (input path — look for where `CharTab::Tinkering` input is routed)

- [ ] **Step 1: Route input when Cooking tab is active**

In the key-handling path, after the existing `if (active_tab_ == CharTab::Tinkering) { ... }` block, add:

```cpp
if (active_tab_ == CharTab::Cooking) {
    if (!player_has_skill(*player_, SkillId::Cat_Cooking)) return;
    if (cooking_qty_prompt_active_) { handle_qty_prompt_key(key); return; }
    handle_cooking_key(key);
    return;
}
```

- [ ] **Step 2: Implement `handle_cooking_key`**

```cpp
void CharacterScreen::handle_cooking_key(int key) {
    if (key == KEY_TAB) {
        cooking_focus_ = static_cast<CookingFocus>(
            (static_cast<int>(cooking_focus_) + 1) % 3);
        return;
    }
    if (key == 'c') { attempt_cook(); return; }
    if (key == 'x' && cooking_focus_ == CookingFocus::Slots) {
        clear_slot(cooking_slot_cursor_);
        return;
    }

    if (cooking_focus_ == CookingFocus::Slots) {
        if (key == KEY_LEFT && cooking_slot_cursor_ > 0) --cooking_slot_cursor_;
        if (key == KEY_RIGHT && cooking_slot_cursor_ < 2) ++cooking_slot_cursor_;
    } else if (cooking_focus_ == CookingFocus::Ingredients) {
        int count = count_inventory_ingredients(*player_);
        if (count == 0) return;
        if (key == KEY_UP && cooking_ingredient_cursor_ > 0) --cooking_ingredient_cursor_;
        if (key == KEY_DOWN && cooking_ingredient_cursor_ < count - 1) ++cooking_ingredient_cursor_;
        if (key == '\n' || key == '\r') open_qty_prompt_for_ingredient();
    } else if (cooking_focus_ == CookingFocus::Cookbook) {
        handle_cookbook_nav(key);
    }
}
```

- [ ] **Step 3: Implement `open_qty_prompt_for_ingredient`**

```cpp
void CharacterScreen::open_qty_prompt_for_ingredient() {
    // Look up the ingredient the cursor points to in inventory order.
    int i = 0;
    for (const auto& it : player_->inventory.items) {
        if (it.type != ItemType::Ingredient) continue;
        if (i == cooking_ingredient_cursor_) {
            // Already slotted? Edit that slot instead of creating a duplicate.
            for (auto& slot : player_->cooking_slots) {
                if (slot.item_def_id == it.item_def_id) {
                    cooking_qty_prompt_active_ = true;
                    cooking_qty_prompt_item_def_id_ = it.item_def_id;
                    cooking_qty_prompt_value_ = std::max(1, slot.qty);
                    return;
                }
            }
            // Otherwise target an empty slot (prefer selected, else first empty).
            cooking_qty_prompt_active_ = true;
            cooking_qty_prompt_item_def_id_ = it.item_def_id;
            cooking_qty_prompt_value_ = 1;
            return;
        }
        ++i;
    }
}
```

- [ ] **Step 4: Implement `handle_qty_prompt_key`**

```cpp
void CharacterScreen::handle_qty_prompt_key(int key) {
    if (key == KEY_ESC) {
        cooking_qty_prompt_active_ = false;
        return;
    }
    int available = inventory_qty_for_def(*player_, cooking_qty_prompt_item_def_id_);
    if (available <= 0) { cooking_qty_prompt_active_ = false; return; }

    if (key >= '0' && key <= '9') {
        int digit = key - '0';
        int new_val = cooking_qty_prompt_value_ * 10 + digit;
        if (new_val >= 1 && new_val <= available) cooking_qty_prompt_value_ = new_val;
    } else if (key == KEY_BACKSPACE) {
        cooking_qty_prompt_value_ = std::max(1, cooking_qty_prompt_value_ / 10);
    } else if (key == '\n' || key == '\r') {
        commit_qty_prompt();
    }
}

void CharacterScreen::commit_qty_prompt() {
    uint16_t def = cooking_qty_prompt_item_def_id_;
    int qty = cooking_qty_prompt_value_;
    // Edit existing slot for this def?
    for (auto& slot : player_->cooking_slots) {
        if (slot.item_def_id == def) {
            slot.qty = qty;
            cooking_qty_prompt_active_ = false;
            return;
        }
    }
    // Otherwise place in selected slot if empty; else first empty.
    int target = -1;
    if (player_->cooking_slots[cooking_slot_cursor_].item_def_id == 0) {
        target = cooking_slot_cursor_;
    } else {
        for (int i = 0; i < 3; ++i)
            if (player_->cooking_slots[i].item_def_id == 0) { target = i; break; }
    }
    if (target == -1) {
        game_->log("No empty slot.");
    } else {
        player_->cooking_slots[target] = PotSlot{ def, qty };
    }
    cooking_qty_prompt_active_ = false;
}
```

- [ ] **Step 5: Implement `clear_slot`**

```cpp
void CharacterScreen::clear_slot(int idx) {
    if (idx < 0 || idx > 2) return;
    player_->cooking_slots[idx] = PotSlot{};
}
```

No inventory refund is needed — ingredients are only "claimed" into the slot bag for the match check at cook time; they remain in inventory until cook resolution consumes them.

- [ ] **Step 6: Implement `handle_cookbook_nav`**

```cpp
void CharacterScreen::handle_cookbook_nav(int key) {
    // Flattened cursor drives row selection (matches draw_cookbook).
    int total = count_cookbook_rows();
    if (total == 0) return;
    if (key == KEY_UP && cooking_cookbook_cursor_ > 0) --cooking_cookbook_cursor_;
    if (key == KEY_DOWN && cooking_cookbook_cursor_ < total - 1) ++cooking_cookbook_cursor_;
    if (key == KEY_LEFT || key == KEY_RIGHT || key == '\n' || key == '\r') {
        RecipeCategory cat;
        bool is_header = cookbook_row_is_category_header(cooking_cookbook_cursor_, cat);
        if (is_header) toggle_category_expanded(cat);
    }
}
```

`count_cookbook_rows`, `cookbook_row_is_category_header`, `toggle_category_expanded` are small helpers that walk the same ordering as `draw_cookbook` and report structure — keep them local to the cooking section of the file.

- [ ] **Step 7: Implement `attempt_cook`**

```cpp
void CharacterScreen::attempt_cook() {
    if (!has_effect(player_->effects, EffectId::CookingFireAura)) {
        game_->log("You need to be near a fire to cook.");
        return;
    }

    // Verify inventory still has at least the slotted quantities.
    for (const auto& slot : player_->cooking_slots) {
        if (slot.item_def_id == 0) continue;
        if (inventory_qty_for_def(*player_, slot.item_def_id) < slot.qty) {
            game_->log("Not enough ingredients available.");
            return;
        }
    }

    // Any slots filled?
    bool any = false;
    for (const auto& s : player_->cooking_slots) if (s.item_def_id != 0) { any = true; break; }
    if (!any) { game_->log("The pot is empty."); return; }

    const Recipe* hit = match_recipe(player_->cooking_slots);

    // Consume slotted ingredients from inventory.
    for (const auto& slot : player_->cooking_slots) {
        if (slot.item_def_id == 0) continue;
        remove_from_inventory_by_def(*player_, slot.item_def_id, slot.qty);
    }

    if (hit) {
        bool was_known = std::find(player_->known_recipes.begin(),
                                   player_->known_recipes.end(),
                                   hit->id) != player_->known_recipes.end();
        Item dish = build_food_by_def_id(hit->result_item_def_id);
        dish.stack_count = hit->result_qty;
        player_->inventory.items.push_back(std::move(dish));
        if (!was_known) {
            player_->known_recipes.push_back(hit->id);
            game_->log("*You discover a new recipe: " + hit->name + "!*");
        }
        game_->log("You cook " + hit->name + ".");
    } else {
        player_->inventory.items.push_back(build_burnt_slop());
        game_->log("The result is inedible. You produced Burnt Slop.");
    }

    for (auto& s : player_->cooking_slots) s = PotSlot{};
}
```

`remove_from_inventory_by_def` and `build_food_by_def_id` are small helpers. `remove_from_inventory_by_def` walks stacks decrementing `stack_count` and erases empties. `build_food_by_def_id` dispatches on the six cooked-dish def_ids plus Burnt Slop; it's a switch that calls the right `build_*()` function.

Declare them in `item_defs.h`:

```cpp
void remove_from_inventory_by_def(Player& p, uint16_t def_id, int qty);
Item build_food_by_def_id(uint16_t def_id);
```

Implement in `item_defs.cpp`.

- [ ] **Step 8: Build**

```bash
cmake --build build -j
```

Expected: clean build.

- [ ] **Step 9: Smoke check**

Use the dev console to spawn `Raw Meat x3`, `Carrot x2`, `Herbs x2`, `Flour x4`, `Synth-Protein x2`. Place a Campfire and stand on it.

1. Open Cooking tab, Tab to Ingredients, select Raw Meat, Enter, qty 1, Enter. SLOT 1 shows "meat x1".
2. Press `c`. "You cook Cooked Meat." appears. Inventory has a Cooked Meat. Slots cleared. Raw Meat stack down by 1.
3. Slot 2x Raw Meat, 1x Carrot, 1x Herbs. Press `c`. "You cook Hearty Stew." (You already know the recipe because DevCommander starts with all six, so no discovery message.)
4. On a non-DevCommander class, slot 2x Synth-Protein + 1x Flour. First time → "*You discover a new recipe: Protein Bake!*".
5. Slot 2x Flour + 1x Carrot (no recipe). `c` → "The result is inedible. You produced Burnt Slop."
6. Step off the campfire. `c` → "You need to be near a fire to cook." — no ingredients consumed.

- [ ] **Step 10: Commit**

```bash
git add src/character_screen.cpp include/astra/character_screen.h include/astra/item_defs.h src/item_defs.cpp
git commit -m "feat(ui): cooking tab input, qty prompt, cook action resolution"
```

---

## Task 14: Save / load — version bump and new fields

**Files:**
- Modify: `src/save_file.cpp`
- Modify: any save-version constant in headers

- [ ] **Step 1: Bump save version**

Find `constexpr uint32_t SAVE_VERSION = 44;` (or similar) and bump to `45`. The load path must reject anything below 45:

```cpp
if (version < 45) {
    log("Save is from a pre-cooking build and is not supported.");
    return false;
}
```

- [ ] **Step 2: Serialize new player fields**

In the player save/load block:

```cpp
// Save
write_u32(out, static_cast<uint32_t>(player.known_recipes.size()));
for (uint16_t rid : player.known_recipes) write_u16(out, rid);

for (int i = 0; i < 3; ++i) {
    write_u16(out, player.cooking_slots[i].item_def_id);
    write_i32(out, player.cooking_slots[i].qty);
}

// Load
uint32_t n = read_u32(in);
player.known_recipes.resize(n);
for (uint32_t i = 0; i < n; ++i) player.known_recipes[i] = read_u16(in);
for (int i = 0; i < 3; ++i) {
    player.cooking_slots[i].item_def_id = read_u16(in);
    player.cooking_slots[i].qty = read_i32(in);
}
```

- [ ] **Step 3: Serialize new item fields**

In the item save/load block:

```cpp
// Save
bool has_dish = item.dish.has_value();
write_bool(out, has_dish);
if (has_dish) {
    write_i32(out, item.dish->hunger_shift);
    write_i32(out, item.dish->hp_restore);
    write_u32(out, static_cast<uint32_t>(item.dish->granted.size()));
    for (EffectId eid : item.dish->granted) write_u32(out, static_cast<uint32_t>(eid));
}
write_u16(out, item.teaches_recipe_id);

// Load
bool has_dish = read_bool(in);
if (has_dish) {
    DishOutput d;
    d.hunger_shift = read_i32(in);
    d.hp_restore = read_i32(in);
    uint32_t n = read_u32(in);
    d.granted.resize(n);
    for (uint32_t i = 0; i < n; ++i) d.granted[i] = static_cast<EffectId>(read_u32(in));
    item.dish = std::move(d);
}
item.teaches_recipe_id = read_u16(in);
```

Adjust to whatever the actual write_/read_ helpers are called in the codebase.

- [ ] **Step 4: Build**

```bash
cmake --build build -j
```

Expected: clean build.

- [ ] **Step 5: Smoke check**

Start a new game, cook a dish, save. Reload — `known_recipes`, `cooking_slots`, and the Dish item's `dish` field must round-trip. An old (v44) save should be rejected with the message above.

- [ ] **Step 6: Commit**

```bash
git add src/save_file.cpp include/astra
git commit -m "feat(save): bump to v45; serialize cooking state and DishOutput"
```

---

## Task 15: Merchants and loot — stock cookbooks and ingredients

**Files:**
- Modify: `src/item_defs.cpp` (`generate_food_merchant_stock`)
- Modify: `src/shop.cpp` or `src/item_gen.cpp` if additional vendor / loot hooks exist

- [ ] **Step 1: Extend `generate_food_merchant_stock`**

Add the five ingredients and three cookbooks to the stock list. Use the existing faction_rep gating pattern:

```cpp
stock.push_back(stack(build_raw_meat(), 5));
stock.push_back(stack(build_carrot(), 5));
stock.push_back(stack(build_flour(), 5));
stock.push_back(stack(build_herbs(), 3));
stock.push_back(stack(build_synth_protein(), 3));

if (faction_rep >= 0)   stock.push_back(build_cookbook_hearty_stew());
if (faction_rep >= 60)  stock.push_back(build_cookbook_protein_bake());
if (faction_rep >= 300) stock.push_back(build_cookbook_heros_feast());
```

- [ ] **Step 2: Build**

```bash
cmake --build build -j
```

Expected: clean build.

- [ ] **Step 3: Smoke check**

Visit a food merchant on The Heavens Above. Verify ingredients and at least the basic cookbook appear in stock. Buy a cookbook, use it, recipe is learned.

- [ ] **Step 4: Commit**

```bash
git add src/item_defs.cpp
git commit -m "feat(shop): food merchants stock ingredients and cookbooks"
```

---

## Task 16: Docs update

**Files:**
- Modify: `docs/roadmap.md`
- Optionally modify: `docs/formulas.md` (hunger/HP-restore ranges)

- [ ] **Step 1: Tick the cooking feature in the roadmap**

Find the cooking-related row or add one under the appropriate section (e.g., "Survival / Crafting"). Include a short description and check the box.

- [ ] **Step 2: Document cooking formulas (if applicable)**

In `docs/formulas.md`, add a small Cooking section:

```markdown
## Cooking

- Hunger shift: DishOutput::hunger_shift is added to the player's
  HungerState index (Satiated = 0, Starving = 3), clamped to
  [Satiated, Starving]. Negative shifts move toward Satiated.
- HP restore on consume: clamped to `player.effective_max_hp()`.
- AdvancedFireMaking: reduces `camp_making_cooldown_ticks` by 40%.
```

- [ ] **Step 3: Commit**

```bash
git add docs/roadmap.md docs/formulas.md
git commit -m "docs: cooking system roadmap tick and formulas"
```

---

## Self-review checklist (run after all tasks)

- [ ] All 6 recipes from the spec are registered in `recipe_catalog()` with matching item_def_ids.
- [ ] `has_effect(player.effects, EffectId::CookingFireAura)` is the sole gate in `attempt_cook` — no proximity scans added anywhere.
- [ ] `DishOutput` is read only in `use_item`'s Food branch; no duplicate hunger/HP logic remains.
- [ ] `AdvancedFireMaking` prereq on `CampMaking` is enforced at purchase time.
- [ ] Save version is 45; v44 saves are rejected.
- [ ] Cooking tab shows the gate stub without `Cat_Cooking`, full UI with it.
- [ ] Duplicate ingredient across slots is prevented (qty edit on existing slot, not a new slot).
- [ ] `build_ration_pack` uses the `DishOutput` pipeline — no regression on eating rations.
