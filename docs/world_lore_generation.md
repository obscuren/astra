# Procedural World Lore Generation

**Date:** 2026-04-04  
**Status:** Design brief — no implementation yet

## Vision

Every playthrough of Astra generates billions of years of galactic history before the player takes their first step. Civilizations rise, build, war, collapse, and leave their mark on the galaxy. The ruins the player explores, the artifacts they find, the systems they traverse — all shaped by a deep procedural history that points inexorably toward Sagittarius A*.

The player isn't just exploring a random galaxy. They're following breadcrumbs laid across eons by civilizations that all discovered the same truth: everything leads to the black hole at the center.

---

## Core Principles

1. **Lore first, galaxy second** — history is generated before the star chart. Key locations, wars, empires, and collapses define which systems matter. The galaxy generator places these into the physical map and fills in the rest.
2. **Single binary** — all generation is compiled in. No external data files. Phoneme pools, cultural templates, and event patterns are code.
3. **Layered deep time** — minimum 2 precursor civilizations, potentially up to 5+. Each leaves distinct fingerprints. Humanity is the latest layer, building on top of ruins upon ruins.
4. **Sgr A* is the convergence** — every civilization eventually discovers that the black hole is significant. Some reach it and vanish. Some build infrastructure pointing toward it. The galaxy is a puzzle box assembled across billions of years, and the cycle of rebirth upon entry is the deepest secret of the universe.

---

## Historical Structure

### Epochs

The generator produces a sequence of epochs. Each epoch has a dominant civilization, defining events, and a legacy. The minimum structure:

```
Epoch 0: Primordial Genesis (billions of years ago)
  - First intelligence emerges
  - Megastructure construction
  - Discovery of Sgr A* significance
  - Legacy: foundational technology, deepest ruins

[Silence — millions of years]

Epoch 1..N: Successor Civilizations (randomized count, min 1)
  - Discover Primordial ruins
  - Build upon or misuse ancient tech
  - Rise, peak, collapse
  - Each adds a layer to the path toward Sgr A*
  - Legacy: their own ruins, tech, cultural artifacts

[Silence — variable duration]

Epoch N+1: Humanity Arrives (thousands of years ago)
  - Discovers layered ruins of multiple civilizations
  - Reverse-engineers hyperspace technology
  - Golden age of expansion
  - The Fracture — civilization fragments
  - Present day: scattered factions, lost knowledge
```

### Randomized Elements Per Epoch

- **Civilization count:** 2-5 precursor civilizations (min 2: Primordials + at least one successor)
- **Duration:** each epoch spans randomized millions/billions of years
- **Rise pattern:** gradual expansion, sudden emergence, or awakening from dormancy
- **Peak achievement:** what they built at their height (Dyson structures, wormhole networks, living ships, etc.)
- **Collapse cause:** war, transcendence, plague, resource exhaustion, Sgr A* obsession, unknown
- **Relationship to predecessors:** did they revere, exploit, fear, or ignore the ruins they found?
- **Relationship to Sgr A*:** did they reach it? Try and fail? Build toward it? Flee from it?

---

## Civilization Generation

### Identity

Each civilization gets a procedurally generated identity composed from curated pools:

**Name generation** — phoneme-based with curated syllable banks:
- Root syllable pools tuned for distinct linguistic feels (harsh/guttural, flowing/vowel-heavy, clicking/staccato, humming/resonant)
- Each civilization rolls a primary phoneme pool, giving it a consistent sound
- Names for the civilization, key figures, locations, and artifacts all draw from the same pool
- Structure templates: "The [name] [polity]", "[name]i [descriptor]", "The [adjective] [name]"

Example pools (illustrative, not final):
```
Pool A (crystalline/sharp): Keth, Vor, Thex, Zan, Cyr, Qal
Pool B (flowing/ancient):   Ael, Vyn, Osa, Leri, Ithae, Myr  
Pool C (guttural/deep):     Groth, Durr, Khar, Mog, Zhul, Brek
Pool D (ethereal/light):    Phi, Sei, Lua, Wen, Tia, Nev
```

A civilization drawing from Pool B might produce: "The Aelithae Convergence", Archon Vynosar, the Spire of Lerimund.

**Cultural aesthetic** — composed from trait pools:
- **Architecture:** crystalline, organic/grown, geometric, void-carved, light-woven
- **Technology:** gravitational, bio-mechanical, quantum-lattice, harmonic-resonance, phase-shifting
- **Philosophy:** expansionist, contemplative, predatory, symbiotic, transcendent
- **Art form:** sound-sculpture, light-memory, gene-poetry, void-mapping, time-carving

These traits determine how their ruins look, how their artifacts behave, and how NPCs describe them.

### Tech Signature

Each civilization has a distinct technology flavor that manifests in:
- **Dungeon aesthetics** — their ruins have unique tile themes, fixture types, color palettes
- **Artifact properties** — items from their era have characteristic bonuses, materials, visual style
- **Station architecture** — stations they built feel different from stations built by other eras

---

## Key Events

The simulation generates a timeline of events. Events are drawn from templates and filled with civilization-specific details.

### Event Types

**Emergence Events:**
- First consciousness / awakening
- Discovery of predecessors' ruins
- Decipherment of ancient knowledge
- Reverse-engineering of precursor technology

**Expansion Events:**
- Colonization of first system beyond homeworld
- Establishment of hyperspace routes
- Construction of megastructures (gates, beacons, stations)
- First contact with another active civilization (if epochs overlap)

**Conflict Events:**
- Civil war / schism (faction splits)
- Resource wars over precursor artifacts
- Border conflicts between contemporaneous civilizations
- The Artifact Wars (fighting over Sgr A*-related technology)

**Discovery Events:**
- First detection of Sgr A* anomaly
- Discovery of the convergence pattern (all paths lead to center)
- Breakthrough in understanding precursor intent
- Creation of key artifacts (galaxy-shaping items)

**Collapse Events:**
- Transcendence (voluntarily left physical form)
- Self-destruction (weapon or experiment gone wrong)
- Consumption (absorbed by Sgr A* or something from it)
- Fragmentation (scattered, devolved, forgotten)
- Unknown (mystery — most compelling for gameplay)

**Legacy Events:**
- Sealing of vaults / knowledge caches
- Creation of guardian systems (automated defenses still active)
- Scattering of key artifacts across the galaxy
- Encoding of warnings or invitations in ruins

### Event Chains

Events can trigger follow-up events across epochs:
- Civilization A seals a vault → Civilization B discovers and opens it → consequences
- Civilization A builds a beacon network toward Sgr A* → Civilization B extends it → Humanity discovers the combined path
- A collapse event in one epoch creates the asteroid field a later civilization mines

---

## Key Figures

Each civilization generates 3-8 notable figures. Figures are referenced in lore fragments, item descriptions, station names, and quest hooks.

### Figure Archetypes

- **The Founder** — established the civilization or led its emergence
- **The Conqueror** — expanded territory, waged wars
- **The Sage** — made the key scientific/philosophical breakthrough
- **The Traitor** — caused a schism or sold out to enemies
- **The Explorer** — pushed the frontier, discovered critical paths toward Sgr A*
- **The Last** — final leader, present at the collapse, fate unknown
- **The Builder** — chief architect of megastructures, stations, or the beacon network

### Figure Properties

```
- Name (from civilization's phoneme pool)
- Title/role (from archetype)
- Era (which epoch, roughly when)
- Key achievement (one sentence)
- Associated location (system/body where they're most remembered)
- Associated artifact (item they created or carried)
- Fate (known death, disappearance, transcendence, unknown)
```

### Gameplay Integration

- **Named items:** "The Blade of [Conqueror Name]", "[Sage Name]'s Codex"
- **Station names:** "[Builder Name]'s Station", "The [Founder Name] Observatory"  
- **Lore fragments:** journal entries reference figures and their deeds
- **Quest hooks:** "Find the tomb of [The Last]", "Recover [The Explorer]'s star charts"

---

## Epic / Legendary Items

The lore generator creates a set of unique artifacts tied to history. These are the most powerful items in the game, each with a story.

### Generation

Each civilization produces 1-3 legendary artifacts. Properties:
- **Name:** drawn from civilization's phoneme pool + item type
- **Origin:** which figure created it, when, why
- **Power:** gameplay effect (stat bonuses, unique abilities)
- **Location:** where it ended up (which system/body/dungeon)
- **Lore text:** 2-3 sentences connecting it to historical events
- **Visual:** distinct glyph and color on the map

### Artifact Categories

- **Weapons:** forged during wars, carried by conquerors
- **Navigation tools:** created by explorers, point toward Sgr A*
- **Knowledge stores:** codices, crystals, engrams holding ancient data
- **Keys:** literally or figuratively unlock paths deeper into the galaxy
- **Anomalies:** objects that defy understanding, behave strangely, tied to Sgr A*

### Placement

Artifacts are placed during galaxy generation:
- Some in deep dungeons on historically significant planets
- Some carried by powerful NPCs (guardians, descendants)
- Some in sealed vaults requiring prerequisites (archaeology skill, prior artifacts)
- Some fragmented — pieces scattered across multiple systems

---

## Galaxy Integration

### How Lore Shapes the Physical Galaxy

**System significance levels:**
- **Tier 0 (mundane):** ~70% of systems. No special historical significance. Normal generation.
- **Tier 1 (touched):** ~20% of systems. Minor historical events. A ruin here, a named asteroid there. Flavor text and lore fragments.
- **Tier 2 (significant):** ~8% of systems. Major historical sites. Unique dungeon types, named stations, quest-relevant locations.
- **Tier 3 (pivotal):** ~2% of systems. Civilization homeworlds, great battle sites, beacon network nodes. Unique biomes, legendary artifacts, major quest chains.

**Physical effects of history:**
- Ancient wars leave asteroid fields, debris rings, irradiated planets
- Megastructure remnants create unique stellar phenomena
- Beacon network nodes glow on the star chart (once discovered)
- Collapsed civilizations leave "dead zones" — systems with no stations, high danger
- Terraformed worlds retain alien biomes millions of years later
- Binary systems might result from stellar engineering gone wrong

**The path to Sgr A*:**
- The beacon network traces a rough route from the galactic rim toward the center
- Each civilization extended the path further inward
- The player must follow and reactivate this network
- Systems along the path are disproportionately significant (Tier 2-3)

### Generation Order

```
1. Generate lore timeline (epochs, civilizations, events, figures)
2. Determine key locations (homeworlds, battle sites, beacons, vaults)
3. Generate galaxy structure (spiral arms, star density)
4. Place key locations into galaxy (respecting spatial logic)
5. Generate remaining systems around key locations
6. Assign significance tiers to all systems
7. Populate systems with bodies, biomes, stations, dungeons
8. Place artifacts, lore fragments, quest hooks
9. Generate present-day factions (informed by recent history)
10. Player begins
```

---

## The Archaeology Skill

A new skill category that directly interfaces with the lore system.

### Category: Archaeology (proposed)

**Category unlock:** 75 SP  
**Base effect:** can identify which civilization built a ruin (era label shown on dungeon entry)

**Proposed skills:**

| Skill | Type | Cost | Effect |
|-------|------|------|--------|
| Ruin Reader | Passive | 50 SP | Lore fragments found in ruins reveal more detail (full text vs partial) |
| Artifact Identification | Passive | 75 SP | Unidentified ancient items are auto-identified on pickup |
| Excavation | Active | 50 SP | Search a ruin tile for hidden caches (chance to find lore/items) |
| Cultural Attunement | Passive | 75 SP | Bonus to using artifacts from civilizations you've studied extensively |
| Precursor Linguist | Passive | 100 SP | Can read ancient inscriptions — unlocks sealed doors, reveals vault locations |
| Beacon Sense | Passive | 100 SP | Beacon network nodes glow on star chart before visiting the system |

### Interaction with Minimap

The Cartographer wayfinding skill shows POIs on the minimap. Archaeology skills could add an overlay:
- Excavation sites highlighted
- Hidden chambers detected
- Beacon signatures visible

---

## Lore Delivery to the Player

### Passive Discovery (always present)

- **Dungeon entry text:** "You enter ruins of [civilization] origin, dating to the [epoch name]."
- **Item descriptions:** "[Artifact name] — [origin sentence]. [gameplay effect]."
- **Station names:** reflect their builders
- **Star chart annotations:** historically significant systems get a small marker after visiting

### Active Discovery (requires engagement)

- **Lore fragments:** found in ruins, read via journal. Piece together history.
- **NPC scholars:** Astronomers and other NPCs discuss history, reference events and figures
- **Inscriptions:** found on walls in dungeons. Require Precursor Linguist skill for full text.
- **Codex entries:** auto-compiled as the player discovers lore. A "Galactic History" section in the journal.

### Progressive Revelation

The player doesn't get a lore dump. History is revealed in fragments:
1. First ruins visited — player learns "ancient civilization existed"
2. Multiple ruins visited — player notices different architectural styles (multiple civilizations)
3. Lore fragments collected — timeline begins to form
4. Key locations discovered — major events revealed
5. Beacon network found — the path to Sgr A* becomes clear
6. Deep history pieced together — the cycle of convergence understood

This mirrors the player's journey toward Sgr A* — the closer they get, the more they understand why.

---

## Technical Considerations

### Seed-Based Generation

All lore is deterministic from the world seed. Same seed = same history, same civilizations, same artifacts. This ensures:
- Consistent world across save/load
- Multiplayer compatibility (future)
- Reproducible for debugging

### Data Size

The full lore state (timeline, civilizations, figures, artifacts, location assignments) should be lightweight:
- ~5-10 civilization records
- ~30-50 key figures
- ~10-20 legendary artifacts  
- ~100-200 significant system annotations
- Estimate: <50KB serialized

### Phoneme Pool Size

Each phoneme pool needs ~30-50 syllables to produce varied but consistent names. With 5-6 pools, that's ~200-300 curated syllables total. Small enough to compile in, large enough for variety.

### Integration Points

The lore system needs to interface with:
- `StarChart` / galaxy generation — place key locations
- `TileMap` / dungeon generation — apply civilization aesthetics
- `Item` / item generation — create legendary artifacts with lore
- `Npc` / dialog — NPCs reference history
- `Quest` / quest generation — lore-driven quest hooks
- `Journal` — codex/lore fragment storage
- `SkillDefs` — archaeology skill category
- `SaveFile` — persist generated lore state
- `Minimap` — archaeology overlays (future)

---

## What This Enables

With this system in place, every playthrough tells a different story:

*"In this run, the Primordials were the Aelithae — contemplative light-weavers who built the first beacon network. They entered Sgr A* voluntarily, leaving behind crystalline spires across the inner galaxy. 300 million years later, the Grothkhar — a predatory species — found their ruins and weaponized the beacon technology. The Grothkhar wars shattered three star systems (now asteroid fields). A third civilization, the Seiwenari, emerged in the aftermath, built stations to study the destruction, and extended the beacon network further inward before vanishing in a plague. Humanity arrived to find layers of alien history, reverse-engineered hyperspace from Grothkhar wreckage, and now the player discovers that all three civilizations were following the same signal from the center..."*

Next playthrough — completely different civilizations, different events, different path to Sgr A*. Same structure, infinite stories.

---

## Developer History Log

In dev mode, a full history viewer is accessible from the developer menu. This dumps the entire generated timeline in readable form — essential for debugging, tuning, and appreciating what the generator produces.

### Contents

The dev history log displays:

**Header:**
- World seed
- Number of epochs / civilizations generated
- Total timeline span (billions of years)

**Per Epoch:**
```
═══════════════════════════════════════════════════
EPOCH 0: The Aelithae Convergence
    Span: 4.2 - 3.8 billion years ago (400M years)
    Phoneme pool: B (flowing/ancient)
    Aesthetic: crystalline architecture, light-woven technology
    Philosophy: contemplative
    Collapse: voluntary transcendence via Sgr A*
═══════════════════════════════════════════════════

  TIMELINE:
    4.2 Bya  Emergence — first Aelithae consciousness on Lerimund Prime (Sys #4471)
    4.1 Bya  Discovery — Aelithae detect Sgr A* gravitational anomaly
    4.0 Bya  Expansion — first hyperspace route established
    3.95 Bya Construction — Beacon Alpha placed at Sys #2208
    3.9 Bya  Construction — Spire of Vynosar built at Sys #891
    3.85 Bya Discovery — convergence pattern recognized
    3.8 Bya  Transcendence — Archon Ithaemund leads the Convergence into Sgr A*

  KEY FIGURES:
    Archon Ithaemund (The Founder) — established the Convergence
      Location: Sys #4471 (Lerimund Prime)
      Artifact: The Ithaemund Lens
      Fate: entered Sgr A* with the last wave

    Vynosar the Builder — chief architect of the beacon network
      Location: Sys #891 (Vynosar's Spire)
      Artifact: Beacon Resonance Key
      Fate: remained behind, died at the Spire

  ARTIFACTS:
    The Ithaemund Lens — navigation device, reveals beacon locations
      Origin: Archon Ithaemund, crafted before transcendence
      Location: sealed vault on Lerimund Prime (Sys #4471, Body 2)
      Effect: +3 view radius, beacon network visible on star chart

    Beacon Resonance Key — activates dormant beacons
      Origin: Vynosar the Builder
      Location: Vynosar's Spire (Sys #891, Body 0)
      Effect: unlocks beacon fast-travel network

  GALAXY IMPACT:
    Sys #4471 — Tier 3 (Primordial homeworld), unique biome
    Sys #891  — Tier 3 (Spire), megastructure dungeon
    Sys #2208 — Tier 2 (first beacon), navigation node
    12 additional Tier 1 systems with minor ruins

[... repeats for each epoch ...]

═══════════════════════════════════════════════════
PRESENT DAY: Humanity
═══════════════════════════════════════════════════

  FACTIONS:
    Stellari Conclave — descended from first wave colonists
    Kreth Mining Guild — formed during resource wars
    [... generated factions ...]

  BEACON NETWORK:
    Total beacons placed: 14 (across 3 civilizations)
    Active: 2  Dormant: 9  Destroyed: 3
    Path coverage: rim → ~40% toward core

  SYSTEM SIGNIFICANCE:
    Tier 3 (pivotal):      7 systems
    Tier 2 (significant): 31 systems
    Tier 1 (touched):     84 systems
    Tier 0 (mundane):    remaining
```

### Access

- Dev menu option: "View World History" (or dev console command: `history`)
- Outputs to a scrollable full-screen view (reusing the existing help screen / journal pattern)
- Optionally also dumps to a text file in the save directory for easy reading outside the game

### Why This Matters

The history log serves multiple purposes:
- **Debugging:** verify the generator produces coherent, non-contradictory timelines
- **Tuning:** see if epoch durations, event density, and artifact counts feel right
- **Content review:** read through generated history to catch nonsensical combinations
- **Fun:** it's genuinely interesting to read what the generator comes up with
- **Documentation:** auto-generated world bible for each playthrough

---

## Implementation Priority

1. **Epoch & civilization generator** — the core simulation that produces the timeline
2. **Phoneme-based naming system** — gives everything identity
3. **Developer history log** — dump the full timeline in dev mode for debugging and review
4. **Galaxy integration** — lore shapes system generation
5. **Lore fragment system** — player discovery mechanism
6. **Legendary artifact generation** — items tied to history
7. **Archaeology skill tree** — player interaction with lore
8. **NPC dialog integration** — NPCs reference history
9. **Quest hooks** — lore-driven quests

This is a large feature but highly modular — each piece adds value independently.
