# Minimap Widget Design

**Date:** 2026-04-04  
**Status:** Approved

## Overview

A player-centered minimap rendered in the side panel widget system using half-block pixel characters for double vertical resolution. Renders walkable space as filled colored blocks across all map types. Designed with extension points for the Wayfinding skill tree.

## Rendering Engine

### Half-Block Pixel Style

Each terminal cell uses upper/lower half-block characters (`▀ ▄ █`) to pack 2 map rows into 1 terminal row. This gives 20 rows of effective map resolution in the 10-row widget.

- Scale: 1:1 horizontal (1 minimap column = 1 map tile), 2:1 vertical (1 terminal row = 2 map rows)
- No downsampling — the minimap is a scrolling viewport, not a shrunken full-map view

### Walkable Space Rendering

The minimap renders **passable/walkable space** as filled blocks. Walls are not drawn — rooms appear as solid colored shapes, corridors as single-width lines. This maximizes information density.

Structures with walls (stations, outposts, buildings on detail maps) show wall outlines where they exist in the tile data, since walls are distinct tile types that get their own color.

### Player-Centered Viewport

The viewport is always centered on the player and scrolls as they move. At map edges, the viewport clamps — the player dot moves off-center rather than showing blank space beyond the map boundary.

### Widget Size

- Height: 10 terminal rows (20 map rows of resolution via half-blocks)
- Width: determined by side panel width (~28-35 columns depending on terminal size)

## Color Palette

Subdued multi-color. Tile type determines base color, visibility determines brightness.

### Visibility Modifiers

| Visibility State | Effect |
|-----------------|--------|
| Visible | Normal/warm tones |
| Explored (not visible) | Cool/dim tones |
| Unexplored (dungeon) | Blank — nothing drawn |
| Unexplored (station) | Faint structural outline (schematic) |

### Tile Color Mapping

**Indoor (dungeon/station):**
- Floor: dim warm gray
- Walls/structural: brighter gray (where rendered)
- Portals/stairs: magenta or cyan
- Doors: distinct marker color
- Water: blue

**Outdoor (detail map / overworld):**
- Plains/desert: warm yellow/tan
- Forest/fungal: green
- Mountains: gray
- Water/river/lake/swamp: blue
- Ice: cyan
- Lava: red
- POI tiles (caves, ruins, settlements, outposts, landing): bright distinct colors

### Entity Colors

- Player: bright yellow (always visible, always on top)
- Hostile NPCs: red (requires Wayfinding skill)
- Friendly NPCs: cyan (requires Wayfinding skill)
- Ground items: to be determined (requires Wayfinding skill)

## Map Mode Behavior

### Dungeons (caves, asteroid interiors)

- Strict fog of war — only explored tiles rendered
- Walkable space as filled blocks
- Corridors as single-width lines
- Portals/stairs always visible (structural features)

### Stations (space stations, ships)

- Schematic mode — unexplored areas show faint structural outlines
- Player would reasonably have access to station schematics
- Explored areas render at full brightness

### Detail Maps (outdoor zones)

- Fog of war (same as dungeons)
- Terrain-colored fills by tile type
- Structures (outposts, ruins, settlements) show wall outlines
- POI markers in distinct bright colors

### Overworld (planetary surface)

- Fog of war — only explored overworld tiles shown
- Terrain-colored fills per biome tile type
- POI tiles (cave entrances, ruins, settlements, crashed ships, landing zone) in bright distinct colors
- Player position highlighted
- Player-centered with clamping (same behavior as other maps)

## Wayfinding Skill Integration

The minimap is designed to grow with the Wayfinding skill category. The rendering accepts a configuration that controls what gets displayed.

### Base Minimap (no skills required)

- Walkable space / terrain fills
- Player position (bright yellow)
- Portals, exits, stairs, hatches (structural map features)
- Fog of war / schematic visibility rules

### Planned Wayfinding Skill Unlocks

These are rendered by the minimap but gated by skill checks. The minimap class accepts a flags struct so the skill system can toggle features:

- **Enemy Detection** — hostile NPCs shown as red markers on minimap
- **NPC Awareness** — friendly/neutral NPCs shown as cyan markers
- **Item Sense** — ground items shown on minimap
- **POI Discovery** — discovered POIs highlighted at range
- **Extended Cartography** — potential for larger viewport or reduced fog

The Minimap class should expose a `MinimapFlags` or similar config struct:

```cpp
struct MinimapFlags {
    bool show_enemies = false;
    bool show_npcs = false;
    bool show_items = false;
    bool show_pois = false;
};
```

The Game or skill system sets these flags based on learned skills before each render call.

## Implementation Scope

### This implementation

- Minimap class with half-block rendering engine
- Player-centered scrolling viewport with edge clamping
- Walkable-space fill rendering (no wall outlines drawn as separate features — walls just get their own fill color where they exist as tiles)
- Tile-type color mapping for all indoor and outdoor tile types
- Visibility-aware rendering (visible/explored/unexplored/schematic)
- Base features always shown: player, terrain, portals/exits
- MinimapFlags struct for future skill integration (all flags default false for now)
- Wire into existing `render_minimap_widget` stub
- Update `widget_desired_height` to 10

### Deferred

- Wayfinding skill definitions and unlocks
- POI discovery mechanic
- Extended cartography effects
- Items on minimap

## Technical Notes

- The Minimap class lives in `include/astra/minimap.h` and `src/minimap.cpp`
- Uses UIContext `put()` for half-block character rendering
- Accesses TileMap, VisibilityMap, player position, and NPC list via draw parameters
- No new renderer interface needed
- The existing widget system handles layout — minimap just draws into the UIContext it receives
