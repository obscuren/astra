# Custom Star Systems & Reveal — Design

**Date:** 2026-04-14
**Status:** Draft — not yet implemented
**Companion:** `docs/plans/nova-stellar-signal-gap-analysis.md` (motivating use case: Nova arc Stage 3 hidden beacon system)

## Summary

Add a small API that lets a story quest create a custom star system on-demand and reveal or hide any system (custom or procedural) on the star chart. The existing `StarSystem::discovered` flag already drives chart rendering and is serialized; this spec adds the creation pathway (absent today) and a thin wrapper for reveal-by-id. A sibling `body_presets.h` header grows per-quest with small factory functions for common body archetypes (asteroid orbit, paradise planet, scar planet, neutron-crystal asteroid, …).

The motivating case is Nova's Stage 3 "beacon" system: unmapped at game start, created by the quest's `on_unlocked` with a single asteroid body, revealed when the stage begins. The same primitive serves any future arc that needs a specific system with specific bodies (a paradise arc, a follow-up scar-planet arc, …).

---

## Goals

- One-call custom system creation from a `StoryQuest` hook.
- Stable, collision-free ids via a dedicated 0x80000000+ counter persisted across save/load.
- Optional pre-filled bodies — caller composes `CelestialBody` entries from factory helpers or hand-constructs them.
- Uniform reveal path: `reveal_system(nav, id)` works on custom and procedural systems identically.
- No churn on existing NavigationData serialization — custom systems ride the existing per-system writer.

## Non-goals

- Forced body-generation overrides beyond the pre-fill mechanism.
- Reveal animation, "warp there" UX after reveal, or star-chart-state diff tracking.
- A body editor or runtime body-preset DSL — presets are C++ factory functions.
- Per-quest system lifecycle (cleanup on fail) — custom systems persist once created. Removal is out of scope (nothing on the Nova path needs it).

---

## Data Model

### `CustomSystemSpec` (new)

```cpp
struct CustomSystemSpec {
    std::string name;
    float gx = 0.0f;
    float gy = 0.0f;
    StarClass star_class = StarClass::ClassG;
    bool discovered = true;                   // created visible by default
    bool binary = false;
    bool has_station = false;
    LoreAnnotation lore = {};                 // optional — default empty
    std::vector<CelestialBody> bodies;        // empty = procedural on first access
};
```

Defaults chosen so the most common one-liner is short:

```cpp
uint32_t id = add_custom_system(nav, {
    .name = "Unnamed — Beacon",
    .gx = 48.3f, .gy = -71.2f,
    .star_class = StarClass::ClassM,
    .discovered = false,
    .bodies = { make_asteroid_orbit("Beacon Rock") },
});
```

### `NavigationData` extension

One new private field:

```cpp
uint32_t next_custom_system_id_ = 0x80000000u;
```

Public accessor is not required — allocation is internal to `add_custom_system`. Serialization adds this field (see Save/Load).

### `LoreAnnotation`

Already exists. Callers may pass a default-constructed value (no lore) or set `lore_tier` / flags as needed. The spec doesn't change `LoreAnnotation`.

---

## API

### `include/astra/star_chart.h`

```cpp
// Create a custom system and append it to nav.systems. Returns the allocated id.
// IDs are drawn from nav.next_custom_system_id_ (starts at 0x80000000) and
// incremented after each call; the counter survives save/load so ids remain
// unique across sessions.
//
// If spec.bodies is non-empty, the bodies are moved into the new system and
// bodies_generated is set to true (the lazy generator will not overwrite them).
// If spec.bodies is empty, bodies_generated stays false and the generator runs
// on first access, as with procedural systems.
uint32_t add_custom_system(NavigationData& nav, CustomSystemSpec spec);

// Set discovered=true for the system with this id. Returns false if the id
// is unknown. Works uniformly for custom and procedural systems.
bool reveal_system(NavigationData& nav, uint32_t system_id);

// Symmetry helper; currently no in-tree caller but trivial to maintain.
bool hide_system(NavigationData& nav, uint32_t system_id);
```

No other existing functions change signature. `generate_system_bodies` continues to early-return on `bodies_generated`, so pre-filled bodies are preserved.

### `include/astra/body_presets.h` (new)

Starts small; grows per quest:

```cpp
#pragma once

#include "astra/star_chart.h"   // CelestialBody, BodyType
#include <string>

namespace astra {

// Nova Stage 3: a bare asteroid hosting the beacon fixture on its detail map.
CelestialBody make_asteroid_orbit(std::string name);

} // namespace astra
```

The impl (`src/body_presets.cpp`) constructs a sensible `CelestialBody` with a single-asteroid profile — `BodyType::Asteroid`, `landable=true`, `has_dungeon=false`, dangerous=false, etc. Future quests add more presets in the same file.

---

## Integration

### Creation from a `StoryQuest`

Inside `NovaStellarSignalQuest::on_unlocked(Game& game)` (Stage 3 unlock trigger — follows Stage 2 completion):

```cpp
auto& nav = game.world().navigation();
uint32_t beacon_id = add_custom_system(nav, {
    .name = "Unnamed — Beacon",
    .gx = pick_unmapped_coords(nav).x,       // helper, quest-local
    .gy = pick_unmapped_coords(nav).y,
    .star_class = StarClass::ClassM,
    .discovered = true,                      // revealed immediately on stage unlock
    .bodies = { make_asteroid_orbit("Beacon Rock") },
});

// Mark the quest objective's target
// (Quest::target_system_id / target_body_index already render a chart marker)
stage3_goto_objective.target_system_id = beacon_id;
stage3_goto_objective.target_body_index = 0;

// Register the beacon fixture placement on the body's detail map
LocationKey k = {beacon_id, 0, -1, false, -1, -1, 0};
game.world().quest_locations()[k].fixtures.push_back(
    {"nova_beacon", -1, -1});   // resolver stamps on first entry
```

### Reveal an existing system

The companion case — e.g., when a quest wants to expose a procedural system that was otherwise hidden:

```cpp
reveal_system(game.world().navigation(), some_id);
```

### Dev-console

New subcommands under an existing top-level verb (pick `chart` if it's free, else nest under `lore`):

- `chart create <name>` — creates a small demo system offset from Sol with one asteroid, returns the id in the log. Useful to smoke-test reveal/hide and to test fixture placement on a custom body.
- `chart reveal <name>` — sets `discovered=true` on the first system matching `name` (substring match). Reports the id.
- `chart hide <name>` — inverse.

These mirror the existing `lore list` / `lore warp` style.

---

## ID Allocation & Collision Safety

Procedural ids are `seed ^ (index + 1) * 2654435761u`. Over a 1000-system galaxy that occupies a sparse subset of the 32-bit space; Sol (1) and Sgr A* (0) are reserved low ids. Custom ids are drawn from the counter starting at `0x80000000u` (increments per allocation). Collision with a procedural id is effectively impossible: the XOR hash for a procedural system with index ≥ 1 would have to land exactly on `0x80000000`, `0x80000001`, etc., which is vanishingly unlikely for any given seed and trivially resolved on-allocation if it ever happens (we could check and skip — see Failure Modes).

On save: `next_custom_system_id_` is serialized alongside `current_system_id`.
On load: the counter is restored; subsequent `add_custom_system` calls resume from wherever the last session left off. No reshuffling of existing ids.

---

## Save / Load

Bump the NAVA (or equivalent navigation) save section version. Two changes:

1. Serialize `next_custom_system_id_` after `current_system_id`.
2. No per-system changes — `StarSystem`'s existing serializer already covers `discovered`, `bodies`, `bodies_generated`, `lore`, `gx/gy`, etc. Custom systems ride this path identically.

Older saves (no counter on disk) read the counter as `0x80000000u` default — same as a fresh game that never created a custom system. Adding custom systems later continues to allocate from the counter; the existing `systems` vector just has whatever custom entries were written at save time.

---

## Failure Modes

- **Unknown id in `reveal_system` / `hide_system`**: returns `false`. Caller decides whether to log or ignore.
- **ID collision with procedural**: on `add_custom_system`, walk the counter forward until `std::find_if(systems, id==counter)` yields nothing. In practice this loop runs zero iterations.
- **Duplicate system name**: allowed. Names aren't unique in the galaxy today (procedural generator doesn't enforce). The dev-console `chart reveal <name>` uses first-match semantics.
- **Body pre-fill with `bodies_generated=false`**: caller error. `add_custom_system` always sets the flag correctly based on whether `spec.bodies` is empty — callers don't touch `bodies_generated`.

---

## File Map

| File | Kind | Responsibility |
|---|---|---|
| `include/astra/star_chart.h` | MODIFY | `CustomSystemSpec`, `add_custom_system`, `reveal_system`, `hide_system`; `next_custom_system_id_` member on NavigationData |
| `src/star_chart.cpp` | MODIFY | Implementations |
| `include/astra/body_presets.h` | NEW | Factory function declarations |
| `src/body_presets.cpp` | NEW | Factory function implementations (starting with `make_asteroid_orbit`) |
| `CMakeLists.txt` | MODIFY | Add `src/body_presets.cpp` |
| `src/save_file.cpp` | MODIFY | Serialize / read `next_custom_system_id_`; bump NAV section version |
| `include/astra/save_file.h` | MODIFY | Bump `SaveData::version` default |
| `src/dev_console.cpp` | MODIFY | `chart create / reveal / hide` subcommands |

---

## Implementation Checklist (for the forthcoming plan)

1. Define `CustomSystemSpec` in `star_chart.h`.
2. Add `next_custom_system_id_` member + serializer field.
3. Implement `add_custom_system`, `reveal_system`, `hide_system`.
4. Bump the save version; gate reads on version.
5. Create `body_presets.h` / `.cpp`; implement `make_asteroid_orbit`.
6. Add `src/body_presets.cpp` to CMake.
7. Add dev-console `chart` subcommands.
8. Smoke-test: `chart create Testville`, confirm new id reported, open star chart, system visible at the offset. `chart hide Testville` / `chart reveal Testville` to exercise the flip.

---

## Out of scope — explicitly deferred

- Removing custom systems (no quest needs it yet).
- Reveal animation / sound.
- "Warp to newly revealed" auto-action — lore warp path already exists.
- System name uniqueness or collision UX.
- Per-quest system cleanup on `fail_quest`.
- Exposing `next_custom_system_id_` publicly for callers that want to pre-allocate ids.
