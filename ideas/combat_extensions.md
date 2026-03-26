# Combat System Extensions

The CombatSystem class is now a clean container for all combat logic. Here's what can be added.

## Low Complexity

### Dodge/Miss Chance
- In `attack_npc()` and `process_npc_turn()`: roll against target's effective_dodge()
- Miss = no damage, log "X dodges your attack"
- Ties into Swiftness skill (+5 DV vs missile) and DodgeBoost effect

### Critical Hits
- In `attack_npc()`: roll against player LUC stat
- Crit = 2x damage, special message
- LUC-10 as percentage bonus to crit chance

### Status Effects from Combat
- In `attack_npc()` / `shoot_target()`: weapon or skill can apply burn, poison, slow to target
- Check weapon properties or active skill, then `add_effect(npc.effects, make_burn(...))`

### Skill-Based Combat Bonuses (from ideas/skill_implementation.md)
- **ThickSkin**: +1 defense in `effective_defense()`
- **Haggle**: 10% price modifier in trade (not combat, but same pattern)
- Check `player_has_skill()` in damage calculation

## Medium Complexity

### Active Abilities
- New methods on CombatSystem: `use_ability(SkillId, Game&)`
- Triggered from ability bar (keys 1-5) or input
- Each ability has cooldown tracking (use effects system with duration)
- **Jab**: extra attack at 50% damage, 3 turn cooldown
- **Cleave**: hit all adjacent hostiles, 5 turn cooldown
- **Quickdraw**: shoot at reduced action cost, 3 turn cooldown
- **Intimidate**: target flees for 3-5 turns, 10 turn cooldown

### Ranged NPC Attacks
- In `process_npc_turn()`: if NPC has ranged weapon and target in range, shoot instead of chase
- Needs: NPC equipment or ranged flag on NPC struct
- Line-of-sight check before firing

### Weapon Class Bonuses
- Requires: `WeaponClass` enum on Item (ShortBlade, LongBlade, Pistol, Rifle)
- In `attack_npc()`: check equipped weapon class + learned skill
- **ShortBladeExpertise**: +1 hit, -25% action cost
- **LongBladeExpertise**: +1 hit, parry chance
- **SteadyHand**: +1 pistol accuracy
- **Marksman**: +2 rifle range

### AoE Attacks
- **Cleave**: iterate adjacent hostile NPCs in an arc
- **SuppressingFire**: cone-shaped area, apply slow effect to all enemies
- Need: direction selection, area calculation

## High Complexity

### Reactive Abilities
- **Tumble** (Acrobatics): on taking melee damage, 20% chance to dodge away
- Triggers automatically in `process_npc_turn()` damage block
- Needs: interrupt the normal damage flow, reposition player

### Flee Behavior
- For **Intimidate** skill and low-HP NPCs
- In `process_npc_turn()`: if NPC has Flee effect, move away from player instead of toward
- Pathfinding away from player (reverse of chase logic)

## Dependencies

- Weapon class tagging → weapon expertise skills
- NPC ranged capability → ranged NPC attacks
- Ability bar UI → active abilities
- Status effect duration tracking → cooldowns
