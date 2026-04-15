# Custom Star Systems Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let story quests create custom star systems on demand (Nova Stage 3 beacon system is the motivating case) with an idiomatic one-liner, plus a thin reveal/hide API over the existing `StarSystem::discovered` flag.

**Architecture:** A new `CustomSystemSpec` + `add_custom_system(nav, spec)` API allocates ids from a counter starting at `0x80000000` stored on `NavigationData` and persisted through save/load. Bodies can be pre-filled via the spec; otherwise the existing lazy generator runs on first access. A new `body_presets.h/.cpp` grows per-quest with factory functions (`make_asteroid_orbit`, later `make_paradise_planet`, etc.). `reveal_system(nav, id)` / `hide_system(nav, id)` flip `discovered` by id.

**Tech Stack:** C++20, existing NavigationData + star chart. No new deps. No test harness in the repo — verification is build + dev-console smoke.

**Spec:** `docs/superpowers/specs/2026-04-14-custom-star-systems-design.md`

**Save version:** bumps v30 → v31 (STAR section gains one trailing u32).

**Worktree:** run this plan in `.worktrees/custom-star-systems` on branch `feat/custom-star-systems`, forked from `main`.

---

## File Structure

| File | Kind | Responsibility |
|---|---|---|
| `include/astra/star_chart.h` | MODIFY | `CustomSystemSpec`, `NavigationData::next_custom_system_id`, `add_custom_system`/`reveal_system`/`hide_system` declarations |
| `src/star_chart.cpp` | MODIFY | Implementations |
| `include/astra/body_presets.h` | NEW | Factory function declarations |
| `src/body_presets.cpp` | NEW | Factory impl (`make_asteroid_orbit`) |
| `CMakeLists.txt` | MODIFY | Add `src/body_presets.cpp` to `ASTRA_SOURCES` |
| `include/astra/save_file.h` | MODIFY | Bump `SaveData::version` default to 31 |
| `src/save_file.cpp` | MODIFY | Write/read `next_custom_system_id` in STAR section; gate read on v31 |
| `src/dev_console.cpp` | MODIFY | New `chart` top-level verb with `create`/`reveal`/`hide` subcommands |

Build: `cmake --build build` (DEV build already configured).
Run: `./build/astra-dev --term`.
Commit style: `feat(starchart): …`, `feat(save): …`, `feat(dev): …`.

---

### Task 1: Add `CustomSystemSpec` + counter field

**Files:**
- Modify: `include/astra/star_chart.h:49` (near `StarSystem`), `include/astra/star_chart.h:67` (NavigationData)

- [ ] **Step 1: Declare the spec**

In `include/astra/star_chart.h`, add the following struct AFTER the `StarSystem` definition and BEFORE `NavigationData` (around line 66 — between the two structs). Include `astra/celestial_body.h` at the top if not already pulled in (it already is, because StarSystem holds `std::vector<CelestialBody> bodies`).

```cpp
struct CustomSystemSpec {
    std::string name;
    float gx = 0.0f;
    float gy = 0.0f;
    StarClass star_class = StarClass::ClassG;
    bool discovered = true;
    bool binary = false;
    bool has_station = false;
    LoreAnnotation lore = {};
    std::vector<CelestialBody> bodies;        // empty = lazy procedural
};
```

- [ ] **Step 2: Add the counter field to NavigationData**

In `NavigationData` (around line 67-75), append at the end of the struct (all existing fields are public, keep it public — NavigationData is a POD-style struct with no trailing-underscore convention):

```cpp
    uint32_t next_custom_system_id = 0x80000000u;
```

- [ ] **Step 3: Add function declarations**

Below `NavigationData`, at namespace scope (alongside the existing free functions like `generate_galaxy` and `generate_system_bodies`), add:

```cpp
// Create a custom system and append it to nav.systems. Returns the allocated id.
// IDs come from nav.next_custom_system_id (starts at 0x80000000); the counter
// survives save/load. If spec.bodies is non-empty, bodies are moved in and
// bodies_generated is set to true. Empty spec.bodies leaves bodies_generated
// false so the lazy generator runs on first access.
uint32_t add_custom_system(NavigationData& nav, CustomSystemSpec spec);

// Set discovered=true for the system with this id. Returns false if not found.
bool reveal_system(NavigationData& nav, uint32_t system_id);

// Symmetry helper; sets discovered=false.
bool hide_system(NavigationData& nav, uint32_t system_id);
```

- [ ] **Step 4: Build**

Run: `cmake --build build`
Expected: clean build. No callers yet, but declarations must compile.

- [ ] **Step 5: Commit**

```bash
git add include/astra/star_chart.h
git commit -m "$(cat <<'EOF'
feat(starchart): CustomSystemSpec + custom-id counter + API decl

Adds the spec struct, the next_custom_system_id counter on
NavigationData (default 0x80000000), and declarations for
add_custom_system / reveal_system / hide_system. Implementations
land in the next commit.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Implement `add_custom_system`, `reveal_system`, `hide_system`

**Files:**
- Modify: `src/star_chart.cpp` — append the three functions at the end of the file (above any trailing `} // namespace astra` closer)

- [ ] **Step 1: Implement `add_custom_system`**

In `src/star_chart.cpp`, add at the end of the namespace body:

```cpp
uint32_t add_custom_system(NavigationData& nav, CustomSystemSpec spec) {
    // Allocate an id, stepping past any collision (extremely unlikely).
    uint32_t id = nav.next_custom_system_id;
    while (std::any_of(nav.systems.begin(), nav.systems.end(),
                       [id](const StarSystem& s){ return s.id == id; })) {
        ++id;
    }
    nav.next_custom_system_id = id + 1;

    StarSystem sys;
    sys.id = id;
    sys.name = std::move(spec.name);
    sys.star_class = spec.star_class;
    sys.binary = spec.binary;
    sys.has_station = spec.has_station;
    sys.gx = spec.gx;
    sys.gy = spec.gy;
    sys.discovered = spec.discovered;
    sys.lore = std::move(spec.lore);
    sys.planet_count = 0;
    sys.asteroid_belts = 0;
    sys.danger_level = 1;

    if (!spec.bodies.empty()) {
        sys.bodies = std::move(spec.bodies);
        sys.bodies_generated = true;
    } else {
        sys.bodies_generated = false;
    }

    nav.systems.push_back(std::move(sys));
    return id;
}
```

Add `#include <algorithm>` at the top if not already present.

- [ ] **Step 2: Implement `reveal_system` and `hide_system`**

Append:

```cpp
static bool set_discovered(NavigationData& nav, uint32_t system_id, bool value) {
    for (auto& s : nav.systems) {
        if (s.id == system_id) {
            s.discovered = value;
            return true;
        }
    }
    return false;
}

bool reveal_system(NavigationData& nav, uint32_t system_id) {
    return set_discovered(nav, system_id, true);
}

bool hide_system(NavigationData& nav, uint32_t system_id) {
    return set_discovered(nav, system_id, false);
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add src/star_chart.cpp
git commit -m "$(cat <<'EOF'
feat(starchart): implement add_custom_system / reveal_system / hide_system

ID allocation walks past any (extremely unlikely) procedural collision
before committing the counter. Body pre-fill sets bodies_generated=true
so the lazy generator skips; empty bodies keep the existing procedural
behavior on first access.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: `body_presets.h/.cpp` + `make_asteroid_orbit`

**Files:**
- Create: `include/astra/body_presets.h`
- Create: `src/body_presets.cpp`
- Modify: `CMakeLists.txt` — add `src/body_presets.cpp` to `ASTRA_SOURCES`

- [ ] **Step 1: Write the header**

`include/astra/body_presets.h`:

```cpp
#pragma once

#include "astra/celestial_body.h"

#include <string>

namespace astra {

// A bare asteroid intended to host a single quest fixture on its detail map.
// Used by Nova Stage 3's beacon system.
CelestialBody make_asteroid_orbit(std::string name);

} // namespace astra
```

- [ ] **Step 2: Write the implementation**

`src/body_presets.cpp`:

```cpp
#include "astra/body_presets.h"

namespace astra {

CelestialBody make_asteroid_orbit(std::string name) {
    CelestialBody b;
    b.name = std::move(name);
    b.type = BodyType::AsteroidBelt;
    b.atmosphere = Atmosphere::None;
    b.temperature = Temperature::Cold;
    b.size = 1;
    b.moons = 0;
    b.orbital_distance = 1.0f;
    b.landable = true;
    b.explored = false;
    b.has_dungeon = false;
    b.danger_level = 1;
    b.day_length = 200;
    return b;
}

} // namespace astra
```

- [ ] **Step 3: Register in CMake**

Open `CMakeLists.txt`. Find `src/body_mapping.cpp` or similar `src/body_*.cpp` in `ASTRA_SOURCES` (if any). Insert `src/body_presets.cpp` in alphabetical position (after `src/body_*.cpp` entries, before `src/camera.cpp` etc.). If no `src/body_*.cpp` neighbors exist, find the nearest alphabetical slot.

- [ ] **Step 4: Build**

Run: `cmake --build build`
Expected: clean build. The new TU links into `astra-dev`.

- [ ] **Step 5: Commit**

```bash
git add include/astra/body_presets.h src/body_presets.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(starchart): body_presets.h — make_asteroid_orbit

First of many per-quest CelestialBody factory helpers. Grows as new
arcs need specific body archetypes (paradise planet, scar planet,
neutron-crystal asteroid, ...).

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: Save/load — bump to v31 and serialize the counter

**Files:**
- Modify: `include/astra/save_file.h:63` — version bump
- Modify: `src/save_file.cpp:824-875` — write_navigation_section
- Modify: `src/save_file.cpp:1623-~1700` — read_navigation_section

- [ ] **Step 1: Bump version**

In `include/astra/save_file.h` around line 63, change:

```cpp
uint32_t version = 31;   // v31: NavigationData.next_custom_system_id
```

- [ ] **Step 2: Write the counter at the end of STAR**

In `src/save_file.cpp::write_navigation_section` (starting at line 824), just before `w.end_section(pos);` at line 874, add:

```cpp
    // v31: custom system id counter
    w.write_u32(nav.next_custom_system_id);
```

- [ ] **Step 3: Read it back, gated**

In `src/save_file.cpp::read_navigation_section` (starting at line 1623), find the end of the per-system for-loop (around the closing brace matching `for (uint32_t i = 0; i < count; ++i)`). Immediately after that loop closes, add:

```cpp
    if (version >= 31) {
        nav.next_custom_system_id = r.read_u32();
    }
    // else: default 0x80000000u from NavigationData's in-class initializer
```

The read function takes `version` as a parameter already — use it.

- [ ] **Step 4: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 5: Smoke test — save/reload round-trip**

Run: `./build/astra-dev --term`

Start a new game, save (game menu), quit to main menu, load. Expect no version error. Quit. This only confirms the new reader doesn't misparse older/newer formats; a proper round-trip of the counter happens in Task 6's smoke test.

- [ ] **Step 6: Commit**

```bash
git add include/astra/save_file.h src/save_file.cpp
git commit -m "$(cat <<'EOF'
feat(save): v31 — serialize NavigationData.next_custom_system_id

Appended at the end of the STAR section. Older saves load with the
default counter (0x80000000), which is correct — they have no
custom systems to collide with yet.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: Dev-console `chart` verb

**Files:**
- Modify: `src/dev_console.cpp` — add the top-level verb dispatch

- [ ] **Step 1: Identify the dispatch block**

In `src/dev_console.cpp` around lines 104-147, the dev console dispatches on the first token (`warp`, `quest`, `lore`, etc.). Add a new `else if (verb == "chart")` block. Place it next to the existing `lore` branch for locality.

- [ ] **Step 2: Add includes**

At the top of `src/dev_console.cpp`, confirm these are present (add if missing):

```cpp
#include "astra/star_chart.h"
#include "astra/body_presets.h"
```

- [ ] **Step 3: Write the handler**

Add (inside the top-level verb dispatch, after the `lore` branch):

```cpp
    else if (verb == "chart") {
        auto& nav = game.world().navigation();
        if (args.size() >= 2 && args[1] == "create") {
            std::string name = args.size() >= 3 ? args[2] : std::string("Custom");
            // Place near Sol so it's easy to find on the chart.
            float dx = 1.5f + 0.3f * static_cast<float>(nav.systems.size() % 7);
            CustomSystemSpec spec;
            spec.name = name;
            spec.gx = dx;
            spec.gy = 1.0f;
            spec.star_class = StarClass::ClassM;
            spec.discovered = true;
            spec.bodies = { make_asteroid_orbit(name + " Rock") };
            uint32_t id = add_custom_system(nav, std::move(spec));
            log("Created custom system '" + name + "' id=" + std::to_string(id) +
                " at (" + std::to_string(dx) + ", 1.0)");
        } else if (args.size() >= 2 && args[1] == "reveal") {
            if (args.size() < 3) { log("Usage: chart reveal <name-substring>"); return; }
            std::string needle = args[2];
            for (auto& s : nav.systems) {
                if (s.name.find(needle) != std::string::npos) {
                    if (reveal_system(nav, s.id)) {
                        log("Revealed '" + s.name + "' (id=" + std::to_string(s.id) + ")");
                    }
                    return;
                }
            }
            log("No system matches '" + needle + "'");
        } else if (args.size() >= 2 && args[1] == "hide") {
            if (args.size() < 3) { log("Usage: chart hide <name-substring>"); return; }
            std::string needle = args[2];
            for (auto& s : nav.systems) {
                if (s.name.find(needle) != std::string::npos) {
                    if (hide_system(nav, s.id)) {
                        log("Hid '" + s.name + "' (id=" + std::to_string(s.id) + ")");
                    }
                    return;
                }
            }
            log("No system matches '" + needle + "'");
        } else {
            log("Usage: chart create [name]|reveal <name>|hide <name>");
        }
    }
```

The `args` vector splits by whitespace — the existing handlers (`lore warp`, `quest scout`, etc.) follow the same pattern.

- [ ] **Step 4: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add src/dev_console.cpp
git commit -m "$(cat <<'EOF'
feat(dev): chart verb — create / reveal / hide custom systems

Dev-console smoke-test surface for custom systems. `chart create`
allocates an id, places a one-asteroid system near Sol, and reports
the id. `chart reveal`/`chart hide` flip discovered by name substring.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: End-to-end smoke test

**Files:** none modified — manual verification.

- [ ] **Step 1: Launch**

```
./build/astra-dev --term
```

- [ ] **Step 2: Create a system**

Start a new game. Open dev console (backtick). Run:

```
chart create Testville
```

Expected log: `Created custom system 'Testville' id=2147483648 at (...)` (`0x80000000` == 2147483648).

- [ ] **Step 3: Verify on the chart**

Close the console. Open the star chart (`m` in dev mode). Zoom out to the galaxy view — `Testville` should appear near Sol as a discovered dot. Pan/zoom to confirm.

- [ ] **Step 4: Hide/reveal cycle**

Back to dev console:

```
chart hide Testville
```

Reopen chart — system should be hidden (not rendered, or rendered in dark-gray depending on the zoom level's hidden style).

```
chart reveal Testville
```

Reopen chart — system visible again.

- [ ] **Step 5: Save/reload round-trip**

Save via game menu. Quit to main menu. Load.

Open dev console:

```
chart create Second
```

Expected log: `id=2147483649` (counter advanced — counter survived save/load).

Open chart — both `Testville` and `Second` visible.

- [ ] **Step 6: Commit a test evidence note (optional)**

No source changes. If you want a stamp, commit an empty `--allow-empty` with the test log. Otherwise skip.

---

## Acceptance Criteria

- `cmake --build build` is clean at every commit.
- `chart create Foo` creates a system with id ≥ `0x80000000`, visible on the star chart near Sol.
- `chart reveal <name>` / `chart hide <name>` flip discovery.
- After save → quit → load → `chart create Bar`, the new id is the counter's pre-save value + 1 (not reset to `0x80000000`).
- No existing save-loading path breaks (v30 saves load without complaint; counter defaults to `0x80000000` when reading older saves).
- The NovaStellarSignalQuest work is unblocked (it'll land in a separate plan, but will be a pure call-site of `add_custom_system` + `make_asteroid_orbit` + `QuestFixture` registration).

---

## Out of Scope (explicitly deferred)

- Removing custom systems from `nav.systems` (no in-tree caller needs it).
- Reveal animation / sound.
- Auto-warp-to-newly-revealed.
- System name uniqueness / collision UX.
- More body presets — added per-quest as needed.
- Exposing `next_custom_system_id` publicly for caller-supplied ids.
