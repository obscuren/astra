# Gameplay Formulas

Reference for all gameplay formulas used in Astra.

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

### Weapon Damage Dice

| Rarity | Melee | Ranged |
|---|---|---|
| Common | 1d4 | 1d6 |
| Uncommon | 1d6+1 | 1d8+1 |
| Rare | 2d4+2 | 2d6 |
| Epic | 2d6+2 | 2d8+1 |
| Legendary | 3d6+3 | 3d8+2 |

Unarmed: 1d3 Kinetic

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
