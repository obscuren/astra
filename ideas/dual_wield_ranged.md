# Dual-Wield Ranged Weapons

## Problem

Currently ranged weapons can only go in the Missile slot. Players should be able to equip pistols in left/right hand too, but this raises design questions.

## Design Questions

### Which weapon fires when pressing `s`?
- Option A: Always fires the Missile slot (current behavior)
- Option B: Fires the "active" hand (toggle with a key)
- Option C: Fires both if both are ranged (dual-wield burst)

### How does reload (`r`) work?
- Option A: Reloads whichever has less charge
- Option B: Shows a popup asking which weapon
- Option C: Reloads the active hand

### How does targeting overlay show range?
- Different weapons may have different ranges
- Show the shorter range? The active hand's range?

### What about the Missile slot?
- If pistols can go in hands, is Missile slot for rifles only?
- Or can any ranged weapon go in any of the 3 slots (left, right, missile)?

## Suggested Approach

Keep it simple initially:
1. Pistols (small ranged) can go in LeftHand or RightHand
2. Rifles stay Missile-only (too big for one hand)
3. `s` fires Missile slot first, falls back to right hand, then left hand
4. `r` reloads the weapon that would fire next (same priority)
5. Targeting range uses the weapon that would fire

This avoids complex UI while allowing pistol + melee or pistol + shield combos.

## Dependencies
- WeaponClass system (done — Pistol vs Rifle distinction exists)
- Hand equip UI (done — left/right hand selection for melee)
- Ammo/charge rework may be needed for dual pistols
