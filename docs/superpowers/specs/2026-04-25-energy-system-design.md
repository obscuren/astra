# Energy System — Design

**Date:** 2026-04-25
**Status:** Approved (design phase)

## Goal

Replace the throwaway-cell ranged-weapon "reload" with a normalized energy system shared by every player-side energy consumer (ranged weapons, energy shields, future gadgets). Cells become persistent items with their own charge state, refillable in-world via Solar Panels and recharge stations. The system must be cheap to extend: new consumers and new energy mods drop into the same plumbing without bespoke code paths.

NPCs are out of scope — their ranged attacks remain raw damage rolls with no energy bookkeeping.

## Non-goals

- Gadget content (item defs, balancing, animations). Framework supports future gadgets; no gadgets ship in this spec.
- Save migration shims. Per project policy, old saves are rejected when the schema bumps.
- Environmental gating beyond the indoor/outdoor binary (no day/night, no overcast, no shadow tiles).
- Multi-tick recharge animations.

## Core data model

Three new components, each composable on `Item`:

```cpp
// Storage. Anything that holds energy.
struct EnergyStore {
    int current = 0;
    int capacity = 0;
};

// Consumer. Anything that spends energy on use.
struct EnergyConsumer {
    int energy_per_use = 1;
};

// Bonuses applied via tinkering enhancement slots.
struct EnergyModifiers {
    int capacity_bonus = 0;        // +X to max
    int charge_rate_bonus = 0;     // +X% to incoming energy per tick
    int discharge_efficiency = 0;  // every N units transferred yields +1 free
};
```

`Item` gains:

- `std::optional<EnergyStore> energy;`
- `std::optional<EnergyConsumer> consumer;`

`RangedData` keeps `max_range` and any future ranged-only fields. Its `charge_capacity`, `current_charge`, and `charge_per_shot` fields are removed — those responsibilities move to `energy` and `consumer`. This is the unifying win: cells, shields, weapons, and future gadgets all expose the same pair of optionals.

A small helper module (`include/astra/energy.h`) provides:

- `int transfer_energy(EnergyStore& src, EnergyStore& dst, int requested)` — drains `src` into `dst`, applies discharge-efficiency, returns units actually moved.
- `bool is_full(const EnergyStore&)`, `bool is_empty(const EnergyStore&)`.
- `int effective_capacity(const Item&)` — base + modifier bonuses.

## Cell tiers

Each tier is its own item def with its own item id. Cells are non-stackable (each instance carries its own charge state). Tinkering enhancement slots scale with tier.

| Tier | Capacity | Tinkering slots | Rarity |
|---|---|---|---|
| Small Energy Cell | 60 | 1 | Common |
| Standard Energy Cell | 150 | 1 | Common |
| Large Energy Cell | 400 | 2 | Uncommon |
| Industrial Energy Cell | 800 | 2 | Rare |
| Antimatter Cell | 2000 | 3 | Epic |

The existing `Energy Cell` item def is retired. The Standard Energy Cell takes its place in vendor stock and loot tables. Vendors are restocked to carry a mix of Small and Standard tiers; higher tiers come from loot only.

## Solar Panel — energy mod

Solar Panel is a **tinkering material** that fills an `EnhancementSlot` on any item that has an `EnergyStore`. It is flagged as an *energy mod* (a sibling of the existing stat mods) and carries this state, stored on the slot:

```cpp
struct SolarPanelData {
    bool active = true;
    int energy_per_tick = 5;     // tier-based
    int tick_interval = 2;       // game-ticks between deposits
    int accumulator = 0;         // internal: ticks accrued since last deposit
};
```

Tier scaling for the panel itself:

| Panel Tier | energy_per_tick | tick_interval |
|---|---|---|
| Common Solar Panel | 5 | 2 |
| Uncommon Solar Panel | 8 | 2 |
| Rare Solar Panel | 12 | 2 |

### Tick handling

A new `EnergySystem` (own header / cpp) is invoked once per `Game::advance_world()` call, receiving the elapsed game-tick count (the same value driving NPC AI, hunger, etc.). Per panel:

1. If the host item is at `current >= effective_capacity`, skip — accumulator does not advance.
2. If the world reports `is_outdoor() == false`, skip — accumulator does not advance.
3. If `active == false`, skip.
4. Otherwise add `elapsed_ticks` to `accumulator`. While `accumulator >= tick_interval`, subtract `tick_interval` and add `energy_per_tick` to the host's `EnergyStore.current` (clamped to capacity, applying any `charge_rate_bonus` from cell modifiers).

**Important:** the system must use *real game-ticks*, not keypress count. Overworld movement advances many more ticks per keypress than detail-zone movement; both must be honored.

### Outdoor query

`World::is_outdoor()` returns true on overworld and detail-zone (planet surface) maps; false on dungeons, ship interiors, station interiors, and any other interior zone. Implementation reuses whatever zone-class flag the existing weather/lighting code already touches; if no such flag exists yet, add a single `bool outdoor` to the zone descriptor.

### Toggle

The inventory interact menu (space on a panel-modded item) gets a **"Toggle Solar Panel"** entry that flips `active`. Toggle is free (no tick cost). State is per panel — if a player has multiple panel-modded items, each can be toggled independently.

### Coverage

Panels tick on items in the player's inventory and on the player's equipped items. Panels on items dropped on the ground or sitting in containers do not tick — they're abandoned. This keeps the per-tick walk small and the rule easy to reason about.

## Tinkering on cells

Tinkering materials can now carry `EnergyModifiers` alongside `StatModifiers`. `EnhancementSlot` is extended from a single `StatModifiers bonus` field into three sibling fields: `StatModifiers stat_bonus` (renamed for clarity), `EnergyModifiers energy_bonus`, and `std::optional<SolarPanelData> solar_panel`. Each material populates whichever it cares about; existing stat-mod materials only set `stat_bonus` and remain unchanged at the call site.

On a cell, applicable bonuses:

- `capacity_bonus` — additive to base capacity.
- `charge_rate_bonus` — multiplies incoming energy per tick (Solar Panel deposits, future recharge stations).
- `discharge_efficiency` — for every N units drained out of this cell, the receiving item gets +1 free.

A small starter set of tinker mats (specific items deferred to implementation phase, since several can reuse existing material defs by tagging them as energy-mod):

- **Capacitor Coil** — `+capacity_bonus`
- **Charge Catalyst** — `+charge_rate_bonus`
- **Polished Conduit** — `+discharge_efficiency`
- **Solar Panel (3 tiers)** — passive recharge

The existing tinkering UI does not need a new tab. Materials are discovered and applied through the same flow; the slot just renders both stat-mod and energy-mod fields when it has them.

## Recharge actions

### Quick recharge — weapon: `r`

1. Find the equipped ranged weapon (`equipment.missile`). If none or no `EnergyStore`, log "No ranged weapon equipped." and return.
2. If already full, log "Already fully charged." and return.
3. Walk cells in inventory sorted by `current` descending (highest-charge cell first, so a single full cell handles the recharge whenever possible). For each cell, call `transfer_energy(cell.energy, weapon.energy, weapon.deficit)`. Stop when the weapon is full or all cells are dry.
4. Log result: `Recharged Plasma Rifle. (+12 charge, 20/20)`.
5. Cost one game-tick (`ActionCost::wait`).

### Quick recharge — shield: `b`

Same as above but targets `equipment.shield`. The current `reload_shield` (dead code) is removed; the new path replaces it.

### Manual cell picker — `Shift-R` (weapon), `Shift-B` (shield)

Opens a modal listing cells in inventory, sorted by current charge descending: `Antimatter Cell - 1840/2000 charge`, `Standard Energy Cell - 80/150 charge`, etc. Player selects one. Game runs a single `transfer_energy(cell, target, deficit)`. One tick.

### Auto-recharge on shoot

When the player fires a ranged weapon and `weapon.energy.current < weapon.consumer.energy_per_use`, the game runs the `r` flow once, then re-checks. If still insufficient (no cells, all empty), log "Weapon empty. No charged cells available." and abort the shot. Existing behavior, just routed through the new code.

### Recharge through inventory

Every item with `EnergyStore` gets a contextual **"Recharge"** entry in the inventory interact menu. Selecting it opens the manual picker (same UI as `Shift-R`/`Shift-B`) and pours the chosen cell into the selected item. This covers:

- Recharging a non-equipped weapon held in inventory.
- Recharging one cell from another (e.g. dump the dregs of a Small Cell into a Standard Cell).
- Recharging a future gadget.

If the selected item has an installed Solar Panel, the menu also shows **"Toggle Solar Panel"**.

## UI

### Bottom bar

Current layout (`game_rendering.cpp:1446`):

```
EFFECTS: ...     │     ...      │     [t]arget [s]hoot [r]eload 12/20
```

New layout when both items are equipped:

```
EFFECTS: ...    │   ...   │  [t]arget [s]hoot [r]echarge 12/20  │  [b] 30/50
```

Rules:

- The weapon section appears only when a ranged weapon is equipped (existing behavior).
- The shield section appears only when an energy shield is equipped (i.e. has `EnergyStore`).
- If only one is equipped, only that section renders, right-aligned with one separator before it (existing behavior, unchanged for weapon; new for shield-only case).
- Use the same separator (`│` U+2502 in dark gray) as the existing right-section divider.
- Numbers color-coded by readiness, same scheme as today (cyan when shootable / above zero, red when below the threshold to fire / shielded hit).
- `[r]eload` rename to `[r]echarge`. Width budget recalculated.
- Shield section uses minimal `[b] N/M` rendering — `b` is the key, the numbers are the shield charge. Implementation phase may add a short label if the layout has room.

### Item labels

- Cells: `Small Energy Cell - <blue>32/60</blue> charge`. The "charge" suffix is plain. The number pair is colored blue (a new UITag, `ChargeNumber`, or reuse the existing cyan readiness color — implementation phase decides).
- Ranged weapons: keep `Plasma Rifle - 1d8`. Energy-per-shot does not appear in the label (per design call). It does appear in look mode and inventory detail.
- Solar Panel installation: not surfaced on the label. Look mode and inventory detail show installed enhancements.

### Look / inventory detail

For an item with energy components, the detail view adds:

- `Charge: 32/60`
- `Energy per shot: 5` (consumers only)
- `Mods: Solar Panel (active, +5 / 2 turns), Capacitor Coil (+20 capacity)` etc.

### Wording sweep

All instances of "Reload" / "reload" → "Recharge" / "recharge". UI labels, log lines, key hints, function names.

- `CombatSystem::reload_weapon` → `recharge_weapon`
- `CombatSystem::reload_shield` → `recharge_shield`

## NPC scope

Out of scope. NPCs continue to use raw damage rolls; nothing on their `Npc` struct changes. The energy system code paths run only against the player's items.

## Save / migration

Save schema version bumps. Old saves are rejected on load with a clear message. New persisted fields:

- `Item.energy` (optional `EnergyStore`)
- `Item.consumer` (optional `EnergyConsumer`)
- `EnhancementSlot.stat_bonus` (renamed from `bonus`; same payload as before)
- `EnhancementSlot.energy_bonus` (`EnergyModifiers`)
- `EnhancementSlot.solar_panel` (optional `SolarPanelData`, including `accumulator`)
- Zone descriptor `outdoor` flag

The Energy Cell item def id remains in code (still parses), but generates a Standard Energy Cell when constructed by name. New cell tier ids slot into the existing item-def registry.

## File layout

New:

- `include/astra/energy.h` — `EnergyStore`, `EnergyConsumer`, `EnergyModifiers`, `SolarPanelData`, helper functions.
- `include/astra/energy_system.h` / `src/energy_system.cpp` — per-tick deposit loop, outdoor query plumbing.

Modified:

- `include/astra/item.h` — drop `RangedData::charge_*`, add `energy` / `consumer` optionals on `Item`. Extend `EnhancementSlot` with `energy_mods` and `solar_panel` fields.
- `src/item_defs.cpp` — replace single Energy Cell with five-tier set; define Solar Panel material (3 tiers); update ranged weapons to set `energy` and `consumer` instead of `RangedData` charge fields.
- `src/game_combat.cpp` — rename + rewrite `reload_weapon` / `reload_shield`; auto-recharge path on shoot.
- `src/game_input.cpp` — bindings for `r`, `Shift-R`, `b`, `Shift-B`; remove obsolete shield reload code if any.
- `include/astra/combat_system.h` — function rename.
- `src/game_rendering.cpp` — bottom-bar shield section, label rename, item-label `charge` suffix.
- `src/game_interaction.cpp` — "Recharge" and "Toggle Solar Panel" interact entries.
- `src/tinkering.cpp` — accept Solar Panel and energy-modifier materials.
- `src/save_file.cpp` — schema bump, reject older versions, serialize new fields.
- `src/game_world.cpp` — `is_outdoor()` query (or wherever zone descriptors live).
- `docs/formulas.md` — energy normalization, panel rates, cell capacities.
- `docs/roadmap.md` — energy system entry checked off.

## Testing strategy

- Unit-style: `transfer_energy` semantics (full transfer, partial, exact, with discharge bonus, into a full target).
- Combat scenario: shoot a weapon empty, ensure auto-recharge picks the lowest-charge cell, drains it, then fires.
- Tick scenario: equip a panel-modded cell, advance one overworld step, assert charge increased; advance one indoor step, assert no change.
- UI: bottom bar shows both `[r]echarge` and `[b]` sections when both equipped; only `[r]echarge` when shield missing; neither when ranged weapon missing.
- Save round-trip: serialize then load a player with a panel-modded cell mid-charge; accumulator state preserved.
- Wording: grep the codebase for stray "reload" / "Reload" — none should remain in player-facing strings.

## Open extension points

Listed for future work, not built here:

- Recharge stations (interactable fixtures that pour energy at a high rate).
- Day/night gating on Solar Panels.
- Gadget content (scanners, injectors, beacons).
- Energy-mod materials beyond the starter set (overload caps, redirector arrays, dampener fields).
- Cooldown-based abilities that draw from a separate "personal energy" pool — currently this spec keeps energy strictly item-scoped.
