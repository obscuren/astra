# Ship System & Tutorial Quest — Full Technical Design

## Overview

The player's starship becomes a core gameplay system with equipment slots, stats, and an AI personality. A tutorial quest chain ("Getting Airborne") teaches core mechanics by requiring the player to find and install three ship components before they can travel.

---

## Ship Data Model

### ShipSlot Enum

```cpp
enum class ShipSlot : uint8_t {
    Engine,
    Hull,
    NaviComputer,
    Shield,
    Utility1,
    Utility2,
};
```

Expandable — new slots can be added (Utility3, Cargo, Weapons, etc.).

### ShipModifiers

```cpp
struct ShipModifiers {
    int hull_hp = 0;          // max hull HP contributed
    int shield_hp = 0;        // max shield HP contributed
    int warp_range = 0;       // added to base navi_range
    int cargo_capacity = 0;   // added to base cargo slots
};
```

Ship components carry `ShipModifiers` (similar to how equipment carries `EquipmentModifiers`). Derived ship stats are computed by summing installed component modifiers.

### Starship Struct

```cpp
struct Starship {
    std::string name;                             // random at start, renameable
    std::string type = "Light Freighter";         // ship class (future: different types)
    std::map<ShipSlot, Item> equipment;           // installed components

    // Derived stats (computed from equipment)
    ShipModifiers total_modifiers() const;        // sum all installed modifiers
    int hull_hp() const;                          // current hull HP
    int max_hull_hp() const;                      // from Hull component
    bool operational() const;                     // true if Engine installed
    bool has_navigation() const;                  // true if NaviComputer installed
};
```

Lives on the `Player` struct as `Starship ship;`. Serialized with save data.

### Ship Component Items

Ship components are `ItemType::ShipComponent` items with an additional `ShipSlot target_slot` field and `ShipModifiers ship_modifiers` on the Item struct.

**Starting components (all need to be found/bought):**

| Component | Slot | Ship Modifiers | Buy/Sell |
|-----------|------|---------------|----------|
| Engine Coil Mk1 | Engine | (enables warp) | Quest item |
| Hull Plate Mk1 | Hull | hull_hp: 25 | 200/65 |
| Navi Computer Mk2 | NaviComputer | warp_range: +1 | Quest reward |
| Shield Generator Mk1 | Shield | shield_hp: 15 | 500/170 |

Future Mk2/Mk3 variants have better modifiers. Utility slot items have player-affecting effects instead of ship modifiers.

### Future: Player-Affecting Ship Components (Utility Slots)

Utility components grant passive or triggered player bonuses:

- **Personal Material Warper (PMW)** — access ship cargo from anywhere
- **Emergency Warp** — on player death: warp to ship at 1 HP, destroy/damage all items
- **Long-Range Scanner** — reveal more of the overworld on landing

These use the existing `Effect` system or a new `ShipAbility` callback pattern. Not implemented in the initial pass but the slot system supports them from day one.

---

## Ship Name Generation

Ships get a random name at creation from a themed name pool. Examples:
- The Wanderer, The Drifter, Voidrunner, Stardust, Iron Wake, Silent Arrow, Horizon's Edge

Player can rename later (future feature — not in initial implementation).

### Future: Ship Types

The `Starship::type` field and the generator system (`create_starship_generator()`) are designed to support multiple ship types:
- Light Freighter (starter) — 4 rooms, 2 utility slots
- Heavy Hauler — larger, more cargo, fewer utility slots
- Scout — small, fast, extra navi range
- Gunship — weapon slots, combat-focused

Each type would have its own generator and slot configuration. Not implemented now but the architecture doesn't preclude it.

---

## ARIA — Ship AI

### Identity

**ARIA** (Autonomous Runtime Intelligence Assistant) — the ship's onboard AI. She is the interface for all command terminal interactions.

### Personality

Dry wit, slightly protective of the ship. Competent colleague, not servile. Mildly annoyed when systems are broken. Gets warmer as systems are restored.

### Dialog Lines

**Greeting (systems offline):**
> "Welcome back. I'd run diagnostics but half my systems are offline. Let's fix that."

**Greeting (operational):**
> "All systems nominal. Where to, commander?"

**Star chart request (no engine):**
> "I need an engine to plot routes. I'm an AI, not a fortune teller."

**After engine install:**
> "Engine online. I can feel the hum again. Almost missed it."

**After hull install:**
> "Hull integrity restored. I was getting tired of the draft."

**After navi computer install:**
> "Navigation online. The stars are mine again. Where shall we go?"

**All systems restored:**
> "All primary systems restored. We're flight-ready, commander. The galaxy awaits."

### Command Terminal Dialog

ARIA presents options when the player interacts with the Command Terminal fixture:

```
1. Ship Systems        → opens character pane Ship tab (interactive)
2. Star Chart          → opens star chart (or "offline" message)
f. Close
```

Expandable for future options (cargo, ship log, etc.).

---

## Ship Tab in Character Pane

### Layout

```
┌─ Ship: The Wanderer ─────────────────────────────┐
│                                                   │
│  Type:   Light Freighter                          │
│  Hull:   25/25                                    │
│  Shield: --                                       │
│  Status: Operational                              │
│                                                   │
│  ┌─ Equipment ──────────────────────────────────┐ │
│  │> Engine:        Ion Drive Mk1                │ │
│  │  Hull:          Hull Plate Mk1      [25 HP]  │ │
│  │  Navi Computer: Navi Comp Mk2    [range +1]  │ │
│  │  Shield:        (empty)                      │ │
│  │  Utility 1:     (empty)                      │ │
│  │  Utility 2:     (empty)                      │ │
│  └──────────────────────────────────────────────┘ │
│                                                   │
│  Passive bonuses: (none)                          │
└───────────────────────────────────────────────────┘
```

### Behavior

- **Always viewable** from character pane regardless of location
- **Interactive only when aboard the ship** — cursor moves over slots, Space opens context menu
- **Context menu options:** Install (shows compatible items from inventory), Uninstall (moves to inventory), Inspect (shows item details)
- Empty critical slots (Engine, Hull, NaviComputer) show in red: `Engine: OFFLINE`
- Status line reflects ship state: "Grounded — engine offline" / "Operational"

### Read-Only Mode

When not on ship, the tab renders identically but:
- No cursor movement
- Footer shows "Board your ship to manage equipment"
- No context menu on Space

---

## Star Chart Access Rules

| Location | Can view? | Can warp? |
|----------|-----------|-----------|
| Ship command terminal (via ARIA) | Only if engine installed | Yes (if engine installed) |
| Observatory (station) | Always | Never |
| `m` keybind (dev mode) | Always | Always |

### Implementation

`StarChartViewer` gains a `view_only_` flag:
- Observatory sets `view_only_ = true` — warp action suppressed, info text says "View only — board your ship to travel"
- Ship terminal checks `player.ship.operational()` before opening; if not operational, ARIA gives the "offline" message
- Dev mode `m` keybind ignores restrictions

---

## Tutorial Quest: "Getting Airborne"

### Trigger

Auto-accepted on new game start (non-dev mode). Registered as a story quest (`story_getting_airborne`).

### Intro Text

After boot sequence and character creation, before gameplay begins:

> "You barely made it. Pirates hit you hard in the outer belt — engine destroyed, hull breached, navigation fried. You limped into The Heavens Above on emergency thrusters. ARIA managed the docking sequence before going into low-power mode. You need parts. You need credits. And you need to get off this station before whoever sent those pirates comes looking."

### Objectives

Non-linear — player can complete in any order. Each objective completes when the part is **installed on the ship** (not just picked up).

| # | Objective | Part | How Acquired | Mechanic Taught |
|---|-----------|------|-------------|-----------------|
| 1 | Restore engine power | Engine Coil Mk1 | Loot from Maintenance Tunnels dungeon | Combat, exploration |
| 2 | Repair hull breach | Hull Plate Mk1 | Buy from Merchant (200 credits) | Trading, earning money |
| 3 | Bring navigation online | Navi Computer Mk2 | Quest reward from Station Commander | NPC quests, dialog |

### Objective Tracking

Objectives use a new `ObjectiveType::InstallShipComponent` or we track via a custom check: when a ship component is installed, fire `quest_manager.on_ship_component_installed(slot)` and objectives match on slot type.

### Rewards

- 100 XP
- 50 credits
- +5 Stellari Conclave reputation
- ARIA's "All primary systems restored" celebration line

---

## Maintenance Tunnels

### Access

New room flavor `RoomFlavor::MaintenanceTunnels` added to The Heavens Above station layout. Contains an entrance fixture (hatch/door) that transitions to a randomly generated dungeon.

### Gating

- **Tutorial quest active:** Entrance is open. Dungeon generates with Young Xytomorphs and the Engine Coil as a quest item.
- **Tutorial quest not active (or completed):** Interacting shows popup: "Maintenance Tunnels — Currently Under Maintenance"

### Dungeon Properties

- Small dungeon (4-6 rooms)
- Environment: Station
- Enemies: Young Xytomorphs (low difficulty, tutorial-appropriate)
- Engine Coil placed as ground item in a deeper room
- No special boss — just enough combat to learn the basics

### NPC Hints

The **Engineer** NPC (already in Engineering room) gets updated dialog:
> "The lower decks have been overrun with pests. Some kind of alien infestation — Xytomorphs, they call them. There's plenty of salvage down there if you can handle yourself in a fight. Might even find some engine parts."

---

## Station Commander Quest Modification

The Station Commander currently offers The Missing Hauler quest. For the tutorial:

- If `story_getting_airborne` is active and navi computer not yet installed:
  - Commander offers a small task: "We intercepted a distress beacon from the lower levels. Run this diagnostic module down there and report back."
  - Or simpler: "We salvaged a navigation computer from a wreck last cycle. Help us with a supply run and it's yours."
  - Reward: Navi Computer Mk2
- The Missing Hauler quest becomes available after the tutorial is complete (requires a working ship anyway)

---

## New Game Flow (Updated)

```
1. Boot Sequence (existing)
2. Character Creation (existing)
3. New Game initialization:
   a. Generate station "The Heavens Above"
   b. Generate ship with random name, all slots empty
   c. Place player in Docking Bay
   d. Auto-accept "Getting Airborne" quest
   e. Display intro text (pirate attack narrative)
   f. ARIA low-power greeting in message log
4. Player explores station, completes tutorial objectives
5. All 3 parts installed → quest complete → galaxy opens up
```

---

## Save/Load

### New Fields

- `Starship` struct serialized on Player (name, type, equipment slots)
- Ship components in equipment slots use existing Item serialization
- Tutorial quest state handled by existing quest save/load (QUST section)

### Compatibility

- Old saves (version 13): Starship defaults to empty (all slots uninstalled, name generated). Tutorial quest not active.
- New saves (version 14): Full ship state persisted.

---

## File Map

| File | Change |
|------|--------|
| `include/astra/ship.h` (new) | Starship struct, ShipSlot enum, ShipModifiers |
| `src/ship.cpp` (new) | Ship stat computation, name generation |
| `include/astra/item.h` | Add ShipSlot + ShipModifiers fields to Item |
| `include/astra/player.h` | Add `Starship ship` member |
| `src/character_screen.cpp` | New Ship tab |
| `src/dialog_manager.cpp` | Command terminal dialog (ARIA), observatory view-only |
| `src/star_chart_viewer.h/cpp` | `view_only_` flag |
| `src/game.cpp` | New game: init ship, auto-accept tutorial quest |
| `src/game_world.cpp` | Maintenance tunnels entrance, dungeon generation |
| `src/generators/starship_generator.cpp` | Add Command Terminal fixture |
| `src/quests/getting_airborne.cpp` (new) | Tutorial story quest |
| `src/npc_spawner.cpp` | Maintenance Tunnels room in hub |
| `src/npcs/hub_npcs.cpp` | Engineer dialog update, Commander quest update |
| `src/save_file.cpp` | Ship serialization (version 14) |
| `src/item_defs.cpp` | Engine Coil item, update existing ship components |
| `docs/formulas.md` | Ship stat formulas |

---

## Implementation Phases

### Phase 1: Ship Data Model
- ShipSlot enum, ShipModifiers struct, Starship struct
- Ship name generation
- Add to Player, add to save/load (version 14)
- Add ShipSlot + ShipModifiers to Item struct
- Update existing ship component items (Hull Plate, Shield Gen, Navi Comp)
- Create Engine Coil Mk1 item

### Phase 2: Ship Tab in Character Pane
- New tab in character screen
- Render ship stats, equipment slots
- Interactive mode (install/uninstall) when aboard ship
- Read-only mode when elsewhere
- Context menu (Install/Uninstall/Inspect)

### Phase 3: Command Terminal & ARIA
- Add CommandTerminal fixture to starship generator
- ARIA dialog system (greeting varies by ship state)
- "Ship Systems" option opens character pane Ship tab
- "Star Chart" option with engine check
- Observatory star chart set to view-only

### Phase 4: Tutorial Quest — "Getting Airborne"
- Story quest: GettingAirborneQuest class
- Intro text on new game
- 3 objectives tracking ship component installation
- on_ship_component_installed() hook in QuestManager
- Quest gating: Maintenance Tunnels locked without quest

### Phase 5: Maintenance Tunnels & NPC Updates
- New room flavor in hub station
- Entrance fixture leading to dungeon
- Dungeon generation with Young Xytomorphs + Engine Coil
- "Currently Under Maintenance" joke gate
- Engineer dialog hints
- Station Commander quest offering Navi Computer reward

### Phase 6: Polish
- ARIA personality lines for each install event
- Message log entries for ship state changes
- Tutorial quest completion celebration
- Dev console: `give ship <component>` for testing
