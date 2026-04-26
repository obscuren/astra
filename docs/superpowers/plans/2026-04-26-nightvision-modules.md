# Nightvision Goggles + AI Module / Light Sensor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Convert Night Goggles into a powered, optionally-automated gadget driven by two new tinker modules (AI Module + Light Sensor).

**Architecture:** Existing Night Goggles item (id=24) gets `EnergyStore` + 1 enhancement slot, renamed display + identifier to "Nightvision Goggles." Two new items (`Category::AccessoryMod`) plug into the slot to promote manual-toggle behavior to auto. Per-tick drain runs in `Game::advance_world`, mirrors the FOV restriction predicate from `recompute_fov` to detect "dark contexts."

**Tech Stack:** C++20, CMake, single-binary build. No formal test framework — verification is build success + dev-mode runtime smoke tests.

**Spec:** `docs/superpowers/specs/2026-04-26-nightvision-modules-design.md`

**Build / verify commands (use throughout):**
- One-time setup (already done): `cmake -B build -DDEV=ON`
- Rebuild: `cmake --build build` (must succeed after every step that touches code)
- Run: `./build/astra-dev`
- Open dev console in-game: backtick (`` ` ``)

**File structure (4 commits):**
```
include/astra/item.h           # +Item::toggleable/active/drain_accumulator
                               # +EnhancementSlot::module_kind + ModuleKind enum
include/astra/loot_source.h    # +Category::AccessoryMod
include/astra/save_file.h      # SAVE_FILE_VERSION 47 → 48
include/astra/item_ids.h       # +ITEM_AI_MODULE (94), +ITEM_LIGHT_SENSOR (95)
src/save_file.cpp              # serialize new fields under v48 (positional, append at end)
src/item.cpp                   # extend total_modifiers() for toggleable powered boost
src/loot_source.h              # +"accessory mod" in category_name()
src/item_defs.cpp              # +build_ai_module(), +build_light_sensor(),
                               #  rename build_night_goggles → build_nightvision_goggles,
                               #  +build_by_def_id dispatch arms (2)
src/loot_table.cpp             # +2 LootEntry rows, +AccessoryMod weight on Chest,
                               #  rename "night_goggles" identifier → "nightvision_goggles"
src/npcs/merchant.cpp          # +Light Sensor + AI Module manifest entries
src/npcs/hub_npcs.cpp          # +arms-dealer manifest entries (Light Sensor + AI Module)
src/npcs/black_market_vendor.cpp # +Random AccessoryMod manifest entry
src/game_world.cpp             # +is_dark_context() helper, +per-tick drain in advance_world
src/character_screen.cpp       # extend 'g' to toggle item.active when toggleable+manual
src/tinkering.cpp              # +2 synthesis recipes (AI Module, Light Sensor)
```

---

## Task 1: Schema additions — types, enums, save bump

**Files:**
- Modify: `include/astra/item.h` (Item + EnhancementSlot fields)
- Modify: `include/astra/loot_source.h` (Category enum + category_name)
- Modify: `include/astra/save_file.h` (version bump)
- Modify: `src/save_file.cpp` (serialize new fields)

This task is purely additive — adds storage and serialization for the new fields. Build green at end. No call sites use the new fields yet.

- [ ] **Step 1.1: Add `ModuleKind` enum + `EnhancementSlot::module_kind` field**

In `include/astra/item.h`, add the `ModuleKind` enum just above the `EnhancementSlot` struct (around line 142):

```cpp
// Behavioral module kind for accessory mod slots. Promotes the host item
// from manual toggle to automatic when committed.
enum class ModuleKind : uint8_t {
    None,
    AiModule,      // generic auto-trigger for any manual benefit
    LightSensor,   // light-conditional auto-toggle
};
```

Then add `module_kind` to `EnhancementSlot`. Locate the existing struct (lines 143-151) and add a single field after `solar_panel`:

```cpp
struct EnhancementSlot {
    bool filled = false;
    bool committed = false;
    uint32_t material_id = 0;
    std::string material_name;
    StatModifiers stat_bonus;
    EnergyModifiers energy_bonus;
    std::optional<SolarPanelData> solar_panel;
    ModuleKind module_kind = ModuleKind::None;   // NEW
};
```

- [ ] **Step 1.2: Add `Item::toggleable`, `active`, `drain_accumulator` fields**

In `include/astra/item.h`, locate the `Item` struct (line 165). Add three fields immediately after the existing `enhancements` vector (around line 198). Group them so the diff is contiguous:

```cpp
    int enhancement_slots = 0;
    std::vector<EnhancementSlot> enhancements;

    // Powered toggleable items (e.g., Nightvision Goggles).
    // toggleable: this item can be switched on/off (manual or auto).
    // active:     current toggle state (per-instance, saved).
    // drain_accumulator: ticks accrued since last 1-charge drain.
    bool toggleable        = false;
    bool active            = false;
    int  drain_accumulator = 0;
```

- [ ] **Step 1.3: Add `Category::AccessoryMod` to `loot_source.h`**

In `include/astra/loot_source.h`, locate the `Category` enum (around line 100). Append `AccessoryMod` as the last value before `QuestItem`:

```cpp
enum class Category : uint8_t {
    Weapon,
    Armor,
    Shield,
    Accessory,
    Consumable,
    Battery,
    Junk,
    CraftingMaterial,
    ShipComponent,
    Ingredient,
    Cookbook,
    EnergyMod,
    AccessoryMod,    // NEW
    QuestItem,
};
```

Then update `category_name()` (around line 126) to handle the new value:

```cpp
constexpr std::string_view category_name(Category c) {
    switch (c) {
        case Category::Weapon:           return "weapon";
        case Category::Armor:            return "armor";
        case Category::Shield:           return "shield";
        case Category::Accessory:        return "accessory";
        case Category::Consumable:       return "consumable";
        case Category::Battery:          return "battery";
        case Category::Junk:             return "junk";
        case Category::CraftingMaterial: return "crafting";
        case Category::ShipComponent:    return "ship";
        case Category::Ingredient:       return "ingredient";
        case Category::Cookbook:         return "cookbook";
        case Category::EnergyMod:        return "energy mod";
        case Category::AccessoryMod:     return "accessory mod";   // NEW
        case Category::QuestItem:        return "quest";
    }
    return "?";
}
```

- [ ] **Step 1.4: Bump save schema version 47 → 48**

In `include/astra/save_file.h`, find the line:

```cpp
inline constexpr uint32_t SAVE_FILE_VERSION = 47;   // v47: cell procs (Item::proc)
```

Change to:

```cpp
inline constexpr uint32_t SAVE_FILE_VERSION = 48;   // v48: toggleable items + module_kind
```

- [ ] **Step 1.5: Serialize new Item fields in `src/save_file.cpp`**

Locate the item-write block — search for `// v47: cell proc` (around line 321). After the cell-proc serialization completes (around line 329, just before the line `// Enhancement slots`), insert v48 fields:

```cpp
    // v48: toggleable items
    w.write_u8(item.toggleable ? 1 : 0);
    w.write_u8(item.active ? 1 : 0);
    w.write_i32(item.drain_accumulator);
```

Then locate the matching read block. Search for `// v47: cell proc` in the read path (likely around line 395-415; the read mirrors the write structurally). After the cell-proc read completes, before the enhancement slot read:

```cpp
    // v48: toggleable items
    item.toggleable = r.read_u8() != 0;
    item.active = r.read_u8() != 0;
    item.drain_accumulator = r.read_i32();
```

- [ ] **Step 1.6: Serialize `EnhancementSlot::module_kind` in the slot loop**

Still in `src/save_file.cpp`. Locate the slot write loop — search for `for (const auto& enh : item.enhancements)` in the writer (around line 333). At the end of each iteration (after the existing solar_panel block, around line 353), append the v48 module_kind byte:

```cpp
        // v48: module_kind
        w.write_u8(static_cast<uint8_t>(enh.module_kind));
    }
```

Then in the matching read loop (search for the enhancement_slots read, around line 438-460), at the end of each iteration after the solar_panel read:

```cpp
        // v48: module_kind
        item.enhancements[i].module_kind = static_cast<ModuleKind>(r.read_u8());
    }
```

If `ModuleKind` isn't visible in `save_file.cpp`'s includes, add `#include "astra/item.h"` (it's likely already included transitively — check first).

- [ ] **Step 1.7: Build to verify schema additions compile**

Run: `cmake --build build`
Expected: clean build. The new fields default-initialize, so existing code remains valid.

If linker complains about any missing symbol, double-check the enum spelling and field names match exactly between header and serialization.

- [ ] **Step 1.8: Commit**

```bash
git add include/astra/item.h include/astra/loot_source.h \
        include/astra/save_file.h src/save_file.cpp
git commit -m "$(cat <<'EOF'
feat(gadgets): schema for toggleable powered items + module slots

Adds new fields:
- Item::toggleable, active, drain_accumulator
- EnhancementSlot::module_kind (ModuleKind enum: None / AiModule / LightSensor)
- Category::AccessoryMod (with "accessory mod" name)

Bumps SAVE_FILE_VERSION 47 -> 48. Old saves rejected per
no-backcompat-pre-ship policy. New fields serialized at the
appropriate points in the v48 layout.

Pure scaffolding — no callers use the new fields yet.

Spec: docs/superpowers/specs/2026-04-26-nightvision-modules-design.md
EOF
)"
```

---

## Task 2: AI Module + Light Sensor — items, dispatch, loot integration

**Files:**
- Modify: `include/astra/item_ids.h` (new constants)
- Modify: `src/item_defs.cpp` (builders + dispatch)
- Modify: `src/loot_table.cpp` (loot entries + category weight)
- Modify: `src/npcs/merchant.cpp` (general manifest)
- Modify: `src/npcs/hub_npcs.cpp` (arms-dealer manifest)
- Modify: `src/npcs/black_market_vendor.cpp` (black market manifest)

After this task: both modules exist as items, drop in the loot pool, appear in merchant inventories, and survive the dispatch-coverage check.

- [ ] **Step 2.1: Add `ITEM_AI_MODULE` and `ITEM_LIGHT_SENSOR` constants**

In `include/astra/item_ids.h`, find the comment block "Minor energy mods for cell customization (89-93)" (around line 119). Add a new section after it:

```cpp
// Accessory modules (94-95)
constexpr uint16_t ITEM_AI_MODULE              = 94;
constexpr uint16_t ITEM_LIGHT_SENSOR           = 95;
```

- [ ] **Step 2.2: Add `build_ai_module()` and `build_light_sensor()` to `src/item_defs.cpp`**

Find a sensible location — alongside the other tinker materials (after `build_tuned_catalyst` at ~line 1018, before the ship components section). Insert:

```cpp
Item build_ai_module() {
    Item it;
    it.item_def_id = ITEM_AI_MODULE;
    it.id = 2070;
    it.name = "AI Module";
    it.description = "Adaptive control circuit. Slotted into an item to automate any manual trigger.";
    it.type = ItemType::CraftingMaterial;
    it.rarity = Rarity::Rare;
    it.weight = 1;
    it.buy_value = 280;
    it.sell_value = 95;
    return it;
}

Item build_light_sensor() {
    Item it;
    it.item_def_id = ITEM_LIGHT_SENSOR;
    it.id = 2071;
    it.name = "Light Sensor";
    it.description = "Photodiode array. Slotted into an item to auto-toggle anything that depends on ambient light.";
    it.type = ItemType::CraftingMaterial;
    it.rarity = Rarity::Uncommon;
    it.weight = 1;
    it.buy_value = 120;
    it.sell_value = 40;
    return it;
}
```

If neighboring builders also call helpers like `build_energy_mod_(...)`, this minimal pattern is fine — modules don't need an enhancement-mod helper because they're consumed *into* slots rather than carrying their own slots.

If `item_defs.h` declares peer builders (it does for Solar Panels etc.), add forward declarations there too. Search for `build_solar_panel_common` in `include/astra/item_defs.h` and add nearby:

```cpp
Item build_ai_module();
Item build_light_sensor();
```

- [ ] **Step 2.3: Add dispatch arms in `build_by_def_id`**

In `src/item_defs.cpp`, locate the `build_by_def_id` switch (around line 1226). Find the energy-mods section (cases for `ITEM_TUNED_CATALYST` etc., around line 1280-1290). Add two arms immediately after the last energy-mod case:

```cpp
        case ITEM_AI_MODULE:               return build_ai_module();
        case ITEM_LIGHT_SENSOR:            return build_light_sensor();
```

- [ ] **Step 2.4: Add loot table entries**

In `src/loot_table.cpp`, locate the energy-mod section in `s_loot_table_data()` (search for `// ----- Energy mods -----`). After the last energy-mod entry (Tuned Catalyst), add a new section:

```cpp
        // ----- Accessory mods -------------------------------------------
        LootEntry{ ITEM_AI_MODULE,    "ai_module",    R::Rare,     R::Rare,     5, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,                              T::Tech, 3, C::AccessoryMod },
        LootEntry{ ITEM_LIGHT_SENSOR, "light_sensor", R::Uncommon, R::Uncommon, 12, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms,                          T::Tech, 1, C::AccessoryMod },
```

(Match the existing column-aligned format — pad with spaces so columns line up with the rest of the table.)

- [ ] **Step 2.5: Add `AccessoryMod` to per-source category weights**

Still in `src/loot_table.cpp`, find `s_category_weights()`. In the `LootSource::Chest` block, add an entry for `AccessoryMod`:

```cpp
        { LootSource::Chest, {
            { C::Weapon,           15 },
            { C::Armor,            15 },
            { C::Shield,            5 },
            { C::Accessory,         5 },
            { C::Consumable,       15 },
            { C::Battery,          10 },
            { C::Junk,             10 },
            { C::CraftingMaterial, 15 },
            { C::EnergyMod,        10 },
            { C::AccessoryMod,      5 },   // NEW
        }},
```

(`NpcDrop` and `MaintenanceTunnel` don't get `AccessoryMod` — only chests and merchants stock these. Per spec.)

- [ ] **Step 2.6: Add manifest entries in `src/npcs/merchant.cpp`**

Locate the existing `s_general_merchant_manifest` static const. Append two entries — Light Sensor at baseline, AI Module at rep ≥ 50:

```cpp
    // Accessory modules
    { StockManifestEntry::Mode::Always, ITEM_LIGHT_SENSOR, Category::AccessoryMod, 1 },
    { StockManifestEntry::Mode::Always, ITEM_AI_MODULE,    Category::AccessoryMod, 1, /*min_rep=*/50 },
```

Add `#include "astra/item_ids.h"` if not already present (Task 4 of the loot table moved it into here, so it should be).

- [ ] **Step 2.7: Add manifest entries in `src/npcs/hub_npcs.cpp`**

Locate `s_arms_dealer_manifest`. Add Light Sensor + AI Module at baseline, plus quantity bumps at rep ≥ 50:

```cpp
    // Accessory modules
    { StockManifestEntry::Mode::Always, ITEM_LIGHT_SENSOR, Category::AccessoryMod, 1 },
    { StockManifestEntry::Mode::Always, ITEM_AI_MODULE,    Category::AccessoryMod, 1 },

    { StockManifestEntry::Mode::Always, ITEM_LIGHT_SENSOR, Category::AccessoryMod, 1, /*min_rep=*/50 },
    { StockManifestEntry::Mode::Always, ITEM_AI_MODULE,    Category::AccessoryMod, 1, 50 },
```

- [ ] **Step 2.8: Add manifest entry in `src/npcs/black_market_vendor.cpp`**

Locate `s_black_market_manifest`. Add a Random entry filtered to AccessoryMod:

```cpp
    { StockManifestEntry::Mode::Random, 0, Category::AccessoryMod, 1 },
```

- [ ] **Step 2.9: Build to verify dispatch + table compile**

Run: `cmake --build build`
Expected: clean build. The dispatch-coverage check (`verify_dispatch_coverage()`) will run on next launch and confirm both new entries dispatch correctly.

- [ ] **Step 2.10: Smoke-test the new items appear**

Run: `timeout 3 ./build/astra-dev 2>&1 | grep -i "loot_table\|error" || echo "no errors"`
Expected: "no errors". The startup self-check passes — both AccessoryMod entries dispatch cleanly through `build_by_def_id`.

Optional manual check (in the running game):
```
give item                                # list — both should appear under [accessory mod]
give item ai_module                      # spawn one in inventory
give item light_sensor                   # spawn one in inventory
```

- [ ] **Step 2.11: Commit**

```bash
git add include/astra/item_ids.h include/astra/item_defs.h src/item_defs.cpp \
        src/loot_table.cpp src/npcs/merchant.cpp src/npcs/hub_npcs.cpp \
        src/npcs/black_market_vendor.cpp
git commit -m "$(cat <<'EOF'
feat(gadgets): AI Module + Light Sensor items

Two new accessory mods (Category::AccessoryMod):
- AI Module (id=94, Rare, 280c) — generic auto-trigger
- Light Sensor (id=95, Uncommon, 120c) — light-conditional auto-toggle

Both wired into the loot table (Chest 5% AccessoryMod weight),
general merchant (Light Sensor baseline, AI Module rep>=50), arms
dealer (both baseline + qty bump at rep>=50), and black market
(Random AccessoryMod). Dispatch arms added.

Currently lookup-only — Task 3 wires the slot semantics into the
Nightvision Goggles host item.

Spec: docs/superpowers/specs/2026-04-26-nightvision-modules-design.md
EOF
)"
```

---

## Task 3: Nightvision Goggles — rename, energy, slot, drain, view aggregation

**Files:**
- Modify: `src/item_defs.cpp` (rename + new fields on builder)
- Modify: `include/astra/item_defs.h` (declaration rename)
- Modify: `src/loot_table.cpp` (identifier + builder reference rename)
- Modify: `src/item.cpp` (`Equipment::total_modifiers` view aggregation)
- Modify: `src/game_world.cpp` (`is_dark_context` helper + per-tick drain in `advance_world`)

This is the heart of the feature. After this task: NV Goggles consume energy, drain when in dark contexts, grant +5 view when powered+active, fall back to +1 when depleted.

- [ ] **Step 3.1: Rename builder + display in `src/item_defs.cpp`**

Find `Item build_night_goggles()` (around line 789 — the location may have shifted; grep if needed). Rename the function and update its body:

```cpp
Item build_nightvision_goggles() {
    Item it;
    it.item_def_id = ITEM_NIGHT_GOGGLES;
    it.id = 4002;
    it.name = "Nightvision Goggles";
    it.type = ItemType::Accessory;
    it.description = "Amplifies ambient light. Useful in dark environments. Powered by a built-in cell.";
    it.slot = EquipSlot::Face;
    it.rarity = Rarity::Common;
    it.weight = 1;
    it.buy_value = 80;
    it.sell_value = 25;
    it.modifiers.view_radius = 1;
    it.energy = EnergyStore{ /*current=*/60, /*capacity=*/60 };
    it.consumer = EnergyConsumer{ /*energy_per_use=*/1 };
    it.toggleable = true;
    it.enhancement_slots = 1;
    init_enhancement_slots(it);
    return it;
}
```

The `init_enhancement_slots(it)` call follows the existing convention (every cell builder does this) — it allocates the slot vector based on `enhancement_slots`.

- [ ] **Step 3.2: Update declaration in `include/astra/item_defs.h`**

Find `Item build_night_goggles();` and rename to:

```cpp
Item build_nightvision_goggles();
```

- [ ] **Step 3.3: Update `build_by_def_id` dispatch arm**

In `src/item_defs.cpp`, find the dispatch case `case ITEM_NIGHT_GOGGLES:` (search). Update:

```cpp
        case ITEM_NIGHT_GOGGLES:           return build_nightvision_goggles();
```

- [ ] **Step 3.4: Update loot table identifier**

In `src/loot_table.cpp`, find the `LootEntry{ ITEM_NIGHT_GOGGLES, ... }` row (search for `ITEM_NIGHT_GOGGLES`). Change the identifier string from `"night_goggles"` to `"nightvision_goggles"`:

```cpp
        LootEntry{ ITEM_NIGHT_GOGGLES,      "nightvision_goggles",      R::Common,    R::Rare,      20, {}, LootSource::Chest | LootSource::MerchantGeneral,                                                  T::Civilian, 1, C::Accessory },
```

(Re-pad the column if needed to keep visual alignment with neighbours.)

- [ ] **Step 3.5: Tweak view aggregation in `Equipment::total_modifiers`**

In `src/item.cpp`, locate `Equipment::total_modifiers()` (line 85). Update the view_radius accumulation to give powered toggleable items a `+4` boost when active and powered:

```cpp
StatModifiers Equipment::total_modifiers() const {
    StatModifiers total;
    const std::optional<Item>* slots[] = {
        &face, &head, &body, &left_arm, &right_arm,
        &left_hand, &right_hand, &back, &feet,
        &thrown, &missile, &shield,
    };
    for (const auto* s : slots) {
        if (*s) {
            total.av += (*s)->modifiers.av;
            total.dv += (*s)->modifiers.dv;
            total.max_hp += (*s)->modifiers.max_hp;
            total.view_radius += (*s)->modifiers.view_radius;
            // Powered toggleable items contribute an extra +4 view when active.
            // Total bonus = base +1 (above) + boost +4 = +5 when powered.
            // Falls back to base +1 when depleted or toggled off.
            if ((*s)->toggleable && (*s)->active &&
                (*s)->energy && (*s)->energy->current > 0) {
                total.view_radius += 4;
            }
            total.quickness += (*s)->modifiers.quickness;
        }
    }
    return total;
}
```

- [ ] **Step 3.6: Add `is_dark_context` helper to `src/game_world.cpp`**

Locate `Game::recompute_fov` at `src/game_world.cpp:2042` to verify the FOV restriction logic. Add a private helper just above it (or as a free function inside the file's anonymous namespace if one exists; otherwise as a `Game` method). Free-function form, file-local:

```cpp
namespace {

// Mirrors the FOV restriction rules in Game::recompute_fov(). Returns true
// whenever the player's view radius would otherwise be clipped — i.e.,
// goggles would actually help.
bool is_dark_context(const World& world) {
    if (world.navigation().on_ship) return false;
    auto map_type = world.map().map_type();
    bool is_indoor = (map_type == MapType::SpaceStation
                  || map_type == MapType::DerelictStation
                  || map_type == MapType::Starship);
    if (is_indoor) return false;
    if (world.on_overworld()) return false;

    // Surface detail map: dark if the day clock would clip view radius.
    if (world.on_detail_map() || map_type == MapType::DetailMap) {
        int max_radius = std::max(world.map().width(), world.map().height());
        // dark if the day-modulated view radius is below the natural max
        return world.day_clock().effective_view_radius(max_radius, 0) < max_radius;
    }

    // Dungeon (anything else) — always dark.
    return true;
}

} // anonymous namespace
```

(Pass `0` as `light_radius` to `effective_view_radius` because we only need to know whether the radius is clipped, not what value to clip to. Verify the signature; if `effective_view_radius` chokes on `0`, pass `1` or any small int — the comparison `< max_radius` is what we care about.)

If `World`, `MapType`, or related types aren't already in scope at that point of `game_world.cpp`, they should be — `recompute_fov` uses them right below. No new includes needed.

- [ ] **Step 3.7: Helper for "any committed module slot" in `Item`**

Add a small helper alongside `Equipment::total_modifiers`. In `src/item.cpp`, after the `total_modifiers` definition (around line 102), add:

```cpp
bool item_has_active_module(const Item& item) {
    for (const auto& slot : item.enhancements) {
        if (slot.committed && slot.module_kind != ModuleKind::None) {
            return true;
        }
    }
    return false;
}
```

Declare it in `include/astra/item.h` near the existing free-function declarations (search for `total_modifiers` or just place after the `Equipment` struct):

```cpp
// True if any committed slot on this item carries a behavioral module.
bool item_has_active_module(const Item& item);
```

- [ ] **Step 3.8: Add per-tick drain + auto-toggle in `Game::advance_world`**

In `src/game_world.cpp`, locate `Game::advance_world` at line 2120. Find a clean insertion point — between the world_tick increment (line 2141) and the effect tick (line 2144) is a natural spot. Add a new block:

```cpp
    // --- Toggleable powered items (Nightvision Goggles, etc.) -----------
    {
        bool dark = is_dark_context(world_);
        std::optional<Item>* slots[] = {
            &player_.equipment.face, &player_.equipment.head,
            &player_.equipment.body, &player_.equipment.left_arm,
            &player_.equipment.right_arm, &player_.equipment.left_hand,
            &player_.equipment.right_hand, &player_.equipment.back,
            &player_.equipment.feet, &player_.equipment.thrown,
            &player_.equipment.missile, &player_.equipment.shield,
        };
        for (auto* s : slots) {
            if (!*s) continue;
            Item& it = **s;
            if (!it.toggleable) continue;
            if (!it.energy.has_value()) continue;

            // Auto-mode: any committed module overrides player intent.
            if (item_has_active_module(it)) {
                it.active = (dark && it.energy->current > 0);
            }

            if (!it.active) continue;
            if (!dark) continue;
            if (it.energy->current <= 0) continue;

            it.drain_accumulator += 1;
            if (it.drain_accumulator >= 10) {
                it.drain_accumulator -= 10;
                int cost = it.consumer ? it.consumer->energy_per_use : 1;
                it.energy->current -= cost;
                if (it.energy->current < 0) it.energy->current = 0;
            }
        }
    }
```

The slot list mirrors `Equipment::total_modifiers()`'s slot iteration. The check guards (toggleable + has energy) skip every existing item harmlessly.

If `item_has_active_module` isn't visible in `game_world.cpp`, add `#include "astra/item.h"` (it's almost certainly already included transitively).

- [ ] **Step 3.9: Build and verify**

Run: `cmake --build build`
Expected: clean build.

If any compile errors mention `night_goggles`, you've missed a rename in `include/astra/item_defs.h` or somewhere else. `grep -rn "build_night_goggles\|night_goggles\b" src/ include/` to find lingering references.

- [ ] **Step 3.10: Smoke test the goggles work end-to-end**

```
timeout 3 ./build/astra-dev 2>&1 | grep -i "error\|loot_table" || echo "no errors"
```

Optional in-game manual test:
1. Open dev console (backtick).
2. `give item nightvision_goggles` — should appear in inventory.
3. Equip via 'q' / character screen.
4. Check current view radius. Try toggling 'g' to activate (manual mode: no module slotted yet) — Task 4 finishes the toggle UI; for now you can verify by inspecting the inventory.
5. The dispatch-coverage startup check should pass — that confirms `build_by_def_id(ITEM_NIGHT_GOGGLES)` returns an item with `item_def_id == 24`.

If the toggle key 'g' doesn't yet activate the goggles (because Task 4 wires that), the goggles passively show `+1` view from `modifiers.view_radius`, even before Task 4 ships. That's the graceful fallback working.

- [ ] **Step 3.11: Commit**

```bash
git add src/item_defs.cpp include/astra/item_defs.h \
        src/loot_table.cpp src/item.cpp include/astra/item.h \
        src/game_world.cpp
git commit -m "$(cat <<'EOF'
feat(gadgets): Nightvision Goggles with energy + per-tick drain

Renames Night Goggles -> Nightvision Goggles (same item_def_id 24,
display + identifier change). Adds:
- EnergyStore { current=60, capacity=60 } and EnergyConsumer (1/use)
- 1 enhancement slot (accepts AI Module / Light Sensor in Task 4 use)
- toggleable=true; active flag drives the +5 view boost
- Equipment::total_modifiers() adds +4 view when item is toggleable+
  active+powered (base +1 stays for graceful fallback when depleted)

is_dark_context() helper mirrors Game::recompute_fov restriction
rules — true on detail map at night/dawn/dusk, true in dungeons,
false on overworld + station + ship interiors.

Per-tick drain block in Game::advance_world walks equipment, applies
auto-toggle if a committed module is present, drains 1 charge per
10 ticks while active in a dark context.

Spec: docs/superpowers/specs/2026-04-26-nightvision-modules-design.md
EOF
)"
```

---

## Task 4: Manual toggle UI + tinkering recipes

**Files:**
- Modify: `src/character_screen.cpp` (extend 'g' to toggle active for toggleable items)
- Modify: `src/tinkering.cpp` (2 synthesis recipes)

After this task: player can manually toggle goggles via 'g' in inventory; modules promote to auto-mode and disable manual; both modules are craftable through the synthesizer.

- [ ] **Step 4.1: Extend 'g' inventory key in `src/character_screen.cpp`**

Find the existing 'g' handler (around line 944 — currently toggles solar panels). Refactor to handle both cases:

```cpp
        } else if (key == 'g') {
            auto& item = items[inv_cursor_];

            // First: solar panel toggle on a host cell (existing behavior)
            bool handled = false;
            for (auto& enh : item.enhancements) {
                if (enh.committed && enh.solar_panel) {
                    enh.solar_panel->active = !enh.solar_panel->active;
                    context_message_ = std::string("Solar Panel ") +
                                       (enh.solar_panel->active ? "enabled." : "disabled.");
                    context_msg_timer_ = 3;
                    handled = true;
                    break;
                }
            }
            if (handled) return;

            // Second: toggle a powered item (Nightvision Goggles, etc.)
            if (item.toggleable) {
                if (item_has_active_module(item)) {
                    context_message_ = "Auto mode (module installed) — manual toggle disabled.";
                    context_msg_timer_ = 3;
                } else {
                    item.active = !item.active;
                    context_message_ = item.label() + (item.active ? " on." : " off.");
                    context_msg_timer_ = 3;
                }
            }
        }
```

(Match the existing `return;` / fall-through idiom in the surrounding handler — copy whatever the neighbouring branches use. The `return` after the solar-panel branch above is the typical pattern; adjust if the handler is in a `case`/`break` style.)

If `item_has_active_module` isn't already available, `#include "astra/item.h"` should suffice.

- [ ] **Step 4.2: Update inventory footer hint (if present)**

Search for the inventory help-text or footer that describes 'g' (probably near where other key hints are listed in `character_screen.cpp` — search for `"g"` or `"toggle"` or footer-render code). Update the hint to reflect the broader meaning, e.g.:

```
"g toggle gear / solar panel"
```

If no such hint exists in this code path, skip this step.

- [ ] **Step 4.3: Add synthesis recipes for AI Module + Light Sensor**

In `src/tinkering.cpp`, find `synthesis_recipes()` (around line 376). Locate the section where energy-mod recipes use the `custom_builder` field (around line 428 — search for `&build_reinforced_casing`). After the last energy-mod recipe, add:

```cpp
        // Accessory module recipes — produce behavioral modules via custom builders.
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

Verify these blueprint pairs aren't already used elsewhere in `synthesis_recipes()`. As of writing:
- `Optic Module + Joint Mechanism` — unused.
- `Optic Module + Padding Weave` — unused.

If a future recipe steals one of these pairs, change the blueprint pairing for the affected module here.

The `&build_ai_module` / `&build_light_sensor` references resolve via the existing `#include "astra/item_defs.h"` (already at top of `tinkering.cpp` — verify).

- [ ] **Step 4.4: Build to verify recipes + UI compile**

Run: `cmake --build build`
Expected: clean build.

If a linker error mentions `build_ai_module` / `build_light_sensor` from `tinkering.cpp`, add `#include "astra/item_defs.h"` to the top of `tinkering.cpp`.

- [ ] **Step 4.5: Smoke test**

```
timeout 3 ./build/astra-dev 2>&1 | grep -i "error\|loot_table" || echo "no errors"
```

Optional in-game manual test:
1. `give item nightvision_goggles` → equip.
2. `give item light_sensor` → enter character screen, navigate to tinkering, slot the Light Sensor into the goggles' enhancement slot, commit.
3. Observe: pressing 'g' should now show "Auto mode (module installed) — manual toggle disabled."
4. Walk into a dungeon (or wait for night via `wait` command) — view radius should expand from base to +5 above lights.
5. After ~600 ticks of dark exposure, charge should hit 0 and view drops back to +1.
6. Verify the synthesis path: `give item` shows AI Module / Light Sensor in [accessory mod]. In tinkering, learning blueprints `Optic Module` + `Joint Mechanism` and synthesizing should produce an AI Module.

- [ ] **Step 4.6: Commit**

```bash
git add src/character_screen.cpp src/tinkering.cpp
git commit -m "$(cat <<'EOF'
feat(gadgets): manual toggle UI + tinker recipes for modules

- Inventory 'g' key now toggles powered items (item.active) in
  manual mode, or shows an "auto mode" status message when a
  committed module is present (manual toggle disabled).
- Synthesis recipes for both modules:
  - Optic Module + Joint Mechanism -> AI Module
  - Optic Module + Padding Weave   -> Light Sensor
  Both use custom_builder to skip equipment-result fields.

Spec: docs/superpowers/specs/2026-04-26-nightvision-modules-design.md
EOF
)"
```

---

## Self-Review Checklist (run by writer, fix inline)

1. **Spec coverage:**
   - Item shape: `EnergyStore`, `EnergyConsumer`, 1 slot, toggleable=true, active, drain_accumulator → Task 1 (fields), Task 3.1 (set on builder).
   - View aggregation +5/+1 → Task 3.5.
   - Per-tick drain + auto-toggle → Task 3.6 (helper) + 3.7 (slot helper) + 3.8 (advance_world block).
   - `is_dark_context` matching FOV rules → Task 3.6.
   - AI Module + Light Sensor items → Task 2.1-2.3.
   - `Category::AccessoryMod` → Task 1.3.
   - Loot table entries + chest weight → Task 2.4-2.5.
   - Merchant manifests (general, arms, black market) → Task 2.6-2.8.
   - Save bump 47→48 → Task 1.4-1.6.
   - Manual toggle UI → Task 4.1-4.2.
   - Auto-mode disables manual → Task 4.1 (the `if (item_has_active_module(item))` branch).
   - Synthesis recipes → Task 4.3.
   - Item rename Night → Nightvision → Task 3.1 (builder), 3.2 (decl), 3.3 (dispatch), 3.4 (loot identifier).

2. **Placeholder scan:** No "TBD" / "TODO" / "implement later" in plan body. Step 4.2 has a conditional ("If no such hint exists, skip") but provides the precise change if it does — acceptable conditional.

3. **Type consistency:** Spot-checked names across tasks:
   - `ITEM_AI_MODULE`, `ITEM_LIGHT_SENSOR`, `ModuleKind`, `Category::AccessoryMod`, `Item::toggleable / active / drain_accumulator`, `EnhancementSlot::module_kind`, `build_ai_module`, `build_light_sensor`, `build_nightvision_goggles`, `is_dark_context`, `item_has_active_module` — all spelled identically wherever used.
   - Manifest field order matches the `StockManifestEntry` struct (`Mode::Always/Random`, item_def_id, category, quantity, min_reputation).
   - `LootEntry` initializer field order matches the struct (item_def_id, identifier, min_rarity, max_rarity, default_weight, source_weights, source_mask, theme, min_level, category).

4. **Bite-sized step granularity:** Steps mostly fit 2-5 minutes. The heaviest are Step 3.5 (replace ~15 lines of `total_modifiers`) and Step 3.8 (insert new ~30-line block in `advance_world`) — both unavoidable because they're the integration points; full code is shown.

5. **Build gate at every commit:** Tasks 1, 2, 3, 4 each end with build verification before commit. Each commit leaves the build green.
