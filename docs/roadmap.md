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
- [x] **Tumble** — active 3-tile telegraphed dash (Line telegraph, Agility 17), 25-tick cooldown

### Wayfinding Skills
- [x] **Scout's Eye** — show NPCs on minimap (75 SP, INT 13)
- [x] **Cartographer** — show items and POIs on minimap (100 SP, INT 14)
- [x] **Camp Making** — place a campfire, 150-tick lifetime, Cozy aura (2× natural regen) within 6 tiles (50 SP, INT 12)

### Acrobatics Skills
- [x] **Category passive** — +1 DV always-on while Cat_Acrobatics is learned
- [x] **Swiftness** — +5 DV vs ranged attacks (passive, 50 SP)
- [x] **Sidestep** — +2 DV while a hostile is adjacent (passive, 75 SP, AGI 13)
- [x] **Sure-Footed** — dungeon move cost −10% (passive, 75 SP, AGI 15)
- [x] **Adrenaline Rush** — self-cast, +2 DV and +25% quickness for 3 ticks, 40-tick cooldown (active, 150 SP, WIL 14)

### Archaeology Skills
- [x] **Archaeology skill category** — Ruin Reader, Artifact ID, Excavation, Cultural Attunement, Precursor Linguist, Beacon Sense

### NPC Combat
- [x] **Ranged NPC attacks** — Turret AI holds position and fires at player/NPCs with LOS; Sentry Drone fires plasma bolts at 6 tiles
- [x] **Flee behavior** — Intimidate causes NPCs to flee (move away from player)

---

## Crafting & Tinkering

- [ ] **More synthesis recipes** — expand beyond the initial 10
- [ ] **Blueprint discovery flow** — find blueprints in dungeons, learn from analyzing loot
- [x] **Repair bench fixture** — pay credits to restore durability, no skill required
- [x] **Tinkering salvage system** — Spare Parts / Circuitry items, three mechanical enemies (Rust Hound, Sentry Drone, Archon Automaton), `Cat_Tinkering`-gated auto-salvage on mechanical kills, 5% ungated ground-drop on other kills (2026-04-21)
- [x] **Tinkering expansion** — 24-material catalog across 3 tiers, junk dual-use + 9 refinement recipes, hand-tuned synthesis costs, schematic-based crafting for 16 inert consumables (stims/grenades/mines), tier-weighted salvage, `docs/tinkering.md` reference (2026-04-27)
- [ ] **Consumable use code** — wire throw / inject / detonate for the 16 stims/grenades/mines crafted via schematics
  - [x] **Mines** (v50) — Thrown-slot equip + `[T]` hotkey, `TelegraphShape::Burst` aiming with stylized tether and `░` footprint, generalized `Trap` framework on `WorldManager.traps_`, live faction-eval triggers, placer-immune splash, detection roll keyed off `Player.trap_detection`, Decoy noise events, dungeon-trap API + `spawn-trap` dev command. Reuses also benefit Tumble (now uses Burst telegraph)
  - [x] **Grenades** (v50) — same Thrown-slot + `[T]` flow, immediate detonation on impact, blast radius shown directly in the telegraph footprint, AoE damage + status (Frag/EMP/Cryo/Incendiary fully wired). Cryo schematic added (was missing in tinkering expansion)
  - [ ] **Stims** — inject path still inert (needs HP restore + status grant per `DishOutput`-style data). Healing Stim, Endure, Focus, Berserker, Medkit
  - [x] **Smoke Grenade** (v51) — generic per-tile `GroundEffect` registry on `WorldManager.ground_effects_` (mirrors `traps_` / `noise_events_`), 5×5 Bresenham wall-LOS-clipped stamp on detonation, per-ring TTL falloff (24/18/12 — outer ring dissipates first), `OpacityProbe` extends FOV opacity without coupling `TileMap` to game state, lit-region reveal post-pass also respects smoke LOS (otherwise fully-lit rooms see through it), gray block-glyph render (`▓`/`▒`/`░`) with per-world-tick churn animation. Reusable for future Acid / Ice / Fire ground effects (additive — no save schema bump needed) (2026-04-29)
  - [ ] **Flashbang** — currently applies EMP-Disabled as placeholder; needs a true Stun effect (blocks turns / actions, not just energy weapons). New `EffectId::Stunned` + action-cost gate
- [ ] **Trap detection skill scaling** — `Player.trap_detection` is currently a flat int. Needs INT-derived bonus, a Perception/Awareness skill, and equipment/buff modifiers
- [ ] **AI trap pathfinding** — hostile NPCs walk into both hidden and visible traps in v50; revisit once a Perception layer exists so high-tier enemies can spot and avoid

### Cooking
- [x] **Cooking system v1** — `Cat_Cooking` skill category, three-slot pot UI in a new character tab, aura-gated via `CookingFireAura` on any `FixtureTag::CookingSource` fixture, 6 recipes (3 Basic starters + 3 cookbook-obtained), 5 stackable ingredients, `DishOutput` driving hunger/HP/GE on consumption, Burnt Slop on experiment miss, `AdvancedFireMaking` sub-skill cuts Camp Making cooldown 40% (2026-04-23)
- [ ] **Cooking v2 — skill-driven failure and raw edibility** — Cooking sub-skills gate recipe complexity and reduce burn chance; survival path for eating raw ingredients

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
- [x] **Story quest chains** — hand-tailored multi-quest DAGs (A → B → {C, D} → E), prerequisite system, NPC-offer / auto-accept modes, reveal policies, failure cascade. Demo: Hauler Arc fan-out (2026-04-14)
- [ ] **World-driven random quests** — generate quests from actual world state instead of hardcoded arrays
- [ ] **Quest failure mechanics** — expiration, consequences, reputation loss

### The Stellar Signal (main arc)
- [x] **Stage 1 — Static in the Dark** — hook quest, Nova NPC, signal reveal dialog
- [x] **Stage 2 — Three Echoes** — three-system exploration, receiver drones, per-system Fragment audio logs
- [x] **Stage 3 — The Beacon** — hidden beacon system, multi-line audio log reveal
- [x] **Stage 4 infra — EventBus + scenario graph** — in-process typed event bus, effect primitives, scenario registration (2026-04-18, see `docs/plans/scenario_graph_vision.md`)
- [x] **Stage 4 — Conclave hostility & ambushes** — -300 rep drop on Stage 3 completion, one-shot transmission, per-system Conclave Sentry ambushes
- [x] **System faction ownership** — controlling_faction per system, deterministic clustered generation, galaxy-view band rendering with `F` toggle, Stage 4 ambushes gated to Conclave space (2026-04-20)
- [x] **Stage 4 — Station siege & lockdown** — THA unlandable during siege arc; Return quest completes on Sol arrival (ARIA panic + Siege popup); THA traffic control plays automated docking denial; flag cleared on Siege completion (2026-04-21)
- [x] **Stage 4 — Conclave Archive (Io)** — 3-level Precursor ruin on Io with reusable multi-level DungeonRecipe generator, Archon Sentinel boss on the deepest level, Nova's resonance crystal fixture, Siege completion clears THA lockdown (2026-04-21)
- [ ] **Stage 5 — The Long Way Home** — three branching endings, timed objective, Nova core extraction
- [ ] **Nova companion NPC** — follower unlocked by Ending C, cross-run persistence
- [ ] **New Game+ loop** — Ending A cycle-reset, knowledge carryover, meta-unlocks

### Gameplay
- [x] **Auto-walk/explore** — `w` + direction = walk straight, `ww` = BFS auto-explore
- [x] **Space station types** — Normal/Scav/Pirate/Abandoned/Infested with unique per-station keepers and specialty rooms (see docs/superpowers/specs/2026-04-13-space-station-types-design.md)

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
- [x] Layered dungeon generator pipeline (6 layers: backdrop, layout, connectivity, overlay, decoration, fixtures)
- [x] **Archive Dungeon Migration** — `StyleId::PrecursorRuin` + `LayoutKind::PrecursorVault` (per-depth authored topologies: L1 fractured outer ruin, L2 nave+chapels, L3 antechamber→approach→vault); pipeline layer 6.iii style-required fixtures (Plinth/Altar/Inscription/Pillar/ResonancePillar/Brazier × SanctumCenter/ChapelCenter/EachRoomOnce/WallAttached/FlankPair); `precursor_vault` decoration pack; `required_plinth` quest-fixture hint. `old_impl::` legacy body deleted; `SAVE_FILE_VERSION` bumped 38→39 with no backcompat (2026-04-22)
- [x] **Dungeon Puzzle Framework** — pipeline layer 7 (`apply_puzzles`) dispatches on `DungeonStyle::required_puzzles`. First kind: `SealedStairsDown` on Archive L1 — seals terminal-room doorway with `StructuralWall`, places wall-attached `PrecursorButton` (gold `◘`) outside entry+sanctum. Pressing the button unseals and swaps stairs to `StairsDownPrecursor` (Nova violet). Generic fixture proximity triggers (`FixtureData::proximity_radius`/`proximity_message`) emit flavor lines when the player enters a fixture's Chebyshev radius. `:solve` dev command + `dumpmap` puzzle output. `SAVE_FILE_VERSION` bumped 40→41 (2026-04-22)
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
- [x] **Layered POI site selection** — deterministic per-planet `PoiBudget` drives a unified placement pass that expands the budget into prioritised `PoiRequest`s, scores candidate sites against terrain requirements, and writes anchor hints (cliff/water adjacency with direction) for stage-2 generators. Kills the cave-entrance PlacementScorer bypass for variant selection. Design + plan in `docs/superpowers/specs/2026-04-11-poi-budget-and-hidden-ruins-design.md` and `docs/superpowers/plans/2026-04-11-poi-budget-and-hidden-ruins.md`.
- [x] **Hidden ruin discovery** — subset of ruins rolled hidden at budget time, render as underlying biome until stepped on, then log to the Journal (`JournalCategory::Discovery`) with a live overworld preview. Discovery counts feed the Scanner Report on the star chart planet info panel and will extend to the ship scanner component later.
- [ ] More overworld POI stamps (temples, factories, research labs)
- [ ] **Interactive shelves** — 3-tile shelf structures (║~║ or ═~═) against walls; middle tile holds lootable item (book, scroll, data crystal); picking up item reverts to empty shelf
- [ ] Derelict station expansion — more room types

### Items & Gear
- [ ] New weapon types
- [x] **Ship components and upgrades** — ShipSlot system, Mk1 components, cargo hold
- [ ] Consumables with effects (stims, shields, scanners)
- [x] **Energy system** — persistent multi-tier cells, Solar Panel tinkering mod, normalized recharge for weapons and shields

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
- [x] **Ability bar rows** — 3-row paged hotbar, persistent slot assignments on Player, auto-assign on learn, compact on remove, PgUp/PgDn paging with wrap
- [x] **Message log scrollback** — scroll through message history
- [x] **Quest tab categorization** — Main Missions / Contracts / Bounties / Completed; arcs rendered under Main with active, locked (title/hidden), and completed steps inline
- [ ] **Item comparison** — show stat diff when hovering equipment
- [ ] **Character panel tab memory** — remember the last active tab; reopening shows the same tab
- [x] **Equipment tab read-only** — side panel equipment is display-only
- [x] **Remove inventory from side panel** — inventory management via character screen only
- [x] **PDA refactor** — `character_screen` renamed to `pda_screen`; per-tab modules (one `pda_<tab>_tab.cpp` per tab); new (placeholder) Hacking tab. Foundation for the Hacking & The Grid feature spec'd in `docs/superpowers/specs/2026-04-29-hacking-design.md`.
- [x] **Hacking & The Grid — Plan 2 (Cyberdeck + Quickhacks)** — `Cat_Hacking` skill category (8 unlocks declared, gating only). Cyberdeck item type with 2 tiers (Pidgin Mk I, Polyglot DCK-2) + `EquipSlot::Cyberdeck`. Program item type, 8 starter programs (3 active QH: reboot_optics, friendly_fire, data_leech). Code fragment crafting material + 3 tinker recipes. `Hackable` trait on fixtures + NPCs. `HackingSystem` coordinator. `H` keybind opens quickhack target cursor with cyan crosshair + program picker. PDA Hacking tab — locked splash + bounded terminal (10 commands). Hackable fixture interaction menu with Jack In stub for Precursor consoles. Detection counter with threshold-based reputation coupling at 50/75/100. Save schema bumped 51 → 52.
- [x] **Hacking & The Grid — Plan 3 (A-layer / The Grid)** — full jack-in lifecycle. New `GameState::Grid` with its own input + render path. `GridSession` runtime owned by `HackingSystem` carries avatar HP/RAM/Trace + cached skill flags. Per-galaxy `GridNetwork` graph (subnet / regional darknet / deep-Grid anchor) registered on Precursor console placement and persisted via save schema v54. Tron-styled tilemap render with UTF-8 glyphs (`░ ▓ ▼ ◇ ▲ ⌬ ⊙ ⊘`) + HUD bars. Procedural subnet/regional sector generators + one hand-authored Consciousness Anchor sector. ICE actors: white patrols and raises Trace, gray pursues for 1 HP, black bleeds real HP through to the body. Heat → Trace coupling, forced reboot at heat_cap, breakpoints (50 alert, 75 gray reinforcement, 100 black ICE summon). All five `.exe` programs live (icebreaker_lite, ghost_trace, cooldown, breach, decrypt). Six `Cat_Hacking` skills wired (Intrusion, IceBreaking, DaemonMastery, GhostProtocol, DeepGridNavigator, NeuralFortitude). Voluntary, hard, non-black-death, and black-ice-death disconnect outcomes; soft disconnect on save/load. PDA Hacking tab `netmap` and `jack -t <node>` go real. Dev verbs: `:jack`, `:jack-out`, `:trace`, `:spawn-ice`. Save schema v53 → v54.
- [x] **Hacking & The Grid — Plan 4 (D-layer / Deep-Grid persistence)** — second save scope `consciousness.dat` carries identity across Sgr A* rebirths. New `Implant` equipment slot + Neural Backup item with -1 WIL modifier; Equipment-tab `Tab` toggle reveals an implant paper-doll. Per-`Hackable` lore fragments + `soul_mirror_progress`. Soul Mirror channel (non-hacker access path) — adjacent-to-Precursor channelling at 1 EP/turn that auto-syncs lore on commit; HUD strip shows progress. `CodeCraft` + two T3 programs (`pulse_hammer.exe`, `daemon_hijack.exe`) with tinker recipes. `ConsciousnessAnchor` capstone seeds a 30×20 player deep-Grid base persisted in `consciousness.dat`; one-time event, owned-by-consciousness wired into `GridNetwork` graph. Grid renderer follows the avatar with a 4-cell deadzone. Regional darknet BSP generator (40×24, 4–8 firewall-bordered rooms with floor doorways). `GridNetmapWidget` overlay replaces scrollback netmap — regional + deep-Grid zooms, cursor-stepping, deferred jack-in, locked-edge styling. Sgr A* `RebirthSequence` — confirmation modal listing what survives + first-crossing cinematic + `consciousness.dat` increment + return-to-MainMenu. Dev verbs: `:unlock-anchor`, `:rebirth`, `:rebirth-reset`, `:unequip-implant`. Save schema v55 → v58.

- [x] **Hacking & The Grid — Plan 5 (Grid expansion and LAN redesign)** (2026-05-02)
  - Tag-driven capability model (HackTagMask bitmask, tag-set filtering retires `Hackable::available_qh`)
  - LAN auto-registration on every map enter (Precursor console / station / ship roots, child fixture subnets)
  - Procedural LAN sector generator (firewall rings, office floor plan, connector wiring, BSP or hand-shaped layouts)
  - Subnet gateways (`⌬`) and deep-Grid gateway (`⊕`) intra-sector navigation
  - Per-subnet device avatars (fixture-specific icons on sector walls)
  - Jack-out node (`⊙`) voluntary exit with lore unlock
  - Tile-mutation persistence (cracked firewalls, looted DataNodes, decrypted EncryptedFiles, killed ICE)
  - 60×40 hand-authored deep-Grid base (Anchor v1 + Atlas + Frontier placeholder)
  - WarpAnchor population on first `⊕` crack; past-galaxy memorial flagging
  - Galaxy reseed via Game::start_new_galaxy
  - nmap/ping/jack/lore IP-driven PDA verbs
  - NPC implants carry `Electronic | Mobile` tags, auto-register, targetable by filtered quickhacks
  - Save schema v60 + consciousness.dat v2

### Deferred / next

- [x] **Hacking & The Grid — Plan 6 (UI / Grid HUD redesign)** (2026-05-03) — Tron-styled 70%×70% overlay window with world+UI rendered monochrome behind. Top status (▶ GRID + breadcrumb + IP + tier-coloured Trace gauge), deck strip (HP/RAM/Heat block-bars), playfield (re-anchored sector + Telegraph preview), right-pane message log (F1, word-wrapped, prefix-aware continuation indent), program bar (1–8 fire, abbrev + dim-when-unaffordable + active-slot inverse-video). Number keys 1–8 fire programs through Telegraph; per-program TargetingMode/TelegraphSpec/valid_target predicate. UTF-8-aware text helper. GridSession log isolated from world log; Convulsing GE bumped to AV-5 / QN-50 with jack-in lockout; player quickness now wired into advance_world's NPC energy scaling. Dev `:mono on/off` toggle for the monochrome filter.
- [ ] **Hacking & The Grid — Plan 7+ (multi-track)** — see `docs/plans/2026-05-03-plan-7-roadmap.md` for the full sub-project map. Summary:
  - **Plan 7 — Device Shells** *(done on `feature/device-shells`, awaiting playtest + merge)*. Per-`docs/superpowers/specs/2026-05-01-device-shells-design.md`. Diegetic CLI shells for every electronic fixture; tag-driven command set; guest/root auth; long-channels; two doorways (Shell Access / in-Grid ssh-adjacent); cyberdeck mod gate for `jack <ip>`.
    - [x] **Phase A** — Hackable schema (firmware_state, cracked_digits, escalated, dumped_bytes; save v62→v63). HackCommand + Registry + HackChannel + DeviceShell skeletons. Civilian flavor + FixtureOsId table. Universals (`help`, `whoami`, `clear`, `history`, `exit`, `<cmd> --help`). `pda> ssh [<user>@]<ip>` with strict manual semantics. `(hack) Shell Access` interactable on Electronic Hackables (autoruns smart-ssh). Real-world wired-in body state.
    - [x] **Phase B** — Full Locked/HasOptics/Weaponized/PowerNode/Mobile/DataStore command sets. `hashcat` + `dump` partial-state. Corp + Cartel flavor packs. Civilian fallback rule. DeviceFsView + permission gate. Save schema v63→v64.
    - [x] **Phase C** — In-Grid Tron-window shell takeover (HUD chrome stays visible). NPC implant Shell Access (hostile-cyber-NPC adjacency dialog). AlienTech opt-out for manual ssh + `nmap -l` `OS: ??? (unknown)`. CyberdeckMods + `jack <ip>` mod gate. Aerojack + Untether items. ColdHands + RootKit skills wired into `scaled_cost`. Plan 5 amendments (drop `nmap -m b`, refine `nmap -l` tier/lock column).
  - [x] **Plan 8 — Grid Layout / Generator changes** *(shipped on `feature/plan-8-grid-layout`)*. Per [`docs/superpowers/specs/2026-05-04-plan-8-grid-layout-design.md`](superpowers/specs/2026-05-04-plan-8-grid-layout-design.md). Flat sector per LAN (subnet sectors retired); packed independently-walled rooms; tag-themed templates; T1/T2/T3 zone clusters with locked-door chokes; door-only `breach.exe`; zone HUD overlay (dashed perimeters + `— LOBBY —` banners). Foundation Plan 9 reads against.
  - **Plan 9 — Your.Anchor v2 + AI contacts.** Stash, customisation, AI characters as Grid-side dialogs (NOT shell endpoints — uses `DialogManager` rendered Tron-themed). Depends on Plan 8.
  - **Plan 10 — Galaxy Survival.** Sgr A* in-world warp trigger, WarpAnchor traversal, `galaxy_id` save round-trip, past-galaxy memorial surface.
  - **Plan 11 — Polish.** Frontier zone content, real-body damage decision, NPC death tombstones, residual hostile-QH model.

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
- [ ] **Unique-equipped mechanic** — flag certain item types so equipping a second copy auto-swaps with the existing one rather than allowing duplicate equips. Currently special-cased in the cyberdeck equip path; generalize across utility slots and any future "one-of-a-kind" gear.
