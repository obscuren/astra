# Space Station Types — Design Spec

**Date:** 2026-04-13
**Status:** Approved, ready for implementation planning

## Problem

Currently only THA (The Heavens Above, Sol system id=1) is a fully-realized station. Every other station uses a minimal generator with a single merchant and a xytomorph infestation — they offer no meaningful variety, no reason to visit, and no faction or threat distinction. We need station variety so the galaxy feels populated and exploration pays off.

## Goals

- Multiple station archetypes with clear gameplay identity.
- Normal stations feel distinct from each other (not copy-paste) while sharing infrastructure.
- THA remains the single scripted hub; THA-specific NPCs and dialog stay THA-exclusive.
- Remove xytomorph spawns from normal stations; keep them where they belong (THA Maintenance Tunnels, Abandoned, Infested).
- Deterministic from the station seed — same seed, same station, always.

## Non-Goals (out of scope)

- Full quest implementation for keeper hooks (generate hook dialog only; quest payload stubbed).
- New monster types for Infested stations beyond xytomorph.
- Contraband trading mechanics beyond a flavor tag.
- Faction reputation system.
- Pirate station patrols or advanced hostile AI beyond standard hostile behavior.
- Scav-specific salvage loot tables (reuse existing merchant inventory with scrap flavor).

## Station Type Taxonomy

Rolled deterministically from station seed at generation time:

| Type | Share | Friendly | Shops | NPCs | Xytomorphs |
|---|---|---|---|---|---|
| NormalHub | 70% | yes | yes | full roster | no |
| Scav | 10% | yes (wary) | junk dealer only | scav roster | no |
| Pirate | 7% | no (except black-market vendor) | black market | pirate grunts + captain | no |
| Abandoned | 7% | n/a | no | none | 1–2 wanderers |
| Infested | 6% | no | no | none | heavy |

THA (Sol, id=1) is hardcoded `NormalHub`, specialty `Generic`, `is_tha=true`.

### NormalHub Specialties

Second roll on NormalHub stations picks one specialty. Drives additional/weighted rooms and shop emphasis:

- **Mining**: Refinery, extra Storage Bay, ore merchant emphasis.
- **Research**: Lab, Observatory, scientist NPCs, rare tech stock.
- **Frontier**: Armory, Barracks, arms-heavy.
- **Trade**: Market Hall, extra Cantina, more merchants.
- **Industrial**: Engineering, Maintenance access.
- **Generic**: random subset, no emphasis. THA uses this.

## Architecture & Data Model

### New header: `include/astra/station_type.h`

```cpp
enum class StationType {
    NormalHub,
    Scav,
    Pirate,
    Abandoned,
    Infested,
};

enum class StationSpecialty {
    Generic,
    Mining,
    Research,
    Frontier,
    Trade,
    Industrial,
};

struct StationContext {
    bool is_tha;
    StationType type;
    StationSpecialty specialty;
    uint64_t keeper_seed;
    std::string station_name;
};
```

### `Station` struct changes (`star_chart.h/cpp`)

- Remove `bool derelict`.
- Add `StationType type`.
- Add `StationSpecialty specialty` (only meaningful when `type == NormalHub`; `Generic` otherwise).
- Add `uint64_t keeper_seed`.

### Roll

Deterministic hash of the station seed → type bucket (70/10/7/7/6). For `NormalHub`, a second hash → specialty. The roll runs during `Station` construction/generation and is cached with the station. THA bypasses the roll.

### Dispatch (`map_generator.cpp`)

Switch on `StationType`:

- `NormalHub` → `make_hub_station_generator(context)`
- `Scav` → `make_scav_station_generator(context)`
- `Pirate` → `make_pirate_station_generator(context)`
- `Abandoned` → `make_derelict_station_generator(context)` (repurposed from today's derelict)
- `Infested` → `make_infested_station_generator(context)` (new, or derelict + heavy xytomorph pass)

## Generators

### Hub station generator (generalized from today's THA generator)

Signature: `make_hub_station_generator(StationContext)`.

Base roster (always): Docking Bay, Storage Bay, Cantina, Station Keeper's office.

Specialty-weighted additions:
- **Mining**: +Refinery, +extra Storage, Arms rare.
- **Research**: +Lab, +Observatory, Commander rare.
- **Frontier**: +Armory, +Barracks, Observatory rare.
- **Trade**: +Market Hall, +extra Cantina, +more merchants.
- **Industrial**: +Engineering, +Maintenance access.
- **Generic**: random subset of the above.

THA-only rooms (gated on `is_tha`): Observatory-with-Nova, full Command Center, Maintenance Tunnels entrance.

### Scav station generator (new)

4–5 rooms: Docking Bay, Mess Hall (replaces Cantina), Scrap Yard (replaces Armory/Storage), Keeper's nook, bunk room. Salvage-tile decorations. No xytomorphs.

### Pirate station generator (new)

5–6 rooms: Docking Bay (patrolled), Brig, Captain's Quarters (captain + loot), Cantina-turned-den, Black Market back room (neutral vendor), scattered loot stashes.

### Abandoned (repurpose derelict generator)

Existing derelict aesthetic. Add: lootable containers, occasional traps/hazards, 1–2 wandering monsters. No NPCs.

### Infested (new, or extend derelict)

Dark, xytomorph nests in 3–5 rooms, crew-remains loot piles. No NPCs, no shops.

### Xytomorph placement rules

Explicitly strip xytomorph spawns from NormalHub, Scav, and Pirate generators. Xytomorphs remain in:
- THA Maintenance Tunnels (unchanged).
- Abandoned stations (1–2 wanderers).
- Infested stations (heavy).

## NPCs & Dialog

### Dialog split

NPCs with current THA-specific lines (Station Keeper, Commander, Astronomer, Arms Dealer, Engineer, civilian/drifter flavor) split into:

- **Generic pool**: role-appropriate, station-agnostic. Talks about this station's name, specialty, local traffic, a generic rumor.
- **THA pool**: existing lore — three centuries, the Collapse, Nova references, the cargo-hauler quest.

Each NPC builder takes a `StationContext` and selects its dialog branch on `is_tha`.

### Unique station keepers

`keeper_seed` drives:
- Name (first + last from rolled tables).
- One of ~6 personality archetypes: gruff veteran, chatty bureaucrat, nervous newcomer, retired spacer, corporate stiff, eccentric loner. Each archetype has its own dialog tone template.
- A small flavor quest hook tied to station specialty (Mining: missing ore shipment; Research: recover a data core; Frontier: patrol didn't return; Trade: overdue convoy; Industrial: malfunctioning relay). Hook dialog only — quest payload stubbed.

THA keeper is the existing hand-written keeper, selected via the `is_tha` branch.

### Scav NPCs

- **Scav keeper**: separate archetype pool (weathered, pragmatic traders).
- **Scav merchant** (junk dealer): reuses existing merchant inventory, flagged "used/scrap" — cheaper prices, buys anything.
- Optional scav medic and 1–2 civilians.

### Pirate NPCs

- **Pirate captain** (new builder): hostile, higher HP, drops key loot.
- **Black-market vendor** (new builder): neutral, premium prices, wider inventory including a simple `contraband` tag on items.
- **Pirate grunts**: hostile, reuse existing hostile-human stats.

### Spawner changes (`npc_spawner.cpp`)

Dispatch on `StationType`. Current `spawn_hub_npcs` generalizes to `spawn_normal_hub_npcs(context)`; add `spawn_scav_npcs(context)`, `spawn_pirate_npcs(context)`. Abandoned and Infested use monster-only spawn paths.

## Migration

Existing saves have `bool derelict` on `Station`. On load:
- `derelict == true` → `StationType::Abandoned`.
- `derelict == false` → re-roll `StationType` from the station seed (deterministic; a fresh install and an existing save land on the same type for any given station).

The `derelict` field is then removed. Bump star chart save-format version if required.

## Testing

Terminal-only, dev mode.

- Dev overlay / command that prints each station's rolled type on the star chart, so we can eyeball distribution.
- Unit-level: seed a fixed galaxy, assert ~70/10/7/7/6 split across N stations within tolerance.
- Manual: visit one station of each type; verify roster, NPC set, xytomorph presence/absence, and unique-keeper naming across two NormalHub stations.

## Files Touched

- `include/astra/star_chart.h`, `src/star_chart.cpp` — types, `Station` fields, roll.
- `include/astra/station_type.h` (new) — enums + `StationContext`.
- `src/map_generator.cpp` — dispatch on `StationType`.
- `src/generators/hub_station_generator.{h,cpp}` — generalize; specialty rosters.
- `src/generators/scav_station_generator.{h,cpp}` (new).
- `src/generators/pirate_station_generator.{h,cpp}` (new).
- `src/generators/infested_station_generator.{h,cpp}` (new, or extend derelict).
- `src/generators/derelict_station_generator.cpp` — Abandoned tweaks.
- `src/npc_spawner.cpp` — dispatch on type + context.
- `src/npcs/station_keeper.cpp` — generic path + archetype rolls.
- `src/npcs/commander.cpp`, `astronomer.cpp`, `arms_dealer.cpp`, `engineer.cpp` — dialog split on `is_tha`.
- `src/npcs/scav_keeper.cpp`, `scav_merchant.cpp`, `pirate_captain.cpp`, `pirate_grunt.cpp`, `black_market_vendor.cpp` (new).
- `docs/formulas.md`, `docs/roadmap.md` — update.
