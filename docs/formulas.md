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
