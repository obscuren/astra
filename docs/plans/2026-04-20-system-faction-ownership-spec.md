# System Faction Ownership — Design Spec

**Date:** 2026-04-20
**Status:** Spec — ready for plan
**Related:**
- Follows the Stage 4 EventBus slice (`2026-04-20-stage4-hostility-event-bus.md` → merged)
- Blocks Stage 4 quest scaffolding (gauntlet quest needs Conclave-space gating)
- Referenced by `scenario_graph_vision.md` (future emergent faction-war scenarios)

---

## Problem

Stage 4's hostility scenario currently spawns Conclave ambushes in **every** fresh system the player warps to. Narratively it should only fire in Conclave-controlled space — but the game has no concept of a system being faction-owned. `StarSystem` tracks star class, lore tier, precursor civ annotations, and station type, but not a living-faction owner.

This is missing infrastructure, not just a Stage 4 concern. Pirate territory, Kreth Mining zones, future faction-war scenarios, and any "am I in safe space?" signaling all want it.

## Goals

1. Every `StarSystem` has a `controlling_faction` — one of {Stellari Conclave, Terran Federation, Kreth Mining Guild, Veldrani Accord, Unclaimed}.
2. Assignment is clustered around procedurally-placed "capitals" so the galaxy has legible territorial bands, not dotted pointillism.
3. Sol is pinned to Terran Federation (humans' homeworld). All other capitals fall from the galaxy seed.
4. ~10% of territory-owned systems are enclaves of a different faction (noise / variety).
5. Assignment is deterministic from `galaxy_seed` — no save-format impact.
6. Galaxy-view renderer paints faction territory as muted background tints. Stars keep their stellar-class foreground colors. A keybind toggles the band-fill layer on/off (default on).
7. Hover popup on the galaxy chart names the faction.
8. Stage 4 hostility scenario only ambushes in `Faction_StellariConclave` systems.

## Non-goals

- **Station-level faction ownership.** `StationInfo::operating_faction` is not added. Heavens Above is modeled as "a station in Sol" (Terran territory); Nova's individual NPC faction remains Conclave — that's enough. If a future feature needs station-level factions (e.g. an embassy scenario), we add it then with a concrete driver.
- **Mutable ownership.** Immutable for this slice. Faction wars / conquest / shifting borders are deferred; a runtime override layer is designed when its first consumer appears.
- **Region / minimap view tinting.** Galaxy view only. Region / local / detail views unchanged.
- **Shop or dialog gating by system faction.** Existing NPC-level faction checks continue to work — no system-level gating in this slice.
- **Warning text on territorial entry.** "You've entered Conclave space" is not shown. The Stage 4 transmission already gives narrative cover; generic territorial messaging can land later if desired.
- **Faction capital naming or lore writeups.** Capitals are placed but unnamed as distinct entities; no lore records generated.

---

## Data model

### 1. `StarSystem::controlling_faction`

```cpp
struct StarSystem {
    // ... existing fields ...
    std::string controlling_faction;   // "" = Unclaimed; else one of Faction_* constants
};
```

Empty string encodes "Unclaimed" (no sentinel constant needed). Reads route through a helper so future code isn't sprinkled with `.empty()` checks.

### 2. `CustomSystemSpec::controlling_faction`

```cpp
struct CustomSystemSpec {
    // ... existing fields ...
    std::optional<std::string> controlling_faction;   // nullopt = fall through to territorial assignment
};
```

`nullopt` means "compute like a normal system" (based on nearest capital). A caller can explicitly set `""` to force Unclaimed (e.g. Stage 3 beacon: "deep beyond charted space"). An explicit non-empty value is taken verbatim.

### 3. Helper functions (new header `include/astra/faction_territory.h`)

```cpp
namespace astra {

// Compute faction territory for every system in the navigation state.
// Idempotent: may be called multiple times; later calls overwrite.
// Deterministic for a given galaxy_seed.
void assign_system_factions(NavigationState& nav, uint64_t galaxy_seed);

// Cheap accessor, reads the field directly.
inline const std::string& controlling_faction(const StarSystem& s) {
    return s.controlling_faction;
}

// True if the system has no owning faction.
inline bool is_unclaimed(const StarSystem& s) {
    return s.controlling_faction.empty();
}

// Returns the faction that controls the galaxy-space coord (gx, gy), regardless
// of whether a StarSystem exists there. Used by the renderer to tint empty
// space between stars. Implemented via a precomputed grid for speed.
std::string faction_at_coord(const NavigationState& nav, float gx, float gy);

} // namespace astra
```

The rendered `faction_at_coord` is the load-bearing one for the band-fill UI. Implementation detail below.

---

## Generation algorithm

Invoked once during galaxy construction, after all `StarSystem` entries exist with their `gx/gy` coords populated.

### Capital budget

| Faction | Capitals | Notes |
|---|---|---|
| Stellari Conclave | 3 | Dominant power — three zones of influence |
| Terran Federation | 2 | Sol (pinned) + 1 procedural |
| Kreth Mining Guild | 1 | Industrial belt region |
| Veldrani Accord | 1 | Mid-tier power |
| **Total** | **7** | Out of ~hundreds of systems |

### Placement

1. **Pin Sol.** System with `id == 1` (or name "Sol") is hard-assigned as a Terran capital before any procedural placement.
2. **Procedural rejection sampling** for the remaining 6 capitals. For each:
   - Pick a random system uniformly (from the already-generated set, excluding existing capitals).
   - Check min-distance constraint: distance to the *nearest* existing capital must be ≥ `K_CAPITAL_MIN_DIST` (proposed: 40 galaxy units — tune during implementation if the sprawl feels wrong).
   - Accept or retry. Hard cap the retry count (e.g. 500) to guarantee termination; if the cap hits, relax the min-distance by 10% and continue.
3. RNG is seeded from `galaxy_seed` so placement is deterministic per seed.

### System-to-faction assignment

For each non-capital system:

1. Compute distance to the nearest capital.
2. If distance ≤ `K_INFLUENCE_RADIUS` (proposed: 35 galaxy units), assign that capital's faction.
3. Else leave as `""` (Unclaimed).

### Noise pass — enclaves

For each territory-owned system (`controlling_faction != ""`), with probability `K_NOISE_RATE` (proposed: 10%), re-roll the faction:

- 80% chance: swap to another random territorial faction.
- 20% chance: swap to Unclaimed.

Unclaimed systems are *not* subject to noise — wilderness stays wilderness.

RNG for this pass is seeded deterministically from `(galaxy_seed ^ system_id)` so per-system enclaves are stable across runs.

### Custom system handling

A `CustomSystemSpec` with `controlling_faction.has_value()` uses the provided string verbatim (including `""` for explicit Unclaimed). With `nullopt`, the system is run through the normal capital-distance logic and noise pass like any other.

Stage 3 beacon system: explicitly assigned `""` (Unclaimed) at `add_custom_system` call site.

---

## Rendering

### Band-fill layer

Rendered as a **background color per galaxy-map cell** in `star_chart_viewer.cpp`'s galaxy view. Stars render on top with their normal stellar-class foreground colors, unchanged.

Color palette (256-color ANSI indexes — all deliberately muted so they don't fight the stars):

| Faction | BG index | Rough appearance |
|---|---|---|
| Stellari Conclave | 53 | Dim magenta |
| Terran Federation | 17 | Dim blue |
| Kreth Mining Guild | 58 | Dim olive / bronze |
| Veldrani Accord | 22 | Dim teal |
| Unclaimed | 0 (black) | Default — visible gap between territories |

Exact palette indexes are tuning targets; adjust during implementation if any read as too bright against stars.

### Coord → faction lookup

A naive "compute nearest capital per cell per frame" is expensive (capitals count is small — 7 — but the galaxy view has thousands of cells and redraws on interaction).

**Implementation:** pre-build a `FactionMap` owned by `NavigationState` (new field alongside `systems`) during `assign_system_factions`. It's a 2D `std::vector<uint8_t>` sized to the galaxy-view grid, each cell holding a small faction enum index (0=Unclaimed, 1=Conclave, 2=Terran, 3=Kreth, 4=Veldrani). `faction_at_coord(nav, gx, gy)` does one O(1) lookup on the grid. Rebuild only when galaxy is (re)generated. Like `controlling_faction`, the map itself is not serialized — regenerated on load alongside faction assignment.

Size bound: galaxy view is on the order of 200×200 cells → 40 KB for a byte-per-cell lookup. Trivial.

### Toggle

- Keybind: `F` (mnemonic: **F**action view). Confirm during implementation that `F` isn't already bound in the galaxy viewer; fallback `V` if it is.
- Default state: **on**. Territory is visible from the first moment the player opens the galaxy chart.
- Toggle state is session-local — not persisted across runs. Re-opening the chart retains the current session's setting; new session starts with default on.
- When off: galaxy view renders with default black backgrounds (current behavior). Stars unchanged.

### Hover popup

The existing galaxy-hover popup (system name + lore tier) gains one line:

```
Faction: Stellari Conclave
```

or `Unclaimed space` if the system has no owner. No popup for empty cells (non-system coords).

---

## Stage 4 scenario integration

`src/scenarios/stage4_hostility.cpp` handler becomes:

```cpp
if (!world.world_flag(kStage4Active)) return;
if (world.ambushed_systems().count(payload.system_id)) return;

// Locate the system via whatever lookup the nav state already offers
// (nav.systems scan or existing helper — implementation detail).
const StarSystem* sys = find_system_by_id(nav, payload.system_id);
if (!sys) return;

// Transmission fires on first post-Stage-3 warp regardless of location.
if (!world.world_flag(kTransmissionSeen)) {
    open_transmission(g, "INCOMING TRANSMISSION — STELLARI CONCLAVE", {...});
    set_world_flag(g, kTransmissionSeen, true);
}

// Ambushes only in Conclave space.
if (sys->controlling_faction != Faction_StellariConclave) return;

// ... existing ambush injection ...
world.ambushed_systems().insert(payload.system_id);
```

Narrative consequence: the player gets the transmission immediately after Stage 3 (on their next warp, wherever), but ambushes start only when they actually cross into Conclave-controlled territory. The Conclave is telling them off from afar; the enforcement comes when you're in their space.

---

## Persistence

**None.** `controlling_faction` is not serialized. On save load:

1. Existing `galaxy_seed` is restored from save file (already persisted via `NavigationState`).
2. All `StarSystem` entries are deserialized with `controlling_faction = ""` (not in save file).
3. After load, `assign_system_factions(nav, galaxy_seed)` is called to fill the field.

No save format version bump.

**Verification:** identical `galaxy_seed` across sessions must produce identical `controlling_faction` values for every system (determinism). Covered by manual smoke test: save, record 5 system factions, reload, confirm all five match.

---

## Open decisions / tuning

These are **default values encoded in the plan** — adjustable during implementation if playtest shows issues:

| Constant | Default | Why this value |
|---|---|---|
| `K_CAPITAL_MIN_DIST` | 40 galaxy units | Roughly separates the four powers cleanly across a galaxy of the current scale |
| `K_INFLUENCE_RADIUS` | 35 galaxy units | Slightly less than min-distance so some wild-space buffer exists between empires |
| `K_NOISE_RATE` | 10% | Enough enclaves to feel varied, not so many that territory bands blur |
| Toggle keybind | `F` | If taken, fall back to `V` |
| Default toggle state | On | Territory is the new information; show it by default |

---

## Testing plan

No test framework exists — validation is build + manual smoke tests.

1. **Build** clean with `cmake --build build -j`.
2. **Deterministic gen**: run game, note `controlling_faction` of 5 specific systems via dev console, save, quit, reload, confirm identical.
3. **Sol is Terran**: check Sol's faction == `Faction_TerranFederation` on every new game, regardless of seed.
4. **Galaxy view renders bands**: open galaxy chart, verify each of the 4 faction colors visible across the map, Unclaimed space stays black.
5. **Toggle works**: press `F` (or fallback), bands disappear; press again, bands return.
6. **Hover shows faction**: hover over a system with a known faction, verify popup line.
7. **Stage 4 gating**: complete Stages 1–3, warp to a Terran system → transmission fires (if first warp) but NO ambush. Warp to a Conclave system → ambush spawns.
8. **Beacon system is Unclaimed**: enter Stage 3 beacon system, verify its popup reads "Unclaimed space."

---

## Rollback / risk

- **Generation determinism bug**: if `assign_system_factions` produces different output across runs for the same seed, save/load will desync visually (faction colors on galaxy view change after reload). Mitigation: strict seeded RNG, no use of global random, deterministic iteration order (vector by index, not map by hash).
- **Capital placement starvation**: a very small galaxy could fail to place 7 capitals at min-distance. Mitigated by retry cap + auto-relax (10% radius shrink). Hard floor: if still failing after 3 relaxations, abandon remaining capitals (could end up with 6/7 or 5/7 — acceptable degradation).
- **Performance on galaxy-view redraw**: precomputed `FactionMap` grid is essential. Naive per-cell nearest-capital lookup would stall UI.
- **Rendering read as noise vs. territory**: if the bg tints read too vividly against stars, toggle to off and fix palette indexes before considering the design broken.

---

## Implementation slice

One plan, ~10 tasks, estimated to be similar in size to the EventBus slice we just shipped:

1. Add `controlling_faction` field to `StarSystem` and `CustomSystemSpec`
2. Create `faction_territory.h/cpp` with `assign_system_factions` (pin Sol, procedural capitals, assignment, noise)
3. Call `assign_system_factions` from galaxy generation and post-load paths
4. Create `FactionMap` precompute + `faction_at_coord` lookup
5. Galaxy-view renderer: background tint layer
6. Toggle keybind + session state
7. Hover popup faction line
8. Stage 4 scenario: gate ambushes on `controlling_faction == Faction_StellariConclave`
9. Explicitly set Stage 3 beacon system to Unclaimed
10. Roadmap + gap-analysis docs update

Handoff to the `writing-plans` skill for detailed task breakdown.
