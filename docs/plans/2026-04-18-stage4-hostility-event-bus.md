# Stage 4 Hostility & Ambush Slice — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship the first narrative slice of Stellar Signal Stage 4 — the Stellari Conclave turns on the player after completing Stage 3, an incoming transmission plays once, and subsequent warps into fresh systems spawn Conclave Sentry ambushes until the arc advances.

**Architecture:** Introduce a minimal in-process **EventBus** (typed events, functor subscribers, no scripting layer), a small **effect primitives** library (C++ free functions), and one concrete **scenario** — Stage 4 hostility — that drives entirely off these two pieces. The bus has three producers (Stage 3 completion, system entry, quest stage completion) and a handful of effects (`shift_faction_standing`, `set_world_flag`, `open_transmission`, `inject_quest_location_encounter`). Saves persist the world-flag state; handlers are re-registered deterministically from flags on load so the bus itself never serializes.

**Tech Stack:** C++20, existing Astra headers (`include/astra/`), CMake build. No new third-party dependencies. No tests — Astra currently has no test framework; validation is build + in-game smoke test in dev mode.

**Non-goals (explicit):**
- Station siege state (separate slice)
- Archive / Io dungeon (separate slice)
- Timed objectives
- Companion system, NG+, endings
- Scripting layer, YAML/JSON config, hot reload

**Related docs:**
- `docs/plans/stellar_signal_phase4_5_gaps.md` — full gap analysis
- `docs/plans/scenario_graph_vision.md` — where this architecture grows toward

---

## File Structure

**New files:**

- `include/astra/event_bus.h` — `Event` variant, `EventBus` class, `HandlerId` handle
- `src/event_bus.cpp` — subscribe/unsubscribe/emit implementation
- `include/astra/scenario_effects.h` — declarations for `shift_faction_standing`, `set_world_flag`, `open_transmission`, `inject_quest_location_encounter`
- `src/scenario_effects.cpp` — effect primitive implementations (thin wrappers over existing systems)
- `include/astra/scenarios.h` — declaration of `register_all_scenarios(Game&)` and individual scenario registration functions
- `src/scenarios/stage4_hostility.cpp` — the Stage 4 hostility scenario (subscribes to events, runs effects)
- `src/npcs/conclave_sentry.cpp` — new NPC archetype (Stellari Conclave faction, mid-tier stats)

**Modified files:**

- `include/astra/world_manager.h` — add `world_flags_` map with get/set helpers; add `ambushed_systems_` set; expose `event_bus()` accessor on `Game` (not `WorldManager`, since EventBus is session-global)
- `src/world_manager.cpp` — accessors
- `include/astra/game.h` — own an `EventBus` member, expose `event_bus()` accessor, call `register_all_scenarios(*this)` in `start_new_game` / after load
- `src/game.cpp` — registration wiring
- `include/astra/faction.h` — add declaration for `modify_faction_standing(Player&, const std::string& faction, int delta)`
- `src/faction.cpp` — implementation
- `src/game_world.cpp` — emit `SystemEntered` event when `nav.current_system_id` changes (at the warp boundary)
- `src/quests/stellar_signal_beacon.cpp` — `on_completed` sets `stage4_active=true` and shifts Conclave standing by -300; emits `QuestStageCompleted{"story_stellar_signal_beacon"}`
- `include/astra/save_file.h` + `src/save_file.cpp` — serialize `world_flags_` and `ambushed_systems_`
- `include/astra/npc_defs.h` (or equivalent NpcRole enum home) — add `ConclaveSentry` role
- `src/npc_spawner.cpp` — dispatch `ConclaveSentry` role to the new archetype
- `CMakeLists.txt` — add new source files to `ASTRA_SOURCES`
- `docs/roadmap.md` — tick relevant Stage 4 boxes

---

## Task 1: Faction standing modifier helper

**Files:**
- Modify: `include/astra/faction.h:38-48` (add declaration)
- Modify: `src/faction.cpp` (add implementation)

- [ ] **Step 1: Add declaration to `include/astra/faction.h`**

Insert after the existing `reputation_for` / `is_hostile_to_player` declarations:

```cpp
// Add or subtract from the player's standing with a named faction.
// If the faction isn't in the reputation vector yet, it is appended.
// Clamps to [-1000, 1000]. Returns the new standing value.
int modify_faction_standing(Player& player, const std::string& faction, int delta);
```

- [ ] **Step 2: Implement in `src/faction.cpp`**

```cpp
int modify_faction_standing(Player& player, const std::string& faction, int delta) {
    for (auto& fs : player.reputation) {
        if (fs.faction_name == faction) {
            fs.reputation = std::clamp(fs.reputation + delta, -1000, 1000);
            return fs.reputation;
        }
    }
    FactionStanding fs;
    fs.faction_name = faction;
    fs.reputation = std::clamp(delta, -1000, 1000);
    player.reputation.push_back(fs);
    return fs.reputation;
}
```

Make sure `<algorithm>` is included for `std::clamp`.

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: clean build, no new warnings.

- [ ] **Step 4: Commit**

```bash
git add include/astra/faction.h src/faction.cpp
git commit -m "feat(faction): add modify_faction_standing helper"
```

---

## Task 2: World flags + ambushed-systems set on WorldManager

**Files:**
- Modify: `include/astra/world_manager.h`
- Modify: `src/world_manager.cpp`

- [ ] **Step 1: Add flag storage + accessors to `include/astra/world_manager.h`**

In the `WorldManager` class `public:` section (near other accessors), add:

```cpp
// Scenario world flags — string-keyed boolean state flipped by scenarios.
// Persisted in the save file. Examples: "stage4_active".
bool world_flag(const std::string& name) const;
void set_world_flag(const std::string& name, bool value);
const std::unordered_map<std::string, bool>& world_flags() const { return world_flags_; }
std::unordered_map<std::string, bool>& world_flags() { return world_flags_; }

// Systems the player has already been ambushed in during current Stage 4 run.
// Used so each system spawns its Conclave ambush at most once.
const std::unordered_set<uint32_t>& ambushed_systems() const { return ambushed_systems_; }
std::unordered_set<uint32_t>& ambushed_systems() { return ambushed_systems_; }
```

In the `private:` section:

```cpp
std::unordered_map<std::string, bool> world_flags_;
std::unordered_set<uint32_t> ambushed_systems_;
```

Add `#include <unordered_map>` and `#include <unordered_set>` at the top.

- [ ] **Step 2: Implement accessors in `src/world_manager.cpp`**

```cpp
bool WorldManager::world_flag(const std::string& name) const {
    auto it = world_flags_.find(name);
    return it != world_flags_.end() && it->second;
}

void WorldManager::set_world_flag(const std::string& name, bool value) {
    world_flags_[name] = value;
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add include/astra/world_manager.h src/world_manager.cpp
git commit -m "feat(world): add world_flags and ambushed_systems state"
```

---

## Task 3: Save/load for world flags & ambushed systems

**Files:**
- Modify: `include/astra/save_file.h`
- Modify: `src/save_file.cpp`

- [ ] **Step 1: Bump save file version and add serialization**

Find the `SAVE_FILE_VERSION` constant in `include/astra/save_file.h` and bump it by 1. Note the old version so the loader can skip the new sections for older saves.

- [ ] **Step 2: Write world flags in save path**

In `src/save_file.cpp`, in the function that serializes `WorldManager`, append:

```cpp
// World flags (map<string,bool>)
write_u32(static_cast<uint32_t>(world.world_flags().size()));
for (const auto& [k, v] : world.world_flags()) {
    write_string(k);
    write_u8(v ? 1 : 0);
}

// Ambushed systems (set<uint32_t>)
write_u32(static_cast<uint32_t>(world.ambushed_systems().size()));
for (uint32_t sid : world.ambushed_systems()) {
    write_u32(sid);
}
```

Use the exact helper function names already in `save_file.cpp` — check the file for the conventions (e.g. `write_u32`, `write_string`). Match them verbatim.

- [ ] **Step 3: Read in load path, gated on version**

```cpp
if (version >= NEW_VERSION_WITH_WORLD_FLAGS) {
    uint32_t flag_count = read_u32();
    for (uint32_t i = 0; i < flag_count; ++i) {
        std::string k = read_string();
        bool v = read_u8() != 0;
        world.world_flags()[k] = v;
    }
    uint32_t ambush_count = read_u32();
    for (uint32_t i = 0; i < ambush_count; ++i) {
        world.ambushed_systems().insert(read_u32());
    }
}
```

- [ ] **Step 4: Build and run an in-game save/load round-trip**

Run: `cmake --build build -j`
Then launch `./build/astra` (with `-DDEV=ON`), save, load, verify no crash.

- [ ] **Step 5: Commit**

```bash
git add include/astra/save_file.h src/save_file.cpp
git commit -m "feat(save): persist world flags and ambushed systems set"
```

---

## Task 4: EventBus skeleton

**Files:**
- Create: `include/astra/event_bus.h`
- Create: `src/event_bus.cpp`
- Modify: `CMakeLists.txt` (add `src/event_bus.cpp` to `ASTRA_SOURCES`)

- [ ] **Step 1: Write `include/astra/event_bus.h`**

```cpp
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace astra {

class Game; // forward declare

// ─── Event payloads ───────────────────────────────────────────────
// Keep these tiny. Add new event types by adding a new struct + a new
// variant arm. Producers emit, scenarios subscribe.

struct SystemEnteredEvent {
    uint32_t system_id = 0;
    uint32_t previous_system_id = 0;  // 0 on first entry
};

struct BodyEnteredEvent {
    uint32_t system_id = 0;
    int body_index = -1;
    bool is_station = false;
};

struct QuestStageCompletedEvent {
    std::string quest_id;
};

using Event = std::variant<
    SystemEnteredEvent,
    BodyEnteredEvent,
    QuestStageCompletedEvent
>;

// ─── Bus ──────────────────────────────────────────────────────────

using HandlerId = uint64_t;

enum class EventKind : uint32_t {
    SystemEntered = 0,
    BodyEntered,
    QuestStageCompleted,
    _Count
};

EventKind event_kind_of(const Event& ev);

using EventHandler = std::function<void(Game&, const Event&)>;

class EventBus {
public:
    HandlerId subscribe(EventKind kind, EventHandler handler);
    void unsubscribe(HandlerId id);

    // Calls every handler subscribed to the event's kind in registration
    // order. Exceptions escape — handlers must not throw on hot paths.
    void emit(Game& game, const Event& ev);

    // Removes all subscriptions. Called on world reset / new game.
    void clear();

private:
    struct Subscription {
        HandlerId id;
        EventKind kind;
        EventHandler handler;
    };
    std::vector<Subscription> subs_;
    HandlerId next_id_ = 1;
};

} // namespace astra
```

- [ ] **Step 2: Write `src/event_bus.cpp`**

```cpp
#include "astra/event_bus.h"

#include <algorithm>

namespace astra {

EventKind event_kind_of(const Event& ev) {
    return std::visit([](auto&& payload) -> EventKind {
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, SystemEnteredEvent>)
            return EventKind::SystemEntered;
        else if constexpr (std::is_same_v<T, BodyEnteredEvent>)
            return EventKind::BodyEntered;
        else if constexpr (std::is_same_v<T, QuestStageCompletedEvent>)
            return EventKind::QuestStageCompleted;
    }, ev);
}

HandlerId EventBus::subscribe(EventKind kind, EventHandler handler) {
    HandlerId id = next_id_++;
    subs_.push_back({id, kind, std::move(handler)});
    return id;
}

void EventBus::unsubscribe(HandlerId id) {
    subs_.erase(std::remove_if(subs_.begin(), subs_.end(),
                               [id](const Subscription& s) { return s.id == id; }),
                subs_.end());
}

void EventBus::emit(Game& game, const Event& ev) {
    EventKind kind = event_kind_of(ev);
    // Copy list to allow handlers to subscribe/unsubscribe during emit.
    auto snapshot = subs_;
    for (auto& s : snapshot) {
        if (s.kind == kind) s.handler(game, ev);
    }
}

void EventBus::clear() {
    subs_.clear();
    next_id_ = 1;
}

} // namespace astra
```

- [ ] **Step 3: Add to `CMakeLists.txt`**

Find the `set(ASTRA_SOURCES ...)` block and insert `src/event_bus.cpp` in alphabetical/grouped position (e.g. near `src/faction.cpp`).

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: clean build. If `std::visit` with lambda returning an enum fails on the compiler, confirm C++20 is being applied.

- [ ] **Step 5: Commit**

```bash
git add include/astra/event_bus.h src/event_bus.cpp CMakeLists.txt
git commit -m "feat(events): add minimal EventBus with three event types"
```

---

## Task 5: Wire EventBus onto Game + register_all_scenarios stub

**Files:**
- Modify: `include/astra/game.h`
- Modify: `src/game.cpp`
- Create: `include/astra/scenarios.h`
- Create: `src/scenarios/stage4_hostility.cpp` (stub)
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add EventBus member + accessor to `include/astra/game.h`**

Include `"astra/event_bus.h"` and inside the `Game` class:

```cpp
public:
    EventBus& event_bus() { return event_bus_; }
    const EventBus& event_bus() const { return event_bus_; }

private:
    EventBus event_bus_;
```

- [ ] **Step 2: Create `include/astra/scenarios.h`**

```cpp
#pragma once

namespace astra {
class Game;

// Called once per session after world is loaded / new game is created.
// Each scenario subscribes its own event handlers. Idempotent: on load,
// the bus was cleared, so calling this again is safe.
void register_all_scenarios(Game& game);

// Individual registrations — expose for tests / debug console.
void register_stage4_hostility_scenario(Game& game);

} // namespace astra
```

- [ ] **Step 3: Create `src/scenarios/stage4_hostility.cpp` as stub**

```cpp
#include "astra/scenarios.h"
#include "astra/game.h"

namespace astra {

void register_stage4_hostility_scenario(Game& /*game*/) {
    // Implementation lands in Task 9.
}

void register_all_scenarios(Game& game) {
    register_stage4_hostility_scenario(game);
}

} // namespace astra
```

- [ ] **Step 4: Call `register_all_scenarios` from Game lifecycle**

In `src/game.cpp`, find the function(s) that correspond to "start a new game" and "finish loading a save." After world state is initialized in each path, add:

```cpp
event_bus_.clear();
register_all_scenarios(*this);
```

The `clear()` guarantees a clean slate on reload.

- [ ] **Step 5: Add `src/scenarios/stage4_hostility.cpp` to `CMakeLists.txt`**

- [ ] **Step 6: Build + launch the game and confirm no regression**

Run: `cmake --build build -j && ./build/astra`
Play briefly, save, load. Expected: behaves identically to before.

- [ ] **Step 7: Commit**

```bash
git add include/astra/game.h src/game.cpp include/astra/scenarios.h \
        src/scenarios/stage4_hostility.cpp CMakeLists.txt
git commit -m "feat(scenarios): wire EventBus into Game lifecycle"
```

---

## Task 6: Scenario effect primitives

**Files:**
- Create: `include/astra/scenario_effects.h`
- Create: `src/scenario_effects.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write `include/astra/scenario_effects.h`**

```cpp
#pragma once

#include <string>
#include <vector>

namespace astra {

class Game;

// ─── Effect primitives ────────────────────────────────────────────
// Thin wrappers around existing subsystems (faction.h, world_manager,
// playback_viewer). Scenarios compose these; they do not reach into
// subsystems directly. This is how the effect vocabulary accumulates.

// Shift the player's standing with a named faction by delta.
void shift_faction_standing(Game& game, const std::string& faction, int delta);

// Set or clear a named world flag.
void set_world_flag(Game& game, const std::string& flag, bool value);

// Display an incoming transmission modal. Blocks input until dismissed.
// Implemented via the existing playback_viewer AudioLog style.
void open_transmission(Game& game,
                       const std::string& header,
                       const std::vector<std::string>& lines);

// Inject ambient NPC spawns into a LocationKey via QuestLocationMeta.
// The next time the player enters this location, the named NPC roles
// spawn as ambient encounter.
void inject_location_encounter(Game& game,
                               uint32_t system_id,
                               int body_index,
                               bool is_station,
                               const std::vector<std::string>& npc_roles,
                               const std::string& source_tag);

} // namespace astra
```

- [ ] **Step 2: Write `src/scenario_effects.cpp`**

```cpp
#include "astra/scenario_effects.h"

#include "astra/faction.h"
#include "astra/game.h"
#include "astra/player.h"
#include "astra/playback_viewer.h"
#include "astra/world_manager.h"

namespace astra {

void shift_faction_standing(Game& game, const std::string& faction, int delta) {
    modify_faction_standing(game.player(), faction, delta);
}

void set_world_flag(Game& game, const std::string& flag, bool value) {
    game.world().set_world_flag(flag, value);
}

void open_transmission(Game& game,
                       const std::string& header,
                       const std::vector<std::string>& lines) {
    game.playback_viewer().open(PlaybackStyle::AudioLog, header, lines);
}

void inject_location_encounter(Game& game,
                               uint32_t system_id,
                               int body_index,
                               bool is_station,
                               const std::vector<std::string>& npc_roles,
                               const std::string& source_tag) {
    LocationKey key = {system_id, body_index, -1, is_station, -1, -1, 0};
    QuestLocationMeta meta;
    meta.quest_id = source_tag;
    meta.quest_title = "";
    meta.target_system_id = system_id;
    meta.target_body_index = body_index;
    meta.npc_roles = npc_roles;
    meta.remove_on_completion = false;
    game.world().quest_locations()[key] = std::move(meta);
}

} // namespace astra
```

Verify `game.player()` accessor exists — if not, use the actual getter (check `game.h`). Same for `game.world()` and `game.playback_viewer()`.

- [ ] **Step 3: Add to `CMakeLists.txt` and build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add include/astra/scenario_effects.h src/scenario_effects.cpp CMakeLists.txt
git commit -m "feat(scenarios): add effect primitive library"
```

---

## Task 7: SystemEntered producer in warp path

**Files:**
- Modify: `src/game_world.cpp` (or whichever function advances `nav.current_system_id`)

- [ ] **Step 1: Locate the warp boundary**

Grep for writes to `current_system_id` in `src/` to find the one function that transitions between systems (not the ones that merely read it for LocationKey construction):

```
grep -nE "nav\.current_system_id\s*=" src/*.cpp src/**/*.cpp
```

Typical candidate: the "warp arrive" function in `ship.cpp` or `game_world.cpp`.

- [ ] **Step 2: Emit `SystemEnteredEvent` at the transition**

Capture the previous id before reassignment, then after the reassignment + any other state updates:

```cpp
#include "astra/event_bus.h"

// ... inside warp-arrive function, at the bottom ...
SystemEnteredEvent ev;
ev.system_id = nav.current_system_id;
ev.previous_system_id = prev_system_id;
game.event_bus().emit(game, ev);
```

Do **not** emit on initial game load — only when the player actively warps. Check surrounding code for an existing load-vs-warp branch.

- [ ] **Step 3: Build + play briefly**

Run: `cmake --build build -j && ./build/astra`
Warp between two systems to confirm nothing crashes. No visible behavior yet (no subscribers).

- [ ] **Step 4: Commit**

```bash
git add src/game_world.cpp   # or wherever the change landed
git commit -m "feat(events): emit SystemEntered on warp arrival"
```

---

## Task 8: Conclave Sentry NPC archetype

**Files:**
- Create: `src/npcs/conclave_sentry.cpp`
- Modify: `include/astra/npc_defs.h` (or wherever `NpcRole` / role dispatch lives)
- Modify: `src/npc_spawner.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add `ConclaveSentry` to the NpcRole enum / role name table**

Find the existing enum (grep for `ArchonRemnant` in `include/astra/`). Add `ConclaveSentry` next to other faction sentries. Add a string name `"Conclave Sentry"` to whatever role-to-name mapping exists.

- [ ] **Step 2: Write `src/npcs/conclave_sentry.cpp`**

Model after `src/npcs/archon_remnant.cpp`. Key settings:

```cpp
#include "astra/npc.h"
#include "astra/faction.h"

namespace astra {

Npc make_conclave_sentry(int level) {
    Npc n;
    n.name = "Conclave Sentry";
    n.glyph = 'S';
    n.color = 135;  // Stellari resonance color, matches signal fixtures
    n.faction = Faction_StellariConclave;
    n.role_name = "Conclave Sentry";
    n.max_hp = 30 + level * 4;
    n.hp = n.max_hp;
    n.damage_type = DamageType::Plasma;
    n.base_damage = 6 + level / 2;
    n.defense_value = 8 + level / 2;
    n.armor_value = 4;
    n.xp_award = 40;
    n.aggressive = true;
    n.behavior = NpcBehavior::Hostile;
    return n;
}

} // namespace astra
```

Adjust field names/types to match whatever `Npc` actually uses (mirror `archon_remnant.cpp`).

- [ ] **Step 3: Hook role dispatch in `src/npc_spawner.cpp`**

Where the spawner switches on role name/enum, add a case for `ConclaveSentry` calling `make_conclave_sentry(level)`.

- [ ] **Step 4: Add to `CMakeLists.txt`**

- [ ] **Step 5: Smoke-test via dev console**

Run: `cmake --build build -j && ./build/astra`
Use the dev console spawn command (see `src/dev_console.cpp`) to spawn a Conclave Sentry directly. Confirm it renders with glyph `S`, is hostile, and fights using the dice combat system.

- [ ] **Step 6: Commit**

```bash
git add include/astra/npc_defs.h src/npcs/conclave_sentry.cpp \
        src/npc_spawner.cpp CMakeLists.txt
git commit -m "feat(npc): add Conclave Sentry archetype"
```

---

## Task 9: Stage 4 Hostility scenario implementation

**Files:**
- Modify: `src/scenarios/stage4_hostility.cpp`
- Modify: `src/quests/stellar_signal_beacon.cpp`

- [ ] **Step 1: Fill in `src/scenarios/stage4_hostility.cpp`**

Replace the stub with:

```cpp
#include "astra/scenarios.h"

#include "astra/event_bus.h"
#include "astra/faction.h"
#include "astra/game.h"
#include "astra/scenario_effects.h"
#include "astra/world_manager.h"

namespace astra {

namespace {
constexpr const char* kStage4Active = "stage4_active";
constexpr const char* kTransmissionSeen = "stage4_transmission_seen";

int ambush_count_for_level(int level) {
    if (level < 5) return 1;
    if (level < 10) return 2;
    return 3;
}
} // namespace

void register_stage4_hostility_scenario(Game& game) {
    game.event_bus().subscribe(EventKind::SystemEntered,
        [](Game& g, const Event& ev) {
            auto& payload = std::get<SystemEnteredEvent>(ev);
            auto& world = g.world();

            if (!world.world_flag(kStage4Active)) return;
            if (world.ambushed_systems().count(payload.system_id)) return;

            // First warp after stage 3 completion fires the transmission.
            if (!world.world_flag(kTransmissionSeen)) {
                open_transmission(g,
                    "INCOMING TRANSMISSION — STELLARI CONCLAVE",
                    {
                        "Commander. You have interfered with a sacred cycle.",
                        "Cease immediately and return to Sgr A* trajectory.",
                        "",
                        "This is your only warning.",
                    });
                set_world_flag(g, kTransmissionSeen, true);
            }

            // Queue an ambush in the system's overworld / inbound location.
            int level = g.player().level;
            int count = ambush_count_for_level(level);
            std::vector<std::string> roles(count, "Conclave Sentry");

            // Body index 0 is the first body; we use the overworld key
            // (is_station=false) so the ambush meets the player on the
            // primary planet. Adjust if the Astra warp model drops the
            // player somewhere else on arrival.
            inject_location_encounter(g,
                payload.system_id, 0, false,
                roles,
                "stage4_ambush");

            world.ambushed_systems().insert(payload.system_id);
        });
}

} // namespace astra
```

- [ ] **Step 2: Trigger Stage 4 from beacon quest completion**

In `src/quests/stellar_signal_beacon.cpp`, extend `on_completed`:

```cpp
#include "astra/scenario_effects.h"
#include "astra/faction.h"

// ... inside on_completed, after the existing playback_viewer.open(...) call:
shift_faction_standing(game, Faction_StellariConclave, -300);
set_world_flag(game, "stage4_active", true);
```

Do **not** emit a `QuestStageCompleted` event here — the scenario reacts to `SystemEntered` with `stage4_active`, which flips on at exactly this moment. Emitting another event would be redundant for this slice.

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 4: In-game smoke test**

Run `./build/astra` in dev mode. Use the dev console to:
  1. Accept and complete Stages 1, 2, 3 of the Stellar Signal arc (or console-advance them if a helper exists; otherwise play through — stage 3 takes minutes once stages 1-2 data is primed).
  2. Confirm on completion of Stage 3: Stellari Conclave standing drops by 300 (check Character tab → Reputation).
  3. Warp to a fresh system. Expected: Conclave transmission modal appears, close it, arrive in overworld, 1–3 Conclave Sentries are hostile and attack.
  4. Warp to a second fresh system. Expected: no transmission, but ambush still spawns.
  5. Return to the first system. Expected: no ambush re-spawn (already in `ambushed_systems`).
  6. Save, quit, relaunch, load. Expected: flags and ambushed-systems set persist; re-warping into a fresh system still spawns.

- [ ] **Step 5: Commit**

```bash
git add src/scenarios/stage4_hostility.cpp src/quests/stellar_signal_beacon.cpp
git commit -m "feat(stellar-signal): Stage 4 hostility & ambush scenario"
```

---

## Task 10: Roadmap + docs update

**Files:**
- Modify: `docs/roadmap.md` — tick any relevant Stage 4 boxes
- Modify: `docs/plans/stellar_signal_phase4_5_gaps.md` — mark "Faction ambush events" and "Incoming transmission UI" as done

- [ ] **Step 1: Tick roadmap boxes**

Open `docs/roadmap.md` and mark completed items. If no matching row exists for Stage 4 hostility, add one under the Stellar Signal arc heading.

- [ ] **Step 2: Update gap analysis status**

In `docs/plans/stellar_signal_phase4_5_gaps.md`, change the status for:
- "Faction ambush events at jump points" → Done
- "Incoming transmission UI (comms modal)" → Done (reused playback_viewer)
- "Archon Remnants increased galaxy-wide spawning" → still Missing (separate scenario later)

- [ ] **Step 3: Commit**

```bash
git add docs/roadmap.md docs/plans/stellar_signal_phase4_5_gaps.md
git commit -m "docs: mark Stage 4 hostility slice complete"
```

---

## Final Verification Checklist

Before declaring the slice done, manually confirm:

- [ ] Clean build on default target (`cmake --build build -j`)
- [ ] Clean build with `-DSDL=ON` (optional but encouraged)
- [ ] Save/load round-trip does not corrupt world flags or ambushed_systems
- [ ] Completing Stage 3 drops Conclave standing to Hated tier (-300 or lower)
- [ ] First post-Stage-3 warp shows transmission modal exactly once
- [ ] Subsequent warps into unseen systems spawn Conclave Sentries
- [ ] Re-entering a system that was already ambushed does NOT respawn
- [ ] Existing Stellari Conclave NPCs in already-generated systems are hostile on sight (free win via `is_hostile_to_player`)
- [ ] No dev-mode console spam / warnings

---

## Open questions to resolve during or after this slice

These are explicit deferrals — answer as scenarios accumulate, not up front:

1. **Ambush pacing** — should every fresh system ambush, or only a subset (e.g. 50% roll, or only systems with existing Conclave presence)?
2. **Rep recovery** — if the player finds a way to raise Conclave standing above Hated, should ambushes stop mechanically, or should this slice be narrative-locked via `stage4_active`? (Current code is narrative-locked.)
3. **Encounter placement** — is body index 0 always the right ambush slot, or should Stage 4 ambushes land at jump-point proximity / random body? Revisit once the siege scenario lands and we see the pattern second time.
4. **Handler lifetime** — the scenario currently subscribes forever. When Stage 4 resolves (Stage 5 start), we probably want to unsubscribe. Add an "end Stage 4" scenario effect in the next slice rather than here.
5. **Multiple handlers same event** — when the siege scenario also subscribes to `SystemEntered`, do we care about ordering? Likely no, but note if it becomes ambiguous.
