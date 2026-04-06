# Galaxy Simulation Engine

**Date:** 2026-04-05  
**Status:** Approved — implementation follows immediately

## Overview

Replace the current random-event-pick lore generator with a galaxy-scale civilization simulation. Civilizations have state (population, resources, knowledge, stability, territory, military, Sgr A* awareness) that evolves over time. Events emerge from state thresholds rather than random rolls. Multiple civilizations coexist, compete, trade, and war based on proximity and power.

## Time Resolution

- **Timeline:** ~8 billion years
- **Tick:** 1 million years
- **Total ticks:** ~8,000
- **Performance budget:** 5-10 seconds total (including narrative generation)

## Civilization State Variables

| Variable | Range | Description |
|----------|-------|-------------|
| population | 0-1000 | Abstract population units |
| resources | 0-1000 | Available materials, fuel, food |
| knowledge | 0-1000 | Scientific/technological advancement |
| stability | 0-100 | Internal cohesion, social order |
| territory | vector of system IDs | Claimed star systems |
| military | 0-500 | Defense and offense capability |
| sgra_awareness | 0-100 | Understanding of Sgr A* convergence |

### Initial Values (at emergence)

```
population: 10
resources: 50
knowledge: 5
stability: 70
military: 0
sgra_awareness: 0
territory: [homeworld_system]
```

## Per-Tick Update (per active civilization)

Each tick, in order:

### 1. Growth

```
pop_growth = base_growth * (resources / population) * (stability / 100)
population += pop_growth
resources -= population * consumption_rate
knowledge += research_rate * (stability / 100)
```

Base rates modified by philosophy:
- Expansionist: 1.5x pop growth, 1.2x consumption
- Contemplative: 0.7x pop growth, 0.5x consumption, 2x research
- Predatory: 1.3x pop growth, 1.5x consumption, 1.5x military growth
- Symbiotic: 1.0x pop growth, 0.8x consumption, 1.3x stability recovery
- Transcendent: 0.8x pop growth, 0.6x consumption, 2.5x research, 2x sgra growth

### 2. Expansion (if population > territory.size() * 100)

- Claim nearest unclaimed system
- New colony costs resources
- If no unclaimed systems nearby → expansion pressure builds → instability

### 3. Resource Discovery

- Each owned system produces resources based on body types
- Knowledge unlocks better extraction (more resources per system at higher tech)
- Diminishing returns as systems are depleted over millions of years

### 4. Stability Check

```
if resources < population * 0.5: stability -= 5 (famine)
if territory.size() > stability: stability -= 2 (overextension)
if knowledge > 500 && stability < 50: stability -= 3 (existential crisis)
stability naturally drifts toward 50 (±1 per tick)
```

### 5. Event Triggers

Events fire when conditions are met. Each event modifies state and generates an event record. Events have cooldowns to prevent spam.

### 6. Interactions (between civs with adjacent territory)

Checked after individual updates. Two civs interact when their territories are within range.

## Event Trigger Table

| Condition | Event | State Change | Cooldown |
|-----------|-------|-------------|----------|
| knowledge crosses 50, 100, 200, 400, 800 | ScientificBreakthrough | +knowledge burst, +stability | 100 ticks |
| population > territory * 150 | Colonization (expand) | +territory, -resources | 50 ticks |
| resources < population * 0.3 | ResourceWar (internal) | -population, -stability, ±resources | 200 ticks |
| stability < 20 | CivilWar | -population, -military, -stability | 300 ticks |
| stability < 10 | FactionSchism | split territory, spawn rebel civ? | once |
| stability > 80 && knowledge > 200 | CulturalRenaissance | +stability, +knowledge | 200 ticks |
| stability > 80 && resources > 500 | MegastructureBuilt | -resources, +knowledge, +sgra | 300 ticks |
| military > 100 && territory.size() > 5 | OrbitalConstruction | -resources, +military | 100 ticks |
| knowledge > 300 | SgrADetection (first time) | +sgra_awareness | once |
| sgra_awareness > 50 | ConvergenceDiscovery | +sgra_awareness burst | once |
| sgra_awareness > 80 | Transcendence attempt | collapse or transcend | once |
| population * 0.01 (random) | Plague | -population, -stability | 500 ticks |
| territory.size() > 3 (random) | MiningDisaster | -resources, -population | 200 ticks |
| knowledge > 100 (random) | ArtifactCreation | record in lore | 200 ticks |
| knowledge crosses 100 | RuinDiscovery (if predecessors) | +knowledge boost | once per predecessor |
| population < 5 | Collapse begins | cascade | terminal |
| population <= 0 | Civilization dead | legacy events | terminal |

## Inter-Civilization Interactions

When two civs have territories within interaction range (~30 galaxy units):

| Civ A | Civ B | Outcome |
|-------|-------|---------|
| symbiotic/contemplative | symbiotic/contemplative | Trade: both +resources, +knowledge |
| any | any (first meeting) | FirstContact event |
| predatory | weaker (military < 0.5x) | Conquest: A takes B's border systems |
| predatory | stronger | Border skirmish: both -military |
| expansionist | any (overlapping claims) | BorderConflict: fight for contested systems |
| transcendent | any | Knowledge sharing: both +knowledge |
| any (collapsing) | any | Refugees: B gains population, A loses |

Interaction events generate records for BOTH civilizations.

## Civilization Emergence

New civilizations don't all start at tick 0. They emerge at random intervals:

- First civilization: tick 0
- Subsequent: random interval of 200-800 ticks (200M-800M years)
- Each picks a random unclaimed system as homeworld
- Total 8-15 civilizations over the full timeline

## Collapse & Legacy

When population hits 0:
- Civilization marked as dead
- All territory becomes unclaimed (ruins)
- Legacy events generated: VaultSealed, GuardianCreated, etc.
- Systems retain "ruins_of" metadata for future civs to discover

## What Stays The Same

- NameGenerator (phoneme pools, name composition)
- NarrativeGenerator (template composition, 5 styles, contradictions)
- LoreRecord, KeyFigure, LoreArtifact generation (triggered by sim events)
- Race origins (Stellari, Sylphari, etc. — woven in after sim)
- format_history / lore viewer
- Save/load

## What Changes

- `generate_events()` is replaced by the simulation loop
- Event descriptions are now generated from sim state (richer context)
- System IDs in events are real (claimed by the civ) instead of placeholders
- Figures are generated from notable events during the sim
- Artifacts are generated from ArtifactCreation events

## File Changes

- Modify: `src/lore_generator.cpp` — replace generate_events + generate loop with simulation
- Create: `include/astra/galaxy_sim.h` — CivState struct, GalaxySim class
- Create: `src/galaxy_sim.cpp` — simulation loop
- Keep: everything else unchanged
