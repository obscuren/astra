# Tinkering Expansion — Materials, Refinement, and Schematic Crafting

**Date:** 2026-04-27
**Status:** Design

## Goals

1. Expand the crafting material catalog from 4 to 24 items across 3 tiers, leaving room for future content.
2. Promote junk items (Scrap Metal, Broken Circuit, Empty Casing, Spare Parts, Circuitry) to dual-use: still sellable, also reagents.
3. Make Scrap a baseline reagent in nearly every recipe and Circuitry a reagent in most "smart" recipes.
4. Introduce a refinement pipeline that converts junk into Tier 2 materials.
5. Revise the 15 existing synthesis recipes against the new catalog.
6. Add a schematic-based crafting path for consumables (stims, grenades, mines).
7. Create the consumable items themselves so they exist in inventory; their gameplay effects ("use" code) are out of scope and handled by the next spec.
8. Publish a canonical `docs/tinkering.md` reference, linked from `docs/mechanics.md` and `docs/items.md`.

## Non-Goals

- Throw / use / detonation code for grenades, mines, and stims. The 16 consumable items exist but `use_item` returns "not yet implemented".
- Status-effect framework (buffs, burning, EMP-disabled, stun). Defined by the next spec.
- AoE damage and mine trigger systems.
- Reworking the analyze flow or blueprint catalog (still 12 paired blueprints).
- Migration of older saves (per project rule: schema bumps reject old saves until v1.0).

## High-Level Architecture

```
docs/tinkering.md  ◀── new canonical reference
        │
        ▼
┌─────────────────────────────────────────────────────────┐
│ MaterialDef catalog (24 entries, 3 tiers)               │
│   ─ id, name, tier, glyph, color, sell_value             │
└─────────────────────────────────────────────────────────┘
        │
        ├── used by: SynthesisRecipe.material_costs
        ├── used by: RefinementRecipe.material_costs
        ├── used by: SchematicRecipe.material_costs
        ├── used by: salvage_item (tier-weighted yields)
        └── used by: items.md materials table
```

`SynthesisRecipe`, `RefinementRecipe`, and `SchematicRecipe` all share a `std::vector<MaterialReq>` cost shape. The fixed `material_cost[4]` array is removed.

## Section 1 — Data Model

### 1.1 New types in `tinkering.h`

```cpp
struct MaterialReq { uint32_t material_id; int count; };

enum class MaterialTier : uint8_t { Common = 1, Uncommon, Rare };

struct MaterialDef {
    uint32_t material_id;          // == Item::id (used for stack matching)
    const char* name;
    MaterialTier tier;
    char glyph;
    uint8_t color;                 // ColorId
    int sell_value;
    bool is_junk_typed;            // true for Scrap, Broken Circuit, etc.
};

const std::vector<MaterialDef>& material_catalog();
const MaterialDef* find_material(uint32_t id);
```

`SynthesisRecipe::material_cost[4]` is replaced by `std::vector<MaterialReq> material_costs`.

### 1.2 New crafting paths

```cpp
struct RefinementRecipe {
    const char* name;                       // "Smelt Alloy Ingot"
    std::vector<MaterialReq> inputs;
    uint32_t output_id;
    int output_count;
};
const std::vector<RefinementRecipe>& refinement_recipes();
TinkerResult refine(const RefinementRecipe&, Player&);

struct SchematicRecipe {
    uint32_t schematic_id;          // 7100-7115
    uint32_t output_id;             // 7200-7215
    const char* output_name;
    const char* output_desc;
    std::vector<MaterialReq> material_costs;
    int output_count = 1;
};
const std::vector<SchematicRecipe>& schematic_recipes();
TinkerResult craft_schematic(const SchematicRecipe&, Player&);
```

### 1.3 New player state

```cpp
struct LearnedSchematic {
    uint32_t schematic_id;
    std::string name;
    std::string description;
};
std::vector<LearnedSchematic> learned_schematics; // on Player
```

Mirrors `learned_blueprints`. Persisted via the same save path.

### 1.4 New `ItemType` value

```cpp
enum class ItemType { ..., Schematic, Consumable };
```

`Schematic` is a single-use pickup that, when read, populates `learned_schematics` and is consumed (mirrors Cookbook). `Consumable` is the inert grenade/mine/stim that sits in inventory.

### 1.5 ID ranges

| Range | Purpose |
|---|---|
| 30, 31, 32, 47, 48 | Existing junk-typed reagents (Scrap, Broken Circuit, Empty Casing, Spare Parts, Circuitry) |
| 7001-7004 | Existing T2 materials (Nano-Fiber, Power Core, Circuit Board, Alloy Ingot) |
| 7010-7019 | New T1 materials |
| 7020-7029 | New T2 materials |
| 7030-7039 | T3 materials |
| 7100-7115 | Schematics |
| 7200-7215 | Consumables |

## Section 2 — Material Catalog

### Tier 1 — Common (8)

| Material | ID | Type | Glyph | Sell | Notes |
|---|---|---|---|---|---|
| Scrap Metal | 30 | Junk | `~` Dark Gray | 1 | promoted: also a reagent |
| Broken Circuit | 31 | Junk | `~` Dark Gray | 2 | promoted |
| Empty Casing | 32 | Junk | `~` Dark Gray | 1 | promoted |
| Copper Wire | 7010 | CraftingMaterial | `,` Yellow | 2 | stack name "Strand of Copper Wire" |
| Polymer Strip | 7011 | CraftingMaterial | `,` White | 2 | flexible plastic |
| Glass Shard | 7012 | CraftingMaterial | `,` Cyan | 1 | optic / vial reagent |
| Adhesive Resin | 7013 | CraftingMaterial | `,` Orange | 2 | bonding agent |
| Coolant Vial | 7014 | CraftingMaterial | `,` Blue | 3 | thermal reagent |

### Tier 2 — Uncommon (10)

| Material | ID | Type | Glyph | Sell | Notes |
|---|---|---|---|---|---|
| Nano-Fiber | 7001 | CraftingMaterial | `+` Cyan | 8 | existing |
| Power Core | 7002 | CraftingMaterial | `+` Yellow | 12 | existing |
| Circuit Board | 7003 | CraftingMaterial | `+` Green | 10 | existing |
| Alloy Ingot | 7004 | CraftingMaterial | `+` White | 10 | existing |
| Spare Parts | 47 | Junk | `~` Yellow | 6 | promoted |
| Circuitry | 48 | Junk | `~` Cyan | 8 | promoted; in most recipes |
| Nano Lattice | 7020 | CraftingMaterial | `+` Bright White | 14 | structural advanced |
| Polished Lens | 7021 | CraftingMaterial | `+` Bright Cyan | 12 | optics |
| Micro-Servo | 7022 | CraftingMaterial | `+` Bright Yellow | 14 | mechanical |
| Plasma Cartridge | 7023 | CraftingMaterial | `+` Bright Red | 16 | energy weapon |

### Tier 3 — Rare (6)

| Material | ID | Type | Glyph | Sell | Notes |
|---|---|---|---|---|---|
| Quantum Resonance Crystal | 7030 | CraftingMaterial | `*` Bright Magenta | 50 | exotic energy |
| Strange Strobing Crystal | 7031 | CraftingMaterial | `*` Bright White | 60 | unstable, legendary recipes |
| Prime Catalyst | 7032 | CraftingMaterial | `*` Bright Yellow | 55 | exotic energy reagent |
| Prime Filament | 7033 | CraftingMaterial | `*` Bright Cyan | 55 | exotic structural |
| Voidshard | 7034 | CraftingMaterial | `*` Magenta | 70 | warp-space matter |
| Phase Coil | 7035 | CraftingMaterial | `*` Bright Blue | 65 | partially out-of-phase |

5 T3 materials are intentionally seed-only this spec (Strange Strobing Crystal, Prime Catalyst, Prime Filament, Voidshard, Phase Coil) — they drop and stash for future content. The remaining 19 are referenced by recipes in §3 and §4.

Polished Lens, Micro-Servo, Plasma Cartridge, Nano Lattice, and the four existing T2 materials are reachable via refinement (§3.1) and used as recipe inputs. Quantum Resonance Crystal is the only T3 material with a recipe role this spec (AI Module input).

## Section 3 — Refinement & Synthesis Recipes

### 3.1 Refinement recipes

All require the Basic Repair skill (same as enhance/repair). Performed at any workbench under a new tinker submenu `[t] → [r]efine`.

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

### 3.2 Revised synthesis recipes (15 existing)

Hand-tuned. Every recipe takes Scrap; 13 of 15 take Circuitry. Reagents are flavor-led.

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

Reinforced Pack and Armored Blade are deliberately Circuitry-free (theme: pure mechanical / leather).

## Section 4 — Schematic Crafting (16 consumables)

Each schematic is a single-use pickup (`ItemType::Schematic`) that, when read, adds a `LearnedSchematic` to the player. The output `Consumable` exists in inventory and stacks; using it is out of scope.

### 4.1 Stims (6)

| Item | Recipe Cost |
|---|---|
| Healing Stim | 1 Scrap + 1 Empty Casing + 1 Nano-Fiber + 1 Glass Shard |
| Adrenaline Stim | 1 Scrap + 1 Empty Casing + 1 Power Core + 1 Coolant Vial |
| Endure Stim | 1 Scrap + 1 Empty Casing + 1 Nano-Fiber + 1 Adhesive Resin |
| Focus Stim | 1 Scrap + 1 Empty Casing + 1 Polished Lens + 1 Coolant Vial |
| Berserker Stim | 2 Scrap + 1 Empty Casing + 1 Power Core + 1 Plasma Cartridge |
| Medkit | 2 Scrap + 1 Polymer Strip + 2 Nano-Fiber + 1 Adhesive Resin |

### 4.2 Grenades (5)

| Item | Recipe Cost |
|---|---|
| Frag Grenade | 2 Scrap + 1 Empty Casing + 1 Power Core + 1 Adhesive Resin |
| EMP Grenade | 1 Scrap + 1 Empty Casing + 1 Circuitry + 1 Circuit Board + 1 Copper Wire |
| Incendiary Grenade | 2 Scrap + 1 Empty Casing + 1 Coolant Vial + 1 Plasma Cartridge |
| Smoke Grenade | 1 Scrap + 1 Empty Casing + 1 Polymer Strip + 1 Adhesive Resin |
| Flashbang | 1 Scrap + 1 Empty Casing + 1 Circuitry + 1 Glass Shard + 1 Power Core |

### 4.3 Mines (5)

| Item | Recipe Cost |
|---|---|
| Proximity Mine | 2 Scrap + 1 Empty Casing + 1 Circuitry + 1 Power Core + 1 Spare Parts |
| EMP Mine | 1 Scrap + 1 Empty Casing + 1 Circuitry + 1 Circuit Board + 1 Spare Parts |
| Incendiary Mine | 2 Scrap + 1 Empty Casing + 1 Circuitry + 1 Plasma Cartridge + 1 Spare Parts |
| Decoy Mine | 1 Scrap + 1 Empty Casing + 1 Circuitry + 1 Copper Wire + 1 Spare Parts |
| Caltrops | 3 Scrap + 1 Adhesive Resin |

Caltrops is the cheap mine: pure mechanical, no electronics, T1 only.

## Section 5 — Salvage Update

`salvage_item` (currently hard-codes 4 builders) is rewritten to draw from `material_catalog()` with tier-weighted probabilities.

| Item rarity | Yield | Tier weights |
|---|---|---|
| Common | 1-2 mats | T1 only |
| Uncommon | 2-3 | 70% T1 / 30% T2 |
| Rare | 2-3 | 40% T1 / 60% T2 |
| Epic | 3 | 20% T1 / 70% T2 / 10% T3 |
| Legendary | 3-4 | 60% T2 / 40% T3 |

T3 materials are excluded from salvage of <Epic items so they remain genuinely rare.

Quest items still cannot be salvaged. Junk-typed reagents (Scrap, Broken Circuit, etc.) are eligible salvage outputs (they merge into existing junk stacks).

## Section 6 — Loot & Vendor Placement

- **T1 materials** — drop from any monster / chest at low rate; cheap from any vendor.
- **T2 materials** — drop from mid+ dungeon depths (existing 4 keep current spawn logic; new 4 added to the same loot tables); occasional vendor stock.
- **T3 materials** — boss drops, deep dungeon levels (depth ≥ 5), and quest rewards only. Never on standard vendor inventories.
- **Schematics** — exactly 1 schematic per generated dungeon as a "special reward" tile (chests near stairs-down, locked rooms). Three basic schematics (Frag Grenade, Healing Stim, Caltrops) are also stocked occasionally by the Heavens Above tinkerer NPC so a fresh player can learn at least one consumable on day one.

Implementation: extend `loot_table.cpp` with new tier buckets keyed by `MaterialTier`, and add a `kind=Schematic` slot to dungeon-reward generators.

## Section 7 — Documentation Deliverables

### 7.1 New `docs/tinkering.md`

Canonical reference. Sections:
1. Overview (what tinkering is, where to do it).
2. Materials — full 24-entry table from §2, grouped by tier.
3. Refinement recipes — table from §3.1.
4. Blueprints — paired-blueprint catalog (the 12 existing).
5. Synthesis recipes — table from §3.2.
6. Schematics & consumables — tables from §4.
7. Salvage rules — table from §5.

### 7.2 Cross-references

- `docs/mechanics.md` — add a new "Tinkering" subsection with a one-line summary and a link to `docs/tinkering.md`.
- `docs/items.md` — replace the small Crafting Materials table with a brief "see `tinkering.md` for the full materials catalog and recipes" pointer; promoted junk entries get a "(also: crafting reagent)" annotation.
- `docs/roadmap.md` — check off the tinkering-expansion line item and add a follow-up entry for "consumable use code (next spec)".

## Section 8 — UI

The existing "learned blueprints" panel becomes a two-tab **Codex**:
- **Blueprints** tab — current paired-blueprint list.
- **Schematics** tab — learned schematic list.

The tinker workbench gains an action map: `[a]nalyze [s]alvage [e]nhance [r]epair [R]efine [c]raft-blueprint [S]chematic`. (Capital letters used where the lower-case is taken; final binding to be picked during plan phase.) The schematic crafting flow is symmetric to synthesis but takes one input (the chosen learned schematic) instead of two blueprints.

## Section 9 — Save / Load

Save schema bumps to add:
- `learned_schematics` array on Player.
- New material item ids in inventory stacks (handled by existing item-id serialization).

Per project rule, schema bumps reject older saves at load time with a clear message; no migration shim.

## Section 10 — Testing

- Unit tests: each refinement recipe consumes inputs and produces output; schematic crafting consumes inputs and produces output and respects `learned_schematics`; salvage tier-weight bands are sane.
- Manual: dev-spawn each new material and each new schematic; read schematic; craft each consumable once; salvage one item of each rarity; refine one of each pipeline.
- Regression: existing 15 synthesis recipes still produce items with sensible stats; existing analyze/repair/enhance flows untouched.

## Open Questions / Deferred

- Whether the Codex tabs need filtering / search (defer until population grows past ~30 entries).
- Whether refinement should have a chance of failure or material loss (defer; flat 100% for now).
- Schematic vendor stocking probabilities (set placeholders; tune after first playtest).
- Use-code spec for stims/grenades/mines (next spec, written immediately after this lands).
