# Game Mechanics

Reference for the rules, formulas, and systems that drive Astra. Item-specific stats (per-weapon dice, per-cell capacity, per-mod effects) live in their own catalog: [`docs/items.md`](items.md).

## Derived Stats

- **DV (Dodge Value)**: `dodge_value + (AGI - 10) / 2 + equipment.dv + effects.dv`
- **AV (Armor Value)**: `sum(equipped_armor.av) + sum(armor.type_affinity[damage_type]) + effects.av`
- **Effective Max HP**: `max_hp + (TOU - 10) * 2 + equipment.max_hp + effects.max_hp`

Base values: dodge_value=3, max_hp=10

## Combat

### Attack Roll

All attacks use a d20 roll against the target's DV:

```
attack_roll = 1d20 + attacker_modifier + weapon_skill_bonus
hit = attack_roll >= target_DV
```

- **Natural 20**: always hits
- **Natural 1**: always misses
- **Player modifier**: `(AGI - 10) / 2`
- **NPC modifier**: `level / 2`
- **Weapon skill bonus**: +2 if trained in the matching weapon class

| Weapon Class | Skill Required |
|---|---|
| ShortBlade | ShortBladeExpertise |
| LongBlade | LongBladeExpertise |
| Pistol | SteadyHand |
| Rifle | Marksman |

### Shield Layer

If the target has shield HP > 0, damage is absorbed by the shield before armor:

```
raw_damage = damage_dice.roll()  (penetrate as if AV = 0)
shield_cost = shield_absorb(raw_damage, damage_type, shield_affinity)
shield_hp -= shield_cost
```

**Shield absorb with affinity:**
```
if affinity_bonus > 0:
    shield_cost = max(1, raw_damage * 100 / (100 + affinity_bonus))
else:
    shield_cost = raw_damage
```

Excess damage when shield HP reaches 0 is discarded (shield absorbs the killing blow). No auto-recharge — player manually recharges from Battery items (+5 shield HP per cell).

### Penetration Roll

If no shield (or shield depleted), roll penetration against armor:

```
penetration_value = 1d10 + (STR - 10) / 2    (player)
penetration_value = 1d10 + level / 3          (NPC)
effective_AV = target.AV + target.type_affinity[damage_type]
```

- **Natural 10**: always penetrates (exactly 1 penetration)
- **Natural 1**: always fails (0 damage)
- **PV > AV**: 1 penetration → roll weapon damage dice once
- **Each additional +4 over AV**: additional penetration → roll damage dice again
- Total damage = sum of all penetration damage rolls

### Critical Hits (Player only)

- **Crit chance**: `clamp((LUC - 8) * 2 + 3, 0, 30)%`
- **Crit effect**: auto-penetrate (skip penetration roll) + roll damage dice twice and sum
- Makes LUC a direct counter to high-AV targets

### Damage Types

Five damage types:

| Type | Thematic Source | Resistance Stat |
|---|---|---|
| Kinetic | Blades, batons, projectiles | kinetic |
| Plasma | Superheated energy weapons | heat |
| Electrical | Ion/arc weapons | electrical |
| Cryo | Cold-based weapons | cold |
| Acid | Corrosive attacks | acid |

### Resistance

Applied as percentage reduction after penetration damage:

```
final_damage = damage - (damage * resistance_pct / 100)
```

### Damage Effects Pipeline

After resistance, for each active effect on the target:
```
damage = damage * effect.damage_multiplier / 100
damage += effect.damage_flat_mod
damage = max(damage, 0)
```
- Invulnerable: `damage_multiplier = 0` (immune to all damage)

### Weapon damage

Unarmed: 1d3 Kinetic. Per-weapon dice are listed in [`docs/items.md`](items.md) (Ranged Weapons, Melee Weapons). The dice shown there are the **base** values; `scale_item_to_rarity` multiplies the dice modifier by the rarity tier (×1.00 / ×1.10 / ×1.25 / ×1.45 / ×1.75 for Common→Legendary), and `scale_item_to_level` adds the level multiplier on top.

### NPC Stat Scaling

- **DV**: `base_dv + (level - 1)`
- **AV**: `base_av + (level - 1) / 2`
- **HP**: `base_hp * level`
- **Elite bonus**: HP x2, quickness x1.5, DV +2, AV +1
- **Damage dice**: fixed (not scaled by level), elite NPCs get bonus dice

### NPC Ranged Attacks

Preconditions: attacker has `ai == NpcAi::Turret`, chebyshev distance in `[2, attack_range]`, non-empty `ranged_damage_dice`, and Bresenham LOS (no opaque tile between attacker and target).

- Attack roll: `1d20 + level/2` vs target DV (player: `effective_dv()`; NPC: `dv`). Natural 1 always misses; natural 20 always hits.
- Penetration: `1d10 + level/3` vs effective AV (+ `type_affinity` for NPC target). Natural 1 = no damage; natural 10 = always penetrates. Each 4 points of excess triggers another damage roll.
- Shield (player only): penetration rolled against AV=0; damage scaled by `shield_affinity`; never bypasses shield.
- Damage: `ranged_damage_dice` of `ranged_damage_type`, then `apply_resistance` (player) and `apply_damage_effects` (player or NPC).

Turret AI holds position when out of range or LOS is blocked; it never pursues.

### Loot Drops

- 50% chance on enemy kill
- Item level = npc.level
- Rarity: Common 50%, Uncommon 30%, Rare 15%, Epic 4%, Legendary 1%
- Drop table: 30% weapon, 25% armor, 20% consumable, 15% junk, 10% crafting

### Combat Reputation

- **Kill faction NPC**: -30 reputation with victim's faction (clamped to -600)
- From Neutral (0), takes 10 kills to reach Hated (≤ -300)
- Killing unaligned, Feral, Xytomorph, Archon, or Void Reaver NPCs: no reputation change (for now)

### Ability Cooldowns

| Ability | Cooldown | Action Cost | Weapon | Effect |
|---------|----------|-------------|--------|--------|
| Jab | 3 ticks | 25 | ShortBlade | 50% weapon dice damage |
| Cleave | 5 ticks | 50 | LongBlade | Full weapon dice to all adjacent hostiles |
| Quickdraw | 3 ticks | 25 | Pistol | Full missile weapon dice to current target |
| Intimidate | 10 ticks | 50 | Any | Target flees for 3 + (WIL-10)/2 ticks (min 2) |

### NPC XP & Credits

- **XP Reward**: `base_xp * level * (elite ? 3 : 1)`
- **Kill Credits**: `level * 2 + (elite ? 5 : 0)`

## Economy

### Trade Prices

```
total_mod  = effect_pct + faction_pct
buy_cost   = buy_value + (buy_value * total_mod / 100)
sell_price  = sell_value + (sell_value * (-faction_pct + effect_sell_pct) / 100)
```
- Haggle effect: `buy_price_pct = -10`, `sell_price_pct = +10`
- Faction reputation modifier (`faction_pct`):
  - Hated (rep <= -300): +30%
  - Disliked (rep -299 to -60): +15%
  - Neutral (rep -59 to 59): 0%
  - Liked (rep 60 to 299): -10%
  - Trusted (rep >= 300): -20%

### Ship Stats

```
hull_hp    = sum of installed component ship_modifiers.hull_hp
shield_hp  = sum of installed component ship_modifiers.shield_hp
warp_range = sum of installed component ship_modifiers.warp_range
operational = engine slot is occupied
```

### Repair Bench Cost

```
cost = max(1, missing_durability * 2)
```

## Character Creation

### Attribute Point Buy

- Budget: 10 points
- Max per attribute: +8
- Final = `class_base + race_modifier + player_allocation`

## NPC Energy & Turns

- **NPC Energy Gain**: `action_cost * npc_quickness / 100` per player action
- **NPC Turn Threshold**: `energy >= 100`

## Health Regeneration

Accumulator-based passive regen. A `regen_counter` increments each world tick while the player is alive and damaged. When the counter reaches the interval threshold, the player heals 1 HP and the counter resets.

| Hunger State | Ticks per 1 HP | Effect |
|---|---|---|
| Satiated | 15 | Well-fed bonus |
| Normal | 20 | Base rate |
| Hungry | 40 | Half speed |
| Starving | 0 (no regen) | No natural recovery |

At base rate (Normal), full recovery from 1 HP to 10 HP takes 180 ticks.

## Action Costs

| Action | Cost |
|---|---|
| Move | 50 |
| Attack | 100 |
| Shoot | 100 |
| Interact | 50 |
| Wait | 50 |

## Time of Day

### Global Calendar

- **Day number**: `(world_tick / 200) + 1`
- **Cycle number**: `((day - 1) / 30) + 1`
- **Day in cycle**: `((day - 1) % 30) + 1`

Constants: `ticks_per_global_day = 200`, `days_per_cycle = 30`

### Day Phases

Phases are defined as percentages of a body's local `day_length`:

| Phase | Start | End | Duration | Effect |
|---|---|---|---|---|
| Night | 75% | 10% | 35% | View = `light_radius` |
| Dawn | 10% | 20% | 10% | View lerps `light_radius` → `max_radius` |
| Day | 20% | 60% | 40% | View = `max_radius` (full map) |
| Dusk | 60% | 75% | 15% | View lerps `max_radius` → `light_radius` |

Light/dark split: **65% / 35%**

### View Radius by Context

| Location | View Radius |
|---|---|
| Day (surface) | `max(map_width, map_height)` |
| Night (surface) | `light_radius` (default 6) |
| Dawn/Dusk (surface) | Lerp between night and day based on phase progress |
| Dungeon | Always `light_radius` (no sunlight underground) |
| Space station | Always `view_radius` (fully lit) |
| Ship | Full reveal |
| Overworld | `view_radius` (no time effect) |

### Celestial Body Day Length

Derived from body type and size:

| Body Type | Base Ticks |
|---|---|
| Rocky | 150 |
| Gas Giant | 280 |
| Ice Giant | 260 |
| Terrestrial | 200 |
| Dwarf Planet | 180 |
| Asteroid Belt | 160 |
| Moon (any) | 400 (tidally locked) |
| Station / Ship | 200 (standard) |

Final: `base + (size * 8)`, clamped to 100–400


## Getting Lost

When moving on the overworld, each step has a chance to get lost.

**Get Lost Chance:** `base 15%` per overworld move
- TODO: terrain modifiers (forest/swamp higher, plains lower)
- TODO: wayfaring skill reduces chance

**Regain Bearings:** checked each move while lost on detail map
- No chance for first 30 moves (grace period)
- Formula: `min((lost_moves - 30) / 3, 25)`
- After grace: ramps 1% every 3 moves, caps at 25%
- Expected ~60-100 total moves to regain bearings
- TODO: wayfaring skill reduces grace period and increases ramp

| Moves | Chance |
|-------|--------|
| 0-29 | 0% |
| 30 | 0% |
| 45 | 5% |
| 60 | 10% |
| 75 | 15% |
| 105+ | 25% (cap) |

When lost:
- Player enters detail map at random zone in the 3x3 grid
- `<` key blocked until bearings regained
- Dev mode never gets lost

## Light Sources

Fixtures with `light_radius > 0` extend the player's FOV toward them.
When a light source is visible, the player can see tiles that are:
1. Within the player's line-of-sight (shadowcast from player position)
2. Within `light_radius` distance of the light source

Effective extended range = distance_to_light + light_radius.
Walls still block line of sight — no seeing around corners.

| Fixture  | light_radius |
|----------|-------------|
| Torch    | 4           |
| Console  | 2           |
| Viewport | 1           |

## Wayfinding Skills

**Category unlock** (Cat_Wayfinding): -2% flat lost chance (15% → 13%)

**Compass Sense**: Grace period 30→15 moves, ramp 1/3→1/2 per move, cap 25%→40%

**Terrain Lore** (per terrain type):
- Lost chance halved for matching terrain
- Overworld travel ticks halved (15→7) for matching terrain
- Cross-tile zone transitions halved (15→7) for matching terrain

| Skill | Terrains |
|-------|----------|
| Lore: Plains | Plains, Desert |
| Lore: Forest | Forest, Fungal |
| Lore: Wetlands | Swamp, River, Lake |
| Lore: Mountains | Mountains, Crater |
| Lore: Tundra | Ice Field, Lava Flow |

## Overworld Generation

### Architecture

Overworld generators inherit from `OverworldGeneratorBase` which provides a template-method pipeline. Subclasses override virtual hooks to customize terrain for specific body types. `DefaultOverworldGenerator` handles all body types without a dedicated generator.

**Pipeline (non-virtual, calls hooks in order):**
1. Build `TerrainContext` from body properties
2. `configure_noise()` — set elevation/moisture noise scales
3. Generate dual fBm noise fields (elevation + moisture)
4. `pre_classify()` — setup before classification (e.g., derived layers)
5. `classify_terrain()` per cell — assign tile type
6. `apply_lore_overlays()` — scar/alien terrain from lore influence map
7. `carve_rivers()` — body-specific river generation
8. `place_landing_pad()` — spiral search from center for passable tile
9. `ensure_connectivity()` — flood-fill + mountain pass carving

**Shared `place_features()` pipeline:**
1. Place lore landmarks (beacons, megastructures)
2. `place_pois()` — body-specific POI placement

### Default Generator (all non-temperate bodies)

Three classifiers dispatched by body type:
- **Terrestrial + atmosphere**: elevation/moisture thresholds modified by temperature and atmosphere
- **Rocky / airless**: elevation only → Mountains, Crater, Desert, Plains
- **Asteroid belt**: elevation only → Mountains, Crater, Plains

Rivers: 2-4, 40-step steepest descent from mountain-adjacent sources. Only on terrestrial with atmosphere, not frozen/scorching.

### Temperate Planet Generator (Terrestrial + Temperate + Standard/Dense)

**Layered simulation approach** — biomes emerge from physical conditions rather than direct threshold assignment.

**Layer 1: Elevation** — fBm noise at scale 0.06 (low frequency for large mountain ranges and valleys).

**Layer 2: Moisture** — fBm noise at scale 0.08 (low frequency for large wet/dry zones).

**Layer 3: Temperature** — derived from latitude + elevation:
```
base_temp    = y / (h - 1)                              // 0=cold (north), 1=hot (south)
elev_cooling = max(0, (elevation - 0.4) * 1.5)          // high ground is colder
noise_var    = (fbm(x, y, seed, 0.05, 3) - 0.5) * 0.15 // natural irregularity
temperature  = clamp(base_temp - elev_cooling + noise_var, 0, 1)
```

**Biome classification** from elevation + temperature + moisture:

| Condition | Tile |
|-----------|------|
| elevation > 0.75 | Mountains |
| elevation < 0.2 | Lake |
| temp < 0.2 | Ice Field |
| temp < 0.3, elev > 0.6 | Ice Field |
| temp < 0.3 | Plains (tundra) |
| temp > 0.8, moist > 0.6 | Swamp (tropical) |
| temp > 0.8, moist > 0.4 | Plains (savanna) |
| temp > 0.8 | Desert |
| temp > 0.65, moist > 0.55 | Forest (warm) |
| temp > 0.65, moist > 0.35 | Plains |
| temp > 0.65 | Desert |
| elev < 0.32, moist > 0.5 | Swamp (temperate) |
| moist > 0.55 | Forest |
| moist > 0.3 | Plains |
| else | Desert |

**Rivers:** 3-8, steepest descent from mountain-adjacent sources, up to 120 steps. Form lakes (3-8 tiles) when reaching basins with no downhill path.

**Result:** Ice caps at poles/peaks, forests in wet temperate band, desert in hot+dry south, swamp in low+wet areas, natural latitude-based transitions.

## Galaxy Simulation (Lore Generation)

### Civilization Traits

Each civilization allocates **100 points** across 9 traits:

| Trait | Drives |
|-------|--------|
| Aggression | War frequency, conquest, military growth rate |
| Curiosity | Research rate, exploration, ruin investigation |
| Industriousness | Resource extraction, construction, expansion speed |
| Cohesion | Stability baseline, schism resistance |
| Spirituality | Sgr A* awareness growth, sacred sites, transcendence |
| Adaptability | Terraforming success, plague resistance, crisis recovery |
| Diplomacy | Trade likelihood, alliance formation, peaceful outcomes |
| Creativity | Artifact creation, cultural renaissance, breakthroughs |
| Technology | Tech advancement rate, weapon breakthroughs, megastructures |

### Trait-Derived Multipliers

- **Pop growth**: `0.7 + industriousness * 0.03 + adaptability * 0.02`
- **Consumption**: `0.8 + aggression * 0.02 + industriousness * 0.01`
- **Research**: `0.5 + curiosity * 0.05 + technology * 0.05 + creativity * 0.03`
- **Military growth**: `aggression * 0.04 + technology * 0.02`
- **Stability drift**: `(cohesion - 10) * 0.01 + diplomacy * 0.005 - aggression * 0.005`
- **Sgr A* mult**: `0.5 + spirituality * 0.05 + curiosity * 0.02`

### Philosophy Derivation

Philosophy label assigned from highest combined traits:
- **Expansionist**: aggression + industriousness
- **Contemplative**: curiosity + creativity
- **Predatory**: aggression * 2
- **Symbiotic**: diplomacy + cohesion + adaptability
- **Transcendent**: spirituality + curiosity

### Per-Tick Economy (1 tick = 1 million years)

- **Resource need**: `sqrt(population) * 0.3 * consumption_mult`
- **Resource income per system**: `richness * (2.0 + knowledge * 0.005)`
- **Net resources**: `income - need` (clamped 0-5000)
- **Carrying capacity**: `territory_count * 120`

### Population Growth

```
growth_rate = pop_growth_mult * 0.2
resource_factor = clamp(resources / (pop_need * 3 + 1), 0, 2)
stability_factor = stability / 100
growth = growth_rate * resource_factor * stability_factor
if population > capacity * 0.8: growth *= 0.3 (soft cap)
if resources <= 0: growth = -0.1 * (1 + population * 0.001) (starvation)
```

### Knowledge & Military Growth

- **Knowledge**: `+0.05 * research_mult * (0.5 + stability/200)`
- **Military**: `+military_growth * 0.05`
- **Sgr A* awareness**: `+0.003 * sgra_mult * (knowledge / 200)`

### Stability

```
target = 55 + stability_drift * 50
stability += (target - stability) * 0.02  (drift toward target)
if resources <= 0: stability -= 0.5       (famine)
if territory > stability * 0.5: stability -= 0.2  (overextension)
if knowledge > 500 && stability < 40: stability -= 0.2  (existential crisis)
if faction_count > 1: stability -= 0.1 * faction_count
if stability < 30: stability += adaptability * 0.02  (recovery)
```

### Faction Tension

```
if stability < 40: tension += (40 - stability) * 0.02
else: tension -= 0.1 (decay)
if tension > 80: faction_count++, tension = 0, stability -= 15
```

### Weapon Technology

- Breakthrough at: `knowledge > (weapon_tech + 1) * 150`
- Effect: `military += 50 * weapon_tech` (decisive advantage)

### Inter-Civ Interaction Conditions

- **Adjacent**: territories within 30 galaxy units
- **Aggressive**: `aggression > 15`
- **Peaceful**: `diplomacy > 12 && aggression < 10`
- **Conquest**: aggressive + `military > target * 1.5`
- **Trade**: both peaceful, `1/200` per interaction check
- **Border clash**: both aggressive, `~3%` per check
- **Transcendence**: `spirituality > 15 || 1/50 chance` when sgra > 85

### Collapse

Civilization dies when `population < 3`. Territory released, systems marked with ruin layers.

## Station Type Roll

Deterministic per station_seed (station_seed = system.id):

```
StationType:
  r = splitmix(seed ^ 0xA1) mod 100
  r < 70 → NormalHub
  r < 80 → Scav
  r < 87 → Pirate
  r < 94 → Abandoned
  else     Infested

StationSpecialty (NormalHub only):
  r = splitmix(seed ^ 0xB2) mod 6
  → Generic / Mining / Research / Frontier / Trade / Industrial

keeper_seed = splitmix(station_seed ^ 0xC3)
```

THA (Sol, id=1) is hardcoded NormalHub + Generic with is_tha=true.

## Dungeon Generation

### Dungeon layer sub-seeding

Each layer of the dungeon generator pipeline derives its own
`std::mt19937` from the level seed:

    backdrop     = seed ^ 0xBDBDBDBD
    layout       = seed ^ 0x1A1A1A1A
    connectivity = seed ^ 0xC0FFEE00
    overlays     = seed ^ 0x0FEB0FEB
    decoration   = seed ^ 0xDEC02011
    fixtures     = seed ^ 0xF12F12F1

This prevents adding or removing an overlay from reshuffling
decoration or fixture placement.

## Cozy (Campfire aura)

Applied while the player is within Chebyshev distance ≤ 6 of one of
their own `FixtureType::Campfire` fixtures. Re-applied each world tick
with `duration = 1`, so it disappears automatically the tick after the
player leaves the radius.

Effect:

```
regen_interval_cozy = max(1, regen_interval(hunger) / 2)
```

Natural HP regeneration ticks roughly twice as fast while Cozy is
active. No effect on hunger, other effects, or any other regen source.

## Camp Making

- Cooldown: 300 world ticks (`EffectId::CooldownCampMaking`)
- Action cost: 100
- Campfire lifetime: 150 world ticks from the `world_tick` at which it
  was placed; the fixture is removed cleanly on expiry.
- Placement: 8-neighbour scan in fixed order (NW, N, NE, W, E, SW, S,
  SE); first passable tile with no fixture wins. Fails with no cooldown
  if no space is available.

## Aura System

Fixtures, the player, and NPCs can all emit gameplay effects to entities
in range each world tick. Auras are pure data:

```
Aura {
    template_effect   // Effect copied onto each receiver on apply
    radius            // Chebyshev tiles
    target_mask       // Player | FriendlyNpc | HostileNpc (bitflags)
    source / source_id
}
```

Emission runs inside `Game::advance_world` after effect tick/expire
and before passive regen. Each tick, every emitter's auras are applied
to every in-range receiver via `add_effect`, which replaces any existing
effect of the same `EffectId` — so repeated emission acts as a refresh
and multiple overlapping emitters collapse to the most recently written.

Default aura duration is 1 world tick (baked into the `template_effect`)
so an aura drops the tick after a receiver steps out of range. Longer
lifetimes are expressed as larger `duration` values on the template
(e.g. `duration = 5` gives a 5-tick grace period).

Fixture auras come from two registries:

- **Tag-derived** — any `FixtureData` carrying a specific `FixtureTag`
  contributes an aura (e.g. every `CookingSource` fixture could grant
  `CookingFireAura`).
- **Type-specific** — a specific `FixtureType` contributes an aura
  regardless of tags (e.g. `FixtureType::Campfire → Cozy`, deliberately
  not `HeatSource`-wide).

Player and NPC auras live on `Entity::auras` and are rebuilt from sources
(equipment, active effects, learned skills) by
`rebuild_auras_from_sources`. Manual entries (dev-console) persist
untouched across rebuilds.

Only `Manual`-sourced entity auras are serialised; everything else
reconstructs from items/effects/skills after save-load.

Cozy (campfire aura) is the first tenant: `FixtureType::Campfire`
emits Cozy to the player within 6 tiles, halving the natural regen
interval. See
`docs/superpowers/specs/2026-04-23-aura-system-design.md` for the full
design rationale and
`docs/superpowers/plans/2026-04-23-aura-system.md` for the phased
rollout.

## Cooking

Cooking resolves in the character screen's Cooking tab when the player
is within a `CookingSource` fixture's aura (`CookingFireAura`, Chebyshev
radius 2). Three pot slots accept ingredient type + quantity; the slot
bag is matched order-independently against `recipe_catalog()`.

- **Match**: exact set of `(item_def_id, qty)` pairs (UI prevents
  duplicate def_ids across slots). Known recipes consume ingredients
  and produce the dish. Unknown recipes additionally teach the recipe.
  No match produces one Burnt Slop.
- **Consumption** (reading `Item::dish`):
  - `hunger_shift` added to the `HungerState` ordinal (Satiated=0,
    Normal=1, Hungry=2, Starving=3) and clamped to `[0, 3]`. Negative
    values heal hunger.
  - `hp_restore` added to current HP, clamped to
    `player.effective_max_hp()`.
  - `granted` EffectIds are applied via `add_effect`, materialised by
    `effect_for_id`.
- **Advanced Fire Making skill**: reduces `camp_making_cooldown_ticks`
  by 40% for the player (`effective_cooldown = ticks * 60 / 100`).

See `docs/superpowers/specs/2026-04-23-cooking-system-design.md` and
`docs/superpowers/plans/2026-04-23-cooking-system.md`.

### Acrobatics
- Cat_Acrobatics: +1 DV (always-on aura via make_acrobatics_ge)
- Swiftness: +5 DV applied only at ranged attack resolution against the player
- Sidestep: +2 DV applied at melee resolution when any hostile NPC sits at Chebyshev distance == 1
- Sure-Footed: dungeon move cost = floor(ActionCost::move * 9 / 10) = 45 (vs 50 baseline)
- Tumble: dash up to range 3 in one of 8 directions via Line telegraph; ignores enemies in path; landing tile must be passable and unoccupied; cooldown 25 ticks; action cost 50
- Adrenaline Rush: +2 DV, +25% quickness for 3 ticks; cooldown 40 ticks; self-cast; action cost 25

### Telegraph System
Reusable targeting/preview system for abilities. Shapes supported: Line (implemented). Declared but pending implementation: Ray, Cone, Burst, Adjacent. Consumed by abilities that set `Ability::telegraph = TelegraphSpec{...}` — `use_ability` routes through `Game::telegraph()` and invokes `execute_telegraphed(game, result)` on Enter confirm. Cancel (Esc) incurs no cooldown/turn cost.

## Energy System

Player-side energy is stored on items via `EnergyStore { current, capacity }`. Consumers spend `EnergyConsumer { energy_per_use }` per action.

> Per-item capacities, costs, and tier breakdowns live in [`docs/items.md`](items.md) (Energy Cells, Energy Mods, Solar Panels, Ranged Weapons).

### Recharge actions

- `r` — recharge equipped weapon (drains inventory cells highest-charge-first into `equipment.missile->energy`).
- `b` — recharge equipped shield (same flow, `shield_energy()` target).
- `Shift-R` / `Shift-B` — manual cell picker for weapon / shield. Pick which cell to drain.
- Inventory interact (`space` on an item with `EnergyStore`) — open the picker for that item, including cell-from-cell transfers.
- Cost: one `ActionCost::wait` tick. Auto-recharge during `s` (shoot) is free (the shot itself already costs one tick).

### Solar panels

Solar Panels deposit energy into their host item only when the player is outdoors (overworld or detail map). Indoor zones (dungeons, ship interiors, station interiors) do not advance the panel's accumulator. `charge_rate_bonus` from a host's committed enhancements adds an integer-percent bonus to each deposit (`deposit + deposit * pct / 100`).

### Discharge efficiency

`EnergyModifiers::discharge_efficiency` from any committed slot on the source cell adds +1 free unit to the destination for every N units actually drained (`bonus = drained / N`, `0` disables).

### Tinkering

Crafting system covering repair, enhancement, refinement, synthesis, and schematic-based consumable crafting. **24 materials across 3 tiers** with tier-weighted salvage. See [tinkering.md](tinkering.md) for the full materials catalog, refinement recipes, synthesis recipes, schematic recipes, and salvage rules.

### Tinker mod stacking

Multiple mods on the same cell stack: the system sums `capacity_bonus`, `charge_rate_bonus`, and `discharge_efficiency` across all committed slots.

### Cell procs (Legendary specialty cells)

Some cells fire a one-off effect once per `threshold` units actually drained from them. Each cell carries a `CellProc { kind, magnitude, duration, threshold, accumulator }`. The accumulator is per-instance and persists across drains. See [`docs/items.md`](items.md#legendary-specialty-cells-procs) for the catalog.

### Toggleable powered items

Items with `toggleable = true` (currently: Nightvision Goggles) carry their own `EnergyStore` and can be switched on or off. When active and in a *dark context* (dungeon, or surface detail map at Night/Dawn/Dusk), they drain **1 charge per 10 world-ticks** via a per-tick accumulator. When charge reaches zero, the item falls back to its passive stat only (Nightvision Goggles: view +1 passive, view +5 while powered+active).

**Dark context rules** (mirrors FOV restriction logic):
- Overworld, space station, ship interior, derelict station → lit (not dark).
- Surface detail map at Day → not dark.
- Surface detail map at Night, Dawn, or Dusk → dark.
- Dungeon (any non-overworld, non-detail, non-station map) → always dark.

**Auto-mode:** If a committed `AiModule` or `LightSensor` enhancement slot is present, the item auto-toggles based on context and remaining charge. Without a module the player toggles manually (inventory `g` key).

**Accessory module synthesis recipes:** `Optic Module + Joint Mechanism → AI Module` (nano:1 power:1 circuit:2); `Optic Module + Padding Weave → Light Sensor` (nano:1 circuit:1). Both use `custom_builder` — equipment fields in the recipe are unused.

## Traps

Generalized framework that powers the 5 deployable mine items and pre-placed dungeon traps. Live `Trap` registry on `WorldManager` (per-active-map, persisted in `MapState.traps`); telegraph-driven deploy; live faction-eval triggers.

> Per-mine effects, costs, throw range and stack behavior live in [`docs/items.md` § Mines](items.md#mines). Trap framework source: [`src/trap.cpp`](../src/trap.cpp), [`include/astra/trap.h`](../include/astra/trap.h).

### Trigger modes

Each `Trap` carries a `TrapTrigger`:

- `NonFriendlyToOwner` — default for player-deployed and NPC-deployed mines. Fires only when the stepping entity is hostile to the placer (live faction lookup via `is_hostile` / `is_hostile_to_player`).
- `AnyEntity` — default for `place_dungeon_trap`. Fires on any stepper, including the player.
- `PlayerOnly` — fires only when the stepper is the player.

### Owner immunity

The placer is **splash-immune** from their own traps. `apply_damage_and_status` early-outs if `placer_is_player` and the affected entity is the player, or if the placer NPC id matches the affected NPC. This means a player can throw a Proximity Mine, then walk through its burst radius safely while it detonates on a passing hostile.

### Detection roll

Hidden traps roll a 1d20 detection check **once** when the player enters a trap's `reveal_radius` (Chebyshev). The roll is `1d20 + Player.trap_detection ≥ detection_dc`. A success unhides the trap and logs a directional sighting (e.g. "You spot a proximity mine to the northeast!"). The check is debounced via `Trap.was_in_player_radius` — leaving and re-entering the radius re-rolls. Player-placed traps and traps with `hidden = false` skip the roll.

### Per-kind defaults

| Kind | Damage | Burst | Status (duration / tick) | Hidden | Detection DC |
|---|---|---|---|---|---|
| Proximity Mine | 12 | 3×3 (r=1) | — | yes | 12 |
| EMP Mine | 4 | 3×3 (r=1) | EMP-Disabled (5) | yes | 13 |
| Incendiary Mine | 8 | 3×3 (r=1) | Burn (4 / 2 dmg) | yes | 11 |
| Decoy Mine | — | — | — (emits noise) | no (visible) | n/a |
| Caltrops | 3 | single tile (3 activations) | Slow (3) | no (visible) | n/a |
| Dungeon generic | 6 | single tile | — | yes | 14 (overridable) |

`EMP-Disabled` blocks energy-weapon firing, freezes ability cooldown advance, and suppresses aura emission for the duration.

### Decoy noise event

When a Decoy Mine triggers it logs "The decoy beeps loudly!" and pushes a `NoiseEvent { radius = 5, ttl_ticks = 5 }` onto the active map's `noise_events` queue. NPCs in the `Idle` or `Wandering` AI state, hostile to the emitter (player or owner faction), retarget toward the noise tile for the event's TTL. Friendly NPCs and the player ignore noise events. `tick_noise_events` decrements TTL once per world tick and erases expired entries.

### Lifecycle hooks

- `on_entity_enters_tile(game, x, y, is_player, npc_id)` — called from player movement and NPC movement. Iterates traps at `(x, y)`, runs `should_trigger`, calls `resolve_trap` on hit, decrements `activations_remaining`, erases when zero.
- `update_trap_detection(game)` — called after every player position change. Runs the d20 detection check.

### Pre-placed dungeon traps

Generators call `place_dungeon_trap(world, x, y, kind, trigger, hidden, detection_dc)` to seed a trap. Defaults: `trigger = AnyEntity`, `hidden = true`, `detection_dc = 14`. Owner is treated as the dungeon (no placer faction); detection roll applies normally.

## Ground effects

Per-tile transient effects laid down on the active map. One entry per affected tile; each entry carries `kind`, `(x, y)`, and remaining `ttl`. Live registry on `WorldManager.ground_effects_`, persisted in `MapState.ground_effects` (v51). Decremented every world tick by `tick_ground_effects`; entries with `ttl ≤ 0` are erased.

> Source: [`src/ground_effect.cpp`](../src/ground_effect.cpp), [`include/astra/ground_effect.h`](../include/astra/ground_effect.h).

### Stamping

`stamp_ground_effect(game, kind, x, y)` walks a `(2*radius+1)²` Chebyshev square from impact and skips tiles whose Bresenham line-of-sight from impact is blocked by a wall, `StructuralWall`, or vision-blocking fixture (the same tiles that block FOV). Each kept tile gets `ttl = max(1, center_ttl − ring * ring_falloff)`. On overlap with an existing entry of the same kind on the same tile, the new TTL is `max(new, existing)` (refresh, never stack).

### FOV integration

`compute_fov` and `compute_fov_lit` take an `OpacityProbe { TileMap*, unordered_set<uint64_t>* }`. The probe's `opaque(x, y)` returns true when the underlying tile is opaque OR when the override-set contains a packed `(x << 32) | y`. `Game::recompute_fov()` builds the override-set once per call from `opaque_ground_effect_tiles(game)` (kinds with `blocks_vision = true`).

`Game::recompute_fov()` also runs a "lit-region reveal" post-pass for fully-lit rooms — once a player can see one tile of a lit region, every tile of that region is marked visible. Smoke would be a no-op without an extra gate, since the room-reveal ignores normal FOV opacity. Each tile considered for reveal runs a Bresenham smoke-LOS check from the player; if any smoke tile sits between, the reveal is skipped.

### Per-kind table

| Kind | Radius | Center TTL | Ring falloff | Blocks vision | Notes |
|---|---|---|---|---|---|
| Smoke | 2 (5×5) | 24 | 6 (24 / 18 / 12 by ring) | yes | Outer ring expires after 12 ticks; cloud visibly shrinks before fully dissipating. Render glyph: `▓` / `▒` / `░` by TTL bucket, alternating per world tick. |

---

## Hacking — Detection (Plan 2 B-layer)

The **Detection** counter is per-zone, range `[0, 100]`. Each quickhack fired in the world adds `1-3` (per program). Detection decays `-1` every 5 ticks while the player is in steady state.

Threshold callbacks (fire only on upward crossing):

- **≥ 50** — local NPCs investigate (log line; Plan 3 will redirect them via the noise event system).
- **≥ 75** — dominant faction in the zone takes a `-10` reputation hit.
- **= 100** — ZONE ALARM. Every `Hackable` on the map flips to `Alarmed`; the dominant faction takes another `-25`. Existing reputation-driven hostility runs from there.

When the player moves to a different zone, the Detection counter resets to 0.

## Hacking — The Grid (Plan 3 A-layer)

### Trace

`Trace` is a per-`GridSession` counter `[0, 100]` that drives the heist clock. It rises every turn while jacked in.

| Source | Effect |
|---|---|
| Tier baseline | Subnet +1, Regional darknet +2, Deep-Grid anchor +3 / turn |
| White ICE in vision (manhattan ≤ 5) | +2 / turn (+1 with `Intrusion` skill) |
| Heat > 5 on equipped deck | +1 / turn |
| Forced deck reboot (heat > heat_cap) | +10 burst (one-shot) |
| Killing ICE | +3 per kill (`kill_if_dead`) |
| Walking onto a Firewall via `breach.exe` | +5 burst |
| Cracking a Gateway via `breach.exe` | +5 burst |
| `DeepGridNavigator` passive crack | +5 burst (50/50 roll on step) |
| `ghost_trace.exe` | -3 (lower bound 0) |

Breakpoints (each fires once per session — gated by `trace_alert_pulses`):

- **≥ 50** — Alert log line.
- **≥ 75** — One Gray ICE reinforcement spawns far from the avatar.
- **= 100** — One Black ICE summons; "BLACK ICE CONVERGING" log line.

### Heat

Per-deck (lives on `CyberdeckData`). Range `[0, deck.heat_cap]`. T1 deck = 10, T2 = 12 (+2 per tier).

- Each program firing adds `heat_cost` (skipped on the first program of the session if `GhostProtocol` is unlocked).
- Heat decays `cooling_rate` per turn (T1 = 1, T2 = 1).
- While Heat > 5, +1 Trace per turn (coupling).
- Heat > heat_cap → forced reboot: ram_current and heat_current zeroed, `s.ram = 0`, Trace +10, log line.

### ICE behavior

| Color | HP | Behavior |
|---|---|---|
| **White** | 1 | Patrols cardinally; in vision, raises Trace. Skipped while the player has `GhostCloak` (from `ghost_trace.exe`). |
| **Gray** | 2 | Pursues avatar greedily (manhattan); attacks for 1 avatar HP when adjacent. |
| **Black** | 4 | Always pursues; attacks for 2 (or 1 with `NeuralFortitude`). On avatar HP-zero, sets `last_killer_color = Black` so the disconnect path picks `BlackIceDeath`. |

`spawn_for_sector` places one White (always) and one Gray (tier ≥ 2) per session at sector init. Black is only spawned by the Trace = 100 breakpoint or the `:spawn-ice` dev verb.

### Disconnect outcomes

Set on `JackOutKind`; chosen by `tick_grid`'s avatar-HP-zero check (Gray/White → `NonBlackDeath`, Black → `BlackIceDeath`) or by player input.

| Kind | Loot | Side effect |
|---|---|---|
| **Voluntary** (walk to ⊙) | full credits + lore | none |
| **HardJackOut** (Shift+Q) | 50% credits | log line; lore not unlocked |
| **NonBlackDeath** | none | `BlackIceShock` (short, 20 ticks) on body |
| **BlackIceDeath** | none | Body HP -10 (-5 with `NeuralFortitude`); GameOver if HP ≤ 0; otherwise `BlackIceShock` (long, 60 ticks; `av/quickness -1`) |
| **SoftDisconnect** | none | Load-time recovery — Trace cleared, no penalty |

### Skill effects (Plan 3 v1)

| Skill | Effect |
|---|---|
| `Cat_Hacking` | Required to jack in. |
| `Intrusion` | White-ICE Trace bonus halved (2 → 1). |
| `IceBreaking` | `icebreaker_lite.exe` deals +1 damage. |
| `DaemonMastery` | +1 effective deck slot when picking programs to fire. |
| `GhostProtocol` | First program of each session is heatless. |
| `DeepGridNavigator` | Stepping onto a locked gateway: 50/50 free crack (Trace +5). |
| `NeuralFortitude` | Avatar HP_max +1 at jack-in; Black-ICE attack damage halved (2 → 1); Black-ICE bleed halved (10 → 5). |
| `CodeCraft` | Tinker workbench can craft T3 programs (`pulse_hammer.exe`, `daemon_hijack.exe`). |
| `ConsciousnessAnchor` | Capstone — unlocks a persistent deep-Grid base sector that survives Sgr A* rebirth. |

### Constants (`include/astra/grid_constants.h`)

```cpp
constexpr int kTraceMax       = 100;
constexpr int kIceVisionRange = 5;
constexpr int kKillIceTrace   = 3;
```

## Hacking — Plan 4 (D-layer): Consciousness, Deep-Grid, Rebirth

Plan 4 layers a **second save scope** on top of the per-galaxy save: a
profile-wide `consciousness.dat` that carries identity across Sgr A* rebirths.

### Soul Mirror channel

A non-hacker access path that lets any character with a Neural Backup implant
sync lore from a Precursor console without jacking in.

| Field | Value |
|---|---|
| Trigger | Stand adjacent to a Precursor console while wearing the Neural Backup implant. |
| Cost | 1 EP per turn while channelling. |
| Progress | +10% per turn at base. Each missing lore fragment on the console contributes once. |
| Commit | Channel completes at 100% — fragments append to `consciousness.dat.lore_archive`. |
| Interrupt | Taking damage, leaving the adjacent tile, or unequipping the implant cancels the channel. Progress is lost. |
| HUD | A horizontal strip below the side panel shows `Soul Mirror ░░░░░░░░░ 60%` while active. |

Per-console progress persists on the `Hackable` itself, so partial channels
resume on the next attempt.

### Sgr A* rebirth

Crossing the Sgr A* event horizon ends the current galaxy and starts a new
one with the player's consciousness intact.

1. **Confirm modal** — lists what survives the crossing (consciousness id,
   rebirth count, deep-Grid base presence, lore fragment count, grid currency).
2. **Cinematic** (first crossing only) — six key-paced reveal lines.
   `seen_first_rebirth` in `consciousness.dat` skips this on later rebirths.
3. **Apply** — `rebirth_count++`, write `consciousness.dat`, return to the
   main menu so the next New Game starts on a fresh galaxy with the saved
   consciousness applied.

### Cross-build survival matrix

| Build | What survives the crossing |
|---|---|
| `ConsciousnessAnchor` capstone | Lore + currency + AI contacts + deep-Grid base + signature programs |
| `Cat_Hacking` only | Lore + currency + AI contacts |
| Non-hacker w/ Neural Backup | Lore archive only |
| Non-hacker, no implant | Nothing (true rebirth) |

The schema fields exist for every save; gating is at the *write* side. A
non-hacker without `ConsciousnessAnchor` simply never has `deep_grid_base`
populated — the field stays `nullopt`.

### Netmap overlay

`netmap` (or `N` in the Hacking tab) opens a modal overlay over the terminal
pane. Two zoom layers — **Regional** (Subnet + RegionalDarknet nodes) and
**Deep-Grid** (DeepGridAnchor nodes). Switch with `,`. Arrow keys step the
cursor between nodes by closest-in-direction. `Enter` jacks into the
selected node; locked edges show red and ignore Enter (Plan 5 will add the
breach UX). `Esc` closes.

### Regional darknet generation

Regional sectors are 40×24 BSP-split into 4–8 firewall-bordered rooms with
single-tile floor doorways between siblings. The exit node lands in the room
farthest from spawn. Decoration: 1–4 `EncryptedFile`, 0–2 `DataNode`, 50%
chance of a deep-Grid `Gateway`. Tier 3 networks gain an extra encrypted file.

### Camera

The grid renderer follows the avatar with a 4-cell deadzone. Sectors that
fit the viewport stay locked at the origin (legacy behaviour); larger
sectors scroll only when the avatar approaches the viewport edge.
