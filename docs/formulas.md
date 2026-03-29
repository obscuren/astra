# Gameplay Formulas

Reference for all gameplay formulas used in Astra.

## Derived Stats

- **Effective Attack**: `attack_value + (STR - 10) / 2 + equipment.attack + effects.attack`
- **Effective Defense**: `defense_value + (TOU - 10) / 3 + equipment.defense + effects.defense`
- **Effective Dodge**: `dodge_value + (AGI - 10) / 3 + effects.dodge_mod`
- **Effective Max HP**: `max_hp + (TOU - 10) * 2 + equipment.max_hp + effects.max_hp`

Base values: attack=1, defense=5, dodge=3, max_hp=10

## Combat

### Damage Formulas

- **Player → NPC (melee)**: `effective_attack + weapon_expertise_bonus(+1)`
- **Player → NPC (ranged)**: same as melee, uses missile slot weapon
- **NPC → Player**: `npc.base_damage * npc.level + (elite ? 1 : 0) - effective_defense`
- **NPC XP Reward**: `base_xp * level * (elite ? 3 : 1)`
- **Kill Credits**: `level * 2 + (elite ? 5 : 0)`

### Dodge/Miss Chance

- **Player dodge chance**: `min(effective_dodge * 2, 50)` — checked before NPC deals damage
- **NPC dodge chance**: `min(npc.level + (elite ? 5 : 0), 25)` — checked before player deals damage
- Dodge check: `roll 1-100 <= dodge_chance` → miss (no damage applied)
- Ranged miss still consumes ammo

### Critical Hits (Player only)

- **Crit chance**: `clamp((LUC - 8) * 2 + 3, 0, 30)`
- **Crit multiplier**: 1.5x → `damage + (damage + 1) / 2` (always at least +1)
- Checked after base damage, before effects pipeline

### Damage Effects Pipeline

For each active effect on the target:
```
damage = damage * effect.damage_multiplier / 100
damage += effect.damage_flat_mod
damage = max(damage, 0)
```
- Invulnerable: `damage_multiplier = 0` (immune to all damage)

### Loot Drops

- 50% chance on enemy kill
- Item level = npc.level
- Rarity: Common 50%, Uncommon 30%, Rare 15%, Epic 4%, Legendary 1%

### Ability Cooldowns

| Ability | Cooldown | Action Cost | Weapon | Effect |
|---------|----------|-------------|--------|--------|
| Jab | 3 ticks | 25 | ShortBlade | 50% effective_attack damage |
| Cleave | 5 ticks | 50 | LongBlade | Full damage to all adjacent hostiles |
| Quickdraw | 3 ticks | 25 | Pistol | Full damage to current target |
| Intimidate | 10 ticks | 50 | Any | Target flees for 3 + (WIL-10)/2 ticks (min 2) |

## Economy

### Trade Prices

```
total_mod  = effect_pct + faction_pct
buy_cost   = buy_value + (buy_value * total_mod / 100)
sell_price  = sell_value + (sell_value * (-faction_pct + effect_sell_pct) / 100)
```
- Haggle effect: `buy_price_pct = -10`, `sell_price_pct = +10`
- Faction reputation modifier (`faction_pct`):
  - Hated (rep <= -50): +30%
  - Disliked (rep -49 to -10): +15%
  - Neutral (rep -9 to 9): 0%
  - Liked (rep 10 to 49): -10%
  - Trusted (rep >= 50): -20%

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
