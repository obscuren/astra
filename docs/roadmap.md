# Astra Roadmap

## Combat & Skills

### Quick Wins
- [x] **Haggle** — 10% better buy/sell prices via permanent passive effect
- [x] **ThickSkin** — +1 defense via permanent passive effect

### Combat Mechanics
- [x] **Dodge/miss chance** — player dodge (DV*2, cap 50%), NPC dodge (level-based, cap 25%)
- [x] **Critical hits** — LUC-based crit chance (cap 30%), 1.5x multiplier, player only
- [ ] **Status effects from combat** — weapons/skills apply burn, poison, slow on hit
- [x] **Weapon class system** — WeaponClass enum, all 10 weapons tagged

### Weapon Expertise (requires weapon classes)
- [x] **ShortBladeExpertise** — +1 damage with short blades
- [x] **LongBladeExpertise** — +1 damage with long blades
- [x] **SteadyHand** — +1 damage with pistols
- [x] **Marksman** — +1 damage, +2 range with rifles

### Active Abilities (requires ability bar UI)
- [x] **Jab** — 50% damage quick strike, 3 tick cooldown (ShortBlade)
- [x] **Cleave** — hit all adjacent hostiles, 5 tick cooldown (LongBlade)
- [x] **Quickdraw** — fast ranged shot, 3 tick cooldown (Pistol)
- [x] **Intimidate** — frighten adjacent enemy, flee for WIL-scaled duration
- [ ] **SuppressingFire** — cone AoE, apply slow, 8 turn cooldown
- [ ] **Tumble** — reactive dodge on melee hit (complex)

### NPC Combat
- [ ] **Ranged NPC attacks** — NPCs with ranged weapons shoot instead of chase
- [x] **Flee behavior** — Intimidate causes NPCs to flee (move away from player)

---

## Crafting & Tinkering

- [ ] **More synthesis recipes** — expand beyond the initial 10
- [ ] **Blueprint discovery flow** — find blueprints in dungeons, learn from analyzing loot
- [x] **Repair bench fixture** — pay credits to restore durability, no skill required

---

## Content

### NPCs & Quests
- [x] **Quest system core** — QuestManager, objectives, rewards, quests tab
- [x] **Quest visual markers** — `!` markers on star chart (galaxy/region/local/system views) and overworld
- [x] **Story quest framework** — StoryQuest base class, quest-triggered world modification
- [x] **The Missing Hauler** — first story quest with dialog integration, dungeon spawn, quest items
- [ ] More NPC types with unique dialog trees
- [x] **Random quest generation** — kill/fetch/deliver/scout templates, NPC role-based offering
- [x] **Quest save/load** — full quest state persistence (active, completed, quest locations)
- [x] **Faction reputation effects** — pricing, dialog gates, quest availability, tiered shop stock

### Gameplay
- [x] **Auto-walk/explore** — `w` + direction = walk straight, `ww` = BFS auto-explore

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

- [x] **Conversation dialog in contextual menu** — NPC speech shown as body text in PopupMenu
- [ ] **Minimap** — small overview in corner
- [x] **Ability bar** — display and activate learned abilities (keys 1-5)
- [ ] **Message log scrollback** — scroll through message history
- [ ] **Item comparison** — show stat diff when hovering equipment
- [x] **Equipment tab read-only** — side panel equipment is display-only
- [x] **Remove inventory from side panel** — inventory management via character screen only

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
