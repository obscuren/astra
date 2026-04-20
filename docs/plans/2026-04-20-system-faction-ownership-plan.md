# System Faction Ownership — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give every `StarSystem` a `controlling_faction`, generate the ownership map deterministically from the galaxy seed with clustered territories, render territorial bands on the galaxy chart with a toggle, and gate the Stage 4 hostility scenario to Conclave-controlled space.

**Architecture:** New header `faction_territory.h` owns the generation algorithm (capital placement, territorial assignment, noise, FactionMap precompute). `NavigationData` grows two fields (`controlling_faction` on each `StarSystem`, `FactionMap faction_map`), both regenerated on load so no save-format bump. `star_chart_viewer.cpp` paints muted background tints in galaxy view only, gated behind a session-local toggle (default on). `stage4_hostility` scenario gains one filter line.

**Tech Stack:** C++20, existing Astra conventions (snake_case_ members, `#pragma once`, namespace `astra`, headers in `include/astra/`, sources in `src/`). No tests — validation is build + in-game smoke test.

**Related docs:**
- `docs/plans/2026-04-20-system-faction-ownership-spec.md` — design spec (read first)
- `docs/plans/scenario_graph_vision.md` — scenario-graph architecture
- `docs/plans/stellar_signal_phase4_5_gaps.md` — gap analysis (faction-space gating referenced here)

---

## File Structure

**New files:**

- `include/astra/faction_territory.h` — public API: `FactionMap` type, `assign_system_factions()`, `faction_at_coord()`, `is_unclaimed()`
- `src/generators/faction_territory.cpp` — generation algorithm + FactionMap construction + coord lookup

**Modified files:**

- `include/astra/star_chart.h` — add `controlling_faction` to `StarSystem`; add optional `controlling_faction` to `CustomSystemSpec`; add `FactionMap faction_map` to `NavigationData`
- `src/star_chart.cpp` — `add_custom_system` respects `CustomSystemSpec::controlling_faction`; include path for faction_territory (if needed)
- `src/game.cpp` — call `assign_system_factions` at both `generate_galaxy → apply_lore_to_galaxy` sites
- `src/save_system.cpp` — call `assign_system_factions` after `apply_lore_to_galaxy` in the load path
- `src/star_chart_viewer.cpp` — background tint layer in galaxy view, toggle handler, hover popup faction line
- `src/quests/stellar_signal_beacon.cpp` — set explicit `controlling_faction = ""` on the beacon system
- `src/scenarios/stage4_hostility.cpp` — filter ambush to Conclave-owned systems only; keep transmission unconditional
- `CMakeLists.txt` — add `src/generators/faction_territory.cpp` to `ASTRA_SOURCES`
- `docs/roadmap.md` — tick new Stage 4 row
- `docs/plans/stellar_signal_phase4_5_gaps.md` — note the "Faction-space gating" item is done

---

## Task 1: Add `controlling_faction` to `StarSystem` and `CustomSystemSpec`

**Files:**
- Modify: `include/astra/star_chart.h:52-81` (StarSystem struct), `:70-81` (CustomSystemSpec struct)

- [ ] **Step 1: Add field to `StarSystem`**

Open `include/astra/star_chart.h`. Inside the `StarSystem` struct (starts around line 52), add after the `lore` field:

```cpp
    LoreAnnotation lore;                // populated by apply_lore_to_galaxy()
    std::string controlling_faction;    // "" = Unclaimed; else one of Faction_* constants
```

- [ ] **Step 2: Add optional field to `CustomSystemSpec`**

In the same file, inside the `CustomSystemSpec` struct, add `<optional>` include at the top if not already present:

```cpp
#include <optional>
```

Then inside the struct add:

```cpp
    LoreAnnotation lore = {};
    // nullopt = compute from territorial assignment; else use this value verbatim
    // (including "" for explicit Unclaimed).
    std::optional<std::string> controlling_faction;
```

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: clean build. No behavior change yet — field defaults to `""`.

- [ ] **Step 4: Commit**

```bash
git add include/astra/star_chart.h
git commit -m "$(cat <<'EOF'
feat(star-chart): add controlling_faction field to StarSystem

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Create `faction_territory.h` — public API skeleton

**Files:**
- Create: `include/astra/faction_territory.h`

- [ ] **Step 1: Write the header**

Create `include/astra/faction_territory.h`:

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

struct NavigationData;
struct StarSystem;

// ─── FactionMap ──────────────────────────────────────────────────
// Precomputed galaxy-space → faction-index grid. Used by the renderer
// to tint the background of every cell on the galaxy chart, including
// the empty space between stars.

constexpr int kFactionMapWidth  = 256;
constexpr int kFactionMapHeight = 256;

// Tight packing — one byte per cell, index into the faction palette.
enum class FactionTerritory : uint8_t {
    Unclaimed        = 0,
    StellariConclave = 1,
    TerranFederation = 2,
    KrethMiningGuild = 3,
    VeldraniAccord   = 4,
};

struct FactionMap {
    // Row-major, size = kFactionMapWidth * kFactionMapHeight.
    // Empty before assign_system_factions runs.
    std::vector<FactionTerritory> cells;

    // Galaxy-space bounds the grid covers (min/max gx, gy). Used to map
    // floating-point coords to cell indexes.
    float gx_min = 0.0f;
    float gx_max = 0.0f;
    float gy_min = 0.0f;
    float gy_max = 0.0f;

    bool empty() const { return cells.empty(); }
};

// ─── Generation ──────────────────────────────────────────────────

// Assign every system a controlling_faction deterministically from the
// provided seed, then populate nav.faction_map for renderer lookups.
// Idempotent — calling twice overwrites. Safe to call after
// apply_lore_to_galaxy (and should be).
void assign_system_factions(NavigationData& nav, uint32_t seed);

// ─── Queries ─────────────────────────────────────────────────────

inline bool is_unclaimed(const StarSystem& s);

// Galaxy-space coord → faction label. Returns "" for Unclaimed or out-of-bounds.
// Cheap O(1) lookup against the precomputed FactionMap.
std::string faction_at_coord(const NavigationData& nav, float gx, float gy);

// Enum ↔ string helpers.
FactionTerritory faction_enum_from_name(const std::string& faction);
const char* faction_name_from_enum(FactionTerritory t);

} // namespace astra

// Inline definition must see the full StarSystem definition.
#include "astra/star_chart.h"

namespace astra {
inline bool is_unclaimed(const StarSystem& s) {
    return s.controlling_faction.empty();
}
} // namespace astra
```

- [ ] **Step 2: Build (header-only check)**

Run: `cmake --build build -j`
Expected: clean build. Nothing links against the header yet.

- [ ] **Step 3: Commit**

```bash
git add include/astra/faction_territory.h
git commit -m "$(cat <<'EOF'
feat(faction-territory): add public API header

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Implement `assign_system_factions` — capital placement

**Files:**
- Create: `src/generators/faction_territory.cpp`
- Modify: `CMakeLists.txt` (add source)

- [ ] **Step 1: Create the source with capital placement only**

Create `src/generators/faction_territory.cpp`:

```cpp
#include "astra/faction_territory.h"

#include "astra/faction.h"
#include "astra/star_chart.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace astra {

namespace {

constexpr float kCapitalMinDist   = 40.0f;  // galaxy units between capitals
constexpr float kInfluenceRadius  = 35.0f;  // owned if dist <= this
constexpr float kNoiseRate        = 0.10f;  // fraction of owned systems that enclave

struct Capital {
    size_t system_index = 0;
    FactionTerritory faction = FactionTerritory::Unclaimed;
};

// Faction capital budget. Total = 7. Sol is a hard-pinned Terran capital
// before procedural placement runs.
struct FactionBudget {
    FactionTerritory faction;
    int count;
};

constexpr FactionBudget kBudget[] = {
    {FactionTerritory::StellariConclave, 3},
    {FactionTerritory::TerranFederation, 2},
    {FactionTerritory::KrethMiningGuild, 1},
    {FactionTerritory::VeldraniAccord,   1},
};

float dist_sq(const StarSystem& a, const StarSystem& b) {
    float dx = a.gx - b.gx, dy = a.gy - b.gy;
    return dx * dx + dy * dy;
}

// Find Sol's index (system id == 1). Returns -1 if absent.
int find_sol(const NavigationData& nav) {
    for (size_t i = 0; i < nav.systems.size(); ++i) {
        if (nav.systems[i].id == 1) return static_cast<int>(i);
    }
    return -1;
}

bool far_enough_from_all(const StarSystem& candidate,
                         const std::vector<Capital>& existing,
                         const std::vector<StarSystem>& systems,
                         float min_dist) {
    float min_sq = min_dist * min_dist;
    for (const auto& cap : existing) {
        if (dist_sq(candidate, systems[cap.system_index]) < min_sq) {
            return false;
        }
    }
    return true;
}

// Place capitals. Returns the placed set.
std::vector<Capital> place_capitals(const NavigationData& nav,
                                    std::mt19937& rng) {
    std::vector<Capital> capitals;

    // Pin Sol to Terran first.
    int sol = find_sol(nav);
    int terran_remaining = 0;
    for (const auto& b : kBudget) {
        if (b.faction == FactionTerritory::TerranFederation) {
            terran_remaining = b.count - 1; // Sol counts as one
            break;
        }
    }
    if (sol >= 0) {
        capitals.push_back({static_cast<size_t>(sol),
                            FactionTerritory::TerranFederation});
    }

    // Remaining budget (Sol-adjusted).
    auto try_place = [&](FactionTerritory f, float min_dist) -> bool {
        std::uniform_int_distribution<size_t> pick(0, nav.systems.size() - 1);
        for (int attempt = 0; attempt < 500; ++attempt) {
            size_t idx = pick(rng);
            if (nav.systems[idx].id == 1) continue;  // Sol already used
            bool already = false;
            for (const auto& c : capitals) {
                if (c.system_index == idx) { already = true; break; }
            }
            if (already) continue;
            if (!far_enough_from_all(nav.systems[idx], capitals,
                                     nav.systems, min_dist)) continue;
            capitals.push_back({idx, f});
            return true;
        }
        return false;
    };

    // Place procedural capitals with up to 3 relaxation rounds.
    float min_dist = kCapitalMinDist;
    for (int relax = 0; relax < 4; ++relax) {
        bool all_placed = true;

        // Remaining Terran (after Sol)
        for (int i = 0; i < terran_remaining; ++i) {
            if (!try_place(FactionTerritory::TerranFederation, min_dist)) {
                all_placed = false;
            }
        }
        terran_remaining = 0;  // placed or abandoned

        // Other factions
        for (const auto& b : kBudget) {
            if (b.faction == FactionTerritory::TerranFederation) continue;
            for (int i = 0; i < b.count; ++i) {
                if (!try_place(b.faction, min_dist)) {
                    all_placed = false;
                }
            }
        }
        if (all_placed) break;
        min_dist *= 0.9f;  // relax
    }

    return capitals;
}

} // namespace

void assign_system_factions(NavigationData& nav, uint32_t seed) {
    // Clear previous assignment (idempotent).
    for (auto& s : nav.systems) s.controlling_faction.clear();
    nav.faction_map.cells.clear();

    if (nav.systems.empty()) return;

    std::mt19937 rng(seed ^ 0xF4C710Du);  // "FACTION"

    auto capitals = place_capitals(nav, rng);
    if (capitals.empty()) return;

    // TODO next task: territorial assignment + noise + FactionMap precompute.
}

// Stubs filled in next task.
std::string faction_at_coord(const NavigationData&, float, float) {
    return "";
}

FactionTerritory faction_enum_from_name(const std::string& faction) {
    if (faction == Faction_StellariConclave) return FactionTerritory::StellariConclave;
    if (faction == Faction_TerranFederation) return FactionTerritory::TerranFederation;
    if (faction == Faction_KrethMiningGuild) return FactionTerritory::KrethMiningGuild;
    if (faction == Faction_VeldraniAccord)   return FactionTerritory::VeldraniAccord;
    return FactionTerritory::Unclaimed;
}

const char* faction_name_from_enum(FactionTerritory t) {
    switch (t) {
        case FactionTerritory::StellariConclave: return Faction_StellariConclave;
        case FactionTerritory::TerranFederation: return Faction_TerranFederation;
        case FactionTerritory::KrethMiningGuild: return Faction_KrethMiningGuild;
        case FactionTerritory::VeldraniAccord:   return Faction_VeldraniAccord;
        case FactionTerritory::Unclaimed:        return "";
    }
    return "";
}

} // namespace astra
```

- [ ] **Step 2: Add `FactionMap faction_map` member to `NavigationData`**

In `include/astra/star_chart.h`, inside `NavigationData`:

```cpp
#include "astra/faction_territory.h"  // add near the top, below existing includes

// ... inside struct NavigationData, after uint32_t next_custom_system_id ...
    FactionMap faction_map;   // regenerated on load; not serialized
```

Wait — `faction_territory.h` already includes `star_chart.h` for the inline `is_unclaimed`. Circular. Break it by forward-declaring `FactionMap` in `star_chart.h` instead:

```cpp
// In star_chart.h near other forward decls:
struct FactionMap;  // defined in faction_territory.h
```

And change the `NavigationData` field to a unique_ptr to avoid needing the full definition:

```cpp
#include <memory>
// ...
    std::unique_ptr<FactionMap> faction_map;  // nullptr until assign_system_factions runs
```

Or simpler — put `FactionMap` in its own tiny header that doesn't include `star_chart.h`, and include that header from both. Cleanest fix:

Create `include/astra/faction_map.h`:

```cpp
#pragma once

#include <cstdint>
#include <vector>

namespace astra {

constexpr int kFactionMapWidth  = 256;
constexpr int kFactionMapHeight = 256;

enum class FactionTerritory : uint8_t {
    Unclaimed        = 0,
    StellariConclave = 1,
    TerranFederation = 2,
    KrethMiningGuild = 3,
    VeldraniAccord   = 4,
};

struct FactionMap {
    std::vector<FactionTerritory> cells;
    float gx_min = 0.0f;
    float gx_max = 0.0f;
    float gy_min = 0.0f;
    float gy_max = 0.0f;
    bool empty() const { return cells.empty(); }
};

} // namespace astra
```

Then remove the `kFactionMapWidth/Height`, `FactionTerritory`, and `FactionMap` definitions from `faction_territory.h` and replace with:

```cpp
#include "astra/faction_map.h"
```

And in `star_chart.h`:

```cpp
#include "astra/faction_map.h"
// ... inside NavigationData ...
    FactionMap faction_map;   // regenerated on load; not serialized
```

- [ ] **Step 3: Add source to CMake**

Open `CMakeLists.txt`. Find the `ASTRA_SOURCES` list. Insert `src/generators/faction_territory.cpp` in the `src/generators/` block (alphabetical ordering — after `src/generators/exterior_decorator.cpp` or similar).

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: clean build. `faction_at_coord` returns `""` for now.

- [ ] **Step 5: Commit**

```bash
git add include/astra/faction_map.h include/astra/faction_territory.h \
        src/generators/faction_territory.cpp include/astra/star_chart.h \
        CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(faction-territory): capital placement scaffolding

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Territorial assignment + noise pass

**Files:**
- Modify: `src/generators/faction_territory.cpp`

- [ ] **Step 1: Replace the TODO in `assign_system_factions` with the assignment + noise pass**

Inside `assign_system_factions` in `src/generators/faction_territory.cpp`, delete the `// TODO next task: ...` line and append:

```cpp
    // ── Assign every system to the nearest capital within influence. ──
    const float inf_sq = kInfluenceRadius * kInfluenceRadius;
    for (size_t i = 0; i < nav.systems.size(); ++i) {
        StarSystem& s = nav.systems[i];

        // If this system is itself a capital, set its faction directly.
        bool is_capital = false;
        for (const auto& cap : capitals) {
            if (cap.system_index == i) {
                s.controlling_faction = faction_name_from_enum(cap.faction);
                is_capital = true;
                break;
            }
        }
        if (is_capital) continue;

        // Nearest-capital search.
        FactionTerritory best = FactionTerritory::Unclaimed;
        float best_sq = inf_sq;
        for (const auto& cap : capitals) {
            float d2 = dist_sq(s, nav.systems[cap.system_index]);
            if (d2 <= best_sq) {
                best_sq = d2;
                best = cap.faction;
            }
        }
        s.controlling_faction = faction_name_from_enum(best);
    }

    // ── Noise pass: enclaves. ──
    // Deterministic per-system by seeding from (seed ^ system_id).
    for (auto& s : nav.systems) {
        if (s.controlling_faction.empty()) continue;  // no enclaves in wilderness

        std::mt19937 r(seed ^ (s.id * 2654435761u));
        std::uniform_real_distribution<float> roll(0.0f, 1.0f);
        if (roll(r) >= kNoiseRate) continue;

        // 80% swap to different territorial faction, 20% to Unclaimed.
        if (roll(r) < 0.8f) {
            FactionTerritory current = faction_enum_from_name(s.controlling_faction);
            std::uniform_int_distribution<int> pick(1, 4);  // 1..4 = non-Unclaimed
            FactionTerritory chosen;
            do {
                chosen = static_cast<FactionTerritory>(pick(r));
            } while (chosen == current);
            s.controlling_faction = faction_name_from_enum(chosen);
        } else {
            s.controlling_faction = "";
        }
    }

    // TODO next task: FactionMap precompute.
```

- [ ] **Step 2: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add src/generators/faction_territory.cpp
git commit -m "$(cat <<'EOF'
feat(faction-territory): territorial assignment and enclave noise

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: FactionMap precompute + `faction_at_coord`

**Files:**
- Modify: `src/generators/faction_territory.cpp`

- [ ] **Step 1: Replace the remaining TODO with the map precompute**

Replace the `// TODO next task: FactionMap precompute.` line in `assign_system_factions` with:

```cpp
    // ── Precompute FactionMap for renderer lookup. ──
    // Compute galaxy bounds from system positions.
    float gx_min = nav.systems.front().gx;
    float gx_max = gx_min;
    float gy_min = nav.systems.front().gy;
    float gy_max = gy_min;
    for (const auto& s : nav.systems) {
        gx_min = std::min(gx_min, s.gx);
        gx_max = std::max(gx_max, s.gx);
        gy_min = std::min(gy_min, s.gy);
        gy_max = std::max(gy_max, s.gy);
    }

    // Pad bounds slightly so edge systems aren't clipped.
    const float pad = kInfluenceRadius;
    gx_min -= pad; gx_max += pad;
    gy_min -= pad; gy_max += pad;

    nav.faction_map.gx_min = gx_min;
    nav.faction_map.gx_max = gx_max;
    nav.faction_map.gy_min = gy_min;
    nav.faction_map.gy_max = gy_max;
    nav.faction_map.cells.assign(
        static_cast<size_t>(kFactionMapWidth) * kFactionMapHeight,
        FactionTerritory::Unclaimed);

    const float cell_w = (gx_max - gx_min) / kFactionMapWidth;
    const float cell_h = (gy_max - gy_min) / kFactionMapHeight;
    const float inf_sq_map = kInfluenceRadius * kInfluenceRadius;

    for (int cy = 0; cy < kFactionMapHeight; ++cy) {
        const float wy = gy_min + (cy + 0.5f) * cell_h;
        for (int cx = 0; cx < kFactionMapWidth; ++cx) {
            const float wx = gx_min + (cx + 0.5f) * cell_w;

            FactionTerritory best = FactionTerritory::Unclaimed;
            float best_sq = inf_sq_map;
            for (const auto& cap : capitals) {
                const auto& cs = nav.systems[cap.system_index];
                float dx = wx - cs.gx, dy = wy - cs.gy;
                float d2 = dx * dx + dy * dy;
                if (d2 <= best_sq) {
                    best_sq = d2;
                    best = cap.faction;
                }
            }
            nav.faction_map.cells[cy * kFactionMapWidth + cx] = best;
        }
    }
```

Note: the map renders territory by capital proximity only — noise enclaves are only visible at the system glyph level, not the background tint. This is intentional: enclaves are "surprises in the territory," not holes punched in the bg color.

- [ ] **Step 2: Replace the `faction_at_coord` stub with the real implementation**

At the bottom of `src/generators/faction_territory.cpp`, replace the existing stub:

```cpp
std::string faction_at_coord(const NavigationData& nav, float gx, float gy) {
    const auto& m = nav.faction_map;
    if (m.empty()) return "";
    if (gx < m.gx_min || gx >= m.gx_max || gy < m.gy_min || gy >= m.gy_max) {
        return "";
    }
    const float cell_w = (m.gx_max - m.gx_min) / kFactionMapWidth;
    const float cell_h = (m.gy_max - m.gy_min) / kFactionMapHeight;
    int cx = static_cast<int>((gx - m.gx_min) / cell_w);
    int cy = static_cast<int>((gy - m.gy_min) / cell_h);
    cx = std::clamp(cx, 0, kFactionMapWidth - 1);
    cy = std::clamp(cy, 0, kFactionMapHeight - 1);
    return faction_name_from_enum(m.cells[cy * kFactionMapWidth + cx]);
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add src/generators/faction_territory.cpp
git commit -m "$(cat <<'EOF'
feat(faction-territory): precompute FactionMap for renderer lookup

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Call `assign_system_factions` from generation + load paths

**Files:**
- Modify: `src/game.cpp` (two call sites near `apply_lore_to_galaxy`)
- Modify: `src/save_system.cpp` (one call site near `apply_lore_to_galaxy`)

- [ ] **Step 1: Add include to `src/game.cpp`**

At the top of `src/game.cpp` among the astra includes:

```cpp
#include "astra/faction_territory.h"
```

- [ ] **Step 2: Wire the two new-game sites**

Find both `apply_lore_to_galaxy(world_.navigation(), world_.lore());` lines in `src/game.cpp` (around lines 847 and 1102 per current main). Immediately after each, add:

```cpp
    assign_system_factions(world_.navigation(), world_.seed());
```

Use the **same** seed variable used to drive galaxy generation two lines above so the territorial map is deterministic per game seed.

- [ ] **Step 3: Add include to `src/save_system.cpp`**

At the top of `src/save_system.cpp`:

```cpp
#include "astra/faction_territory.h"
```

- [ ] **Step 4: Wire the load path**

In `src/save_system.cpp` around line 217, after `apply_lore_to_galaxy(world.navigation(), world.lore());`, add:

```cpp
    assign_system_factions(world.navigation(), world.seed());
```

Match the exact argument pattern used by `apply_lore_to_galaxy` at the same site.

- [ ] **Step 5: Build + smoke test**

Run: `cmake --build build -j && ./build/astra`
Start a new game. Open the star chart (galaxy view). Nothing should look different yet — no rendering changes. Close and quit.

- [ ] **Step 6: Dev-console spot-check**

If the dev console has a print-system-info command or similar, inspect Sol's `controlling_faction`. Expected: `"Terran Federation"`. Pick two other systems far from Sol — they should be either another faction or Unclaimed. If there's no such command, skip.

- [ ] **Step 7: Commit**

```bash
git add src/game.cpp src/save_system.cpp
git commit -m "$(cat <<'EOF'
feat(faction-territory): assign factions in gen and load paths

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: Honor `CustomSystemSpec::controlling_faction` + pin beacon as Unclaimed

**Files:**
- Modify: `src/star_chart.cpp` (`add_custom_system` function)
- Modify: `src/quests/stellar_signal_beacon.cpp` (beacon creation call site)

- [ ] **Step 1: Honor the optional field in `add_custom_system`**

Open `src/star_chart.cpp`. Locate the `add_custom_system` function. Inside, after the new `StarSystem` is populated from the spec and pushed into `nav.systems`, but BEFORE returning, **leave a hook** — the caller has already done all the field copies. Find the section that copies `lore` from spec to system. Add immediately after:

```cpp
    // If caller explicitly provided a faction, record it. Otherwise the
    // value remains "" — assign_system_factions or any subsequent call
    // will populate it via the normal territorial pass.
    if (spec.controlling_faction.has_value()) {
        sys.controlling_faction = *spec.controlling_faction;
    }
```

(Replace `sys` with the actual variable name used in the existing implementation — check the function first.)

- [ ] **Step 2: Pin the beacon system to Unclaimed**

In `src/quests/stellar_signal_beacon.cpp`, find the `add_custom_system(nav, {...})` call (around `on_unlocked`, line 107 in current main). Add `.controlling_faction = std::string("")` to the spec literal:

```cpp
    uint32_t beacon_id = add_custom_system(nav, {
        .name = "Unnamed — Beacon",
        .gx = coords->first, .gy = coords->second,
        .star_class = StarClass::ClassM,
        .discovered = true,
        .controlling_faction = std::string(""),  // Unclaimed — beyond charted space
        .bodies = { make_landable_asteroid("Beacon Core") },
    });
```

Add `#include <string>` at the top if not already present.

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add src/star_chart.cpp src/quests/stellar_signal_beacon.cpp
git commit -m "$(cat <<'EOF'
feat(faction-territory): honor custom spec override; beacon is Unclaimed

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 8: Galaxy-view background tint layer + toggle + hover

**Files:**
- Modify: `src/star_chart_viewer.cpp`
- Modify: `include/astra/star_chart_viewer.h` (if toggle state lives on the viewer class)

This task has the largest UI surface. Break it into three sub-passes: toggle state, bg tint rendering, hover-popup line.

### 8a — toggle state and keybind

- [ ] **Step 1: Add state + keybind**

Open `include/astra/star_chart_viewer.h`. In the viewer class private section add:

```cpp
    bool show_faction_tint_ = true;  // session-local; default on
```

In `src/star_chart_viewer.cpp`, locate the input handler switch. Add a case for `'F'` (or `'f'`) — check existing bindings first; if `F` is taken use `V`:

```cpp
    case 'F':
    case 'f':
        show_faction_tint_ = !show_faction_tint_;
        break;
```

Place this only in the Galaxy zoom handler section so it doesn't hijack the key at other zoom levels.

- [ ] **Step 2: Build**

Run: `cmake --build build -j`
Expected: clean build. Pressing F in galaxy view toggles the (unused) flag.

- [ ] **Step 3: Commit**

```bash
git add include/astra/star_chart_viewer.h src/star_chart_viewer.cpp
git commit -m "$(cat <<'EOF'
feat(star-chart): add faction-tint toggle state and keybind

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

### 8b — background tint rendering in galaxy view

- [ ] **Step 1: Palette helper**

At the top of `src/star_chart_viewer.cpp` (in an anonymous namespace or as `static`), add:

```cpp
namespace {

astra::Color faction_tint_color(astra::FactionTerritory t) {
    // Muted 256-color ANSI indexes that sit behind stars without fighting them.
    // Tune palette if anything reads too bright.
    switch (t) {
        case astra::FactionTerritory::StellariConclave: return 53;   // dim magenta
        case astra::FactionTerritory::TerranFederation: return 17;   // dim blue
        case astra::FactionTerritory::KrethMiningGuild: return 58;   // dim olive
        case astra::FactionTerritory::VeldraniAccord:   return 22;   // dim teal
        case astra::FactionTerritory::Unclaimed:        return 0;    // default black
    }
    return 0;
}

} // namespace
```

(Replace `astra::Color` with whatever type the existing rendering uses — grep for how `star_class_color` is called; use that type.)

- [ ] **Step 2: Include faction_territory**

Near the top of `src/star_chart_viewer.cpp`:

```cpp
#include "astra/faction_territory.h"
```

- [ ] **Step 3: Paint the bg before painting stars**

Find the Galaxy-view rendering pass — the block where it iterates screen cells and draws stars. BEFORE that loop, add a new loop that fills each screen cell's background with the faction tint at that galaxy coordinate. Pseudocode pattern (exact field names depend on the existing renderer):

```cpp
    if (show_faction_tint_) {
        for (int sy = 0; sy < view_h; ++sy) {
            for (int sx = 0; sx < view_w; ++sx) {
                // Convert screen cell to galaxy coord — use the same
                // transform the existing star-draw loop uses.
                float gx = screen_to_galaxy_x(sx);
                float gy = screen_to_galaxy_y(sy);
                auto f = faction_enum_from_name(
                    faction_at_coord(nav, gx, gy));
                if (f == FactionTerritory::Unclaimed) continue;
                auto bg = faction_tint_color(f);
                // Set background color on this cell without overwriting
                // an existing star glyph. If the cell is empty, paint a
                // space with the bg color.
                renderer.set_bg(sx, sy, bg);  // exact API per existing code
            }
        }
    }
```

**Critical:** do not clobber star foreground colors or glyphs. The star loop runs after this and paints fg on top.

If the existing renderer doesn't have a "set only bg" primitive, paint a space character with fg=default, bg=tint:

```cpp
                renderer.draw_cell(sx, sy, ' ', 0 /* default fg */, bg);
```

— and ensure the subsequent star loop uses a draw that writes fg+bg (bg should be preserved or re-applied with the tint).

- [ ] **Step 4: Build + visual smoke test**

Run: `cmake --build build -j && ./build/astra`
Open the galaxy chart. Expected:
  - Territorial bands visible as muted colored backgrounds
  - Stars render on top with their normal stellar-class colors
  - Unclaimed space stays black
  - Pressing `F` toggles the bands on/off
  - Nothing flickers or tears

If palette reads too bright, adjust the indexes in `faction_tint_color`.

- [ ] **Step 5: Commit**

```bash
git add src/star_chart_viewer.cpp
git commit -m "$(cat <<'EOF'
feat(star-chart): render territorial bands in galaxy view

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

### 8c — hover popup faction line

- [ ] **Step 1: Extend the galaxy-hover popup**

Find the block in `src/star_chart_viewer.cpp` that builds the hover popup content when the cursor is over a known system. It currently emits lines like name + star class + lore tier. Append one line:

```cpp
    if (!system.controlling_faction.empty()) {
        popup_lines.push_back("Faction: " + system.controlling_faction);
    } else {
        popup_lines.push_back("Unclaimed space");
    }
```

Use whatever string vector / UI-line builder the existing popup code uses.

- [ ] **Step 2: Build + smoke test**

Run: `cmake --build build -j && ./build/astra`
Open galaxy chart, hover over Sol — expect "Faction: Terran Federation". Hover over the Stage 3 beacon system (if accessible from dev) — expect "Unclaimed space".

- [ ] **Step 3: Commit**

```bash
git add src/star_chart_viewer.cpp
git commit -m "$(cat <<'EOF'
feat(star-chart): show faction in galaxy hover popup

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 9: Gate Stage 4 ambushes to Conclave space

**Files:**
- Modify: `src/scenarios/stage4_hostility.cpp`

- [ ] **Step 1: Add faction check**

Open `src/scenarios/stage4_hostility.cpp`. In the `SystemEntered` handler, locate the block between the transmission logic and the `inject_location_encounter` call. Look up the system by id and early-return if it isn't Conclave space.

Current handler body ends roughly like:
```cpp
    // transmission logic
    ...
    std::vector<std::string> roles(count, "Conclave Sentry");
    inject_location_encounter(g, payload.system_id, 0, false, roles, "stage4_ambush");
    world.ambushed_systems().insert(payload.system_id);
```

Change to:
```cpp
    // transmission logic (unchanged — fires on first post-Stage-3 warp anywhere)
    ...

    // Ambushes only in Conclave-controlled space.
    const auto& nav = world.navigation();
    const StarSystem* sys = nullptr;
    for (const auto& s : nav.systems) {
        if (s.id == payload.system_id) { sys = &s; break; }
    }
    if (!sys) return;
    if (sys->controlling_faction != Faction_StellariConclave) return;

    std::vector<std::string> roles(count, "Conclave Sentry");
    inject_location_encounter(g, payload.system_id, 0, false, roles, "stage4_ambush");
    world.ambushed_systems().insert(payload.system_id);
```

Add `#include "astra/star_chart.h"` if not already included.

- [ ] **Step 2: Build + smoke test**

Run: `cmake --build build -j && ./build/astra`
Complete stages 1→3 (or use dev console to fast-forward the flags). Warp to a known Terran system (should be near Sol) — expect transmission (if first post-stage-3 warp) but NO ambush. Warp to a known Conclave system — expect ambush.

- [ ] **Step 3: Commit**

```bash
git add src/scenarios/stage4_hostility.cpp
git commit -m "$(cat <<'EOF'
feat(stellar-signal): gate Stage 4 ambushes to Conclave space

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 10: Docs

**Files:**
- Modify: `docs/roadmap.md`
- Modify: `docs/plans/stellar_signal_phase4_5_gaps.md`

- [ ] **Step 1: Add roadmap entry**

Open `docs/roadmap.md`. In the "The Stellar Signal" section (added during the EventBus slice), add immediately after the `Stage 4 — Conclave hostility & ambushes` line:

```markdown
- [x] **System faction ownership** — controlling_faction per system, deterministic clustered generation, galaxy-view band rendering with `F` toggle, Stage 4 ambushes gated to Conclave space (2026-04-20)
```

- [ ] **Step 2: Update gap analysis**

Open `docs/plans/stellar_signal_phase4_5_gaps.md`. Append an entry under Stage 4 marked Done:

```markdown
### **Feature: Faction-space gating for Stage 4 ambushes**

**Stage:** 4
**Status:** ✅ Done (2026-04-20) — `controlling_faction` field on `StarSystem`, deterministic clustered assignment from galaxy seed around procedurally-placed capitals (Sol pinned to Terran), galaxy chart renders territorial bands, Stage 4 scenario filters ambushes to Conclave-owned systems.
**Touch points shipped:** `include/astra/faction_map.h`, `include/astra/faction_territory.h`, `src/generators/faction_territory.cpp`, `include/astra/star_chart.h`, `src/star_chart_viewer.cpp`, `src/scenarios/stage4_hostility.cpp`, `src/quests/stellar_signal_beacon.cpp`.
```

- [ ] **Step 3: Commit**

```bash
git add docs/roadmap.md docs/plans/stellar_signal_phase4_5_gaps.md
git commit -m "$(cat <<'EOF'
docs: mark system faction ownership slice complete

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Final Verification Checklist

Before declaring the slice done, confirm:

- [ ] Clean build on default target
- [ ] Sol is Terran Federation on every new game regardless of seed
- [ ] Determinism: same seed on two separate runs → identical `controlling_faction` across systems
- [ ] Save → reload → faction values and map identical (deterministic regen)
- [ ] Galaxy view shows muted faction tint bands; stars render on top cleanly
- [ ] `F` toggle flips bands on/off in galaxy view (no effect on other zoom levels)
- [ ] Hover popup on any system names a faction or says "Unclaimed space"
- [ ] Stage 3 beacon system reads as Unclaimed
- [ ] Completing Stage 3 + warping to a Terran system: transmission fires (once), NO ambush
- [ ] Completing Stage 3 + warping to a Conclave system: ambush fires
- [ ] Re-entering an ambushed system: no respawn (existing dedup)
- [ ] No regressions in the existing star chart, ship, or dialog flows

---

## Open questions / deferred

These are deferred on purpose — don't do them in this slice:

1. **Palette fine-tuning** — if the tint colors read wrong in practice, adjust after seeing them on the user's actual terminal.
2. **Region / local-view tinting** — only galaxy view is in scope.
3. **Capital lore writeups** — no narrative content is generated for capital systems.
4. **Faction distribution over time** — immutable for this slice.
5. **Dev-console faction commands** — if useful for Stage 4 testing, add as a trivial follow-up (`faction <system_id> <name>` to override).
6. **Station-level faction ownership** — explicitly out of scope.
