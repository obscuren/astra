# Loot Table System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace hardcoded item-spawn ladders with a tag-driven loot table — single source of truth for what items exist, where they drop, at what rarity.

**Architecture:** Static C++ table (`s_loot_table`) of `LootEntry` structs. Each entry tags itself with sources (bitset), category, rarity range, and weights. Two query APIs: `roll_loot()` for procedural drops, `assemble_stock()` for merchant inventories. Dev console `give item` becomes table-driven.

**Tech Stack:** C++20, CMake, single-binary build. **No formal test framework** — verification is build success + dev-mode runtime smoke tests + a startup self-check assertion that walks the table.

**Spec:** `docs/superpowers/specs/2026-04-25-loot-table-design.md`

**Build / verify commands (use these throughout):**
- One-time setup: `cmake -B build -DDEV=ON`
- Rebuild: `cmake --build build` (must succeed after every step that touches code)
- Run: `./build/astra-dev`
- Open dev console in-game: backtick (`` ` ``)

**File structure created by this plan:**
```
include/astra/loot_source.h    # LootSource, Theme, Category enums + helpers
include/astra/loot_table.h     # LootEntry, StockManifestEntry, public API
src/loot_table.cpp             # s_loot_table data + roll_loot + assemble_stock
                               # + s_category_weights + verify_dispatch_coverage()
include/astra/item_gen.h       # +scale_item_to_rarity declaration (existing file)
src/item_gen.cpp               # +scale_item_to_rarity definition (existing file)
src/item_defs.cpp              # build_by_def_id expanded to all items (existing file)
src/dev_console.cpp            # give-item rewritten table-driven (existing file)
src/game_combat.cpp            # roll_loot replaces generate_loot_drop (existing file)
src/npcs/merchant.cpp          # manifest + assemble_stock (existing file)
src/npcs/hub_npcs.cpp          # manifest + assemble_stock (existing file)
src/npcs/scav_merchant.cpp     # manifest + assemble_stock (existing file)
src/npcs/black_market_vendor.cpp # manifest + assemble_stock (existing file)
include/astra/item.h           # delete dead LootTable/LootEntry structs (existing file)
CMakeLists.txt                 # add src/loot_table.cpp to ASTRA_SOURCES
```

---

## Task 1: Scaffolding — types, helpers, and full `build_by_def_id` dispatch

**Files:**
- Create: `include/astra/loot_source.h`
- Create: `include/astra/loot_table.h`
- Modify: `include/astra/item_gen.h` (add `scale_item_to_rarity` declaration)
- Modify: `src/item_gen.cpp` (add `scale_item_to_rarity` definition)
- Modify: `src/item_defs.cpp` (expand `build_by_def_id` switch)

This task adds types and the universal item constructor. No call-site changes — everything is purely additive. Build must succeed at the end.

- [ ] **Step 1.1: Create `include/astra/loot_source.h` with the three enums + helpers**

```cpp
#pragma once

#include <cstdint>
#include <string_view>

namespace astra {

// Bitset enum: an item's source_mask is OR of every LootSource it appears in.
// uint64_t gives us 64 distinct source slots.
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
};

constexpr uint64_t loot_source_bit(LootSource s) {
    return static_cast<uint64_t>(s);
}

constexpr uint64_t operator|(LootSource a, LootSource b) {
    return loot_source_bit(a) | loot_source_bit(b);
}

constexpr uint64_t operator|(uint64_t a, LootSource b) {
    return a | loot_source_bit(b);
}

// Single-valued per entry. Used for filtering and future affix-theme alignment.
enum class Theme : uint8_t {
    None,
    Military,
    Ancient,
    Alien,
    Scrap,
    Civilian,
    Tech,
};

// Coarser than ItemType: MeleeWeapon and RangedWeapon both → Category::Weapon.
// Used by the per-source category roll and by manifest filters.
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
    QuestItem,
};

std::string_view category_name(Category c);

} // namespace astra
```

- [ ] **Step 1.2: Add `category_name` definition to `src/loot_table.cpp`** — wait, `loot_table.cpp` doesn't exist yet. Put `category_name` in a new tiny file alongside the enum. Add this *inline* in `loot_source.h` instead to avoid a CMake change in this task.

Replace the trailing declaration `std::string_view category_name(Category c);` with:

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
        case Category::QuestItem:        return "quest";
    }
    return "?";
}
```

(The `switch` is exhaustive over the enum — no `default` clause, so adding a new Category triggers a compiler warning if the function isn't updated.)

- [ ] **Step 1.3: Create `include/astra/loot_table.h` with `LootEntry`, `StockManifestEntry`, and public API**

```cpp
#pragma once

#include "astra/item.h"
#include "astra/loot_source.h"

#include <map>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace astra {

struct LootEntry {
    uint16_t    item_def_id     = 0;       // → build_by_def_id()
    std::string identifier;                 // stable string handle for `give item`
    Rarity      min_rarity      = Rarity::Common;
    Rarity      max_rarity      = Rarity::Common;
    int         default_weight  = 0;
    std::map<LootSource, int> source_weights;  // optional per-source override
    uint64_t    source_mask     = 0;
    Theme       theme           = Theme::None;
    int         min_level       = 1;
    Category    category        = Category::Junk;
};

struct StockManifestEntry {
    enum Mode { Always, Random };
    Mode      mode           = Random;
    uint16_t  item_def_id    = 0;          // Always: which item; Random: ignored
    Category  category       = Category::Junk;  // Random: filter; Always: ignored
    int       quantity       = 1;
    int       min_reputation = 0;
};

// --- Public API ---------------------------------------------------------

// Pick + build a single loot item.
// `source` is the bitset of contexts the call is rolling for (typically a single bit).
// `forced_category` bypasses the per-source category roll when set.
// Returns std::nullopt if no entry matches the filter.
std::optional<Item> roll_loot(LootSource source,
                              int level,
                              std::mt19937& rng,
                              std::optional<Category> forced_category = std::nullopt);

// Build a merchant stock vector from a manifest.
std::vector<Item> assemble_stock(const std::vector<StockManifestEntry>& manifest,
                                 LootSource source,
                                 int faction_rep,
                                 int level,
                                 std::mt19937& rng);

// Find an entry by its string identifier (case-insensitive). For dev console.
const LootEntry* find_entry_by_identifier(std::string_view identifier);

// Read-only access to the full table (for dev-console list, debug overlays, etc).
const std::vector<LootEntry>& loot_table_all_entries();

// Startup self-check: returns false (and logs to stderr) if any entry's
// item_def_id is not handled by build_by_def_id. Call from main() in DEV builds.
bool verify_dispatch_coverage();

} // namespace astra
```

- [ ] **Step 1.4: Add `scale_item_to_rarity` declaration to `include/astra/item_gen.h`**

Open `include/astra/item_gen.h` and add the new declaration alongside the existing ones. The file currently declares `scale_item_to_level`, `apply_affix`, `roll_rarity`, `generate_*`, `apply_rarity_affixes`. Add this declaration:

```cpp
// Apply a flat rarity multiplier on top of base stats.
// Multipliers: Common 1.00, Uncommon 1.10, Rare 1.25, Epic 1.45, Legendary 1.75.
// Scales: damage_dice.modifier, av, dv, max_hp, durability, energy capacity,
// buy/sell value. Idempotent if you pass Common.
void scale_item_to_rarity(Item& item, Rarity rarity);
```

- [ ] **Step 1.5: Add `scale_item_to_rarity` definition to `src/item_gen.cpp`**

Find the existing `scale_item_to_level` definition (around line 46). Add this function immediately after it:

```cpp
void scale_item_to_rarity(Item& item, Rarity rarity) {
    float mult = 1.0f;
    switch (rarity) {
        case Rarity::Common:    mult = 1.00f; break;
        case Rarity::Uncommon:  mult = 1.10f; break;
        case Rarity::Rare:      mult = 1.25f; break;
        case Rarity::Epic:      mult = 1.45f; break;
        case Rarity::Legendary: mult = 1.75f; break;
    }
    if (mult == 1.0f) {
        item.rarity = rarity;
        return;
    }

    auto scale = [mult](int v) -> int {
        return static_cast<int>(std::round(v * mult));
    };

    item.rarity = rarity;
    item.damage_dice.modifier = scale(item.damage_dice.modifier);
    item.modifiers.av         = scale(item.modifiers.av);
    item.modifiers.dv         = scale(item.modifiers.dv);
    item.modifiers.max_hp     = scale(item.modifiers.max_hp);
    item.max_durability       = scale(item.max_durability);
    item.durability           = item.max_durability;
    item.buy_value            = scale(item.buy_value);
    item.sell_value           = scale(item.sell_value);

    if (item.energy && (item.ranged || item.type == ItemType::Shield)) {
        int scaled = scale(item.energy->capacity);
        item.energy->capacity = scaled;
        item.energy->current  = scaled;
    }
}
```

- [ ] **Step 1.6: Expand `build_by_def_id` in `src/item_defs.cpp` to dispatch every existing item**

Replace the existing switch body at `src/item_defs.cpp:1226-1242` with the full dispatch. The current switch only covers cooking-related items; this expansion adds every other item that has a builder.

```cpp
Item build_by_def_id(uint16_t def_id) {
    switch (def_id) {
        // Ranged weapons
        case ITEM_PLASMA_PISTOL:           return build_plasma_pistol();
        case ITEM_ION_BLASTER:             return build_ion_blaster();
        case ITEM_PULSE_RIFLE:             return build_pulse_rifle();
        case ITEM_ARC_CASTER:              return build_arc_caster();
        case ITEM_VOID_LANCE:              return build_void_lance();

        // Melee weapons
        case ITEM_COMBAT_KNIFE:            return build_combat_knife();
        case ITEM_VIBRO_BLADE:             return build_vibro_blade();
        case ITEM_PLASMA_SABER:            return build_plasma_saber();
        case ITEM_STUN_BATON:              return build_stun_baton();
        case ITEM_ANCIENT_MONO_EDGE:       return build_ancient_mono_edge();

        // Armor
        case ITEM_PADDED_VEST:             return build_padded_vest();
        case ITEM_COMPOSITE_ARMOR:         return build_composite_armor();
        case ITEM_EXO_SUIT:                return build_exo_suit();
        case ITEM_FLIGHT_HELMET:           return build_flight_helmet();
        case ITEM_TACTICAL_HELMET:         return build_tactical_helmet();
        case ITEM_COMBAT_BOOTS:            return build_combat_boots();
        case ITEM_MAG_LOCK_BOOTS:          return build_mag_lock_boots();
        case ITEM_ARM_GUARD:               return build_arm_guard();

        // Shields
        case ITEM_BASIC_DEFLECTOR:         return build_basic_deflector();
        case ITEM_PLASMA_SCREEN:           return build_plasma_screen();
        case ITEM_ION_BARRIER:             return build_ion_barrier();
        case ITEM_COMPOSITE_BARRIER:       return build_composite_barrier();
        case ITEM_HARDLIGHT_AEGIS:         return build_hardlight_aegis();
        case ITEM_VOID_MANTLE:             return build_void_mantle();

        // Accessories
        case ITEM_RECON_VISOR:             return build_recon_visor();
        case ITEM_NIGHT_GOGGLES:           return build_night_goggles();
        case ITEM_JETPACK:                 return build_jetpack();
        case ITEM_CARGO_PACK:              return build_cargo_pack();

        // Grenades
        case ITEM_FRAG_GRENADE:            return build_frag_grenade();
        case ITEM_EMP_GRENADE:             return build_emp_grenade();
        case ITEM_CRYO_GRENADE:            return build_cryo_grenade();

        // Consumables
        case ITEM_BATTERY:                 return build_battery();
        case ITEM_RATION_PACK:             return build_ration_pack();
        case ITEM_COMBAT_STIM:             return build_combat_stim();

        // Energy cells
        case ITEM_SMALL_ENERGY_CELL:       return build_small_energy_cell();
        case ITEM_STANDARD_ENERGY_CELL:    return build_standard_energy_cell();
        case ITEM_LARGE_ENERGY_CELL:       return build_large_energy_cell();
        case ITEM_INDUSTRIAL_ENERGY_CELL:  return build_industrial_energy_cell();
        case ITEM_ANTIMATTER_CELL:         return build_antimatter_cell();
        case ITEM_BULWARK_CELL:            return build_bulwark_cell();
        case ITEM_VOLATILE_CELL:           return build_volatile_cell();
        case ITEM_ADRENAL_CELL:            return build_adrenal_cell();

        // Energy mods
        case ITEM_SOLAR_PANEL_COMMON:      return build_solar_panel_common();
        case ITEM_SOLAR_PANEL_UNCOMMON:    return build_solar_panel_uncommon();
        case ITEM_SOLAR_PANEL_RARE:        return build_solar_panel_rare();
        case ITEM_CAPACITOR_COIL:          return build_capacitor_coil();
        case ITEM_CHARGE_CATALYST:         return build_charge_catalyst();
        case ITEM_POLISHED_CONDUIT:        return build_polished_conduit();
        case ITEM_REINFORCED_CASING:       return build_reinforced_casing();
        case ITEM_RECEPTOR_PLATE:          return build_receptor_plate();
        case ITEM_BRASS_CONDUIT:           return build_brass_conduit();
        case ITEM_POWER_JUNCTION:          return build_power_junction();
        case ITEM_TUNED_CATALYST:          return build_tuned_catalyst();

        // Junk
        case ITEM_SCRAP_METAL:             return build_scrap_metal();
        case ITEM_BROKEN_CIRCUIT:          return build_broken_circuit();
        case ITEM_EMPTY_CASING:            return build_empty_casing();

        // Salvage
        case ITEM_SPARE_PARTS:             return build_spare_parts();
        case ITEM_CIRCUITRY:               return build_circuitry();

        // Crafting materials
        case ITEM_NANO_FIBER:              return build_nano_fiber();
        case ITEM_POWER_CORE:              return build_power_core();
        case ITEM_CIRCUIT_BOARD:           return build_circuit_board();
        case ITEM_ALLOY_INGOT:             return build_alloy_ingot();

        // Ship components
        case ITEM_ENGINE_COIL_MK1:         return build_engine_coil_mk1();
        case ITEM_HULL_PLATE:              return build_hull_plate();
        case ITEM_SHIELD_GENERATOR:        return build_shield_generator();
        case ITEM_NAVI_COMPUTER_MK2:       return build_navi_computer_mk2();

        // Cooking ingredients
        case ITEM_RAW_MEAT:                return build_raw_meat();
        case ITEM_CARROT:                  return build_carrot();
        case ITEM_FLOUR:                   return build_flour();
        case ITEM_HERBS:                   return build_herbs();
        case ITEM_SYNTH_PROTEIN:           return build_synth_protein();

        // Cooked dishes
        case ITEM_COOKED_MEAT:             return build_cooked_meat();
        case ITEM_BOWL_OF_BROTH:           return build_bowl_of_broth();
        case ITEM_FLATBREAD:               return build_flatbread();
        case ITEM_HEARTY_STEW:             return build_hearty_stew();
        case ITEM_PROTEIN_BAKE:            return build_protein_bake();
        case ITEM_HEROS_FEAST:             return build_heros_feast();
        case ITEM_BURNT_SLOP:              return build_burnt_slop();

        // Cookbooks
        case ITEM_COOKBOOK_HEARTY_STEW:    return build_cookbook_hearty_stew();
        case ITEM_COOKBOOK_PROTEIN_BAKE:   return build_cookbook_protein_bake();
        case ITEM_COOKBOOK_HEROS_FEAST:    return build_cookbook_heros_feast();
    }
    return Item{};
}
```

Note: `ITEM_RIOT_SHIELD` is intentionally **not** dispatched — search the codebase first to confirm it has no `build_riot_shield()` function (it's declared in `item_ids.h` but no builder exists). If a builder is found, add a case. Otherwise leave it absent — the `verify_dispatch_coverage()` check (added in Task 2) will not flag it because nothing in the table will reference it.

- [ ] **Step 1.7: Build to verify scaffolding compiles**

Run: `cmake --build build`
Expected: clean build. No new warnings about unused declarations (since the new headers are declared but not yet referenced from any `.cpp` outside `item_defs.cpp` and `item_gen.cpp`).

If build fails: most likely a missing `#include` in `item_gen.cpp` (need `<cmath>` for `std::round` — already present from `scale_item_to_level`) or a typo in a `case` arm. Read the compiler error literally.

- [ ] **Step 1.8: Commit**

```bash
git add include/astra/loot_source.h include/astra/loot_table.h \
        include/astra/item_gen.h src/item_gen.cpp src/item_defs.cpp
git commit -m "$(cat <<'EOF'
feat(loot): scaffolding for tag-driven loot table

Introduces LootSource (uint64_t bitset), Theme, Category enums.
Declares LootEntry, StockManifestEntry, and the roll_loot /
assemble_stock public API (no implementation yet).

Adds scale_item_to_rarity helper. Expands build_by_def_id to
dispatch every existing item, making it the universal item
constructor for the upcoming table-driven pipeline.

Pure scaffolding — no call-site changes, build remains green.

Spec: docs/superpowers/specs/2026-04-25-loot-table-design.md
EOF
)"
```

---

## Task 2: Populate the loot table + implement `roll_loot` / `assemble_stock`

**Files:**
- Create: `src/loot_table.cpp` — table data + query implementations
- Modify: `CMakeLists.txt` — add the new source file
- Modify: `src/main.cpp` — wire `verify_dispatch_coverage()` into DEV-build startup

This task adds all the data and logic. Table sits unused at the end of this task; commits 3-5 wire it in.

- [ ] **Step 2.1: Add `src/loot_table.cpp` to `CMakeLists.txt`**

Open `CMakeLists.txt`. Find the line `src/item_gen.cpp` (line ~125). Add `src/loot_table.cpp` immediately after it:

```cmake
    src/item_gen.cpp
    src/loot_table.cpp
    src/recipe_defs.cpp
```

- [ ] **Step 2.2: Create `src/loot_table.cpp` skeleton with includes and namespace**

```cpp
#include "astra/loot_table.h"
#include "astra/item_defs.h"
#include "astra/item_gen.h"
#include "astra/item_ids.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <numeric>
#include <unordered_set>

namespace astra {

namespace {

// ---------------------------------------------------------------------------
// The loot table. One entry per item. Each entry is the single source of
// truth for: where the item can drop (source_mask), at what rarity range,
// at what weight, in which category, with which theme.
// ---------------------------------------------------------------------------

const std::vector<LootEntry>& s_loot_table_data();
const std::map<LootSource, std::map<Category, int>>& s_category_weights();

}  // anonymous namespace

}  // namespace astra
```

(We define the data through a function-local static so initialization order is well-defined and the table can reference inline-built `Rarity` constants.)

- [ ] **Step 2.3: Populate `s_loot_table_data()` with every item**

Inside the anonymous namespace in `src/loot_table.cpp`, add:

```cpp
const std::vector<LootEntry>& s_loot_table_data() {
    using R = Rarity;
    using C = Category;
    using T = Theme;

    static const std::vector<LootEntry> data = {
        // ----- Ranged weapons --------------------------------------------
        { ITEM_PLASMA_PISTOL,  "plasma_pistol",  R::Common,    R::Rare,      40, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                T::Tech,     1, C::Weapon },
        { ITEM_ION_BLASTER,    "ion_blaster",    R::Common,    R::Rare,      30, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                T::Tech,     1, C::Weapon },
        { ITEM_PULSE_RIFLE,    "pulse_rifle",    R::Uncommon,  R::Epic,      18, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                T::Military, 2, C::Weapon },
        { ITEM_ARC_CASTER,     "arc_caster",     R::Rare,      R::Epic,       9, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket, T::Tech, 3, C::Weapon },
        { ITEM_VOID_LANCE,     "void_lance",     R::Epic,      R::Legendary,  3, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::BlackMarket,                  T::Ancient,  5, C::Weapon },

        // ----- Melee weapons ---------------------------------------------
        { ITEM_COMBAT_KNIFE,       "combat_knife",       R::Common,    R::Rare,      35, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms | LootSource::ScavMerchant, T::Civilian, 1, C::Weapon },
        { ITEM_STUN_BATON,         "stun_baton",         R::Common,    R::Rare,      20, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                              T::Tech,     1, C::Weapon },
        { ITEM_VIBRO_BLADE,        "vibro_blade",        R::Uncommon,  R::Epic,      20, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                              T::Tech,     2, C::Weapon },
        { ITEM_PLASMA_SABER,       "plasma_saber",       R::Rare,      R::Epic,      17, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,    T::Military, 3, C::Weapon },
        { ITEM_ANCIENT_MONO_EDGE,  "ancient_mono_edge",  R::Epic,      R::Legendary,  8, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::BlackMarket,                                T::Ancient,  5, C::Weapon },

        // ----- Armor -----------------------------------------------------
        { ITEM_PADDED_VEST,     "padded_vest",     R::Common,    R::Rare,      22, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantGeneral | LootSource::ScavMerchant, T::Civilian, 1, C::Armor },
        { ITEM_FLIGHT_HELMET,   "flight_helmet",   R::Common,    R::Rare,      16, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantGeneral,                              T::Civilian, 1, C::Armor },
        { ITEM_COMBAT_BOOTS,    "combat_boots",    R::Common,    R::Rare,      16, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms,    T::Military, 1, C::Armor },
        { ITEM_ARM_GUARD,       "arm_guard",       R::Common,    R::Rare,      13, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                                  T::Military, 1, C::Armor },
        { ITEM_COMPOSITE_ARMOR, "composite_armor", R::Uncommon,  R::Epic,      13, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                                  T::Military, 2, C::Armor },
        { ITEM_TACTICAL_HELMET, "tactical_helmet", R::Uncommon,  R::Epic,      10, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                                  T::Military, 2, C::Armor },
        { ITEM_MAG_LOCK_BOOTS,  "mag_lock_boots",  R::Rare,      R::Epic,       7, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,        T::Tech,     3, C::Armor },
        { ITEM_EXO_SUIT,        "exo_suit",        R::Epic,      R::Legendary,  3, {}, LootSource::Chest | LootSource::BlackMarket,                                                          T::Military, 5, C::Armor },

        // ----- Shields ---------------------------------------------------
        { ITEM_BASIC_DEFLECTOR,    "basic_deflector",    R::Common,    R::Rare,      35, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms, T::Tech,     1, C::Shield },
        { ITEM_PLASMA_SCREEN,      "plasma_screen",      R::Uncommon,  R::Epic,      20, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                                T::Tech,     2, C::Shield },
        { ITEM_ION_BARRIER,        "ion_barrier",        R::Uncommon,  R::Epic,      20, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                                T::Tech,     2, C::Shield },
        { ITEM_COMPOSITE_BARRIER,  "composite_barrier",  R::Rare,      R::Epic,      15, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,    T::Military, 3, C::Shield },
        { ITEM_HARDLIGHT_AEGIS,    "hardlight_aegis",    R::Rare,      R::Legendary,  7, {}, LootSource::Chest | LootSource::BlackMarket,                                                       T::Tech,     4, C::Shield },
        { ITEM_VOID_MANTLE,        "void_mantle",        R::Epic,      R::Legendary,  3, {}, LootSource::Chest | LootSource::BlackMarket,                                                       T::Ancient,  5, C::Shield },

        // ----- Accessories -----------------------------------------------
        { ITEM_NIGHT_GOGGLES,      "night_goggles",      R::Common,    R::Rare,      20, {}, LootSource::Chest | LootSource::MerchantGeneral,                                                  T::Civilian, 1, C::Accessory },
        { ITEM_RECON_VISOR,        "recon_visor",        R::Uncommon,  R::Epic,      10, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms,                       T::Tech,     2, C::Accessory },
        { ITEM_JETPACK,            "jetpack",            R::Rare,      R::Epic,       5, {}, LootSource::Chest | LootSource::BlackMarket,                                                       T::Tech,     3, C::Accessory },
        { ITEM_CARGO_PACK,         "cargo_pack",         R::Common,    R::Rare,      15, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::ScavMerchant,                       T::Civilian, 1, C::Accessory },

        // ----- Consumables -----------------------------------------------
        { ITEM_RATION_PACK,        "ration_pack",        R::Common,    R::Common,    50, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantFood, T::Civilian, 1, C::Consumable },
        { ITEM_COMBAT_STIM,        "combat_stim",        R::Common,    R::Uncommon,  30, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantFood | LootSource::MerchantArms, T::Tech, 1, C::Consumable },
        { ITEM_FRAG_GRENADE,       "frag_grenade",       R::Common,    R::Uncommon,  20, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                              T::Military, 1, C::Consumable },
        { ITEM_EMP_GRENADE,        "emp_grenade",        R::Uncommon,  R::Rare,      15, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantArms,                              T::Tech,     2, C::Consumable },
        { ITEM_CRYO_GRENADE,       "cryo_grenade",       R::Uncommon,  R::Rare,      10, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,                          T::Tech,     2, C::Consumable },

        // ----- Batteries (own category, distinct from Consumable) -------
        { ITEM_SMALL_ENERGY_CELL,      "cell_small",      R::Common,    R::Common,    50, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms | LootSource::ScavMerchant, T::Tech, 1, C::Battery },
        { ITEM_STANDARD_ENERGY_CELL,   "cell_standard",   R::Common,    R::Uncommon,  35, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms,                          T::Tech, 1, C::Battery },
        { ITEM_LARGE_ENERGY_CELL,      "cell_large",      R::Uncommon,  R::Rare,      18, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,                                                     T::Tech, 2, C::Battery },
        { ITEM_INDUSTRIAL_ENERGY_CELL, "cell_industrial", R::Rare,      R::Epic,       7, {}, LootSource::Chest | LootSource::BlackMarket,                                                                                T::Tech, 3, C::Battery },
        { ITEM_ANTIMATTER_CELL,        "cell_antimatter", R::Epic,      R::Legendary,  2, {}, LootSource::Chest | LootSource::BlackMarket,                                                                                T::Ancient, 5, C::Battery },
        { ITEM_BULWARK_CELL,           "cell_bulwark",    R::Legendary, R::Legendary,  1, {}, LootSource::Chest | LootSource::BlackMarket,                                                                                T::Tech, 5, C::Battery },
        { ITEM_VOLATILE_CELL,          "cell_volatile",   R::Legendary, R::Legendary,  1, {}, LootSource::Chest | LootSource::BlackMarket,                                                                                T::Tech, 5, C::Battery },
        { ITEM_ADRENAL_CELL,           "cell_adrenal",    R::Legendary, R::Legendary,  1, {}, LootSource::Chest | LootSource::BlackMarket,                                                                                T::Tech, 5, C::Battery },

        // ----- Junk ------------------------------------------------------
        { ITEM_SCRAP_METAL,        "scrap_metal",        R::Common,    R::Common,    35, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::ScavMerchant, T::Scrap, 1, C::Junk },
        { ITEM_BROKEN_CIRCUIT,     "broken_circuit",     R::Common,    R::Common,    35, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::ScavMerchant, T::Scrap, 1, C::Junk },
        { ITEM_EMPTY_CASING,       "empty_casing",       R::Common,    R::Common,    30, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::ScavMerchant, T::Scrap, 1, C::Junk },

        // ----- Crafting materials ----------------------------------------
        { ITEM_NANO_FIBER,         "nano_fiber",         R::Common,    R::Uncommon,  30, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::ScavMerchant, T::Tech,     1, C::CraftingMaterial },
        { ITEM_POWER_CORE,         "power_core",         R::Uncommon,  R::Rare,      25, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::ScavMerchant, T::Tech,     2, C::CraftingMaterial },
        { ITEM_CIRCUIT_BOARD,      "circuit_board",      R::Common,    R::Uncommon,  25, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::ScavMerchant, T::Tech,     1, C::CraftingMaterial },
        { ITEM_ALLOY_INGOT,        "alloy_ingot",        R::Uncommon,  R::Rare,      20, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::ScavMerchant, T::Civilian, 2, C::CraftingMaterial },
        { ITEM_SPARE_PARTS,        "spare_parts",        R::Common,    R::Uncommon,  30, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::ScavMerchant,        T::Scrap,    1, C::CraftingMaterial },
        { ITEM_CIRCUITRY,          "circuitry",          R::Common,    R::Uncommon,  20, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::ScavMerchant,        T::Scrap,    1, C::CraftingMaterial },

        // ----- Ship components -------------------------------------------
        { ITEM_HULL_PLATE,         "hull_plate",         R::Common,    R::Rare,      35, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms | LootSource::MaintenanceTunnel, T::Tech, 1, C::ShipComponent },
        { ITEM_SHIELD_GENERATOR,   "shield_generator",   R::Uncommon,  R::Epic,      25, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms | LootSource::MaintenanceTunnel, T::Tech, 2, C::ShipComponent },
        { ITEM_NAVI_COMPUTER_MK2,  "navi_computer_mk2",  R::Rare,      R::Epic,      15, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::MaintenanceTunnel,                            T::Tech, 3, C::ShipComponent },
        // Note: ITEM_ENGINE_COIL_MK1 deliberately omitted — placed by hand
        // at game_world.cpp:401, never rolled. See spec non-goals.

        // ----- Energy mods -----------------------------------------------
        { ITEM_SOLAR_PANEL_COMMON,   "solar_panel",          R::Common,    R::Common,    30, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms, T::Tech, 1, C::EnergyMod },
        { ITEM_SOLAR_PANEL_UNCOMMON, "solar_panel_uncommon", R::Uncommon,  R::Uncommon,  20, {}, LootSource::Chest | LootSource::MerchantArms,                              T::Tech, 2, C::EnergyMod },
        { ITEM_SOLAR_PANEL_RARE,     "solar_panel_rare",     R::Rare,      R::Rare,      10, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,    T::Tech, 4, C::EnergyMod },
        { ITEM_CAPACITOR_COIL,       "capacitor_coil",       R::Uncommon,  R::Uncommon,  20, {}, LootSource::Chest | LootSource::MerchantArms,                              T::Tech, 2, C::EnergyMod },
        { ITEM_CHARGE_CATALYST,      "charge_catalyst",      R::Uncommon,  R::Uncommon,  20, {}, LootSource::Chest | LootSource::MerchantArms,                              T::Tech, 2, C::EnergyMod },
        { ITEM_POLISHED_CONDUIT,     "polished_conduit",     R::Rare,      R::Rare,      10, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,    T::Tech, 4, C::EnergyMod },
        { ITEM_REINFORCED_CASING,    "reinforced_casing",    R::Common,    R::Common,    30, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::ScavMerchant,   T::Scrap, 1, C::EnergyMod },
        { ITEM_RECEPTOR_PLATE,       "receptor_plate",       R::Common,    R::Common,    30, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::ScavMerchant,   T::Scrap, 1, C::EnergyMod },
        { ITEM_BRASS_CONDUIT,        "brass_conduit",        R::Common,    R::Common,    30, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::ScavMerchant,   T::Scrap, 1, C::EnergyMod },
        { ITEM_POWER_JUNCTION,       "power_junction",       R::Uncommon,  R::Uncommon,  15, {}, LootSource::Chest | LootSource::MerchantArms,                              T::Tech, 2, C::EnergyMod },
        { ITEM_TUNED_CATALYST,       "tuned_catalyst",       R::Rare,      R::Rare,       8, {}, LootSource::Chest | LootSource::MerchantArms | LootSource::BlackMarket,    T::Tech, 4, C::EnergyMod },

        // ----- Cookbooks (merchant-only, themed by Civilian) -------------
        { ITEM_COOKBOOK_HEARTY_STEW,  "cookbook_hearty_stew",  R::Common,    R::Common,    1, {}, LootSource::MerchantFood, T::Civilian, 1, C::Cookbook },
        { ITEM_COOKBOOK_PROTEIN_BAKE, "cookbook_protein_bake", R::Uncommon,  R::Uncommon,  1, {}, LootSource::MerchantFood, T::Civilian, 2, C::Cookbook },
        { ITEM_COOKBOOK_HEROS_FEAST,  "cookbook_heros_feast",  R::Rare,      R::Rare,      1, {}, LootSource::MerchantFood, T::Civilian, 4, C::Cookbook },

        // Note: ingredients (raw_meat, carrot, flour, herbs, synth_protein),
        // cooked dishes, and burnt_slop are deliberately NOT in the loot
        // table — they're placed by the cooking system, not by drops.
        // Same for synthesized items (1000+) — those are tinkering output.
    };
    return data;
}
```

- [ ] **Step 2.4: Define per-source category-weight tables**

Append to the anonymous namespace in `src/loot_table.cpp`:

```cpp
const std::map<LootSource, std::map<Category, int>>& s_category_weights() {
    using C = Category;
    static const std::map<LootSource, std::map<C, int>> data = {
        { LootSource::NpcDrop, {
            { C::Weapon,           30 },
            { C::Armor,            25 },
            { C::Consumable,       15 },
            { C::Battery,           5 },
            { C::Junk,             15 },
            { C::CraftingMaterial, 10 },
        }},
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
        }},
        { LootSource::MaintenanceTunnel, {
            { C::ShipComponent,    40 },
            { C::CraftingMaterial, 30 },
            { C::Junk,             30 },
        }},
        // Merchants drive their own selection through manifests; no entry here.
    };
    return data;
}
```

- [ ] **Step 2.5: Implement `roll_loot`**

Append to `src/loot_table.cpp`, *outside* the anonymous namespace (so it's part of `astra::`):

```cpp
std::optional<Item> roll_loot(LootSource source,
                              int level,
                              std::mt19937& rng,
                              std::optional<Category> forced_category) {
    Rarity rarity = roll_rarity(rng);

    // Step 2: pick category.
    std::optional<Category> picked_category = forced_category;
    if (!picked_category.has_value()) {
        const auto& cw = s_category_weights();
        auto it = cw.find(source);
        if (it != cw.end() && !it->second.empty()) {
            int total = 0;
            for (auto& kv : it->second) total += kv.second;
            if (total > 0) {
                int roll = std::uniform_int_distribution<int>(0, total - 1)(rng);
                int acc  = 0;
                for (auto& kv : it->second) {
                    acc += kv.second;
                    if (roll < acc) {
                        picked_category = kv.first;
                        break;
                    }
                }
            }
        }
    }

    // Step 3 + 4: filter and weighted-pick.
    const auto& table = s_loot_table_data();
    int total_weight = 0;
    std::vector<const LootEntry*> eligible;
    eligible.reserve(table.size());

    uint64_t source_bit = loot_source_bit(source);
    for (const auto& entry : table) {
        if ((entry.source_mask & source_bit) == 0)            continue;
        if (rarity < entry.min_rarity || rarity > entry.max_rarity) continue;
        if (level < entry.min_level)                          continue;
        if (picked_category.has_value() && entry.category != *picked_category) continue;

        int w = entry.default_weight;
        auto sw = entry.source_weights.find(source);
        if (sw != entry.source_weights.end()) w = sw->second;
        if (w <= 0) continue;

        eligible.push_back(&entry);
        total_weight += w;
    }

    if (eligible.empty() || total_weight <= 0) {
        return std::nullopt;
    }

    int roll = std::uniform_int_distribution<int>(0, total_weight - 1)(rng);
    int acc  = 0;
    const LootEntry* chosen = eligible.back();
    for (const auto* e : eligible) {
        int w = e->default_weight;
        auto sw = e->source_weights.find(source);
        if (sw != e->source_weights.end()) w = sw->second;
        acc += w;
        if (roll < acc) { chosen = e; break; }
    }

    // Steps 5-8: build, scale, scale, affixes.
    Item item = build_by_def_id(chosen->item_def_id);
    scale_item_to_rarity(item, rarity);
    scale_item_to_level(item, level);
    apply_rarity_affixes(item, rarity, rng);
    return item;
}
```

- [ ] **Step 2.6: Implement `assemble_stock`**

Append to `src/loot_table.cpp`:

```cpp
std::vector<Item> assemble_stock(const std::vector<StockManifestEntry>& manifest,
                                 LootSource source,
                                 int faction_rep,
                                 int level,
                                 std::mt19937& rng) {
    std::vector<Item> stock;
    stock.reserve(manifest.size() * 2);

    for (const auto& entry : manifest) {
        if (faction_rep < entry.min_reputation) continue;

        for (int i = 0; i < entry.quantity; ++i) {
            if (entry.mode == StockManifestEntry::Always) {
                Item item = build_by_def_id(entry.item_def_id);
                if (item.item_def_id != 0) {
                    stock.push_back(std::move(item));
                }
            } else { // Random
                auto rolled = roll_loot(source, level, rng, entry.category);
                if (rolled.has_value()) {
                    stock.push_back(std::move(*rolled));
                }
            }
        }
    }

    return stock;
}
```

- [ ] **Step 2.7: Implement `find_entry_by_identifier`, `loot_table_all_entries`, and `verify_dispatch_coverage`**

Append to `src/loot_table.cpp`:

```cpp
const LootEntry* find_entry_by_identifier(std::string_view identifier) {
    auto lower = [](char c) -> char {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    };
    auto ieq = [&](std::string_view a, std::string_view b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (lower(a[i]) != lower(b[i])) return false;
        }
        return true;
    };

    for (const auto& entry : s_loot_table_data()) {
        if (ieq(entry.identifier, identifier)) return &entry;
    }
    return nullptr;
}

const std::vector<LootEntry>& loot_table_all_entries() {
    return s_loot_table_data();
}

bool verify_dispatch_coverage() {
    bool ok = true;
    std::unordered_set<uint16_t> seen;
    for (const auto& entry : s_loot_table_data()) {
        if (entry.item_def_id == 0) {
            std::fprintf(stderr,
                "[loot_table] entry '%s' has item_def_id=0\n",
                entry.identifier.c_str());
            ok = false;
            continue;
        }
        if (!seen.insert(entry.item_def_id).second) {
            std::fprintf(stderr,
                "[loot_table] duplicate item_def_id %u (identifier '%s')\n",
                entry.item_def_id, entry.identifier.c_str());
            ok = false;
        }
        Item probe = build_by_def_id(entry.item_def_id);
        if (probe.item_def_id != entry.item_def_id) {
            std::fprintf(stderr,
                "[loot_table] build_by_def_id(%u) for '%s' returned def_id=%u "
                "(probable missing dispatch arm)\n",
                entry.item_def_id, entry.identifier.c_str(), probe.item_def_id);
            ok = false;
        }
    }
    return ok;
}
```

- [ ] **Step 2.8: Wire `verify_dispatch_coverage()` into DEV-build startup**

Open `src/main.cpp`. Find the early startup code (after argument parsing, before the game loop). Add this in a `#ifdef ASTRA_DEV_MODE` block:

```cpp
#ifdef ASTRA_DEV_MODE
    if (!astra::verify_dispatch_coverage()) {
        std::fprintf(stderr,
            "[FATAL] loot table has entries without build_by_def_id dispatch.\n"
            "        See errors above. Exiting.\n");
        return 1;
    }
#endif
```

If `main.cpp` doesn't currently include `<cstdio>` or `astra/loot_table.h`, add them at the top.

Locate the right insertion point: somewhere after `#include` lines and the parsing of `--sdl`/`--term`, but before the `Game` is constructed or any window opened. A safe spot is right after argument parsing finishes and before renderer construction.

- [ ] **Step 2.9: Build to verify everything compiles**

Run: `cmake --build build`
Expected: clean build. Possible complaints:
- Unused variable warnings in `s_loot_table_data` initializer if you accidentally double-listed an entry — fix.
- Missing `#include <cmath>` etc. — add to top of `loot_table.cpp`.

- [ ] **Step 2.10: Smoke-test the dispatch coverage check**

Run: `./build/astra-dev` (just open the title screen, then quit)
Expected: no `[loot_table]` errors on stderr. If you see one, the table references a `def_id` that isn't dispatched in `build_by_def_id` — go back to Task 1.6 and add the missing case.

- [ ] **Step 2.11: Commit**

```bash
git add src/loot_table.cpp src/main.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(loot): populate loot table + roll_loot/assemble_stock

Adds src/loot_table.cpp with:
- s_loot_table: one LootEntry per existing item, tagged with
  source mask, rarity range, category, theme, weight, min level
- s_category_weights: per-source category mix (NpcDrop, Chest,
  MaintenanceTunnel)
- roll_loot(): three-stage pipeline (rarity, category, item) with
  filter-then-roll semantics. Optional forced_category for manifest
  Random entries.
- assemble_stock(): walks a StockManifestEntry list with Always +
  Random modes, rep-gated.
- find_entry_by_identifier(): case-insensitive name lookup
- verify_dispatch_coverage(): startup check that every table entry
  has a working build_by_def_id arm. Wired into main.cpp under
  ASTRA_DEV_MODE.

Table is wired to no callers yet — commits 3-5 swap call sites.

Spec: docs/superpowers/specs/2026-04-25-loot-table-design.md
EOF
)"
```

---

## Task 3: Switch loot drops in combat to `roll_loot`

**Files:**
- Modify: `src/game_combat.cpp` (replace `generate_loot_drop` calls at lines ~107, 652, 873)
- Modify: `src/item_gen.cpp` (delete obsolete picker functions)
- Modify: `include/astra/item_gen.h` (delete obsolete declarations)
- Modify: `src/item_defs.cpp` (delete `random_*` picker functions)
- Modify: `include/astra/item_defs.h` (delete `random_*` declarations)

After this task, the loot drops on NPC kill come from the table.

- [ ] **Step 3.1: Replace `generate_loot_drop` calls in `src/game_combat.cpp`**

There are three drop sites in `src/game_combat.cpp`:
- `:107` — Spare Parts salvage (5% chance) — this stays as-is (hardcoded `build_spare_parts()` call, not a table query).
- `:652` — main loot drop on NPC death (50% gate)
- `:873` — alternative drop path (player ranged kill)

For each of `:652` and `:873`:

Find the existing block that looks like:
```cpp
if (rng_chance(rng, 50)) {
    Item drop = generate_loot_drop(rng, npc.level);
    ground_items.push_back({npc.x, npc.y, std::move(drop)});
}
```

Replace with:
```cpp
if (rng_chance(rng, 50)) {
    auto drop = roll_loot(LootSource::NpcDrop, npc.level, rng);
    if (drop.has_value()) {
        ground_items.push_back({npc.x, npc.y, std::move(*drop)});
    }
}
```

(The exact surrounding code may vary — preserve the surrounding gate, position calculation, and ground-items insertion. The change is *only* the inner two lines: `generate_loot_drop` → `roll_loot` + `std::optional` unwrap.)

Add the include at the top of `src/game_combat.cpp` if not already present:
```cpp
#include "astra/loot_table.h"
```

- [ ] **Step 3.2: Build to verify the swap compiles**

Run: `cmake --build build`
Expected: clean build. The old `generate_loot_drop` is still present in `item_gen.cpp` — its only callers are now removed from `game_combat.cpp`, but it's not yet deleted, so the symbol still exists.

- [ ] **Step 3.3: Smoke test — kill an NPC and verify a drop**

Run: `./build/astra-dev`
1. Open dev console (backtick).
2. `give xp 1000` → level up
3. Find any hostile NPC (or use `spawn` if available).
4. Kill it.
5. Walk over the drop, press `,` to pick up. Verify the item is sensible (not "Item{}").

Repeat 5-10 times. You should see varied drops across categories. If `roll_loot` returns `nullopt` consistently, check that `LootSource::NpcDrop` entries actually exist in the table.

- [ ] **Step 3.4: Delete obsolete pickers from `src/item_defs.cpp`**

Delete these functions entirely:
- `random_ranged_weapon` (~line 1075)
- `random_melee_weapon` (~line 1087)
- `random_armor` (~line 1097)
- `random_junk` (~line 1110)

Leave `random_shield` (~line 763) **alone for now** — merchants still call it. It dies in Task 4 cleanup.

- [ ] **Step 3.5: Delete obsolete pickers from `src/item_gen.cpp`**

Delete these functions entirely:
- `generate_random_weapon` (~line 158)
- `generate_random_armor` (~line 170)
- `generate_loot_drop` (~line 177)

Leave `roll_rarity`, `apply_affix`, `apply_rarity_affixes`, `scale_item_to_level`, `scale_item_to_rarity` — those are still used.

- [ ] **Step 3.6: Delete the obsolete declarations from `include/astra/item_gen.h`**

Find and remove the declarations corresponding to the deleted definitions: `generate_random_weapon`, `generate_random_armor`, `generate_loot_drop`.

- [ ] **Step 3.7: Delete the obsolete declarations from `include/astra/item_defs.h`**

Find and remove the declarations corresponding to the deleted definitions: `random_ranged_weapon`, `random_melee_weapon`, `random_armor`, `random_junk`. Leave `random_shield`.

- [ ] **Step 3.8: Build to confirm nothing else referenced the deleted symbols**

Run: `cmake --build build`
Expected: clean build. If the linker complains about an undefined reference to one of the deleted functions, you've missed a caller — `grep -rn "<symbol>" src/` to find it and convert it to `roll_loot`.

- [ ] **Step 3.9: Smoke-test loot drops still work after the deletions**

Run: `./build/astra-dev`. Repeat the NPC-kill smoke test from 3.3 to confirm nothing regressed.

- [ ] **Step 3.10: Commit**

```bash
git add src/game_combat.cpp src/item_defs.cpp src/item_gen.cpp \
        include/astra/item_defs.h include/astra/item_gen.h
git commit -m "$(cat <<'EOF'
feat(loot): combat drops use the loot table

NPC-kill drops at game_combat.cpp now invoke roll_loot(NpcDrop, ...)
instead of the old generate_loot_drop pipeline. Deletes:
- item_gen: generate_loot_drop, generate_random_weapon, generate_random_armor
- item_defs: random_ranged_weapon, random_melee_weapon, random_armor,
  random_junk

random_shield stays (still called by merchants until Task 4).

Spec: docs/superpowers/specs/2026-04-25-loot-table-design.md
EOF
)"
```

---

## Task 4: Switch merchants to manifests + `assemble_stock`

**Files:**
- Modify: `src/npcs/merchant.cpp` (~line 56 — `generate_merchant_stock` call site)
- Modify: `src/npcs/hub_npcs.cpp` (~lines 45 + 228 — Food + Arms dealer call sites)
- Modify: `src/npcs/scav_merchant.cpp` (~line 48)
- Modify: `src/npcs/black_market_vendor.cpp` (~line 54)
- Modify: `src/item_defs.cpp` — delete the three stock generators + `random_shield`
- Modify: `include/astra/item_defs.h` — delete corresponding declarations

After this task, every merchant pulls stock from the table via a declarative manifest.

- [ ] **Step 4.1: Add manifest + replacement call in `src/npcs/merchant.cpp`**

Find the existing call to `generate_merchant_stock(rng)` (around line 56). Above it, declare a static manifest matching the current behavior (see `src/item_defs.cpp:1128-1153`):

```cpp
static const std::vector<StockManifestEntry> s_general_merchant_manifest = {
    // Always-stocked basics
    { StockManifestEntry::Always, ITEM_SMALL_ENERGY_CELL,    Category::Battery,       2 },
    { StockManifestEntry::Always, ITEM_STANDARD_ENERGY_CELL, Category::Battery,       1 },
    { StockManifestEntry::Always, ITEM_RATION_PACK,          Category::Consumable,    5 },
    { StockManifestEntry::Always, ITEM_COMBAT_STIM,          Category::Consumable,    2 },
    { StockManifestEntry::Always, ITEM_FRAG_GRENADE,         Category::Consumable,    3 },
    { StockManifestEntry::Always, ITEM_NIGHT_GOGGLES,        Category::Accessory,     1 },
    { StockManifestEntry::Always, ITEM_HULL_PLATE,           Category::ShipComponent, 1 },
    { StockManifestEntry::Always, ITEM_SHIELD_GENERATOR,     Category::ShipComponent, 1 },
    { StockManifestEntry::Always, ITEM_SOLAR_PANEL_COMMON,   Category::EnergyMod,     1 },

    // Random rotating stock
    { StockManifestEntry::Random, 0, Category::Weapon, 1 },
    { StockManifestEntry::Random, 0, Category::Armor,  1 },
    { StockManifestEntry::Random, 0, Category::Shield, 1 },

    // Liked tier (rep ≥ 10): more weapons + stims
    { StockManifestEntry::Random, 0, Category::Weapon,     1, /*min_rep=*/10 },
    { StockManifestEntry::Always, ITEM_COMBAT_STIM, Category::Consumable, 3, /*min_rep=*/10 },

    // Trusted tier (rep ≥ 50): more armor
    { StockManifestEntry::Random, 0, Category::Armor, 1, /*min_rep=*/50 },
};
```

Add includes at the top of `src/npcs/merchant.cpp` if missing:
```cpp
#include "astra/loot_table.h"
#include "astra/item_ids.h"
```

Replace the existing line `merchant.shop.inventory = generate_merchant_stock(rng);` (or whatever exact form it takes — check the current file) with:

```cpp
merchant.shop.inventory = assemble_stock(
    s_general_merchant_manifest,
    LootSource::MerchantGeneral,
    /*faction_rep=*/get_faction_rep_for_merchant(merchant),  // use whatever
                                                              // accessor exists
    /*level=*/1,                                              // or scale per
                                                              // station/system
    rng);
```

Read the surrounding code first to confirm the exact accessor for faction reputation and the right `level` to pass — these may differ per merchant. If the original generator did not accept reputation (it was hardcoded to a starting value), pass `0` for now; rep-gated entries will simply be skipped.

- [ ] **Step 4.2: Add manifests + swap calls in `src/npcs/hub_npcs.cpp`**

This file has two merchants — Food Merchant (~line 45) and Arms Dealer (~line 228). Add two manifests before each call.

**Food Merchant manifest** (mirrors `generate_food_merchant_stock`, item_defs.cpp:1193-1224):

```cpp
static const std::vector<StockManifestEntry> s_food_merchant_manifest = {
    { StockManifestEntry::Always, ITEM_RATION_PACK,        Category::Consumable, 10 },
    { StockManifestEntry::Always, ITEM_COMBAT_STIM,        Category::Consumable,  3 },
    { StockManifestEntry::Always, ITEM_RAW_MEAT,           Category::Ingredient,  5 },
    { StockManifestEntry::Always, ITEM_CARROT,             Category::Ingredient,  5 },
    { StockManifestEntry::Always, ITEM_FLOUR,              Category::Ingredient,  5 },
    { StockManifestEntry::Always, ITEM_SYNTH_PROTEIN,      Category::Ingredient,  5 },
    { StockManifestEntry::Always, ITEM_HERBS,              Category::Ingredient,  3 },
    { StockManifestEntry::Always, ITEM_COOKBOOK_HEARTY_STEW, Category::Cookbook,  1 },

    { StockManifestEntry::Always, ITEM_RATION_PACK,        Category::Consumable,  5, /*min_rep=*/10 },
    { StockManifestEntry::Always, ITEM_COMBAT_STIM,        Category::Consumable,  2, /*min_rep=*/10 },
    { StockManifestEntry::Always, ITEM_COOKBOOK_PROTEIN_BAKE, Category::Cookbook, 1, /*min_rep=*/60 },
    { StockManifestEntry::Always, ITEM_COOKBOOK_HEROS_FEAST,  Category::Cookbook, 1, /*min_rep=*/300 },
};
```

**Arms Dealer manifest** (mirrors `generate_arms_dealer_stock`, item_defs.cpp:1155-1192):

```cpp
static const std::vector<StockManifestEntry> s_arms_dealer_manifest = {
    { StockManifestEntry::Random, 0, Category::Weapon,        2 },  // ranged
    { StockManifestEntry::Random, 0, Category::Weapon,        1 },  // melee (random per category)
    { StockManifestEntry::Always, ITEM_SMALL_ENERGY_CELL,    Category::Battery,    2 },
    { StockManifestEntry::Always, ITEM_STANDARD_ENERGY_CELL, Category::Battery,    1 },
    { StockManifestEntry::Random, 0, Category::Armor,         1 },
    { StockManifestEntry::Random, 0, Category::Shield,        1 },
    { StockManifestEntry::Always, ITEM_EMP_GRENADE,           Category::Consumable, 2 },
    { StockManifestEntry::Always, ITEM_SOLAR_PANEL_COMMON,    Category::EnergyMod,  1 },
    { StockManifestEntry::Always, ITEM_CAPACITOR_COIL,        Category::EnergyMod,  1 },
    { StockManifestEntry::Always, ITEM_REINFORCED_CASING,     Category::EnergyMod,  1 },
    { StockManifestEntry::Always, ITEM_RECEPTOR_PLATE,        Category::EnergyMod,  1 },
    { StockManifestEntry::Always, ITEM_BRASS_CONDUIT,         Category::EnergyMod,  1 },

    // Liked tier
    { StockManifestEntry::Random, 0, Category::Weapon,        1, /*min_rep=*/10 },
    { StockManifestEntry::Always, ITEM_EMP_GRENADE,           Category::Consumable, 2, 10 },
    { StockManifestEntry::Always, ITEM_SOLAR_PANEL_UNCOMMON,  Category::EnergyMod,  1, 10 },
    { StockManifestEntry::Always, ITEM_CHARGE_CATALYST,       Category::EnergyMod,  1, 10 },
    { StockManifestEntry::Always, ITEM_POWER_JUNCTION,        Category::EnergyMod,  1, 10 },

    // Trusted tier
    { StockManifestEntry::Random, 0, Category::Weapon,        1, /*min_rep=*/50 },
    { StockManifestEntry::Random, 0, Category::Armor,         1, 50 },
    { StockManifestEntry::Always, ITEM_SOLAR_PANEL_RARE,      Category::EnergyMod,  1, 50 },
    { StockManifestEntry::Always, ITEM_POLISHED_CONDUIT,      Category::EnergyMod,  1, 50 },
    { StockManifestEntry::Always, ITEM_TUNED_CATALYST,        Category::EnergyMod,  1, 50 },
};
```

Replace each `generate_food_merchant_stock(rng)` call with:
```cpp
assemble_stock(s_food_merchant_manifest, LootSource::MerchantFood,
               /*faction_rep=*/<rep_accessor>, /*level=*/1, rng)
```

And each `generate_arms_dealer_stock(rng)` call with:
```cpp
assemble_stock(s_arms_dealer_manifest, LootSource::MerchantArms,
               /*faction_rep=*/<rep_accessor>, /*level=*/1, rng)
```

- [ ] **Step 4.3: Add manifest + swap call in `src/npcs/scav_merchant.cpp`**

Read the existing `generate_merchant_stock` call at ~line 48 to see what's currently used (likely the same general-merchant generator). Either:
(a) reuse `s_general_merchant_manifest` from `merchant.cpp` (would require exposing it via a header), or
(b) declare a `s_scav_merchant_manifest` local to this file with scav-flavor adjustments.

Pick (b) — scav merchants are thematically distinct (more junk, more crafting, less premium gear):

```cpp
static const std::vector<StockManifestEntry> s_scav_merchant_manifest = {
    { StockManifestEntry::Always, ITEM_SMALL_ENERGY_CELL, Category::Battery,          2 },
    { StockManifestEntry::Always, ITEM_RATION_PACK,       Category::Consumable,       3 },
    { StockManifestEntry::Always, ITEM_COMBAT_STIM,       Category::Consumable,       1 },
    { StockManifestEntry::Always, ITEM_SCRAP_METAL,       Category::Junk,             5 },
    { StockManifestEntry::Always, ITEM_BROKEN_CIRCUIT,    Category::Junk,             3 },

    { StockManifestEntry::Random, 0, Category::CraftingMaterial, 3 },
    { StockManifestEntry::Random, 0, Category::EnergyMod,        2 },
    { StockManifestEntry::Random, 0, Category::Weapon,           1 },
    { StockManifestEntry::Random, 0, Category::Armor,            1 },
    { StockManifestEntry::Random, 0, Category::Accessory,        1 },
};
```

Replace the current generator call with `assemble_stock(s_scav_merchant_manifest, LootSource::ScavMerchant, ..., 1, rng)`.

- [ ] **Step 4.4: Add manifest + swap call in `src/npcs/black_market_vendor.cpp`**

Read the existing call at ~line 54 to see what's currently used. Add:

```cpp
static const std::vector<StockManifestEntry> s_black_market_manifest = {
    { StockManifestEntry::Random, 0, Category::Weapon,    2 },
    { StockManifestEntry::Random, 0, Category::Armor,     1 },
    { StockManifestEntry::Random, 0, Category::Shield,    1 },
    { StockManifestEntry::Random, 0, Category::Battery,   2 },
    { StockManifestEntry::Random, 0, Category::EnergyMod, 2 },
    { StockManifestEntry::Random, 0, Category::Accessory, 1 },
    { StockManifestEntry::Always, ITEM_CRYO_GRENADE,      Category::Consumable, 2 },
    { StockManifestEntry::Always, ITEM_EMP_GRENADE,       Category::Consumable, 2 },
};
```

Replace the existing generator call with `assemble_stock(s_black_market_manifest, LootSource::BlackMarket, /*faction_rep=*/<accessor>, /*level=*/1, rng)`.

- [ ] **Step 4.5: Build to confirm everything still compiles before deleting old generators**

Run: `cmake --build build`
Expected: clean build. The three stock generators in `item_defs.cpp` are now unreferenced but still compile.

- [ ] **Step 4.6: Smoke-test merchant stocks**

Run: `./build/astra-dev`
1. Spawn into the world. Travel to The Heavens Above.
2. Talk to a merchant (general). Verify stock includes batteries, rations, ration packs.
3. Talk to the Food Merchant. Verify ration packs, ingredients, cookbook present.
4. Talk to the Arms Dealer. Verify weapons, armor, energy mods.
5. Use dev console `give rep <faction> 50` then re-talk to merchants — verify rep-gated entries appear.

If a merchant has empty inventory, check that the manifest's `Random` entries actually have eligible items in the table for that source mask. (E.g., ScavMerchant entries need `LootSource::ScavMerchant` in their `source_mask`.)

- [ ] **Step 4.7: Delete the three stock generators + `random_shield`**

In `src/item_defs.cpp`, delete:
- `generate_merchant_stock` (~line 1128)
- `generate_arms_dealer_stock` (~line 1155)
- `generate_food_merchant_stock` (~line 1193)
- `random_shield` (~line 763) — confirm via `grep -rn random_shield src/` first that nothing else uses it. If something does, leave it.

In `include/astra/item_defs.h`, remove the corresponding declarations.

- [ ] **Step 4.8: Build to confirm no stragglers reference the deleted generators**

Run: `cmake --build build`
Expected: clean build. If linker errors mention any of the deleted symbols, grep and convert the caller.

- [ ] **Step 4.9: Smoke-test merchants again post-deletion**

Run: `./build/astra-dev` and re-do step 4.6 to verify nothing regressed.

- [ ] **Step 4.10: Commit**

```bash
git add src/npcs/merchant.cpp src/npcs/hub_npcs.cpp \
        src/npcs/scav_merchant.cpp src/npcs/black_market_vendor.cpp \
        src/item_defs.cpp include/astra/item_defs.h
git commit -m "$(cat <<'EOF'
feat(loot): merchants pull from manifest + assemble_stock

Each merchant now has a static StockManifestEntry list declaring
its stock with Always (deterministic) and Random (table-rolled)
entries. Reputation gating moves from if-chains into per-entry
min_reputation fields. Deletes the three legacy stock generators
and random_shield from item_defs.cpp.

Spec: docs/superpowers/specs/2026-04-25-loot-table-design.md
EOF
)"
```

---

## Task 5: Refactor `give item` dev command

**Files:**
- Modify: `src/dev_console.cpp` (~lines 137-145 help text, ~lines 420+ `give item` handler)

After this task, the dev console drives item creation through the loot table — list mode, identifier lookup, optional rarity and level args.

- [ ] **Step 5.1: Add helper to parse rarity from a string**

Near the top of `src/dev_console.cpp` (after includes, before any function), add:

```cpp
static std::optional<Rarity> parse_rarity_arg(std::string_view s) {
    auto eq_ci = [](std::string_view a, std::string_view b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
            char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
            if (ca != cb) return false;
        }
        return true;
    };
    if (eq_ci(s, "c") || eq_ci(s, "common"))    return Rarity::Common;
    if (eq_ci(s, "u") || eq_ci(s, "uncommon"))  return Rarity::Uncommon;
    if (eq_ci(s, "r") || eq_ci(s, "rare"))      return Rarity::Rare;
    if (eq_ci(s, "e") || eq_ci(s, "epic"))      return Rarity::Epic;
    if (eq_ci(s, "l") || eq_ci(s, "legendary")) return Rarity::Legendary;
    return std::nullopt;
}

static std::string_view rarity_short_name(Rarity r) {
    switch (r) {
        case Rarity::Common:    return "common";
        case Rarity::Uncommon:  return "uncommon";
        case Rarity::Rare:      return "rare";
        case Rarity::Epic:      return "epic";
        case Rarity::Legendary: return "legendary";
    }
    return "?";
}
```

Add `#include <cctype>` and `#include "astra/loot_table.h"` if not already present.

- [ ] **Step 5.2: Replace the `give item` if-else chain**

Find the existing block at `src/dev_console.cpp:420+` that begins with `else if (verb == "give" && args.size() >= 3 && args[1] == "item") {` and contains the long if/else chain mapping names to `build_*()` calls. Delete the entire block.

Insert in its place:

```cpp
else if (verb == "give" && args.size() >= 2 && args[1] == "item") {
    // Subcommand: list mode (no identifier given)
    if (args.size() == 2) {
        const auto& entries = loot_table_all_entries();

        // Group by category for readability.
        std::map<Category, std::vector<const LootEntry*>> by_cat;
        for (const auto& e : entries) by_cat[e.category].push_back(&e);

        log("Loot table — " + std::to_string(entries.size()) + " items");
        for (const auto& [cat, list] : by_cat) {
            log("  [" + std::string(category_name(cat)) + "]");
            for (const auto* e : list) {
                std::string line = "    " + e->identifier + "  ("
                                 + std::string(rarity_short_name(e->min_rarity))
                                 + ".."
                                 + std::string(rarity_short_name(e->max_rarity))
                                 + ")";
                log(line);
            }
        }
        return; // or whatever the existing handler termination idiom is
    }

    // Subcommand: spawn an item
    std::string_view identifier = args[2];
    Rarity rarity = Rarity::Common;
    int level = 1;

    if (args.size() >= 4) {
        auto parsed = parse_rarity_arg(args[3]);
        if (!parsed.has_value()) {
            log("give item: unknown rarity '" + std::string(args[3])
                + "' (expected c/u/r/e/l or full name)");
            return;
        }
        rarity = *parsed;
    }
    if (args.size() >= 5) {
        try {
            level = std::stoi(args[4]);
        } catch (...) {
            log("give item: level must be an integer");
            return;
        }
        if (level < 1) level = 1;
    }

    const LootEntry* entry = find_entry_by_identifier(identifier);
    if (entry == nullptr) {
        log("give item: unknown identifier '" + std::string(identifier)
            + "' (try `give item` with no args to list)");
        return;
    }
    if (rarity < entry->min_rarity || rarity > entry->max_rarity) {
        log("give item: '" + entry->identifier + "' rarity out of range ("
            + std::string(rarity_short_name(entry->min_rarity)) + ".."
            + std::string(rarity_short_name(entry->max_rarity)) + ")");
        return;
    }

    Item item = build_by_def_id(entry->item_def_id);
    scale_item_to_rarity(item, rarity);
    scale_item_to_level(item, level);
    // No affixes — dev command stays deterministic.

    player.inventory.push_back(std::move(item));
    log("Added " + entry->identifier + " ("
        + std::string(rarity_short_name(rarity)) + ", lvl "
        + std::to_string(level) + ") to inventory.");
}
```

(The exact `player.inventory` accessor and `log()` call may differ — read the current handler around line 420 and match its idioms.)

- [ ] **Step 5.3: Update the help text at `src/dev_console.cpp:137-145`**

Replace the existing line:
```cpp
log("  give item <name>   - drop item in inventory (cell_small, cell_standard,");
```
(plus any continuation lines listing every name) with:

```cpp
log("  give item                     - list all items (identifier, category, rarity range)");
log("  give item <identifier>        - spawn item at Common, level 1");
log("  give item <id> <rarity>       - spawn at given rarity, level 1");
log("  give item <id> <rarity> <lvl> - fully specified (rarity: c/u/r/e/l)");
```

- [ ] **Step 5.4: Update `give ship <component>` to use the table lookup**

Find the existing `give ship` handler at `src/dev_console.cpp:407+`. Replace its if-chain with a table lookup so ship components share the same identifier system:

```cpp
else if (verb == "give" && args.size() >= 3 && args[1] == "ship") {
    const LootEntry* entry = find_entry_by_identifier(args[2]);
    if (entry == nullptr || entry->category != Category::ShipComponent) {
        log("give ship: unknown component '" + std::string(args[2])
            + "' (try: hull_plate, shield_generator, navi_computer_mk2)");
        return;
    }
    Item item = build_by_def_id(entry->item_def_id);
    log("Added " + item.name + " to ship cargo.");
    player.ship.cargo.push_back(std::move(item));
}
```

Note: `engine_coil_mk1` is *not* in the loot table (intentional — it's a hardcoded one-off). To keep the dev command's existing ability to give an engine coil, add a special case before the table lookup:

```cpp
else if (verb == "give" && args.size() >= 3 && args[1] == "ship") {
    Item item;
    if (args[2] == "engine" || args[2] == "engine_coil_mk1") {
        item = build_engine_coil_mk1();
    } else {
        const LootEntry* entry = find_entry_by_identifier(args[2]);
        if (entry == nullptr || entry->category != Category::ShipComponent) {
            log("give ship: unknown component '" + std::string(args[2])
                + "'");
            return;
        }
        item = build_by_def_id(entry->item_def_id);
    }
    log("Added " + item.name + " to ship cargo.");
    player.ship.cargo.push_back(std::move(item));
}
```

- [ ] **Step 5.5: Build to verify the dev console refactor compiles**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 5.6: Smoke-test the new `give item` command**

Run: `./build/astra-dev`. Open the dev console (backtick) and try each form:

1. `give item` → should print a paginated list grouped by category
2. `give item plasma_pistol` → spawns Common Plasma Pistol in inventory
3. `give item plasma_pistol r` → spawns Rare Plasma Pistol
4. `give item plasma_pistol l 5` → tries Legendary; should print "rarity out of range" since Plasma Pistol's max is Rare
5. `give item void_lance l 10` → spawns Legendary Void Lance, level 10
6. `give item garbage` → "unknown identifier"
7. `give ship hull_plate` → adds Hull Plate to ship cargo
8. `give ship engine` → still works (hardcoded)

If any case crashes or misbehaves, fix and re-test.

- [ ] **Step 5.7: Commit**

```bash
git add src/dev_console.cpp
git commit -m "$(cat <<'EOF'
feat(loot): table-driven dev console give item

Replaces the ~85-entry if-else chain at dev_console.cpp with a
loot-table lookup. New forms:
  give item                     - list everything by category
  give item <identifier>        - spawn at Common, level 1
  give item <id> <rarity>       - rarity short (c/u/r/e/l) or full
  give item <id> <rarity> <lvl> - fully specified

Pipeline: build_by_def_id -> scale_item_to_rarity -> scale_item_to_level.
No affix roll (dev command stays deterministic). Validates rarity
against the entry's [min_rarity, max_rarity] range.

give ship now uses the same identifier lookup, with engine_coil_mk1
remaining hardcoded since it's not in the loot table.

Spec: docs/superpowers/specs/2026-04-25-loot-table-design.md
EOF
)"
```

---

## Task 6: Cleanup sweep — no parallel item-creation paths remain

**Files:**
- Modify: `include/astra/item.h` — remove dead `LootTable`/`LootEntry` structs at ~line 264
- Audit: every `.cpp` under `src/` (including `src/npcs/`, `src/quests/`, `src/generators/`)

After this task, the only sanctioned ways to create an `Item` are:
1. `build_by_def_id(def_id)` — called via `roll_loot()`, `assemble_stock()`, or the dev console.
2. The two hardcoded one-offs called out in non-goals: engine coil at `game_world.cpp:401`, quest items at `game_world.cpp:1113`.

- [ ] **Step 6.1: Delete the dead `LootTable` / `LootEntry` structs from `item.h`**

Open `include/astra/item.h`. Find the block at lines ~264-267 (the spec-referenced location):

```cpp
struct LootTable {
    uint8_t npc_role = 0;
    std::vector<LootEntry> entries;
};

struct LootEntry {
    uint32_t item_id = 0;
    float drop_chance = 0.0f;
    int min_qty = 1;
    int max_qty = 1;
};
```

Delete both structs entirely. They were never populated or queried; the new `astra::LootEntry` lives in `loot_table.h` and is unrelated.

If any code (probably none, since they were dead) references the old types, the compiler will tell us in the next step.

- [ ] **Step 6.2: Audit external `build_*()` callers**

Run: `grep -rn 'build_[a-z_]*()' src/ include/ | grep -v 'src/item_defs.cpp\|include/astra/item_defs.h\|src/loot_table.cpp\|src/dev_console.cpp'`

Expected callers that should remain (sanctioned hardcoded one-offs):
- `src/game_world.cpp:~401` — `build_engine_coil_mk1()`
- `src/game_world.cpp:~1113` — quest item builders (these are dynamically-built `Item{}` from quest metadata, not direct `build_*` calls — verify by reading)
- `src/dev_console.cpp` — `build_engine_coil_mk1()` only (from Task 5.4 special case)
- Test stubs (none in this repo)

For every other hit, evaluate:
- If it's procedural (e.g., spawning loot), convert to `roll_loot()` or `assemble_stock()`.
- If it's a deterministic placement (single instance, specific id), convert to `build_by_def_id(ITEM_X)`.

Common likely offenders to check carefully:
- `src/repair_bench.cpp` — may directly build items it produces (consumed nano fiber, etc.)
- `src/tinkering.cpp` — synthesis output (this is OK; it's explicitly building synthesized 1000+ items, which are not in the loot table)
- `src/game_combat.cpp:~107` — Spare Parts salvage drop (`build_spare_parts()` direct call) — this is a sanctioned hardcoded one-off; leave it OR convert to `build_by_def_id(ITEM_SPARE_PARTS)` for consistency. Pick consistency: convert it.

For each conversion, the diff is small: replace `build_xyz()` with `build_by_def_id(ITEM_XYZ)`. No behavior change.

- [ ] **Step 6.3: Audit `src/dev_console.cpp` for leftover `if (args[N] == "...") item = build_X()` patterns**

Run: `grep -n 'build_[a-z_]*()' src/dev_console.cpp`

After Task 5, the only `build_*` call left should be `build_engine_coil_mk1()` in the `give ship engine` special case. Anything else is leftover that should have been swept by Task 5 — convert to `build_by_def_id()` via the table lookup.

- [ ] **Step 6.4: Audit `src/npcs/` for direct item construction outside manifests**

Run: `grep -rn 'build_[a-z_]*()\|push_back(Item' src/npcs/`

After Task 4, no NPC file should construct items directly. If you find one, decide:
- Is it on-death loot? → use `roll_loot(LootSource::NpcDrop, ...)`.
- Is it part of a merchant inventory? → add to that merchant's manifest.
- Is it a unique flavor item the NPC always carries? → `build_by_def_id(ITEM_X)` with a comment explaining it's intentional.

- [ ] **Step 6.5: Build and confirm no parallel paths remain**

Run: `cmake --build build`
Expected: clean build. If anything broke, the linker will report missing symbols (you deleted something still referenced) — go back and fix.

- [ ] **Step 6.6: Final smoke test — run the game and exercise everything**

Run: `./build/astra-dev`. Verify:
1. Title screen → no `[loot_table]` startup errors
2. Spawn into world. Kill an NPC. Verify a drop appears.
3. Visit each merchant type (general, food, arms, scav, black market). Verify stock is sensible and rep-gated where expected (use `give rep <faction> 50` to test).
4. `give item` → prints full table list
5. `give item plasma_pistol r 3` → spawns a Rare lvl-3 Plasma Pistol with stats scaled.
6. Equip and use weapons; check shop trade UI, repair bench. Nothing should regress.

If any regression appears, isolate via `git bisect` against the six commits.

- [ ] **Step 6.7: Commit**

```bash
git add include/astra/item.h src/game_combat.cpp \
        $(git diff --name-only HEAD)  # any audit conversions
git commit -m "$(cat <<'EOF'
refactor(loot): cleanup — single item-creation path

Removes the dead LootTable/LootEntry structs from item.h (they were
never populated; the new types live in loot_table.h).

Audits build_*() call sites across src/ and converts any remaining
external direct callers to build_by_def_id(). After this commit, the
only sanctioned ways to create an Item are:
- build_by_def_id() via roll_loot / assemble_stock / dev console
- Two hardcoded one-offs: engine coil placement (game_world.cpp:401)
  and quest-item placement (game_world.cpp:1113)

Pre-merge cleanup of docs/formulas.md → docs/mechanics.md is a manual
step (see ~/.claude/.../memory/project_loot_table_premerge.md).

Spec: docs/superpowers/specs/2026-04-25-loot-table-design.md
EOF
)"
```

---

## Self-Review Checklist (run by writer, fix inline)

1. **Spec coverage:** every section in `docs/superpowers/specs/2026-04-25-loot-table-design.md` has a corresponding task above:
   - Three-roll model → Task 2 step 2.5
   - Data model → Task 1 steps 1.1, 1.3
   - Pipeline `roll_loot` → Task 2 step 2.5
   - Pipeline `assemble_stock` → Task 2 step 2.6
   - File layout → Task 1 + Task 2
   - `give` dev command refactor → Task 5
   - Six-commit migration → Tasks 1-6
   - Stays-untouched (game_combat.cpp:652 gate, engine coil, quest items, affix system) → covered by Task 3 (preserves gate), Task 6 (audit excludes them)
   - Open questions deferred → not implemented (correct — deferred)
   - Risks (build_by_def_id expansion test, merchant determinism, save compat) → Task 1.6 + Task 2 verify_dispatch_coverage covers risk #1; merchant determinism gets a smoke test in Task 4.6/4.9; save compat is unchanged (item_def_id is preserved).

2. **Placeholder scan:** searched for "TBD", "TODO", "implement later" — none in plan body. The string "TBD at impl time" appears in spec but not plan.

3. **Type consistency:** spot-checked types/names across tasks:
   - `LootEntry`, `LootSource`, `Theme`, `Category`, `StockManifestEntry`, `roll_loot`, `assemble_stock`, `find_entry_by_identifier`, `loot_table_all_entries`, `verify_dispatch_coverage`, `scale_item_to_rarity`, `parse_rarity_arg`, `rarity_short_name`, `category_name` — all spelled identically wherever used.
   - `LootSource` enum values — used identically across all tasks.
   - Manifest field order matches the struct declaration in Task 1.3.

4. **Bite-sized step granularity:** most steps fit in 2-5 minutes. The two heaviest are Step 1.6 (expand build_by_def_id to ~85 cases — straightforward repetitive work, ~10-15 min) and Step 2.3 (populate the loot table — ~15-20 min). Both unavoidably big because they're the "data" steps; they're given complete code.

5. **Merge gates:** Tasks 3, 4, 5 each end with a smoke test before commit, so any regression caught early.
