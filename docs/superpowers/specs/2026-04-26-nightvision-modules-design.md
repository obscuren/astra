# Energy-gated Nightvision Goggles + AI Module / Light Sensor — Design

**Status:** Design approved 2026-04-26. Implementation pending.

## Problem

The current Night Goggles are a passive `+1 view_radius` accessory with no cost and no decision surface. They're a free upgrade — once equipped, the player forgets they exist. We want them to be:

- A *powered* gadget that meaningfully drains energy when actively in use.
- A *smarter* gadget when paired with control modules — the player can choose how much micromanagement they want.
- A canvas for a more general "accessory module" mechanic that future items can plug into.

## Goals

- **Powered behavior** — Nightvision Goggles carry their own `EnergyStore` and consume charge while active in dark contexts.
- **Meaningful view bonus when powered** — `+5` (up from `+1`) so the player feels the difference.
- **Graceful degrade** — at zero charge, goggles still grant `+1` (the lens still works passively) so dead goggles aren't dead weight.
- **Modular control system** — two new tinker modules (AI Module, Light Sensor) that, when slotted, take the manual toggle off the player's plate.
- **Reusable for future items** — AI Module and Light Sensor are general-purpose modules that other items can declare compatibility with later (auto-stim, smart targeting, etc.).
- **Crafting integration** — both modules are findable via the loot table *and* synthesizable through the existing tinkering recipe system.

## Non-goals

- **Replacing the existing Night Goggles item identity.** Same `ITEM_NIGHT_GOGGLES = 24` def_id; only the display name and behavior change. (Save format is bumped per the no-backcompat-pre-ship policy, so this isn't a migration concern.)
- **Designing other modules.** Future expansion lives outside this spec.
- **Auto-stim / auto-grenade / auto-firing.** AI Module exists in the design space but only the goggles are wired to it in this iteration.
- **Module-on-module stacking.** Two modules in two slots don't combine into something new — for the goggles we deliberately ship one slot.
- **Dynamic recharge UI.** Goggles recharge through the existing inventory `space` cell-from-cell transfer flow that already supports any item with `EnergyStore`. No new keybind.

## Design

### Pipeline overview

The new behavior threads through three subsystems:

1. **Item shape** — Nightvision Goggles gain `EnergyStore`, `EnergyConsumer`, one enhancement slot, and a toggle-state field. AI Module and Light Sensor are new items, tagged into the new `Category::AccessoryMod`.
2. **Per-tick drain + auto-toggle** — runs inside the world tick, before view-radius computation. Walks equipped items, decides active state (auto vs manual), drains charge.
3. **View aggregation** — the existing equipment view-radius sum gets the toggleable item's contribution (`+5` if powered+active+dark, `+1` otherwise).

### Item changes

**`Item`** (in `include/astra/item.h`) gains three fields:

```cpp
bool toggleable        = false;   // can the player switch this on/off
bool active            = false;   // current toggle state (per instance, saved)
int  drain_accumulator = 0;       // ticks accrued since last 1-charge drain
```

Defaults preserve current behavior for every existing item. Only `build_nightvision_goggles()` sets `toggleable = true`.

**`EnhancementSlot`** (in `include/astra/item.h`) gains:

```cpp
enum class ModuleKind : uint8_t { None, AiModule, LightSensor };
ModuleKind module_kind = ModuleKind::None;
```

When a player commits an AI Module or Light Sensor into an enhancement slot, the slot's `module_kind` is set on commit. (Bonus data continues to live alongside `stat_bonus`/`energy_bonus`/`solar_panel`; this is one more variant.)

**Helper:** `bool item_has_active_module(const Item& item)` — returns true if any enhancement slot has `committed == true && module_kind != None`. Modules go through the same staged → committed flow as existing energy mods (player slots a module into an empty slot, then commits via the tinkering UI). Auto-mode promotion does not apply until the slot is committed.

### Nightvision Goggles spec

`build_nightvision_goggles()` (renamed from `build_night_goggles()`) constructs:

```
item_def_id: ITEM_NIGHT_GOGGLES        (= 24, unchanged)
identifier:  "nightvision_goggles"     (loot table key — old "night_goggles" replaced)
name:        "Nightvision Goggles"
type:        ItemType::Accessory
slot:        EquipSlot::Face
rarity:      Common
weight:      1
buy_value:   80   sell_value: 25       (unchanged baseline; rarity scaling applies)
modifiers.view_radius: 1               (passive lens — see View aggregation below)
energy:      EnergyStore { current=60, capacity=60 }
consumer:    EnergyConsumer { energy_per_use=1 }
toggleable:  true
active:      false                     (player toggles on; or auto-mode flips it)
enhancement_slots: 1                   (accepts AI Module or Light Sensor)
```

`build_nightvision_goggles()` calls `init_enhancement_slots(item)` per the existing convention so the slot is created and ready for staging.

`active` defaults to `false`. In auto mode the per-tick logic flips it to true on the first dark+charged tick; in manual mode the player must explicitly toggle on once.

The `60` capacity comes from matching the Small Energy Cell. With drain `1 per 10 ticks` while active, that's 600 dark-ticks per charge — roughly 8-9 surface night cycles or one moderate dungeon dive.

### Per-tick drain + auto-toggle

Runs in `Game::advance_world` (after effect tick, before passive regen — same neighborhood as aura emission). Iterates the player's equipment slots once per world tick:

```
for each equipped item:
    if (!item.toggleable) continue;
    if (!item.energy.has_value()) continue;       // misconfiguration guard

    bool dark      = is_dark_context(world);      // see below
    bool auto_mode = item_has_active_module(item);

    if (auto_mode) {
        // Modules override player intent. Item flips itself on/off
        // based on environment + remaining charge.
        item.active = (dark && item.energy->current > 0);
    }
    // else: manual mode; item.active retains the player's setting

    if (!item.active)              continue;
    if (!dark)                     continue;
    if (item.energy->current <= 0) continue;

    item.drain_accumulator += 1;
    if (item.drain_accumulator >= 10) {
        item.drain_accumulator -= 10;
        item.energy->current  -= item.consumer->energy_per_use;  // = 1
    }
```

**`is_dark_context(world)`** is a small helper that returns true whenever the player's view radius would otherwise be clipped — i.e., goggles would actually help. Mirrors the existing FOV restriction rules in `Game::recompute_fov()`:

- Surface detail map, `TimePhase::Night` → true
- Surface detail map, `TimePhase::Dawn` or `Dusk` → true (during the lerp; effective radius < max)
- Surface detail map, `TimePhase::Day` → false
- Dungeon (any non-detail, non-overworld, non-station/ship map) → true (always dark per existing rules)
- Ship interior, Station interior, Derelict Station → false (existing FOV rules already treat these as fully lit)
- Overworld map → false (always lit per existing rules)

Implementation cribs from `Game::recompute_fov` and `effective_view_radius()` in `time_of_day.h` — same predicate, distilled to a boolean.

### View aggregation tweak

Currently `Equipment::total_modifiers().view_radius` sums every equipped item's `view_radius` modifier. For toggleable items, the sum needs to know whether they're powered + active.

The cleanest change is in `total_modifiers()` (or wherever view aggregation happens — `effective_view_radius()` on Player likely): for each item, contribute either:

- `item.modifiers.view_radius`  (the base `+1` lens) when item is **not** toggleable, OR is toggleable but not powered+active.
- `item.modifiers.view_radius + 4`  when item is toggleable + powered + active in a dark context. (`+1` base + `+4` boost = `+5` total.)

This keeps the *baseline* contribution as the existing `view_radius` field and treats the powered boost as a `+4` bonus computed at aggregation time. New items that want similar behavior set `toggleable=true` and the same logic applies.

(Alternative considered: encode `+5` directly and have powered-off goggles contribute `+1` via a separate field. Rejected as more code for the same effect.)

### AI Module + Light Sensor — new items

Both live in the standard `1-999` ID range as discrete items.

**ITEM_AI_MODULE = 94**
- `identifier`: `ai_module`
- `name`: "AI Module"
- `description`: "Adaptive control circuit. Slotted into an item to automate any manual trigger benefits."
- `type`: `ItemType::CraftingMaterial` (existing type — modules tag into this; the loot Category provides the finer bucket)
- `Category::AccessoryMod`
- `Rarity::Rare`, `Theme::Tech`
- `min_level`: 3
- `default_weight`: 5
- `source_mask`: `Chest | MerchantArms | BlackMarket`
- `buy_value`: 280, `sell_value`: 95

**ITEM_LIGHT_SENSOR = 95**
- `identifier`: `light_sensor`
- `name`: "Light Sensor"
- `description`: "Photodiode array. Slotted into an item to auto-toggle anything that depends on ambient light."
- `type`: `ItemType::CraftingMaterial`
- `Category::AccessoryMod`
- `Rarity::Uncommon`, `Theme::Tech`
- `min_level`: 1
- `default_weight`: 12
- `source_mask`: `Chest | MerchantGeneral | MerchantArms`
- `buy_value`: 120, `sell_value`: 40

Both get `build_*()` functions in `item_defs.cpp` and dispatch arms in `build_by_def_id`.

### Module distinction (mechanical roles)

Both modules are interchangeable for the Nightvision Goggles — either one promotes the goggles to auto-mode. They're mechanically distinct for *future* items:

- **AI Module** = generic auto-trigger. Promotes any manual-toggle behavior to automatic. Future items: auto-stim on low HP, auto-grenade on adjacent threat, etc.
- **Light Sensor** = specifically light-conditional. Promotes light-dependent behaviors to automatic. Future items: auto-engage solar panel when sunlight, auto-disengage flashlight when bright, etc.

For Nightvision Goggles (which are both manually toggleable AND light-conditional), both apply. The runtime check is simply "any committed slot has `module_kind != None`" — it doesn't currently differentiate. When future items have *both* manual and light-conditional behaviors but with different runtime semantics, the discriminator can be added then.

### `Category::AccessoryMod`

New `Category` enum value in `loot_source.h`. Updates:

- `category_name(Category::AccessoryMod)` → `"accessory mod"`.
- Per-source `s_category_weights` (in `loot_table.cpp`):
  - `Chest`: add `{ AccessoryMod, 5 }` (folds into the existing distribution).
  - `MerchantArms` and `MerchantGeneral` weights table is empty (merchants are manifest-driven), so nothing to update there.
- Manifest entries:
  - `s_general_merchant_manifest` (in `merchant.cpp`): add Light Sensor as `Always` baseline qty 1; AI Module as `Always` qty 1 gated `min_reputation: 50`.
  - `s_arms_dealer_manifest` (in `hub_npcs.cpp`): add Light Sensor as `Always` qty 1 baseline; AI Module as `Always` qty 1 baseline (arms dealer caters to advanced gear); promote both to qty 2 at `min_reputation: 50`.
  - `s_black_market_manifest` (in `black_market_vendor.cpp`): add AI Module as `Random` qty 1 with `Category::AccessoryMod` filter.

### Tinkering recipes

Blueprints in `tinkering.cpp` are abstract sub-component names (`Optic Module`, `Power Conduit`, `Padding Weave`, etc.) learned by analyzing items of the right type — not item-name strings. `synthesis_recipes()` gets two new entries using the existing `custom_builder` field (so the synthesis pipeline calls the regular builder rather than synthesizing equipment fields):

```cpp
{"Optic Module", "Joint Mechanism", "AI Module",
 "Adaptive control circuit. Slotted into an item to automate any manual trigger.",
 ItemType::CraftingMaterial, EquipSlot::Face, '*',
 {}, 0, {/*nano*/1, /*power*/1, /*circuit*/2, /*alloy*/0},
 &build_ai_module},

{"Optic Module", "Padding Weave", "Light Sensor",
 "Photodiode array. Slotted into an item to auto-toggle anything that depends on ambient light.",
 ItemType::CraftingMaterial, EquipSlot::Face, '*',
 {}, 0, {/*nano*/1, /*power*/0, /*circuit*/1, /*alloy*/0},
 &build_light_sensor},
```

Both blueprint pairs are unused by existing recipes (verified against the current `synthesis_recipes()` table). Players obtain `Optic Module` blueprints by analyzing any Accessory; `Joint Mechanism` from any Armor; `Padding Weave` from any Armor.

(Equipment-only fields — `EquipSlot`, glyph, modifiers, durability — are unused when `custom_builder` is set; the existing energy-mod recipes already follow this pattern.)

### Manual toggle UI

When a player presses `space` on a `toggleable` item in the inventory:

- If the item is in **manual mode** (no committed module slot), the existing inventory item-interact menu gets a new action "Toggle on/off" that flips `item.active`. A short status line confirms.
- If the item is in **auto mode** (has a committed module), the toggle action is replaced with a read-only "Auto (AI Module / Light Sensor)" label — clicking it does nothing, just informs the player.

This piggybacks on the existing inventory interact menu — no new keybinds.

### Save format

Per `feedback_no_backcompat_pre_ship.md`, bump the save schema version (find the version constant in `save_file.cpp` / `save_system.cpp` at impl time). New serialized fields:

- `Item::toggleable` (bool)
- `Item::active` (bool)
- `Item::drain_accumulator` (int)
- `EnhancementSlot::module_kind` (uint8_t enum)

Old saves load → version mismatch → reject. Player starts fresh; new Nightvision Goggles spawn with the new fields populated.

### What stays untouched

- `time_of_day.h` API — `is_dark_context` is a thin wrapper that consults `TimePhase` and the world location type. Doesn't modify existing functions.
- `EnergyStore`, `EnergyConsumer`, `EnergyModifiers` — unchanged.
- Existing energy mods (Capacitor Coil, Solar Panel, etc.) — unaffected; their slot bonuses ignore `module_kind`.
- Recharge flow — Nightvision Goggles work with the existing inventory `space` cell-from-cell transfer that supports any item with `EnergyStore`.
- `apply_rarity_affixes` — modules don't roll affixes; same as energy mods.
- `verify_dispatch_coverage()` — automatically catches the two new items as long as they're in the loot table and `build_by_def_id` dispatches them.

### Open questions deferred to implementation time

- **`is_dark_context` signal source.** The function needs access to current `TimePhase` and current location type. Pick the right accessor at impl time (probably `world.time_of_day().phase()` plus `world.current_location_type()` or similar).
- **Equipment-slot iteration** for the per-tick drain — a generic walker over the 12 equipment slots vs. a hardcoded "check the Face slot." Pick whichever fits the existing pattern best.
- **Recharge action's effect on `active`** — when the player drains a cell into the goggles bringing them from 0 → 60, does `active` automatically flip back on? Probably yes if in auto mode; manual mode stays whatever the player set. Pin down at impl time.

## Risks

- **`is_dark_context` definition drift** — if surface lighting rules change, this predicate needs to track them. Mitigate by keeping it as a thin wrapper around `effective_view_radius`-style logic, not a duplicate.
- **Per-tick cost of equipment walk** — touching 12 equipment slots every world tick is fine at a per-key-press cadence; flag if profiling later shows it's hot.
- **Module behavior for non-goggle items** — AI Module / Light Sensor have semantics that future items will need to declare compatibility with. Until any other item is `toggleable`, the modules' "promotion" behavior is meaningless on those items. Players could slot a module into a future tinker-slot accessory and see nothing happen — call out in tooltip text or item descriptions when that case arises.
- **Existing `night_goggles` identifier in the loot table.** Renaming to `nightvision_goggles` breaks any dev-console muscle memory and any hypothetical save references. Per the no-backcompat policy + the loot table being so new, acceptable.

## Implementation scope

Roughly 4 commits:

1. **Schema additions** — `Category::AccessoryMod`, `category_name` update, `EnhancementSlot::module_kind` + `ModuleKind` enum, `Item::toggleable`/`active`/`drain_accumulator` fields, save version bump.
2. **New items** — `ITEM_AI_MODULE` + `ITEM_LIGHT_SENSOR` constants, `build_ai_module()` + `build_light_sensor()` builders, `build_by_def_id` dispatch arms, loot-table entries (2), merchant manifest entries (3 manifests touched).
3. **Nightvision Goggles upgrade** — rename `build_night_goggles` → `build_nightvision_goggles` (function + display name + identifier), wire EnergyStore/Consumer/slot/toggleable, view-aggregation tweak, `is_dark_context` helper, per-tick drain + auto-toggle in `Game::advance_world`. Loot-table identifier update.
4. **UX + crafting** — manual toggle action in inventory `space` interact menu (with auto-mode read-only fallback), tinkering recipes for both modules.

Each commit leaves the build green.
