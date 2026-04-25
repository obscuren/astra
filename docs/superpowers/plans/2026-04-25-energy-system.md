# Energy System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the throwaway-cell reload model with a normalized energy system shared by ranged weapons, energy shields, energy cells, and future gadgets. Cells become persistent items with charge state, refillable via Solar Panels (tinkering enhancement) and the recharge actions.

**Architecture:** Three composable components on `Item` — `EnergyStore` (current/capacity), `EnergyConsumer` (energy_per_use), and `EnergyModifiers` (tinkering bonuses). Solar Panel slots into the existing `EnhancementSlot` mechanism. A new `EnergySystem` ticks installed panels each `advance_world()`. Recharge actions drain inventory cells (highest-charge first) into the equipped weapon (`r`) or shield (`b`), with manual cell pickers on `Shift-R` / `Shift-B`.

**Tech Stack:** C++20, CMake (with `-DDEV=ON`), the existing `Renderer` abstraction. No test framework exists in this repo — verification is build success + manual dev-mode scenarios + targeted greps.

**Note:** This repo has no unit-test framework. "Test" steps verify by (a) building cleanly with `-DDEV=ON`, (b) running scripted dev-mode scenarios, or (c) grepping for absent / present strings. The existing project memory says always build with `-DDEV=ON`.

**Spec:** `docs/superpowers/specs/2026-04-25-energy-system-design.md`

---

## File Structure

**New files:**
- `include/astra/energy.h` — `EnergyStore`, `EnergyConsumer`, `EnergyModifiers`, `SolarPanelData`, transfer/query helpers.
- `include/astra/energy_system.h` — `EnergySystem` class declaration.
- `src/energy_system.cpp` — tick loop walking player items, applying Solar Panel deposits when outdoors.

**Modified files:**
- `include/astra/item.h` — drop `RangedData::charge_*`; add `energy` / `consumer` optionals on `Item`; extend `EnhancementSlot` with `stat_bonus` (renamed), `energy_bonus`, `solar_panel`. Drop `Item::shield_capacity` / `shield_hp` (replaced by `energy`).
- `include/astra/tinkering.h` — `MaterialEffect` carries `EnergyModifiers` and a `solar_panel_template` field; add a `MaterialKind` discriminator.
- `include/astra/combat_system.h` — rename `reload_weapon` / `reload_shield` to `recharge_weapon` / `recharge_shield`.
- `include/astra/player.h` — drop `shield_hp`, `shield_max_hp` (now read from equipped shield item).
- `include/astra/world_manager.h` — add `is_outdoor()`.
- `include/astra/game.h` — add `EnergySystem energy_;` member, wire into `advance_world()`.
- `include/astra/save_file.h` — bump `SAVE_FILE_VERSION`.
- `src/item_defs.cpp` — five cell tiers, three Solar Panel tiers, ranged weapons populate `energy`/`consumer`, shields populate `energy`.
- `src/save_file.cpp` — schema bump, write/read new fields, reject older saves.
- `src/game.cpp`, `src/game_combat.cpp`, `src/game_input.cpp`, `src/game_rendering.cpp`, `src/game_interaction.cpp`, `src/character_screen.cpp`, `src/help_screen.cpp`, `src/tinkering.cpp`, `src/ui.cpp` — call-site updates and UI changes.
- `docs/formulas.md`, `docs/roadmap.md` — design fact-sheet updates.

---

## Task Sequencing

The plan stages changes so the build stays green between tasks. Tasks 1–7 introduce data-model fields without removing the old ones, so existing call sites keep compiling. Tasks 8–10 migrate combat reads to the new fields. Tasks 11–12 then drop the obsolete fields. Tasks 13+ build out Solar Panel, UI, save bump.

---

### Task 1: Add energy.h with foundational structs

**Files:**
- Create: `include/astra/energy.h`

- [ ] **Step 1: Create the new header**

```cpp
// include/astra/energy.h
#pragma once

#include <cstdint>

namespace astra {

// Anything that holds energy.
struct EnergyStore {
    int current = 0;
    int capacity = 0;
};

// Anything that spends energy on use.
struct EnergyConsumer {
    int energy_per_use = 1;
};

// Tinkering bonuses for energy items.
struct EnergyModifiers {
    int capacity_bonus = 0;        // +X to max
    int charge_rate_bonus = 0;     // +X% to incoming energy per tick
    int discharge_efficiency = 0;  // every N units transferred yields +1 free
};

// Per-slot Solar Panel state. Sits inside EnhancementSlot.
struct SolarPanelData {
    bool active = true;
    int energy_per_tick = 5;     // tier-based: 5 / 8 / 12
    int tick_interval = 2;       // game-ticks between deposits
    int accumulator = 0;         // ticks accrued since last deposit
};

inline bool is_full(const EnergyStore& s)  { return s.current >= s.capacity; }
inline bool is_empty(const EnergyStore& s) { return s.current <= 0; }
inline int  deficit(const EnergyStore& s)  { return s.capacity - s.current; }

// Transfer energy from src to dst. Drains up to `requested` units from src.
// `efficiency_bonus_per_n` from EnergyModifiers::discharge_efficiency on src
// adds +1 free to dst for every N units actually drained.
// Returns the units actually deposited into dst.
int transfer_energy(EnergyStore& src, EnergyStore& dst, int requested,
                    int efficiency_bonus_per_n = 0);

} // namespace astra
```

- [ ] **Step 2: Add the helper definition**

Create `src/energy.cpp`:

```cpp
// src/energy.cpp
#include "astra/energy.h"

#include <algorithm>

namespace astra {

int transfer_energy(EnergyStore& src, EnergyStore& dst, int requested,
                    int efficiency_bonus_per_n) {
    if (requested <= 0) return 0;
    int drain = std::min({requested, src.current, deficit(dst)});
    if (drain <= 0) return 0;
    src.current -= drain;
    int bonus = (efficiency_bonus_per_n > 0)
                  ? drain / efficiency_bonus_per_n
                  : 0;
    int deposited = std::min(drain + bonus, deficit(dst) + bonus);
    deposited = std::min(deposited, dst.capacity - dst.current);
    dst.current += deposited;
    return deposited;
}

} // namespace astra
```

- [ ] **Step 3: Register `src/energy.cpp` in CMake**

Open `CMakeLists.txt`, find the source list (search for `src/item_defs.cpp` or any other `.cpp` line), add `src/energy.cpp` next to it.

- [ ] **Step 4: Build**

Run: `cmake -B build -DDEV=ON && cmake --build build -j`
Expected: build succeeds.

- [ ] **Step 5: Commit**

```bash
git add include/astra/energy.h src/energy.cpp CMakeLists.txt
git commit -m "feat(energy): add EnergyStore/Consumer/Modifiers structs and transfer helper"
```

---

### Task 2: Extend EnhancementSlot — rename `bonus` to `stat_bonus`, add `energy_bonus` and `solar_panel`

**Files:**
- Modify: `include/astra/item.h:142-148`
- Modify: All call sites that access `EnhancementSlot::bonus` (search the codebase)

- [ ] **Step 1: Update the struct in `include/astra/item.h`**

Replace the existing `EnhancementSlot`:

```cpp
struct EnhancementSlot {
    bool filled = false;
    bool committed = false;   // true after assemble, false while staged
    uint32_t material_id = 0;
    std::string material_name;
    StatModifiers stat_bonus;            // renamed from `bonus`
    EnergyModifiers energy_bonus;        // new
    std::optional<SolarPanelData> solar_panel;  // new — present when this slot holds a Solar Panel
};
```

Add `#include "astra/energy.h"` at the top of `item.h` (just under the `aura_grant.h` include).

- [ ] **Step 2: Find all call sites that read/write `enh.bonus`**

Run: `grep -rn "\\.bonus\\." /Users/jeffrey/dev/crawler/src /Users/jeffrey/dev/crawler/include | grep -v "ship_modifiers\|granted_auras\|target_npc\|is_combat\|stat_bonus"`

Expected hits: `src/tinkering.cpp` (apply enhancement), `src/save_file.cpp:322-326,399-403` (serialize), `include/astra/item.h::Equipment::total_modifiers()` (sum), `src/character_screen.cpp` (display), possibly `src/game_rendering.cpp`.

- [ ] **Step 3: Rename in every hit**

For each file from Step 2, change `enh.bonus.X` → `enh.stat_bonus.X`. Use editor search-and-replace within those files only (don't blanket-replace `.bonus.` because `ship_modifiers` etc. are unrelated).

- [ ] **Step 4: Update `src/save_file.cpp` write_item to write the new fields**

Locate `write_item` near line 317. After the existing `bonus.quickness` write, append:

```cpp
        // v46: energy_bonus + solar_panel
        w.write_i32(enh.energy_bonus.capacity_bonus);
        w.write_i32(enh.energy_bonus.charge_rate_bonus);
        w.write_i32(enh.energy_bonus.discharge_efficiency);
        w.write_u8(enh.solar_panel.has_value() ? 1 : 0);
        if (enh.solar_panel) {
            w.write_u8(enh.solar_panel->active ? 1 : 0);
            w.write_i32(enh.solar_panel->energy_per_tick);
            w.write_i32(enh.solar_panel->tick_interval);
            w.write_i32(enh.solar_panel->accumulator);
        }
```

In `read_item` near line 403, add the matching reader after the renamed `stat_bonus.quickness` read:

```cpp
        item.enhancements[i].energy_bonus.capacity_bonus = r.read_i32();
        item.enhancements[i].energy_bonus.charge_rate_bonus = r.read_i32();
        item.enhancements[i].energy_bonus.discharge_efficiency = r.read_i32();
        if (r.read_u8() != 0) {
            SolarPanelData sp;
            sp.active = r.read_u8() != 0;
            sp.energy_per_tick = r.read_i32();
            sp.tick_interval = r.read_i32();
            sp.accumulator = r.read_i32();
            item.enhancements[i].solar_panel = sp;
        }
```

(The save version bump itself happens in Task 21 — for now we're just adding fields; older saves still won't load past v46 because we'll bump the version then.)

- [ ] **Step 5: Build**

Run: `cmake --build build -j`
Expected: build succeeds.

- [ ] **Step 6: Commit**

```bash
git add include/astra/item.h src/save_file.cpp src/tinkering.cpp src/character_screen.cpp src/game_rendering.cpp
git commit -m "refactor(item): rename EnhancementSlot::bonus → stat_bonus, add energy_bonus and solar_panel"
```

---

### Task 3: Add `energy` and `consumer` optionals to Item (alongside RangedData)

**Files:**
- Modify: `include/astra/item.h:165-220`

- [ ] **Step 1: Add the optionals to `Item`**

In `Item` (after the existing `std::optional<RangedData> ranged;` line ~185), add:

```cpp
    std::optional<EnergyStore> energy;
    std::optional<EnergyConsumer> consumer;
```

- [ ] **Step 2: Add `<optional>` include if missing**

Already present in item.h (line 9). No change.

- [ ] **Step 3: Update save_file write_item / read_item to (de)serialize the optionals**

In `write_item`, after the existing `ranged` block (~line 313), append:

```cpp
    // v46: energy / consumer
    w.write_u8(item.energy.has_value() ? 1 : 0);
    if (item.energy) {
        w.write_i32(item.energy->current);
        w.write_i32(item.energy->capacity);
    }
    w.write_u8(item.consumer.has_value() ? 1 : 0);
    if (item.consumer) {
        w.write_i32(item.consumer->energy_per_use);
    }
```

In `read_item`, after the existing ranged block (~line 389), append:

```cpp
    if (r.read_u8() != 0) {
        EnergyStore e;
        e.current  = r.read_i32();
        e.capacity = r.read_i32();
        item.energy = e;
    }
    if (r.read_u8() != 0) {
        EnergyConsumer c;
        c.energy_per_use = r.read_i32();
        item.consumer = c;
    }
```

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: build succeeds.

- [ ] **Step 5: Commit**

```bash
git add include/astra/item.h src/save_file.cpp
git commit -m "feat(item): add EnergyStore and EnergyConsumer optionals to Item"
```

---

### Task 4: Mirror RangedData charge into `energy` / `consumer` on weapon defs

This task makes the new fields populated on every ranged weapon while the old fields still drive combat. Combat will switch over in Task 6.

**Files:**
- Modify: `src/item_defs.cpp:12-118` (each `build_*` for a ranged weapon)

- [ ] **Step 1: Update each ranged weapon builder**

For `build_plasma_pistol` (line 29), add after the existing `it.ranged = ...` line:

```cpp
    it.energy = EnergyStore{20, 20};
    it.consumer = EnergyConsumer{1};
```

Repeat for the other four weapons with values matching their `RangedData{capacity, per_shot, current, range}`:

| Function | EnergyStore | EnergyConsumer |
|---|---|---|
| `build_plasma_pistol` | `{20, 20}` | `{1}` |
| `build_ion_blaster` | `{15, 15}` | `{2}` |
| `build_pulse_rifle` | `{30, 30}` | `{2}` |
| `build_arc_caster` | `{12, 12}` | `{3}` |
| `build_void_lance` | `{10, 10}` | `{4}` |

(The `RangedData` line stays untouched for now — Task 6 switches combat to read `energy`/`consumer`, Task 11 deletes the duplicate fields.)

- [ ] **Step 2: Add `#include "astra/energy.h"` to `src/item_defs.cpp`**

Top of file, alongside the existing `astra/item_defs.h` include.

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/item_defs.cpp
git commit -m "feat(item_defs): populate EnergyStore/Consumer on ranged weapons (alongside RangedData)"
```

---

### Task 5: Replace single Energy Cell with five tier defs

**Files:**
- Modify: `include/astra/item_ids.h` — add four new IDs
- Modify: `src/item_defs.cpp:124-141` — split `build_battery` into five builders
- Modify: any callers of `build_battery()` — switch to `build_standard_energy_cell()`

- [ ] **Step 1: Add new item IDs**

In `include/astra/item_ids.h`, after the existing `ITEM_BATTERY = 6;` line:

```cpp
constexpr uint16_t ITEM_SMALL_ENERGY_CELL       = 6;   // was ITEM_BATTERY
constexpr uint16_t ITEM_STANDARD_ENERGY_CELL    = 70;
constexpr uint16_t ITEM_LARGE_ENERGY_CELL       = 71;
constexpr uint16_t ITEM_INDUSTRIAL_ENERGY_CELL  = 72;
constexpr uint16_t ITEM_ANTIMATTER_CELL         = 73;
```

Keep the `ITEM_BATTERY` alias for one task so call sites still compile. After Task 12 we can remove.

```cpp
// Backwards-compatible alias — to be removed after callers migrate.
constexpr uint16_t ITEM_BATTERY = ITEM_SMALL_ENERGY_CELL;
```

(Pick free IDs in the 70-79 range — confirm they're unused with `grep "= 7[0-9]" include/astra/item_ids.h`.)

- [ ] **Step 2: Replace `build_battery` with five tier builders in `src/item_defs.cpp`**

Delete the existing `build_battery` (lines ~124-141). Add:

```cpp
static Item build_cell(uint16_t def_id, uint32_t id, const char* name,
                       Rarity rarity, int capacity, int weight, int buy, int sell) {
    Item it;
    it.item_def_id = def_id;
    it.id = id;
    it.name = name;
    it.description = "Persistent power cell. Holds energy for weapons, shields, and gadgets.";
    it.type = ItemType::Battery;
    it.rarity = rarity;
    it.weight = weight;
    it.stackable = false;          // per-instance state
    it.stack_count = 1;
    it.buy_value = buy;
    it.sell_value = sell;
    it.usable = true;
    it.energy = EnergyStore{capacity, capacity};
    return it;
}

Item build_small_energy_cell()        { return build_cell(ITEM_SMALL_ENERGY_CELL,       2001, "Small Energy Cell",       Rarity::Common,   60,  1,  15,  5); }
Item build_standard_energy_cell()     { return build_cell(ITEM_STANDARD_ENERGY_CELL,    2010, "Standard Energy Cell",    Rarity::Common,   150, 1,  35,  12); }
Item build_large_energy_cell()        { return build_cell(ITEM_LARGE_ENERGY_CELL,       2011, "Large Energy Cell",       Rarity::Uncommon, 400, 2,  90,  30); }
Item build_industrial_energy_cell()   { return build_cell(ITEM_INDUSTRIAL_ENERGY_CELL,  2012, "Industrial Energy Cell",  Rarity::Rare,     800, 3,  220, 70); }
Item build_antimatter_cell()          { return build_cell(ITEM_ANTIMATTER_CELL,         2013, "Antimatter Cell",         Rarity::Epic,     2000,3,  650, 200); }

// Legacy: used by old callers; returns a Standard cell.
Item build_battery() { return build_standard_energy_cell(); }
```

- [ ] **Step 3: Declare new builders in `include/astra/item_defs.h`**

Add to the public builder list:

```cpp
Item build_small_energy_cell();
Item build_standard_energy_cell();
Item build_large_energy_cell();
Item build_industrial_energy_cell();
Item build_antimatter_cell();
```

- [ ] **Step 4: Run init_enhancement_slots on cells**

`init_enhancement_slots(item)` is already called by the rarity-based path. Cells receive 1/2/2/3/3 slots based on their rarity (Common→1, Uncommon→2, Rare→3, Epic→3, Legendary→3 by current logic — verify in `src/tinkering.cpp:71-78`). For the spec's "Antimatter Cell — 3 slots" target, the rarity-based mapping is fine. For "Large Energy Cell — 2 slots" the Uncommon mapping gives 2. Match.

In `build_cell`, after setting fields, call:

```cpp
    init_enhancement_slots(it);
```

(Need `#include "astra/tinkering.h"` at the top of `item_defs.cpp`.)

- [ ] **Step 5: Add the new cells to vendor stock and loot tables**

Search for callers of `build_battery`:

`grep -rn "build_battery\(" /Users/jeffrey/dev/crawler/src/ /Users/jeffrey/dev/crawler/include/`

For each:
- Vendors: replace with a mix — push 2× `build_small_energy_cell()` and 1× `build_standard_energy_cell()` per stock pass.
- Loot tables / quest rewards: keep as `build_battery()` (which now resolves to Standard).

- [ ] **Step 6: Build**

Run: `cmake --build build -j`
Expected: build succeeds.

- [ ] **Step 7: Sanity test in dev mode**

Run: `./build/astra` and use dev-mode item spawn (check existing dev console help for the spawn command). Spawn each cell tier; confirm names render and rarities color-code as expected.

- [ ] **Step 8: Commit**

```bash
git add include/astra/item_ids.h include/astra/item_defs.h src/item_defs.cpp
git commit -m "feat(item_defs): five-tier Energy Cell lineup with persistent EnergyStore"
```

---

### Task 6: Switch combat firing path to read from `Item::energy`/`consumer`

**Files:**
- Modify: `src/game_combat.cpp:748-790` (the firing path that currently reads `rd.current_charge` / `rd.charge_per_shot`)

- [ ] **Step 1: Replace charge reads in `shoot_target`**

Find the block starting at `auto& rd = *weapon->ranged;` (line ~748). Below the range check, replace the charge-read section:

```cpp
    // Energy check — auto-recharge if below per-shot cost
    if (!weapon->energy || !weapon->consumer) {
        game.log("Weapon has no energy system."); // shouldn't happen
        return;
    }
    auto& estore = *weapon->energy;
    int per_shot = weapon->consumer->energy_per_use;
    if (estore.current < per_shot) {
        // Auto-recharge from inventory
        bool recharged = combat_.recharge_weapon(game, /*log_full=*/false);
        if (!recharged || estore.current < per_shot) {
            game.log("Weapon empty. No charged cells available.");
            return;
        }
    }

    // Spend energy
    estore.current -= per_shot;
```

(Remove the existing block from `if (rd.current_charge < rd.charge_per_shot)` through `rd.current_charge -= rd.charge_per_shot;`.)

- [ ] **Step 2: Update the post-shot log line**

Find the line `... + std::to_string(rd.current_charge) + "/" + std::to_string(rd.charge_capacity) + ...` (~line 856). Replace `rd.current_charge` → `estore.current`, `rd.charge_capacity` → `estore.capacity`.

- [ ] **Step 3: Update range check use**

`rd.max_range` stays — `RangedData::max_range` is preserved.

- [ ] **Step 4: Add `recharge_weapon` declaration**

In `include/astra/combat_system.h`, change `void reload_weapon(Game& game);` to:

```cpp
    bool recharge_weapon(Game& game, bool log_full = true);
```

(Returns true if any energy was deposited.)

In `src/game_combat.cpp`, rename the existing `void CombatSystem::reload_weapon(Game& game)` to a stub that delegates:

```cpp
bool CombatSystem::recharge_weapon(Game& game, bool log_full) {
    // Implementation comes in Task 8. For now, behave like the old reload code
    // but reading from Item::energy.
    auto& weapon = game.player().equipment.missile;
    if (!weapon || !weapon->energy) {
        if (log_full) game.log("No ranged weapon equipped.");
        return false;
    }
    auto& estore = *weapon->energy;
    if (is_full(estore)) {
        if (log_full) game.log(weapon->label() + " is fully charged.");
        return false;
    }
    for (auto& it : game.player().inventory.items) {
        if (it.type == ItemType::Battery && it.energy && !is_empty(*it.energy)) {
            int moved = transfer_energy(*it.energy, estore, deficit(estore));
            if (moved > 0) {
                game.log("Recharged " + weapon->label() + ". (+" +
                         std::to_string(moved) + " charge, " +
                         std::to_string(estore.current) + "/" +
                         std::to_string(estore.capacity) + ")");
                game.advance_world(ActionCost::wait);
                return true;
            }
        }
    }
    if (log_full) game.log("No charged cells to recharge from.");
    return false;
}
```

(The richer "drain multiple cells, sort by highest" logic lands in Task 8. This stub gets the build green and pulls from one cell.)

- [ ] **Step 5: Update key binding to call the new name**

In `src/game_input.cpp:368`:

```cpp
        case 'r': combat_.recharge_weapon(*this); break;
```

- [ ] **Step 6: Update auto-reload call inside the firing path**

Replace `combat_.reload_weapon(...)` if any other call sites refer to it:

`grep -rn "reload_weapon" /Users/jeffrey/dev/crawler/src/ /Users/jeffrey/dev/crawler/include/`

Each hit becomes `recharge_weapon`. Remove `reload_weapon` entirely from the header.

- [ ] **Step 7: Build**

Run: `cmake --build build -j`
Expected: build succeeds.

- [ ] **Step 8: Sanity test**

Run: `./build/astra`, spawn a Plasma Pistol and a Standard Energy Cell, equip the pistol, target an NPC, fire until empty, press `r`, verify the weapon recharges and the log shows the new format.

- [ ] **Step 9: Commit**

```bash
git add include/astra/combat_system.h src/game_combat.cpp src/game_input.cpp
git commit -m "refactor(combat): firing path reads from Item::energy/consumer; rename reload→recharge"
```

---

### Task 7: Migrate shield state to equipped Item::energy

This drops `Player::shield_hp` / `shield_max_hp`. Shield charge lives on the equipped shield item via `Item::energy`. On equip, the item's energy is initialized to (capacity, capacity) if it's missing or zero. Damage drains `equipment.shield->energy.current`.

**Files:**
- Modify: `include/astra/player.h:60-61`
- Modify: `src/item_defs.cpp` (shield builders ~line 630-710)
- Modify: `src/game_combat.cpp:927+` (`reload_shield` → `recharge_shield`) and any damage-to-shield path
- Modify: `src/character_screen.cpp` (shield display)
- Modify: any place that reads `player.shield_hp`

- [ ] **Step 1: Update shield item builders to use `energy`**

For each shield in `src/item_defs.cpp:630-710` (search for `shield_capacity = `), replace:

```cpp
    it.shield_capacity = 10; it.shield_hp = 10;
```

with:

```cpp
    it.energy = EnergyStore{10, 10};
```

Repeat for all six shields with their respective capacities.

- [ ] **Step 2: Drop `shield_capacity` and `shield_hp` from `Item`**

In `include/astra/item.h`, delete:

```cpp
    int shield_capacity = 0;
    int shield_hp = 0;
```

and matching reads/writes in `src/save_file.cpp` (lines 345-346 write, 422-423 read). The save schema will bump in Task 21; for now simply remove these reads.

- [ ] **Step 3: Drop `Player::shield_hp` / `shield_max_hp`**

In `include/astra/player.h`:

```cpp
    // remove:
    int shield_hp = 0;
    int shield_max_hp = 0;
```

Add a helper accessor at the bottom of `Player`:

```cpp
    EnergyStore* shield_energy() {
        if (equipment.shield && equipment.shield->energy)
            return &*equipment.shield->energy;
        return nullptr;
    }
    const EnergyStore* shield_energy() const {
        if (equipment.shield && equipment.shield->energy)
            return &*equipment.shield->energy;
        return nullptr;
    }
```

(Need `#include "astra/energy.h"` in `player.h`.)

- [ ] **Step 4: Update every reader of `player.shield_hp`**

Run: `grep -rn "shield_hp\|shield_max_hp" /Users/jeffrey/dev/crawler/src/ /Users/jeffrey/dev/crawler/include/`

Expected hits: `src/game_combat.cpp` (reload_shield, damage), `src/character_screen.cpp` (display), `src/game_rendering.cpp` (HUD if any), `src/save_file.cpp` (read/write — drop these), `src/game.cpp` (init).

For each hit, rewrite to use `player.shield_energy()`:

```cpp
auto* sh = player.shield_energy();
if (sh) {
    // sh->current, sh->capacity
}
```

- [ ] **Step 5: Rewrite `reload_shield` → `recharge_shield`**

In `include/astra/combat_system.h`, replace:

```cpp
    void reload_shield(Game& game);
```

with:

```cpp
    bool recharge_shield(Game& game, bool log_full = true);
```

In `src/game_combat.cpp:927`, replace the function body with the same shape as `recharge_weapon` from Task 6, but operating on `player.shield_energy()`:

```cpp
bool CombatSystem::recharge_shield(Game& game, bool log_full) {
    auto* sh = game.player().shield_energy();
    if (!sh) {
        if (log_full) game.log("No energy shield equipped.");
        return false;
    }
    if (is_full(*sh)) {
        if (log_full) game.log("Shield is at full charge.");
        return false;
    }
    for (auto& it : game.player().inventory.items) {
        if (it.type == ItemType::Battery && it.energy && !is_empty(*it.energy)) {
            int moved = transfer_energy(*it.energy, *sh, deficit(*sh));
            if (moved > 0) {
                game.log("Recharged shield. (+" + std::to_string(moved) +
                         " charge, " + std::to_string(sh->current) + "/" +
                         std::to_string(sh->capacity) + ")");
                game.advance_world(ActionCost::wait);
                return true;
            }
        }
    }
    if (log_full) game.log("No charged cells to recharge shield.");
    return false;
}
```

- [ ] **Step 6: Build**

Run: `cmake --build build -j`
Expected: build succeeds.

- [ ] **Step 7: Sanity test**

Run: `./build/astra`, equip a shield, take damage, verify the shield absorbs and depletes; spawn cells and verify shield can be recharged after Task 12 binds the key (until then, exercise via dev console or ignore).

- [ ] **Step 8: Commit**

```bash
git add include/astra/item.h include/astra/player.h include/astra/combat_system.h src/item_defs.cpp src/game_combat.cpp src/save_file.cpp src/character_screen.cpp src/game_rendering.cpp src/game.cpp
git commit -m "refactor(shield): migrate shield state to Item::energy; rename reload_shield→recharge_shield"
```

---

### Task 8: Multi-cell, highest-charge-first recharge for weapon and shield

**Files:**
- Modify: `src/game_combat.cpp` (`recharge_weapon`, `recharge_shield`)

- [ ] **Step 1: Add a private helper in CombatSystem**

In `include/astra/combat_system.h`, add:

```cpp
private:
    // Drain cells from inventory (highest-charge first) into target until full.
    // Returns total energy deposited.
    int recharge_target_(Game& game, EnergyStore& target);
```

In `src/game_combat.cpp`, define:

```cpp
int CombatSystem::recharge_target_(Game& game, EnergyStore& target) {
    auto& items = game.player().inventory.items;
    // Build index list of cells with current > 0, sorted by current descending.
    std::vector<int> idxs;
    for (int i = 0; i < (int)items.size(); ++i) {
        const auto& it = items[i];
        if (it.type == ItemType::Battery && it.energy && it.energy->current > 0)
            idxs.push_back(i);
    }
    std::sort(idxs.begin(), idxs.end(), [&](int a, int b) {
        return items[a].energy->current > items[b].energy->current;
    });

    int total = 0;
    for (int i : idxs) {
        if (is_full(target)) break;
        // discharge_efficiency: sum across the cell's enhancement slots
        int eff = 0;
        for (const auto& enh : items[i].enhancements)
            if (enh.committed) eff += enh.energy_bonus.discharge_efficiency;
        int moved = transfer_energy(*items[i].energy, target, deficit(target), eff);
        total += moved;
    }
    return total;
}
```

- [ ] **Step 2: Replace `recharge_weapon` body to call the helper**

```cpp
bool CombatSystem::recharge_weapon(Game& game, bool log_full) {
    auto& weapon = game.player().equipment.missile;
    if (!weapon || !weapon->energy) {
        if (log_full) game.log("No ranged weapon equipped.");
        return false;
    }
    auto& estore = *weapon->energy;
    if (is_full(estore)) {
        if (log_full) game.log(weapon->label() + " is fully charged.");
        return false;
    }
    int moved = recharge_target_(game, estore);
    if (moved == 0) {
        if (log_full) game.log("No charged cells to recharge from.");
        return false;
    }
    game.log("Recharged " + weapon->label() + ". (+" + std::to_string(moved) +
             " charge, " + std::to_string(estore.current) + "/" +
             std::to_string(estore.capacity) + ")");
    game.advance_world(ActionCost::wait);
    return true;
}
```

- [ ] **Step 3: Replace `recharge_shield` body identically**

Same structure but using `player.shield_energy()`.

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: build succeeds.

- [ ] **Step 5: Sanity test**

Run: `./build/astra`, spawn a Pulse Rifle (capacity 30), and three Small Energy Cells (60 each, with one at 5/60). Empty the rifle. Press `r`. Verify the highest-charge cell drains first; total movement equals the rifle's deficit; lowest cell preserved.

- [ ] **Step 6: Commit**

```bash
git add include/astra/combat_system.h src/game_combat.cpp
git commit -m "feat(combat): multi-cell recharge, highest-charge-first, with discharge efficiency"
```

---

### Task 9: Drop obsolete `RangedData::charge_*` fields

**Files:**
- Modify: `include/astra/item.h:158-163`
- Modify: `src/item_defs.cpp` — remove `it.ranged = RangedData{...}` 4-arg form, switch to a struct that only has `max_range`
- Modify: `src/save_file.cpp` (write_item / read_item ranged blocks)

- [ ] **Step 1: Strip `RangedData`**

In `include/astra/item.h`:

```cpp
struct RangedData {
    int max_range = 8;
};
```

- [ ] **Step 2: Update item_defs.cpp**

For each ranged weapon, replace `it.ranged = RangedData{20, 1, 20, 6};` with `it.ranged = RangedData{6};` (max_range only):

| Weapon | New |
|---|---|
| Plasma Pistol | `it.ranged = RangedData{6};` |
| Ion Blaster | `it.ranged = RangedData{8};` |
| Pulse Rifle | `it.ranged = RangedData{12};` |
| Arc Caster | `it.ranged = RangedData{5};` |
| Void Lance | `it.ranged = RangedData{15};` |

- [ ] **Step 3: Update save_file.cpp**

In `write_item`, replace the existing ranged block with:

```cpp
    w.write_u8(item.ranged.has_value() ? 1 : 0);
    if (item.ranged) {
        w.write_i32(item.ranged->max_range);
    }
```

In `read_item`:

```cpp
    if (r.read_u8() != 0) {
        RangedData rd;
        rd.max_range = r.read_i32();
        item.ranged = rd;
    }
```

- [ ] **Step 4: Find lingering callers**

Run: `grep -rn "charge_capacity\|charge_per_shot\|current_charge" /Users/jeffrey/dev/crawler/src /Users/jeffrey/dev/crawler/include`

Expected: zero hits. Fix any that remain (likely in `src/game_rendering.cpp` for the bottom-bar — that gets fully rewritten in Task 18; for now redirect to `weapon->energy->current` etc.).

- [ ] **Step 5: Build**

Run: `cmake --build build -j`
Expected: build succeeds.

- [ ] **Step 6: Commit**

```bash
git add include/astra/item.h src/item_defs.cpp src/save_file.cpp src/game_rendering.cpp src/game_combat.cpp
git commit -m "refactor(item): drop RangedData::charge_*; energy now lives in Item::energy/consumer"
```

---

### Task 10: Add World::is_outdoor()

**Files:**
- Modify: `include/astra/world_manager.h:95-96`

- [ ] **Step 1: Add the predicate**

After `bool on_detail_map() const ...`:

```cpp
    bool is_outdoor() const { return on_overworld() || on_detail_map(); }
```

- [ ] **Step 2: Build**

Run: `cmake --build build -j`
Expected: build succeeds.

- [ ] **Step 3: Commit**

```bash
git add include/astra/world_manager.h
git commit -m "feat(world): add is_outdoor() predicate (overworld or detail map)"
```

---

### Task 11: Solar Panel — material catalog entries and tinkering integration

**Files:**
- Modify: `include/astra/tinkering.h` — extend `MaterialEffect` with `EnergyModifiers` and `SolarPanelData`
- Modify: `src/tinkering.cpp` — material catalog adds Solar Panel × 3 tiers; `enhance_item` writes panel data into the slot
- Modify: `include/astra/item_ids.h` — IDs for the three Solar Panels

- [ ] **Step 1: Extend `MaterialEffect`**

In `include/astra/tinkering.h`:

```cpp
struct MaterialEffect {
    uint32_t material_id;
    const char* name;
    StatModifiers stat_bonus;            // renamed
    EnergyModifiers energy_bonus;        // new
    std::optional<SolarPanelData> solar_panel; // present for Solar Panel materials
};
```

Add `#include <optional>` and `#include "astra/energy.h"` if not already present.

- [ ] **Step 2: Add IDs in `include/astra/item_ids.h`**

```cpp
constexpr uint16_t ITEM_SOLAR_PANEL_COMMON   = 80;
constexpr uint16_t ITEM_SOLAR_PANEL_UNCOMMON = 81;
constexpr uint16_t ITEM_SOLAR_PANEL_RARE     = 82;
```

- [ ] **Step 3: Add Solar Panel entries to the material table**

Find the `MaterialEffect` table in `src/tinkering.cpp` (search for an array literal of `MaterialEffect{...}`). Append:

```cpp
    {ITEM_SOLAR_PANEL_COMMON,   "Solar Panel",          {}, {}, SolarPanelData{ true, 5,  2, 0 }},
    {ITEM_SOLAR_PANEL_UNCOMMON, "Polished Solar Panel", {}, {}, SolarPanelData{ true, 8,  2, 0 }},
    {ITEM_SOLAR_PANEL_RARE,     "Prismatic Solar Panel",{}, {}, SolarPanelData{ true, 12, 2, 0 }},
```

If the existing table also includes `EnergyModifiers`-bearing materials (Capacitor Coil etc.), add those too. If not, defer those to a follow-up — Solar Panel alone satisfies this spec; the other energy-mod materials are listed as future content in the spec.

- [ ] **Step 4: Update `enhance_item` to apply solar_panel from material**

Find the `enhance_item` function in `src/tinkering.cpp`. After the existing `slot.bonus = effect->bonus;` (now `slot.stat_bonus = effect->stat_bonus;`):

```cpp
    slot.energy_bonus = effect->energy_bonus;
    slot.solar_panel  = effect->solar_panel;  // optional, copied as-is
```

- [ ] **Step 5: Update `commit_enhancements` (if it changes the bonus shape)**

Likely no change — committing flips a flag; bonuses already populated.

- [ ] **Step 6: Add Solar Panel item builders if loot/vendors need them**

Add to `src/item_defs.cpp`:

```cpp
Item build_solar_panel(uint16_t def_id, uint32_t id, const char* name, Rarity rarity, int buy, int sell) {
    Item it;
    it.item_def_id = def_id;
    it.id = id;
    it.name = name;
    it.description = "Photovoltaic mod. Slots into any energy item; recharges it while outdoors.";
    it.type = ItemType::CraftingMaterial;
    it.rarity = rarity;
    it.weight = 1;
    it.stackable = false;
    it.buy_value = buy;
    it.sell_value = sell;
    return it;
}

Item build_solar_panel_common()   { return build_solar_panel(ITEM_SOLAR_PANEL_COMMON,   2050, "Solar Panel",          Rarity::Common,   60,   20); }
Item build_solar_panel_uncommon() { return build_solar_panel(ITEM_SOLAR_PANEL_UNCOMMON, 2051, "Polished Solar Panel", Rarity::Uncommon, 180,  60); }
Item build_solar_panel_rare()     { return build_solar_panel(ITEM_SOLAR_PANEL_RARE,     2052, "Prismatic Solar Panel",Rarity::Rare,     500,  170); }
```

Declare in `include/astra/item_defs.h`. Add to vendor stock and tinker-shop loot.

- [ ] **Step 7: Build**

Run: `cmake --build build -j`
Expected: build succeeds.

- [ ] **Step 8: Sanity test**

Run: `./build/astra`. Spawn a Small Energy Cell and a Solar Panel. Open tinkering, slot the panel into the cell, commit. Inspect the cell's enhancements via the inventory detail view and verify the slot now reports a Solar Panel.

- [ ] **Step 9: Commit**

```bash
git add include/astra/tinkering.h include/astra/item_ids.h include/astra/item_defs.h src/tinkering.cpp src/item_defs.cpp
git commit -m "feat(tinkering): Solar Panel materials (3 tiers) installable into energy items"
```

---

### Task 12: EnergySystem — tick loop deposits energy from active panels when outdoors

**Files:**
- Create: `include/astra/energy_system.h`, `src/energy_system.cpp`
- Modify: `include/astra/game.h` — own an `EnergySystem energy_;`
- Modify: `src/game.cpp` — call `energy_.tick(player_, world_, ticks)` from `advance_world`

- [ ] **Step 1: Declare the system**

```cpp
// include/astra/energy_system.h
#pragma once

namespace astra {
struct Player;
class WorldManager;

class EnergySystem {
public:
    // Deposit energy from active Solar Panels on the player's items
    // when the player is outdoors. `ticks` is the wall-tick count
    // elapsed during this advance_world() call.
    void tick(Player& player, const WorldManager& world, int ticks);
};

} // namespace astra
```

- [ ] **Step 2: Implement the system**

```cpp
// src/energy_system.cpp
#include "astra/energy_system.h"

#include "astra/energy.h"
#include "astra/item.h"
#include "astra/player.h"
#include "astra/world_manager.h"

#include <algorithm>

namespace astra {

namespace {

void tick_item(Item& item, int ticks) {
    if (!item.energy) return;
    if (is_full(*item.energy)) return;
    // Sum any cell-side charge_rate_bonus from committed enhancement slots
    int rate_bonus_pct = 0;
    for (const auto& enh : item.enhancements)
        if (enh.committed) rate_bonus_pct += enh.energy_bonus.charge_rate_bonus;
    for (auto& enh : item.enhancements) {
        if (!enh.committed || !enh.solar_panel) continue;
        auto& sp = *enh.solar_panel;
        if (!sp.active) continue;
        sp.accumulator += ticks;
        while (sp.accumulator >= sp.tick_interval) {
            sp.accumulator -= sp.tick_interval;
            int deposit = sp.energy_per_tick + (sp.energy_per_tick * rate_bonus_pct) / 100;
            item.energy->current = std::min(item.energy->capacity, item.energy->current + deposit);
            if (is_full(*item.energy)) {
                sp.accumulator = 0;
                break;
            }
        }
    }
}

} // anon

void EnergySystem::tick(Player& player, const WorldManager& world, int ticks) {
    if (ticks <= 0) return;
    if (!world.is_outdoor()) return;
    for (auto& it : player.inventory.items) tick_item(it, ticks);
    auto sweep = [&](std::optional<Item>& slot) { if (slot) tick_item(*slot, ticks); };
    sweep(player.equipment.face);
    sweep(player.equipment.head);
    sweep(player.equipment.body);
    sweep(player.equipment.left_arm);
    sweep(player.equipment.right_arm);
    sweep(player.equipment.left_hand);
    sweep(player.equipment.right_hand);
    sweep(player.equipment.back);
    sweep(player.equipment.feet);
    sweep(player.equipment.thrown);
    sweep(player.equipment.missile);
    sweep(player.equipment.shield);
}

} // namespace astra
```

- [ ] **Step 3: Wire into Game**

In `include/astra/game.h`, add the member near other systems:

```cpp
    EnergySystem energy_;
```

Add `#include "astra/energy_system.h"`.

In `src/game.cpp`, find `void Game::advance_world(int cost) { ... }` and append at the end:

```cpp
    energy_.tick(player_, world_, cost);
```

- [ ] **Step 4: Register the new .cpp in CMake**

Add `src/energy_system.cpp` to the source list (alongside `src/energy.cpp`).

- [ ] **Step 5: Build**

Run: `cmake --build build -j`
Expected: build succeeds.

- [ ] **Step 6: Sanity test (overworld)**

Run: `./build/astra`. New character. Move to overworld. Spawn a Small Energy Cell, drain it (e.g. recharge a weapon to drain the cell to ~5/60), install a Solar Panel, commit. Walk on the overworld — every two ticks the cell should gain +5 energy. Verify by re-opening inventory after ~10 moves.

- [ ] **Step 7: Sanity test (indoor)**

Enter a building or station interior. Walk for several turns. Cell charge should NOT change.

- [ ] **Step 8: Commit**

```bash
git add include/astra/energy_system.h src/energy_system.cpp include/astra/game.h src/game.cpp CMakeLists.txt
git commit -m "feat(energy): EnergySystem ticks Solar Panel deposits when outdoors"
```

---

### Task 13: Bind `Shift-R`, `b`, `Shift-B` and add manual cell picker

**Files:**
- Modify: `src/game_input.cpp:368` (existing `r`, add three more)
- Modify: `include/astra/combat_system.h` — declare manual-picker entry points
- Modify: `src/game_combat.cpp` — implement manual picker
- Modify: `include/astra/ui_types.h` or wherever modal pickers live — add an inventory-cell picker if one doesn't exist

- [ ] **Step 1: Decide on cell-picker UI**

Search for existing modal lists: `grep -rn "ModalList\|PickerModal\|popup\|select_from" /Users/jeffrey/dev/crawler/include/astra | head -20`

- If a generic list-picker exists (e.g. for cookbook recipes, character screen), reuse it.
- Otherwise, lean on the existing inventory-list rendering — open the inventory in a "select cell" mode that filters to `ItemType::Battery`.

(For this plan, assume the latter and add a `picker_mode_` flag to whatever holds inventory state.)

- [ ] **Step 2: Add bindings in `src/game_input.cpp`**

In the keypress switch (around line 368), replace and extend:

```cpp
        case 'r':
            combat_.recharge_weapon(*this);
            break;
        case 'R':
            combat_.begin_recharge_picker(*this, /*shield=*/false);
            break;
        case 'b':
            combat_.recharge_shield(*this);
            break;
        case 'B':
            combat_.begin_recharge_picker(*this, /*shield=*/true);
            break;
```

(Confirm `R` and `B` aren't already bound — `grep -n "case 'R'\\|case 'B'"`.)

- [ ] **Step 3: Implement `begin_recharge_picker`**

In `include/astra/combat_system.h`:

```cpp
    void begin_recharge_picker(Game& game, bool target_is_shield);
    bool finish_recharge_picker(Game& game, int cell_inventory_index);
```

In `src/game_combat.cpp`:

```cpp
void CombatSystem::begin_recharge_picker(Game& game, bool target_is_shield) {
    EnergyStore* target = nullptr;
    if (target_is_shield) target = game.player().shield_energy();
    else if (game.player().equipment.missile && game.player().equipment.missile->energy)
        target = &*game.player().equipment.missile->energy;
    if (!target) {
        game.log(target_is_shield ? "No energy shield equipped." : "No ranged weapon equipped.");
        return;
    }
    if (is_full(*target)) {
        game.log(target_is_shield ? "Shield already full." : "Weapon already full.");
        return;
    }
    recharge_picker_target_is_shield_ = target_is_shield;
    game.open_inventory_in_mode(InventoryMode::CellPicker);  // new mode flag
}

bool CombatSystem::finish_recharge_picker(Game& game, int cell_inventory_index) {
    auto& items = game.player().inventory.items;
    if (cell_inventory_index < 0 || cell_inventory_index >= (int)items.size()) return false;
    auto& cell = items[cell_inventory_index];
    if (cell.type != ItemType::Battery || !cell.energy || is_empty(*cell.energy)) {
        game.log("That cell is empty.");
        return false;
    }
    EnergyStore* target = recharge_picker_target_is_shield_
        ? game.player().shield_energy()
        : (game.player().equipment.missile ? &*game.player().equipment.missile->energy : nullptr);
    if (!target) return false;
    int eff = 0;
    for (const auto& enh : cell.enhancements)
        if (enh.committed) eff += enh.energy_bonus.discharge_efficiency;
    int moved = transfer_energy(*cell.energy, *target, deficit(*target), eff);
    game.log("Recharged. (+" + std::to_string(moved) + " from " + cell.name + ")");
    game.advance_world(ActionCost::wait);
    return moved > 0;
}
```

(Add the private member `bool recharge_picker_target_is_shield_ = false;` to the header.)

- [ ] **Step 4: Add inventory `CellPicker` mode**

Search for existing inventory mode handling (`grep -rn "InventoryMode\|inventory_mode" /Users/jeffrey/dev/crawler/include/astra /Users/jeffrey/dev/crawler/src`). If an enum exists, add `CellPicker`. If not, declare:

```cpp
// include/astra/ui_types.h or near the inventory header
enum class InventoryMode : uint8_t {
    Normal,
    CellPicker,
};
```

In the inventory rendering / input code:

- When `mode == CellPicker`, filter the displayed list to `ItemType::Battery` items, sorted by `energy->current` descending.
- `Enter` on a row calls `combat_.finish_recharge_picker(game, index)` and closes the inventory.
- `Esc` closes without action.

- [ ] **Step 5: Build**

Run: `cmake --build build -j`
Expected: build succeeds.

- [ ] **Step 6: Sanity test**

Run: `./build/astra`. Spawn a weapon and several cells with varied charges. Drain weapon. Press `Shift-R`. Verify the picker shows cells sorted by charge descending. Pick a low-charge cell — verify it drains into the weapon. Repeat for shield with `b` and `Shift-B`.

- [ ] **Step 7: Commit**

```bash
git add include/astra/combat_system.h include/astra/ui_types.h src/game_combat.cpp src/game_input.cpp src/ui.cpp
git commit -m "feat(combat): add Shift-R/Shift-B cell pickers and b shield-recharge binding"
```

---

### Task 14: Inventory interact menu — "Recharge" and "Toggle Solar Panel"

**Files:**
- Modify: `src/game_interaction.cpp` — interact menu builder
- Modify: wherever interact entries are dispatched (search for the existing entries like "Equip", "Use", "Drop")

- [ ] **Step 1: Find the interact-menu builder**

Run: `grep -rn "Equip\|Drop\|Use\\b" /Users/jeffrey/dev/crawler/src/game_interaction.cpp | head -30`

Identify the function that adds entries based on item type.

- [ ] **Step 2: Add "Recharge" entry for any item with `EnergyStore`**

In the entry-builder (after Use / Equip), add:

```cpp
if (item.energy) {
    entries.push_back({"Recharge", InteractionAction::Recharge});
}
```

Add `Recharge` to the `InteractionAction` enum.

In the dispatcher:

```cpp
case InteractionAction::Recharge:
    combat_.begin_recharge_picker_for_item(game, &item);
    break;
```

`begin_recharge_picker_for_item` is a new variant of `begin_recharge_picker` that targets an arbitrary item rather than the equipped slot. Implement it analogously, using the item's `energy` directly.

- [ ] **Step 3: Add "Toggle Solar Panel" entry**

```cpp
for (auto& enh : item.enhancements) {
    if (enh.committed && enh.solar_panel) {
        entries.push_back({"Toggle Solar Panel", InteractionAction::ToggleSolarPanel});
        break;
    }
}
```

Dispatcher:

```cpp
case InteractionAction::ToggleSolarPanel:
    for (auto& enh : item.enhancements) {
        if (enh.committed && enh.solar_panel) {
            enh.solar_panel->active = !enh.solar_panel->active;
            game.log(std::string("Solar Panel ") + (enh.solar_panel->active ? "activated." : "deactivated."));
            break;
        }
    }
    break;
```

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: build succeeds.

- [ ] **Step 5: Sanity test**

Run: `./build/astra`. Spawn a partially drained Small Energy Cell with a Solar Panel installed. Open inventory, press space on the cell. Verify "Recharge" and "Toggle Solar Panel" entries appear. Toggle off — verify outdoor walking no longer refills the cell. Toggle on — verify it does.

- [ ] **Step 6: Commit**

```bash
git add include/astra/interaction.h src/game_interaction.cpp src/game_combat.cpp include/astra/combat_system.h
git commit -m "feat(interact): Recharge and Toggle Solar Panel entries on energy items"
```

---

### Task 15: Item label format — cells show `current/capacity charge`

**Files:**
- Modify: `include/astra/item.h:215-219` (`Item::label()`)

- [ ] **Step 1: Update `label()` to handle cells**

```cpp
std::string label() const {
    if (!damage_dice.empty())
        return name + " - " + damage_dice.to_string();
    if (type == ItemType::Battery && energy)
        return name + " - " + std::to_string(energy->current) + "/" +
               std::to_string(energy->capacity) + " charge";
    return name;
}
```

- [ ] **Step 2: Where the label is rendered with color, add the charge color**

`Item::label()` returns a plain string. The blue color around the numbers is applied at render-time. Find any inventory list renderer (`grep -rn "label()" /Users/jeffrey/dev/crawler/src | head`) — for cells, render the `name`, then render the ` - 32/60 charge` portion with the cyan `Color::Cyan` numbers.

If applying color requires segmenting the string, instead expose a helper:

```cpp
struct LabelSegment { std::string text; Color color; };
std::vector<LabelSegment> segmented_label() const;
```

(Or extend whatever colored-text helpers the inventory list already uses.) Implementation specifics depend on the existing inventory rendering; the simplest path is to special-case rendering for cells in the inventory list.

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: build succeeds.

- [ ] **Step 4: Sanity test**

Run: `./build/astra`, open inventory, verify the cell line reads `Small Energy Cell - 60/60 charge` with the numbers colored.

- [ ] **Step 5: Commit**

```bash
git add include/astra/item.h src/character_screen.cpp src/ui.cpp
git commit -m "feat(item): cell labels show current/capacity charge"
```

---

### Task 16: Bottom bar — rename "[r]eload" → "[r]echarge" and add shield section

**Files:**
- Modify: `src/game_rendering.cpp:1446-1505` (`render_effects_bar`)

- [ ] **Step 1: Rename the weapon hint string**

In the existing weapon hint block (around line 1486):

```cpp
        put(x, "]echarge ", Color::White);   // was "]eload "
```

Adjust the width budget at the top of the block:

```cpp
        // "[t]arget [s]hoot [r]echarge " = 28 chars
        int hint_width = 28 + ...
```

- [ ] **Step 2: Add shield section**

After the weapon hint block, before the section dividers, add:

```cpp
    // --- Shield hint (right of the ranged hint, or alone if no weapon) ---
    int shield_start = ranged_start;  // initial: same start; we may push left
    const auto* sh = player_.shield_energy();
    if (sh) {
        std::string scur = std::to_string(sh->current);
        std::string scap = std::to_string(sh->capacity);
        int shint_width = 5 + (int)scur.size() + 1 + (int)scap.size(); // "[b] N/M"
        int sx;
        if (rw && rw->energy) {
            // place to the LEFT of the weapon section, with separator before
            sx = ranged_start - shint_width - 2;
            shield_start = sx - 2;
            // separator between shield and weapon:
            ctx.text(ranged_start - 1, 0, "\xe2\x94\x82", Color::Black, bg);
            // recompute outer separator below
        } else {
            // alone: right-align like the weapon section
            sx = ctx.width() - shint_width - 1;
            shield_start = sx - 2;
        }
        const Color sc = (sh->current > 0) ? Color::Cyan : Color::Red;
        auto put = [&](int& x, std::string_view s, Color fg) {
            ctx.text(x, 0, s, fg, bg);
            x += static_cast<int>(s.size());
        };
        int x = sx;
        put(x, "[", Color::White);
        put(x, "b", Color::Yellow);
        put(x, "] ", Color::White);
        put(x, scur, sc);
        put(x, "/", Color::White);
        put(x, scap, sc);
    }
```

- [ ] **Step 3: Update the outer right separator placement**

```cpp
    const int right_sep_x = sh ? shield_start : ranged_start;
```

So the leftmost vertical bar appears just before whichever right-section is leftmost.

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: build succeeds.

- [ ] **Step 5: Sanity test**

Run: `./build/astra`. Equip only a ranged weapon — verify `[t]arget [s]hoot [r]echarge N/M` shows. Equip only a shield — verify `[b] N/M` shows alone. Equip both — verify both sections render with a separator between them.

- [ ] **Step 6: Commit**

```bash
git add src/game_rendering.cpp
git commit -m "feat(ui): bottom bar shows recharge for weapon and [b] shield section"
```

---

### Task 17: Auto-recharge on shoot routes through `recharge_weapon`

This was effectively done in Task 6. Verify and tighten the wording.

**Files:**
- Modify: `src/game_combat.cpp` (the auto-recharge inside the firing path)

- [ ] **Step 1: Verify the auto-recharge log line**

In the firing path, change the log lines so "auto-recharge" replaces any "auto-reload":

```cpp
game.log("Auto-recharged from cell.");
```

- [ ] **Step 2: Confirm the auto path doesn't double-log**

If `recharge_weapon` already emits a log line, the calling code should pass `log_full=false` and not log again. Make sure both paths produce one clean line per auto-event.

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: build succeeds.

- [ ] **Step 4: Sanity test**

Drain a weapon's last shot, confirm the next `s` (shoot) auto-recharges from inventory and fires in one action — single log line.

- [ ] **Step 5: Commit**

```bash
git add src/game_combat.cpp
git commit -m "polish(combat): clean up auto-recharge logging on shoot"
```

---

### Task 18: Look-mode and inventory-detail show energy info

**Files:**
- Modify: `src/character_screen.cpp` and / or wherever inventory detail is rendered (search for existing "Damage:", "Durability:" lines)

- [ ] **Step 1: Find the detail renderer**

Run: `grep -rn "Durability:\|Damage:" /Users/jeffrey/dev/crawler/src | head -10`

Locate the function that prints item stats (likely in `character_screen.cpp` or `ui.cpp`).

- [ ] **Step 2: Add energy lines**

After the existing stat lines, before durability:

```cpp
if (item.energy) {
    out_line("Charge:", std::to_string(item.energy->current) + "/" + std::to_string(item.energy->capacity));
}
if (item.consumer) {
    out_line("Energy/use:", std::to_string(item.consumer->energy_per_use));
}
// Mod summary
for (const auto& enh : item.enhancements) {
    if (!enh.committed) continue;
    if (enh.solar_panel) {
        const auto& sp = *enh.solar_panel;
        out_line("Mod:", std::string(sp.active ? "Solar Panel (active, +" : "Solar Panel (inactive, +") +
                          std::to_string(sp.energy_per_tick) + "/" +
                          std::to_string(sp.tick_interval) + " turns)");
    }
    if (enh.energy_bonus.capacity_bonus)
        out_line("Mod:", "+" + std::to_string(enh.energy_bonus.capacity_bonus) + " capacity");
    if (enh.energy_bonus.charge_rate_bonus)
        out_line("Mod:", "+" + std::to_string(enh.energy_bonus.charge_rate_bonus) + "% charge rate");
    if (enh.energy_bonus.discharge_efficiency)
        out_line("Mod:", "+1 free per " + std::to_string(enh.energy_bonus.discharge_efficiency) + " transferred");
}
```

(`out_line` stands in for whatever helper the detail renderer uses — copy the surrounding pattern.)

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: build succeeds.

- [ ] **Step 4: Sanity test**

Run: `./build/astra`, open inventory, hover/select a Plasma Pistol — detail panel shows `Charge: 20/20`, `Energy/use: 1`. Same for a panel-modded cell — shows the mod line.

- [ ] **Step 5: Commit**

```bash
git add src/character_screen.cpp src/ui.cpp
git commit -m "feat(ui): inventory detail shows energy charge, per-use cost, and installed mods"
```

---

### Task 19: Save schema bump and old-save rejection

**Files:**
- Modify: `include/astra/save_file.h:28`
- Modify: `src/save_file.cpp` — version-gate logic

- [ ] **Step 1: Bump the version constant**

```cpp
inline constexpr uint32_t SAVE_FILE_VERSION = 46;   // v46: energy system
```

- [ ] **Step 2: Reject old saves on load**

Find the load entry in `src/save_file.cpp` (search for `version` comparison and any per-version branching, around line 162-2134). Add at the top of the load:

```cpp
if (h.version < 46) {
    return SaveLoadResult{
        .ok = false,
        .message = "This save is from an older version (energy system update). "
                   "Old saves are no longer supported."
    };
}
```

(Match the actual `SaveLoadResult` shape used by the codebase — adjust message-passing accordingly.)

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: build succeeds.

- [ ] **Step 4: Sanity test**

Run: `./build/astra` with no existing save — start a new game, play, save, quit, reload — verify round-trip works on a v46 save (cell charges, panel state, equipped shield energy all preserved).

If you have a pre-v46 save lying around, attempt to load — verify the rejection message displays and the game returns to the menu.

- [ ] **Step 5: Commit**

```bash
git add include/astra/save_file.h src/save_file.cpp
git commit -m "feat(save): bump to v46; reject older saves (energy system schema)"
```

---

### Task 20: Update docs

**Files:**
- Modify: `docs/formulas.md`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Add Energy section to `docs/formulas.md`**

Append a section:

```markdown
## Energy System

**Cell tiers:**

| Tier | Capacity | Tinker Slots |
|---|---|---|
| Small Energy Cell | 60 | 1 |
| Standard Energy Cell | 150 | 1 |
| Large Energy Cell | 400 | 2 |
| Industrial Energy Cell | 800 | 3 |
| Antimatter Cell | 2000 | 3 |

**Weapon energy cost (per shot):**

| Weapon | Capacity | Cost |
|---|---|---|
| Plasma Pistol | 20 | 1 |
| Ion Blaster | 15 | 2 |
| Pulse Rifle | 30 | 2 |
| Arc Caster | 12 | 3 |
| Void Lance | 10 | 4 |

**Solar Panel rates** (only outdoors: overworld + detail map):

| Tier | energy/tick | tick interval |
|---|---|---|
| Solar Panel | 5 | 2 |
| Polished Solar Panel | 8 | 2 |
| Prismatic Solar Panel | 12 | 2 |

**Recharge action:** `r` recharges weapon, `b` recharges shield. `Shift-R` / `Shift-B` open a manual cell picker. Cells are drained highest-charge-first by default. Single-tick action cost.
```

- [ ] **Step 2: Tick the energy-system box in `docs/roadmap.md`**

Open `docs/roadmap.md`. Find the energy system entry (or add one if missing under a Combat / Items section). Mark complete:

```markdown
- [x] Energy system — persistent cells, Solar Panel mods, normalized recharge
```

- [ ] **Step 3: Commit**

```bash
git add docs/formulas.md docs/roadmap.md
git commit -m "docs: energy system formulas and roadmap update"
```

---

## Self-Review

### Spec coverage check

| Spec section | Task |
|---|---|
| Core data model — EnergyStore/Consumer/Modifiers/SolarPanelData | T1, T2, T3 |
| Cell tiers (5) | T5 |
| Solar Panel as tinkering enhancement (3 tiers) | T11 |
| Tinkering on cells (capacity/rate/efficiency) | T2 (struct), T11 (mat catalog) |
| Recharge `r` / `b` bindings | T6, T7, T13 |
| Manual picker `Shift-R` / `Shift-B` | T13 |
| Auto-recharge on shoot | T6, T17 |
| Inventory "Recharge" / "Toggle Solar Panel" entries | T14 |
| Bottom bar rename + shield section | T16 |
| Item label `current/max charge` for cells | T15 |
| Look / inventory detail shows energy info | T18 |
| Wording sweep (reload→recharge) | T6, T7 |
| NPCs out of scope | (no task — confirmed by absence of NPC changes) |
| Save schema bump, reject old | T19 |
| Outdoor gate (overworld + detail) | T10, T12 |
| Real game-tick handling (not keypress) | T12 (uses `cost` from `advance_world`) |
| Always tick (inventory + equipped) | T12 |
| File layout | All tasks aligned with the spec's file list |

No spec gaps detected.

### Placeholder scan

No "TBD" / "TODO" in the plan. Code blocks in every step. Function names consistent across tasks (`recharge_weapon`, `recharge_shield`, `transfer_energy`, `EnergySystem::tick`, `is_outdoor`).

### Type consistency

- `EnhancementSlot::stat_bonus` consistently used across T2, T11, T18.
- `EnergyModifiers::capacity_bonus` / `charge_rate_bonus` / `discharge_efficiency` consistently used in T1, T2, T8, T12, T18.
- `SolarPanelData` fields consistent: `active`, `energy_per_tick`, `tick_interval`, `accumulator`. Used in T1, T2, T11, T12, T14, T18.
- `MaterialEffect` extension in T11 matches the slot extension in T2.

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-04-25-energy-system.md`. Two execution options:

**1. Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration.

**2. Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints.

Which approach?
