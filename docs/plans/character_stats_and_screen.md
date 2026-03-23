# Plan: Character Stats, Attributes & Fullscreen Character Screen

## Status: Complete

## Summary

Added a full character data model (name, race, class, primary/secondary attributes, resistances, skills, reputation) and a fullscreen tabbed character screen with CoQ-style boxed stat rendering. Equipment system expanded from 8 to 11 body slots.

## Changes

| File | Change |
|------|--------|
| `include/astra/race.h` | New — Race enum extracted from npc.h |
| `include/astra/character.h` | New — PlayerClass, PrimaryAttributes, Resistances, Skill, FactionStanding |
| `src/character.cpp` | New — class_name() |
| `include/astra/npc.h` | Removed Race enum, includes race.h |
| `src/npc.cpp` | Added race_name() |
| `include/astra/item.h` | Expanded EquipSlot to 11 slots, Equipment struct updated |
| `src/item.cpp` | Updated slot_ref(), total_modifiers(), added equip_slot_name() |
| `src/item_defs.cpp` | RangedWeapon → Missile |
| `include/astra/player.h` | Added name, race, class, attributes, resistances, dodge_value, skills, reputation, derived methods |
| `include/astra/character_screen.h` | New — CharacterScreen class with 8 tabs |
| `src/character_screen.cpp` | New — fullscreen tabbed UI with CoQ-style stat boxes |
| `include/astra/game.h` | Added character_screen_ member |
| `src/game.cpp` | Wired input/render, 'c' keybind, dev mode character setup |
| `include/astra/save_file.h` | Version 11 → 12 |
| `src/save_file.cpp` | Save/load new player fields, 11-slot equipment, backward compat |
| `CMakeLists.txt` | Added character.cpp, character_screen.cpp |

## Equipment Slots (11)

Face, Head, Body, Left Arm, Right Arm, Left Hand, Right Hand, Back, Feet, Thrown, Missile

## Tabs

Skills (stub), Attributes (implemented), Equipment (implemented), Tinkering (stub), Journal (stub), Quests (stub), Reputation (stub with data), Ship (stub)
