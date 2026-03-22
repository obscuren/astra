# Gameplay Formulas

Reference for all gameplay formulas used in Astra.

## Combat

- **NPC Attack Damage**: `base_damage * level + (elite ? 1 : 0)`
- **Player Attack Damage**: `attack_value` (minimum 1)
- **NPC XP Reward**: `base_xp * level * (elite ? 3 : 1)`

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
