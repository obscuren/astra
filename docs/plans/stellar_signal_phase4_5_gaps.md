# Stellar Signal — Phase 4 & 5 Gap Analysis

**Date:** April 17, 2026
**Source design doc:** `/Users/jeffrey/dev/Unreal/lyra/nova-arc-the-stellar-signal.md`
**Scope:** Feature requirements for implementing Stages 4 ("The Echo Chamber") and 5 ("The Long Way Home") of the main arc. Stages 1–3 already implemented.

---

## Summary of required features

### Stage 4: The Echo Chamber

- Conclave faction standing drop (-300) on first warp after Stage 3, triggering hostile status
- Transmission/comms UI delivery (incoming transmission message modal)
- Increased Archon Remnant spawning across the galaxy (activated defense mechanic)
- Ambush encounters at jump points controlled by Conclave (faction-driven combat encounters)
- Station siege state: Conclave fleet in orbit, station locked down, Nova confined to Observatory
- ARIA panicked ship-comms dialog (dialogue state tied to world event)
- Travel gauntlet: series of combat-heavy systems to reach The Heavens Above
- Io location with Conclave Archive (Precursor ruin, Io-specific variant, high-tier loot)
- Multi-level ruin exploration with procedural generation supporting deep dungeons

### Stage 5: The Long Way Home

- Stellari-resonance crystal as quest fixture with recorded audio message (Nova's final message)
- Three branching endpoints with persisted consequences:
  - Ending A (The Cycle): standard New Game+ rebirth, Nova loses current-cycle memory
  - Ending B (The Silence): galaxy simulation runs post-reset without cycle reset (unique ending state, no NG+)
  - Ending C (The Heavens Above): timed objective (race to HA before lockdown completes), extract Nova's core, unlock companion NPC
- Legendary Accessory "Nova's Signal" (`~` glyph, color 135, INT+3/WIL+2/Heat Resistance+10, passive: reveals one undiscovered system per 50 turns)
- Companion NPC system: Nova as persistent follower across runs (NG+ memory carryover)
- Meta-unlock layer: persistent lore entries (e.g., "The Last Cycle"), companion unlocks, legendary items carried to NG+
- Cross-run persistence system (New Game+ framework with player knowledge retention)
- Timed objective mechanic: countdown timer, failure condition when timer expires
- "Echo of Nova" legendary accessory for NG+ cycle after Ending A (similar to Nova's Signal)
- Lore entry unlock: "The Last Cycle" (Ending B-specific)

---

## Current coverage

### Quest system & arc chaining
**Files:** `include/astra/quest.h`, `include/astra/quest_graph.h`, `src/quest.cpp`, `src/quest_graph.cpp`, `src/quests/stellar_signal_hook/echoes/beacon.cpp`

Stages 1–3 are fully implemented. Quest DAG with prerequisites, story quest branching, and RevealPolicy (Hidden/TitleOnly/Full) all exist. The arc_id "stellar_signal" and quest chaining infrastructure is in place. Quest fixtures for planting drones and accessing the beacon are working.

### Faction standing & hostility
**Files:** `include/astra/faction.h`, `src/faction.cpp`, `src/game_interaction.cpp`

Faction system is fully featured: reputation tiers (Hated ≤ -300, Disliked, Neutral, Liked, Trusted), hostility checks via `is_hostile_to_player()` (reputation ≤ -300 threshold), and NPC attack-on-sight behavior for hostile factions. Stellari Conclave is defined. Shops currently close to hated factions (implicit; gating logic exists in character_screen for trade windows).

### NPC system & dialog
**Files:** `include/astra/npc.h`, `src/npcs/nova.cpp`, `include/astra/dialog_manager.h`, `src/dialog_manager.cpp`

Nova NPC exists with full dialog tree (8 nodes in current implementation). DialogManager handles branching dialog, quest turns, and story quest offer paths. NpcRole enum includes `Nova`, `ArchonSentinel`, `ArchonRemnant`, `VoidReaver` — all needed for Stage 4/5 encounters. Archon Sentinel miniboss fully configured: 50 hp, elite flag, Plasma damage, AV=10 (forces STR penetration).

### Dice combat system
**Files:** `include/astra/combat_system.h`, `include/astra/dice.h`, `src/game_combat.cpp`

Full dice combat implemented: attack rolls, defense values, armor class (AV), STR penetration (DV*2), type affinity (5-type system: Kinetic/Plasma/Cryo/EMP/Sonic), shields with affinities. Monsters scale to level. Stage 4/5 encounters (gauntlet, Conclave ambushes, Archive defenders) will leverage this system.

### Star chart & galaxy navigation
**Files:** `include/astra/star_chart.h`, `src/star_chart.cpp`, `src/star_chart_viewer.cpp`, `src/generators/body_presets.cpp`

Star systems, celestial bodies, star classes (M/K/G/F/A/B/O/Neutron), custom system insertion via `add_custom_system()`. Io exists in the chart. Navigation state persists with current_system_id, current_body_index. No existing mechanics for hidden-until-revealed systems (Stage 3 beacon is marked but always visible once the quest reaches it).

### Procedural ruin generation
**Files:** `src/generators/ruin_generator.cpp`, `include/astra/ruin_types.h`, `include/astra/ruin_decay.h`

Multi-room ruin generation with configurable civilization (Monolithic/Baroque/Crystal/Industrial), room types, furniture placement, loot tables. Supports map properties (lore tier, battle sites, infested, flooded). Legendary rarity loot exists. Single-level dungeons; multi-level support (Stage 4 Conclave Archive deep extraction) not yet implemented.

### Lore & journal system
**Files:** `include/astra/journal.h`, `src/journal.cpp`, `include/astra/lore_viewer.h`, `src/lore_viewer.cpp`

Journal entries per quest with categories (Blueprint, Discovery, Encounter, Event, Quest). Lore viewer displays narrative text. No multi-section audio-log playback or timed reveal pacing (needed for Stage 5 Nova's final message).

### Item definitions
**Files:** `include/astra/item_ids.h`, `include/astra/item_defs.h`, `src/item_defs.cpp`

Items 1–46 defined and built. "Nova's Signal" accessory is NOT yet defined (ID would be 47+). Ship components, accessories, and legendary rarity system exist; no legendary item with custom passive effect tied to quest reward yet.

### Galaxy simulation (post-cycle)
**Files:** `include/astra/galaxy_sim.h`, `src/galaxy_sim.cpp`, `include/astra/lore_types.h`

Full civilization state machine: traits, population, resources, knowledge, stability, military, weapon tech, age, territory, faction tension. Simulation advances via per-cycle ticks. No explicit "post-cycle without reset" mode (Ending B requirement).

### Save & load
**Files:** `include/astra/save_system.h`, `include/astra/save_file.h`, `src/save_file.cpp`, `src/save_system.cpp`

Binary save/load for player, world lore, quests, NPC state. No meta-layer for NG+ unlocks, cross-run knowledge, or persistent companion state. No branching save states (one save slot per playthrough).

---

## Gap analysis

### **Feature: Faction ambush events at jump points**

**Stage:** 4
**Status:** ✅ Done (2026-04-18, branch `feat/stage4-event-bus`)
**What shipped:** Implemented via the new EventBus (`include/astra/event_bus.h`) with a single `stage4_hostility` scenario in `src/scenarios/stage4_hostility.cpp`. Beacon quest completion drops Stellari Conclave standing by -300 and sets `stage4_active` world flag; `SystemEntered` event fires from `Game::travel_to_destination`; the scenario injects 1–3 Conclave Sentries (scaled by player level) via `inject_location_encounter`, with per-system dedup through `WorldManager::ambushed_systems_`.
**Touch points shipped:** `include/astra/event_bus.h`, `src/event_bus.cpp`, `include/astra/scenarios.h`, `src/scenarios/stage4_hostility.cpp`, `include/astra/scenario_effects.h`, `src/scenario_effects.cpp`, `src/quests/stellar_signal_beacon.cpp`, `src/game_world.cpp`, `src/npcs/conclave_sentry.cpp`.

---

### **Feature: Faction-space gating for Stage 4 ambushes**

**Stage:** 4
**Status:** ✅ Done (2026-04-20) — `controlling_faction` field on `StarSystem`, deterministic clustered assignment from galaxy seed around procedurally-placed capitals (Sol pinned to Terran), galaxy chart renders territorial bands, Stage 4 scenario filters ambushes to Conclave-owned systems.
**Touch points shipped:** `include/astra/faction_map.h`, `include/astra/faction_territory.h`, `src/generators/faction_territory.cpp`, `include/astra/star_chart.h`, `src/star_chart_viewer.cpp`, `src/scenarios/stage4_hostility.cpp`, `src/quests/stellar_signal_beacon.cpp`.

---

### **Feature: Incoming transmission UI (comms modal)**

**Stage:** 4
**Status:** ✅ Done (2026-04-18) — reused the existing `PlaybackViewer` `AudioLog` style instead of adding a new modal type. The `open_transmission` effect in `scenario_effects.cpp` wraps it. Upgrade to a dedicated comms modal later if the visual treatment needs to diverge from audio logs.

---

### **Feature: Station siege state & lockdown mechanics**

**Stage:** 4
**Status:** Missing
**What's needed:** World state flag marking The Heavens Above as under siege (Conclave fleet in orbit, power lockdown active). Affects: (1) Observatory access locked until later, (2) Nova confined to Observatory on the map, (3) ARIA dialog panicked tone on ship comms. On arrival at HA during siege, spawn combat encounter guarding Observatory entrance. Lockdown timer ticks down (for Stage 5 Ending C timed objective).
**Touch points:** `src/world_manager.cpp`, `src/generators/hub_station_generator.cpp`, `src/quests/stellar_signal_echoes.cpp` (set siege_active flag on Stage 4 start), `src/dialog_manager.cpp` (ARIA comms state), `src/game_interaction.cpp` (Observatory door lock check)
**Risk/Complexity:** M — state flag + conditional logic in multiple places.

---

### **Feature: Hidden system reveal on quest stage**

**Stage:** 4 (beacon system hidden until Stage 3)
**Status:** Partial — system exists but is always visible once quest marks it
**What's needed:** Mechanism to create a star system that does not appear on the star chart until a specific quest stage completes. At Stage 3 completion, beacon system becomes visible and marked as discovered. Currently Stage 3's hidden_system_id is added and marked discovered immediately; hide it from initial star chart list until the quest updates.
**Touch points:** `include/astra/star_chart.h` (hidden flag on StarSystem), `src/star_chart_viewer.cpp` (skip hidden systems in display), `src/quests/stellar_signal_beacon.cpp` (unhide on completion)
**Risk/Complexity:** S — star chart visibility flag + two-line changes.

---

### **Feature: Archon Remnants increased galaxy-wide spawning**

**Stage:** 4
**Status:** Missing
**What's needed:** On Stage 4 start, set a world flag (archon_alert_active). NPC spawner biases spawn pool toward Archon Remnants (higher weight) in non-station overworld locations. Thematic: Conclave is activating precursor defenses to stop the signal. Revert bias on Stage 5 start or quest completion.
**Touch points:** `src/npc_spawner.cpp` (spawn weight logic), `src/quests/stellar_signal_echoes.cpp` (set/unset flag)
**Risk/Complexity:** S — spawn bias is a weighted selection; flag toggle.

---

### **Feature: Legendary accessory "Nova's Signal"**

**Stage:** 3 (reward on completion)
**Status:** Missing
**What's needed:** Define item ID (e.g., 47), glyph `~` (color 135), stats INT+3/WIL+2/Heat Resistance+10, passive effect: reveals one undiscovered system per 50 turns (increments a turn counter, triggers on reach). Item flavor text references signal/memory/cycles.
**Touch points:** `include/astra/item_ids.h` (ITEM_NOVAS_SIGNAL = 47), `src/item_defs.cpp` (build_novas_signal function), `src/item.cpp` (passive effect on turn tick), `src/game.cpp` (on_world_tick: decrement counter, reveal system if ready)
**Risk/Complexity:** M — new item definition + turn-tick passive effect logic.

---

### **Feature: Conclave Archive ruin variant (Io)**

**Stage:** 4/5
**Status:** Missing
**What's needed:** Io-specific Precursor ruin with Conclave aesthetic (defensive architecture, sealed chambers, high-tier loot). Must support multi-level (2–3 levels deep for story pacing). Generator variant flag or civ_name parameter to ruin_generator distinguishing Archive from standard ruins (e.g., denser combat, treasure-guarded final chamber with Nova's crystal).
**Touch points:** `include/astra/ruin_types.h` (RuinVariant enum or properties), `src/generators/ruin_generator.cpp` (Archive-specific layout logic), `src/quests/stellar_signal_echoes.cpp` (spawn Archive when entering Io during Stage 4)
**Risk/Complexity:** L — multi-level dungeon generation, Conclave-themed fixtures and loot tables, placement of Nova's resonance crystal in deepest chamber.

---

### **Feature: Multi-level dungeon generation**

**Stage:** 4/5
**Status:** Missing
**What's needed:** Ruin generator and map system must support stacked levels (Level 1 → stairs down → Level 2 → stairs down → Level 3). Each level transitions via fixture (staircase). Currently single-level only. Loot/difficulty scale with depth. Stage 4 Archive is 3-level (bottom level holds Nova's crystal fixture).
**Touch points:** `include/astra/map_generator.h` (multi-level support), `src/map_generator.cpp` (staircase fixture handling, level transitions), `src/generators/ruin_generator.cpp` (per-level loot tables), `include/astra/world_manager.h` (track current_depth per location)
**Risk/Complexity:** XL — foundational map system change; affects navigation, save/load, monster spawning, POI generation.

---

### **Feature: Timed objective (countdown timer)**

**Stage:** 5 (Ending C only)
**Status:** Missing
**What's needed:** Quest objective that displays a countdown timer (e.g., 300 turns to reach HA). Timer ticks down each world tick. Failure: timer reaches 0 and quest fails (lockdown completes, Nova erased). Success: player reaches Observatory before timer expires and extracts Nova's core. Timer UI shown in quest panel + HUD. No existing timer mechanic; requires turn-counting in quest objectives.
**Touch points:** `include/astra/quest.h` (QuestObjective timer field), `src/game.cpp` (world tick decrements all active timers), `src/character_screen.cpp` (render timer in quest UI)
**Risk/Complexity:** M — quest objective extension + rendering.

---

### **Feature: Three branching quest endings with persisted state**

**Stage:** 5
**Status:** Missing
**What's needed:** At end of Conclave Archive, three mutually exclusive choices (Ending A/B/C), each sets a world state flag (ending_chosen = A/B/C). Flag persists in save file. Subsequent game logic branches:
  - Ending A: normal save/load, NG+ loop (cycle resets, player retains knowledge)
  - Ending B: unique ending state, no NG+ until new game restart
  - Ending C: Nova becomes saved companion NPC, unlocks on future runs

Currently no branching world outcomes or exclusive endings.
**Touch points:** `include/astra/world_manager.h` (ending_state field), `src/save_file.cpp` (persist ending_state), `src/game.cpp` (game-over epilogue rendering based on ending), `include/astra/quest.h` (choice outcome storage)
**Risk/Complexity:** L — branching logic touches save system, game-over screen, and NG+ loop.

---

### **Feature: Companion NPC system (Nova as follower)**

**Stage:** 5 (Ending C only; carries forward to NG+)
**Status:** Missing
**What's needed:** Companion framework: NPC that follows player, participates in combat (optional, dialog-driven choice), persists across playthroughs (saved as companion_state in meta-layer, not erased on cycle reset). Nova as first companion with unique dialog tree, memories of previous cycles, reactions to other quests. Requires:
  - Companion inventory / equipment slot (shared or separate)
  - AI pathfinding (follow player, maintain distance)
  - Dialog menu (companion interactions, camp banter, memories)
  - Combat participation (optional auto-attack or turn-skipping)
  - Death handling (can be knocked out, must rescue or restart)

This is a major feature; can be minimal MVP (Nova follows silently, no combat) or full-featured.
**Touch points:** `include/astra/companion.h` (new), `src/companion.cpp`, `include/astra/npc.h` (companion flag), `src/game.cpp` (companion pathfinding & rendering each tick), `src/dialog_manager.cpp` (companion dialog tree), `src/game_combat.cpp` (companion combat if enabled), `include/astra/save_file.h` (companion_state persistence)
**Risk/Complexity:** XL — new subsystem, cross-cutting (rendering, AI, combat, dialog, save/load).

---

### **Feature: Meta-unlock / cross-run persistence layer**

**Stage:** 5 (post-game)
**Status:** Missing
**What's needed:** Persistent storage outside of individual save files: unlocked legendary accessories, companion state, lore entries, achievements. Format: JSON or binary file in saves/ directory (_meta.json or similar). On NG+ start, load meta-unlocks:
  - Ending A grants "Echo of Nova" accessory on next run (INT+3/WIL+2, reveals system/50 turns, same as Nova's Signal)
  - Ending B unlocks "The Last Cycle" lore entry (permanent journal entry visible in new saves)
  - Ending C unlocks Nova as a companion (available as pre-game choice or at HA on first arrival)

Structure: `{ "unlocked_items": [...], "unlocked_companions": [...], "unlocked_lore": [...], "runs_completed": N }`
**Touch points:** `include/astra/save_system.h` (load_meta_unlocks, save_meta_unlocks), `src/save_system.cpp`, `include/astra/world_manager.h` (active_unlocks vector), `src/character_creation.cpp` (apply unlocks at game start), `src/journal.cpp` (inject lore entries)
**Risk/Complexity:** L — new save format, load/merge logic on startup, multiple places checking unlock state.

---

### **Feature: Stellari-resonance crystal fixture (audio log display)**

**Stage:** 5
**Status:** Missing
**What's needed:** Quest fixture in Conclave Archive (deepest level) that, when interacted, plays Nova's final message (multi-line text, line-by-line reveal with optional pause between lines). Should feel cinematic, distinct from normal quest text. Plays once per playthrough. After playback, player is presented with three choice buttons (Ending A/B/C).
**Touch points:** `include/astra/quest_fixture.h`, `src/quest_fixture.cpp`, `include/astra/lore_viewer.h` (repurpose or extend for multi-line paced playback), `src/dialog_manager.cpp` (final choice dialog post-playback)
**Risk/Complexity:** M — new fixture type with paced text reveal; choice handling (conditional quest completion).

---

### **Feature: New Game+ / cycle rebirth mechanics**

**Stage:** 5 (post-game, Ending A only)
**Status:** Missing
**What's needed:** When player reaches Sgr A* at the end (Ending A), trigger a New Game+ loop:
  1. Show epilogue screen: "The cycle resets. Knowledge survives. Nova does not — yet."
  2. Prompt: "New Game+?" → Yes/No
  3. If Yes: create new save with baseline character (Level 1, no equipment), load meta-unlocks (items/lore), reset world lore (new civilizations, same seed or fresh?), reset quests (story quests available again), preserve star chart (player knowledge of systems = revealed systems persist).
  4. If No: game-over / main menu.

No existing NG+ infrastructure. Requires save-to-NG+ logic, baseline character template, and world reset except for knowledge layer.
**Touch points:** `src/game.cpp` (Sgr A* arrival event), `src/save_system.cpp` (new NG+ save format), `include/astra/player.h` (ng_plus_run flag), `src/world_manager.cpp` (reset with knowledge carryover), `src/character_creation.cpp` (NG+ baseline build)
**Risk/Complexity:** XL — systemic change to save/load, character creation, world generation, quest reset.

---

### **Feature: Stellari-resonance beacon destruction (Ending B)**

**Stage:** 5
**Status:** Missing
**What's needed:** At Conclave Archive, Ending B choice lets player "destroy the beacon." This requires a high-stakes combat encounter (Conclave boss) or skill check (Archaeology/Technician), then beacon destruction animation/VFX. On success, sets ending_state = B and triggers unique ending epilogue (galaxy simulation runs forward without reset, civ-focused lore dump). On failure, revert to choice menu.
**Touch points:** `src/quests/stellar_signal_echoes.cpp` (Ending B choice triggers encounter/check), `src/game_combat.cpp` (Conclave boss fight), `src/game_rendering.cpp` (beacon destruction VFX), `src/lore_generator.cpp` (post-cycle simulation output for epilogue)
**Risk/Complexity:** L — boss encounter design, post-cycle lore generation, ending epilogue screen.

---

### **Feature: "The Last Cycle" lore entry (Ending B reward)**

**Stage:** 5
**Status:** Missing
**What's needed:** Unique lore journal entry unlocked only by Ending B. Content: narrative summary of what happens post-cycle (civilizations thrive/fall without protection, consequences unfold). Persisted in meta-layer. Visible in character screen journal + achievement notification.
**Touch points:** `include/astra/lore_types.h`, `src/journal.cpp`, `src/save_system.cpp` (load from meta on NG+)
**Risk/Complexity:** S — static text entry + meta persistence.

---

### **Feature: "Echo of Nova" legendary accessory (Ending A NG+ reward)**

**Stage:** 5
**Status:** Missing
**What's needed:** Second legendary accessory granted on first return to The Heavens Above in an NG+ run after Ending A. Identical stats to "Nova's Signal" (INT+3/WIL+2/Heat Resistance+10, reveals system/50 turns). Flavor text references memory/echo. Distinct from Nova's Signal for narrative (player chose to reset, this is the echo of the cycle that was).
**Touch points:** `include/astra/item_ids.h` (ITEM_ECHO_OF_NOVA), `src/item_defs.cpp`, `src/generators/hub_station_generator.cpp` (grant on HA entry if ng_plus_run && !received_echo_of_nova)
**Risk/Complexity:** S — new item definition + NG+ grant logic.

---

### **Feature: Nova's core extraction mechanic (Ending C success)**

**Stage:** 5
**Status:** Missing
**What's needed:** Ending C: if player reaches Observatory before lockdown timer expires, trigger "extract Nova's core" interaction with Observatory terminal (fixture). On success, Nova is saved as companion NPC, quest completes, cycle still resets but Nova carries forward. Mechanically: remove Nova from HA (no longer appears in Observatory on future runs), flag her as active_companion in meta-layer.
**Touch points:** `src/generators/hub_station_generator.cpp` (Observatory terminal fixture), `src/quests/stellar_signal_echoes.cpp` (quest on_complete: set companion_nova_saved in meta), `src/dialog_manager.cpp` (extraction success dialog), `src/npc_spawner.cpp` (skip Nova spawn if already extracted)
**Risk/Complexity:** M — condition + flag flow.

---

### **Feature: Io location / Conclave Archive quest marker**

**Stage:** 4/5
**Status:** Partial — Io exists on star chart, no Archive location
**What's needed:** Create Conclave Archive as a POI (Precursor ruin dungeon) spawnable on Io as the target location for Stage 4. Mark it on quest when Stage 4 starts. Flavor: hidden Precursor facility that Conclave discovered and thinks they control; Nova hid her fragment there long ago.
**Touch points:** `src/star_chart.cpp` (Io body list), `src/quests/stellar_signal_echoes.cpp` (quest objective: explore Conclave Archive, collect Nova's crystal)
**Risk/Complexity:** M — POI placement + ruin variant selection.

---

### **Feature: Scar-terrain biome for "The Fire-Worn" (Echo 1)**

**Stage:** 2 (already implemented quest-wise)
**Status:** Partial — scar terrain referenced in lore, not fully biome-generated
**What's needed:** Red dwarf + glassed/scarred planet biome. Visual: blackened ground, cracked glass-like tiles, minimal vegetation, ash/dust. Lore: ancient civilization nuked themselves here. Biome type enum addition or variant of existing biome with scarred stamp. Stage 2 Echo 1 currently uses make_scar_planet() which may be stubbed.
**Touch points:** `include/astra/biome_profile.h`, `src/generators/default_overworld_generator.cpp` (scar biome render), `src/body_presets.cpp` (make_scar_planet flesh out)
**Risk/Complexity:** M — biome variant + tile palette.

---

### **Feature: Neutron star system variant**

**Stage:** 2 (Echo 3: "The Edge")
**Status:** Partial — StarClass::Neutron exists, no special rendering
**What's needed:** Neutron star visual: small, dense, bright white/blue, pulsar beam VFX (rotating beam sweeping across screen). Star chart may show it differently (warning glyph?). Pulsing crystalline asteroid in orbit (Star::Neutron systems only, for Stage 2 Echo 3 fixture).
**Touch points:** `src/star_chart_viewer.cpp` (render neutron star unique), `src/generators/overworld_generator.cpp` (neutron star presence affects layout), `src/body_presets.cpp` (crystalline asteroid fixture)
**Risk/Complexity:** M — VFX + overworld layout bias.

---

### **Feature: Derelict station variant with quest flavor**

**Stage:** 2 (Echo 2: "The Quiet Shell")
**Status:** Partial — derelict station generator exists, not quest-specific
**What's needed:** Derelict station on gas giant with broken comms array as the primary interactive fixture. Generator should guarantee a communications room with a damaged comms array terminal. Stage 2 Echo 2 objective: plant drone on comms array.
**Touch points:** `src/generators/derelict_station_generator.cpp` (guarantee comms room), `src/quests/stellar_signal_echoes.cpp` (fixture targeting "stellar_signal_echo2" on the array)
**Risk/Complexity:** S — room type guarantee + fixture placement.

---

### **Feature: Archon Remnant spawn/scaling in Stage 4 gauntlet**

**Stage:** 4
**Status:** Partial — Archon Remnants exist as faction, no gauntlet scenario
**What's needed:** When Stage 4 quest is active and player is in-transit to HA, increased Archon Remnant encounters (1–3 per system visited). Combat encounters use Archon Remnant faction template. Scale to player level. Thematic: Conclave is weaponizing the precursor remnants.
**Touch points:** `src/game_interaction.cpp` (spawn encounter on enter system during stage4_active), `src/npcs/archon_remnant.cpp` (scaling logic)
**Risk/Complexity:** M — conditional spawn based on quest state.

---

### **Feature: ARIA ship-comms panicked dialog state**

**Stage:** 4 (during siege)
**Status:** Partial — ARIA exists, dialog is static
**What's needed:** ARIA's ship comms dialog changes tone when stage4_active (siege state). Instead of "All systems operational," she's panicked: "Commander, we've got Conclave weapons tracking us. The Heavens Above is under attack. We need to get Nova out of there!" Requires dialog state tied to world flag.
**Touch points:** `src/dialog_manager.cpp` (ARIA dialog tree), `src/quests/stellar_signal_echoes.cpp` (set siege_active on Stage 4 start)
**Risk/Complexity:** S — conditional dialog text.

---

### **Feature: Gallery/epilogue screens for Endings B & C**

**Stage:** 5
**Status:** Missing
**What's needed:** End-game screens showing outcomes:
  - Ending B: "The Silence" — post-cycle epilogue text or image, galaxy thrives/falls, "The cycle is broken."
  - Ending C: "The Heavens Above" — Nova and player together, "Nova is free. The cycle continues without her."

Both should feel cinematic, allow save-to-credits or return-to-main-menu.
**Touch points:** `src/game_rendering.cpp` (new ending_screen state), `src/game.cpp` (game loop on ending sequence)
**Risk/Complexity:** M — UI screen + simple narrative text/image.

---

## Suggested implementation order

1. **Faction ambush events** — Quick win; enables Stage 4 combat encounters without requiring full siege state.
2. **Incoming transmission UI** — Minimal UI addition; sets up Stage 4 narrative beat.
3. **Archon Remnants increased spawning** — One-line spawn bias; thematic feedback for Stage 4 world state.
4. **Station siege state & lockdown mechanics** — Foundational for Stage 4 narrative; unblocks Observatory access logic.
5. **Hidden system reveal** — Star chart flag; enables Stage 3 beacon mechanics to feel hidden.
6. **Conclave Archive ruin variant** — Io-specific ruin; can start as single-level, extend to multi-level later.
7. **Scar-terrain & Neutron biome variants** — Stage 2 flavor (already in-quest; improves immersion).
8. **Derelict station with comms room guarantee** — Stage 2 flavor.
9. **Legendary accessory "Nova's Signal"** — Item definition + turn-tick passive; testable in isolation.
10. **Timed objective mechanic** — Quest system extension; prerequisite for Ending C.
11. **Three branching quest endings** — Core Stage 5; combines choice UI + world-state branching.
12. **Stellari-resonance crystal fixture** — Stage 5 narrative anchor; relies on multi-line text display.
13. **Ending B: beacon destruction encounter** — Boss fight + ending logic.
14. **Ending C: core extraction & lockdown timer** — Timed objective + companion unlock.
15. **Multi-level dungeon generation** — XL effort; required for deep Archive exploration. Start after Stage 4 core is working.
16. **Companion NPC system** — XL effort; foundational for Ending C carry-over; requires significant AI/combat/dialog work. Can be MVP (silent follower) or full-featured.
17. **Meta-unlock / cross-run persistence** — Unblocks NG+ and meta-rewards; build after Companion if doing full version.
18. **New Game+ mechanics** — Final integration; save/load + world reset + knowledge persistence.
19. **"Echo of Nova" & "The Last Cycle" unlocks** — Reward items/lore for NG+ runs.
20. **Ending epilogues (B & C)** — Narrative screens; polish pass.

---

## Open questions

1. **Timed objective duration** — How many turns (world ticks) for the lockdown timer in Ending C? (Suggest: 300–500 turns = ~30–50 min at normal speed, enough for gauntlet + Archive traversal.)
2. **Conclave boss identity** — Is the beacon destruction guardian a new NPC (Conclave Commander), or does Nova herself manifest as a test (thematic but dark)?
3. **Multi-level dungeon scope** — Archive: 2 levels (short) or 3+ (full descent)? Does each level increase in difficulty/loot?
4. **Companion MVP vs. full** — For Ending C, is Nova a silent follower with no combat, or full companion with dialog, combat AI, and equipment slots?
5. **NG+ baseline character** — Do players start Level 1 and rebuild, or Level X (current level - 10 cap)? Do they keep unlocked skills/recipes?
6. **Galaxy sim post-cycle output** — For Ending B epilogue, how is post-cycle civilization evolution rendered? Text narrative, slide show, or procedural text dump?
7. **Companion memory flavor** — Does Nova (Ending C carry-over) remember only the Ending C run, or all prior runs? How is memory wipe on non-Ending-C runs explained narratively?
8. **Stellari echo in NG+ after Ending B or C** — If player breaks the cycle (Ending B) or saves Nova (Ending C), does a new Stellari echo appear in the next NG+, or is Nova the only one?
9. **Ship-to-ship combat** — Stage 4 mentions a "Conclave fleet in orbit." Is the siege a ground-based Observatory encounter, or does ship combat factor in? (Suggest: flavor only; no ship combat mechanics needed for this arc.)
10. **Fortress/station upgrade after Ending C** — If Nova is free and no longer the signal-keeper, does The Heavens Above play a different role in future content? (Out of scope for Phase 4/5, but good hook for future arcs.)

---

This gap analysis catalogs 23 major features and subsystems. Stages 1–3 are feature-complete. Stages 4–5 require infrastructure (multi-level dungeons, companion system, NG+ loop, meta-persistence) alongside narrative content (encounters, endings, unlocks). The suggested order balances quick narrative wins early with foundational systems mid-effort, leaving the largest architectural changes (companions, NG+, multi-level) for final phases.
