# Quest Fixtures — Design

**Date:** 2026-04-14
**Status:** Draft — not yet implemented
**Companion:** `docs/plans/nova-stellar-signal-gap-analysis.md` (motivating use case: Nova arc Stage 2 receiver-drone planting)

## Summary

Add a generic **quest fixture** primitive: an interactable map fixture whose visuals, prompt, and quest-side effects are driven by a string id rather than hardcoded per quest. Interacting with a quest fixture fires a new `QuestManager::on_fixture_interacted(id)` hook and increments matching objectives of a new `ObjectiveType::InteractFixture`.

This is the minimum primitive needed to author Stages 1–3 of the Nova arc as data + a single `StoryQuest` subclass, with no per-quest changes to the fixture or interaction systems.

---

## Goals

- One new fixture type (`FixtureType::QuestFixture`) that any story quest can place without touching `dialog_manager.cpp`.
- Per-fixture glyph/color/prompt/log-line authored alongside the quest, looked up by id.
- Deferred placement: quests declare *that* a fixture goes in a location, the world injector picks *where* on first entry.
- Round-trips through save/load.
- Auto-cleanup tied to the existing location-level `remove_on_completion` flag.

## Non-goals

- Per-fixture `remove_on_completion` (location-level only).
- Before/after visual state baked into the primitive (quests swap ids if they need it).
- Placement hints (`NearCenter`, `OnOrbit`, `InRoom`) — first open spot only in this revision.
- Storing callbacks (`std::function`) on `FixtureData` — breaks save/load.
- Cooldowns or multi-use semantics — quest fixtures are single-purpose; the quest decides what subsequent interactions do.

---

## Components

### 1. Quest fixture registry

New header `include/astra/quest_fixture.h`, impl `src/quest_fixture.cpp`.

```cpp
struct QuestFixtureDef {
    std::string id;            // "nova_signal_node_echo1"
    char glyph = '?';
    int color = 7;             // terminal color index
    std::string prompt;        // shown on hover/interact, e.g. "Plant receiver drone"
    std::string log_message;   // optional line written to game log on interact ("" = none)
};

void register_quest_fixture(QuestFixtureDef def);
const QuestFixtureDef* find_quest_fixture(const std::string& id);
void clear_quest_fixtures();   // tests / reload
```

Storage: `static std::unordered_map<std::string, QuestFixtureDef>` in the `.cpp`. Registration is idempotent — re-registering the same id overwrites (last wins; warning logged in dev builds).

**Population timing.** Defs are registered via a new `StoryQuest::register_fixtures()` virtual called once per quest at story-quest catalog build (every game start, including after load). This guarantees defs exist whenever a save is restored — `restore()` does not re-fire `on_accepted`, so registration cannot live there. Defs are content, not state, so they're cheap to repopulate on every boot and do not need to be saved.

```cpp
class StoryQuest {
    // existing virtuals ...
    virtual void register_fixtures() {}   // called from build_catalog()
};
```

Quests that compute fixture content from runtime state (rare) can register a placeholder def at boot and overwrite it later — `register_quest_fixture` is idempotent.

### 2. `FixtureData` extension

Add to `FixtureData` (in `include/astra/tilemap.h`):

```cpp
std::string quest_fixture_id;  // empty for non-quest fixtures
```

Add to `enum class FixtureType`:

```cpp
QuestFixture,
```

Constructor leaves `quest_fixture_id` empty. The new field is serialized as part of the existing fixture serialization (length-prefixed string).

### 3. Rendering

`fixture_glyph(FixtureType)` in `terminal_theme.cpp` returns a placeholder (`?`) for `QuestFixture`. The renderer (or whatever calls `fixture_glyph` today) must check: if `fixture.type == QuestFixture && !fixture.quest_fixture_id.empty()`, look up the def via `find_quest_fixture(id)` and use the def's glyph + color instead.

If the def is missing (catalog drift, save from older build), fall back to the placeholder glyph and a default color so the world stays renderable. Log a warning once per missing id.

### 4. Placement via `QuestLocationMeta`

Extend `QuestLocationMeta` (in `include/astra/world_manager.h`):

```cpp
struct QuestFixturePlacement {
    std::string fixture_id;   // registry key
    int x = -1;               // -1 = unresolved (deferred placement)
    int y = -1;
};

// new field on QuestLocationMeta:
std::vector<QuestFixturePlacement> fixtures;
```

**Resolution.** On first entry to the location (existing `enter_detail_map` / `enter_dungeon_from_detail` hooks in `src/game_world.cpp`, the same place that already spawns quest NPCs and items):

```
for each placement in meta.fixtures:
    if placement.x >= 0 and a fixture already exists at (x,y) on this map:
        skip (already placed in a previous session)
    else:
        find an open floor tile not already occupied by a quest entity
        add a FixtureData{ type=QuestFixture, interactable=true, quest_fixture_id=placement.fixture_id }
        write the chosen (x,y) back into placement
```

The placement update mutates `WorldManager::quest_locations_[key]`, so subsequent saves capture the resolved coords.

**Spot selection.** Reuse the existing helper used for quest NPCs (`find_open_spot_other_room` or equivalent — picked by the implementer at plan time). Avoid stacking multiple quest fixtures on the same tile.

### 5. Interaction hook

In `src/dialog_manager.cpp::interact_fixture()`, add a case for `FixtureType::QuestFixture`:

```cpp
case FixtureType::QuestFixture: {
    const QuestFixtureDef* def = find_quest_fixture(f.quest_fixture_id);
    if (def && !def->log_message.empty()) {
        game.log(def->log_message);
    }
    game.quest_manager().on_fixture_interacted(f.quest_fixture_id);
    break;
}
```

No removal, no cooldown, no state change inside the dispatcher. The quest decides what (if anything) to do next via `on_fixture_interacted`.

### 6. Quest manager hook + new objective type

In `include/astra/quest.h`:

```cpp
enum class ObjectiveType : uint8_t {
    KillNpc,
    GoToLocation,
    CollectItem,
    TalkToNpc,
    DeliverItem,
    InteractFixture,   // NEW
};

class QuestManager {
    // existing on_* hooks ...
    void on_fixture_interacted(const std::string& fixture_id);
};
```

Implementation mirrors `on_npc_killed`: walk active quests, for each objective with `type == InteractFixture && target_id == fixture_id`, increment `current_count` and check completion.

### 7. Cleanup on completion / failure

In `complete_quest` and `fail_quest`, after the existing reward / state-transition logic:

```
for each (key, meta) in world.quest_locations_ where meta.quest_id == this quest:
    if meta.remove_on_completion:
        for each placement in meta.fixtures with x >= 0:
            if the map for `key` is currently loaded:
                map.remove_fixture_at(placement.x, placement.y)
            else:
                mark for removal on next entry (see below)
        erase quest_locations_[key]
```

**Pending removal for unloaded maps.** Keep a small `std::set<LocationKey> pending_quest_cleanup_` on `WorldManager`. When a map is loaded, the entry hook drains its corresponding entry and removes the listed fixtures before the player sees the map. Serialized with the rest of `WorldManager`.

NPCs and items spawned by the same `QuestLocationMeta` already follow analogous cleanup; this only adds the fixture branch.

### 8. Save / load

Bump the QUST save section version (current is 13 per existing spec; this becomes 14).

- `FixtureData.quest_fixture_id` — write/read as a length-prefixed string in the existing fixture serializer. Old saves (no field) read empty.
- `QuestLocationMeta.fixtures` — write/read a `u32` count followed by `{ string fixture_id; i32 x; i32 y; }` per entry. Old saves read zero.
- `WorldManager.pending_quest_cleanup_` — write/read a count + LocationKey tuples.

The registry itself is **not** saved. Defs are repopulated on every load by the story-quest catalog. If a save references a `quest_fixture_id` whose def is gone (mod / catalog shrink), the fixture renders with the fallback glyph and a one-time warning; interaction still calls `on_fixture_interacted` (the id is preserved on disk).

### 9. Validation

Extend `quest_validator.cpp` startup checks:

- Any `QuestObjective` with `type == InteractFixture` must have a non-empty `target_id`. (Optional warning if no `QuestFixtureDef` is registered for that id at validate time — defs may be registered later by `on_accepted`, so this is a warning, not an error.)

---

## Authoring Example — Nova Echo 1

Inside `NovaStellarSignalQuest::register_fixtures()` (called at catalog build, every boot):

```cpp
register_quest_fixture({
    "nova_signal_node_echo1",
    '*',                       // glyph
    135,                       // Stellari resonance color
    "Plant receiver drone",
    "You plant the drone. It hums against the silence.",
});
// ...register echo2, echo3 defs the same way
```

Inside `NovaStellarSignalQuest::on_accepted(Game& game)` (fires when the player accepts the quest):

```cpp
QuestLocationMeta meta;
meta.quest_id = "nova_stellar_signal_stage2";
meta.quest_title = "Three Echoes";
meta.remove_on_completion = false;   // leave drones as breadcrumbs
meta.fixtures.push_back({"nova_signal_node_echo1", -1, -1});
game.world().quest_locations()[echo1_key] = std::move(meta);
```

The objective in `create_quest()`:

```cpp
q.objectives.push_back({
    ObjectiveType::InteractFixture,
    "Plant the receiver drone in the Fire-Worn system",
    1, 0,
    "nova_signal_node_echo1",
});
```

No changes to `dialog_manager.cpp`, `game_world.cpp`, or any rendering code per quest. Adding Echoes 2 and 3 is three more registrations and three more meta entries.

---

## File Map

| File | Change |
|---|---|
| `include/astra/quest_fixture.h` | NEW — `QuestFixtureDef`, register/find/clear |
| `src/quest_fixture.cpp` | NEW — registry storage |
| `include/astra/tilemap.h` | Add `FixtureType::QuestFixture`, `FixtureData::quest_fixture_id` |
| `src/tilemap.cpp` | Serialize new field; `remove_fixture_at(x,y)` if not present |
| `src/terminal_theme.cpp` | Placeholder glyph for `QuestFixture` |
| Renderer call sites | Look up registry def when rendering `QuestFixture` |
| `include/astra/world_manager.h` | `QuestFixturePlacement`, `fixtures` field, `pending_quest_cleanup_` |
| `src/world_manager.cpp` | Serialize pending cleanup set |
| `src/game_world.cpp` | Resolve placements on map entry; drain pending cleanup |
| `src/dialog_manager.cpp` | New case in `interact_fixture()` |
| `include/astra/quest.h` | `ObjectiveType::InteractFixture`, `on_fixture_interacted` |
| `src/quest.cpp` | Hook impl; cleanup integration in `complete_quest`/`fail_quest` |
| `src/quest_validator.cpp` | Validate `InteractFixture.target_id` non-empty |
| `src/save_file.cpp` | Bump QUST to v14; serialize new fields |

---

## Implementation Checklist (for the forthcoming plan)

1. Add `FixtureType::QuestFixture` and `FixtureData::quest_fixture_id`; serialize.
2. Create `quest_fixture.h/.cpp` with the registry; add `StoryQuest::register_fixtures()` virtual and call it from `build_catalog()`.
3. Wire renderer to consult the registry for `QuestFixture`.
4. Add `ObjectiveType::InteractFixture` and `QuestManager::on_fixture_interacted`.
5. Extend `QuestLocationMeta` with `fixtures` vector; serialize.
6. Resolve placements on map entry (`enter_detail_map` / `enter_dungeon_from_detail`).
7. Wire `interact_fixture()` case → log + hook.
8. Implement `WorldManager::pending_quest_cleanup_` and drain on map load.
9. Hook into `complete_quest`/`fail_quest` for cleanup.
10. Bump QUST save version, write migration-tolerant readers.
11. Extend startup validator.
12. Smoke-test by registering a debug quest fixture in the dev console (`quest fixture <id>`), confirming pickup, hook firing, save/load, and cleanup.

---

## Out of scope — explicitly deferred

- Multi-state fixtures (planted/unplanted visual swap) — quests handle this by re-registering or swapping ids.
- Placement hints / biome-aware spot selection — first open tile only.
- Per-fixture cleanup overrides.
- Cooldowns, multi-use timers.
- Animation / particle effects on interaction.
