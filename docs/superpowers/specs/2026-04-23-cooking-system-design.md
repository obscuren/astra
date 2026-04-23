# Cooking System — Design Spec

**Date:** 2026-04-23
**Status:** Draft
**Builds on:** `2026-04-23-aura-system-design.md`

## Overview

A cooking system that lets the player combine ingredients over a cooking
source to produce food dishes. Cooking is gated on proximity to a
fixture tagged `FixtureTag::CookingSource`, using the aura system as a
pure capability query — no proximity scans at cook-time call sites.

**Scope for v1:** known/unknown recipe gating (Option B from brainstorming)
with a full character-screen tab as the UI. A Cooking skill category is
included with a single sub-skill. Raw-vs-cooked edibility, failure
states driven by a cooking check, NPC teachers, and cooking-skill-driven
recipe quality are deferred to later iterations.

## Goals

- Cooking is gated on "are you near a fire?" via a `EffectId::CookingFireAura`
  effect emitted by any `FixtureTag::CookingSource` fixture.
- Recipes are a first-class data type, separate from items.
- Ingredients are a first-class item type (`ItemType::Ingredient`).
- Food has an explicit `DishOutput` (hunger, HP, optional GEs) attached
  to the item definition so eating is the same regardless of source.
- Recipes are learned (starting set + cookbooks + experimentation).
- UI mirrors the Tinkering tab's conventions: three slots, a materials
  list, and a collapsible catalog on the right.
- A single `[c] cook` action resolves known/unknown/miss cases.
- A Cooking skill category gates the full tab; without it, the tab
  shows a stub like Tinkering does.

## Non-goals (v1)

- No cooking-skill-driven success checks (no burnt food from low skill).
- No raw-food edibility (ingredients cannot be eaten directly).
- No NPC dialog teachers for recipes.
- No food spoilage, no ticks-to-cook timing.
- No multi-output recipes (one recipe → one dish def).
- No migration of existing saves — save version bumps, old saves
  rejected (project no-backcompat policy).

## Architecture

Four layers, each mapping to existing Astra conventions:

### 1. Capability layer — `EffectId::CookingFireAura`

- New `EffectId::CookingFireAura` in `include/astra/effect.h`.
- Factory `make_cooking_fire_aura_ge()` in `src/effect.cpp`, using the
  `_ge` naming convention. Zero stat modifiers — it's a capability
  token only.
- Registered in `src/aura.cpp` inside `tag_auras()`:
  `FixtureTag::CookingSource → Aura { effect = CookingFireAura,
  radius = 2, target_mask = TargetMask::Player }`.
- Consumers query `has_effect(player.effects, EffectId::CookingFireAura)`.
  No proximity scan at the call site.

Cooking-source-tagged fixtures already exist (Campfire, CampStove,
Kitchen in `src/tilemap.cpp`). Adding the registry entry is the only
plumbing needed to make them emit the aura.

### 2. Data layer — recipes, ingredients, dishes, cookbooks

**New file `include/astra/recipe.h`:**

```cpp
enum class RecipeCategory : uint8_t {
    Basic,      // starting repertoire
    Hearty,     // mid-tier, from common cookbooks
    Gourmet,    // rare cookbooks / future NPC teachers
    Survival,   // field cooking, future tier
};

struct RecipeIngredient {
    uint16_t item_def_id;
    int qty;
};

struct Recipe {
    uint16_t id;
    std::string name;
    std::string description;
    RecipeCategory category;
    std::vector<RecipeIngredient> ingredients;  // size 1..3
    uint16_t result_item_def_id;                // points at a Food def
    int result_qty = 1;
};

const std::vector<Recipe>& recipe_catalog();
const Recipe* find_recipe(uint16_t id);

struct PotSlot {
    uint16_t item_def_id = 0;  // 0 = empty
    int qty = 0;
};

// Order-independent, quantity-exact match against the catalog.
const Recipe* match_recipe(const std::array<PotSlot, 3>& slots);
```

`match_recipe` canonicalises the slot bag (drop empties, sort by
`item_def_id`, sum duplicates if the UI ever permits them) and scans
the catalog for an exact match. Returns `nullptr` if no recipe matches.

**Registry in `src/recipe_defs.cpp`** — static `recipe_catalog()` built
on first call.

**New `ItemType::Ingredient` enum value** in `include/astra/item.h`.
Ingredient item defs (e.g. `build_raw_meat`, `build_carrot`,
`build_flour`, `build_herbs`, `build_synth_protein`) added to
`src/item_defs.cpp`. Ingredients are stackable.

**New `ItemType::Cookbook` enum value.** A new field on `Item`:
`uint16_t teaches_recipe_id = 0;`. Using a cookbook (`usable = true`)
adds the recipe to `player_.known_recipes` and consumes the book.
If already known, does not consume, logs "You already know this recipe."

**`DishOutput` struct** (new, `include/astra/item.h`):

```cpp
struct DishOutput {
    int hunger_shift = 0;           // negative moves toward Satiated
    int hp_restore = 0;             // instant heal on consume, clamped
    std::vector<EffectId> granted;  // GEs via add_effect
};

struct Item {
    // ...
    std::optional<DishOutput> dish;
};
```

`DishOutput` lives on the Item definition (not on the Recipe) so eating
is symmetric across cooked dishes, looted rations, and shop-bought
food. The existing `build_ration_pack` gets a populated `dish` so it
flows through the same consumption path.

### 3. Skill layer — `Cat_Cooking`

Extend `include/astra/skill_defs.h`:

```cpp
Cat_Cooking         = 11,
AdvancedFireMaking  = 1100,
```

Registered in `src/skill_defs.cpp` with a new `SkillCategory`:
`{ Cat_Cooking, "Cooking", ..., skills = { AdvancedFireMaking } }`.

**`AdvancedFireMaking`:**
- Passive skill.
- Prerequisite: `CampMaking` (which lives under `Cat_Wayfinding`).
  Cross-category prerequisite — enforced at skill-purchase time in
  the skill tree UI. The skill system has no built-in `prereq` field
  today; enforcement is added in the skill-screen purchase path and
  validated on load.
- Effect: reduces `world::camp_making_cooldown_ticks` by 40% when
  applied to the `CampMakingAbility` cooldown calculation. Exact
  percentage is a tuning knob; default ships at 40%.

**Tab gating:** if `!player_has_skill(*player_, SkillId::Cat_Cooking)`,
`draw_cooking` renders a stub ("Learn Cooking to use the kitchen.")
exactly as `draw_tinkering` does for `Cat_Tinkering`.

### 4. UI layer — `CharTab::Cooking`

Extend `CharTab` in `include/astra/character_screen.h`:

```cpp
enum class CharTab : uint8_t {
    // ...existing...
    Cooking,
};
```

New rendering function `draw_cooking(UIContext& ctx)` in
`src/character_screen.cpp`. Layout, left half:

```
COOKING POT
  ┌─────────┐ ┌─────────┐ ┌─────────┐
  │ SLOT 1  │ │ SLOT 2  │ │ SLOT 3  │
  │ meat x2 │ │carrot x1│ │  empty  │
  └─────────┘ └─────────┘ └─────────┘

  ● Near Campfire     (or dim: "No cooking source nearby")

INGREDIENTS
  ▸ raw meat            x5
    carrot              x3
    flour               x4
    herbs               x2
    synth-protein       x1
```

Right half (mirrors blueprint catalog):

```
COOKBOOK
▾ Basic
   ▸ Cooked Meat
      + 1x Raw Meat
      Cooks raw meat over the fire. Restores a little hunger and +3 HP.
     Warm Broth
     Campfire Bread
▾ Hearty
     Hearty Stew
     Protein Bake
▸ Gourmet
```

**`CookingFocus` enum** drives pane focus. Tab cycles
`Slots → Ingredients → Cookbook → Slots`. Shift-Tab reverses. Arrows
navigate within the focused pane. Matches tinkering's focus model.

**Quantity prompt.** When Enter is pressed on an ingredient, an inline
prompt appears at the bottom of the left pane:

```
How many carrot? [ 2 ]   (1–3)   Enter=confirm  Esc=cancel
```

Clamped to `[1, available_qty]`. Digits edit, Enter commits into the
currently-selected slot (or the first empty slot if none selected).
Esc cancels. While active, all input routes to the prompt.

**Duplicate ingredients across slots are not allowed.** If the player
tries to add an ingredient whose `item_def_id` is already in another
slot, the prompt opens on that existing slot instead — editing its qty
rather than creating a duplicate. This keeps the slot bag canonical and
simplifies `match_recipe`.

**Cook action (`c` from any focus).**
1. If `has_effect(player.effects, EffectId::CookingFireAura)` is false:
   log `"You need to be near a fire to cook."` — do nothing.
2. Else resolve: see §4.

**Persisted UI state on `Player`:**
```cpp
std::vector<uint16_t> known_recipes;
std::array<PotSlot, 3> cooking_slots;
```
Slots persist across tab switches and game sessions so partially-built
meals survive save/load. Cleared on successful cook.

## Cook action resolution

Pseudocode below illustrates intent. Helpers like `build_item_from_def`,
`shift_hunger`, and `consume_one_from_stack` do not exist verbatim
today; the implementation plan will either introduce them as small
named helpers or inline the equivalent logic.

```cpp
void cook_from_slots(Player& player, MessageLog& log) {
    const Recipe* hit = match_recipe(player.cooking_slots);
    consume_slotted_ingredients(player);  // removes qty from inventory
    if (hit) {
        bool was_known = contains(player.known_recipes, hit->id);
        Item dish = build_item_from_def(hit->result_item_def_id);
        dish.stack_count = hit->result_qty;
        player.inventory.items.push_back(std::move(dish));
        if (!was_known) {
            player.known_recipes.push_back(hit->id);
            log.push("*You discover a new recipe: " + hit->name + "!*");
        }
        log.push("You cook " + hit->name + ".");
    } else {
        player.inventory.items.push_back(build_burnt_slop());
        log.push("The result is inedible. You produced Burnt Slop.");
    }
    clear_cooking_slots(player);
}
```

Three outcome paths: known recipe (produce dish), unknown recipe
(produce dish + learn), miss (Burnt Slop). No RNG. Ingredients are
always consumed.

## Eating

`use_item` gains a branch for `ItemType::Food` with `item.dish` set:

```cpp
if (item.type == ItemType::Food && item.dish) {
    shift_hunger(player, item.dish->hunger_shift);
    player.hp = std::min(player.effective_max_hp(),
                         player.hp + item.dish->hp_restore);
    for (EffectId eid : item.dish->granted) add_effect(player.effects, eid);
    log.push("You eat " + item.name + ".");
    consume_one_from_stack(item);
}
```

`ItemType::Stim` paths are unchanged. `build_ration_pack` populates its
`dish` field so it uses this same branch.

## Content (v1)

### Ingredients (5 new item defs)

| Name          | Stackable | Notes                         |
|---------------|-----------|-------------------------------|
| Raw Meat      | yes       | Dropped by fauna, sold by butchers |
| Carrot        | yes       | Farm produce                  |
| Flour         | yes       | Grain processed               |
| Herbs         | yes       | Foraged                       |
| Synth-Protein | yes       | Station vendor stock          |

### Recipes

**Starting (all players know these):**

| ID | Name           | Category | Ingredients                 | Produces       | Hunger | HP  | GE                  |
|----|----------------|----------|-----------------------------|----------------|-------:|----:|---------------------|
| 1  | Cooked Meat    | Basic    | 1× Raw Meat                 | Cooked Meat    | −1     | +3  | —                   |
| 2  | Warm Broth     | Basic    | 1× Carrot, 1× Herbs         | Bowl of Broth  | −1     | +2  | WarmMeal (100t)     |
| 3  | Campfire Bread | Basic    | 2× Flour                    | Flatbread      | −2     | +1  | —                   |

**Cookbook-obtained:**

| ID | Name         | Category | Ingredients                                   | Produces      | Hunger | HP  | GE                                |
|----|--------------|----------|-----------------------------------------------|---------------|-------:|----:|-----------------------------------|
| 10 | Hearty Stew  | Hearty   | 2× Raw Meat, 1× Carrot, 1× Herbs              | Hearty Stew   | −2     | +5  | WellFed (300t, +1 regen)          |
| 11 | Protein Bake | Hearty   | 2× Synth-Protein, 1× Flour                    | Protein Bake  | −2     | +4  | —                                 |
| 12 | Hero's Feast | Gourmet  | 1× Raw Meat, 1× Herbs, 1× Synth-Protein       | Hero's Feast  | −3     | +10 | Hearty (500t, +1 AV, +5 max HP)   |

Three cookbook item defs: "Cookbook: Hearty Stew", "Cookbook: Protein
Bake", "Cookbook: Hero's Feast". Added to
`generate_food_merchant_stock` with progressive faction-rep gating.

**Failure:**

- **Burnt Slop** — produced whenever the slotted bag matches no recipe.
  Food def with `DishOutput { hunger_shift: −1, hp_restore: 0 }`.

### New GEs

- `CookingFireAura` — capability marker, no modifiers.
- `WarmMeal` — +1 HP regen for 100 ticks.
- `WellFed` — +1 HP regen for 300 ticks.
- `Hearty` — +1 AV, +5 max HP for 500 ticks.

Each gets an `_ge` factory in `src/effect.cpp`.

## Save/load

- Bump save version v44 → v45.
- Serialize: `Player::known_recipes` (vector u16),
  `Player::cooking_slots` (3× PotSlot),
  `Item::dish` (optional DishOutput),
  `Item::teaches_recipe_id` (u16).
- Old saves rejected per project no-backcompat policy.

## File impact

**New files:**
- `include/astra/recipe.h`
- `src/recipe_defs.cpp`

**Modified files:**
- `include/astra/effect.h` — `EffectId::CookingFireAura`, `WarmMeal`,
  `WellFed`, `Hearty`.
- `src/effect.cpp` — four new `_ge` factories.
- `src/aura.cpp` — register `CookingSource → CookingFireAura` in
  `tag_auras()`.
- `include/astra/item.h` — `ItemType::Ingredient`, `ItemType::Cookbook`,
  `DishOutput`, `Item::dish`, `Item::teaches_recipe_id`.
- `include/astra/item_defs.h` + `src/item_defs.cpp` — 5 ingredients,
  6 cooked dishes, 1 Burnt Slop, 3 cookbooks.
- `include/astra/player.h` — `known_recipes`, `cooking_slots`.
- `include/astra/skill_defs.h` + `src/skill_defs.cpp` — `Cat_Cooking`,
  `AdvancedFireMaking`, registration, prereq check.
- `src/ability.cpp` — `CampMakingAbility::cooldown_ticks` reads a
  reduced value when player has `AdvancedFireMaking`.
- `include/astra/character_screen.h` — `CharTab::Cooking`,
  `CookingFocus`, input/focus state fields.
- `src/character_screen.cpp` — `draw_cooking`, input routing.
- `src/game_interaction.cpp` — extend `use_item` for `ItemType::Food`
  with `dish`, and `ItemType::Cookbook`.
- Save/load code — new fields + version bump.
- `docs/roadmap.md` — tick box.
- `docs/formulas.md` — hunger-shift + HP-restore caps if formulas need
  documenting.

## Deferred to later

- `C` from brainstorming: raw-vs-cooked edibility, cooking-skill
  success checks, failure states beyond Burnt Slop.
- NPC dialog teachers for recipes.
- Recipe hints on experiment misses.
- Food spoilage, ticks-to-cook timing, multi-output recipes.
- Additional Cooking sub-skills past `AdvancedFireMaking`.
- Cookbook-as-readable-text UI (v1 consumes on use; later versions
  might add a library).
