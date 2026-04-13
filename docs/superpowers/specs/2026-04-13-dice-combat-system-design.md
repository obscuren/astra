# Dice-Based Combat System Design

**Date:** 2026-04-13
**Status:** Approved

## Overview

Replace the flat arithmetic combat system with a dice-based, two-layer defense system featuring five damage types, equippable energy shields, and dice expressions throughout. Every weapon, armor piece, NPC stat block, and combat interaction uses dice rolls.

## Combat Flow

```
Attacker attacks
  → 1d20 + Agility mod + weapon skill bonus  vs  defender's DV (Dodge Value)
    → Miss? No damage.
    → Hit?
      → Shield HP > 0?
        → Yes: Penetrate as if AV=0, roll damage, reduce shield HP
                (excess damage on the killing blow is absorbed, not passed through)
        → No: Penetration roll
          → 1d10 + Strength mod  vs  defender's effective AV
            → Fail? Bounces off (0 damage)
            → Succeed? Roll weapon damage dice
            → Exceed AV by 4+? Multi-penetration (roll damage dice again per +4 threshold)
              → Sum all penetration damage rolls = total damage dealt
```

### Natural Roll Rules

- **Attack roll (d20):** Natural 20 always hits. Natural 1 always misses.
- **Penetration roll (d10):** Natural 10 always penetrates. Natural 1 always fails.

### Critical Hits

- **Chance:** `clamp((LUC - 8) * 2 + 3, 0, 30)%` (unchanged formula)
- **Effect:** Auto-penetrate (skip penetration roll entirely) + roll weapon damage dice twice and sum. Makes LUC a direct counter to high-AV targets.

## Damage Types

Five damage types:

| Type | Thematic Source |
|------|-----------------|
| **Kinetic** | Physical impacts: blades, batons, projectiles |
| **Plasma** | Superheated energy: plasma weapons |
| **Electrical** | Ion/arc weapons: ion blaster, arc caster |
| **Cryo** | Cold-based weapons |
| **Acid** | Corrosive weapons and creature attacks |

Every weapon has exactly one damage type. Damage type is shown in item descriptions and tooltips.

## DV (Dodge Value)

Replaces the current percent-based dodge system.

**Calculation:**
```
DV = base_dv + (AGI - 10) / 2 + equipment_dv_mod + effect_dv_mod
```

- `base_dv` derived from current `dodge_value` (default 3)
- Light armor may boost DV; heavy armor penalizes DV
- Effects like DodgeBoost add to DV
- Attacker must roll `1d20 + (AGI - 10) / 2 + weapon_skill_bonus >= target DV` to hit

## AV (Armor Value) and Damage Type Affinity

Each armor piece defines:
- **Base AV** — the armor's general protection value
- **Per-type affinity modifiers** — bonuses or penalties to AV for each of the five damage types

Example:
```
Composite Armor:
  Base AV: 4
  Kinetic: +2, Plasma: -1, Electrical: +0, Cryo: +0, Acid: -2
```

**Effective AV** for a hit = sum of all equipped armor base AV + sum of all affinity modifiers for the incoming damage type.

**Penetration roll:** `1d10 + (STR - 10) / 2 vs effective AV`
- If PV > AV: one penetration, roll weapon damage dice
- For each additional +4 over AV: additional penetration, roll damage dice again
- All penetration damage rolls are summed

## Resistances

Player resistances (Kinetic, Plasma/Heat, Electrical, Cold/Cryo, Acid) apply as **percentage reduction** after penetration damage is calculated. This is a final damage reduction step separate from AV.

These resistances are displayed in the UI stats/equipment screen.

## Energy Shields

### Equipment

- **New equipment slot:** Shield (dedicated paperdoll slot, not a hand slot)
- The old hand-held Riot Shield is removed from the game
- Shield items are energy-based technology, not physical shields

### Shield Stats

Each shield item defines:
- **Capacity** — maximum shield HP
- **Current shield HP** — current charge level
- **Per-type affinity modifiers** — optional bonuses/penalties vs specific damage types (e.g. a Plasma Shield absorbs Plasma damage more efficiently)

### Mechanics

1. When a hit lands (d20 vs DV succeeds) and the target has shield HP > 0, roll penetration and damage as if AV = 0 (shields don't resist penetration — they absorb raw damage)
2. Shield type affinities modify how much damage the shield absorbs per type (e.g. a Plasma Screen absorbs 50% more from Plasma hits, meaning less shield HP is drained)
3. Resulting damage reduces shield HP. If shield HP reaches 0, the excess damage is discarded (the shield fully absorbs the killing blow — no overflow to armor on the same hit)
4. On subsequent hits with shield HP at 0, the normal AV/penetration flow applies against equipped armor
5. **No auto-recharge.** Player manually recharges shield HP from Battery items, using the same system as ranged weapon charging

### Shield Items (Initial Set)

| Shield | Capacity | Rarity | Affinities |
|--------|----------|--------|------------|
| Basic Deflector | 10 | Common | None |
| Plasma Screen | 15 | Uncommon | Plasma +50% absorb |
| Ion Barrier | 15 | Uncommon | Electrical +50% absorb |
| Composite Barrier | 20 | Rare | Kinetic +25%, Plasma +25% |
| Hardlight Aegis | 30 | Epic | All types +25% absorb |
| Void Mantle | 40 | Legendary | All types +50% absorb |

## Weapon Damage Dice

Weapons use dice expressions instead of flat attack bonuses. The dice expression is included in the weapon's display name (e.g. "Arc Caster - 2d4").

### Damage Scale by Rarity

| Rarity | Melee | Ranged |
|--------|-------|--------|
| Common | 1d4 | 1d6 |
| Uncommon | 1d6+1 | 1d8+1 |
| Rare | 2d4+2 | 2d6 |
| Epic | 2d6+2 | 2d8+1 |
| Legendary | 3d6+3 | 3d8+2 |

### Weapon Definitions (Updated)

All weapons receive: damage dice, damage type, and updated descriptions.

**Melee Weapons:**

| Weapon | Display Name | Dice | Type | Rarity |
|--------|-------------|------|------|--------|
| Combat Knife | Combat Knife - 1d4 | 1d4 | Kinetic | Common |
| Stun Baton | Stun Baton - 1d4+1 | 1d4+1 | Electrical | Common |
| Vibro Blade | Vibro Blade - 1d6+1 | 1d6+1 | Kinetic | Uncommon |
| Plasma Saber | Plasma Saber - 2d4+2 | 2d4+2 | Plasma | Rare |
| Ancient Mono-Edge | Ancient Mono-Edge - 2d6+2 | 2d6+2 | Kinetic | Epic |

**Ranged Weapons:**

| Weapon | Display Name | Dice | Type | Rarity |
|--------|-------------|------|------|--------|
| Plasma Pistol | Plasma Pistol - 1d6 | 1d6 | Plasma | Common |
| Ion Blaster | Ion Blaster - 1d8+1 | 1d8+1 | Electrical | Uncommon |
| Pulse Rifle | Pulse Rifle - 2d6 | 2d6 | Kinetic | Rare |
| Arc Caster | Arc Caster - 2d8+1 | 2d8+1 | Electrical | Epic |
| Void Lance | Void Lance - 3d8+2 | 3d8+2 | Plasma | Legendary |

## Armor Definitions (Updated)

All armor pieces receive: base AV, per-type affinity modifiers, optional DV modifier, and updated descriptions.

| Armor | Slot | Base AV | Kinetic | Plasma | Electrical | Cryo | Acid | DV Mod | Rarity |
|-------|------|---------|---------|--------|------------|------|------|--------|--------|
| Padded Vest | Body | 2 | +1 | +0 | +0 | +0 | -1 | +0 | Common |
| Composite Armor | Body | 4 | +2 | -1 | +0 | +0 | -2 | -1 | Uncommon |
| Exo-Suit | Body | 6 | +1 | +1 | -2 | +1 | +0 | -2 | Rare |
| Flight Helmet | Head | 1 | +0 | +0 | +0 | +0 | +0 | +0 | Common |
| Tactical Helmet | Head | 2 | +1 | +0 | +0 | -1 | +0 | +0 | Uncommon |

## NPC Stat Blocks

Each NPC definition gets explicit combat stats. No NPC equipment system — values are defined directly.

| NPC | DV | Base AV | Damage Dice | Damage Type | Kinetic | Plasma | Electrical | Cryo | Acid |
|-----|-----|---------|-------------|-------------|---------|--------|------------|------|------|
| Xytomorph | 8 | 4 | 1d6 | Acid | +2 | -2 | +0 | +0 | +3 |
| Drifter | 10 | 2 | 1d4 | Kinetic | +0 | +0 | +0 | +0 | +0 |
| Scavenger | 9 | 3 | 1d6 | Kinetic | +1 | +0 | -1 | +0 | +0 |
| Prospector | 8 | 2 | 1d4+1 | Kinetic | +0 | +0 | +0 | +0 | +0 |

NPC stat blocks scale with level: DV and AV increase slightly per level, damage dice remain fixed but elite NPCs get bonus dice.

## Item Display Changes

- **Weapon names** include dice notation: `"Arc Caster - 2d8+1"`
- **Item descriptions** updated to reference damage type, AV, DV modifiers, and affinities
- **Tooltips** show full breakdown: dice, damage type, and any special properties
- **Combat log** shows roll results: `"You strike the Xytomorph (17 vs DV 8 — hit!) → penetration 7 vs AV 4 — 2 penetrations → 2d4+2: 5 + 7 = 12 damage"`

## Dice Utility

New `Dice` struct for parsing and rolling dice expressions:

```
struct Dice {
    int count;    // number of dice (e.g. 2 in "2d6+3")
    int sides;    // sides per die (e.g. 6 in "2d6+3")
    int modifier; // flat modifier (e.g. 3 in "2d6+3")

    int roll(std::mt19937& rng) const;
    std::string to_string() const;  // "2d6+3"
    static Dice parse(const std::string& expr);
};
```

## UI Changes

- **Shield HP bar** in the player info panel alongside the HP bar
- **Per-type resistance/affinity display** in the equipment and stats screens
- **Dice expressions** shown on weapon tooltips instead of flat +ATK numbers
- **DV and AV** shown in player stats panel
- **Combat log** updated with roll details

## What Gets Removed

- Flat `attack_value` / `defense_value` as damage arithmetic → replaced by dice rolls + AV
- Percent-based dodge system → replaced by DV (d20 roll)
- `StatModifiers::attack` / `StatModifiers::defense` → replaced by `Dice` and AV
- Riot Shield item → replaced by energy shield items
- `roll_percent` for dodge → replaced by d20 vs DV comparison

## What Gets Updated

- All weapon definitions in `item_defs.cpp` — dice, damage type, display names, descriptions
- All armor definitions in `item_defs.cpp` — base AV, per-type affinities, DV modifiers, descriptions
- All NPC definitions — DV, AV, damage dice, damage type, per-type affinities
- `CombatSystem` — complete rewrite of attack, shoot, and NPC turn logic
- `Player` struct — add DV/AV derived stat methods, shield slot, shield HP
- `Equipment` struct — add Shield slot
- `Item` struct — add `Dice` field, `DamageType` field, per-type affinity map
- `StatModifiers` — replace attack/defense with AV, DV modifiers
- `effect.cpp` — update `apply_damage_effects` for new damage pipeline
- `formulas.md` — complete rewrite of combat formulas section
- Ability damage calculations — Jab, Cleave, Quickdraw use weapon dice
- Level scaling in `item_gen.cpp` — scale dice (bonus modifier) instead of flat attack
- Affix system — affixes modify dice bonus, AV, DV, or add type affinities
