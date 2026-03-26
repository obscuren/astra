# Skill Implementation — Making Skills Matter

## Status Quo

Only tinkering skills (BasicRepair, Disassemble, Synthesize, Cat_Tinkering) have real gameplay logic. The remaining 12 skills are defined and learnable but have zero gameplay effect.

---

## Passive Skills — Stat Modifiers

These should be checked wherever the relevant stat is computed. Most can be implemented as simple conditionals in existing formulas.

### Endurance

**ThickSkin** — "+1 natural armor"
- In `effective_defense()` (player.h): if player has ThickSkin, add +1 to defense value
- Simple: `if (player_has_skill(p, SkillId::ThickSkin)) def += 1;`

**IronWill** — "+5 psionic resistance"
- Needs a resistance check system first (psionic effects don't exist yet)
- Defer until psionic enemies/effects are added
- When implemented: add to resistance roll in status effect application

### Acrobatics

**Swiftness** — "+5 DV vs missile weapons"
- In damage calculation for ranged attacks against player: check if attacker is using ranged, then add +5 to player's dodge value
- Requires: distinguishing melee vs ranged NPC attacks (NPCs currently only melee)
- Defer until NPCs have ranged attacks

### Weapon Expertise (ShortBlade, LongBlade, Pistol, Rifle)

**ShortBladeExpertise** — "+1 hit with short blades, -25% action cost"
**LongBladeExpertise** — "+1 hit with long blades, parry chance"
**SteadyHand** — "+1 pistol accuracy"
**Marksman** — "+2 rifle range"

All require:
1. **Weapon type tagging** — items need a `WeaponClass` field (ShortBlade, LongBlade, Pistol, Rifle)
2. **Hit/accuracy roll** — combat currently uses flat attack vs defense, needs an accuracy modifier
3. **Action cost system** — attacks currently cost a fixed amount, needs per-weapon cost

Implementation plan:
- Add `WeaponClass` enum to Item (ShortBlade, LongBlade, Pistol, Rifle, Unarmed)
- Tag existing weapons appropriately
- In `attack_npc()`: check equipped weapon class, apply expertise bonuses
- In `shoot_target()`: check weapon class, apply accuracy/range bonuses
- Action cost: multiply base cost by 0.75 if ShortBladeExpertise + short blade equipped

### Persuasion

**Haggle** — "10% better prices"
- In trade window: when computing buy/sell values, check for Haggle
- `buy_price = item.buy_value * (player_has_skill(p, Haggle) ? 0.9 : 1.0)`
- `sell_price = item.sell_value * (player_has_skill(p, Haggle) ? 1.1 : 1.0)`
- Straightforward — can implement immediately in trade_window.cpp

---

## Active Skills — Abilities System

These require a new "use ability" action. Each active skill has a cooldown, targets, and an effect.

### Architecture

```
struct Ability {
    SkillId skill_id;
    const char* name;
    int cooldown_ticks;        // turns between uses
    int last_used_tick = -999;
    bool requires_target;      // needs targeting cursor
    bool requires_adjacent;    // target must be adjacent
};
```

Player gets an ability bar (already reserved: "ABILITIES: [reserved]" in the UI). Abilities are unlocked when the corresponding skill is learned. Activate with number keys 1-5 or a dedicated ability menu.

### Ability Definitions

**Tumble** (Acrobatics) — Dodge away when hit
- Trigger: automatic on taking melee damage (passive-reactive, not truly active)
- Effect: 20% chance (scales with AGI) to move 1 tile away from attacker, halving damage
- Alternative: make it active — press key to dodge next incoming attack

**Jab** (Short Blade) — Off-hand quick strike
- Requires: short blade equipped
- Effect: immediate extra attack at 50% damage, costs 25 ticks
- Cooldown: 3 turns

**Cleave** (Long Blade) — AoE arc attack
- Requires: long blade equipped
- Effect: attack all adjacent hostile NPCs in a 3-tile arc facing cursor direction
- Cooldown: 5 turns

**Quickdraw** (Pistol) — Instant shot
- Requires: pistol equipped
- Effect: fire at target for reduced action cost (25 ticks instead of 50)
- Cooldown: 3 turns

**SuppressingFire** (Rifle) — Pin enemies in cone
- Requires: rifle equipped, ammo
- Effect: 3-tile cone, all enemies in cone lose 50% movement speed for 5 turns
- Consumes 3 ammo
- Cooldown: 8 turns

**Intimidate** (Persuasion) — Frighten enemy
- Requires: adjacent hostile NPC
- Effect: target flees (moves away from player) for 3-5 turns (scales with WIL)
- Cooldown: 10 turns

---

## Implementation Priority

### Phase 1 — Quick wins (passive, no new systems needed)
1. **Haggle** — price modifier in trade window
2. **ThickSkin** — +1 defense in effective_defense()

### Phase 2 — Weapon class system
1. Add `WeaponClass` enum to Item
2. Tag all existing weapons
3. **ShortBladeExpertise, LongBladeExpertise, SteadyHand, Marksman** — apply bonuses based on weapon class

### Phase 3 — Active ability system
1. Build ability bar UI and cooldown tracking
2. Implement each active skill one at a time
3. Start with **Jab** (simplest — extra attack) and **Intimidate** (flee behavior)
4. Then **Cleave**, **Quickdraw**, **SuppressingFire**
5. **Tumble** last (reactive trigger is more complex)

### Phase 4 — Status effect prerequisites
1. Build status effect system (buffs/debuffs with duration)
2. **IronWill** — resistance to psionic status effects
3. **Swiftness** — DV bonus vs ranged (needs NPC ranged attacks)
4. **SuppressingFire** slow effect

---

## Dependencies

- Weapon class tagging → weapon expertise skills
- NPC ranged attacks → Swiftness
- Status effect system → IronWill, SuppressingFire, Intimidate flee
- Ability bar UI → all active skills
- Psionic enemies → IronWill
