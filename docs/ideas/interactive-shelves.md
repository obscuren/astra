# Interactive Shelves

## Concept

Shelves in settlement buildings (and future POIs) can hold lootable items. This gives players a reason to explore interiors and creates visual variety — some shelves stocked, some empty.

## Visual Structure

Shelves are 3-tile structures placed against walls. Orientation follows the wall:

**Against east/west walls (vertical):**
```
#║   ← shelf top (brown ║)
#~   ← item slot (colored by item type)
#║   ← shelf bottom (brown ║)
```

**Against north/south walls (horizontal):**
```
######
 ═~═    ← shelf-left, item, shelf-right
```

## Rendering

- Empty shelf tile: `║` or `═` in brown (color 137), FixtureType::Shelf
- Item slot (occupied): `~` colored by item type (red = book/scroll, cyan = data crystal, yellow = rare)
- Item slot (empty/looted): reverts to `║` or `═` (same as shelf)

## Interaction

- Player presses interact key (`e`) adjacent to the item slot tile
- Item is added to player inventory
- The item fixture is removed, tile reverts to empty shelf
- Loot table determines what spawns: books, scrolls, data crystals, rare artifacts
- Loot quality influenced by lore_tier and biome

## Loot Types (ideas)

- **Books/scrolls** — lore fragments, skill books, recipes
- **Data crystals** — memory engrams with civilization history (ties into lore fragment system)
- **Components** — small ship/gear parts
- **Rare artifacts** — unique items tied to historical figures (ties into legendary artifact generation)

## Integration

- Furniture placement system creates the 3-tile shelf structure
- A separate loot spawner populates item slots based on building type and lore context
- Main Hall shelves might have data terminals; Market shelves have goods; Dwelling shelves have personal items

## Dependencies

- Furniture placement system (in progress)
- Lore fragment system (roadmap item)
- Potentially legendary artifact generation (roadmap item)
