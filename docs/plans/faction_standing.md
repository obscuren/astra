# Faction Standing System

## Overview

NPC faction reputation affects gameplay: shop pricing, dialog access, quest availability, and item stock tiers.

## Reputation Tiers

| Tier | Range | Price Mod | Dialog | Quests | Stock |
|------|-------|-----------|--------|--------|-------|
| Hated | <= -50 | +30% | Refused | None | N/A |
| Disliked | -49 to -10 | +15% | Allowed | None | Base |
| Neutral | -9 to 9 | 0% | Allowed | Available | Base |
| Liked | 10 to 49 | -10% | Allowed | Available | Base + bonus |
| Trusted | >= 50 | -20% | Allowed | Available | Base + bonus + premium |

## NPC Faction Assignments

| NPC | Faction |
|-----|---------|
| Station Keeper, Commander, Medic, Astronomer, Engineer | Stellari Conclave |
| Merchant, Arms Dealer, Food Merchant | Kreth Mining Guild |
| Drifter | Unaligned |
| Xytomorph | Xytomorph Hive |

## Key Files

| File | Purpose |
|------|---------|
| `include/astra/character.h` | ReputationTier enum, helper declarations |
| `src/faction.cpp` | Tier logic, price modifiers, reputation lookup |
| `include/astra/npc.h` | `faction` field on Npc struct |
| `src/npcs/*.cpp` | Faction set on all NPC builders |
| `src/trade_window.cpp` | Faction-based price modifiers |
| `src/dialog_manager.cpp` | Hated gate, quest rep requirement |
| `src/item_defs.cpp` | Tiered stock generation |
| `src/npc_spawner.cpp` | Passes player reputation to shop NPC builders |

## Implementation Status

All phases complete:
- [x] Phase 1: Foundation (tier enum, helpers, NPC faction field)
- [x] Phase 2: Shop pricing
- [x] Phase 3: Dialog & quest gates
- [x] Phase 4: Tiered shop stock
- [x] Phase 5: Polish (formulas doc, dev console `give rep`, trade window indicator)
