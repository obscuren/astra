# Ruins POI — Design Spec

Second POI type after settlements. Multi-tile ancient structures with decay, overgrowth, and edge connections between overworld tiles.

## Overview

Ruins are remnants of precursor civilizations — large, monumental buildings in various states of collapse. Unlike settlements (which are small, functional villages), ruins feel ancient and grand. They can span multiple overworld tiles with structures connecting at shared edges.

## Structure Types

Five large building types, all bigger than settlement buildings:

| Type | Size | Description |
|------|------|-------------|
| Temple | 18-24 × 10-14 | Columns (pillar fixtures), central altar area |
| Vault | 12-16 × 8-12 | Thick double walls, narrow entrance, inner chamber |
| Great Hall | 20-28 × 12-16 | Massive open space, collapsed sections with rubble |
| Archive | 14-18 × 8-10 | Walls lined with shelf structures |
| Observatory | 10-14 × 10-14 | Roughly square, elevated center platform |

Each ruin tile gets 1-2 structures. Single-tile ruins get 1 structure. Multi-tile ruins get 1-2 per tile.

## Architecture Theming

- **Default ruins** (~70%) — generic ancient stone/metal architecture. Walls, columns, rubble. No special civ theming. Used when `lore_alien_strength` is low.
- **Civ-themed ruins** (~30%) — when `lore_alien_architecture` or `lore_tier` indicates significance. Architecture type (Crystalline, Organic, Geometric, VoidCarved, LightWoven) influences wall glyphs, column styles, and fixture choices.

Both use the same structure types (Temple, Vault, etc.) — theming changes the visual style, not the layout.

## Generation Pipeline

```
poi_phase detects OW_Ruins
  ├── PlacementScorer     — find best location in core zone
  ├── RuinPlanner         — picks structure types, plans edge connections
  ├── BuildingGenerator   — places structures (high decay CivStyle)
  ├── RuinDecay           — post-processing:
  │     • extra wall collapse
  │     • rubble where walls were
  │     • overgrowth (flora on interior tiles, biome-dependent)
  │     • wall tinting flags (moss/crumble)
  ├── EdgeConnector       — extends walls to connected map edges
  ├── PathRouter          — corridors between structures (winding, overgrown)
  └── ExteriorDecorator   — minimal (scatter clearing only, no lamps/benches)
```

### New Components

**RuinPlanner** — replaces SettlementPlanner for ruins. Determines:
- Which structure types to generate (1-2 per tile, random from pool)
- Position within the core zone (center of map, avoiding edges unless connected)
- Which edges have ruin neighbors (from `detail_neighbor_n/s/e/w`)
- Edge connection points

**RuinDecay** — post-processing pass run after BuildingGenerator:
- Additional wall removal beyond CivStyle decay (total 50-70% walls missing)
- Rubble placement: Debris fixtures where walls were removed
- Overgrowth: FloraGrass/FloraHerb on interior floor tiles, density scaled by biome (jungle=heavy, rocky=light)
- Wall tinting: set `custom_flags_` bit on surviving walls to signal renderer to apply moss (green) or crumble (brown) coloring based on seed

**EdgeConnector** — extends structures to connected map edges:
- Builds wall segments along connected edges (~60% coverage, organic noise offset)
- Carves 2-3 passage gaps per edge (4-5 tiles wide each)
- Gap positions deterministic: seeded from `overworld_x * 7919 + overworld_y * 104729 + direction` so adjacent tiles match
- Winding 3-wide corridors from each gap to nearest structure in core zone

### Reused Components

- **PlacementScorer** — same terrain-aware scoring as settlements
- **BuildingGenerator** — with higher decay (0.5-0.7), larger size ranges, ruin-specific interior layouts (columns for temples, shelves for archives)
- **PathRouter** — winding corridors between structures, same as settlement entry paths
- **ExteriorDecorator** — only scatter clearing within footprint, no lamps/benches/planters

### CivStyle for Ruins

Use `civ_ruined()` with increased decay:
- `decay = 0.5f` for default ruins, `0.7f` for heavily damaged
- `wall_tile = Tile::Wall` (renderer applies tinting via custom_flags_)
- All furniture types → `Debris`

## Decay System

### DecayContext (extensible)

```cpp
struct DecayContext {
    float age_decay = 0.5f;       // wall removal probability
    bool battle_scarred = false;  // future: directional damage
    int blast_direction = -1;     // future: which side took hits
    bool seismic = false;         // future: ground collapse
};
```

Only `age_decay` implemented now. Other fields reserved for future damage types (battle sites, seismic events). No code for them yet.

### Wall Tinting

Surviving ruin walls get a `custom_flags_` bit that the renderer reads:

```
flag bit set → renderer checks seed:
  seed % 3 == 0 → normal gray wall
  seed % 3 == 1 → green-tinted (mossy)
  seed % 3 == 2 → brown-tinted (crumbling)
```

This provides visual variety without new tile types or fixtures on walls.

### Overgrowth

After decay removes walls and places rubble, the overgrowth pass places flora fixtures on interior floor tiles:

| Biome | Overgrowth Density | Flora Types |
|-------|-------------------|-------------|
| Jungle | Heavy (20%) | FloraGrass, FloraHerb, FloraFlower |
| Forest | Moderate (12%) | FloraGrass, FloraMushroom, FloraHerb |
| Grassland | Moderate (10%) | FloraGrass, FloraFlower |
| Marsh | Heavy (15%) | FloraGrass, FloraHerb |
| Rocky | Light (3%) | FloraLichen |
| Volcanic | None (0%) | — |
| Ice | Light (2%) | FloraLichen |
| Sandy | Light (3%) | FloraGrass |

## Multi-Tile Connections

### Overworld Context

The overworld already generates multi-tile ruin clusters via stamps (1x1, 2x2 L-shape, 3x3 cross). `MapProperties::detail_neighbor_n/s/e/w` tells the detail map generator which neighbors are ruins.

### Edge Connection Algorithm

For each connected edge:

1. **Edge wall** — place a wall segment along the map boundary. Horizontal for N/S edges, vertical for E/W edges. Coverage ~60% of edge length with organic noise offset (same noise system as settlement perimeter).

2. **Passage gaps** — 2-3 gaps per edge, each 4-5 tiles wide. Positions are deterministic from overworld coordinates + direction so adjacent tiles produce matching openings.

3. **Corridors inward** — from each gap, carve a winding 3-wide corridor toward the nearest structure in the core zone.

For non-connected edges: no walls, no corridors. Structures stay in the core zone with natural terrain at edges.

### Gap Alignment

The passage gap seed ensures two adjacent tiles generate gaps at the same positions along their shared edge:
- Tile A's east edge gaps = seeded from `(A.overworld_x, A.overworld_y, East)`
- Tile C's west edge gaps = seeded from `(A.overworld_x, A.overworld_y, East)` (same seed — C knows its west neighbor is A)

Both tiles use the same seed for the shared edge, producing aligned gaps.

## NPC Spawning

Minimal — ruins are mostly abandoned:
- Use existing `spawn_settlement_npcs_v2` with `style_name = "Ruined"` and `size_category = 0` (small)
- Results in 2-3 NPCs: scavengers, drifters
- Lore-significant ruins may have an archaeologist or prospector via the optional role pool

## Dev Testing

Extend `biome_test` command:

```
biome_test <biome> ruins              — single-tile ruin, no edge connections
biome_test <biome> ruins connected    — fake all 4 neighbors as ruins (test edge walls)
```

## File Structure

```
Create:
  include/astra/ruin_planner.h        — RuinPlanner class
  include/astra/ruin_decay.h          — RuinDecay class + DecayContext
  include/astra/edge_connector.h      — EdgeConnector class
  src/generators/ruin_planner.cpp     — structure selection, position planning
  src/generators/ruin_decay.cpp       — wall collapse, rubble, overgrowth, tinting
  src/generators/edge_connector.cpp   — edge walls, gaps, corridors

Modify:
  src/generators/poi_phase.cpp        — add OW_Ruins branch
  src/terminal_theme.cpp              — wall tinting for ruin flag
  src/dev_console.cpp                 — biome_test ruins/connected flags
  src/game.cpp                        — dev_command_biome_test ruins params
  include/astra/game.h                — signature update
  CMakeLists.txt                      — new source files
```
