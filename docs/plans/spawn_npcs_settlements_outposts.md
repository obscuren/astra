# Plan: Spawn NPCs in Settlements and Outposts

## Status: Implemented

## Summary

When entering settlement or outpost detail maps, NPCs are now spawned near relevant fixtures using a fixture-scanning approach. No changes to the generator interface were needed.

## Approach

Scan the generated map's fixture grid for specific fixture types and place NPCs on adjacent walkable tiles.

### Settlement NPCs (~4-6 per map)

| Fixture | NPC | Builder |
|---------|-----|---------|
| Console (Main Hall) | Settlement Leader | `build_commander` |
| Table (Market) | Merchant | `build_merchant` |
| Crate (Market) | Arms Dealer or Food Merchant (50/50) | `build_arms_dealer` / `build_food_merchant` |
| Bunk #1 (Dwelling) | Resident | `build_drifter` |
| Bunk #2 (Dwelling) | Resident (50% chance) | `build_drifter` |
| Plaza center | Wanderer | `build_drifter` |
| Plaza offset | Wanderer (33% chance) | `build_drifter` |

### Outpost NPCs (~3-4 per map)

| Fixture | NPC | Builder |
|---------|-----|---------|
| Console (Main Building) | Outpost Commander | `build_commander` |
| Bunk (Main Building) | Guard | `build_drifter` |
| Crate (Storage Shed) | Quartermaster | `build_merchant` |
| Courtyard center | Patrol | `build_drifter` |

## Files Modified

- `include/astra/npc_spawner.h` — declared `spawn_settlement_npcs`, `spawn_outpost_npcs`
- `src/npc_spawner.cpp` — implemented both functions + `find_floor_near` and `find_fixture_pos` helpers
- `src/game.cpp` — call spawners in `enter_detail_map()` after player placement
