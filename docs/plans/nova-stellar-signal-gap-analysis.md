# Nova Arc "The Stellar Signal" — Feature Gap Analysis

**Date:** 2026-04-14
**Source design doc:** `/Users/jeffrey/dev/Unreal/lyra/nova-arc-the-stellar-signal.md` (authored by the in-game Nova persona)
**Purpose:** Catalog which systems the 5-stage Nova arc needs, what already exists in Astra, and what must be built.

---

## Arc summary

A 5-stage story arc given by Nova (Stellari Engineer, Observatory, The Heavens Above):

1. **Static in the Dark** — hook; accept the quest
2. **Three Echoes** — plant receiver drones in three systems
3. **The Beacon** — hidden system unlocks; play audio log
4. **The Echo Chamber** — Conclave turns hostile; station siege; flee to Io
5. **The Long Way Home** — Conclave Archive ruin on Io; three branching endings

Reward: legendary accessory **"Nova's Signal"** on Stage 3 completion. Stage 5 endings diverge into cycle-reset, cycle-break, or save-Nova-as-companion paths.

---

## EXISTS (usable as-is)

| Feature | Evidence |
|---|---|
| Quest DAG / arc chaining with prereqs | `include/astra/quest.h`, `src/quest_graph.cpp` |
| Nova NPC in Observatory with branching dialog | `src/npcs/nova.cpp`, `src/generators/hub_station_generator.cpp` |
| "Getting Airborne" tutorial quest (Stage 1 prereq) | `src/quests/getting_airborne.cpp` |
| Stellari Conclave / Archon Remnants / Void Reavers factions + rep tiers | `src/faction.cpp` |
| Dice combat: AV, STR penetration, type affinity, shields | `src/game_combat.cpp` |
| Precursor Linguist skill (dead-language examine) | `src/skill_defs.cpp` |
| Star chart with per-quest system markers | `src/star_chart.cpp`, `src/star_chart_viewer.cpp` |
| Procedural ruin generator, legendary rarity items | `src/generators/ruin_generator.cpp`, `src/item_gen.cpp` |
| Journal entries per quest | `src/journal.cpp` |
| Io present in star chart; ARIA ship-comms referenced | `src/star_chart.cpp`, dialog hooks |

## PARTIAL (needs extension)

- **Quest-item fixtures** — interactable fixture pattern exists; no "Receiver Drone" / "Signal Node" specific type with plant-on-interact behavior.
- **Lore viewer** — `src/lore_viewer.cpp` shows text; no multi-line "audio log" playback fixture / cinematic pacing.
- **Star chart hidden→revealed transition** — markers exist, but no mechanic for a system that is invisible until a quest stage unlocks it.
- **Archon Sentinel miniboss** — Archon Remnants faction exists, no Sentinel-grade NpcRole or builder.
- **Derelict station variant** — generator exists; not quest-taggable with flavor (broken comms array) or guaranteed content.
- **Scar-terrain biome** — lore references it; no red-dwarf scar biome in generator configs.

## MISSING (new systems required)

1. **Branching quest endings** with divergent persisted world state
2. **Companion NPC system** (Nova as follower, cross-run persistence)
3. **New Game+ / Sgr A\* rebirth** with carry-over unlocks (meta-save layer)
4. **Station siege state** — hostile fleet in orbit, station lockdown, NPC confinement
5. **Timed objectives** — race-against-clock quest mechanic (Stage 5 Ending C)
6. **Faction ambush events at jump points** triggered by reputation thresholds
7. **Neutron star system type** with no primary star + crystalline asteroid fixture
8. **Legendary accessory "Nova's Signal"** — `~` glyph, color 135, INT+3/WIL+2/HeatRes+10, passive: reveal 1 system / 50 turns
9. **Meta-unlock layer** — persistent lore entries, companion unlocks, legendary accessory carried to NG+
10. **Conclave Archive ruin variant** — Io-specific, high-tier, Precursor-themed
11. **Multi-line audio-log fixture** with line-by-line reveal pacing (shared with lore viewer refactor)

---

## Dependency ordering (rough)

Stages 1–3 only require the **PARTIAL** extensions:

- Receiver Drone / Signal Node fixtures
- Audio-log playback fixture
- Hidden-system reveal on quest stage
- Scar-terrain / neutron-star biome variants
- Archon Sentinel miniboss

Stages 4–5 require most of the **MISSING** list:

- Siege state + faction ambushes (Stage 4)
- Conclave Archive ruin variant (Stage 4/5)
- Branching endings + timed objective (Stage 5)
- Companion + NG+ + meta-unlock (Stage 5 rewards)
- Nova's Signal accessory (Stage 3 reward; needs meta layer only for NG+ carry)

A sensible build order is to ship Stages 1–3 first (small, mostly additive) and use the resulting content as the proving ground for the larger Stage 4–5 systems.

---

## Out of scope for this doc

Quest-content authoring (dialog lines, recording text, objective wording) is deferred to per-stage plans. This document only catalogs *systems* needed to make the arc runnable.
