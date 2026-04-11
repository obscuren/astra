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

### Wayfinding Skills
- [x] **Scout's Eye** — show NPCs on minimap (75 SP, INT 13)
- [x] **Cartographer** — show items and POIs on minimap (100 SP, INT 14)

### Archaeology Skills
- [x] **Archaeology skill category** — Ruin Reader, Artifact ID, Excavation, Cultural Attunement, Precursor Linguist, Beacon Sense

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
- [x] **More NPC types with unique dialog trees** — random civilians with race-based glyphs, unique names, flavor dialog pools
- [x] **Random quest generation** — kill/fetch/deliver/scout templates, NPC role-based offering
- [x] **Quest save/load** — full quest state persistence (active, completed, quest locations)
- [x] **Faction reputation effects** — pricing, dialog gates, quest availability, tiered shop stock
- [ ] **Story quest chains** — hand-tailored multi-quest arcs (A → B → C), prerequisite system
- [ ] **World-driven random quests** — generate quests from actual world state instead of hardcoded arrays
- [ ] **Quest failure mechanics** — expiration, consequences, reputation loss

### Gameplay
- [x] **Auto-walk/explore** — `w` + direction = walk straight, `ww` = BFS auto-explore

### World Generation
- [x] **Procedural world lore** — billions of years of layered history, 2-5 precursor civilizations, beacon network toward Sgr A*
- [x] **Phoneme-based naming** — 6 syllable pools, procedural civilization/figure/artifact names
- [x] **Developer history log** — `history` dev console command dumps full timeline
- [x] **Lore save/load** — WorldLore persisted via tagged LORE section
- [x] **Lore-driven galaxy shaping** — system tiers, lore annotations, star chart markers, dungeon entry text
- [x] **Galaxy simulation engine** — state-driven civ sim with traits, inter-species interactions, visual generation screen
- [ ] **Legendary artifact generation** — unique items tied to historical figures and events
- [ ] **Lore fragment items** — data crystals, memory engrams as ground pickups in lore-significant ruins
- [ ] **Lore fragment system** — discoverable history pieces, progressive revelation via journal codex
- [ ] **Starting lore fragment** — player receives a race-specific lore record at game start
- [x] **Terrain shaping from lore** — megastructures as orbital POIs, beacons as unique landmarks, terraforming alters biome, weapon tests scar terrain
- [ ] **Archaeology skill effects** — Excavation active ability, Precursor Linguist sealed doors, Beacon Sense on star chart, Cultural Attunement stat bonuses
- [ ] **Civilization-themed dungeon aesthetics** — tile palette, fixture types, room flavors per precursor civilization
- [ ] **Archaeological strata** — deeper dungeon levels = older civilization layers
- [ ] New dungeon generator types / biomes
- [x] **Outpost POI** — fenced fort with main building, exterior tents, campfires, biome-themed palisade
- [ ] **Outpost dungeon portal** — rare (~20%) chance of a dungeon entrance beneath an outpost
- [ ] **Outpost kind variants** — forward base / refuge / scoundrel hideout / traveler camp with distinct NPCs and loot
- [ ] **Outpost reputation / hostility** — hostile outposts spawn combatants instead of traders
- [x] **Crashed Ship POI** — three classes (escape pod / freighter / corvette), 4-way orientation, long scorched skid marks that plow through scatter, debris field, rare dungeon portal
- [ ] **Crashed ship dungeon theming** — wreck-themed dungeon content when the portal hits
- [ ] **Crashed ship kind variants** — pirate / civilian / military / alien flavoring via scatter + fixture palettes
- [ ] **Crashed ship lore logs** — readable captain's log fixture on cockpit console
- [ ] **Aquatic crashed ships** — partially submerged hull rendering
- [ ] **Haunted wrecks** — optional creature spawning inside wrecks
- [x] **Landing Pad POI removed** — replaced with Tab → Ship tab → Board Ship action. Planet arrival spawns at deterministic center-adjacent tile. `Tile::OW_Landing` is a deprecated no-op.
- [x] **Board Ship action** — character screen Ship tab has an action row that warps the player into the Starship map from any planet overworld tile; disembarking restores the saved overworld position
- [x] **Cave Entrance POI** — three variants (natural cave / abandoned mine / ancient excavation) with cliff-embedded placement and lore-weighted selection
- [ ] **Cave entrance dungeon theming** — variant-specific dungeon generators beneath the portal
- [ ] **Additional cave variants** — flooded cave, sealed vault entrance, collapsed shaft
- [ ] **Ice cave variants** — frozen entrances with crystal fixtures in Ice biome
- [ ] **Beacon POI** — parked. Ground anchor for a Sgr A* lore beacon spire. Needs design work — scope, interaction, how it ties into star chart and lore tier progression. Existing `OW_Beacon` tile type and legacy detail-map stamp remain as placeholders.
- [ ] **Megastructure POI** — parked. Ground anchor for a precursor megastructure. Needs design work — scope, scale, how it ties into multi-tile terrain. Existing `OW_Megastructure` tile type and legacy detail-map stamp remain as placeholders.
- [ ] **Layered POI site selection** — replace the per-POI `PlacementScorer` pass with a planet-wide layered approach that picks candidate sites for all POI types up front (elevation + biome + lore + neighbor awareness), scored by suitability rather than per-generator constraints. Scheduled to land after the Phase 6 POI generators are complete.
- [ ] More overworld POI stamps (temples, factories, research labs)
- [ ] **Interactive shelves** — 3-tile shelf structures (║~║ or ═~═) against walls; middle tile holds lootable item (book, scroll, data crystal); picking up item reverts to empty shelf
- [ ] Derelict station expansion — more room types

### Items & Gear
- [ ] New weapon types
- [x] **Ship components and upgrades** — ShipSlot system, Mk1 components, cargo hold
- [ ] Consumables with effects (stims, shields, scanners)

### Starship
- [x] **Ship component system** — 6 equipment slots, Ship tab in character pane, install/uninstall
- [x] **Ship cargo hold** — separate inventory for ship components
- [x] **ARIA ship AI** — command terminal with context-aware dialog, ship systems access
- [x] **Tutorial quest "Getting Airborne"** — repair ship with 3 components, skip option
- [x] **Maintenance tunnels** — hub station dungeon with Engine Coil + Xytomorphs
- [x] **Observatory view-only star chart** — can browse but not travel
- [ ] Ship combat (space encounters)
- [ ] Ship weapons / utility slots
- [ ] **Ship scanner component** — installable utility that scans planets and systems. Base tier reveals total POI counts on planet info screen; upgraded tiers split visible vs uncharted (see layered POI budget) and eventually reveal hidden POI positions. System-level scans surface body summaries on the star chart. Ties into the Archaeology skill line (Beacon Sense / Ruin Reader augment scanner results).

---

## UI/UX

- [x] **Conversation dialog in contextual menu** — NPC speech shown as body text in PopupMenu
- [x] **Minimap** — half-block pixel minimap widget (F3), 3x3 downsampling, player-centered, all map types
- [x] **Ability bar** — display and activate learned abilities (keys 1-5)
- [x] **Message log scrollback** — scroll through message history
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
