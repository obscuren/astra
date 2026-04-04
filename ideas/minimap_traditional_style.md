# Minimap Alternative: Traditional Wall + Floor Style

**Status:** Not implemented — kept as reference in case the walkable-space style feels too abstract in practice.

## Description

Instead of rendering only walkable space as filled blocks, this approach draws both walls and floors:

- Walls rendered as solid half-blocks (gray/white tones)
- Floors rendered as darker interior fills
- Rooms appear as hollow rectangles with visible outlines
- Corridors show as wall-floor-wall (3 rows wide instead of 1)

## Visual Example (half-block, subdued multi-color)

```
    ▄▄▄▄▄▄▄▄▄                   
    █▄▄▄▄▄▄▄█                   
    █   @   █      ▄▄▄▄▄▄▄      
    █       █▄▄▄▄▄█▄▄▄▄▄█      
    █▀▀▀▀▀▀▀█     █ *   █      
    ▀▀▀▀▀▀▀▀▀     █▀▀▀▀▀█      
                   ▀▀▀▀▀▀▀      
```

## Trade-offs vs Walkable Space

| Aspect | Traditional | Walkable Space |
|--------|------------|----------------|
| Room shapes | Very clear outlines | Implied by filled area |
| Density | ~2 rooms in viewport | ~4+ rooms in viewport |
| Orientation | Good locally | Better globally |
| Corridors | 3 cells wide | 1 cell wide |
| Readability | Immediately intuitive | Needs brief learning |

## When to reconsider

- If players report confusion about room boundaries in the walkable-space minimap
- If dungeon layouts become more complex and room shapes matter more for navigation
- If the minimap widget grows larger (making density less critical)

## Implementation notes

Same half-block rendering technique, same player-centered scrolling with clamping. Only the tile-to-pixel mapping logic changes — instead of skipping wall tiles, render them as brighter blocks surrounding darker floor interiors.
