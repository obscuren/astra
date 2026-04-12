# Reputation-Driven Hostility & NPC Combat

**Date:** 2026-04-12
**Status:** Approved

## Overview

Replace the hardcoded NPC `Disposition` system with reputation-driven hostility. NPCs become hostile when faction reputation drops to Hated (≤ -300). The same system drives NPC-vs-NPC combat — factions that hate each other will fight. This creates emergent tactical situations where the player can exploit inter-faction conflict.

## Goals

- Hostility is **always derived** from reputation, never stored
- Same hostility logic for player-vs-NPC and NPC-vs-NPC
- Single, tweakable faction standings table drives all inter-faction relationships
- Foundation for future combat overhaul (damage formulas unchanged for now)

## Non-Goals

- Combat system overhaul (comes later)
- Dynamic faction standings that shift over time (data structure supports it, but values are static for now)
- Reputation gain from kills (deferred)

---

## 1. Remove Old Disposition System

Delete entirely:
- `Disposition` enum from `npc.h`
- `disposition_` field from NPC struct
- All hardcoded disposition assignments in NPC builders (`hub_npcs.cpp`, `xytomorph.cpp`, `merchant.cpp`, `station_keeper.cpp`, etc.)
- Disposition from save/load serialization

Replace every check against `disposition_` with a call to `is_hostile()`.

## 2. New Reputation Scale

**Range:** -600 to +600

| Tier | Range |
|------|-------|
| Hated | ≤ -300 |
| Disliked | -299 to -60 |
| Neutral | -59 to 59 |
| Liked | 60 to 299 |
| Trusted | ≥ 300 |

**Hostility threshold:** Hated (≤ -300).

**Save migration:** Bump save version. On load, multiply old reputation values by 6 (old range was -100/+100).

## 3. Factions

10 factions total:

### Race-Aligned (5)

| Faction | Associated Race | Role |
|---------|----------------|------|
| Stellari Conclave | Stellari | Station operators, scientists, diplomats |
| Kreth Mining Guild | Kreth | Traders, arms dealers, resource extractors |
| Veldrani Accord | Veldrani | Diplomats, traders |
| Sylphari Wanderers | Sylphari | Nomadic wanderers |
| Terran Federation | Human | Human government |

### Hostile (4)

| Faction | Nature | Notes |
|---------|--------|-------|
| Xytomorph Hive | Intelligent hostile | Alien predators, hostile to all |
| Void Reavers | Intelligent hostile | Pirates/raiders, hostile to most |
| Archon Remnants | Non-intelligent hostile | Ancient automated defenses gone haywire |
| Feral | Non-intelligent hostile | Wild space fauna — still in reputation system so future pacifier items can work |

### Neutral (1)

| Faction | Notes |
|---------|-------|
| The Drift Collective | Scavengers, nomads, neutral toward most |

## 4. Faction Standings Table

A single data definition that maps `(faction_a, faction_b) → reputation`. Symmetric — if A has -400 toward B, B has -400 toward A. Stored once per pair.

This table is the **single source of truth** for all inter-faction relationships. To tweak any relationship, change the number in this table.

### Default Values

```
                  Stellari  Kreth  Veldrani  Sylphari  Terran  Xytomorph  Reavers  Archon  Feral  Drift
Stellari             —        0      100       50       50      -400      -350     -400    -400     0
Kreth                0        —        0      -50       50      -400      -350     -400    -400     0
Veldrani           100        0        —      100      100      -400      -300     -400    -400    50
Sylphari            50      -50      100        —        0      -400      -350     -400    -400    50
Terran              50       50      100        0        —      -400      -350     -400    -400     0
Xytomorph         -400     -400     -400     -400     -400        —       -400     -400    -400  -400
Void Reavers      -350     -350     -300     -350     -350      -400        —      -400    -400  -300
Archon            -400     -400     -400     -400     -400      -400      -400       —     -400  -400
Feral             -400     -400     -400     -400     -400      -400      -400     -400      —   -400
Drift                0        0       50       50        0      -400      -300     -400    -400     —
```

Design notes:
- **Veldrani** are diplomats — slightly positive with most race factions
- **Kreth and Sylphari** have mild tension (-50) — industrialists vs nature wanderers
- **Void Reavers** are Hated by all but just barely by Veldrani and Drift (-300) — room for future diplomacy
- **Xytomorphs, Archon, and Feral** hate everyone deep into Hated territory (-400)
- **Drift Collective** is Neutral to all civilized factions

### Storage

- Defined as a static initializer list / lookup table in code
- Persisted in save file for future dynamic changes
- Queried via `faction_standing(faction_a, faction_b) → int`

## 5. Hostility Query

```cpp
bool is_hostile(const std::string& faction_a, const std::string& faction_b) {
    if (faction_a.empty() || faction_b.empty()) return false;  // unaligned
    if (faction_a == faction_b) return false;                   // same faction
    return faction_standing(faction_a, faction_b) <= -300;
}
```

Used everywhere:
- **Player vs NPC:** `is_hostile_to_player(npc, player)` — checks `player.reputation` for the NPC's faction
- **NPC vs Player:** same check (symmetric — if you're Hated by their faction, they're hostile to you)
- **NPC vs NPC:** `is_hostile(npc_a.faction, npc_b.faction)` — checks faction standings table

## 6. Player Starting Reputation

- **+100** with the player's own race faction
- **0** with all civilized factions (Neutral)
- Hostile factions use their table defaults against the player's race faction (e.g., Xytomorph starts at -400 toward Stellari, so a Stellari player starts at -400 with Xytomorphs)

## 7. Combat Reputation Consequences

| Action | Reputation Change |
|--------|------------------|
| Kill a faction NPC | -30 with victim's faction |
| Kill an unaligned NPC | No change |
| Kill Xytomorph/Feral/Archon/Reaver | No change (for now) |

At -30 per kill starting from 0, it takes **10 kills** to cross from Neutral to Hated.

Rep gain from kills is deferred to a future update.

## 8. NPC AI Changes

### Target Selection (per NPC turn)

1. Scan visible entities within detection range (8 tiles) — both player and other NPCs
2. Filter to hostile using `is_hostile(my_faction, target_faction)` (for player, use `is_hostile_to_player`)
3. If already adjacent to and fighting a target, **continue that fight** (prioritize current engagement)
4. Otherwise, pick **nearest hostile** and chase/attack using existing greedy approach
5. Player is **deprioritized** — if an NPC is engaged with another NPC (adjacent, exchanged blows), they finish that fight before switching to player

### NPC-vs-NPC Damage

Same formula as NPC-vs-player: `base_damage * level + elite_bonus - target_defense`, with dodge checks.

No XP or loot for the player when NPCs kill each other (for now).

### Stationary NPCs

Shopkeepers, quest givers, and other non-combat NPCs remain stationary (quickness 0 or a `stationary` flag). They don't participate in combat even if factions around them are fighting.

### Fleeing

Intimidate flee effect still causes NPC to flee from player specifically — this is a player-targeted ability effect, not faction-based.

## 9. Rendering

NPC color is derived from hostility toward the player:

| Condition | Color |
|-----------|-------|
| `is_hostile_to_player(npc, player)` | Red |
| Reputation tier Disliked | Yellow |
| Reputation tier Neutral or above | Green |

Minimap: hostile = Red, others = Cyan (unchanged logic, new derivation).

## 10. Code Locations to Update

| Area | File(s) | Change |
|------|---------|--------|
| Disposition enum + field | `npc.h` | Remove |
| NPC builders | `hub_npcs.cpp`, `xytomorph.cpp`, `merchant.cpp`, `station_keeper.cpp` | Remove disposition assignment |
| Faction standings table | New: `faction.h` / `faction.cpp` | Add table, query functions |
| Reputation thresholds | `faction.cpp` | Update scale to -600/+600 |
| Hostility check | New function in `faction.h` | `is_hostile()`, `is_hostile_to_player()` |
| Bump-to-attack | `game_interaction.cpp` | Use `is_hostile_to_player()` |
| NPC interaction | `game_interaction.cpp` | Use `is_hostile_to_player()` |
| NPC AI / combat | `game_combat.cpp` | New target selection, NPC-vs-NPC |
| Rendering colors | `game_rendering.cpp` | Derive from reputation |
| Minimap | `minimap.cpp` | Derive from reputation |
| Auto-walk | `game_interaction.cpp` | Use `is_hostile_to_player()` |
| Abilities | `ability.cpp` | Use `is_hostile_to_player()` |
| Dev kill command | `game.cpp` | Use `is_hostile_to_player()` |
| Dialog manager | `dialog_manager.cpp` | Already uses reputation tier — verify |
| Save/load | `save_file.cpp` | Remove disposition, add faction table, migrate rep scale |
| Character screen | `character_screen.cpp` | Update tier thresholds, add new factions |
| Player starting rep | `game.cpp` | Race-based starting bonus, new factions |
| NPC spawner | `npc_spawner.cpp` | Assign new factions to NPCs |
| Kill consequences | `game_combat.cpp` | Add rep penalty on NPC kill |
