# Ruins POI V2 — Wall Network Megastructures

## Overview

Ruins are ancient megastructure complexes that span one or more overworld tiles. Unlike settlements (individual buildings connected by paths), ruins are continuous wall networks that cut through the natural landscape — massive walls extruding through forests, deserts, and other biomes. The player explores a single enormous ruined complex, not a collection of separate buildings.

Each ruin belongs to a specific ancient civilization, which determines its visual style (glyph palette, colors, architectural tendencies). Multiple civilizations exist, each data-driven and configurable.

## Design Principles

- **Megastructure feel**: Thick walls slice through terrain like monumental partitions. The landscape grows around and through the ruins, not the other way around.
- **Gradient exploration**: Outer areas are open sectors with massive wall partitions and wilderness between them. Deeper areas are dense, maze-like room clusters. Push deeper = more intact, more rewarding.
- **One complex, not many buildings**: The wall network is continuous. Rooms are spaces carved within the network, not standalone structures.
- **Visual identity per civilization**: Each ancient civ has distinct glyphs, colors, and architectural logic visible from the overworld and within the detail map.

## Wall Network Generation

### Algorithm: BSP + Noise Decay

The generator uses binary space partitioning (BSP) to recursively subdivide the ruin footprint:

1. **Shallow splits** produce large outer sectors with thick walls (3–5 tiles). Terrain (trees, grass, water) fills the open space between partitions. These are the monumental walls the player sees cutting through the biome.
2. **Deep splits** in select areas produce dense interior clusters with thinner walls (1–2 tiles) forming rooms and corridors. These become themed zones (temples, vaults, archives).
3. **Non-uniform recursion depth**: The algorithm selects 2–4 "nucleus" points within the footprint and recurses deeper around them. The rest of the footprint stays shallow. This creates organic clustering — dense nodes connected by monumental partitions.

### Post-BSP Processing

After the BSP pass:

- **Thickness variation**: Wall thickness is modulated by fbm noise — walls bulge and thin along their length.
- **Gap insertion**: Wall segments are randomly broken to create passageways.
- **Axis bending**: Some walls bend slightly off their split axis to break grid regularity.
- **Opening carving**: Doorway-sized openings are carved at strategic points to ensure navigability.

### Footprint Size

The ruin footprint covers most of the detail tile — approximately 80–95% of the map area. Unlike settlements which sit within the landscape, the ruin IS the landscape. Biome terrain (trees, water, grass) fills the gaps between walls, not the other way around.

### Wall Thickness

- Outer partition walls: 3–5 tiles thick
- Inner room walls: 1–2 tiles thick
- Thickness is driven by BSP depth — shallower splits = thicker walls

## Decay System

### Gradient + Sectoral Decay

Decay uses two overlapping systems:

1. **Gradient decay**: Edges of the footprint decay more heavily than the interior (exposed to elements longer). Creates exploration reward — push deeper to find intact areas.
2. **Sectoral variance**: Random per-sector multiplier overlays the gradient. Some outer sectors survive surprisingly well; some inner sectors collapse catastrophically. Prevents predictable falloff.

Combined formula per sector: `effective_decay = base_gradient(distance_from_edge) * sector_variance(rng)`

### Decay Effects

Reuses and extends existing `RuinDecay`:

- Wall collapse: tiles removed with probability based on effective_decay, replaced with Floor + Debris fixture
- Rubble near surviving walls: floor tiles adjacent to walls get Debris fixtures
- Overgrowth: biome-specific flora on exposed indoor floors (existing biome tables)
- Wall tinting: surviving walls get per-civilization tint colors via `custom_flags_`

## Post-Processing Stamp System

After generation and decay, an ordered list of composable stamp functions modify the result. A ruin can have multiple stamps applied.

### Stamp Types

- **Battle-scarred**: Blast craters punched through walls (circular wall removal), scorch tinting on surrounding walls, debris scatter radiating from impact points
- **Infested**: Organic growths on wall tiles, nest fixtures in rooms, web/slime overlay on floors
- **Flooded**: Water tiles seeping into lower-elevation sectors, corroded wall tinting near water, aquatic flora
- **Excavated**: Sections cleared by scavengers, scaffolding fixtures, clean-cut wall removals (straight edges, not ragged decay)

### Architecture

Each stamp is a function: `void apply(TileMap& map, Rect footprint, RNG& rng, StampConfig config)`

Stamps are data-driven and extensible. New stamps added without modifying the core generator. The `DecayContext` struct is extended to carry stamp configuration.

## Multi-Tile Architecture

### Overworld Ruin Clusters

- During overworld generation, ruin clusters are placed based on region lore and density configuration. The placement system selects a seed tile and grows an irregular shape outward (random walk or noise-driven growth), rejecting tiles that would overlap existing POIs.
- Cluster shapes are non-rectangular: L-shapes, crosses, T-shapes, organic blobs
- Each tile in the cluster stores which edges connect to other ruin tiles and which civilization the ruin belongs to

### Size Scaling

Ruin size scales with significance. Frequency is configurable:

| Category | Overworld tiles | Frequency |
|----------|----------------|-----------|
| Outpost | 1–2 | Common |
| Site | 3–5 | Uncommon |
| Complex | 6+ (irregular) | Rare |

### Edge Continuity

- Wall positions at shared ruin-to-ruin edges are derived from a shared seed: `hash(min(tile_a, tile_b), max(tile_a, tile_b))`
- Both adjacent tiles independently compute matching wall stubs from this seed — no cross-tile loading required
- Ruin-to-wilderness edges get decayed wall stubs that trail off into the biome (no continuity needed)

### Overworld Rendering

- Each ruin tile uses glyphs and colors from its civilization's visual style
- Glyph connections between tiles reflect where walls cross tile boundaries
- Players can identify which civilization built a ruin from the overworld before entering

## Civilization System

### Data-Driven Configuration

Each civilization is a configuration struct:

```
CivConfig:
  name: string
  wall_glyphs: vector<char32_t>    // primary wall characters
  accent_glyphs: vector<char32_t>  // decorative/interior characters
  color_primary: Color             // main wall color
  color_secondary: Color           // accent/highlight color
  color_tint: Color                // ruin tint for custom_flags_
  wall_thickness_bias: float       // multiplier on base thickness (>1 = thicker)
  split_regularity: float          // 0 = organic BSP, 1 = geometric BSP
  preferred_rooms: vector<RoomType> // weighted room type preferences
```

### Initial Civilizations

| Name | Glyphs | Colors | Character |
|------|--------|--------|-----------|
| Monolithic | ░██▓ blocks | Gray, white | Thick walls, large halls. The builders. |
| Baroque | ╔═╗║╬ box-drawing | Warm gold, red | Geometric precision, detailed rooms. The artisans. |
| Crystal | ┌─┐│ thin lines, ◈◎ accents | Cyan, white | Sparse, open layouts. Faintly powered. The scientists. |
| Industrial | ▓█░ mixed blocks, ⊞⊠ | Corroded red, orange | Dense, functional layouts. The engineers. |

Additional civilizations added by creating new `CivConfig` entries — no code changes needed.

### Rendering Integration

- **Overworld**: Civilization glyph palette drives tile rendering
- **Detail map**: Wall tile type and `CF_RUIN_TINT` extended to support per-civilization tint colors (currently only mossy green / crumbling brown — generalize to arbitrary color from CivConfig)

## Room Theming & Content

### Room Discovery

After BSP generates the wall network:

1. Flood-fill identifies enclosed or semi-enclosed spaces
2. Spaces with minimum ~8×8 interior area become candidate rooms
3. Each nucleus cluster gets a deliberate theme assigned
4. Remaining spaces get organic theming based on geometry:
   - Large open spaces → halls
   - Small enclosed spaces → vaults/storage
   - Elongated spaces → corridors/galleries

### Room Types and Content

| Room Type | Content Focus | Key Fixtures |
|-----------|--------------|--------------|
| Vault | Loot | Crates, locked containers, ancient tech |
| Archive | Lore | Terminals with logs, data consoles, history |
| Temple | Lore + encounters | Murals, altars, guardians |
| Great Hall | Encounters + loot | Open space, creature nests, scattered salvage |
| Observatory | Lore + tech | Star maps, nav data, navi-computer upgrades |

### Fixture Placement

Reuses the existing furniture palette system. Each room type defines its own palette with placement rules (Anchor, WallUniform, Center, Corner). Where decay has destroyed a room, functional fixtures are replaced with Debris.

## Integration

### Pipeline Position

POI phase, after scatter, flora, and connectivity — same slot as settlements. New `OW_Ruins` overworld POI type.

### Generation Pipeline (per detail tile)

1. **PlacementScorer** — find best position within detail tile (reuse existing)
2. **BSP subdivision** — generate wall network with variable recursion depth
3. **Wall materialization** — place wall tiles with thickness from CivConfig
4. **Room identification** — flood-fill to find enclosed spaces
5. **Theme assignment** — tag nucleus clusters + organic assignment for remainder
6. **Furniture/content placement** — per-theme palettes
7. **RuinDecay** — gradient + sectoral wall collapse, rubble, overgrowth
8. **Post-processing stamps** — battle-scarred, infested, flooded, etc.
9. **Edge continuity** — wall stubs at ruin-to-ruin edges from shared seed

### Reused Code

- `PlacementScorer` — as-is
- `RuinDecay` — extended with gradient and sectoral decay modes
- Furniture palette system from `BuildingGenerator` — adapted for room theming
- `CivStyle` system — extended with glyph/color palettes into `CivConfig`
- `ExteriorDecorator` — scatter clearing pass for ruins

### New Code

- BSP wall network generator (core algorithm)
- Room identifier (flood-fill post-BSP)
- Theme assigner (nucleus + organic)
- `CivConfig` structs and initial civilization data
- Post-processing stamp system and initial stamp implementations
- Edge continuity module (seed-based wall stub matching)
- Overworld ruin cluster generator (irregular shapes)
- Overworld ruin renderer (per-civ glyph rendering)

### Dev Console

- `biome_test <biome> ruins` — single-tile ruin, random civilization
- `biome_test <biome> ruins <civ>` — specific civilization (e.g. `ruins monolithic`)
- `biome_test <biome> ruins connected` — multi-tile with edge stubs
