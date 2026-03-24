# Tinkering System

## Overview

Tinkering is the game's crafting system. It enables repairing items, enhancing them with crafting materials, analyzing items to learn blueprints, and salvaging items for parts.

## Four Pillars

### Pillar 1: Analysis
- Any item can be analyzed to learn its "molecular blueprint signature"
- Takes time (ticks), may destroy the item
- Higher Intelligence = higher chance to preserve during analysis
- Item signatures are broken into components (e.g. Plasma Pistol → "Plasma Emitter" + "Grip Assembly" + "Power Conduit")
- Rare/Epic/Legendary items have hidden signatures discoverable only with high Tinkering skill

### Pillar 2: Synthesis (Future)
- Combine 2-3 learned signatures + crafting materials to create new items
- Results blend properties from chosen signatures
- e.g. "Plasma Emitter" + "Blade Housing" = Plasma Blade (melee with charge mechanics)
- Somewhat unpredictable — Intelligence and Luck affect quality and affix rolls
- No fixed recipe book — discovery-based

### Pillar 3: Repair & Enhancement
- Repair: spend Nano-Fiber to restore durability
- Enhancement slots: items have 1-3 mod slots based on rarity
  - Common: 1 slot
  - Uncommon: 2 slots
  - Rare+: 3 slots
- Slot in crafting materials for permanent bonuses:
  - Power Core → +2 ATK
  - Circuit Board → +1 view radius
  - Alloy Ingot → +2 DEF
  - Nano-Fiber → durability restore (repair, not enhancement)
- Enhancements are permanent but can be overwritten

### Pillar 4: Salvage
- Disassemble items into crafting materials
- Higher Tinkering skill = better yield
- Higher rarity items yield rarer materials
- Chance to recover item's blueprint signature without destroying it (high INT)

## UI Layout (Tinkering Tab)

```
   ──┤ WORKBENCH ├────────────────────────┬─────────────────────────────────
                                           │
         ┌───────────────────────┐         │  Ion Blaster
         │                       │         │  Uncommon Ranged Weapon
         │     ) Ion Blaster     │         │  ATK: +5  Charge: 15/15
         │                       │         │  Durability: ████████░░ 48/60
         └───────┬───────────────┘         │
                 │                          │  ──┤ ENHANCEMENT SLOTS ├──
        ┌────────┴────────┐                │
        │                  │                │  [1] ┌─────┐  + Power Core
   ┌────┴────┐  ┌────┴────┐  ┌─────────┐  │      │  =  │  ATK +2
   │ SLOT 1  │  │ SLOT 2  │  │ SLOT 3  │  │      └─────┘
   │  empty  │  │  empty  │  │ locked  │  │  [2] ┌─────┐  empty
   │         │  │         │  │    ×    │  │      │     │  drag material
   └─────────┘  └─────────┘  └─────────┘  │      └─────┘  here
                                           │  [3] ┌─────┐  locked
   ──┤ MATERIALS ├────────────────────── │      │  ×  │  (requires Rare+)
                                           │      └─────┘
   + Nano-Fiber x3    + Power Core x2     │
   + Circuit Board x1  + Alloy Ingot x3   │  ──┤ ACTIONS ├────────────────
                                           │
                                           │  [r] Repair      15 Nano-Fiber
                                           │  [a] Analyze     destroys item
                                           │  [s] Salvage     yields parts
```

### UI Elements

- **Workbench**: Large bordered box, place item here with Space
- **Enhancement Slots**: 3 boxes connected to workbench with tree lines (│┴)
  - Empty: selectable, place crafting material
  - Filled: shows material glyph + bonus
  - Locked: shows ×, grayed out, unlocks by rarity
- **Materials**: Crafting material inventory from player's bag
- **Detail Panel (right)**: Full item stats, durability bar, slot details
- **Actions**: Repair/Analyze/Salvage with costs shown

### Enhancement Material Effects

| Material | Slot Effect |
|----------|------------|
| Nano-Fiber | Durability restore (repair only) |
| Power Core | +2 ATK |
| Circuit Board | +1 view radius |
| Alloy Ingot | +2 DEF |

### Flow

1. Place item from inventory onto workbench (Space)
2. Navigate to enhancement slot (arrows)
3. Place crafting material (Space opens material picker)
4. Preview bonus before confirming
5. [r] to repair if durability low

## What Makes It Unique

- No fixed recipe book — discovery through analysis
- Cross-category crafting in synthesis (future)
- Visual workbench → slot tree UI
- Slots unlock based on rarity (visible progression)
- Every crafted/enhanced item is somewhat unique
- Creates reason to pick up and analyze everything
