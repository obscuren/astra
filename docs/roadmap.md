# Astra Roadmap

## Combat & Skills

### Quick Wins
- [x] **Haggle** — 10% better buy/sell prices via permanent passive effect
- [x] **ThickSkin** — +1 defense via permanent passive effect

### Combat Mechanics
- [ ] **Dodge/miss chance** — roll against effective_dodge(), miss = no damage
- [ ] **Critical hits** — roll against LUC stat, crit = 2x damage
- [ ] **Status effects from combat** — weapons/skills apply burn, poison, slow on hit
- [x] **Weapon class system** — WeaponClass enum, all 10 weapons tagged

### Weapon Expertise (requires weapon classes)
- [x] **ShortBladeExpertise** — +1 damage with short blades
- [x] **LongBladeExpertise** — +1 damage with long blades
- [x] **SteadyHand** — +1 damage with pistols
- [x] **Marksman** — +1 damage, +2 range with rifles

### Active Abilities (requires ability bar UI)
- [ ] **Jab** — extra attack at 50% damage, 3 turn cooldown
- [ ] **Cleave** — hit all adjacent hostiles, 5 turn cooldown
- [ ] **Quickdraw** — shoot at reduced action cost, 3 turn cooldown
- [ ] **Intimidate** — target flees for 3-5 turns, 10 turn cooldown
- [ ] **SuppressingFire** — cone AoE, apply slow, 8 turn cooldown
- [ ] **Tumble** — reactive dodge on melee hit (complex)

### NPC Combat
- [ ] **Ranged NPC attacks** — NPCs with ranged weapons shoot instead of chase
- [ ] **Flee behavior** — low-HP or intimidated NPCs run away

---

## Crafting & Tinkering

- [ ] **More synthesis recipes** — expand beyond the initial 10
- [ ] **Blueprint discovery flow** — find blueprints in dungeons, learn from analyzing loot
- [ ] **Repair bench functionality** — currently placeholder ("Under maintenance")

---

## Content

### NPCs & Quests
- [ ] More NPC types with unique dialog trees
- [ ] Quest system expansion — multi-step quests with objectives
- [ ] Faction reputation effects — NPCs react to your standing

### World Generation
- [ ] New dungeon generator types / biomes
- [ ] More overworld POI stamps (temples, factories, research labs)
- [ ] Derelict station expansion — more room types

### Items & Gear
- [ ] New weapon types
- [ ] Ship components and upgrades
- [ ] Consumables with effects (stims, shields, scanners)

### Starship
- [ ] Ship upgrade system (engine, navi computer, hull, weapons)
- [ ] Ship combat (space encounters)
- [ ] Ship inventory / cargo hold

---

## UI/UX

- [ ] **Conversation dialog in contextual menu** — show NPC speech in the popup, not just message log
- [ ] **Autoexplore** — auto-walk through unexplored areas until enemy/item found
- [ ] **Minimap** — small overview in corner
- [ ] **Ability bar** — display and activate learned abilities (keys 1-5)
- [ ] **Message log scrollback** — scroll through message history
- [ ] **Item comparison** — show stat diff when hovering equipment

---

## Technical

### Architecture (completed)
- [x] WorldManager — world state container
- [x] CombatSystem — combat logic
- [x] DialogManager — NPC dialogs
- [x] SaveSystem — save/load
- [x] DevConsole — dev tools
- [x] HelpScreen — help overlay
- [x] InputManager — look mode
- [x] MapRenderer — reusable map drawing
- [x] Effects system — damage pipeline, status effects
- [x] Character creation wizard

### Remaining Architecture
- [ ] Move world transition methods to WorldManager (low priority)
- [ ] Extract more focused renderers from game_rendering.cpp (as needed)
- [ ] SDL renderer parity (deferred)
