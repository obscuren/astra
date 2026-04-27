# Tinkering Expansion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expand the tinkering system: 24-material catalog across 3 tiers, junk dual-use + refinement pipeline, revised synthesis recipes, schematic-based crafting for 16 consumable items (stims/grenades/mines, all inert until next spec), tier-weighted salvage, and a canonical `docs/tinkering.md` reference.

**Architecture:** A single `MaterialDef` catalog drives everything: salvage, refinement, synthesis, schematic crafting, loot table tier buckets, and the docs table. The fixed `material_cost[4]` array is replaced by `std::vector<MaterialReq>` shared by all three recipe types (`SynthesisRecipe`, `RefinementRecipe`, `SchematicRecipe`). `ItemType` gains `Schematic` (mirrors `Cookbook`) and `Mine`; existing `Stim`/`Grenade` are reused. A new save schema version (49) bumps to add `learned_schematics`.

**Tech Stack:** C++20, CMake, terminal renderer. No unit-test framework in the project — verification is build-clean + `--dev` mode `give item` smoke tests + manual workbench flow. Build with `-DDEV=ON`.

**Plan adaptation note vs spec §1.4:** spec proposes adding `Schematic` and `Consumable` to `ItemType`, but `Stim` and `Grenade` already exist. The plan instead adds `Schematic` and `Mine` (no `Consumable`); stims use `ItemType::Stim`, grenades use `ItemType::Grenade`, mines use new `ItemType::Mine`. Existing Frag Grenade (def 27), EMP Grenade (def 28), and Combat Stim (def 8) are **reused** as the output items for the matching schematic recipes (Combat Stim reframed/renamed to Adrenaline Stim — same id, updated name/description). 13 new consumable defs + 16 new schematic defs added.

**Build command (used throughout):** `cmake --build build` (assumes a configured build dir with `-DDEV=ON`; first task configures it).

---

## Phase 1 — Data Model Foundation (existing recipes still build & run)

### Task 1: Configure dev build, verify clean baseline

**Files:**
- No code changes.

- [ ] **Step 1: Configure build with dev mode**

```bash
cmake -B build -DDEV=ON
```

Expected: configure succeeds, no errors.

- [ ] **Step 2: Build clean**

```bash
cmake --build build
```

Expected: build succeeds, `./build/astra-dev` exists.

- [ ] **Step 3: Smoke-run the binary briefly**

```bash
./build/astra-dev --term --help 2>&1 | head -5 || true
```

Expected: prints help / usage or starts and exits cleanly. (The binary may not have `--help`; if it tries to launch into the game, kill with Ctrl-C — this just verifies the binary runs.)

No commit.

---

### Task 2: Add `MaterialReq`, `MaterialTier`, `MaterialDef` types and stub catalog

**Files:**
- Modify: `include/astra/tinkering.h` (add types only; do not change `SynthesisRecipe` yet)
- Modify: `src/tinkering.cpp` (add `material_catalog()` returning empty vector + `find_material()` stub)

- [ ] **Step 1: Edit `include/astra/tinkering.h` — add types above the existing `BlueprintSignature`**

Locate the existing `namespace astra {` opener and the existing `struct BlueprintSignature` declaration. Insert this block immediately after `namespace astra {` and before `struct Player; // forward declare`:

```cpp
// --- Materials ----------------------------------------------------------

struct MaterialReq {
    uint32_t material_id = 0;   // == Item::id used for inventory stack matching
    int count = 0;
};

enum class MaterialTier : uint8_t {
    Common = 1,
    Uncommon = 2,
    Rare = 3,
};

struct MaterialDef {
    uint32_t material_id = 0;       // Item::id
    const char* name = "";
    MaterialTier tier = MaterialTier::Common;
    char glyph = '+';
    uint8_t color = 0;              // ColorId / Color enum value
    int sell_value = 0;
    bool is_junk_typed = false;     // true for Scrap, Broken Circuit, etc.
};

const std::vector<MaterialDef>& material_catalog();
const MaterialDef* find_material(uint32_t material_id);

```

- [ ] **Step 2: Edit `src/tinkering.cpp` — add stub catalog accessor**

Find the existing `s_material_effects[]` array (near top). Above it (after the existing `namespace astra {`), add:

```cpp
// ---------------------------------------------------------------------------
// Material catalog (24 entries — populated in a later task)
// ---------------------------------------------------------------------------

const std::vector<MaterialDef>& material_catalog() {
    static const std::vector<MaterialDef> catalog = {};
    return catalog;
}

const MaterialDef* find_material(uint32_t material_id) {
    for (const auto& m : material_catalog())
        if (m.material_id == material_id) return &m;
    return nullptr;
}

```

- [ ] **Step 3: Build**

```bash
cmake --build build
```

Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add include/astra/tinkering.h src/tinkering.cpp
git commit -m "feat(tinkering): add MaterialDef/MaterialReq/MaterialTier types

Empty catalog stub. Wires the type names into the codebase ahead of
populating entries, so subsequent tasks can pull them in incrementally.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Replace `SynthesisRecipe::material_cost[4]` with `std::vector<MaterialReq>` and migrate the 15 existing recipes

**Files:**
- Modify: `include/astra/tinkering.h` (`SynthesisRecipe` struct)
- Modify: `src/tinkering.cpp` (`synthesis_recipes()` body + `synthesize_item()` consumption loop)

This task **changes the on-disk shape of the recipe table but preserves the ingame semantic for the 15 recipes** by translating their existing 4-slot costs verbatim into the new vector form. Material additions/swaps come in Task 11.

- [ ] **Step 1: Edit `include/astra/tinkering.h` — change `SynthesisRecipe`**

Find the existing `struct SynthesisRecipe { ... }`. Replace the line:

```cpp
    int material_cost[4]; // [0]=Nano-Fiber, [1]=Power Core, [2]=Circuit Board, [3]=Alloy Ingot
```

with:

```cpp
    std::vector<MaterialReq> material_costs;
```

- [ ] **Step 2: Edit `src/tinkering.cpp` — rewrite `synthesis_recipes()` body**

Locate `const std::vector<SynthesisRecipe>& synthesis_recipes()`. Each recipe currently ends with a 4-int brace-init (e.g. `{0, 2, 0, 1}`); change every recipe entry's `material_cost` field to a `material_costs` vector. The id mapping is: index 0→`7001` (Nano-Fiber), 1→`7002` (Power Core), 2→`7003` (Circuit Board), 3→`7004` (Alloy Ingot). Skip zero-count entries.

Replace the entire block of `recipes = { ... }` so that, e.g., the first three become:

```cpp
        {"Plasma Emitter", "Blade Housing", "Plasma Edge",
         "A blade wreathed in plasma energy. Burns on contact.",
         ItemType::MeleeWeapon, EquipSlot::RightHand, '/',
         {8, 0, 0, 0, 0}, 60,
         { {7002, 2}, {7004, 1} } },

        {"Plating Alloy", "Thruster Core", "Thruster Plate",
         "Armored plating with integrated micro-thrusters for agile combat.",
         ItemType::Armor, EquipSlot::Body, ']',
         {0, 4, 0, 0, 3}, 80,
         { {7002, 1}, {7004, 2} } },

        {"Optic Module", "Power Conduit", "Targeting Array",
         "Advanced optics fused with a power feed. Enhances aim and awareness.",
         ItemType::Accessory, EquipSlot::Face, '&',
         {2, 0, 0, 3, 0}, 0,
         { {7002, 1}, {7003, 2} } },
```

Apply the same pattern to **all 15** recipe entries. The current 4-slot costs (in order Nano-Fiber, Power Core, Circuit Board, Alloy Ingot) are already in the file; translate verbatim. Drop any `0` counts.

For reference, the final (custom-builder) entries also keep their `material_costs` field — for example:

```cpp
        {"Plating Alloy", "Storage Frame", "Reinforced Casing",
         "Energy mod. Adds +10 capacity to the host cell.",
         ItemType::CraftingMaterial, EquipSlot::Back, '*',
         {}, 0,
         { {7001, 1}, {7004, 1} },
         &build_reinforced_casing },
```

- [ ] **Step 3: Edit `src/tinkering.cpp` — rewrite the consumption loop in `synthesize_item()`**

Find the existing material-cost-check and material-consumption blocks (they currently iterate over `s_material_ids[4]` and `recipe->material_cost[m]`). Replace both blocks with:

```cpp
    // Check material costs
    for (const auto& req : recipe->material_costs) {
        int have = 0;
        for (const auto& it : player.inventory.items) {
            if (it.id == req.material_id) have += it.stack_count;
        }
        if (have < req.count) {
            const MaterialDef* def = find_material(req.material_id);
            std::string mname = def ? def->name : ("material " + std::to_string(req.material_id));
            return {false, false, "Need " + std::to_string(req.count) + " " + mname +
                    " (have " + std::to_string(have) + ")."};
        }
    }

    // Consume materials
    for (const auto& req : recipe->material_costs) {
        int needed = req.count;
        for (auto it = player.inventory.items.begin(); it != player.inventory.items.end() && needed > 0; ) {
            if (it->id == req.material_id) {
                if (it->stack_count > needed) {
                    it->stack_count -= needed;
                    needed = 0;
                } else {
                    needed -= it->stack_count;
                    it = player.inventory.items.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }
```

Delete the now-unused `s_material_ids[4]` and `s_material_names[4]` static arrays at the top of `synthesize_item()` scope.

- [ ] **Step 4: Build**

```bash
cmake --build build
```

Expected: clean build. Compiler errors here usually mean a recipe entry was missed — every entry in the `recipes = {...}` initializer must have its `material_cost[4]` literal swapped for a `std::vector<MaterialReq>` literal.

- [ ] **Step 5: Smoke-test synthesizing one recipe**

```bash
./build/astra-dev --term
```

In game: open dev console, give a test player Plasma Emitter and Blade Housing blueprints (via `learn blueprint` if available, else hand-edit a fresh save by spawning equipment and analyzing). Easier: `give item alloy_ingot 5; give item power_core 5; give item circuit_board 5; give item nano_fiber 5`, then open Tinkering tab and synthesize a recipe whose blueprints you have. Expected: synthesis still works for whichever recipe blueprints are available; cost check uses the new path.

If you can't easily reach the synth UI in dev mode, alternative: build with `-DDEV=ON` and use `dev_console.cpp`'s `give item` to add materials, then visually verify the workbench Tinkering tab still loads without crashing. The conversion is structural — if it builds and the Tinkering tab opens, this is correct.

- [ ] **Step 6: Commit**

```bash
git add include/astra/tinkering.h src/tinkering.cpp
git commit -m "refactor(tinkering): replace material_cost[4] with vector<MaterialReq>

All 15 existing synthesis recipes migrated verbatim. Synthesizer
consumption loop rewritten to iterate the new vector. No behavior
change — same materials, same counts, same outputs.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Phase 2 — Material Catalog (defs + builders + ids)

### Task 4: Add T1 material item ids and builders (5 new pure-mat T1 items)

**Files:**
- Modify: `include/astra/item_ids.h`
- Modify: `src/item_defs.cpp`

T1 promoted-junk entries (Scrap Metal, Broken Circuit, Empty Casing, Spare Parts, Circuitry) keep their existing ids and builders unchanged. Only **new** T1 items are added here.

- [ ] **Step 1: Edit `include/astra/item_ids.h` — add new T1 material constants**

Find the existing `// Crafting materials (33-36)` block. Add a new block below it (use a free range):

```cpp
// Crafting materials — T1 (96-100)
constexpr uint16_t ITEM_COPPER_WIRE             = 96;
constexpr uint16_t ITEM_POLYMER_STRIP           = 97;
constexpr uint16_t ITEM_GLASS_SHARD             = 98;
constexpr uint16_t ITEM_ADHESIVE_RESIN          = 99;
constexpr uint16_t ITEM_COOLANT_VIAL            = 100;
```

- [ ] **Step 2: Edit `src/item_defs.cpp` — add 5 builder functions**

Find the existing `build_alloy_ingot()` function (around line 957). Immediately after its closing `}`, add:

```cpp
// --- T1 crafting materials -------------------------------------------------

Item build_copper_wire() {
    Item it;
    it.item_def_id = ITEM_COPPER_WIRE;
    it.id = 7010; it.name = "Copper Wire"; it.type = ItemType::CraftingMaterial;
    it.description = "A coil of conductive copper wire. Stacks as 'Strand of Copper Wire'.";
    it.weight = 1; it.stackable = true; it.buy_value = 5; it.sell_value = 2;
    return it;
}

Item build_polymer_strip() {
    Item it;
    it.item_def_id = ITEM_POLYMER_STRIP;
    it.id = 7011; it.name = "Polymer Strip"; it.type = ItemType::CraftingMaterial;
    it.description = "Flexible plastic stripping. Useful for casings and seals.";
    it.weight = 1; it.stackable = true; it.buy_value = 5; it.sell_value = 2;
    return it;
}

Item build_glass_shard() {
    Item it;
    it.item_def_id = ITEM_GLASS_SHARD;
    it.id = 7012; it.name = "Glass Shard"; it.type = ItemType::CraftingMaterial;
    it.description = "A jagged shard of clear glass. Refines into lenses.";
    it.weight = 1; it.stackable = true; it.buy_value = 3; it.sell_value = 1;
    return it;
}

Item build_adhesive_resin() {
    Item it;
    it.item_def_id = ITEM_ADHESIVE_RESIN;
    it.id = 7013; it.name = "Adhesive Resin"; it.type = ItemType::CraftingMaterial;
    it.description = "A small jar of bonding compound. Sticks anything to anything.";
    it.weight = 1; it.stackable = true; it.buy_value = 5; it.sell_value = 2;
    return it;
}

Item build_coolant_vial() {
    Item it;
    it.item_def_id = ITEM_COOLANT_VIAL;
    it.id = 7014; it.name = "Coolant Vial"; it.type = ItemType::CraftingMaterial;
    it.description = "Sealed vial of cryogenic fluid. Used in thermal recipes.";
    it.weight = 1; it.stackable = true; it.buy_value = 7; it.sell_value = 3;
    return it;
}

```

- [ ] **Step 3: Edit `src/item_defs.cpp` — wire builders into `build_by_def_id` dispatch**

Find the dispatch switch (around line 1189 — the `case ITEM_NANO_FIBER:` cluster). Add these cases in the same block:

```cpp
        case ITEM_COPPER_WIRE:             return build_copper_wire();
        case ITEM_POLYMER_STRIP:           return build_polymer_strip();
        case ITEM_GLASS_SHARD:             return build_glass_shard();
        case ITEM_ADHESIVE_RESIN:          return build_adhesive_resin();
        case ITEM_COOLANT_VIAL:            return build_coolant_vial();
```

- [ ] **Step 4: Build**

```bash
cmake --build build
```

Expected: clean build.

- [ ] **Step 5: Smoke-test dev spawning**

```bash
./build/astra-dev --term
```

In dev console: `give item copper_wire 3` — but the dev console looks up by loot-table identifier, which we haven't added yet. Alternative: build, then come back to verify after Task 9 adds loot entries. For now: clean build is sufficient evidence.

- [ ] **Step 6: Commit**

```bash
git add include/astra/item_ids.h src/item_defs.cpp
git commit -m "feat(items): add T1 crafting materials (copper wire, polymer strip, glass shard, adhesive resin, coolant vial)

5 new T1 reagents. Builders + dispatch only; loot table and recipe
references come in later tasks.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: Add T2 material item ids and builders (4 new pure-mat T2 items)

**Files:**
- Modify: `include/astra/item_ids.h`
- Modify: `src/item_defs.cpp`

- [ ] **Step 1: Edit `include/astra/item_ids.h` — add T2 material constants**

After the T1 block from Task 4:

```cpp
// Crafting materials — T2 (101-104)
constexpr uint16_t ITEM_NANO_LATTICE            = 101;
constexpr uint16_t ITEM_POLISHED_LENS           = 102;
constexpr uint16_t ITEM_MICRO_SERVO             = 103;
constexpr uint16_t ITEM_PLASMA_CARTRIDGE        = 104;
```

- [ ] **Step 2: Edit `src/item_defs.cpp` — add 4 builders**

Add immediately after `build_coolant_vial()` from Task 4:

```cpp
// --- T2 crafting materials -------------------------------------------------

Item build_nano_lattice() {
    Item it;
    it.item_def_id = ITEM_NANO_LATTICE;
    it.id = 7020; it.name = "Nano Lattice"; it.type = ItemType::CraftingMaterial;
    it.description = "A woven cage of carbon-nano filaments. Light as foam, hard as steel.";
    it.weight = 1; it.stackable = true; it.buy_value = 35; it.sell_value = 14;
    return it;
}

Item build_polished_lens() {
    Item it;
    it.item_def_id = ITEM_POLISHED_LENS;
    it.id = 7021; it.name = "Polished Lens"; it.type = ItemType::CraftingMaterial;
    it.description = "Optical-grade lens. Used in sensors and targeting arrays.";
    it.weight = 1; it.stackable = true; it.buy_value = 30; it.sell_value = 12;
    return it;
}

Item build_micro_servo() {
    Item it;
    it.item_def_id = ITEM_MICRO_SERVO;
    it.id = 7022; it.name = "Micro-Servo"; it.type = ItemType::CraftingMaterial;
    it.description = "Compact actuator with built-in feedback control. Hums faintly.";
    it.weight = 1; it.stackable = true; it.buy_value = 35; it.sell_value = 14;
    return it;
}

Item build_plasma_cartridge() {
    Item it;
    it.item_def_id = ITEM_PLASMA_CARTRIDGE;
    it.id = 7023; it.name = "Plasma Cartridge"; it.type = ItemType::CraftingMaterial;
    it.description = "Pressurized plasma reagent. Volatile but potent.";
    it.weight = 1; it.stackable = true; it.buy_value = 40; it.sell_value = 16;
    return it;
}

```

- [ ] **Step 3: Edit `src/item_defs.cpp` — extend dispatch**

Add after the T1 cases from Task 4:

```cpp
        case ITEM_NANO_LATTICE:            return build_nano_lattice();
        case ITEM_POLISHED_LENS:           return build_polished_lens();
        case ITEM_MICRO_SERVO:             return build_micro_servo();
        case ITEM_PLASMA_CARTRIDGE:        return build_plasma_cartridge();
```

- [ ] **Step 4: Build**

```bash
cmake --build build
```

- [ ] **Step 5: Commit**

```bash
git add include/astra/item_ids.h src/item_defs.cpp
git commit -m "feat(items): add T2 crafting materials (nano lattice, polished lens, micro-servo, plasma cartridge)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: Add T3 material item ids and builders (6 new T3 items)

**Files:**
- Modify: `include/astra/item_ids.h`
- Modify: `src/item_defs.cpp`

- [ ] **Step 1: Edit `include/astra/item_ids.h` — add T3 material constants**

```cpp
// Crafting materials — T3 (105-110)
constexpr uint16_t ITEM_QUANTUM_RESONANCE_CRYSTAL = 105;
constexpr uint16_t ITEM_STRANGE_STROBING_CRYSTAL  = 106;
constexpr uint16_t ITEM_PRIME_CATALYST            = 107;
constexpr uint16_t ITEM_PRIME_FILAMENT            = 108;
constexpr uint16_t ITEM_VOIDSHARD                 = 109;
constexpr uint16_t ITEM_PHASE_COIL                = 110;
```

- [ ] **Step 2: Edit `src/item_defs.cpp` — add 6 builders**

Append after the T2 builders:

```cpp
// --- T3 crafting materials -------------------------------------------------

Item build_quantum_resonance_crystal() {
    Item it;
    it.item_def_id = ITEM_QUANTUM_RESONANCE_CRYSTAL;
    it.id = 7030; it.name = "Quantum Resonance Crystal"; it.type = ItemType::CraftingMaterial;
    it.description = "A lattice that hums in harmony with nothing in this universe.";
    it.weight = 1; it.stackable = true; it.buy_value = 130; it.sell_value = 50;
    it.rarity = Rarity::Rare;
    return it;
}

Item build_strange_strobing_crystal() {
    Item it;
    it.item_def_id = ITEM_STRANGE_STROBING_CRYSTAL;
    it.id = 7031; it.name = "Strange Strobing Crystal"; it.type = ItemType::CraftingMaterial;
    it.description = "Pulses light at irregular intervals. Watching it too long hurts.";
    it.weight = 1; it.stackable = true; it.buy_value = 160; it.sell_value = 60;
    it.rarity = Rarity::Rare;
    return it;
}

Item build_prime_catalyst() {
    Item it;
    it.item_def_id = ITEM_PRIME_CATALYST;
    it.id = 7032; it.name = "Prime Catalyst"; it.type = ItemType::CraftingMaterial;
    it.description = "A reagent that drives reactions to completion every time. Ancient origin.";
    it.weight = 1; it.stackable = true; it.buy_value = 145; it.sell_value = 55;
    it.rarity = Rarity::Rare;
    return it;
}

Item build_prime_filament() {
    Item it;
    it.item_def_id = ITEM_PRIME_FILAMENT;
    it.id = 7033; it.name = "Prime Filament"; it.type = ItemType::CraftingMaterial;
    it.description = "An indestructible thread of unknown alloy. Bends, never breaks.";
    it.weight = 1; it.stackable = true; it.buy_value = 145; it.sell_value = 55;
    it.rarity = Rarity::Rare;
    return it;
}

Item build_voidshard() {
    Item it;
    it.item_def_id = ITEM_VOIDSHARD;
    it.id = 7034; it.name = "Voidshard"; it.type = ItemType::CraftingMaterial;
    it.description = "A fragment of matter warped by proximity to a singularity.";
    it.weight = 1; it.stackable = true; it.buy_value = 185; it.sell_value = 70;
    it.rarity = Rarity::Epic;
    return it;
}

Item build_phase_coil() {
    Item it;
    it.item_def_id = ITEM_PHASE_COIL;
    it.id = 7035; it.name = "Phase Coil"; it.type = ItemType::CraftingMaterial;
    it.description = "A coil that exists only intermittently. Sometimes you can't pick it up.";
    it.weight = 1; it.stackable = true; it.buy_value = 170; it.sell_value = 65;
    it.rarity = Rarity::Epic;
    return it;
}

```

- [ ] **Step 3: Edit `src/item_defs.cpp` — extend dispatch**

```cpp
        case ITEM_QUANTUM_RESONANCE_CRYSTAL: return build_quantum_resonance_crystal();
        case ITEM_STRANGE_STROBING_CRYSTAL:  return build_strange_strobing_crystal();
        case ITEM_PRIME_CATALYST:            return build_prime_catalyst();
        case ITEM_PRIME_FILAMENT:            return build_prime_filament();
        case ITEM_VOIDSHARD:                 return build_voidshard();
        case ITEM_PHASE_COIL:                return build_phase_coil();
```

- [ ] **Step 4: Build**

```bash
cmake --build build
```

- [ ] **Step 5: Commit**

```bash
git add include/astra/item_ids.h src/item_defs.cpp
git commit -m "feat(items): add T3 crafting materials (qrc, strobing crystal, prime catalyst/filament, voidshard, phase coil)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 7: Populate `material_catalog()` with all 24 entries

**Files:**
- Modify: `src/tinkering.cpp`

- [ ] **Step 1: Edit `src/tinkering.cpp` — replace empty catalog body**

Replace the existing stub from Task 2:

```cpp
const std::vector<MaterialDef>& material_catalog() {
    static const std::vector<MaterialDef> catalog = {};
    return catalog;
}
```

with the full table:

```cpp
const std::vector<MaterialDef>& material_catalog() {
    using T = MaterialTier;
    // Color values use renderer.h Color enum cast to uint8_t.
    // glyph reflects the in-game item glyph: '~' for junk-typed, ',' for T1 mats, '+' for T2, '*' for T3.
    static const std::vector<MaterialDef> catalog = {
        // --- T1 (junk-typed reagents, item ids match existing junk defs) ---
        { 30,   "Scrap Metal",     T::Common,   '~', static_cast<uint8_t>(Color::DarkGray),     1, true },
        { 31,   "Broken Circuit",  T::Common,   '~', static_cast<uint8_t>(Color::DarkGray),     2, true },
        { 32,   "Empty Casing",    T::Common,   '~', static_cast<uint8_t>(Color::DarkGray),     1, true },
        // --- T1 (pure-mat) ---
        { 7010, "Copper Wire",     T::Common,   ',', static_cast<uint8_t>(Color::Yellow),       2, false },
        { 7011, "Polymer Strip",   T::Common,   ',', static_cast<uint8_t>(Color::White),        2, false },
        { 7012, "Glass Shard",     T::Common,   ',', static_cast<uint8_t>(Color::Cyan),         1, false },
        { 7013, "Adhesive Resin",  T::Common,   ',', static_cast<uint8_t>(Color::Yellow),       2, false }, // 'orange' fallback
        { 7014, "Coolant Vial",    T::Common,   ',', static_cast<uint8_t>(Color::Blue),         3, false },
        // --- T2 (existing pure-mat) ---
        { 7001, "Nano-Fiber",      T::Uncommon, '+', static_cast<uint8_t>(Color::Cyan),         8, false },
        { 7002, "Power Core",      T::Uncommon, '+', static_cast<uint8_t>(Color::Yellow),      12, false },
        { 7003, "Circuit Board",   T::Uncommon, '+', static_cast<uint8_t>(Color::Green),       10, false },
        { 7004, "Alloy Ingot",     T::Uncommon, '+', static_cast<uint8_t>(Color::White),       10, false },
        // --- T2 (junk-typed reagents) ---
        { 47,   "Spare Parts",     T::Uncommon, '~', static_cast<uint8_t>(Color::Yellow),       6, true },
        { 48,   "Circuitry",       T::Uncommon, '~', static_cast<uint8_t>(Color::Cyan),         8, true },
        // --- T2 (new pure-mat) ---
        { 7020, "Nano Lattice",    T::Uncommon, '+', static_cast<uint8_t>(Color::White),       14, false },
        { 7021, "Polished Lens",   T::Uncommon, '+', static_cast<uint8_t>(Color::Cyan),        12, false },
        { 7022, "Micro-Servo",     T::Uncommon, '+', static_cast<uint8_t>(Color::Yellow),      14, false },
        { 7023, "Plasma Cartridge",T::Uncommon, '+', static_cast<uint8_t>(Color::Red),         16, false },
        // --- T3 ---
        { 7030, "Quantum Resonance Crystal", T::Rare, '*', static_cast<uint8_t>(Color::Magenta), 50, false },
        { 7031, "Strange Strobing Crystal",  T::Rare, '*', static_cast<uint8_t>(Color::White),   60, false },
        { 7032, "Prime Catalyst",            T::Rare, '*', static_cast<uint8_t>(Color::Yellow),  55, false },
        { 7033, "Prime Filament",            T::Rare, '*', static_cast<uint8_t>(Color::Cyan),    55, false },
        { 7034, "Voidshard",                 T::Rare, '*', static_cast<uint8_t>(Color::Magenta), 70, false },
        { 7035, "Phase Coil",                T::Rare, '*', static_cast<uint8_t>(Color::Blue),    65, false },
    };
    return catalog;
}
```

If `Color::DarkGray` is not a member, look in `include/astra/renderer.h:28` for the actual `Color` enum names and substitute. (Color names are already used elsewhere in the codebase — see existing `it.color = Color::Cyan` style usages — so the enum values are available.)

- [ ] **Step 2: Build**

```bash
cmake --build build
```

If you get a Color name error, open `include/astra/renderer.h` line 28+ and use the closest matching enum value (e.g., `Color::Gray` instead of `Color::DarkGray`). Update the catalog entries accordingly.

- [ ] **Step 3: Commit**

```bash
git add src/tinkering.cpp
git commit -m "feat(tinkering): populate material catalog with 24 entries across 3 tiers

Single source of truth for material metadata used by salvage, refinement,
synthesis, schematic crafting, loot weighting, and the docs table.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 8: Apply revised material costs to all 15 existing synthesis recipes

**Files:**
- Modify: `src/tinkering.cpp` (`synthesis_recipes()` body)

This task changes recipe costs to the revised values from spec §3.2. The recipes themselves (output names, blueprints, stats) stay identical.

- [ ] **Step 1: Edit `src/tinkering.cpp` — set the new costs**

Recall material ids: Scrap=30, Broken Circuit=31, Empty Casing=32, Spare Parts=47, Circuitry=48, Nano-Fiber=7001, Power Core=7002, Circuit Board=7003, Alloy Ingot=7004, Copper Wire=7010, Polymer Strip=7011, Glass Shard=7012, Adhesive Resin=7013, Coolant Vial=7014, Nano Lattice=7020, Polished Lens=7021, Micro-Servo=7022, Plasma Cartridge=7023, Quantum Resonance Crystal=7030.

Replace each recipe's `material_costs` field with the values below, recipe by recipe (in the same order they appear in the file):

```cpp
// Plasma Edge: 2 Scrap + 1 Circuitry + 1 Power Core + 1 Alloy Ingot + 1 Plasma Cartridge
{ {30, 2}, {48, 1}, {7002, 1}, {7004, 1}, {7023, 1} }

// Thruster Plate: 3 Scrap + 1 Circuitry + 2 Alloy Ingot + 1 Micro-Servo
{ {30, 3}, {48, 1}, {7004, 2}, {7022, 1} }

// Targeting Array: 1 Scrap + 1 Circuitry + 1 Circuit Board + 1 Polished Lens + 2 Copper Wire
{ {30, 1}, {48, 1}, {7003, 1}, {7021, 1}, {7010, 2} }

// Dual-Edge: 3 Scrap + 1 Circuitry + 2 Alloy Ingot + 1 Nano-Fiber
{ {30, 3}, {48, 1}, {7004, 2}, {7001, 1} }

// Reinforced Pack: 2 Scrap + 2 Nano-Fiber + 1 Polymer Strip + 1 Adhesive Resin
{ {30, 2}, {7001, 2}, {7011, 1}, {7013, 1} }

// Overcharged Engine: 2 Scrap + 1 Circuitry + 2 Power Core + 1 Coolant Vial + 1 Plasma Cartridge
{ {30, 2}, {48, 1}, {7002, 2}, {7014, 1}, {7023, 1} }

// Articulated Armor: 3 Scrap + 1 Circuitry + 2 Alloy Ingot + 1 Micro-Servo + 1 Nano Lattice
{ {30, 3}, {48, 1}, {7004, 2}, {7022, 1}, {7020, 1} }

// Guided Blaster: 2 Scrap + 1 Circuitry + 1 Power Core + 1 Polished Lens + 1 Plasma Cartridge
{ {30, 2}, {48, 1}, {7002, 1}, {7021, 1}, {7023, 1} }

// Combat Gauntlet: 2 Scrap + 1 Circuitry + 1 Alloy Ingot + 1 Nano-Fiber + 1 Micro-Servo
{ {30, 2}, {48, 1}, {7004, 1}, {7001, 1}, {7022, 1} }

// Armored Blade: 4 Scrap + 2 Alloy Ingot + 1 Nano-Fiber
{ {30, 4}, {7004, 2}, {7001, 1} }

// Reinforced Casing (custom builder): 2 Scrap + 1 Alloy Ingot + 1 Polymer Strip
{ {30, 2}, {7004, 1}, {7011, 1} }

// Receptor Plate (custom builder): 1 Scrap + 1 Circuitry + 1 Copper Wire + 1 Polished Lens
{ {30, 1}, {48, 1}, {7010, 1}, {7021, 1} }

// Brass Conduit (custom builder): 1 Scrap + 1 Copper Wire + 1 Power Core
{ {30, 1}, {7010, 1}, {7002, 1} }

// AI Module (custom builder): 2 Scrap + 1 Circuitry + 1 Circuit Board + 1 Nano Lattice + 1 QRC
{ {30, 2}, {48, 1}, {7003, 1}, {7020, 1}, {7030, 1} }

// Light Sensor (custom builder): 1 Scrap + 1 Circuitry + 1 Circuit Board + 1 Polished Lens
{ {30, 1}, {48, 1}, {7003, 1}, {7021, 1} }
```

Apply each in the order recipes appear in `synthesis_recipes()`.

- [ ] **Step 2: Build**

```bash
cmake --build build
```

- [ ] **Step 3: Commit**

```bash
git add src/tinkering.cpp
git commit -m "feat(tinkering): revise existing synthesis recipe material costs

Hand-tuned per spec §3.2: every recipe takes Scrap, 13/15 take
Circuitry, reagents are flavor-led. Reinforced Pack and Armored Blade
are intentionally Circuitry-free (mechanical theme).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 9: Add new materials to the loot table

**Files:**
- Modify: `src/loot_table.cpp`

- [ ] **Step 1: Edit `src/loot_table.cpp` — extend the crafting-materials block**

Find the existing `// ----- Crafting materials ---` section (around line 86). Add new entries after the existing 6:

```cpp
        // --- T1 pure-mats ---
        LootEntry{ ITEM_COPPER_WIRE,         "copper_wire",         R::Common,    R::Common,    35, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::ScavMerchant | LootSource::MerchantGeneral, T::Tech,     1, C::CraftingMaterial },
        LootEntry{ ITEM_POLYMER_STRIP,       "polymer_strip",       R::Common,    R::Common,    35, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::ScavMerchant | LootSource::MerchantGeneral, T::Civilian, 1, C::CraftingMaterial },
        LootEntry{ ITEM_GLASS_SHARD,         "glass_shard",         R::Common,    R::Common,    30, {}, LootSource::NpcDrop | LootSource::Chest | LootSource::ScavMerchant,                              T::Scrap,    1, C::CraftingMaterial },
        LootEntry{ ITEM_ADHESIVE_RESIN,      "adhesive_resin",      R::Common,    R::Common,    30, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::ScavMerchant,                      T::Civilian, 1, C::CraftingMaterial },
        LootEntry{ ITEM_COOLANT_VIAL,        "coolant_vial",        R::Common,    R::Uncommon,  25, {}, LootSource::Chest | LootSource::MerchantGeneral,                                                  T::Tech,     1, C::CraftingMaterial },
        // --- T2 new pure-mats ---
        LootEntry{ ITEM_NANO_LATTICE,        "nano_lattice",        R::Uncommon,  R::Rare,      18, {}, LootSource::Chest | LootSource::MerchantGeneral,                                                  T::Tech,     2, C::CraftingMaterial },
        LootEntry{ ITEM_POLISHED_LENS,       "polished_lens",       R::Uncommon,  R::Rare,      20, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms,                       T::Tech,     2, C::CraftingMaterial },
        LootEntry{ ITEM_MICRO_SERVO,         "micro_servo",         R::Uncommon,  R::Rare,      20, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::MerchantArms,                       T::Tech,     2, C::CraftingMaterial },
        LootEntry{ ITEM_PLASMA_CARTRIDGE,    "plasma_cartridge",    R::Uncommon,  R::Rare,      15, {}, LootSource::Chest | LootSource::MerchantArms,                                                     T::Military, 2, C::CraftingMaterial },
        // --- T3 (boss/deep dungeon/quest only — Chest at depth filter via min_level) ---
        LootEntry{ ITEM_QUANTUM_RESONANCE_CRYSTAL, "quantum_resonance_crystal", R::Rare, R::Epic,      4, {}, LootSource::Chest | LootSource::BlackMarket, T::Ancient, 5, C::CraftingMaterial },
        LootEntry{ ITEM_STRANGE_STROBING_CRYSTAL,  "strange_strobing_crystal",  R::Rare, R::Legendary, 3, {}, LootSource::Chest | LootSource::BlackMarket, T::Ancient, 5, C::CraftingMaterial },
        LootEntry{ ITEM_PRIME_CATALYST,            "prime_catalyst",            R::Rare, R::Epic,      3, {}, LootSource::Chest | LootSource::BlackMarket, T::Ancient, 5, C::CraftingMaterial },
        LootEntry{ ITEM_PRIME_FILAMENT,            "prime_filament",            R::Rare, R::Epic,      3, {}, LootSource::Chest | LootSource::BlackMarket, T::Ancient, 5, C::CraftingMaterial },
        LootEntry{ ITEM_VOIDSHARD,                 "voidshard",                 R::Epic, R::Legendary, 2, {}, LootSource::Chest | LootSource::BlackMarket, T::Ancient, 6, C::CraftingMaterial },
        LootEntry{ ITEM_PHASE_COIL,                "phase_coil",                R::Epic, R::Legendary, 2, {}, LootSource::Chest | LootSource::BlackMarket, T::Ancient, 6, C::CraftingMaterial },
```

- [ ] **Step 2: Build**

```bash
cmake --build build
```

- [ ] **Step 3: Verify dispatch coverage at startup (DEV mode)**

```bash
./build/astra-dev --term 2>&1 | head -10
```

The dev build calls `verify_dispatch_coverage()` at startup; missing builders log to stderr. Expected: no `[loot_table]` warnings about missing dispatch for the new ids.

- [ ] **Step 4: Smoke-test give command**

In game dev console:

```
give item copper_wire 5
give item nano_lattice 2
give item voidshard 1
```

Expected: items appear in inventory, render correctly.

- [ ] **Step 5: Commit**

```bash
git add src/loot_table.cpp
git commit -m "feat(loot): add 15 new crafting materials to loot table

T1 pure-mats drop broadly. T2 new mats drop at mid+ depths and
merchants. T3 mats are gated to Chest/BlackMarket at depth ≥ 5.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Phase 3 — Refinement

### Task 10: Add `RefinementRecipe` type, recipes table, and `refine()` function

**Files:**
- Modify: `include/astra/tinkering.h`
- Modify: `src/tinkering.cpp`

- [ ] **Step 1: Edit `include/astra/tinkering.h` — declare types**

After the existing `TinkerResult` struct, before the `repair_cost` declaration:

```cpp
// --- Refinement -------------------------------------------------

struct RefinementRecipe {
    const char* name;                    // e.g., "Smelt Alloy Ingot"
    std::vector<MaterialReq> inputs;
    uint32_t output_id;                  // Item::id for inventory matching
    uint16_t output_def_id;              // for build_by_def_id
    int output_count = 1;
};

const std::vector<RefinementRecipe>& refinement_recipes();
TinkerResult refine_item(const RefinementRecipe& recipe, Player& player);

```

- [ ] **Step 2: Edit `src/tinkering.cpp` — add the recipes**

Add a new section after the `material_catalog()` definition:

```cpp
// ---------------------------------------------------------------------------
// Refinement recipes (junk → T2 material)
// ---------------------------------------------------------------------------

const std::vector<RefinementRecipe>& refinement_recipes() {
    using R = RefinementRecipe;
    static const std::vector<R> recipes = {
        { "Smelt Alloy Ingot",     { {30, 3} },                       7004, ITEM_ALLOY_INGOT,      1 },
        { "Recover Circuit Board", { {31, 2}, {7010, 1} },             7003, ITEM_CIRCUIT_BOARD,    1 },
        { "Spin Nano-Fiber",       { {32, 4}, {7013, 1} },             7001, ITEM_NANO_FIBER,       1 },
        { "Assemble Power Core",   { {47, 2}, {7010, 1} },             7002, ITEM_POWER_CORE,       1 },
        { "Build Circuitry",       { {31, 1}, {47, 1}, {7010, 1} },    48,   ITEM_CIRCUITRY,        1 },
        { "Polish Lens",           { {7012, 2}, {7011, 1} },           7021, ITEM_POLISHED_LENS,    1 },
        { "Tune Micro-Servo",      { {47, 2}, {7014, 1} },             7022, ITEM_MICRO_SERVO,      1 },
        { "Weave Nano Lattice",    { {7001, 3}, {7011, 1} },           7020, ITEM_NANO_LATTICE,     1 },
        { "Pressurize Plasma Cartridge", { {7002, 2}, {7014, 1} },     7023, ITEM_PLASMA_CARTRIDGE, 1 },
    };
    return recipes;
}
```

- [ ] **Step 3: Edit `src/tinkering.cpp` — add `refine_item()`**

Append immediately after `refinement_recipes()`:

```cpp
TinkerResult refine_item(const RefinementRecipe& recipe, Player& player) {
    if (!player_has_skill(player, SkillId::BasicRepair))
        return {false, false, "Requires Basic Repair skill."};

    // Cost check
    for (const auto& req : recipe.inputs) {
        int have = 0;
        for (const auto& it : player.inventory.items)
            if (it.id == req.material_id) have += it.stack_count;
        if (have < req.count) {
            const MaterialDef* def = find_material(req.material_id);
            std::string mname = def ? def->name : ("material " + std::to_string(req.material_id));
            return {false, false, "Need " + std::to_string(req.count) + " " + mname +
                    " (have " + std::to_string(have) + ")."};
        }
    }

    // Consume inputs
    for (const auto& req : recipe.inputs) {
        int needed = req.count;
        for (auto it = player.inventory.items.begin(); it != player.inventory.items.end() && needed > 0; ) {
            if (it->id == req.material_id) {
                if (it->stack_count > needed) { it->stack_count -= needed; needed = 0; }
                else { needed -= it->stack_count; it = player.inventory.items.erase(it); continue; }
            }
            ++it;
        }
    }

    // Produce output(s) — merge into existing stack if possible
    for (int i = 0; i < recipe.output_count; ++i) {
        bool merged = false;
        for (auto& inv : player.inventory.items) {
            if (inv.id == recipe.output_id) { inv.stack_count++; merged = true; break; }
        }
        if (!merged) {
            Item out = build_by_def_id(recipe.output_def_id);
            player.inventory.items.push_back(std::move(out));
        }
    }

    return {true, false, std::string("Refined: ") + recipe.name + "."};
}
```

If `build_by_def_id` is in a different namespace or header, add `#include "astra/item_defs.h"` to `tinkering.cpp` (it should already be there from earlier — verify).

- [ ] **Step 4: Build**

```bash
cmake --build build
```

- [ ] **Step 5: Commit**

```bash
git add include/astra/tinkering.h src/tinkering.cpp
git commit -m "feat(tinkering): add refinement recipes (junk → T2 mats)

9 refinement recipes turn junk items and T1 pure-mats into T2
materials. Skill-gated on Basic Repair. UI hookup follows.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 11: Wire refinement UI into the Tinkering tab

**Files:**
- Modify: `include/astra/character_screen.h` (add `TinkerFocus::Refinement` value if enum is here)
- Modify: `src/character_screen.cpp`

- [ ] **Step 1: Locate the `TinkerFocus` enum**

```bash
grep -n "TinkerFocus" /Users/jeffrey/dev/crawler/include/astra/character_screen.h /Users/jeffrey/dev/crawler/src/character_screen.cpp | head -10
```

Read the surrounding code to understand which file owns the enum.

- [ ] **Step 2: Add `TinkerFocus::Refinement` (and later `Schematics`) to the enum**

In whichever file declares `enum class TinkerFocus`, add two new values at the end:

```cpp
    Refinement,
    Schematics,
```

(Adding both now to avoid touching this file twice. The Schematics arm is wired up in Task 17.)

- [ ] **Step 3: Edit `src/character_screen.cpp` — extend Tab key cycle**

Find the Tab navigation block (around line 484-495). Update the forward cycle:

```cpp
            if (tinker_focus_ == TinkerFocus::Materials) tinker_focus_ = TinkerFocus::Refinement;
            else if (tinker_focus_ == TinkerFocus::Refinement) tinker_focus_ = TinkerFocus::Schematics;
            else if (tinker_focus_ == TinkerFocus::Schematics) tinker_focus_ = TinkerFocus::Synthesizer;
            else if (tinker_focus_ == TinkerFocus::Synthesizer) tinker_focus_ = TinkerFocus::Slots;
            else if (tinker_focus_ == TinkerFocus::Slots) tinker_focus_ = TinkerFocus::Workbench;
```

And the reverse cycle similarly.

- [ ] **Step 4: Edit `src/character_screen.cpp` — render Refinement focus**

Find the rendering branch for `TinkerFocus::Materials` (it lists materials). Above or below it, add a branch for `TinkerFocus::Refinement` that lists `refinement_recipes()` with their input cost summary and a hotkey, e.g.:

```cpp
        } else if (tinker_focus_ == TinkerFocus::Refinement) {
            // Header
            ctx.text({.x = 0, .y = ry++, .content = "Refinement", .tag = UITag::TextBright});
            ry++;
            int idx = 0;
            for (const auto& r : refinement_recipes()) {
                std::string cost;
                for (const auto& req : r.inputs) {
                    const MaterialDef* def = find_material(req.material_id);
                    if (!cost.empty()) cost += " + ";
                    cost += std::to_string(req.count) + "× " + (def ? def->name : "?");
                }
                std::string line = std::string(1, '1' + idx) + ". " + r.name + " — " + cost;
                ctx.text({.x = 0, .y = ry++, .content = line, .tag = UITag::TextNormal});
                if (++idx >= 9) break;  // single-digit hotkey limit; expand later if needed
            }
            ry++;
            ctx.text({.x = 0, .y = ry++,
                .content = "[1-9] Refine recipe   [Tab] Next focus",
                .tag = UITag::TextDim});
        }
```

(Adapt class names — `ctx`, `UITag::TextBright`, `UITag::TextNormal` — to match the surrounding rendering style; copy the style of the Materials block.)

- [ ] **Step 5: Edit `src/character_screen.cpp` — handle keypress**

In the input handler for the Tinkering tab, add:

```cpp
        if (tinker_focus_ == TinkerFocus::Refinement) {
            if (key >= '1' && key <= '9') {
                int idx = key - '1';
                const auto& recipes = refinement_recipes();
                if (idx < static_cast<int>(recipes.size())) {
                    auto result = refine_item(recipes[idx], *player_);
                    context_message_ = result.message;
                }
                return true;
            }
        }
```

- [ ] **Step 6: Build**

```bash
cmake --build build
```

- [ ] **Step 7: Smoke-test in game**

```bash
./build/astra-dev --term
```

Open Tinkering tab, Tab over to Refinement focus. Spawn 3 scrap (`give item scrap_metal 3`), press `1` (Smelt Alloy Ingot). Expected: 3 Scrap consumed, 1 Alloy Ingot in inventory, message "Refined: Smelt Alloy Ingot."

- [ ] **Step 8: Commit**

```bash
git add include/astra/character_screen.h src/character_screen.cpp
git commit -m "feat(tinkering): wire refinement UI into Tinkering tab

New TinkerFocus::Refinement renders the 9 refinement recipes; 1-9
triggers refine_item. Schematics focus declared but unused yet.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Phase 4 — Schematic Crafting

### Task 12: Add `ItemType::Schematic`, `ItemType::Mine`, schematic-payload fields, `LearnedSchematic`, player state

**Files:**
- Modify: `include/astra/item.h`
- Modify: `include/astra/tinkering.h`
- Modify: `include/astra/player.h`

- [ ] **Step 1: Edit `include/astra/item.h` — extend `ItemType`**

Find the `enum class ItemType` (around line 21). Add two values to the V2 list (before the closing brace):

```cpp
    Mine,
    Schematic,
```

Then update `item_type_name()` in `src/item.cpp` to return strings for these:

```bash
grep -n "item_type_name" /Users/jeffrey/dev/crawler/src/item.cpp
```

Open the function and add cases:

```cpp
        case ItemType::Mine:      return "Mine";
        case ItemType::Schematic: return "Schematic";
```

- [ ] **Step 2: Edit `include/astra/item.h` — add `teaches_schematic_id` field on `Item`**

Find the `teaches_recipe_id` field (around line 222). Right below it:

```cpp
    // Schematic payload. Non-zero only when type == ItemType::Schematic.
    uint16_t teaches_schematic_id = 0;
```

- [ ] **Step 3: Edit `include/astra/tinkering.h` — add `LearnedSchematic` and `SchematicRecipe`**

After the existing `BlueprintSignature` struct, add:

```cpp
// --- Schematics ---------------------------------------------------------

struct LearnedSchematic {
    uint16_t schematic_id = 0;       // matches SchematicRecipe::schematic_id
    std::string name;
    std::string description;
};

struct SchematicRecipe {
    uint16_t schematic_id;           // unique recipe id
    uint32_t output_id;              // Item::id of the consumable
    uint16_t output_def_id;          // for build_by_def_id
    const char* output_name;
    const char* output_desc;
    std::vector<MaterialReq> material_costs;
    int output_count = 1;
};

const std::vector<SchematicRecipe>& schematic_recipes();
const SchematicRecipe* find_schematic_recipe(uint16_t schematic_id);
TinkerResult craft_schematic(uint16_t schematic_id, Player& player);
TinkerResult learn_schematic(Player& player, uint16_t schematic_id,
                             const char* name, const char* description);

```

- [ ] **Step 4: Edit `include/astra/player.h` — add learned_schematics**

Find the `// Tinkering` comment block (around line 122):

```cpp
    // Tinkering
    std::vector<BlueprintSignature> learned_blueprints;
    std::vector<LearnedSchematic> learned_schematics;
```

- [ ] **Step 5: Build**

```bash
cmake --build build
```

You may see compiler errors about missing switch cases for the new ItemType values — fix any in `src/item.cpp` and any other locations grep flags:

```bash
grep -rn "case ItemType::" /Users/jeffrey/dev/crawler/src/ /Users/jeffrey/dev/crawler/include/ | wc -l
```

Audit each switch statement and add `default:` or `case ItemType::Mine:` / `case ItemType::Schematic:` returning appropriate values. For switches that are exhaustive on intent (no default), add the new cases. For switches with `default`, no action needed.

- [ ] **Step 6: Commit**

```bash
git add include/astra/item.h include/astra/tinkering.h include/astra/player.h src/item.cpp
git commit -m "feat(tinkering): add Schematic + Mine ItemTypes, LearnedSchematic state, SchematicRecipe API

Type/struct declarations only. Implementations in following tasks.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 13: Add 13 new consumable item ids and builders (stims/grenades/mines)

**Files:**
- Modify: `include/astra/item_ids.h`
- Modify: `src/item_defs.cpp`

The 3 reused defs (Frag Grenade, EMP Grenade, Combat Stim/Adrenaline Stim) are not added; their builders stay as-is. Only 13 new defs.

- [ ] **Step 1: Edit `include/astra/item_ids.h` — add new consumable ids**

```cpp
// Consumables — schematic-craftable (200-212)
constexpr uint16_t ITEM_HEALING_STIM            = 200;
constexpr uint16_t ITEM_ENDURE_STIM             = 201;
constexpr uint16_t ITEM_FOCUS_STIM              = 202;
constexpr uint16_t ITEM_BERSERKER_STIM          = 203;
constexpr uint16_t ITEM_MEDKIT                  = 204;
constexpr uint16_t ITEM_INCENDIARY_GRENADE      = 205;
constexpr uint16_t ITEM_SMOKE_GRENADE           = 206;
constexpr uint16_t ITEM_FLASHBANG               = 207;
constexpr uint16_t ITEM_PROXIMITY_MINE          = 208;
constexpr uint16_t ITEM_EMP_MINE                = 209;
constexpr uint16_t ITEM_INCENDIARY_MINE         = 210;
constexpr uint16_t ITEM_DECOY_MINE              = 211;
constexpr uint16_t ITEM_CALTROPS                = 212;
```

- [ ] **Step 2: Edit `src/item_defs.cpp` — add 13 builders**

Add a new section near the existing grenade/stim builders (e.g., after `build_cryo_grenade()`):

```cpp
// ---------------------------------------------------------------------------
// New consumables (schematic-craftable; "use" code is a future spec — these
// items exist in inventory but are inert when used).
// ---------------------------------------------------------------------------

static Item build_consumable_(uint16_t def_id, uint32_t id, const char* name,
                              const char* desc, ItemType type, Rarity rarity,
                              int buy, int sell) {
    Item it;
    it.item_def_id = def_id;
    it.id = id;
    it.name = name;
    it.description = desc;
    it.type = type;
    it.rarity = rarity;
    it.weight = 1;
    it.stackable = true;
    it.stack_count = 1;
    it.buy_value = buy;
    it.sell_value = sell;
    if (type == ItemType::Grenade || type == ItemType::Mine) it.slot = EquipSlot::Thrown;
    return it;
}

// Stims
Item build_healing_stim()    { return build_consumable_(ITEM_HEALING_STIM,    7200, "Healing Stim",    "Auto-injector. Restores HP on use. (Inert until next spec.)", ItemType::Stim,    Rarity::Common,   40, 14); }
Item build_endure_stim()     { return build_consumable_(ITEM_ENDURE_STIM,     7201, "Endure Stim",     "Stabilizer. Hardens you against damage. (Inert.)",            ItemType::Stim,    Rarity::Uncommon, 60, 22); }
Item build_focus_stim()      { return build_consumable_(ITEM_FOCUS_STIM,      7202, "Focus Stim",      "Sharpens senses and accuracy. (Inert.)",                       ItemType::Stim,    Rarity::Uncommon, 60, 22); }
Item build_berserker_stim()  { return build_consumable_(ITEM_BERSERKER_STIM,  7203, "Berserker Stim",  "Risky combat surge. Big hit, big cost. (Inert.)",              ItemType::Stim,    Rarity::Rare,    100, 38); }
Item build_medkit()          { return build_consumable_(ITEM_MEDKIT,          7204, "Medkit",          "Field medical kit. Multi-charge healing. (Inert.)",            ItemType::Stim,    Rarity::Uncommon, 90, 32); }

// Grenades
Item build_incendiary_grenade() { return build_consumable_(ITEM_INCENDIARY_GRENADE, 7205, "Incendiary Grenade", "Spreads burning plasma in a radius. (Inert.)", ItemType::Grenade, Rarity::Uncommon, 60, 22); }
Item build_smoke_grenade()      { return build_consumable_(ITEM_SMOKE_GRENADE,      7206, "Smoke Grenade",      "Creates a vision-blocking cloud. (Inert.)",     ItemType::Grenade, Rarity::Common,   30, 11); }
Item build_flashbang()          { return build_consumable_(ITEM_FLASHBANG,          7207, "Flashbang",          "Stuns nearby enemies with a burst of light. (Inert.)", ItemType::Grenade, Rarity::Uncommon, 50, 18); }

// Mines
Item build_proximity_mine()  { return build_consumable_(ITEM_PROXIMITY_MINE,  7208, "Proximity Mine",  "Triggers on enemy step. Physical AoE. (Inert.)",        ItemType::Mine, Rarity::Uncommon, 70, 26); }
Item build_emp_mine()        { return build_consumable_(ITEM_EMP_MINE,        7209, "EMP Mine",        "Triggers on enemy step. EMP burst. (Inert.)",            ItemType::Mine, Rarity::Uncommon, 80, 30); }
Item build_incendiary_mine() { return build_consumable_(ITEM_INCENDIARY_MINE, 7210, "Incendiary Mine", "Triggers on enemy step. Fire AoE. (Inert.)",             ItemType::Mine, Rarity::Uncommon, 80, 30); }
Item build_decoy_mine()      { return build_consumable_(ITEM_DECOY_MINE,      7211, "Decoy Mine",      "Emits noise to draw attention. (Inert.)",                ItemType::Mine, Rarity::Common,   40, 14); }
Item build_caltrops()        { return build_consumable_(ITEM_CALTROPS,        7212, "Caltrops",        "A handful of jagged spikes. Cheap area denial. (Inert.)", ItemType::Mine, Rarity::Common,   20,  7); }

```

- [ ] **Step 3: Edit `src/item_defs.cpp` — extend dispatch**

Add cases in `build_by_def_id`:

```cpp
        case ITEM_HEALING_STIM:            return build_healing_stim();
        case ITEM_ENDURE_STIM:             return build_endure_stim();
        case ITEM_FOCUS_STIM:              return build_focus_stim();
        case ITEM_BERSERKER_STIM:          return build_berserker_stim();
        case ITEM_MEDKIT:                  return build_medkit();
        case ITEM_INCENDIARY_GRENADE:      return build_incendiary_grenade();
        case ITEM_SMOKE_GRENADE:           return build_smoke_grenade();
        case ITEM_FLASHBANG:               return build_flashbang();
        case ITEM_PROXIMITY_MINE:          return build_proximity_mine();
        case ITEM_EMP_MINE:                return build_emp_mine();
        case ITEM_INCENDIARY_MINE:         return build_incendiary_mine();
        case ITEM_DECOY_MINE:              return build_decoy_mine();
        case ITEM_CALTROPS:                return build_caltrops();
```

- [ ] **Step 4: Rename Combat Stim → Adrenaline Stim (in-place)**

Find `build_combat_stim()` (around line 211). Update `it.name` and `it.description` to:

```cpp
    it.name = "Adrenaline Stim";
    it.description = "Adrenaline injection. Temporarily boosts attack. (Inert until next spec.)";
```

Keep `item_def_id = ITEM_COMBAT_STIM` and `it.id = 2003` — saves and loot still resolve. Optionally also update the loot identifier in `loot_table.cpp` from `"combat_stim"` to `"adrenaline_stim"`. (Skip if you'd rather avoid disrupting `give item combat_stim` muscle memory; either is fine — pick one and move on.) For this plan: keep loot identifier `"combat_stim"`.

- [ ] **Step 5: Build**

```bash
cmake --build build
```

- [ ] **Step 6: Commit**

```bash
git add include/astra/item_ids.h src/item_defs.cpp
git commit -m "feat(items): add 13 new consumables (5 stims, 3 grenades, 5 mines) + rename Combat Stim → Adrenaline Stim

All new consumables are inert (no use code) per spec — they exist in
inventory and stack. Frag/EMP grenades and Adrenaline Stim reuse
existing item_def_ids.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 14: Add 16 schematic item ids, builders, and dispatch

**Files:**
- Modify: `include/astra/item_ids.h`
- Modify: `src/item_defs.cpp`

- [ ] **Step 1: Edit `include/astra/item_ids.h` — add 16 schematic constants**

```cpp
// Schematics (220-235)
constexpr uint16_t ITEM_SCHEM_HEALING_STIM        = 220;
constexpr uint16_t ITEM_SCHEM_ADRENALINE_STIM     = 221;
constexpr uint16_t ITEM_SCHEM_ENDURE_STIM         = 222;
constexpr uint16_t ITEM_SCHEM_FOCUS_STIM          = 223;
constexpr uint16_t ITEM_SCHEM_BERSERKER_STIM      = 224;
constexpr uint16_t ITEM_SCHEM_MEDKIT              = 225;
constexpr uint16_t ITEM_SCHEM_FRAG_GRENADE        = 226;
constexpr uint16_t ITEM_SCHEM_EMP_GRENADE         = 227;
constexpr uint16_t ITEM_SCHEM_INCENDIARY_GRENADE  = 228;
constexpr uint16_t ITEM_SCHEM_SMOKE_GRENADE       = 229;
constexpr uint16_t ITEM_SCHEM_FLASHBANG           = 230;
constexpr uint16_t ITEM_SCHEM_PROXIMITY_MINE      = 231;
constexpr uint16_t ITEM_SCHEM_EMP_MINE            = 232;
constexpr uint16_t ITEM_SCHEM_INCENDIARY_MINE     = 233;
constexpr uint16_t ITEM_SCHEM_DECOY_MINE          = 234;
constexpr uint16_t ITEM_SCHEM_CALTROPS            = 235;
```

- [ ] **Step 2: Edit `src/item_defs.cpp` — add a single helper + 16 thin wrappers**

Append after the new consumable builders:

```cpp
// ---------------------------------------------------------------------------
// Schematics — single-use, taught permanently when read. Mirror Cookbooks.
// ---------------------------------------------------------------------------

static Item build_schematic_(uint16_t def_id, uint32_t id, const char* name,
                             uint16_t teaches_schematic_id, Rarity rarity,
                             int buy, int sell) {
    Item it;
    it.item_def_id = def_id;
    it.id = id;
    it.name = name;
    it.description = "A folded schematic. Read to permanently learn the recipe.";
    it.type = ItemType::Schematic;
    it.rarity = rarity;
    it.weight = 1;
    it.stackable = false;
    it.stack_count = 1;
    it.buy_value = buy;
    it.sell_value = sell;
    it.usable = true;
    it.teaches_schematic_id = teaches_schematic_id;
    return it;
}

// schematic_id values match SchematicRecipe::schematic_id (see tinkering.cpp).
Item build_schem_healing_stim()       { return build_schematic_(ITEM_SCHEM_HEALING_STIM,       7100, "Schematic: Healing Stim",       1,  Rarity::Common,    50,  18); }
Item build_schem_adrenaline_stim()    { return build_schematic_(ITEM_SCHEM_ADRENALINE_STIM,    7101, "Schematic: Adrenaline Stim",    2,  Rarity::Uncommon,  90,  32); }
Item build_schem_endure_stim()        { return build_schematic_(ITEM_SCHEM_ENDURE_STIM,        7102, "Schematic: Endure Stim",        3,  Rarity::Uncommon,  90,  32); }
Item build_schem_focus_stim()         { return build_schematic_(ITEM_SCHEM_FOCUS_STIM,         7103, "Schematic: Focus Stim",         4,  Rarity::Uncommon,  90,  32); }
Item build_schem_berserker_stim()     { return build_schematic_(ITEM_SCHEM_BERSERKER_STIM,     7104, "Schematic: Berserker Stim",     5,  Rarity::Rare,     150,  55); }
Item build_schem_medkit()             { return build_schematic_(ITEM_SCHEM_MEDKIT,             7105, "Schematic: Medkit",             6,  Rarity::Uncommon, 130,  46); }

Item build_schem_frag_grenade()       { return build_schematic_(ITEM_SCHEM_FRAG_GRENADE,       7106, "Schematic: Frag Grenade",       7,  Rarity::Common,    50,  18); }
Item build_schem_emp_grenade()        { return build_schematic_(ITEM_SCHEM_EMP_GRENADE,        7107, "Schematic: EMP Grenade",        8,  Rarity::Uncommon, 100,  35); }
Item build_schem_incendiary_grenade() { return build_schematic_(ITEM_SCHEM_INCENDIARY_GRENADE, 7108, "Schematic: Incendiary Grenade", 9,  Rarity::Uncommon, 100,  35); }
Item build_schem_smoke_grenade()      { return build_schematic_(ITEM_SCHEM_SMOKE_GRENADE,      7109, "Schematic: Smoke Grenade",     10,  Rarity::Common,    40,  14); }
Item build_schem_flashbang()          { return build_schematic_(ITEM_SCHEM_FLASHBANG,          7110, "Schematic: Flashbang",         11,  Rarity::Uncommon,  90,  32); }

Item build_schem_proximity_mine()     { return build_schematic_(ITEM_SCHEM_PROXIMITY_MINE,     7111, "Schematic: Proximity Mine",    12,  Rarity::Uncommon, 110,  40); }
Item build_schem_emp_mine()           { return build_schematic_(ITEM_SCHEM_EMP_MINE,           7112, "Schematic: EMP Mine",          13,  Rarity::Rare,     140,  50); }
Item build_schem_incendiary_mine()    { return build_schematic_(ITEM_SCHEM_INCENDIARY_MINE,    7113, "Schematic: Incendiary Mine",   14,  Rarity::Rare,     140,  50); }
Item build_schem_decoy_mine()         { return build_schematic_(ITEM_SCHEM_DECOY_MINE,         7114, "Schematic: Decoy Mine",        15,  Rarity::Common,    60,  22); }
Item build_schem_caltrops()           { return build_schematic_(ITEM_SCHEM_CALTROPS,           7115, "Schematic: Caltrops",          16,  Rarity::Common,    25,   8); }

```

- [ ] **Step 3: Edit `src/item_defs.cpp` — extend dispatch**

```cpp
        case ITEM_SCHEM_HEALING_STIM:        return build_schem_healing_stim();
        case ITEM_SCHEM_ADRENALINE_STIM:     return build_schem_adrenaline_stim();
        case ITEM_SCHEM_ENDURE_STIM:         return build_schem_endure_stim();
        case ITEM_SCHEM_FOCUS_STIM:          return build_schem_focus_stim();
        case ITEM_SCHEM_BERSERKER_STIM:      return build_schem_berserker_stim();
        case ITEM_SCHEM_MEDKIT:              return build_schem_medkit();
        case ITEM_SCHEM_FRAG_GRENADE:        return build_schem_frag_grenade();
        case ITEM_SCHEM_EMP_GRENADE:         return build_schem_emp_grenade();
        case ITEM_SCHEM_INCENDIARY_GRENADE:  return build_schem_incendiary_grenade();
        case ITEM_SCHEM_SMOKE_GRENADE:       return build_schem_smoke_grenade();
        case ITEM_SCHEM_FLASHBANG:           return build_schem_flashbang();
        case ITEM_SCHEM_PROXIMITY_MINE:      return build_schem_proximity_mine();
        case ITEM_SCHEM_EMP_MINE:            return build_schem_emp_mine();
        case ITEM_SCHEM_INCENDIARY_MINE:     return build_schem_incendiary_mine();
        case ITEM_SCHEM_DECOY_MINE:          return build_schem_decoy_mine();
        case ITEM_SCHEM_CALTROPS:            return build_schem_caltrops();
```

- [ ] **Step 4: Build**

```bash
cmake --build build
```

- [ ] **Step 5: Commit**

```bash
git add include/astra/item_ids.h src/item_defs.cpp
git commit -m "feat(items): add 16 schematic items (one per consumable)

Schematics carry teaches_schematic_id that maps to SchematicRecipe::
schematic_id. Reading a schematic learns the recipe permanently
(handler in following task).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 15: Add `schematic_recipes()` table and `craft_schematic()` / `learn_schematic()`

**Files:**
- Modify: `src/tinkering.cpp`

- [ ] **Step 1: Edit `src/tinkering.cpp` — add the recipes table**

Append after `refinement_recipes()`:

```cpp
// ---------------------------------------------------------------------------
// Schematic recipes (consumable crafting)
// ---------------------------------------------------------------------------

const std::vector<SchematicRecipe>& schematic_recipes() {
    static const std::vector<SchematicRecipe> recipes = {
        // Stims
        {  1, 7200, ITEM_HEALING_STIM,    "Healing Stim",    "Auto-injector. Restores HP.",
           { {30, 1}, {32, 1}, {7001, 1}, {7012, 1} }, 1 },
        {  2, 2003, ITEM_COMBAT_STIM,     "Adrenaline Stim", "Adrenaline injection.",
           { {30, 1}, {32, 1}, {7002, 1}, {7014, 1} }, 1 },
        {  3, 7201, ITEM_ENDURE_STIM,     "Endure Stim",     "Hardens you against damage.",
           { {30, 1}, {32, 1}, {7001, 1}, {7013, 1} }, 1 },
        {  4, 7202, ITEM_FOCUS_STIM,      "Focus Stim",      "Sharpens senses.",
           { {30, 1}, {32, 1}, {7021, 1}, {7014, 1} }, 1 },
        {  5, 7203, ITEM_BERSERKER_STIM,  "Berserker Stim",  "Risky combat surge.",
           { {30, 2}, {32, 1}, {7002, 1}, {7023, 1} }, 1 },
        {  6, 7204, ITEM_MEDKIT,          "Medkit",          "Multi-charge healing.",
           { {30, 2}, {7011, 1}, {7001, 2}, {7013, 1} }, 1 },
        // Grenades
        {  7, 5001, ITEM_FRAG_GRENADE,        "Frag Grenade",        "Physical AoE.",
           { {30, 2}, {32, 1}, {7002, 1}, {7013, 1} }, 1 },
        {  8, 5002, ITEM_EMP_GRENADE,         "EMP Grenade",         "Disables electronics.",
           { {30, 1}, {32, 1}, {48, 1}, {7003, 1}, {7010, 1} }, 1 },
        {  9, 7205, ITEM_INCENDIARY_GRENADE,  "Incendiary Grenade",  "Fire AoE + lingering burn.",
           { {30, 2}, {32, 1}, {7014, 1}, {7023, 1} }, 1 },
        { 10, 7206, ITEM_SMOKE_GRENADE,       "Smoke Grenade",       "Vision-blocking cloud.",
           { {30, 1}, {32, 1}, {7011, 1}, {7013, 1} }, 1 },
        { 11, 7207, ITEM_FLASHBANG,           "Flashbang",           "Stun in radius.",
           { {30, 1}, {32, 1}, {48, 1}, {7012, 1}, {7002, 1} }, 1 },
        // Mines
        { 12, 7208, ITEM_PROXIMITY_MINE,  "Proximity Mine",  "Trigger-on-step physical AoE.",
           { {30, 2}, {32, 1}, {48, 1}, {7002, 1}, {47, 1} }, 1 },
        { 13, 7209, ITEM_EMP_MINE,        "EMP Mine",        "Trigger-on-step EMP.",
           { {30, 1}, {32, 1}, {48, 1}, {7003, 1}, {47, 1} }, 1 },
        { 14, 7210, ITEM_INCENDIARY_MINE, "Incendiary Mine", "Trigger-on-step fire AoE.",
           { {30, 2}, {32, 1}, {48, 1}, {7023, 1}, {47, 1} }, 1 },
        { 15, 7211, ITEM_DECOY_MINE,      "Decoy Mine",      "Emits noise.",
           { {30, 1}, {32, 1}, {48, 1}, {7010, 1}, {47, 1} }, 1 },
        { 16, 7212, ITEM_CALTROPS,        "Caltrops",        "Cheap area denial.",
           { {30, 3}, {7013, 1} }, 1 },
    };
    return recipes;
}

const SchematicRecipe* find_schematic_recipe(uint16_t schematic_id) {
    for (const auto& r : schematic_recipes())
        if (r.schematic_id == schematic_id) return &r;
    return nullptr;
}
```

- [ ] **Step 2: Edit `src/tinkering.cpp` — add `learn_schematic()`**

```cpp
TinkerResult learn_schematic(Player& player, uint16_t schematic_id,
                             const char* name, const char* description) {
    for (const auto& ls : player.learned_schematics) {
        if (ls.schematic_id == schematic_id)
            return {false, false, std::string("You already know ") + name + "."};
    }
    player.learned_schematics.push_back({ schematic_id, name, description ? description : "" });
    return {true, true, std::string("Learned schematic: ") + name + "."};
}
```

- [ ] **Step 3: Edit `src/tinkering.cpp` — add `craft_schematic()`**

```cpp
TinkerResult craft_schematic(uint16_t schematic_id, Player& player) {
    if (!player_has_skill(player, SkillId::Cat_Tinkering))
        return {false, false, "Requires Tinkering skill unlocked."};

    bool known = false;
    for (const auto& ls : player.learned_schematics)
        if (ls.schematic_id == schematic_id) { known = true; break; }
    if (!known)
        return {false, false, "Schematic not learned."};

    const SchematicRecipe* recipe = find_schematic_recipe(schematic_id);
    if (!recipe)
        return {false, false, "Unknown schematic recipe."};

    // Cost check
    for (const auto& req : recipe->material_costs) {
        int have = 0;
        for (const auto& it : player.inventory.items)
            if (it.id == req.material_id) have += it.stack_count;
        if (have < req.count) {
            const MaterialDef* def = find_material(req.material_id);
            std::string mname = def ? def->name : ("material " + std::to_string(req.material_id));
            return {false, false, "Need " + std::to_string(req.count) + " " + mname +
                    " (have " + std::to_string(have) + ")."};
        }
    }

    // Consume inputs
    for (const auto& req : recipe->material_costs) {
        int needed = req.count;
        for (auto it = player.inventory.items.begin(); it != player.inventory.items.end() && needed > 0; ) {
            if (it->id == req.material_id) {
                if (it->stack_count > needed) { it->stack_count -= needed; needed = 0; }
                else { needed -= it->stack_count; it = player.inventory.items.erase(it); continue; }
            }
            ++it;
        }
    }

    // Produce output(s)
    for (int i = 0; i < recipe->output_count; ++i) {
        bool merged = false;
        for (auto& inv : player.inventory.items) {
            if (inv.id == recipe->output_id) { inv.stack_count++; merged = true; break; }
        }
        if (!merged) {
            Item out = build_by_def_id(recipe->output_def_id);
            player.inventory.items.push_back(std::move(out));
        }
    }

    return {true, false, std::string("Crafted: ") + recipe->output_name + "."};
}
```

- [ ] **Step 4: Build**

```bash
cmake --build build
```

- [ ] **Step 5: Commit**

```bash
git add src/tinkering.cpp
git commit -m "feat(tinkering): add schematic recipes + craft_schematic + learn_schematic

16 recipes covering 6 stims, 5 grenades, 5 mines. Crafting requires
the recipe to be in player.learned_schematics. Reused defs (Frag/EMP
grenades, Adrenaline Stim) target the existing item_def_ids.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 16: Wire schematic-read handler (use_item on a Schematic)

**Files:**
- Modify: wherever `use_item` is implemented (find via grep)

- [ ] **Step 1: Locate the use-item handler**

```bash
grep -rn "ItemType::Cookbook\|teaches_recipe_id\|use_item\|consume_item" /Users/jeffrey/dev/crawler/src/ /Users/jeffrey/dev/crawler/include/ | head -20
```

The Cookbook "read" path is the closest analog — it consumes the cookbook and adds to `known_recipes`. Find that branch and read the surrounding code to understand the consume-and-remove pattern.

- [ ] **Step 2: Add a parallel branch for `ItemType::Schematic`**

In the same file/function where `ItemType::Cookbook` is handled (e.g., `game_interaction.cpp` or `character_screen.cpp`), add a branch:

```cpp
if (item.type == ItemType::Schematic && item.teaches_schematic_id != 0) {
    auto result = learn_schematic(*player_, item.teaches_schematic_id,
                                   item.name.c_str(), item.description.c_str());
    if (result.success) {
        // Consume the schematic from inventory (mirror cookbook consumption)
        // ... use the same code path the Cookbook branch uses ...
    }
    // Show message in HUD
    return; // or appropriate return for the function shape
}
```

Match the *exact* pattern used by the Cookbook branch — same removal logic, same message-display path. Substitute schematic-specific names.

- [ ] **Step 3: Build**

```bash
cmake --build build
```

- [ ] **Step 4: Smoke-test**

```bash
./build/astra-dev --term
```

In dev console: `give item schem_caltrops 1`. Open inventory, "use" the schematic. Expected: schematic disappears from inventory, message "Learned schematic: Schematic: Caltrops." (or similar). The recipe shows up in the (yet-unimplemented) Schematics focus.

- [ ] **Step 5: Commit**

```bash
git add <whichever file>
git commit -m "feat(tinkering): reading a schematic learns the recipe permanently

Mirrors cookbook semantics — single-use pickup, recipe persists.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 17: Wire schematic crafting UI (TinkerFocus::Schematics)

**Files:**
- Modify: `src/character_screen.cpp`

- [ ] **Step 1: Edit `src/character_screen.cpp` — render Schematics focus**

Add a rendering branch parallel to `Refinement` from Task 11:

```cpp
        } else if (tinker_focus_ == TinkerFocus::Schematics) {
            ctx.text({.x = 0, .y = ry++, .content = "Schematics", .tag = UITag::TextBright});
            ry++;
            int idx = 0;
            for (const auto& ls : player_->learned_schematics) {
                const SchematicRecipe* r = find_schematic_recipe(ls.schematic_id);
                if (!r) continue;
                std::string cost;
                for (const auto& req : r->material_costs) {
                    const MaterialDef* def = find_material(req.material_id);
                    if (!cost.empty()) cost += " + ";
                    cost += std::to_string(req.count) + "× " + (def ? def->name : "?");
                }
                std::string line = std::string(1, '1' + idx) + ". " + r->output_name + " — " + cost;
                ctx.text({.x = 0, .y = ry++, .content = line, .tag = UITag::TextNormal});
                if (++idx >= 9) break;
            }
            if (player_->learned_schematics.empty()) {
                ctx.text({.x = 0, .y = ry++,
                    .content = "(No schematics learned. Find one in a dungeon and read it.)",
                    .tag = UITag::TextDim});
            }
            ry++;
            ctx.text({.x = 0, .y = ry++,
                .content = "[1-9] Craft   [Tab] Next focus",
                .tag = UITag::TextDim});
        }
```

- [ ] **Step 2: Edit `src/character_screen.cpp` — handle keypress**

```cpp
        if (tinker_focus_ == TinkerFocus::Schematics) {
            if (key >= '1' && key <= '9') {
                int idx = key - '1';
                if (idx < static_cast<int>(player_->learned_schematics.size())) {
                    auto sid = player_->learned_schematics[idx].schematic_id;
                    auto result = craft_schematic(sid, *player_);
                    context_message_ = result.message;
                }
                return true;
            }
        }
```

- [ ] **Step 3: Build**

```bash
cmake --build build
```

- [ ] **Step 4: Smoke-test**

```bash
./build/astra-dev --term
```

`give item schem_caltrops 1; give item scrap_metal 5; give item adhesive_resin 2`. Read the schematic. Open Tinkering tab → Schematics focus → press `1`. Expected: 3 Scrap + 1 Adhesive Resin consumed; 1 Caltrops in inventory.

- [ ] **Step 5: Commit**

```bash
git add src/character_screen.cpp
git commit -m "feat(tinkering): wire schematic crafting UI

TinkerFocus::Schematics renders learned schematics with their costs
and crafts via 1-9. Empty-state hint shown when no schematics known.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Phase 5 — Salvage, Loot Placement, Vendor Stocking

### Task 18: Rewrite `salvage_item` with tier-weighted draws

**Files:**
- Modify: `src/tinkering.cpp`

- [ ] **Step 1: Edit `src/tinkering.cpp` — replace the body of `salvage_item`**

Replace the existing implementation entirely:

```cpp
TinkerResult salvage_item(const Item& item, Player& player, std::mt19937& rng) {
    if (!player_has_skill(player, SkillId::Disassemble))
        return {false, false, "Requires Disassemble skill."};
    if (item.type == ItemType::QuestItem)
        return {false, false, "Cannot salvage quest items."};

    // Yield + tier weights per rarity (spec §5).
    int yield_min = 1, yield_max = 2;
    struct TierWeights { int t1, t2, t3; };
    TierWeights w{100, 0, 0};
    switch (item.rarity) {
        case Rarity::Common:    yield_min = 1; yield_max = 2; w = { 100,  0,  0 }; break;
        case Rarity::Uncommon:  yield_min = 2; yield_max = 3; w = {  70, 30,  0 }; break;
        case Rarity::Rare:      yield_min = 2; yield_max = 3; w = {  40, 60,  0 }; break;
        case Rarity::Epic:      yield_min = 3; yield_max = 3; w = {  20, 70, 10 }; break;
        case Rarity::Legendary: yield_min = 3; yield_max = 4; w = {   0, 60, 40 }; break;
    }
    int yield = std::uniform_int_distribution<int>(yield_min, yield_max)(rng);

    // Build per-tier candidate pools from the catalog.
    std::vector<uint32_t> t1_ids, t2_ids, t3_ids;
    for (const auto& m : material_catalog()) {
        switch (m.tier) {
            case MaterialTier::Common:   t1_ids.push_back(m.material_id); break;
            case MaterialTier::Uncommon: t2_ids.push_back(m.material_id); break;
            case MaterialTier::Rare:     t3_ids.push_back(m.material_id); break;
        }
    }

    auto draw_from = [&](const std::vector<uint32_t>& pool) -> uint32_t {
        if (pool.empty()) return 0;
        return pool[std::uniform_int_distribution<size_t>(0, pool.size() - 1)(rng)];
    };

    int total = w.t1 + w.t2 + w.t3;
    auto pick_tier = [&]() -> int {
        int roll = std::uniform_int_distribution<int>(0, total - 1)(rng);
        if (roll < w.t1) return 1;
        if (roll < w.t1 + w.t2) return 2;
        return 3;
    };

    int produced = 0;
    for (int i = 0; i < yield; ++i) {
        int tier = pick_tier();
        const auto& pool = (tier == 1) ? t1_ids : (tier == 2) ? t2_ids : t3_ids;
        uint32_t mid = draw_from(pool);
        if (mid == 0) {
            // Fallback to T1 if a tier pool is empty.
            mid = draw_from(t1_ids);
            if (mid == 0) continue;
        }

        // Merge into existing stack, else build new item.
        bool merged = false;
        for (auto& inv : player.inventory.items) {
            if (inv.id == mid) { inv.stack_count++; merged = true; break; }
        }
        if (!merged) {
            // Resolve def_id by item_def_id round-trip via build_by_def_id over all defs:
            // every material has a builder reachable through item_def_id → build_by_def_id.
            // Map material_id (Item::id) to its item_def_id by walking the loot table.
            uint16_t def_id = 0;
            for (const auto& entry : loot_table_all_entries()) {
                Item probe = build_by_def_id(entry.item_def_id);
                if (probe.id == mid) { def_id = entry.item_def_id; break; }
            }
            if (def_id == 0) continue;
            Item mat = build_by_def_id(def_id);
            player.inventory.items.push_back(std::move(mat));
        }
        ++produced;
    }

    return {true, true, "Salvaged " + item.name + ". Received " + std::to_string(produced) + " materials."};
}
```

If `loot_table_all_entries` isn't already declared in the headers `tinkering.cpp` includes, add `#include "astra/loot_table.h"` at the top of `tinkering.cpp`.

- [ ] **Step 2: Build**

```bash
cmake --build build
```

- [ ] **Step 3: Smoke-test**

```bash
./build/astra-dev --term
```

Spawn a Common-rarity weapon, salvage it (`s` in Tinkering tab). Expected: 1-2 T1 materials added to inventory. Repeat with a Rare item — should produce a mix of T1 and T2.

- [ ] **Step 4: Commit**

```bash
git add src/tinkering.cpp
git commit -m "feat(tinkering): tier-weighted salvage yields

Salvage now draws from material_catalog() weighted by item rarity per
spec §5. Common items only drop T1; Legendary items drop T2/T3 only.
Quest items still excluded.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 19: Add schematics to vendor stock + dungeon reward placement

**Files:**
- Modify: `src/loot_table.cpp`
- Possibly: dungeon reward generator (find via grep)

- [ ] **Step 1: Edit `src/loot_table.cpp` — add the 16 schematics as loot entries**

Add a new block after Cookbooks:

```cpp
        // ----- Schematics (weighted-rare drops + Heavens Above tinkerer stocks the basic 3) -----
        LootEntry{ ITEM_SCHEM_HEALING_STIM,        "schem_healing_stim",        R::Common,    R::Common,     6, {}, LootSource::Chest | LootSource::MerchantGeneral, T::Tech, 1, C::Junk },
        LootEntry{ ITEM_SCHEM_ADRENALINE_STIM,     "schem_adrenaline_stim",     R::Uncommon,  R::Uncommon,   4, {}, LootSource::Chest | LootSource::MerchantGeneral, T::Tech, 2, C::Junk },
        LootEntry{ ITEM_SCHEM_ENDURE_STIM,         "schem_endure_stim",         R::Uncommon,  R::Uncommon,   4, {}, LootSource::Chest,                              T::Tech, 2, C::Junk },
        LootEntry{ ITEM_SCHEM_FOCUS_STIM,          "schem_focus_stim",          R::Uncommon,  R::Uncommon,   4, {}, LootSource::Chest,                              T::Tech, 2, C::Junk },
        LootEntry{ ITEM_SCHEM_BERSERKER_STIM,      "schem_berserker_stim",      R::Rare,      R::Rare,       2, {}, LootSource::Chest | LootSource::BlackMarket,    T::Tech, 4, C::Junk },
        LootEntry{ ITEM_SCHEM_MEDKIT,              "schem_medkit",              R::Uncommon,  R::Uncommon,   4, {}, LootSource::Chest,                              T::Civilian, 2, C::Junk },
        LootEntry{ ITEM_SCHEM_FRAG_GRENADE,        "schem_frag_grenade",        R::Common,    R::Common,     6, {}, LootSource::Chest | LootSource::MerchantArms,   T::Military, 1, C::Junk },
        LootEntry{ ITEM_SCHEM_EMP_GRENADE,         "schem_emp_grenade",         R::Uncommon,  R::Uncommon,   4, {}, LootSource::Chest | LootSource::MerchantArms,   T::Tech, 2, C::Junk },
        LootEntry{ ITEM_SCHEM_INCENDIARY_GRENADE,  "schem_incendiary_grenade",  R::Uncommon,  R::Uncommon,   4, {}, LootSource::Chest | LootSource::MerchantArms,   T::Military, 2, C::Junk },
        LootEntry{ ITEM_SCHEM_SMOKE_GRENADE,       "schem_smoke_grenade",       R::Common,    R::Common,     5, {}, LootSource::Chest | LootSource::MerchantArms,   T::Military, 1, C::Junk },
        LootEntry{ ITEM_SCHEM_FLASHBANG,           "schem_flashbang",           R::Uncommon,  R::Uncommon,   3, {}, LootSource::Chest | LootSource::MerchantArms,   T::Military, 2, C::Junk },
        LootEntry{ ITEM_SCHEM_PROXIMITY_MINE,      "schem_proximity_mine",      R::Uncommon,  R::Uncommon,   3, {}, LootSource::Chest | LootSource::MerchantArms,   T::Military, 2, C::Junk },
        LootEntry{ ITEM_SCHEM_EMP_MINE,            "schem_emp_mine",            R::Rare,      R::Rare,       2, {}, LootSource::Chest | LootSource::BlackMarket,    T::Tech, 3, C::Junk },
        LootEntry{ ITEM_SCHEM_INCENDIARY_MINE,     "schem_incendiary_mine",     R::Rare,      R::Rare,       2, {}, LootSource::Chest | LootSource::BlackMarket,    T::Military, 3, C::Junk },
        LootEntry{ ITEM_SCHEM_DECOY_MINE,          "schem_decoy_mine",          R::Common,    R::Common,     4, {}, LootSource::Chest,                              T::Tech, 1, C::Junk },
        LootEntry{ ITEM_SCHEM_CALTROPS,            "schem_caltrops",            R::Common,    R::Common,     5, {}, LootSource::Chest | LootSource::MerchantGeneral | LootSource::ScavMerchant, T::Civilian, 1, C::Junk },
```

(Schematics use `Category::Junk` for now — future work could add a `Category::Schematic` like Cookbook gets, but Junk slot in the dispatch table works as a low-friction landing pad.)

- [ ] **Step 2: Build & verify dispatch coverage**

```bash
cmake --build build && ./build/astra-dev --term 2>&1 | head -5
```

Expected: no `[loot_table]` warnings.

- [ ] **Step 3: Add the 3 basic schematics to Heavens Above tinkerer stock**

```bash
grep -rn "tinker\|station_keeper\|Heavens Above" /Users/jeffrey/dev/crawler/src/npcs/ /Users/jeffrey/dev/crawler/src/ 2>/dev/null | head -20
```

Find the file that defines the Heavens Above tinkerer NPC's stock manifest. Add three `StockManifestEntry{ Mode::Always, ITEM_SCHEM_FRAG_GRENADE, ..., 1, 0 }` entries for `ITEM_SCHEM_FRAG_GRENADE`, `ITEM_SCHEM_HEALING_STIM`, `ITEM_SCHEM_CALTROPS`.

If you can't find a clear "Heavens Above tinkerer" NPC, list the candidate files and inspect `assemble_stock` callers. The pattern is in `src/shop.cpp` or an NPC-specific cpp.

- [ ] **Step 4: Build & smoke-test**

```bash
cmake --build build
./build/astra-dev --term
```

Visit Heavens Above, talk to the tinkerer, open trade. Expected: the 3 basic schematics are stocked.

- [ ] **Step 5: Commit**

```bash
git add src/loot_table.cpp src/<tinkerer-stock-file>
git commit -m "feat(loot): schematics drop from chests/merchants + tinkerer stocks 3 basics

16 schematic loot entries with rarity-tier weighting. Heavens Above
tinkerer always stocks Frag Grenade, Healing Stim, and Caltrops
schematics so day-one players can craft at least one consumable.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 20: Place 1 schematic per generated dungeon as a special reward

**Files:**
- Locate via grep: dungeon reward / chest-near-stairs generator
- Modify: that file

- [ ] **Step 1: Find the dungeon reward placement code**

```bash
grep -rn "stairs_down\|special_reward\|chest_near_stairs\|reward_chest" /Users/jeffrey/dev/crawler/src/dungeon/ /Users/jeffrey/dev/crawler/src/generators/ 2>/dev/null | head -20
grep -rn "Cookbook\|ITEM_COOKBOOK" /Users/jeffrey/dev/crawler/src/dungeon/ /Users/jeffrey/dev/crawler/src/generators/ 2>/dev/null | head -10
```

Identify the function that places a single reward per dungeon.

- [ ] **Step 2: Add a schematic option to that picker**

In the reward placement function, add a 1-in-N chance to place a randomly-selected schematic. Pseudocode (adapt to actual local style):

```cpp
// Roll one schematic to place per dungeon. Pick uniformly across the 16 schem ids.
constexpr uint16_t kSchemIds[] = {
    ITEM_SCHEM_HEALING_STIM, ITEM_SCHEM_ADRENALINE_STIM, ITEM_SCHEM_ENDURE_STIM,
    ITEM_SCHEM_FOCUS_STIM, ITEM_SCHEM_BERSERKER_STIM, ITEM_SCHEM_MEDKIT,
    ITEM_SCHEM_FRAG_GRENADE, ITEM_SCHEM_EMP_GRENADE, ITEM_SCHEM_INCENDIARY_GRENADE,
    ITEM_SCHEM_SMOKE_GRENADE, ITEM_SCHEM_FLASHBANG,
    ITEM_SCHEM_PROXIMITY_MINE, ITEM_SCHEM_EMP_MINE, ITEM_SCHEM_INCENDIARY_MINE,
    ITEM_SCHEM_DECOY_MINE, ITEM_SCHEM_CALTROPS,
};
uint16_t pick = kSchemIds[std::uniform_int_distribution<size_t>(0, std::size(kSchemIds) - 1)(rng)];
Item schem = build_by_def_id(pick);
// Place at the reward tile (use the same path as existing reward placement)
```

- [ ] **Step 3: Build**

```bash
cmake --build build
```

- [ ] **Step 4: Smoke-test**

Generate a few dungeons (`--dev`'s map regen or fresh saves). Confirm a schematic appears on the reward tile of at least one.

- [ ] **Step 5: Commit**

```bash
git add src/<dungeon-reward-file>
git commit -m "feat(dungeon): place one schematic per dungeon as special reward

Uniform draw across all 16 schematics. Sits alongside other reward
candidates in the existing placement path.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Phase 6 — Save / Load

### Task 21: Bump SAVE_FILE_VERSION and add `learned_schematics` serialization

**Files:**
- Modify: `include/astra/save_file.h`
- Modify: `src/save_file.cpp`

- [ ] **Step 1: Edit `include/astra/save_file.h` — bump version**

Change:

```cpp
inline constexpr uint32_t SAVE_FILE_VERSION = 48;   // v48: toggleable items + module_kind
```

to:

```cpp
inline constexpr uint32_t SAVE_FILE_VERSION = 49;   // v49: tinkering expansion (learned_schematics)
```

- [ ] **Step 2: Edit `src/save_file.cpp` — add write side for learned_schematics**

Find the existing `learned_blueprints` write (around line 691). Immediately after the loop that serializes blueprints, add:

```cpp
    // v49: learned_schematics
    w.write_u32(static_cast<uint32_t>(p.learned_schematics.size()));
    for (const auto& ls : p.learned_schematics) {
        w.write_u16(ls.schematic_id);
        w.write_string(ls.name);
        w.write_string(ls.description);
    }
```

- [ ] **Step 3: Edit `src/save_file.cpp` — add read side**

Find the matching read for `learned_blueprints` (around line 1550). Immediately after the loop, add:

```cpp
    // v49: learned_schematics
    uint32_t ls_count = r.read_u32();
    p.learned_schematics.resize(ls_count);
    for (uint32_t i = 0; i < ls_count; ++i) {
        p.learned_schematics[i].schematic_id = r.read_u16();
        p.learned_schematics[i].name = r.read_string();
        p.learned_schematics[i].description = r.read_string();
    }
```

- [ ] **Step 4: Add `teaches_schematic_id` to item save format**

Find the `teaches_recipe_id` write site (around line 388):

```cpp
    w.write_u16(item.teaches_recipe_id);
```

Add after it:

```cpp
    w.write_u16(item.teaches_schematic_id);  // v49
```

And for read side (around line 505):

```cpp
    item.teaches_recipe_id = r.read_u16();
    item.teaches_schematic_id = r.read_u16();  // v49
```

- [ ] **Step 5: Build**

```bash
cmake --build build
```

- [ ] **Step 6: Smoke-test save/load**

```bash
./build/astra-dev --term
```

Start fresh game. Spawn `give item schem_caltrops 1`. Read it. Confirm Schematics focus shows Caltrops. Save. Quit. Reload. Confirm Caltrops still in Schematics list.

If the save was made on an old binary, it must be rejected — verify by running on an existing v48 save (if any) and confirming the rejection message in stderr.

- [ ] **Step 7: Commit**

```bash
git add include/astra/save_file.h src/save_file.cpp
git commit -m "feat(save): v49 schema — learned_schematics + teaches_schematic_id

Per project rule, schema bumps reject older saves at load with a
clear message; no migration shim.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Phase 7 — Documentation

### Task 22: Write `docs/tinkering.md`

**Files:**
- Create: `docs/tinkering.md`

- [ ] **Step 1: Create the file with all 7 sections from spec §7.1**

```markdown
# Tinkering

Astra's crafting system: repair, enhance, refine, synthesize, and craft consumables at any workbench. Skills `Basic Repair` (refine/repair/enhance) and `Cat_Tinkering` (analyze/salvage/synthesize/craft) gate access.

## Materials

Materials are the reagents consumed by every recipe. They drop from monsters, chests, and merchants, and can also be produced via salvage and refinement.

### Tier 1 — Common

| Glyph | Name | ID | Sell | Notes |
|---|---|---|---|---|
| `~` Dark Gray | Scrap Metal | 30 | 1 | Junk-typed; sellable + reagent |
| `~` Dark Gray | Broken Circuit | 31 | 2 | Junk-typed |
| `~` Dark Gray | Empty Casing | 32 | 1 | Junk-typed |
| `,` Yellow | Copper Wire | 7010 | 2 | Stack name "Strand of Copper Wire" |
| `,` White | Polymer Strip | 7011 | 2 | Flexible plastic |
| `,` Cyan | Glass Shard | 7012 | 1 | Optic / vial reagent |
| `,` Yellow | Adhesive Resin | 7013 | 2 | Bonding agent |
| `,` Blue | Coolant Vial | 7014 | 3 | Thermal reagent |

### Tier 2 — Uncommon

| Glyph | Name | ID | Sell | Notes |
|---|---|---|---|---|
| `+` Cyan | Nano-Fiber | 7001 | 8 | Repair reagent |
| `+` Yellow | Power Core | 7002 | 12 | Energy recipes |
| `+` Green | Circuit Board | 7003 | 10 | Tech recipes |
| `+` White | Alloy Ingot | 7004 | 10 | Heavy/armor recipes |
| `~` Yellow | Spare Parts | 47 | 6 | Junk-typed; mechanical |
| `~` Cyan | Circuitry | 48 | 8 | Junk-typed; in most recipes |
| `+` White | Nano Lattice | 7020 | 14 | Advanced structural |
| `+` Cyan | Polished Lens | 7021 | 12 | Optics |
| `+` Yellow | Micro-Servo | 7022 | 14 | Mechanical |
| `+` Red | Plasma Cartridge | 7023 | 16 | Energy weapon |

### Tier 3 — Rare

| Glyph | Name | ID | Sell | Notes |
|---|---|---|---|---|
| `*` Magenta | Quantum Resonance Crystal | 7030 | 50 | AI Module input |
| `*` White | Strange Strobing Crystal | 7031 | 60 | Future legendary |
| `*` Yellow | Prime Catalyst | 7032 | 55 | Future exotic |
| `*` Cyan | Prime Filament | 7033 | 55 | Future exotic |
| `*` Magenta | Voidshard | 7034 | 70 | Future warp |
| `*` Blue | Phase Coil | 7035 | 65 | Future phase |

## Refinement (junk → T2)

Performed at the workbench under the Refinement focus. Requires `Basic Repair`.

| Inputs | Output |
|---|---|
| 3× Scrap Metal | 1× Alloy Ingot |
| 2× Broken Circuit + 1× Copper Wire | 1× Circuit Board |
| 4× Empty Casing + 1× Adhesive Resin | 1× Nano-Fiber |
| 2× Spare Parts + 1× Copper Wire | 1× Power Core |
| 1× Broken Circuit + 1× Spare Parts + 1× Copper Wire | 1× Circuitry |
| 2× Glass Shard + 1× Polymer Strip | 1× Polished Lens |
| 2× Spare Parts + 1× Coolant Vial | 1× Micro-Servo |
| 3× Nano-Fiber + 1× Polymer Strip | 1× Nano Lattice |
| 2× Power Core + 1× Coolant Vial | 1× Plasma Cartridge |

## Blueprints (paired)

Equipment recipes use **two** blueprints learned by `analyze`-ing existing equipment. The 12 blueprints by category:

- **Ranged:** Plasma Emitter, Grip Assembly, Power Conduit
- **Melee:** Blade Housing, Hilt Assembly, Edge Material
- **Armor:** Plating Alloy, Padding Weave, Joint Mechanism
- **Accessory:** Optic Module, Thruster Core, Storage Frame

## Synthesis recipes (15)

| Recipe | Material Costs |
|---|---|
| Plasma Edge | 2 Scrap + 1 Circuitry + 1 Power Core + 1 Alloy Ingot + 1 Plasma Cartridge |
| Thruster Plate | 3 Scrap + 1 Circuitry + 2 Alloy Ingot + 1 Micro-Servo |
| Targeting Array | 1 Scrap + 1 Circuitry + 1 Circuit Board + 1 Polished Lens + 2 Copper Wire |
| Dual-Edge | 3 Scrap + 1 Circuitry + 2 Alloy Ingot + 1 Nano-Fiber |
| Reinforced Pack | 2 Scrap + 2 Nano-Fiber + 1 Polymer Strip + 1 Adhesive Resin |
| Overcharged Engine | 2 Scrap + 1 Circuitry + 2 Power Core + 1 Coolant Vial + 1 Plasma Cartridge |
| Articulated Armor | 3 Scrap + 1 Circuitry + 2 Alloy Ingot + 1 Micro-Servo + 1 Nano Lattice |
| Guided Blaster | 2 Scrap + 1 Circuitry + 1 Power Core + 1 Polished Lens + 1 Plasma Cartridge |
| Combat Gauntlet | 2 Scrap + 1 Circuitry + 1 Alloy Ingot + 1 Nano-Fiber + 1 Micro-Servo |
| Armored Blade | 4 Scrap + 2 Alloy Ingot + 1 Nano-Fiber |
| Reinforced Casing (mat) | 2 Scrap + 1 Alloy Ingot + 1 Polymer Strip |
| Receptor Plate (mat) | 1 Scrap + 1 Circuitry + 1 Copper Wire + 1 Polished Lens |
| Brass Conduit (mat) | 1 Scrap + 1 Copper Wire + 1 Power Core |
| AI Module (mat) | 2 Scrap + 1 Circuitry + 1 Circuit Board + 1 Nano Lattice + 1 Quantum Resonance Crystal |
| Light Sensor (mat) | 1 Scrap + 1 Circuitry + 1 Circuit Board + 1 Polished Lens |

## Schematics & Consumables

Schematics are single-use pickups. Read a schematic to permanently learn the recipe; the schematic is consumed. Consumables crafted from schematics currently exist but their `use` effect is deferred to the next spec.

### Stims

| Item | Recipe Cost |
|---|---|
| Healing Stim | 1 Scrap + 1 Empty Casing + 1 Nano-Fiber + 1 Glass Shard |
| Adrenaline Stim | 1 Scrap + 1 Empty Casing + 1 Power Core + 1 Coolant Vial |
| Endure Stim | 1 Scrap + 1 Empty Casing + 1 Nano-Fiber + 1 Adhesive Resin |
| Focus Stim | 1 Scrap + 1 Empty Casing + 1 Polished Lens + 1 Coolant Vial |
| Berserker Stim | 2 Scrap + 1 Empty Casing + 1 Power Core + 1 Plasma Cartridge |
| Medkit | 2 Scrap + 1 Polymer Strip + 2 Nano-Fiber + 1 Adhesive Resin |

### Grenades

| Item | Recipe Cost |
|---|---|
| Frag Grenade | 2 Scrap + 1 Empty Casing + 1 Power Core + 1 Adhesive Resin |
| EMP Grenade | 1 Scrap + 1 Empty Casing + 1 Circuitry + 1 Circuit Board + 1 Copper Wire |
| Incendiary Grenade | 2 Scrap + 1 Empty Casing + 1 Coolant Vial + 1 Plasma Cartridge |
| Smoke Grenade | 1 Scrap + 1 Empty Casing + 1 Polymer Strip + 1 Adhesive Resin |
| Flashbang | 1 Scrap + 1 Empty Casing + 1 Circuitry + 1 Glass Shard + 1 Power Core |

### Mines

| Item | Recipe Cost |
|---|---|
| Proximity Mine | 2 Scrap + 1 Empty Casing + 1 Circuitry + 1 Power Core + 1 Spare Parts |
| EMP Mine | 1 Scrap + 1 Empty Casing + 1 Circuitry + 1 Circuit Board + 1 Spare Parts |
| Incendiary Mine | 2 Scrap + 1 Empty Casing + 1 Circuitry + 1 Plasma Cartridge + 1 Spare Parts |
| Decoy Mine | 1 Scrap + 1 Empty Casing + 1 Circuitry + 1 Copper Wire + 1 Spare Parts |
| Caltrops | 3 Scrap + 1 Adhesive Resin |

## Salvage rules

Salvage destroys an item and returns materials. Skill: `Disassemble`.

| Item rarity | Yield | Tier weights |
|---|---|---|
| Common | 1-2 | 100% T1 |
| Uncommon | 2-3 | 70% T1 / 30% T2 |
| Rare | 2-3 | 40% T1 / 60% T2 |
| Epic | 3 | 20% T1 / 70% T2 / 10% T3 |
| Legendary | 3-4 | 60% T2 / 40% T3 |

Quest items cannot be salvaged.
```

- [ ] **Step 2: Verify the file renders**

```bash
ls -la /Users/jeffrey/dev/crawler/docs/tinkering.md
```

- [ ] **Step 3: Commit**

```bash
git add docs/tinkering.md
git commit -m "docs(tinkering): add canonical tinkering reference

Materials, refinement, blueprints, synthesis, schematics, consumables,
salvage rules — single source of truth.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 23: Update `docs/mechanics.md`, `docs/items.md`, `docs/roadmap.md`

**Files:**
- Modify: `docs/mechanics.md`
- Modify: `docs/items.md`
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Edit `docs/mechanics.md` — add Tinkering pointer**

Locate the table-of-contents or systems list. Add a "Tinkering" subsection or bullet:

```markdown
## Tinkering

Crafting system covering repair, enhancement, refinement, synthesis, and schematic crafting. See [tinkering.md](tinkering.md) for the full materials catalog and all recipes.
```

- [ ] **Step 2: Edit `docs/items.md` — replace Crafting Materials table with a pointer**

Find the existing `## Crafting Materials` section (around line 176-185). Replace with:

```markdown
## Crafting Materials

`Category::CraftingMaterial`. Consumed by tinkering recipes. **24 materials across 3 tiers** — see [tinkering.md](tinkering.md) for the full catalog and recipe details.

Quick summary:
- T1 Common (8): Scrap Metal*, Broken Circuit*, Empty Casing*, Copper Wire, Polymer Strip, Glass Shard, Adhesive Resin, Coolant Vial
- T2 Uncommon (10): Nano-Fiber, Power Core, Circuit Board, Alloy Ingot, Spare Parts*, Circuitry*, Nano Lattice, Polished Lens, Micro-Servo, Plasma Cartridge
- T3 Rare (6): Quantum Resonance Crystal, Strange Strobing Crystal, Prime Catalyst, Prime Filament, Voidshard, Phase Coil

\* = also dropped/sold as `ItemType::Junk`; doubles as crafting reagent.
```

Annotate the existing Junk table entries (Scrap Metal, Broken Circuit, Empty Casing, Spare Parts, Circuitry) with "(also: crafting reagent)".

- [ ] **Step 3: Edit `docs/roadmap.md`**

Find the tinkering line item. Mark it complete. Add a follow-up entry:

```markdown
- [ ] Consumable use code (stims/grenades/mines) — next spec after tinkering expansion
```

- [ ] **Step 4: Commit**

```bash
git add docs/mechanics.md docs/items.md docs/roadmap.md
git commit -m "docs: cross-reference tinkering.md from mechanics, items, roadmap

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Phase 8 — Final Verification

### Task 24: Full smoke run + dispatch coverage check

**Files:**
- None (verification only).

- [ ] **Step 1: Build clean**

```bash
cmake --build build --clean-first
```

Expected: clean compile, no warnings new to this branch.

- [ ] **Step 2: Run dispatch coverage in DEV mode**

```bash
./build/astra-dev --term 2>&1 | head -20
```

Expected: no `[loot_table]` warnings about missing builders or duplicate ids.

- [ ] **Step 3: Smoke-test the full crafting loop**

In game (dev console):

```
give item scrap_metal 10
give item empty_casing 5
give item nano_fiber 3
give item glass_shard 3
give item schem_healing_stim 1
```

1. **Read schematic:** open inventory, use Schem Healing Stim. Expected: schematic disappears, "Learned schematic: Healing Stim" message, Schematics focus now shows Healing Stim.
2. **Refine:** Tinkering tab → Refinement focus → press 1 (Smelt Alloy Ingot). Expected: 3 Scrap consumed, 1 Alloy Ingot in inventory.
3. **Craft consumable:** Schematics focus → press 1 (Healing Stim). Expected: 1 Scrap + 1 Empty Casing + 1 Nano-Fiber + 1 Glass Shard consumed; 1 Healing Stim in inventory.
4. **Salvage a Common item:** spawn a Padded Vest, salvage it. Expected: 1-2 T1 materials in inventory.
5. **Save / Quit / Reload:** Schematics focus still shows Healing Stim; Healing Stim item still in inventory.

- [ ] **Step 4: Confirm legacy synthesis still works**

Use any equipment you have an existing blueprint for (or analyze a fresh weapon to learn one), and synthesize. Expected: synthesis still works using new material costs (will need the new ingredients available).

- [ ] **Step 5: Final commit**

If any small fixups were needed during smoke testing, commit them with a message like:

```bash
git commit -m "fix(tinkering): smoke-test fixups for end-to-end loop"
```

If everything passed cleanly without changes, this task ends with no commit.

---

## Self-Review

**Spec coverage check:**

- [x] §1 Data Model: Tasks 2, 3, 12 (types, struct migration, ItemType extensions, player state).
- [x] §2 Material Catalog: Tasks 4, 5, 6, 7 (item ids, builders, catalog population).
- [x] §3.1 Refinement recipes: Task 10.
- [x] §3.2 Revised synthesis recipes: Task 8.
- [x] §4 Schematic crafting: Tasks 12-17 (types, consumables, schematics, recipes, read-handler, UI).
- [x] §5 Salvage update: Task 18.
- [x] §6 Loot & vendor placement: Tasks 9, 19, 20.
- [x] §7 Documentation: Tasks 22, 23.
- [x] §8 UI (two-tab Codex): Tasks 11, 17 add Refinement/Schematics focuses; the spec's broader "two-tab Codex" naming is realized as parallel TinkerFocus values rather than a literal tab UI rebuild — kept lean since the existing TinkerFocus model already supports the same listing semantics. No information is lost.
- [x] §9 Save/Load: Task 21.
- [x] §10 Testing: woven into per-task smoke tests + Task 24 final smoke run.

**Type / signature consistency:** All recipe types share `std::vector<MaterialReq>`. Material ids are `uint32_t` everywhere (Item::id-based). Schematic ids are `uint16_t` everywhere. `learned_schematics` named consistently. Function signatures (`refine_item`, `craft_schematic`, `learn_schematic`) match between header and implementation tasks.

**Placeholder scan:** Task 16 (use_item handler) and Task 19 (tinkerer NPC stocking) and Task 20 (dungeon reward placement) require locating files via grep before editing — these are real-world fact-finding steps, not "TBD"s. Each gives the exact grep command and the exact code to add once the file is found.

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-04-27-tinkering-expansion.md`. Two execution options:

**1. Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration.

**2. Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints.

Which approach?
