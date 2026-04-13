# Dice-Based Combat System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace flat arithmetic combat with a dice-based two-layer defense system (DV/AV), five damage types with per-armor affinity, equippable energy shields, and dice expressions on all weapons.

**Architecture:** New `Dice` struct and `DamageType` enum in a dedicated header. `Item` gains dice, damage type, and type affinity fields. `Npc` gains DV, AV, affinities, dice, and damage type. `Player` derived stats switch from flat attack/defense to DV/AV calculations. `CombatSystem` is rewritten to use d20 attack rolls, d10 penetration rolls, and dice-based damage. New `Shield` equipment slot with its own HP pool recharged from batteries. Save version bumped to v26.

**Tech Stack:** C++20, no external dependencies beyond existing codebase

---

## File Structure

### New Files
- `include/astra/dice.h` — `Dice` struct (count, sides, modifier, roll, to_string, parse) and `DamageType` enum
- `src/dice.cpp` — `Dice::roll()`, `Dice::to_string()`, `Dice::parse()` implementations
- `include/astra/damage_type.h` — `DamageType` enum, `TypeAffinity` struct, helpers (if dice.h gets too large, but start combined)

### Modified Files
- `include/astra/item.h` — Add `Dice`, `DamageType`, `TypeAffinity` fields to `Item` and `StatModifiers`; add `Shield` equip slot; replace `attack`/`defense` in `StatModifiers` with `av`/`dv`
- `include/astra/npc.h` — Add `dv`, `av`, `damage_dice`, `damage_type`, `TypeAffinity` to `Npc`
- `include/astra/player.h` — Add `shield_hp`, `shield_max_hp`, `shield_type_affinities`; rewrite `effective_attack()` → `effective_dv()`, `effective_av(DamageType)`; remove `effective_defense()`; update `effective_dodge()` → use DV
- `include/astra/character.h` — Add `kinetic` to `Resistances`
- `include/astra/effect.h` — Update `StatModifiers` references (`attack`→something, `defense`→`av`)
- `include/astra/combat_system.h` — No structural changes needed (same public API)
- `include/astra/item_ids.h` — Add shield item IDs (41-46)
- `include/astra/item_defs.h` — Add shield builder declarations
- `include/astra/item_gen.h` — Add `TypeAffinity` to `ItemAffix`
- `src/game_combat.cpp` — Complete rewrite of attack/shoot/NPC turn logic
- `src/item_defs.cpp` — Update all weapon/armor defs with dice, damage type, affinities; add shield defs; update descriptions and display names
- `src/item_gen.cpp` — Update scaling to modify dice modifier; update affixes for AV/DV
- `src/npc.cpp` — Add combat stats to `scale_to_level()`
- `src/npcs/xytomorph.cpp` — Add DV, AV, dice, damage type, affinities
- `src/npcs/drifter.cpp` — Add DV, AV, dice, damage type, affinities
- `src/npcs/scavenger.cpp` — Add DV, AV, dice, damage type, affinities
- `src/npcs/prospector.cpp` — Add DV, AV, dice, damage type, affinities
- `src/ability.cpp` — Update all abilities to use weapon dice
- `src/effect.cpp` — Update `make_thick_skin()`, `make_defense_boost()` for AV; update `make_attack_boost()` concept
- `src/save_file.cpp` — Bump to v26; serialize new Item/Npc/Player fields
- `src/game_rendering.cpp` — Update status bar: AV/DV labels, shield HP display
- `src/character_screen.cpp` — Show damage type affinities, resistances, DV/AV in stats
- `src/ui.cpp` — Update enhancement bonus display
- `src/tinkering.cpp` — Update stat modifier references
- `docs/formulas.md` — Rewrite combat formulas section

---

### Task 1: Dice struct and DamageType enum

**Files:**
- Create: `include/astra/dice.h`
- Create: `src/dice.cpp`
- Modify: `CMakeLists.txt` (add `src/dice.cpp` to sources)

- [ ] **Step 1: Create `include/astra/dice.h`**

```cpp
#pragma once

#include <cstdint>
#include <random>
#include <string>

namespace astra {

enum class DamageType : uint8_t {
    Kinetic,
    Plasma,
    Electrical,
    Cryo,
    Acid,
};

constexpr int damage_type_count = 5;

const char* damage_type_name(DamageType t);

struct TypeAffinity {
    int kinetic = 0;
    int plasma = 0;
    int electrical = 0;
    int cryo = 0;
    int acid = 0;

    int for_type(DamageType t) const;
};

struct Dice {
    int count = 0;     // number of dice (e.g. 2 in "2d6+3")
    int sides = 0;     // sides per die (e.g. 6 in "2d6+3")
    int modifier = 0;  // flat modifier (e.g. 3 in "2d6+3")

    int roll(std::mt19937& rng) const;
    int min() const;
    int max() const;
    std::string to_string() const;
    bool empty() const { return count == 0 || sides == 0; }

    static Dice parse(const std::string& expr);
    static Dice make(int count, int sides, int modifier = 0);
};

} // namespace astra
```

- [ ] **Step 2: Create `src/dice.cpp`**

```cpp
#include "astra/dice.h"

#include <stdexcept>

namespace astra {

const char* damage_type_name(DamageType t) {
    switch (t) {
        case DamageType::Kinetic:    return "Kinetic";
        case DamageType::Plasma:     return "Plasma";
        case DamageType::Electrical: return "Electrical";
        case DamageType::Cryo:       return "Cryo";
        case DamageType::Acid:       return "Acid";
    }
    return "Unknown";
}

int TypeAffinity::for_type(DamageType t) const {
    switch (t) {
        case DamageType::Kinetic:    return kinetic;
        case DamageType::Plasma:     return plasma;
        case DamageType::Electrical: return electrical;
        case DamageType::Cryo:       return cryo;
        case DamageType::Acid:       return acid;
    }
    return 0;
}

int Dice::roll(std::mt19937& rng) const {
    if (count <= 0 || sides <= 0) return modifier;
    int total = 0;
    std::uniform_int_distribution<int> dist(1, sides);
    for (int i = 0; i < count; ++i) total += dist(rng);
    return total + modifier;
}

int Dice::min() const { return count + modifier; }
int Dice::max() const { return count * sides + modifier; }

std::string Dice::to_string() const {
    if (count <= 0 || sides <= 0) {
        return std::to_string(modifier);
    }
    std::string s = std::to_string(count) + "d" + std::to_string(sides);
    if (modifier > 0) s += "+" + std::to_string(modifier);
    else if (modifier < 0) s += std::to_string(modifier);
    return s;
}

Dice Dice::parse(const std::string& expr) {
    Dice d;
    // Format: NdS+M, NdS-M, NdS, or just M
    auto pos_d = expr.find('d');
    if (pos_d == std::string::npos) {
        d.modifier = std::stoi(expr);
        return d;
    }
    d.count = std::stoi(expr.substr(0, pos_d));
    auto rest = expr.substr(pos_d + 1);
    auto pos_plus = rest.find('+');
    auto pos_minus = rest.find('-');
    if (pos_plus != std::string::npos) {
        d.sides = std::stoi(rest.substr(0, pos_plus));
        d.modifier = std::stoi(rest.substr(pos_plus + 1));
    } else if (pos_minus != std::string::npos) {
        d.sides = std::stoi(rest.substr(0, pos_minus));
        d.modifier = -std::stoi(rest.substr(pos_minus + 1));
    } else {
        d.sides = std::stoi(rest);
    }
    return d;
}

Dice Dice::make(int count, int sides, int modifier) {
    return Dice{count, sides, modifier};
}

} // namespace astra
```

- [ ] **Step 3: Add `src/dice.cpp` to CMakeLists.txt**

Add `src/dice.cpp` to the source list in `CMakeLists.txt`, alongside the other `src/*.cpp` files.

- [ ] **Step 4: Build and verify**

```bash
cmake -B build -DDEV=ON && cmake --build build
```
Expected: Compiles with no errors.

- [ ] **Step 5: Commit**

```bash
git add include/astra/dice.h src/dice.cpp CMakeLists.txt
git commit -m "feat: add Dice struct and DamageType enum"
```

---

### Task 2: Update Item struct with dice, damage type, and type affinity

**Files:**
- Modify: `include/astra/item.h`
- Modify: `include/astra/character.h`

- [ ] **Step 1: Add `kinetic` resistance to `Resistances` in `character.h:31-36`**

Add `int kinetic = 0;` to the `Resistances` struct:

```cpp
struct Resistances {
    int kinetic = 0;    // KR
    int acid = 0;       // AR
    int electrical = 0; // ER
    int cold = 0;       // CR
    int heat = 0;       // HR
};
```

- [ ] **Step 2: Update `StatModifiers` in `item.h:124-130`**

Replace `attack` and `defense` with `av` (armor value) and `dv` (dodge value):

```cpp
struct StatModifiers {
    int av = 0;          // armor value modifier
    int dv = 0;          // dodge value modifier
    int max_hp = 0;
    int view_radius = 0;
    int quickness = 0;
};
```

- [ ] **Step 3: Add `Shield` to `EquipSlot` enum in `item.h:45-57`**

Add `Shield` after `Missile` and update `equip_slot_count`:

```cpp
enum class EquipSlot : uint8_t {
    Face,
    Head,
    Body,
    LeftArm,
    RightArm,
    LeftHand,
    RightHand,
    Back,
    Feet,
    Thrown,
    Missile,
    Shield,
};

static constexpr int equip_slot_count = 12;
```

- [ ] **Step 4: Add dice/damage/affinity fields to `Item` struct in `item.h:147-174`**

Add after the `ranged` field (line 167):

```cpp
    // Combat dice (weapons)
    Dice damage_dice;
    DamageType damage_type = DamageType::Kinetic;

    // Armor/shield type affinities
    TypeAffinity type_affinity;

    // Shield fields (only meaningful when slot == EquipSlot::Shield)
    int shield_capacity = 0;
    int shield_hp = 0;
```

Add `#include "astra/dice.h"` to the top of `item.h`.

- [ ] **Step 5: Add `shield` slot to `Equipment` struct in `item.h:182-198`**

Add `std::optional<Item> shield;` after `missile`.

- [ ] **Step 6: Fix all compilation errors from `attack`→`av` and `defense`→`dv` rename**

Search the entire codebase for `modifiers.attack` and `modifiers.defense` and rename:
- `modifiers.attack` → `modifiers.av` (note: for weapons this was the attack bonus — but in the new system weapons use dice, not flat attack. Weapons will set `av = 0` and use `damage_dice` instead. Armor sets `av` for its AV contribution.)
- `modifiers.defense` → `modifiers.dv` (armor DV modifier, negative for heavy armor)

Files to update:
- `src/item_defs.cpp` — all weapon and armor builders (weapon `.attack` → remove, armor `.defense` → `.av`)
- `src/item_gen.cpp` — `scale_item_to_level()` and affix pool
- `src/effect.cpp` — `effect_modifiers()`, `make_thick_skin()`, `make_defense_boost()`
- `src/ability.cpp` — doesn't use modifiers directly, skip
- `src/game_combat.cpp` — `effective_attack()` and `effective_defense()` calls
- `src/character_screen.cpp` — stat display
- `src/game_rendering.cpp` — status bar display
- `src/ui.cpp` — enhancement bonus display
- `src/tinkering.cpp` — stat modifier display
- `src/save_file.cpp` — `write_stat_modifiers` / `read_stat_modifiers`
- `include/astra/player.h` — `effective_attack()` and `effective_defense()` methods

**Important:** At this stage, set weapons' old `modifiers.attack` values to 0 (they now use `damage_dice`). Armor's old `modifiers.defense` becomes `modifiers.av`. Don't worry about full combat logic yet — just make it compile.

- [ ] **Step 7: Update `Equipment::slot_ref()` and `Equipment::total_modifiers()` to include shield slot**

In the source file that implements `slot_ref()` (search for `Equipment::slot_ref`), add the `Shield` case. In `total_modifiers()`, include the shield slot.

- [ ] **Step 8: Update `equip_slot_name()` to include `"Shield"` for the new slot**

- [ ] **Step 9: Build and verify**

```bash
cmake -B build -DDEV=ON && cmake --build build
```
Expected: Compiles. Many combat values will be wrong at this stage — that's fine.

- [ ] **Step 10: Commit**

```bash
git add -A
git commit -m "feat: add dice/damage-type/affinity fields to Item, add Shield equip slot"
```

---

### Task 3: Update NPC struct with combat stats

**Files:**
- Modify: `include/astra/npc.h`
- Modify: `src/npc.cpp`
- Modify: `src/npcs/xytomorph.cpp`
- Modify: `src/npcs/drifter.cpp`
- Modify: `src/npcs/scavenger.cpp`
- Modify: `src/npcs/prospector.cpp`

- [ ] **Step 1: Add combat fields to `Npc` struct in `npc.h:31-59`**

Add after `base_damage` (line 46):

```cpp
    // Dice combat stats
    int dv = 8;                                   // dodge value
    int av = 0;                                   // base armor value
    Dice damage_dice;                              // damage dice (replaces base_damage for new system)
    DamageType damage_type = DamageType::Kinetic;  // damage type
    TypeAffinity type_affinity;                    // per-damage-type AV modifiers
```

Add `#include "astra/dice.h"` to the top of `npc.h`.

- [ ] **Step 2: Update `Npc::attack_damage()` to use dice if available**

Replace the existing `attack_damage()` method:

```cpp
    int attack_damage() const { return base_damage * level + (elite ? 1 : 0); }
```

Keep this for backward compat, but combat system will use `damage_dice.roll()` directly.

- [ ] **Step 3: Update `scale_to_level()` in `npc.cpp:70-80`**

Add DV and AV scaling per level:

```cpp
void Npc::scale_to_level(int lvl, bool is_elite) {
    level = lvl;
    elite = is_elite;
    hp = hp * level;
    max_hp = hp;
    // Scale DV and AV slightly per level
    dv += (level - 1);
    av += (level - 1) / 2;
    if (elite) {
        hp *= 2;
        max_hp *= 2;
        quickness = quickness * 3 / 2;
        dv += 2;
        av += 1;
    }
}
```

- [ ] **Step 4: Set combat stats on Xytomorph in `src/npcs/xytomorph.cpp`**

After existing stat lines, add:

```cpp
    npc.dv = 8;
    npc.av = 4;
    npc.damage_dice = Dice::make(1, 6);
    npc.damage_type = DamageType::Acid;
    npc.type_affinity = {2, -2, 0, 0, 3}; // kinetic, plasma, electrical, cryo, acid
```

Add `#include "astra/dice.h"` to includes.

- [ ] **Step 5: Set combat stats on Drifter in `src/npcs/drifter.cpp`**

```cpp
    npc.dv = 10;
    npc.av = 2;
    npc.damage_dice = Dice::make(1, 4);
    npc.damage_type = DamageType::Kinetic;
    // No special affinities — all zeros by default
```

- [ ] **Step 6: Set combat stats on Scavenger in `src/npcs/scavenger.cpp`**

```cpp
    npc.dv = 9;
    npc.av = 3;
    npc.damage_dice = Dice::make(1, 6);
    npc.damage_type = DamageType::Kinetic;
    npc.type_affinity = {1, 0, -1, 0, 0};
```

- [ ] **Step 7: Set combat stats on Prospector in `src/npcs/prospector.cpp`**

```cpp
    npc.dv = 8;
    npc.av = 2;
    npc.damage_dice = Dice::make(1, 4, 1);
    npc.damage_type = DamageType::Kinetic;
```

- [ ] **Step 8: Build and verify**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

- [ ] **Step 9: Commit**

```bash
git add include/astra/npc.h src/npc.cpp src/npcs/*.cpp
git commit -m "feat: add DV/AV/dice/damage-type combat stats to NPCs"
```

---

### Task 4: Update Player derived stats for DV/AV

**Files:**
- Modify: `include/astra/player.h`

- [ ] **Step 1: Add shield HP fields and rewrite derived stat methods in `player.h`**

Add shield fields after `money` (line 53):

```cpp
    // Shield
    int shield_hp = 0;
    int shield_max_hp = 0;
    TypeAffinity shield_affinity;
```

Replace the derived stat methods (lines 103-121):

```cpp
    // DV = base_dv + AGI modifier + equipment DV + effect DV
    int effective_dv() const {
        auto eq = equipment.total_modifiers();
        auto ef = effect_modifiers(effects);
        return dodge_value + (attributes.agility - 10) / 2 + eq.dv + ef.dv;
    }

    // AV for a given damage type = sum of all equipment base AV + type affinities
    int effective_av(DamageType type) const {
        int total_av = 0;
        auto add_slot = [&](const std::optional<Item>& slot) {
            if (!slot) return;
            if (slot->type != ItemType::Armor) return;
            total_av += slot->modifiers.av + slot->type_affinity.for_type(type);
        };
        add_slot(equipment.head);
        add_slot(equipment.body);
        add_slot(equipment.left_arm);
        add_slot(equipment.right_arm);
        add_slot(equipment.feet);
        // Effects (ThickSkin, DefenseBoost add flat AV)
        auto ef = effect_modifiers(effects);
        total_av += ef.av;
        return total_av;
    }

    int effective_max_hp() const {
        auto eq = equipment.total_modifiers();
        auto ef = effect_modifiers(effects);
        return max_hp + (attributes.toughness - 10) * 2 + eq.max_hp + ef.max_hp;
    }
```

Remove `effective_attack()` and `effective_defense()` methods. Keep `attack_value` and `defense_value` fields for now (they'll be used as base values or removed later).

Add `#include "astra/dice.h"` at the top.

- [ ] **Step 2: Fix all compilation errors from removed `effective_attack()` and `effective_defense()`**

Files that call these methods:
- `src/game_combat.cpp` — will be fully rewritten in Task 6, but for now replace with placeholder `0` to compile
- `src/ability.cpp` — same, placeholder `0`
- `src/game_rendering.cpp` — update status bar to use `effective_dv()` and `effective_av(DamageType::Kinetic)` as representative values

The status bar line at `game_rendering.cpp:861-865` should become:

```cpp
    right.push_back({"AV:", UITag::TextDim});
    right.push_back({std::to_string(player_.effective_av(DamageType::Kinetic)), UITag::StatDefense});
    right.push_back({" :: ", UITag::TextDim});
    right.push_back({"DV:", UITag::TextDim});
    right.push_back({std::to_string(player_.effective_dv()), UITag::StatDefense});
```

- [ ] **Step 3: Build and verify**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

- [ ] **Step 4: Commit**

```bash
git add include/astra/player.h src/game_rendering.cpp src/game_combat.cpp src/ability.cpp
git commit -m "feat: replace effective_attack/defense with DV/AV on Player"
```

---

### Task 5: Update all item definitions

**Files:**
- Modify: `src/item_defs.cpp`
- Modify: `include/astra/item_ids.h`
- Modify: `include/astra/item_defs.h`

- [ ] **Step 1: Add shield item IDs to `item_ids.h`**

Add after the ship components block (after line 67):

```cpp
// Energy shields (41-46)
constexpr uint16_t ITEM_BASIC_DEFLECTOR    = 41;
constexpr uint16_t ITEM_PLASMA_SCREEN      = 42;
constexpr uint16_t ITEM_ION_BARRIER        = 43;
constexpr uint16_t ITEM_COMPOSITE_BARRIER  = 44;
constexpr uint16_t ITEM_HARDLIGHT_AEGIS    = 45;
constexpr uint16_t ITEM_VOID_MANTLE        = 46;
```

- [ ] **Step 2: Update all weapon definitions in `item_defs.cpp` with dice, damage type, and display names**

For each weapon builder, add `damage_dice`, `damage_type`, and update `name` to include dice notation. Remove the old `modifiers.attack` (set to 0 or remove the line since `av` defaults to 0).

**Melee weapons:**

`build_combat_knife()`:
```cpp
    it.name = "Combat Knife - 1d4";
    it.description = "A short, serrated blade. Deals kinetic damage at close range.";
    it.damage_dice = Dice::make(1, 4);
    it.damage_type = DamageType::Kinetic;
    // Remove: it.modifiers.attack = 2;
```

`build_stun_baton()`:
```cpp
    it.name = "Stun Baton - 1d4+1";
    it.description = "An electrified baton. Delivers electrical damage on contact.";
    it.damage_dice = Dice::make(1, 4, 1);
    it.damage_type = DamageType::Electrical;
    // Remove: it.modifiers.attack = 1;
```

`build_vibro_blade()`:
```cpp
    it.name = "Vibro Blade - 1d6+1";
    it.description = "A high-frequency vibrating blade. Kinetic damage cuts through armor.";
    it.damage_dice = Dice::make(1, 6, 1);
    it.damage_type = DamageType::Kinetic;
    // Remove: it.modifiers.attack = 4;
```

`build_plasma_saber()`:
```cpp
    it.name = "Plasma Saber - 2d4+2";
    it.description = "A long blade wreathed in superheated plasma. Devastating plasma damage.";
    it.damage_dice = Dice::make(2, 4, 2);
    it.damage_type = DamageType::Plasma;
    // Remove: it.modifiers.attack = 7;
```

`build_ancient_mono_edge()`:
```cpp
    it.name = "Ancient Mono-Edge - 2d6+2";
    it.description = "A relic blade from a lost civilization. Its molecular edge never dulls.";
    it.damage_dice = Dice::make(2, 6, 2);
    it.damage_type = DamageType::Kinetic;
    // Remove: it.modifiers.attack = 10;
```

**Ranged weapons:**

`build_plasma_pistol()`:
```cpp
    it.name = "Plasma Pistol - 1d6";
    it.description = "Standard-issue sidearm. Fires superheated plasma bolts.";
    it.damage_dice = Dice::make(1, 6);
    it.damage_type = DamageType::Plasma;
    // Remove: it.modifiers.attack = 3;
```

`build_ion_blaster()`:
```cpp
    it.name = "Ion Blaster - 1d8+1";
    it.description = "Disrupts electronics and shields with ionized electrical bursts.";
    it.damage_dice = Dice::make(1, 8, 1);
    it.damage_type = DamageType::Electrical;
    // Remove: it.modifiers.attack = 5;
```

`build_pulse_rifle()`:
```cpp
    it.name = "Pulse Rifle - 2d6";
    it.description = "Military-grade rifle with rapid kinetic energy pulses.";
    it.damage_dice = Dice::make(2, 6);
    it.damage_type = DamageType::Kinetic;
    // Remove: it.modifiers.attack = 8;
```

`build_arc_caster()`:
```cpp
    it.name = "Arc Caster - 2d8+1";
    it.description = "Channels electricity in a devastating arc. Unstable but powerful.";
    it.damage_dice = Dice::make(2, 8, 1);
    it.damage_type = DamageType::Electrical;
    // Remove: it.modifiers.attack = 12;
```

`build_void_lance()`:
```cpp
    it.name = "Void Lance - 3d8+2";
    it.description = "Fires a beam of compressed dark energy. Extremely rare plasma weapon.";
    it.damage_dice = Dice::make(3, 8, 2);
    it.damage_type = DamageType::Plasma;
    // Remove: it.modifiers.attack = 18;
```

- [ ] **Step 3: Update all armor definitions with AV, type affinities, and DV modifiers**

For each armor builder, replace `modifiers.defense` with `modifiers.av`, add `type_affinity`, and optionally set `modifiers.dv`.

`build_padded_vest()`:
```cpp
    it.description = "Basic torso protection. Lightweight composite padding.";
    it.modifiers.av = 2;
    it.type_affinity = {1, 0, 0, 0, -1}; // kinetic +1, acid -1
```

`build_composite_armor()`:
```cpp
    it.description = "Layered ceramic-polymer plates. Strong against kinetic, weak to acid.";
    it.modifiers.av = 4;
    it.modifiers.dv = -1;
    it.type_affinity = {2, -1, 0, 0, -2};
```

`build_exo_suit()`:
```cpp
    it.description = "Powered exoskeleton with integrated armor plating. Weak to electrical.";
    it.modifiers.av = 6;
    it.modifiers.dv = -2;
    it.type_affinity = {1, 1, -2, 1, 0};
```

`build_flight_helmet()`:
```cpp
    it.modifiers.av = 1;
    // No affinities — all zero by default
```

`build_tactical_helmet()`:
```cpp
    it.modifiers.av = 2;
    it.type_affinity = {1, 0, 0, -1, 0};
```

`build_combat_boots()`:
```cpp
    it.modifiers.av = 1;
```

`build_mag_lock_boots()`:
```cpp
    it.modifiers.av = 1;
```

`build_arm_guard()`:
```cpp
    it.modifiers.av = 1;
```

`build_riot_shield()` — **Remove this function entirely.** It is replaced by the energy shield system. Remove its entry from `random_armor()` and from merchant stock generation. Remove `ITEM_RIOT_SHIELD` usage from `random_armor()` in `item_defs.cpp`.

- [ ] **Step 4: Update grenade definitions with damage dice and types**

`build_frag_grenade()`:
```cpp
    it.damage_dice = Dice::make(2, 6);
    it.damage_type = DamageType::Kinetic;
    // Remove: it.modifiers.attack = 6;
```

`build_emp_grenade()`:
```cpp
    it.damage_dice = Dice::make(1, 8);
    it.damage_type = DamageType::Electrical;
    // Remove: it.modifiers.attack = 4;
```

`build_cryo_grenade()`:
```cpp
    it.damage_dice = Dice::make(2, 8);
    it.damage_type = DamageType::Cryo;
    // Remove: it.modifiers.attack = 8;
```

- [ ] **Step 5: Add shield item builders to `item_defs.cpp`**

Add at the end before the random selection section:

```cpp
// ---------------------------------------------------------------------------
// Energy shields
// ---------------------------------------------------------------------------

Item build_basic_deflector() {
    Item it;
    it.item_def_id = ITEM_BASIC_DEFLECTOR;
    it.id = 9001;
    it.name = "Basic Deflector";
    it.description = "Entry-level energy shield. Absorbs incoming damage.";
    it.type = ItemType::Shield;
    it.slot = EquipSlot::Shield;
    it.rarity = Rarity::Common;
    it.weight = 2;
    it.buy_value = 100;
    it.sell_value = 35;
    it.shield_capacity = 10;
    it.shield_hp = 10;
    return it;
}

Item build_plasma_screen() {
    Item it;
    it.item_def_id = ITEM_PLASMA_SCREEN;
    it.id = 9002;
    it.name = "Plasma Screen";
    it.description = "Tuned to absorb plasma energy. Reduces plasma damage to shield.";
    it.type = ItemType::Shield;
    it.slot = EquipSlot::Shield;
    it.rarity = Rarity::Uncommon;
    it.weight = 3;
    it.buy_value = 250;
    it.sell_value = 85;
    it.shield_capacity = 15;
    it.shield_hp = 15;
    it.type_affinity = {0, 50, 0, 0, 0}; // 50% plasma absorb bonus
    return it;
}

Item build_ion_barrier() {
    Item it;
    it.item_def_id = ITEM_ION_BARRIER;
    it.id = 9003;
    it.name = "Ion Barrier";
    it.description = "Ionized field that resists electrical attacks.";
    it.type = ItemType::Shield;
    it.slot = EquipSlot::Shield;
    it.rarity = Rarity::Uncommon;
    it.weight = 3;
    it.buy_value = 250;
    it.sell_value = 85;
    it.shield_capacity = 15;
    it.shield_hp = 15;
    it.type_affinity = {0, 0, 50, 0, 0};
    return it;
}

Item build_composite_barrier() {
    Item it;
    it.item_def_id = ITEM_COMPOSITE_BARRIER;
    it.id = 9004;
    it.name = "Composite Barrier";
    it.description = "Multi-layered shield effective against kinetic and plasma.";
    it.type = ItemType::Shield;
    it.slot = EquipSlot::Shield;
    it.rarity = Rarity::Rare;
    it.weight = 4;
    it.buy_value = 500;
    it.sell_value = 170;
    it.shield_capacity = 20;
    it.shield_hp = 20;
    it.type_affinity = {25, 25, 0, 0, 0};
    return it;
}

Item build_hardlight_aegis() {
    Item it;
    it.item_def_id = ITEM_HARDLIGHT_AEGIS;
    it.id = 9005;
    it.name = "Hardlight Aegis";
    it.description = "Projects a hardlight barrier. Strong all-round protection.";
    it.type = ItemType::Shield;
    it.slot = EquipSlot::Shield;
    it.rarity = Rarity::Epic;
    it.weight = 3;
    it.buy_value = 1200;
    it.sell_value = 400;
    it.shield_capacity = 30;
    it.shield_hp = 30;
    it.type_affinity = {25, 25, 25, 25, 25};
    return it;
}

Item build_void_mantle() {
    Item it;
    it.item_def_id = ITEM_VOID_MANTLE;
    it.id = 9006;
    it.name = "Void Mantle";
    it.description = "Wraps the bearer in a field of compressed void energy. Legendary.";
    it.type = ItemType::Shield;
    it.slot = EquipSlot::Shield;
    it.rarity = Rarity::Legendary;
    it.weight = 2;
    it.buy_value = 3000;
    it.sell_value = 1000;
    it.shield_capacity = 40;
    it.shield_hp = 40;
    it.type_affinity = {50, 50, 50, 50, 50};
    return it;
}
```

- [ ] **Step 6: Add shield builder declarations to `item_defs.h`**

Add after the armor section:

```cpp
// --- Energy shields ---
Item build_basic_deflector();
Item build_plasma_screen();
Item build_ion_barrier();
Item build_composite_barrier();
Item build_hardlight_aegis();
Item build_void_mantle();

// Random shield picker
Item random_shield(std::mt19937& rng);
```

- [ ] **Step 7: Add `random_shield()` to `item_defs.cpp`**

```cpp
Item random_shield(std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(0, 99);
    int roll = dist(rng);
    if (roll < 35) return build_basic_deflector();     // 35%
    if (roll < 60) return build_plasma_screen();        // 25%
    if (roll < 80) return build_ion_barrier();          // 20%
    if (roll < 92) return build_composite_barrier();    // 12%
    if (roll < 98) return build_hardlight_aegis();      //  6%
    return build_void_mantle();                         //  2%
}
```

- [ ] **Step 8: Remove riot shield from `random_armor()` and add shields to loot/merchant tables**

In `random_armor()`, remove the riot shield line (`if (roll < 97) return build_riot_shield();`). Redistribute its 5% to other armor.

In `generate_loot_drop()`, add a shield drop chance (e.g. 5% of the 25% armor becomes shield, or add a separate 5% shield category).

In `generate_merchant_stock()` and `generate_arms_dealer_stock()`, add a `random_shield(rng)` to the stock list.

- [ ] **Step 9: Build and verify**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

- [ ] **Step 10: Commit**

```bash
git add include/astra/item_ids.h include/astra/item_defs.h src/item_defs.cpp
git commit -m "feat: update all items with dice/damage-type, add energy shield items"
```

---

### Task 6: Rewrite CombatSystem with dice rolls

**Files:**
- Modify: `src/game_combat.cpp`

This is the core task. Replace all combat logic with the new dice system.

- [ ] **Step 1: Replace helper functions at top of `game_combat.cpp`**

Remove `roll_percent` and `npc_dodge_chance`. Add:

```cpp
// Roll 1d20 for attack. Returns the natural roll (1-20).
static int roll_d20(std::mt19937& rng) {
    return std::uniform_int_distribution<int>(1, 20)(rng);
}

// Roll 1d10 for penetration. Returns the natural roll (1-10).
static int roll_d10(std::mt19937& rng) {
    return std::uniform_int_distribution<int>(1, 10)(rng);
}

// Weapon skill bonus for attack rolls
static int weapon_skill_bonus(const Player& player, WeaponClass wc) {
    switch (wc) {
        case WeaponClass::ShortBlade:
            return player_has_skill(player, SkillId::ShortBladeExpertise) ? 2 : 0;
        case WeaponClass::LongBlade:
            return player_has_skill(player, SkillId::LongBladeExpertise) ? 2 : 0;
        case WeaponClass::Pistol:
            return player_has_skill(player, SkillId::SteadyHand) ? 2 : 0;
        case WeaponClass::Rifle:
            return player_has_skill(player, SkillId::Marksman) ? 2 : 0;
        default: return 0;
    }
}

// Apply resistance reduction (percentage-based, after penetration damage)
static int apply_resistance(int damage, DamageType type, const Resistances& res) {
    int pct = 0;
    switch (type) {
        case DamageType::Kinetic:    pct = res.kinetic; break;
        case DamageType::Plasma:     pct = res.heat; break;
        case DamageType::Electrical: pct = res.electrical; break;
        case DamageType::Cryo:       pct = res.cold; break;
        case DamageType::Acid:       pct = res.acid; break;
    }
    if (pct <= 0) return damage;
    return std::max(0, damage - damage * pct / 100);
}

// Calculate shield damage absorption considering type affinity.
// Returns how much shield HP is consumed (may be less than damage if affinity bonus).
static int shield_absorb(int damage, DamageType type, const TypeAffinity& affinity) {
    int bonus_pct = affinity.for_type(type);  // e.g. 50 means 50% more efficient
    if (bonus_pct > 0) {
        // Shield absorbs same damage but costs less shield HP
        return std::max(1, damage * 100 / (100 + bonus_pct));
    }
    return damage;
}

struct PenetrationResult {
    int total_damage = 0;
    int penetrations = 0;
};

// Roll penetration: 1d10 + STR mod vs effective AV. Multi-penetrate every +4.
static PenetrationResult roll_penetration(std::mt19937& rng, int str_mod,
                                           int effective_av, const Dice& damage_dice) {
    PenetrationResult result;
    int natural = roll_d10(rng);

    // Natural 1 always fails, natural 10 always penetrates
    if (natural == 1) return result;

    int pv = natural + str_mod;
    if (natural == 10 || pv > effective_av) {
        // First penetration
        result.penetrations = 1;
        result.total_damage = damage_dice.roll(rng);

        // Multi-penetration: each additional +4 over AV
        if (natural != 10) { // natural 10 gives exactly 1 penetration
            int excess = pv - effective_av;
            while (excess >= 4) {
                result.penetrations++;
                result.total_damage += damage_dice.roll(rng);
                excess -= 4;
            }
        }
    }
    return result;
}
```

- [ ] **Step 2: Rewrite `attack_npc()` (player melee attack)**

```cpp
void CombatSystem::attack_npc(Npc& npc, Game& game) {
    auto& rng = game.world().rng();
    auto& player = game.player();

    // Determine weapon and dice
    const auto& weapon = player.equipment.right_hand;
    Dice dice = weapon ? weapon->damage_dice : Dice::make(1, 3); // unarmed
    DamageType dtype = weapon ? weapon->damage_type : DamageType::Kinetic;
    WeaponClass wc = weapon ? weapon->weapon_class : WeaponClass::None;

    // Attack roll: 1d20 + AGI mod + weapon skill vs NPC DV
    int natural_attack = roll_d20(rng);
    int agi_mod = (player.attributes.agility - 10) / 2;
    int skill_bonus = weapon_skill_bonus(player, wc);
    int attack_roll = natural_attack + agi_mod + skill_bonus;
    int target_dv = npc.dv;

    // Natural 1 always misses, natural 20 always hits
    if (natural_attack == 1 || (natural_attack != 20 && attack_roll < target_dv)) {
        game.log("You miss " + npc.display_name() + ". (" +
                 std::to_string(attack_roll) + " vs DV " + std::to_string(target_dv) + ")");
        return;
    }

    game.log("You hit " + npc.display_name() + "! (" +
             std::to_string(attack_roll) + " vs DV " + std::to_string(target_dv) + ")");

    // Critical hit check
    bool is_crit = false;
    int crit_chance = std::clamp((player.attributes.luck - 8) * 2 + 3, 0, 30);
    if (std::uniform_int_distribution<int>(1, 100)(rng) <= crit_chance) {
        is_crit = true;
    }

    int damage = 0;
    if (is_crit) {
        // Auto-penetrate + roll damage twice
        damage = dice.roll(rng) + dice.roll(rng);
    } else {
        // Penetration roll
        int str_mod = (player.attributes.strength - 10) / 2;
        int effective_av = npc.av + npc.type_affinity.for_type(dtype);
        auto pen = roll_penetration(rng, str_mod, effective_av, dice);
        damage = pen.total_damage;
        if (damage <= 0) {
            game.log("Your attack bounces off " + npc.display_name() + "'s armor.");
            return;
        }
    }

    // Apply effects pipeline
    damage = apply_damage_effects(npc.effects, damage);
    if (damage <= 0) {
        game.log("Your attack has no effect on " + npc.display_name() + ".");
        return;
    }

    npc.hp -= damage;
    if (npc.hp < 0) npc.hp = 0;
    game.animations().spawn_effect(anim_damage_flash, npc.x, npc.y);

    std::string msg = is_crit ? "CRITICAL HIT! " : "";
    msg += "You strike " + npc.display_name() + " for " +
           std::to_string(damage) + " " + damage_type_name(dtype) + " damage!";
    game.log(msg);

    if (!npc.alive()) {
        game.log(npc.display_name() + " is destroyed!");
        game.player().kills++;
        if (!npc.faction.empty()) {
            for (auto& fs : game.player().reputation) {
                if (fs.faction_name == npc.faction) {
                    fs.reputation = std::max(fs.reputation - 30, -600);
                    game.log("Your reputation with " + npc.faction + " decreased.");
                    break;
                }
            }
        }
        game.quests().on_npc_killed(npc.role);
        int xp = npc.xp_reward();
        if (xp > 0) {
            game.player().xp += xp;
            game.log("You gain " + std::to_string(xp) + " XP.");
            check_level_up(game);
        }
        int credits = npc.level * 2 + (npc.elite ? 5 : 0);
        if (credits > 0) {
            game.player().money += credits;
            game.log("You salvage " + std::to_string(credits) + "$.");
        }
        if (std::uniform_int_distribution<int>(0, 1)(rng) == 0) {
            Item loot = generate_loot_drop(rng, npc.level);
            game.log("Dropped: " + loot.name);
            game.world().ground_items().push_back({npc.x, npc.y, std::move(loot)});
        }
    }
}
```

- [ ] **Step 3: Rewrite `process_npc_turn()` NPC-vs-player combat (lines 124-149)**

Replace the dodge/damage block within `if (dist <= 1)`:

```cpp
        if (dist <= 1) {
            // NPC attack roll: 1d20 + level/2 vs player DV
            int natural_attack = roll_d20(game.world().rng());
            int npc_attack_bonus = npc.level / 2;
            int attack_roll = natural_attack + npc_attack_bonus;
            int target_dv = game.player().effective_dv();

            if (natural_attack == 1 || (natural_attack != 20 && attack_roll < target_dv)) {
                game.log("You dodge " + npc.display_name() + "'s attack! (" +
                         std::to_string(attack_roll) + " vs DV " + std::to_string(target_dv) + ")");
                return;
            }

            Dice dice = npc.damage_dice.empty() ? Dice::make(1, 3) : npc.damage_dice;
            DamageType dtype = npc.damage_type;

            // Check shield
            if (game.player().shield_hp > 0) {
                // Penetrate as if AV=0, roll damage
                int raw_damage = dice.roll(game.world().rng());
                if (raw_damage < 1) raw_damage = 1;
                int absorbed = shield_absorb(raw_damage, dtype, game.player().shield_affinity);
                game.player().shield_hp -= absorbed;
                if (game.player().shield_hp < 0) game.player().shield_hp = 0;
                game.log(npc.display_name() + " hits your shield for " +
                         std::to_string(absorbed) + " damage. [Shield: " +
                         std::to_string(game.player().shield_hp) + "/" +
                         std::to_string(game.player().shield_max_hp) + "]");
                return;
            }

            // Penetration roll: 1d10 + npc level/3 vs player AV
            int str_mod = npc.level / 3;
            int effective_av = game.player().effective_av(dtype);
            auto pen = roll_penetration(game.world().rng(), str_mod, effective_av, dice);
            int damage = pen.total_damage;

            if (damage <= 0) {
                game.log(npc.display_name() + "'s attack bounces off your armor.");
                return;
            }

            // Apply resistance
            damage = apply_resistance(damage, dtype, game.player().resistances);
            damage = apply_damage_effects(game.player().effects, damage);
            if (damage <= 0) {
                game.log(npc.display_name() + " strikes you but deals no damage.");
                return;
            }

            game.player().hp -= damage;
            if (game.player().hp < 0) game.player().hp = 0;
            game.animations().spawn_effect(anim_damage_flash, game.player().x, game.player().y);
            game.log(npc.display_name() + " strikes you for " +
                     std::to_string(damage) + " " + damage_type_name(dtype) + " damage!");
            if (game.player().hp <= 0) {
                game.set_death_message("Slain by " + npc.display_name());
            }
            return;
        }
```

- [ ] **Step 4: Rewrite `attack_npc_vs_npc()` with dice rolls**

```cpp
void CombatSystem::attack_npc_vs_npc(Npc& attacker, Npc& defender, Game& game) {
    auto& rng = game.world().rng();

    // Attack roll: 1d20 + attacker level/2 vs defender DV
    int natural = roll_d20(rng);
    int attack_roll = natural + attacker.level / 2;

    if (natural == 1 || (natural != 20 && attack_roll < defender.dv)) {
        game.log(defender.display_name() + " dodges " + attacker.display_name() + "'s attack!");
        return;
    }

    Dice dice = attacker.damage_dice.empty() ? Dice::make(1, 3) : attacker.damage_dice;
    DamageType dtype = attacker.damage_type;

    int str_mod = attacker.level / 3;
    int effective_av = defender.av + defender.type_affinity.for_type(dtype);
    auto pen = roll_penetration(rng, str_mod, effective_av, dice);
    int damage = pen.total_damage;

    if (damage <= 0) {
        game.log(attacker.display_name() + "'s attack bounces off " + defender.display_name() + ".");
        return;
    }

    damage = apply_damage_effects(defender.effects, damage);
    if (damage <= 0) {
        game.log(attacker.display_name() + "'s attack has no effect on " + defender.display_name() + ".");
        return;
    }

    defender.hp -= damage;
    if (defender.hp < 0) defender.hp = 0;
    game.animations().spawn_effect(anim_damage_flash, defender.x, defender.y);
    game.log(attacker.display_name() + " strikes " + defender.display_name() +
             " for " + std::to_string(damage) + " damage!");
    if (!defender.alive()) {
        game.log(defender.display_name() + " is destroyed by " + attacker.display_name() + "!");
    }
}
```

- [ ] **Step 5: Rewrite `shoot_target()` with dice rolls**

Same pattern as `attack_npc()` but using the missile slot weapon, range checks, and charge system. The core combat roll is identical:

- Attack roll: `1d20 + AGI mod + weapon skill` vs `target_npc DV`
- Shield check if target has shield HP (for NPCs this won't apply yet, but structure it)
- Penetration roll: `1d10 + STR mod` vs `NPC AV + type affinity`
- Critical hit: auto-penetrate + double dice
- Damage type from weapon

Keep the existing charge/reload logic unchanged. Keep the projectile animation.

- [ ] **Step 6: Build and verify**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

- [ ] **Step 7: Commit**

```bash
git add src/game_combat.cpp
git commit -m "feat: rewrite combat system with d20 attack, d10 penetration, dice damage"
```

---

### Task 7: Update abilities to use weapon dice

**Files:**
- Modify: `src/ability.cpp`

- [ ] **Step 1: Update JabAbility::execute()**

Replace `game.player().effective_attack() / 2` with rolling the weapon's dice at half value:

```cpp
    bool execute(Game& game, Npc* target) override {
        const auto& weapon = game.player().equipment.right_hand;
        Dice dice = weapon ? weapon->damage_dice : Dice::make(1, 3);
        DamageType dtype = weapon ? weapon->damage_type : DamageType::Kinetic;

        int damage = dice.roll(game.world().rng()) / 2;
        if (damage < 1) damage = 1;
        damage = apply_damage_effects(target->effects, damage);
        if (damage > 0) {
            target->hp -= damage;
            if (target->hp < 0) target->hp = 0;
            game.log("You jab " + target->display_name() + " for " +
                     std::to_string(damage) + " " + damage_type_name(dtype) + " damage!");
        }
        return true;
    }
```

- [ ] **Step 2: Update CleaveAbility::execute()**

Replace `game.player().effective_attack()` with rolling weapon dice:

```cpp
        const auto& weapon = game.player().equipment.right_hand;
        Dice dice = weapon ? weapon->damage_dice : Dice::make(1, 3);
        DamageType dtype = weapon ? weapon->damage_type : DamageType::Kinetic;
        // ... in the loop:
        int damage = dice.roll(game.world().rng());
```

- [ ] **Step 3: Update QuickdrawAbility::execute()**

Use missile slot weapon dice:

```cpp
        const auto& weapon = game.player().equipment.missile;
        Dice dice = weapon ? weapon->damage_dice : Dice::make(1, 3);
        DamageType dtype = weapon ? weapon->damage_type : DamageType::Kinetic;
        int damage = dice.roll(game.world().rng());
```

- [ ] **Step 4: Build and verify**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

- [ ] **Step 5: Commit**

```bash
git add src/ability.cpp
git commit -m "feat: update abilities to use weapon dice instead of flat damage"
```

---

### Task 8: Update item generation and affix system

**Files:**
- Modify: `src/item_gen.cpp`
- Modify: `include/astra/item_gen.h`

- [ ] **Step 1: Update `ItemAffix` struct in `item_gen.h`**

The `StatModifiers` change from `attack`/`defense` to `av`/`dv` already happened in Task 2. Now update the affix pool arrays in `item_gen.cpp` to use the new field names:

Replace all `{attack, defense, max_hp, view_radius, quickness}` initializers with `{av, dv, max_hp, view_radius, quickness}`:

```cpp
static const ItemAffix s_prefixes[] = {
    {"Corroded",     true, {0, -1, 0, 0, 0}, -5,  0.7f},   // AV -1
    {"Rusted",       true, {-1, 0, 0, 0, 0}, -3,  0.6f},   // AV -1 (was ATK -2)
    {"Reinforced",   true, {2, 0, 0, 0, 0},   5,  1.3f},   // AV +2
    {"Overcharged",  true, {0, 1, 0, 0, 0},  -5,  1.2f},   // DV +1 (was ATK +3)
    {"Pristine",     true, {0, 0, 0, 0, 0},  10,  1.1f},   // durability only
    {"Salvaged",     true, {-1, -1, 0, 0, 0}, 0,  0.5f},   // AV -1, DV -1
    {"Military",     true, {2, 1, 0, 0, 0},   0,  1.5f},   // AV +2, DV +1
    {"Prototype",    true, {0, 2, 0, 0, 0},   0,  1.4f},   // DV +2 (was ATK +4, DEF -2)
    {"Ancient",      true, {2, 0, 0, 2, 0},   0,  2.0f},   // AV +2, view +2
    {"Nano-Enhanced",true, {2, 1, 0, 0, 0},   0,  1.8f},   // AV +2, DV +1
};

static const ItemAffix s_suffixes[] = {
    {"of Precision",    false, {0, 1, 0, 0, 0},   0, 1.3f},   // DV +1
    {"of the Void",     false, {0, 0, 0, 2, 0},   0, 1.2f},   // view +2
    {"of Endurance",    false, {0, 0, 2, 0, 0},   0, 1.2f},   // HP +2
    {"of Speed",        false, {0, 0, 0, 0, 3},   0, 1.3f},   // quickness +3
    {"of the Ancients", false, {2, 1, 0, 1, 0},   0, 2.0f},   // AV +2, DV +1, view +1
    {"of Salvage",      false, {-1, 0, 0, 0, 0},  0, 0.6f},   // AV -1
    {"of Protection",   false, {3, 0, 0, 0, 0},   0, 1.4f},   // AV +3
    {"of the Swarm",    false, {0, 0, 0, 0, 1},   0, 1.1f},   // quickness +1
    {"of Decay",        false, {0, 0, 0, 0, 0}, -10, 0.5f},   // durability only
    {"of the Stars",    false, {1, 0, 1, 0, 0},   0, 1.5f},   // AV +1, HP +1
};
```

- [ ] **Step 2: Update `scale_item_to_level()` for dice-based items**

Instead of scaling `modifiers.attack`, scale the dice modifier:

```cpp
void scale_item_to_level(Item& item, int level) {
    if (level <= 1) return;
    float mult = 1.0f + (level - 1) * 0.15f;

    auto scale = [mult](int v) -> int {
        return static_cast<int>(std::round(v * mult));
    };

    item.item_level = level;
    // Scale dice modifier (bonus damage per level)
    item.damage_dice.modifier = scale(item.damage_dice.modifier);
    // Scale AV and DV
    item.modifiers.av = scale(item.modifiers.av);
    item.modifiers.dv = scale(item.modifiers.dv);
    item.modifiers.max_hp = scale(item.modifiers.max_hp);
    item.buy_value = scale(item.buy_value);
    item.sell_value = scale(item.sell_value);
    item.max_durability = scale(item.max_durability);
    item.durability = item.max_durability;

    if (item.ranged) {
        item.ranged->charge_capacity = scale(item.ranged->charge_capacity);
        item.ranged->current_charge = item.ranged->charge_capacity;
    }

    // Scale shield capacity
    if (item.shield_capacity > 0) {
        item.shield_capacity = scale(item.shield_capacity);
        item.shield_hp = item.shield_capacity;
    }
}
```

- [ ] **Step 3: Update weapon display name after affixes**

After affixes are applied in `apply_rarity_affixes()`, update the weapon's display name to include the (possibly modified) dice. Add after `apply_affix` calls:

```cpp
    // Update weapon display name with dice
    if (!item.damage_dice.empty()) {
        // Find and replace dice suffix in name, or append
        auto dash = item.name.rfind(" - ");
        if (dash != std::string::npos) {
            item.name = item.name.substr(0, dash);
        }
        item.name += " - " + item.damage_dice.to_string();
    }
```

This should go at the end of `apply_rarity_affixes()`, after all affixes have been applied.

- [ ] **Step 4: Build and verify**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

- [ ] **Step 5: Commit**

```bash
git add src/item_gen.cpp include/astra/item_gen.h
git commit -m "feat: update affix system and level scaling for dice-based items"
```

---

### Task 9: Update effects system

**Files:**
- Modify: `src/effect.cpp`
- Modify: `include/astra/effect.h`

- [ ] **Step 1: Update effect factories that reference attack/defense**

In `effect.cpp`:

`make_thick_skin()`: Change `e.modifiers.defense = 1;` → `e.modifiers.av = 1;`

`make_defense_boost()`: Change `e.modifiers.defense = amount;` → `e.modifiers.av = amount;`

`make_attack_boost()`: This effect boosted flat attack. In the new system, there's no flat attack modifier. Change it to boost DV (evasion boost while empowered) or keep as a damage multiplier effect. Simplest: change to `e.modifiers.dv = amount;` and rename to "Focused" or keep as "Empowered" with a damage multiplier.

Alternative: use the existing `damage_multiplier` field on Effect. Set `e.damage_multiplier = 100 + amount * 10;` (e.g. +1 = 110% damage). But this is a receiving-damage multiplier, not dealing. For now, convert to a DV boost:

```cpp
Effect make_attack_boost(int duration, int amount) {
    Effect e;
    e.id = EffectId::AttackBoost;
    e.name = "Focused";
    e.color = Color::Yellow;
    e.duration = duration;
    e.remaining = duration;
    e.show_in_bar = false;
    e.dodge_mod = amount;  // temporary DV boost
    return e;
}
```

- [ ] **Step 2: Update `effect_modifiers()` for new field names**

The `StatModifiers` fields changed from `attack`/`defense` to `av`/`dv`. Update the aggregation:

```cpp
StatModifiers effect_modifiers(const EffectList& effects) {
    StatModifiers total;
    for (const auto& e : effects) {
        total.av += e.modifiers.av;
        total.dv += e.modifiers.dv;
        total.max_hp += e.modifiers.max_hp;
        total.view_radius += e.modifiers.view_radius;
        total.quickness += e.modifiers.quickness;
    }
    return total;
}
```

- [ ] **Step 3: Build and verify**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

- [ ] **Step 4: Commit**

```bash
git add src/effect.cpp include/astra/effect.h
git commit -m "feat: update effects for AV/DV stat modifiers"
```

---

### Task 10: Shield recharge from batteries

**Files:**
- Modify: `src/game_combat.cpp` (add `reload_shield` method)
- Modify: `include/astra/combat_system.h` (add `reload_shield` declaration)
- Modify: wherever the 'r' key or reload keybind is handled (likely `src/game_input.cpp` or similar)

- [ ] **Step 1: Add `reload_shield()` method to `CombatSystem`**

In `combat_system.h`, add:
```cpp
    void reload_shield(Game& game);
```

In `game_combat.cpp`, add:
```cpp
void CombatSystem::reload_shield(Game& game) {
    auto& shield = game.player().equipment.shield;
    if (!shield || shield->shield_capacity <= 0) {
        game.log("No energy shield equipped.");
        return;
    }

    if (game.player().shield_hp >= game.player().shield_max_hp) {
        game.log("Shield is at full charge.");
        return;
    }

    for (int i = 0; i < static_cast<int>(game.player().inventory.items.size()); ++i) {
        if (game.player().inventory.items[i].type == ItemType::Battery) {
            int added = std::min(5, game.player().shield_max_hp - game.player().shield_hp);
            game.player().shield_hp += added;
            game.log("Shield recharged +" + std::to_string(added) + " HP. (" +
                     std::to_string(game.player().shield_hp) + "/" +
                     std::to_string(game.player().shield_max_hp) + ")");
            auto& cell = game.player().inventory.items[i];
            if (cell.stackable && cell.stack_count > 1) {
                --cell.stack_count;
            } else {
                game.player().inventory.items.erase(game.player().inventory.items.begin() + i);
            }
            game.advance_world(ActionCost::wait);
            return;
        }
    }

    game.log("No energy cells to recharge shield.");
}
```

- [ ] **Step 2: Update shield HP when equipping/unequipping shield items**

Find where equipment changes are handled (likely in `src/character_screen.cpp` or an equip handler). When a shield is equipped, set:
```cpp
player.shield_max_hp = shield_item.shield_capacity;
player.shield_hp = shield_item.shield_hp;  // preserve charge from item
player.shield_affinity = shield_item.type_affinity;
```

When unequipped, save the current charge back to the item and zero out player shield:
```cpp
shield_item.shield_hp = player.shield_hp;
player.shield_hp = 0;
player.shield_max_hp = 0;
player.shield_affinity = {};
```

Search for where equip/unequip happens to find the exact location.

- [ ] **Step 3: Build and verify**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

- [ ] **Step 4: Commit**

```bash
git add src/game_combat.cpp include/astra/combat_system.h
git commit -m "feat: add shield recharge from batteries"
```

---

### Task 11: Update save system (v26)

**Files:**
- Modify: `src/save_file.cpp`
- Modify: `include/astra/save_file.h`

- [ ] **Step 1: Bump save version to 26 in `save_file.h:62`**

Change `uint32_t version = 25;` → `uint32_t version = 26;`

- [ ] **Step 2: Update `write_stat_modifiers` / `read_stat_modifiers`**

Find these functions in `save_file.cpp` and update field names from `attack`/`defense` to `av`/`dv`.

- [ ] **Step 3: Update `write_item` / `read_item` to serialize new fields**

After existing fields, add for v26:

```cpp
// In write_item, after ship component fields:
    // v26: dice combat fields
    w.write_u8(static_cast<uint8_t>(item.damage_type));
    w.write_i32(item.damage_dice.count);
    w.write_i32(item.damage_dice.sides);
    w.write_i32(item.damage_dice.modifier);
    // Type affinity
    w.write_i32(item.type_affinity.kinetic);
    w.write_i32(item.type_affinity.plasma);
    w.write_i32(item.type_affinity.electrical);
    w.write_i32(item.type_affinity.cryo);
    w.write_i32(item.type_affinity.acid);
    // Shield fields
    w.write_i32(item.shield_capacity);
    w.write_i32(item.shield_hp);
```

In `read_item`, after ship component fields:
```cpp
    // v26: dice combat fields
    if (version >= 26) {
        item.damage_type = static_cast<DamageType>(r.read_u8());
        item.damage_dice.count = r.read_i32();
        item.damage_dice.sides = r.read_i32();
        item.damage_dice.modifier = r.read_i32();
        item.type_affinity.kinetic = r.read_i32();
        item.type_affinity.plasma = r.read_i32();
        item.type_affinity.electrical = r.read_i32();
        item.type_affinity.cryo = r.read_i32();
        item.type_affinity.acid = r.read_i32();
        item.shield_capacity = r.read_i32();
        item.shield_hp = r.read_i32();
    }
```

- [ ] **Step 4: Update NPC serialization**

In the NPC write section, add after existing fields:
```cpp
    // v26: dice combat stats
    w.write_i32(npc.dv);
    w.write_i32(npc.av);
    w.write_i32(npc.damage_dice.count);
    w.write_i32(npc.damage_dice.sides);
    w.write_i32(npc.damage_dice.modifier);
    w.write_u8(static_cast<uint8_t>(npc.damage_type));
    w.write_i32(npc.type_affinity.kinetic);
    w.write_i32(npc.type_affinity.plasma);
    w.write_i32(npc.type_affinity.electrical);
    w.write_i32(npc.type_affinity.cryo);
    w.write_i32(npc.type_affinity.acid);
```

In the NPC read section:
```cpp
    if (version >= 26) {
        npc.dv = r.read_i32();
        npc.av = r.read_i32();
        npc.damage_dice.count = r.read_i32();
        npc.damage_dice.sides = r.read_i32();
        npc.damage_dice.modifier = r.read_i32();
        npc.damage_type = static_cast<DamageType>(r.read_u8());
        npc.type_affinity.kinetic = r.read_i32();
        npc.type_affinity.plasma = r.read_i32();
        npc.type_affinity.electrical = r.read_i32();
        npc.type_affinity.cryo = r.read_i32();
        npc.type_affinity.acid = r.read_i32();
    }
```

- [ ] **Step 5: Update player serialization for shield HP and kinetic resistance**

Add shield HP fields and kinetic resistance to player write/read:

```cpp
    // v26: shield HP
    w.write_i32(p.shield_hp);
    w.write_i32(p.shield_max_hp);
    // shield_affinity stored on equipped shield item, not separately
```

In read:
```cpp
    if (version >= 26) {
        p.shield_hp = r.read_i32();
        p.shield_max_hp = r.read_i32();
    }
```

Also add kinetic resistance to the resistances write/read block.

- [ ] **Step 6: Update Equipment serialization for Shield slot**

In `write_equipment`, add: `write_optional_item(w, eq.shield);`
In `read_equipment`, add: `eq.shield = read_optional_item(r, version);` (for v26+)

- [ ] **Step 7: Build and verify**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

- [ ] **Step 8: Commit**

```bash
git add src/save_file.cpp include/astra/save_file.h
git commit -m "feat(save): bump to v26, serialize dice combat fields"
```

---

### Task 12: Update UI displays

**Files:**
- Modify: `src/game_rendering.cpp`
- Modify: `src/character_screen.cpp`
- Modify: `src/ui.cpp`

- [ ] **Step 1: Add shield HP to the status bar in `game_rendering.cpp`**

In the left-side status bar (near the HP display), add shield HP if the player has a shield equipped. Find the HP display section and add after it:

```cpp
    if (player_.shield_max_hp > 0) {
        left.push_back({" :: ", UITag::TextDim});
        left.push_back({"SH:", UITag::TextDim});
        left.push_back({std::to_string(player_.shield_hp) + "/" +
                        std::to_string(player_.shield_max_hp), UITag::StatMana});
    }
```

Use an appropriate UITag for shield (Cyan/Blue-ish).

- [ ] **Step 2: Update enhancement bonus display in `ui.cpp`**

Change `enh.bonus.attack` → `enh.bonus.av` and `enh.bonus.defense` → `enh.bonus.dv`:

```cpp
    if (enh.bonus.av) bonus += " AV+" + std::to_string(enh.bonus.av);
    if (enh.bonus.dv) bonus += " DV+" + std::to_string(enh.bonus.dv);
```

- [ ] **Step 3: Update character screen stats display**

Find where the character screen displays attack/defense values and update to show DV, AV (for kinetic as representative), and damage type resistances. This will be in `src/character_screen.cpp` in the attributes or equipment tab rendering.

- [ ] **Step 4: Build and verify**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

- [ ] **Step 5: Commit**

```bash
git add src/game_rendering.cpp src/character_screen.cpp src/ui.cpp
git commit -m "feat(ui): show shield HP, DV/AV, and damage types in UI"
```

---

### Task 13: Update tinkering system

**Files:**
- Modify: `src/tinkering.cpp`

- [ ] **Step 1: Update any references to `attack`/`defense` in stat modifier displays**

Search `tinkering.cpp` for `modifiers.attack` or `modifiers.defense` and replace with `modifiers.av` / `modifiers.dv`. Also update any display strings from "ATK" to "AV" and "DEF" to "DV".

- [ ] **Step 2: Build and verify**

```bash
cmake -B build -DDEV=ON && cmake --build build
```

- [ ] **Step 3: Commit**

```bash
git add src/tinkering.cpp
git commit -m "fix: update tinkering stat display for AV/DV"
```

---

### Task 14: Update formulas documentation

**Files:**
- Modify: `docs/formulas.md`

- [ ] **Step 1: Rewrite the combat formulas section**

Replace the existing combat formulas with the new system. Include:

- Attack roll formula: `1d20 + (AGI-10)/2 + weapon_skill_bonus >= target DV`
- DV formula: `base_dv + (AGI-10)/2 + equipment_dv + effect_dv`
- Penetration formula: `1d10 + (STR-10)/2 vs effective_AV`
- Effective AV: `sum(armor.av) + sum(armor.type_affinity[damage_type])`
- Multi-penetration: each +4 over AV = additional damage dice roll
- Critical hit: `chance = clamp((LUC-8)*2+3, 0, 30)%`, effect = auto-penetrate + 2x damage dice
- Shield absorption: damage reduces shield HP, affinity reduces HP cost
- Resistance: percentage reduction after penetration damage
- Natural roll rules: d20 nat 20/1, d10 nat 10/1
- NPC attack bonus: `level / 2`
- NPC penetration bonus: `level / 3`
- NPC scaling: DV + (level-1), AV + (level-1)/2

- [ ] **Step 2: Commit**

```bash
git add docs/formulas.md
git commit -m "docs: rewrite combat formulas for dice-based system"
```

---

### Task 15: Final build verification and cleanup

- [ ] **Step 1: Full clean build**

```bash
rm -rf build && cmake -B build -DDEV=ON && cmake --build build
```

- [ ] **Step 2: Run the game and verify**

```bash
./build/astra
```

Manual checks:
- Start a new game, check status bar shows AV/DV values
- Equip a weapon, verify dice notation in name
- Find and fight an NPC, verify combat log shows roll details (attack roll vs DV, penetration vs AV, dice damage)
- Check character screen shows new stats
- Equip a shield (if available in starting gear or via dev commands), verify shield HP displays
- Save and reload, verify no crashes

- [ ] **Step 3: Commit any final fixes**

```bash
git add -A
git commit -m "fix: final cleanup for dice combat system"
```
