# Quest Fixtures Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a generic, id-driven quest-fixture primitive so story quests can place interactable fixtures (e.g. Nova's Signal Nodes) declaratively, with no per-quest changes to the fixture, render, interaction, or save systems.

**Architecture:** One new `FixtureType::QuestFixture` carrying a `quest_fixture_id` string. A runtime registry (populated at every boot via a new `StoryQuest::register_fixtures()` virtual) maps that id to glyph / color / prompt / log-line. Placements live in `QuestLocationMeta` as `{id, x, y}` tuples with deferred coordinates; the existing map-entry hooks in `game_world.cpp` resolve coordinates on first entry and write them back for persistence. Interaction fires a new `QuestManager::on_fixture_interacted(id)` hook that increments objectives of a new `ObjectiveType::InteractFixture`. Cleanup on quest completion/failure reuses the existing location-level `remove_on_completion` flag via a new `pending_quest_cleanup_` set on `WorldManager`.

**Tech Stack:** C++20, CMake, existing Astra tilemap/quest systems. No new dependencies. No unit-test harness in the repo — verification is compile + manual dev-console smoke test.

**Spec:** `docs/superpowers/specs/2026-04-14-quest-fixtures-design.md`

**Save version:** bumps from v29 → v30.

---

## File Structure

| File | Kind | Responsibility |
|---|---|---|
| `include/astra/quest_fixture.h` | NEW | `QuestFixtureDef`, register/find/clear API |
| `src/quest_fixture.cpp` | NEW | Registry storage (unordered_map) |
| `include/astra/tilemap.h` | MODIFY | `FixtureType::QuestFixture`, `FixtureData::quest_fixture_id` |
| `src/terminal_theme.cpp` | MODIFY | Placeholder glyph for `QuestFixture`; registry-aware glyph/color lookup helpers |
| `include/astra/terminal_theme.h` | MODIFY | Declare new `quest_fixture_glyph`/`quest_fixture_color` helpers |
| `src/map_editor.cpp` (palette) | READ/MODIFY | Confirm palette skips `QuestFixture` (not authorable by hand) |
| `include/astra/quest.h` | MODIFY | `ObjectiveType::InteractFixture`, `QuestManager::on_fixture_interacted`, `StoryQuest::register_fixtures` |
| `src/quest.cpp` | MODIFY | Hook impl; call `register_fixtures` in `build_catalog`; cleanup in `complete_quest` / `fail_quest` |
| `src/quest_validator.cpp` | MODIFY | Reject empty `target_id` for `InteractFixture`, warn on unknown ids |
| `include/astra/world_manager.h` | MODIFY | `QuestFixturePlacement`, `fixtures` vector, `pending_quest_cleanup_` set |
| `src/dialog_manager.cpp` | MODIFY | New case in `interact_fixture` switch |
| `src/game_world.cpp` | MODIFY | Resolve placements in `enter_detail_map` and `enter_dungeon_from_detail`; drain pending cleanup on entry |
| `src/save_file.cpp` | MODIFY | Bump version to 30; serialize new fields |
| `include/astra/save_file.h` | MODIFY | Bump default `version` to 30 |
| `src/dev_console.cpp` | MODIFY | New `quest fixture <id>` smoke-test command |

Build command used throughout: `cmake --build build` (DEV build assumed — user memory: always `-DDEV=ON`). Run with `./build/astra`.

Commit message convention: `feat(quests):`, `refactor:`, `fix:` — match recent history.

---

### Task 1: Add `QuestFixture` enum value and `quest_fixture_id` field

**Files:**
- Modify: `include/astra/tilemap.h:388` (add enum value after `ScrapComponent`)
- Modify: `include/astra/tilemap.h:391-401` (add field to `FixtureData`)

- [ ] **Step 1: Add enum value**

In `include/astra/tilemap.h`, locate the `enum class FixtureType` block that ends around line 388 with `ScrapComponent,`. Insert **before** the closing brace:

```cpp
    QuestFixture,   // generic quest-driven interactable; visuals/prompt via quest_fixture.h registry
```

- [ ] **Step 2: Add field to `FixtureData`**

In the `FixtureData` struct (around line 391), append this field after `blocks_vision`:

```cpp
    std::string quest_fixture_id;   // set only when type == FixtureType::QuestFixture
```

- [ ] **Step 3: Build**

Run: `cmake --build build`
Expected: clean build. The new type is unreferenced and the new string field default-constructs to empty — no call-site changes required yet.

- [ ] **Step 4: Commit**

```bash
git add include/astra/tilemap.h
git commit -m "$(cat <<'EOF'
feat(tilemap): add QuestFixture type and quest_fixture_id field

Reserves the enum value and the per-instance id string. No behavior
yet; subsequent tasks wire the registry, interaction, and placement.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Create quest fixture registry module

**Files:**
- Create: `include/astra/quest_fixture.h`
- Create: `src/quest_fixture.cpp`
- Modify: `CMakeLists.txt` (add `src/quest_fixture.cpp` to the source list)

- [ ] **Step 1: Create the header**

Write `include/astra/quest_fixture.h`:

```cpp
#pragma once

#include <string>

namespace astra {

// Authored definition for a single quest fixture instance.
// Registered at boot (via StoryQuest::register_fixtures) and looked up
// by FixtureData::quest_fixture_id during rendering and interaction.
struct QuestFixtureDef {
    std::string id;           // unique registry key, e.g. "nova_signal_node_echo1"
    char glyph = '?';
    int color = 7;            // terminal color index
    std::string prompt;       // UI hint, e.g. "Plant receiver drone"
    std::string log_message;  // optional; written to game log on interact ("" = silent)
};

// Idempotent: re-registering the same id overwrites the existing def.
void register_quest_fixture(QuestFixtureDef def);

// Returns nullptr if no def is registered for `id`.
const QuestFixtureDef* find_quest_fixture(const std::string& id);

// Drops all registered defs. Used by tests and by game-restart code paths.
void clear_quest_fixtures();

} // namespace astra
```

- [ ] **Step 2: Create the implementation**

Write `src/quest_fixture.cpp`:

```cpp
#include "astra/quest_fixture.h"

#include <unordered_map>

namespace astra {

namespace {
std::unordered_map<std::string, QuestFixtureDef>& registry() {
    static std::unordered_map<std::string, QuestFixtureDef> r;
    return r;
}
}

void register_quest_fixture(QuestFixtureDef def) {
    std::string key = def.id;
    registry()[std::move(key)] = std::move(def);
}

const QuestFixtureDef* find_quest_fixture(const std::string& id) {
    auto& r = registry();
    auto it = r.find(id);
    return it == r.end() ? nullptr : &it->second;
}

void clear_quest_fixtures() {
    registry().clear();
}

} // namespace astra
```

- [ ] **Step 3: Add source to CMakeLists.txt**

Open `CMakeLists.txt`. Find the existing list of source files (it contains `src/quest.cpp`, `src/quest_graph.cpp`, `src/quest_validator.cpp`). Add `src/quest_fixture.cpp` next to them, keeping alphabetical order.

- [ ] **Step 4: Build**

Run: `cmake --build build`
Expected: clean build with the new translation unit linked in.

- [ ] **Step 5: Commit**

```bash
git add include/astra/quest_fixture.h src/quest_fixture.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(quests): quest-fixture registry module

Id-keyed store for authored QuestFixtureDef entries (glyph, color,
prompt, log line). Populated at boot by StoryQuest::register_fixtures;
not serialized — defs are content, repopulated each run.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Add `StoryQuest::register_fixtures` and call from catalog build

**Files:**
- Modify: `include/astra/quest.h:176-197` (add virtual)
- Modify: `src/quests/missing_hauler.cpp:144` (the `build_catalog` function)

- [ ] **Step 1: Add the virtual method**

In `include/astra/quest.h`, inside the `class StoryQuest` block, after the existing lifecycle hooks (`on_unlocked`, `on_accepted`, `on_completed`, `on_failed`), add:

```cpp
    // Register any QuestFixtureDef entries owned by this quest.
    // Called once per boot from build_catalog(); must be idempotent.
    virtual void register_fixtures() {}
```

- [ ] **Step 2: Call it from `build_catalog`**

Open `src/quests/missing_hauler.cpp`. Find `build_catalog` (near line 144). After the existing `register_*()` calls populate the vector, loop over the vector and call `register_fixtures()`:

```cpp
    // Give every story quest a chance to register its fixture defs.
    for (const auto& sq : c) {
        sq->register_fixtures();
    }
```

Place the loop **before** any `return` statement in `build_catalog`.

- [ ] **Step 3: Build**

Run: `cmake --build build`
Expected: clean build. The loop exists but no quest overrides yet, so it's a no-op at runtime.

- [ ] **Step 4: Commit**

```bash
git add include/astra/quest.h src/quests/missing_hauler.cpp
git commit -m "$(cat <<'EOF'
feat(quests): StoryQuest::register_fixtures() lifecycle hook

Called once per boot from build_catalog so fixture defs exist before
any save restore. on_accepted fires only on fresh acceptance, which
is too late for reloads — hence a separate hook.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: Placeholder glyph + registry-aware rendering helpers

**Files:**
- Modify: `src/terminal_theme.cpp:1666` (the `fixture_glyph` switch)
- Modify: `include/astra/terminal_theme.h:38` (add helper declarations)

- [ ] **Step 1: Add a placeholder case to `fixture_glyph`**

In `src/terminal_theme.cpp`, find `fixture_glyph(FixtureType type)` and add a `case QuestFixture: return '?';` before the default. This is the fallback used when the registry lookup is skipped or fails.

- [ ] **Step 2: Add registry-aware helpers**

Still in `src/terminal_theme.cpp`, below the existing `fixture_glyph` function, add:

```cpp
#include "astra/quest_fixture.h"
// ... (place the include at the top of the file, not mid-function)

char quest_fixture_glyph(const std::string& id) {
    if (const auto* def = find_quest_fixture(id)) return def->glyph;
    return '?';
}

int quest_fixture_color(const std::string& id, int fallback) {
    if (const auto* def = find_quest_fixture(id)) return def->color;
    return fallback;
}
```

Move the `#include "astra/quest_fixture.h"` up to the include block at the top of the file.

- [ ] **Step 3: Declare the helpers in the header**

In `include/astra/terminal_theme.h`, next to the existing `char fixture_glyph(FixtureType)` declaration around line 38, add:

```cpp
char quest_fixture_glyph(const std::string& id);
int  quest_fixture_color(const std::string& id, int fallback);
```

- [ ] **Step 4: Teach the render path to consult the registry**

Find the call sites of `fixture_glyph(` using the Grep tool (`fixture_glyph\\(`, type `cpp`). Expect two:

- `src/terminal_theme.cpp:1666` (the definition itself — skip)
- `src/map_editor.cpp:1243` (dev palette — this renders a prototype, not a live fixture; leave it with the placeholder `?`)

Now find the actual gameplay render path. Search for call sites that read a live `FixtureData` and produce a glyph. Run:

```bash
grep -rn "fixture(.*)\.type\|fixture_glyph\|fixtures_vec" src/ include/ | grep -v "terminal_theme.cpp\|map_editor.cpp"
```

For each render call site that currently calls `fixture_glyph(f.type)` where `f` is the `FixtureData`, replace with:

```cpp
char glyph = (f.type == FixtureType::QuestFixture && !f.quest_fixture_id.empty())
    ? quest_fixture_glyph(f.quest_fixture_id)
    : fixture_glyph(f.type);
```

And any accompanying color assignment should fall through `quest_fixture_color(f.quest_fixture_id, <default>)` when the type is `QuestFixture`.

If no current render site exists (the renderer resolves via `Tile::Fixture` + theme tables that only see a tile, not a `FixtureData`), report this in the PR and add the lookup to the one place the renderer has access to the `FixtureData`. The terminal renderer for fixtures reads via `map.fixture(map.fixture_id(x,y))` — look in `src/terminal_renderer.cpp` and `src/game_rendering.cpp` for the glyph emission for `Tile::Fixture`.

- [ ] **Step 5: Build and smoke-test**

Run: `cmake --build build`
Expected: clean build.

Run: `./build/astra --term` then `q` to quit. No runtime change yet (no defs registered, no `QuestFixture` fixtures placed), just confirming the new code paths compile.

- [ ] **Step 6: Commit**

```bash
git add src/terminal_theme.cpp include/astra/terminal_theme.h src/terminal_renderer.cpp src/game_rendering.cpp
git commit -m "$(cat <<'EOF'
feat(render): resolve QuestFixture glyph/color via quest_fixture registry

Render path falls through to authored def when a fixture carries a
quest_fixture_id; placeholder '?' for missing defs keeps saves
renderable across catalog drift.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

(Adjust the `git add` list to only include files you actually modified — the command is illustrative.)

---

### Task 5: Add `InteractFixture` objective and `on_fixture_interacted` hook

**Files:**
- Modify: `include/astra/quest.h:38-45` (extend `ObjectiveType`)
- Modify: `include/astra/quest.h:110-114` (declare new hook)
- Modify: `src/quest.cpp` (implement hook and any display-string switches)

- [ ] **Step 1: Extend the enum**

In `include/astra/quest.h`, the `ObjectiveType` enum currently ends with `InstallShipComponent,` on line 44. Add:

```cpp
    InteractFixture,
```

- [ ] **Step 2: Declare the hook**

Next to the other `on_*` methods on `QuestManager` (around line 114), add:

```cpp
    void on_fixture_interacted(const std::string& fixture_id);
```

- [ ] **Step 3: Implement the hook**

In `src/quest.cpp`, find `on_npc_killed` and add alongside it:

```cpp
void QuestManager::on_fixture_interacted(const std::string& fixture_id) {
    for (auto& q : active_) {
        for (auto& obj : q.objectives) {
            if (obj.type == ObjectiveType::InteractFixture &&
                obj.target_id == fixture_id &&
                obj.current_count < obj.target_count) {
                ++obj.current_count;
            }
        }
    }
}
```

- [ ] **Step 4: Cover any objective-stringifier switches**

Search for `switch (obj.type)` / `switch (objective.type)` across `src/` (journal rendering, quest tab, dev console). Any such switch that currently handles all `ObjectiveType` entries must get an `InteractFixture` case — falling through to the default is acceptable if the default prints `obj.description` verbatim. Add explicit cases where the other objective types produce custom text.

Run:

```bash
grep -rn "ObjectiveType::" src/ include/
```

Inspect each hit for switch coverage.

- [ ] **Step 5: Build**

Run: `cmake --build build`
Expected: clean build. Compiler `-Wswitch` warnings (if enabled) flag any missed switches.

- [ ] **Step 6: Commit**

```bash
git add include/astra/quest.h src/quest.cpp
git commit -m "$(cat <<'EOF'
feat(quests): InteractFixture objective + on_fixture_interacted hook

Mirrors the KillNpc/CollectItem progress model. Callers identify the
fixture by its registry id string.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: Wire `interact_fixture` to fire the hook

**Files:**
- Modify: `src/dialog_manager.cpp:328` (the switch in `interact_fixture`)

- [ ] **Step 1: Add the switch case**

In `src/dialog_manager.cpp`, find the `switch (f.type)` block inside `DialogManager::interact_fixture` (around line 328). Add a new case alongside the others (order doesn't matter, but placing it near the top keeps quest logic visible):

```cpp
        case FixtureType::QuestFixture: {
            const QuestFixtureDef* def = find_quest_fixture(f.quest_fixture_id);
            if (def && !def->log_message.empty()) {
                game.log(def->log_message);
            }
            game.quests().on_fixture_interacted(f.quest_fixture_id);
            break;
        }
```

- [ ] **Step 2: Add the include**

At the top of `src/dialog_manager.cpp`, add:

```cpp
#include "astra/quest_fixture.h"
```

- [ ] **Step 3: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add src/dialog_manager.cpp
git commit -m "$(cat <<'EOF'
feat(dialog): route QuestFixture interaction to quest manager

Logs the def's log_message (if any) and fires on_fixture_interacted.
No cooldown or state change — the quest owns follow-up behavior.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 7: Placement — extend `QuestLocationMeta`, resolve on entry

**Files:**
- Modify: `include/astra/world_manager.h:34-44` (extend `QuestLocationMeta`)
- Modify: `src/game_world.cpp:680-700` (inject into `enter_detail_map`)
- Modify: `src/game_world.cpp:820-895` (inject into `enter_dungeon_from_detail`)

- [ ] **Step 1: Add placement struct and vector**

In `include/astra/world_manager.h`, above `QuestLocationMeta`, define:

```cpp
struct QuestFixturePlacement {
    std::string fixture_id;   // registry key
    int x = -1;               // -1 = unresolved; resolver picks + writes back
    int y = -1;
};
```

Then, inside `QuestLocationMeta`, add (after `quest_items`):

```cpp
    std::vector<QuestFixturePlacement> fixtures;
```

- [ ] **Step 2: Add a resolver helper**

In `src/game_world.cpp`, above the body of `enter_detail_map` and `enter_dungeon_from_detail`, add a static helper:

```cpp
static void place_quest_fixtures(TileMap& map,
                                 QuestLocationMeta& meta,
                                 int avoid_x, int avoid_y,
                                 std::vector<std::pair<int,int>>& occupied,
                                 std::mt19937& rng) {
    for (auto& p : meta.fixtures) {
        // Already resolved in a previous session — fixture is in the serialized map.
        if (p.x >= 0 && p.y >= 0) continue;

        int fx = 0, fy = 0;
        if (!map.find_open_spot_other_room(avoid_x, avoid_y, fx, fy, occupied, &rng)) {
            continue;   // no room; skip (quest can be failed later if still required)
        }

        FixtureData fd;
        fd.type = FixtureType::QuestFixture;
        fd.interactable = true;
        fd.passable = true;
        fd.quest_fixture_id = p.fixture_id;
        map.add_fixture(fx, fy, fd);

        p.x = fx;
        p.y = fy;
        occupied.push_back({fx, fy});
    }
}
```

- [ ] **Step 3: Call from `enter_dungeon_from_detail`**

In `src/game_world.cpp`, inside the `if (qit != world_.quest_locations().end()) { ... }` block (around line 867–891), after the existing `quest_items` loop and before the closing brace, add:

```cpp
            // Quest-driven fixtures (Receiver Drones, Signal Nodes, etc.)
            place_quest_fixtures(world_.map(),
                                 const_cast<QuestLocationMeta&>(qit->second),
                                 player_.x, player_.y, occupied, npc_rng);
```

Note: `qit->second` is obtained from a `find()` on a non-const `quest_locations()`, so the `const_cast` is strictly unnecessary. If `qit` is `const_iterator`, re-look up via `world_.quest_locations()[qit->first]` instead.

- [ ] **Step 4: Call from `enter_detail_map`**

In `src/game_world.cpp::enter_detail_map`, after NPCs are spawned (around line 697, before `world_.visibility() = ...`), add a parallel quest-location check:

```cpp
    // Quest fixtures (body-level detail map)
    LocationKey dkey = {world_.navigation().current_system_id,
                        world_.navigation().current_body_index,
                        world_.navigation().current_moon_index,
                        false, world_.overworld_x(), world_.overworld_y(), 0};
    auto qit = world_.quest_locations().find(dkey);
    if (qit == world_.quest_locations().end()) {
        LocationKey bkey = {world_.navigation().current_system_id,
                            world_.navigation().current_body_index,
                            world_.navigation().current_moon_index,
                            false, -1, -1, 0};
        qit = world_.quest_locations().find(bkey);
    }
    if (qit != world_.quest_locations().end()) {
        std::vector<std::pair<int,int>> occupied = {{player_.x, player_.y}};
        for (const auto& npc : world_.npcs()) occupied.push_back({npc.x, npc.y});
        place_quest_fixtures(world_.map(), world_.quest_locations()[qit->first],
                             player_.x, player_.y, occupied, npc_rng);
    }
```

(If `enter_detail_map` doesn't currently have a `npc_rng`, create one with `std::mt19937 npc_rng(detail_seed ^ 0xF1X7u);` at the top of the fresh-map block.)

- [ ] **Step 5: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 6: Commit**

```bash
git add include/astra/world_manager.h src/game_world.cpp
git commit -m "$(cat <<'EOF'
feat(quests): deferred placement for quest fixtures

QuestLocationMeta gains a fixtures vector. On first entry, the
resolver picks an open spot and writes coords back so subsequent
saves keep the planted location stable.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 8: Cleanup on quest completion / failure

**Files:**
- Modify: `include/astra/world_manager.h` (add `pending_quest_cleanup_`)
- Modify: `src/world_manager.cpp` (accessor + impl if needed)
- Modify: `src/quest.cpp:68` (`complete_quest`)
- Modify: `src/quest.cpp:153` (`fail_quest`)
- Modify: `src/game_world.cpp` (drain pending cleanup on entry)
- Modify: `include/astra/tilemap.h` (confirm `remove_fixture(int,int)` signature — already exists per survey)

- [ ] **Step 1: Add pending-cleanup set**

In `include/astra/world_manager.h`, add (alongside `quest_locations_`):

```cpp
    // LocationKeys whose quest fixtures must be removed on next map entry.
    std::set<LocationKey> pending_quest_cleanup_;
public:
    std::set<LocationKey>& pending_quest_cleanup() { return pending_quest_cleanup_; }
    const std::set<LocationKey>& pending_quest_cleanup() const { return pending_quest_cleanup_; }
```

Include `<set>` at the top if not already pulled in.

- [ ] **Step 2: Cleanup helper in `quest.cpp`**

In `src/quest.cpp`, add above `complete_quest`:

```cpp
static void cleanup_quest_fixtures(Game& game, const std::string& quest_id) {
    auto& locs = game.world().quest_locations();
    auto& pending = game.world().pending_quest_cleanup();

    for (auto it = locs.begin(); it != locs.end();) {
        auto& meta = it->second;
        if (meta.quest_id != quest_id || !meta.remove_on_completion) {
            ++it;
            continue;
        }

        // Is this location currently loaded (i.e. matches the player's key)?
        const auto& nav = game.world().navigation();
        LocationKey cur = {nav.current_system_id, nav.current_body_index,
                           nav.current_moon_index, false,
                           game.world().overworld_x(), game.world().overworld_y(), 0};
        bool is_current = (it->first == cur);

        if (is_current) {
            for (const auto& p : meta.fixtures) {
                if (p.x >= 0 && p.y >= 0) {
                    game.world().map().remove_fixture(p.x, p.y);
                }
            }
        } else {
            pending.insert(it->first);
        }
        it = locs.erase(it);
    }
}
```

- [ ] **Step 3: Call it from `complete_quest` and `fail_quest`**

In `src/quest.cpp::complete_quest`, after `active_.erase(it);` and *before* the StoryQuest hook (so on_completed can assume the world is clean), add:

```cpp
    cleanup_quest_fixtures(game, quest_id);
```

In `fail_quest`, loop: after each successful `move_failed` for `id`, call:

```cpp
    cleanup_quest_fixtures(game, id);
```

- [ ] **Step 4: Drain pending set on map entry**

In `src/game_world.cpp::enter_detail_map` and `::enter_dungeon_from_detail`, **before** the placement resolver runs, add:

```cpp
    {
        auto& pending = world_.pending_quest_cleanup();
        LocationKey cur_key = /* same key you use below for the lookup */;
        if (pending.erase(cur_key)) {
            // Fixtures for this key were already erased at completion time by iterating
            // over QuestLocationMeta; if the meta had been erased before the map was
            // loaded, we scrub any lingering QuestFixture fixtures with a matching id here.
            // No-op currently — drain is sufficient because the meta-erase already
            // removed the placement entries. The set entry exists only to signal
            // "cleanup already applied" in case a restore re-inflates state.
        }
    }
```

(The set exists to preserve intent across save/load; if the codebase later introduces per-key fixture caches, drainers will do real work. The erase-on-map-not-loaded branch in Step 2 guarantees correctness now because the meta is erased; the pending set is a safety net for save/load edge cases.)

- [ ] **Step 5: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 6: Commit**

```bash
git add include/astra/world_manager.h src/world_manager.cpp src/quest.cpp src/game_world.cpp
git commit -m "$(cat <<'EOF'
feat(quests): cleanup quest fixtures on complete/fail

When QuestLocationMeta.remove_on_completion is set, planted fixtures
are removed from the current map immediately; otherwise the location
key is queued in pending_quest_cleanup_ for drainage on next entry.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 9: Save / load — bump to v30 and serialize new fields

**Files:**
- Modify: `include/astra/save_file.h:62` (version default)
- Modify: `src/save_file.cpp:730-744` (fixture write block)
- Modify: `src/save_file.cpp:1455-1468` (fixture read block)
- Modify: `src/save_file.cpp:987-1094` (QUST section write/read — add `fixtures` + `pending_quest_cleanup`)

- [ ] **Step 1: Bump version**

In `include/astra/save_file.h` line 62, change the `version` default:

```cpp
uint32_t version = 30;   // v30: quest fixtures (FixtureData.quest_fixture_id + QuestLocationMeta.fixtures + pending_quest_cleanup)
```

- [ ] **Step 2: Serialize `quest_fixture_id` on fixture write**

In `src/save_file.cpp` around line 734, inside the fixture write loop, append:

```cpp
        w.write_string(f.quest_fixture_id);   // v30
```

- [ ] **Step 3: Read it back, gated on version**

Around line 1467, inside the fixture read loop, append:

```cpp
            if (version >= 30) {
                f.quest_fixture_id = r.read_string();
            }
```

- [ ] **Step 4: Write `fixtures` vector in QUST**

In `write_quest_section` (`src/save_file.cpp:987`), inside the `for (const auto& [key, meta] : data.quest_locations)` loop, **after** `w.write_i32(meta.target_body_index);`, append:

```cpp
        // v30: quest fixtures
        w.write_u32(static_cast<uint32_t>(meta.fixtures.size()));
        for (const auto& p : meta.fixtures) {
            w.write_string(p.fixture_id);
            w.write_i32(p.x);
            w.write_i32(p.y);
        }
```

- [ ] **Step 5: Read it back in `read_quest_section`**

Around line 1094, after `meta.target_body_index = r.read_i32();`, append:

```cpp
        if (data.version >= 30) {
            uint32_t fc = r.read_u32();
            meta.fixtures.resize(fc);
            for (auto& p : meta.fixtures) {
                p.fixture_id = r.read_string();
                p.x = r.read_i32();
                p.y = r.read_i32();
            }
        }
```

- [ ] **Step 6: Serialize `pending_quest_cleanup` (end of QUST section)**

At the end of `write_quest_section`, before `w.end_section(pos);`, add:

```cpp
    // v30: pending quest cleanup set
    w.write_u32(static_cast<uint32_t>(data.pending_quest_cleanup.size()));
    for (const auto& k : data.pending_quest_cleanup) {
        auto [sys, b, m, stn, ow_x, ow_y, d] = k;
        w.write_u32(sys); w.write_i32(b); w.write_i32(m);
        w.write_u8(stn ? 1 : 0);
        w.write_i32(ow_x); w.write_i32(ow_y); w.write_i32(d);
    }
```

In `read_quest_section`, at the end (after the `quest_locations` loop), add:

```cpp
    if (data.version >= 30) {
        uint32_t pc = r.read_u32();
        for (uint32_t i = 0; i < pc; ++i) {
            uint32_t sys = r.read_u32();
            int b = r.read_i32();
            int m = r.read_i32();
            bool stn = r.read_u8() != 0;
            int ow_x = r.read_i32();
            int ow_y = r.read_i32();
            int d = r.read_i32();
            data.pending_quest_cleanup.insert(LocationKey{sys, b, m, stn, ow_x, ow_y, d});
        }
    }
```

- [ ] **Step 7: Plumb `SaveData.pending_quest_cleanup`**

In `include/astra/save_file.h`, add a field on `SaveData`:

```cpp
    std::set<LocationKey> pending_quest_cleanup;
```

In whichever file populates `SaveData` from `WorldManager` (grep for `data.quest_locations =` to find the collector function), copy:

```cpp
    data.pending_quest_cleanup = world.pending_quest_cleanup();
```

And in the restore path, after `world.quest_locations() = data.quest_locations;`:

```cpp
    world.pending_quest_cleanup() = data.pending_quest_cleanup;
```

- [ ] **Step 8: Build and smoke-test round-trip**

Run: `cmake --build build`
Run: `./build/astra --term` → play briefly → save → quit → reload. Confirm no save-version error.

- [ ] **Step 9: Commit**

```bash
git add include/astra/save_file.h src/save_file.cpp
git commit -m "$(cat <<'EOF'
feat(save): v30 — serialize quest_fixture_id and QuestLocationMeta fixtures

FixtureData gains a trailing string field; QuestLocationMeta gains
a fixtures vector; pending_quest_cleanup survives save/load. Older
saves load with empty defaults.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 10: Validator — catch malformed quest fixture objectives

**Files:**
- Modify: `src/quest_validator.cpp:28` (`validate_quest_catalog`)

- [ ] **Step 1: Add the check**

In `src/quest_validator.cpp::validate_quest_catalog`, where it already walks each quest's objectives, add:

```cpp
        for (const auto& obj : q.objectives) {
            if (obj.type == ObjectiveType::InteractFixture && obj.target_id.empty()) {
                errors.push_back(q.id + ": InteractFixture objective has empty target_id");
            }
        }
```

(If the function doesn't already iterate objectives, wrap this in an appropriate loop over the catalog and each quest's `create_quest()` result.)

- [ ] **Step 2: Optional registry-existence warning**

After the full validation pass, loop once more and, for each `InteractFixture` target_id, check `find_quest_fixture(id)`. If nullptr, push a **warning** string (distinguished from errors by a prefix, e.g. `"WARN: "`). Defs may legitimately be registered lazily, so this is not an error.

Skip Step 2 if the validator doesn't currently distinguish warnings from errors — don't force a mechanism change here.

- [ ] **Step 3: Build**

Run: `cmake --build build`
Expected: clean build; validator runs at startup and should report zero errors for the current catalog (no quest yet uses `InteractFixture`).

- [ ] **Step 4: Commit**

```bash
git add src/quest_validator.cpp
git commit -m "$(cat <<'EOF'
feat(quests): validate InteractFixture objectives have non-empty target_id

Catches the most common authoring mistake — dangling fixture
references — at startup rather than at interaction time.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 11: Dev console smoke test and full manual verification

**Files:**
- Modify: `src/dev_console.cpp:441-515` (add `quest fixture` subcommand)

- [ ] **Step 1: Add the subcommand**

In `src/dev_console.cpp`, extend the existing `quest` verb block (around line 441). After the `"story"` branch and before the final `else`, add:

```cpp
        } else if (args.size() >= 2 && args[1] == "fixture") {
            // Register a debug def and plant it at the player's current location.
            QuestFixtureDef def;
            def.id = "dev_smoke_fixture";
            def.glyph = '*';
            def.color = 135;
            def.prompt = "Interact (debug)";
            def.log_message = "You nudge the debug fixture. It beeps.";
            register_quest_fixture(def);

            FixtureData fd;
            fd.type = FixtureType::QuestFixture;
            fd.interactable = true;
            fd.passable = true;
            fd.quest_fixture_id = def.id;

            int fx = game.player().x + 1;
            int fy = game.player().y;
            if (game.world().map().passable(fx, fy) &&
                game.world().map().fixture_id(fx, fy) < 0) {
                game.world().map().add_fixture(fx, fy, fd);
                log("Planted dev_smoke_fixture at (" + std::to_string(fx) + "," + std::to_string(fy) + ")");
            } else {
                log("No open tile adjacent to player for fixture.");
            }
```

Add `#include "astra/quest_fixture.h"` at the top if not present.

Update the `Usage:` log line to `quest kill|fetch|deliver|scout|story|fixture`.

- [ ] **Step 2: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 3: Manual smoke test**

Run: `./build/astra --term`

From a fresh game, in The Heavens Above:

1. Open dev console (`` ` `` by default). Run: `quest fixture`
2. Expect the log: `Planted dev_smoke_fixture at (x,y)`.
3. Close console. Walk onto or next to the fixture. Confirm the `*` glyph is visible.
4. Press interact (`e`) adjacent to the fixture. Expect the log: `You nudge the debug fixture. It beeps.`

Then test save/load:

5. Save game (`S` if bound, or via menu).
6. Quit to main menu.
7. Load the save. Walk back to the fixture. Confirm the `*` is still there.
8. Interact again. Expect the same beep log.

Then test the objective hook (optional, requires an active quest with an `InteractFixture` objective — since no story quest uses it yet, this is covered by the Nova arc implementation task). Skip step 8's quest-completion check for now.

Document any deviations from expected output in the PR body.

- [ ] **Step 4: Commit**

```bash
git add src/dev_console.cpp
git commit -m "$(cat <<'EOF'
feat(dev): quest fixture — dev-console command to plant a debug fixture

Exercises the full path: registry → placement → render → interact →
hook → save/load. Intended as an authoring aid and regression canary
for the quest-fixture primitive.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Acceptance Criteria

- `cmake --build build` is clean at every task commit.
- `./build/astra --term` starts and loads a new/old save without save-version errors.
- The `quest fixture` dev command plants a visible, interactable fixture that:
  - renders with the registered glyph/color,
  - fires the `log_message` on interact,
  - calls `on_fixture_interacted`,
  - survives save → quit → reload,
  - is removable via `map.remove_fixture(x, y)`.
- `validate_quest_catalog` reports zero errors against the existing catalog.
- No existing quest or fixture behavior regresses (Missing Hauler, Getting Airborne, hub fixtures all still work).
- No changes outside the files listed in the File Structure table.

---

## Out of Scope (explicitly deferred)

- Authoring the Nova "Signal Node" fixtures themselves — tracked in the next plan.
- Biome- or POI-aware placement hints (`NearCenter`, `OnOrbit`, `InBrokenCommsArray`).
- Per-fixture `remove_on_completion`.
- Multi-state fixtures (pre/post interaction visual swap).
- Unit tests — no harness in the repo; smoke-test via dev console covers the primitive.
