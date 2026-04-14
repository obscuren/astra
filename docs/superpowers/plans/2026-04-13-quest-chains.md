# Quest Chains Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add DAG-based story quest chains with per-quest prerequisites, reveal policies, and NPC-offer vs auto-accept modes to the existing quest system.

**Architecture:** Extend `Quest`, `StoryQuest`, and `QuestManager` in place — no new major subsystems. Introduce `QuestGraph` (runtime lookup helper) and two new status pools (`locked_`, `available_`) in the manager. Unlocking cascades on `complete_quest`; failure cascades on `fail_quest`. Save format bumps to v27.

**Tech Stack:** C++20, existing project conventions (snake_case members, PascalCase types, `#pragma once`, `astra::` namespace). No external libs. Dev-mode verification via `-DDEV=ON` builds and the dev console — this project has no unit test suite, so TDD-style steps are replaced with **build + dev-console verification** steps per CLAUDE.md.

**Spec:** `docs/superpowers/specs/2026-04-13-quest-chains-design.md`

**Working branch:** `feat/quest-chains` (worktree at `.claude/worktrees/quest-chains`)

---

## File Structure

**Created:**
- `include/astra/quest_graph.h` — `QuestGraph` class (catalog-driven lookup).
- `src/quest_graph.cpp` — graph building, dependents/prerequisites lookup, cycle detection.
- `src/quest_validator.cpp` — startup validator (aborts in dev, warns in release).
- `src/quests/hauler_b.cpp` — demo arc node B.
- `src/quests/hauler_c.cpp` — demo arc node C.
- `src/quests/hauler_d.cpp` — demo arc node D.

**Modified:**
- `include/astra/quest.h` — add `Locked`/`Available` to `QuestStatus`; add `RevealPolicy`, `OfferMode`; add chain fields to `Quest`; extend `StoryQuest`; extend `QuestManager` API.
- `src/quest.cpp` — extend `QuestManager` (pools, unlock cascade, failure cascade, catalog init).
- `src/quests/missing_hauler.cpp` — set `arc_id`, `offer_giver_role` on existing quest; register demo arc members in catalog builder.
- `src/dialog_manager.cpp` — offer `Available` quests for matching NPC role; accept flow.
- `include/astra/save_file.h` — bump default `version` to 27; add `locked_quests` and `available_quests` to `SaveData`.
- `src/save_file.cpp` — serialize/deserialize new pools + new `Quest` fields; version gate.
- `src/save_system.cpp` — wire new pools between `SaveData` and `QuestManager`.
- `src/journal.cpp` — arc grouping + reveal policy rendering (`include/astra/journal.h` if API changes).
- `CMakeLists.txt` — add new sources (`quest_graph.cpp`, `quest_validator.cpp`, three demo quest files).

---

## Task 1: Extend core quest enums and data structures

**Files:**
- Modify: `include/astra/quest.h`

- [ ] **Step 1: Extend `QuestStatus`, add `RevealPolicy` and `OfferMode`**

In `include/astra/quest.h`, replace the existing `QuestStatus` enum and add the two new enums immediately after it:

```cpp
enum class QuestStatus : uint8_t {
    Locked,     // Prerequisites not yet satisfied (story quests only)
    Available,  // Unlocked, awaiting NPC acceptance (or about to auto-accept)
    Active,
    Completed,
    Failed,
};

enum class RevealPolicy : uint8_t {
    Hidden,     // Show "??? — ???" with "Locked" hint only
    TitleOnly,  // Show title, hide description
    Full,       // Show title + description
};

enum class OfferMode : uint8_t {
    Auto,       // Becomes Active immediately on unlock
    NpcOffer,   // Enters Available; offered by a named NPC role via dialog
};
```

> **Ordering note:** `Locked` must be first (value 0) so default-constructed quests without prerequisites don't accidentally inherit `Locked` — the `QuestManager` always assigns status explicitly when moving a quest into a pool. We keep `Locked` at 0 anyway because it's the most conservative default for unknown/future saved values. Save loads explicitly convert; do **not** rely on a previous numeric order.

- [ ] **Step 2: Add chain fields to `Quest` struct**

In the same header, extend `struct Quest` after the existing `target_body_index` field:

```cpp
struct Quest {
    std::string id;
    std::string title;
    std::string description;
    std::string giver_npc;
    QuestStatus status = QuestStatus::Available;
    std::vector<QuestObjective> objectives;
    QuestReward reward;
    bool is_story = false;
    int accepted_tick = 0;

    uint32_t target_system_id = 0;
    int target_body_index = -1;

    // Chain / DAG (story quests only; standalone quests leave these defaults)
    std::string arc_id;                          // groups quests under a shared arc
    std::vector<std::string> prerequisite_ids;   // all must be Completed to unlock
    RevealPolicy reveal = RevealPolicy::Full;

    bool all_objectives_complete() const;
    bool ready_for_turnin() const;
};
```

- [ ] **Step 3: Build to confirm no breakage**

Run:
```bash
cd /Users/jeffrey/dev/crawler/.claude/worktrees/quest-chains
cmake -B build -DDEV=ON && cmake --build build -j
```
Expected: Compiles clean. Because `QuestStatus` values are persisted (save_file.cpp writes `static_cast<uint8_t>(q.status)`), the compiler won't catch the re-numbering — Task 9 handles save compat.

- [ ] **Step 4: Commit**

```bash
git add include/astra/quest.h
git commit -m "feat(quest): add chain fields and reveal/offer enums to Quest struct"
```

---

## Task 2: Extend `StoryQuest` base class

**Files:**
- Modify: `include/astra/quest.h`

- [ ] **Step 1: Extend virtual interface**

Replace the existing `StoryQuest` class in `include/astra/quest.h` with:

```cpp
class StoryQuest {
public:
    virtual ~StoryQuest() = default;

    virtual Quest create_quest() = 0;

    // Arc / DAG declarations (defaults = standalone, no arc)
    virtual std::string arc_id() const { return ""; }
    virtual std::string arc_title() const { return ""; }
    virtual std::vector<std::string> prerequisite_ids() const { return {}; }
    virtual RevealPolicy reveal_policy() const { return RevealPolicy::Full; }

    // Offer semantics
    virtual OfferMode offer_mode() const { return OfferMode::NpcOffer; }
    virtual std::string offer_giver_role() const { return ""; }

    // Lifecycle hooks
    virtual void on_unlocked(Game& game) {}
    virtual void on_accepted(Game& game) {}
    virtual void on_completed(Game& game) {}
    virtual void on_failed(Game& game) {}
};
```

- [ ] **Step 2: Build**

```bash
cmake --build build -j
```
Expected: Compiles (existing `MissingHaulerQuest` and `GettingAirborneQuest` override only the existing methods, all new methods have defaults).

- [ ] **Step 3: Commit**

```bash
git add include/astra/quest.h
git commit -m "feat(quest): extend StoryQuest base with arc/DAG virtual hooks"
```

---

## Task 3: Create `QuestGraph` helper

**Files:**
- Create: `include/astra/quest_graph.h`
- Create: `src/quest_graph.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write header**

Create `include/astra/quest_graph.h`:

```cpp
#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace astra {

class StoryQuest;

class QuestGraph {
public:
    // Build from the live story_quest_catalog().
    void build();

    const std::vector<std::string>& dependents_of(const std::string& id) const;
    const std::vector<std::string>& prerequisites_of(const std::string& id) const;

    // Returns "" if unknown.
    std::string arc_of(const std::string& id) const;

    // Quests in an arc, topologically sorted (roots first). Empty if arc unknown.
    const std::vector<std::string>& arc_members(const std::string& arc_id) const;

    // True if the id is registered (catalog membership).
    bool contains(const std::string& id) const;

    // Transitive forward set (all dependents, dependents-of-dependents, ...).
    std::unordered_set<std::string> descendants_of(const std::string& id) const;

private:
    std::unordered_map<std::string, std::vector<std::string>> dependents_;
    std::unordered_map<std::string, std::vector<std::string>> prerequisites_;
    std::unordered_map<std::string, std::string> arc_by_id_;
    std::unordered_map<std::string, std::vector<std::string>> arc_members_;
    std::unordered_set<std::string> ids_;
    std::vector<std::string> empty_;
};

// Global accessor (built lazily on first call).
const QuestGraph& quest_graph();
void rebuild_quest_graph();  // for dev console; normally not called manually

} // namespace astra
```

- [ ] **Step 2: Write implementation**

Create `src/quest_graph.cpp`:

```cpp
#include "astra/quest_graph.h"
#include "astra/quest.h"

#include <algorithm>
#include <memory>

namespace astra {

void QuestGraph::build() {
    dependents_.clear();
    prerequisites_.clear();
    arc_by_id_.clear();
    arc_members_.clear();
    ids_.clear();

    const auto& catalog = story_quest_catalog();

    // First pass: ids, arcs, prerequisite edges
    for (const auto& sq : catalog) {
        Quest q = sq->create_quest();
        const std::string& id = q.id;
        ids_.insert(id);
        std::string arc = sq->arc_id();
        if (!arc.empty()) {
            arc_by_id_[id] = arc;
            arc_members_[arc].push_back(id);
        }
        auto prereqs = sq->prerequisite_ids();
        prerequisites_[id] = prereqs;
        for (const auto& p : prereqs) {
            dependents_[p].push_back(id);
        }
    }

    // Topological sort within each arc (Kahn's)
    for (auto& [arc, members] : arc_members_) {
        std::unordered_map<std::string, int> indeg;
        for (const auto& m : members) {
            indeg[m] = 0;
        }
        for (const auto& m : members) {
            for (const auto& p : prerequisites_[m]) {
                if (indeg.count(p)) indeg[m]++;
            }
        }
        std::vector<std::string> sorted;
        std::vector<std::string> ready;
        for (auto& [id, d] : indeg) {
            if (d == 0) ready.push_back(id);
        }
        std::sort(ready.begin(), ready.end());  // stable by id
        while (!ready.empty()) {
            auto cur = ready.front();
            ready.erase(ready.begin());
            sorted.push_back(cur);
            for (const auto& dep : dependents_[cur]) {
                if (!indeg.count(dep)) continue;
                if (--indeg[dep] == 0) ready.push_back(dep);
            }
            std::sort(ready.begin(), ready.end());
        }
        if (sorted.size() == members.size()) {
            members = std::move(sorted);
        }
        // If cycle: leave members unsorted; validator will report.
    }
}

const std::vector<std::string>& QuestGraph::dependents_of(const std::string& id) const {
    auto it = dependents_.find(id);
    return it == dependents_.end() ? empty_ : it->second;
}

const std::vector<std::string>& QuestGraph::prerequisites_of(const std::string& id) const {
    auto it = prerequisites_.find(id);
    return it == prerequisites_.end() ? empty_ : it->second;
}

std::string QuestGraph::arc_of(const std::string& id) const {
    auto it = arc_by_id_.find(id);
    return it == arc_by_id_.end() ? std::string() : it->second;
}

const std::vector<std::string>& QuestGraph::arc_members(const std::string& arc_id) const {
    auto it = arc_members_.find(arc_id);
    return it == arc_members_.end() ? empty_ : it->second;
}

bool QuestGraph::contains(const std::string& id) const {
    return ids_.count(id) > 0;
}

std::unordered_set<std::string> QuestGraph::descendants_of(const std::string& id) const {
    std::unordered_set<std::string> out;
    std::vector<std::string> stack{id};
    while (!stack.empty()) {
        auto cur = stack.back();
        stack.pop_back();
        for (const auto& dep : dependents_of(cur)) {
            if (out.insert(dep).second) stack.push_back(dep);
        }
    }
    return out;
}

static std::unique_ptr<QuestGraph> g_graph;

const QuestGraph& quest_graph() {
    if (!g_graph) {
        g_graph = std::make_unique<QuestGraph>();
        g_graph->build();
    }
    return *g_graph;
}

void rebuild_quest_graph() {
    if (!g_graph) g_graph = std::make_unique<QuestGraph>();
    g_graph->build();
}

} // namespace astra
```

- [ ] **Step 3: Register in CMake**

In `CMakeLists.txt`, find the section that lists `src/quest.cpp` (around line 106). Add `src/quest_graph.cpp` immediately after it.

Before:
```
    src/quest.cpp
    src/quests/missing_hauler.cpp
```
After:
```
    src/quest.cpp
    src/quest_graph.cpp
    src/quests/missing_hauler.cpp
```

- [ ] **Step 4: Build**

```bash
cmake -B build -DDEV=ON && cmake --build build -j
```
Expected: Clean compile.

- [ ] **Step 5: Commit**

```bash
git add include/astra/quest_graph.h src/quest_graph.cpp CMakeLists.txt
git commit -m "feat(quest): add QuestGraph helper for catalog-driven DAG lookup"
```

---

## Task 4: Startup validator

**Files:**
- Create: `src/quest_validator.cpp`
- Modify: `include/astra/quest.h` (forward declare `validate_quest_catalog`)
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add declaration**

In `include/astra/quest.h`, at the end of the `astra` namespace (before the closing brace), add:

```cpp
// Returns a list of validation errors. Empty vector = catalog is valid.
// Checks: missing prerequisite IDs, cycles, NpcOffer without offer_giver_role.
std::vector<std::string> validate_quest_catalog();
```

- [ ] **Step 2: Write implementation**

Create `src/quest_validator.cpp`:

```cpp
#include "astra/quest.h"
#include "astra/quest_graph.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace astra {

namespace {
enum class Color : uint8_t { White, Gray, Black };

bool has_cycle_dfs(const std::string& id,
                   std::unordered_map<std::string, Color>& color,
                   const QuestGraph& g) {
    color[id] = Color::Gray;
    for (const auto& dep : g.dependents_of(id)) {
        auto c = color[dep];
        if (c == Color::Gray) return true;
        if (c == Color::White && has_cycle_dfs(dep, color, g)) return true;
    }
    color[id] = Color::Black;
    return false;
}
}

std::vector<std::string> validate_quest_catalog() {
    std::vector<std::string> errors;
    const auto& catalog = story_quest_catalog();
    rebuild_quest_graph();
    const auto& g = quest_graph();

    // Collect all ids
    std::unordered_set<std::string> ids;
    for (const auto& sq : catalog) {
        Quest q = sq->create_quest();
        if (ids.count(q.id)) {
            errors.push_back("Duplicate quest id: " + q.id);
        }
        ids.insert(q.id);
    }

    // Check prerequisites and offer_giver_role
    for (const auto& sq : catalog) {
        Quest q = sq->create_quest();
        for (const auto& p : sq->prerequisite_ids()) {
            if (!ids.count(p)) {
                errors.push_back("Quest '" + q.id + "' has unknown prerequisite '" + p + "'");
            }
        }
        if (sq->offer_mode() == OfferMode::NpcOffer
            && sq->offer_giver_role().empty()
            && sq->prerequisite_ids().empty() == false) {
            // Only required for quests that actually need offering; root Auto quests fine.
            errors.push_back("Quest '" + q.id + "' uses NpcOffer but declares no offer_giver_role");
        }
    }

    // Cycle detection via forward edges
    std::unordered_map<std::string, Color> color;
    for (const auto& id : ids) color[id] = Color::White;
    for (const auto& id : ids) {
        if (color[id] == Color::White) {
            if (has_cycle_dfs(id, color, g)) {
                errors.push_back("Cycle detected involving quest '" + id + "'");
                break;
            }
        }
    }

    return errors;
}

} // namespace astra
```

- [ ] **Step 3: Call from catalog construction (dev-mode abort)**

In `src/quest.cpp` (existing file, not the quest_validator one), modify `build_catalog` in `src/quests/missing_hauler.cpp` — **wait, the catalog builder lives in `missing_hauler.cpp`, which is a legacy location.** Leave the builder where it is but add a validation call at first access.

Edit `src/quests/missing_hauler.cpp`, modify the `story_quest_catalog()` function:

Before:
```cpp
const std::vector<std::unique_ptr<StoryQuest>>& story_quest_catalog() {
    static auto catalog = build_catalog();
    return catalog;
}
```

After:
```cpp
const std::vector<std::unique_ptr<StoryQuest>>& story_quest_catalog() {
    static auto catalog = build_catalog();
    static bool validated = false;
    if (!validated) {
        validated = true;
        auto errors = validate_quest_catalog();
        if (!errors.empty()) {
#ifdef ASTRA_DEV
            for (const auto& e : errors) std::fprintf(stderr, "[quest-validator] %s\n", e.c_str());
            std::abort();
#else
            for (const auto& e : errors) std::fprintf(stderr, "[quest-validator] %s\n", e.c_str());
#endif
        }
    }
    return catalog;
}
```

Add `#include <cstdio>` and `#include <cstdlib>` near the top of `missing_hauler.cpp` if not already present.

> **Recursion risk:** `validate_quest_catalog()` calls `story_quest_catalog()` internally. Because `validated` is only set **after** we've returned `catalog` once (the static is constructed first, then `validated` is checked on next call), we must make sure validation happens on **second** call onward — which is too late. Fix by moving the validation into a helper `init_catalog()` that returns the vector:

Replace the above with:

```cpp
static std::vector<std::unique_ptr<StoryQuest>> init_catalog() {
    auto cat = build_catalog();
    // Validator uses story_quest_catalog() — so we inline the checks here using `cat`.
    // (See Task 4 Step 4 alternative below if you prefer a separate pure-data validator.)
    return cat;
}

const std::vector<std::unique_ptr<StoryQuest>>& story_quest_catalog() {
    static auto catalog = init_catalog();
    return catalog;
}
```

- [ ] **Step 4: Rework validator to accept a catalog reference**

Change `validate_quest_catalog` signature to take the catalog explicitly so it doesn't recurse through the global:

In `include/astra/quest.h`:
```cpp
std::vector<std::string> validate_quest_catalog(
    const std::vector<std::unique_ptr<StoryQuest>>& catalog);
```

In `src/quest_validator.cpp`: change the function to take the catalog parameter, and build a *local* `QuestGraph` inside the validator (don't call `quest_graph()`):

```cpp
std::vector<std::string> validate_quest_catalog(
    const std::vector<std::unique_ptr<StoryQuest>>& catalog) {
    std::vector<std::string> errors;

    // Build local graph from this catalog
    QuestGraph g;
    // QuestGraph::build() uses story_quest_catalog() — so we need a build_from(catalog) variant.
    // See Task 4 Step 5.
    // ... (rest of checks use `g`)
}
```

- [ ] **Step 5: Add `QuestGraph::build_from(catalog)` overload**

In `include/astra/quest_graph.h`, add public method:
```cpp
void build_from(const std::vector<std::unique_ptr<StoryQuest>>& catalog);
```

In `src/quest_graph.cpp`, extract the existing `build()` body into `build_from(catalog)` taking a parameter, and have `build()` call it with `story_quest_catalog()`:

```cpp
void QuestGraph::build() {
    build_from(story_quest_catalog());
}

void QuestGraph::build_from(const std::vector<std::unique_ptr<StoryQuest>>& catalog) {
    // ... existing body, using `catalog` parameter instead of the global call ...
}
```

Then the validator calls `g.build_from(catalog);` to stay recursion-free.

- [ ] **Step 6: Wire validator into `init_catalog`**

Update `init_catalog` in `src/quests/missing_hauler.cpp`:

```cpp
static std::vector<std::unique_ptr<StoryQuest>> init_catalog() {
    auto cat = build_catalog();
    auto errors = validate_quest_catalog(cat);
    if (!errors.empty()) {
        for (const auto& e : errors) std::fprintf(stderr, "[quest-validator] %s\n", e.c_str());
#ifdef ASTRA_DEV
        std::abort();
#endif
    }
    return cat;
}
```

- [ ] **Step 7: Register in CMake**

In `CMakeLists.txt`, add `src/quest_validator.cpp` to the source list near `src/quest.cpp`.

- [ ] **Step 8: Build**

```bash
cmake -B build -DDEV=ON && cmake --build build -j
```
Expected: Clean compile. Catalog has no prereq edges yet, so validator passes.

- [ ] **Step 9: Commit**

```bash
git add include/astra/quest.h include/astra/quest_graph.h src/quest_graph.cpp src/quest_validator.cpp src/quests/missing_hauler.cpp CMakeLists.txt
git commit -m "feat(quest): add startup validator for quest catalog (cycles, prereqs, offers)"
```

---

## Task 5: Extend `QuestManager` with locked/available pools

**Files:**
- Modify: `include/astra/quest.h`
- Modify: `src/quest.cpp`

- [ ] **Step 1: Add public API + private state**

In `include/astra/quest.h`, extend `class QuestManager`:

```cpp
class QuestManager {
public:
    QuestManager() = default;

    // Initialization from story catalog — call once on new game.
    void init_from_catalog(Game& game);

    // ... existing methods ...

    // New queries
    const std::vector<Quest>& locked_quests() const { return locked_; }
    const std::vector<Quest>& available_quests() const { return available_; }
    std::vector<const Quest*> available_for_role(const std::string& role) const;

    // Accept from available pool (returns false if not found)
    bool accept_available(const std::string& quest_id, Game& game, int world_tick);

    // Restore-from-save variant accepting all four pools
    void restore(std::vector<Quest> locked,
                 std::vector<Quest> available,
                 std::vector<Quest> active,
                 std::vector<Quest> completed);

    // Reconcile pools with current catalog (called after restore and init)
    void reconcile_with_catalog(Game& game);

private:
    std::vector<Quest> locked_;
    std::vector<Quest> available_;
    std::vector<Quest> active_;
    std::vector<Quest> completed_;
};
```

> **Existing `restore(active, completed)` signature:** keep it as an overload for save-version compatibility — older saves use it. New saves use the 4-arg variant.

- [ ] **Step 2: Implement `init_from_catalog`**

In `src/quest.cpp`, add a new method implementation:

```cpp
#include "astra/quest_graph.h"
#include "astra/game.h"

void QuestManager::init_from_catalog(Game& game) {
    locked_.clear();
    available_.clear();
    // active_/completed_ preserved — caller responsible.

    const auto& catalog = story_quest_catalog();
    for (const auto& sq : catalog) {
        Quest q = sq->create_quest();
        // Skip if already in active/completed
        bool already = false;
        for (const auto& a : active_)    if (a.id == q.id) { already = true; break; }
        for (const auto& c : completed_) if (c.id == q.id) { already = true; break; }
        if (already) continue;

        if (!sq->prerequisite_ids().empty()) {
            q.status = QuestStatus::Locked;
            locked_.push_back(std::move(q));
        } else if (sq->offer_mode() == OfferMode::Auto) {
            sq->on_unlocked(game);
            q.status = QuestStatus::Active;
            q.accepted_tick = 0;
            active_.push_back(std::move(q));
            sq->on_accepted(game);
        } else {
            q.status = QuestStatus::Available;
            available_.push_back(std::move(q));
        }
    }
}
```

- [ ] **Step 3: Implement `available_for_role` and `accept_available`**

Also in `src/quest.cpp`:

```cpp
std::vector<const Quest*> QuestManager::available_for_role(const std::string& role) const {
    std::vector<const Quest*> out;
    for (const auto& q : available_) {
        StoryQuest* sq = find_story_quest(q.id);
        if (sq && sq->offer_giver_role() == role) {
            out.push_back(&q);
        }
    }
    return out;
}

bool QuestManager::accept_available(const std::string& quest_id, Game& game, int world_tick) {
    for (auto it = available_.begin(); it != available_.end(); ++it) {
        if (it->id != quest_id) continue;
        Quest q = std::move(*it);
        available_.erase(it);
        StoryQuest* sq = find_story_quest(quest_id);
        accept_quest(std::move(q), world_tick, game.player());
        if (sq) sq->on_accepted(game);
        return true;
    }
    return false;
}
```

- [ ] **Step 4: Implement the 4-arg `restore` and `reconcile_with_catalog`**

```cpp
void QuestManager::restore(std::vector<Quest> locked,
                           std::vector<Quest> available,
                           std::vector<Quest> active,
                           std::vector<Quest> completed) {
    locked_ = std::move(locked);
    available_ = std::move(available);
    active_ = std::move(active);
    completed_ = std::move(completed);
}

void QuestManager::reconcile_with_catalog(Game& game) {
    const auto& catalog = story_quest_catalog();
    std::unordered_set<std::string> seen;
    for (const auto& q : locked_)    seen.insert(q.id);
    for (const auto& q : available_) seen.insert(q.id);
    for (const auto& q : active_)    seen.insert(q.id);
    for (const auto& q : completed_) seen.insert(q.id);

    for (const auto& sq : catalog) {
        Quest q = sq->create_quest();
        if (seen.count(q.id)) continue;

        // Quest is new since save. Classify.
        bool prereqs_ok = true;
        for (const auto& p : sq->prerequisite_ids()) {
            bool completed = false;
            for (const auto& c : completed_) {
                if (c.id == p && c.status == QuestStatus::Completed) { completed = true; break; }
            }
            if (!completed) { prereqs_ok = false; break; }
        }

        if (!prereqs_ok) {
            q.status = QuestStatus::Locked;
            locked_.push_back(std::move(q));
        } else if (sq->offer_mode() == OfferMode::Auto) {
            sq->on_unlocked(game);
            q.status = QuestStatus::Active;
            active_.push_back(std::move(q));
            sq->on_accepted(game);
        } else {
            q.status = QuestStatus::Available;
            available_.push_back(std::move(q));
        }
    }
}
```

- [ ] **Step 5: Call `init_from_catalog` on new game**

Find where `QuestManager` is created / new game starts. Grep:
```bash
grep -rn "QuestManager\|quest_manager_\|quests_" include/astra/ src/
```
In `src/game.cpp` (or wherever new-game initialization happens), after the `QuestManager` is constructed and player is ready, add:
```cpp
quests_.init_from_catalog(*this);
```
(Exact placement depends on the existing new-game flow; locate the call to existing story-quest setup — `MissingHaulerQuest` is likely currently auto-registered somewhere. Replace that bespoke setup with the catalog-driven init.)

> **Migration note:** If `MissingHauler` and `GettingAirborne` are currently offered/accepted by bespoke code (e.g., direct `accept_quest` calls), convert them to catalog-driven by giving them proper `offer_mode`/`offer_giver_role` overrides. See Task 11.

- [ ] **Step 6: Build**

```bash
cmake --build build -j
```
Expected: Clean compile. Story quests still work the same because their prereqs are empty (no fan-out yet).

- [ ] **Step 7: Commit**

```bash
git add include/astra/quest.h src/quest.cpp src/game.cpp
git commit -m "feat(quest): add locked/available pools and catalog-driven init"
```

---

## Task 6: Unlock cascade on `complete_quest`

**Files:**
- Modify: `src/quest.cpp`

- [ ] **Step 1: Extract cascade helper**

At the top of `src/quest.cpp`, after includes, add a static helper:

```cpp
static void cascade_unlock(QuestManager& qm,
                           std::vector<Quest>& locked,
                           std::vector<Quest>& available,
                           std::vector<Quest>& active,
                           std::vector<Quest>& completed,
                           const std::string& just_completed_id,
                           Game& game,
                           int world_tick);
```

- [ ] **Step 2: Modify `complete_quest` to invoke cascade**

Find the existing `complete_quest` in `src/quest.cpp` (starts around line 65). After `completed_.push_back(std::move(*it));` and `active_.erase(it);` but **before** `return;`, add:

```cpp
            // Cascade unlock
            const std::string completed_id = quest_id;
            const auto& graph = quest_graph();
            for (const auto& dep_id : graph.dependents_of(completed_id)) {
                // Is dep currently locked?
                auto dep_it = std::find_if(locked_.begin(), locked_.end(),
                    [&](const Quest& q){ return q.id == dep_id; });
                if (dep_it == locked_.end()) continue;

                // Are all its prereqs now completed?
                bool ready = true;
                for (const auto& p : graph.prerequisites_of(dep_id)) {
                    bool p_done = false;
                    for (const auto& c : completed_) {
                        if (c.id == p && c.status == QuestStatus::Completed) {
                            p_done = true; break;
                        }
                    }
                    if (!p_done) { ready = false; break; }
                }
                if (!ready) continue;

                // Unlock
                Quest unlocked = std::move(*dep_it);
                locked_.erase(dep_it);
                StoryQuest* sq = find_story_quest(dep_id);
                if (sq) sq->on_unlocked(game_ref);  // see Step 3 re: game_ref

                if (sq && sq->offer_mode() == OfferMode::Auto) {
                    accept_quest(std::move(unlocked), world_tick, game_ref.player());
                    if (sq) sq->on_accepted(game_ref);
                } else {
                    unlocked.status = QuestStatus::Available;
                    available_.push_back(std::move(unlocked));
                }
            }
```

- [ ] **Step 3: Thread `Game&` through `complete_quest`**

The existing signature is `complete_quest(const std::string& quest_id, Player& player)`. Change to:

```cpp
void complete_quest(const std::string& quest_id, Game& game, int world_tick);
```

Update the declaration in `include/astra/quest.h`. In the body, use `game.player()` instead of the `player` parameter, and pass `game` into `on_completed`/`on_unlocked`/`on_accepted`.

- [ ] **Step 4: Update every caller**

Find callers:
```bash
grep -rn "complete_quest" include/astra/ src/
```
Each call site (likely `src/dialog_manager.cpp` around the QuestTurnIn handler, and possibly `src/game.cpp`) must be updated to pass `game` and `world_tick`.

- [ ] **Step 5: Fire `on_completed` inside `complete_quest`**

Within the modified `complete_quest`, immediately after moving the quest into `completed_`, call:
```cpp
StoryQuest* sq = find_story_quest(quest_id);
if (sq) sq->on_completed(game);
```
(This likely was done at call sites before — check and remove the duplicate calls so `on_completed` fires exactly once.)

- [ ] **Step 6: Build and run**

```bash
cmake --build build -j
./build/astra  # or ./build/astra-dev in DEV mode
```
Verify: existing story quests (Missing Hauler, Getting Airborne) still accept and complete normally. Cascade has nothing to unlock yet — that comes in Task 11.

- [ ] **Step 7: Commit**

```bash
git add include/astra/quest.h src/quest.cpp src/dialog_manager.cpp src/game.cpp
git commit -m "feat(quest): cascade-unlock dependents on quest completion"
```

---

## Task 7: Failure cascade

**Files:**
- Modify: `src/quest.cpp`
- Modify: `include/astra/quest.h`

- [ ] **Step 1: Change `fail_quest` signature**

In `include/astra/quest.h`, change:
```cpp
void fail_quest(const std::string& quest_id);
```
to:
```cpp
void fail_quest(const std::string& quest_id, Game& game);
```

- [ ] **Step 2: Implement cascade**

Replace the body of `fail_quest` in `src/quest.cpp`:

```cpp
void QuestManager::fail_quest(const std::string& quest_id, Game& game) {
    // Collect all descendants before we start mutating
    const auto& graph = quest_graph();
    std::unordered_set<std::string> to_fail = graph.descendants_of(quest_id);
    to_fail.insert(quest_id);

    auto move_failed = [&](std::vector<Quest>& pool, const std::string& id) -> bool {
        for (auto it = pool.begin(); it != pool.end(); ++it) {
            if (it->id == id) {
                it->status = QuestStatus::Failed;
                StoryQuest* sq = find_story_quest(id);
                if (sq) sq->on_failed(game);
                completed_.push_back(std::move(*it));
                pool.erase(it);
                return true;
            }
        }
        return false;
    };

    for (const auto& id : to_fail) {
        if (move_failed(active_, id)) continue;
        if (move_failed(available_, id)) continue;
        if (move_failed(locked_, id)) continue;
        // Already completed or unknown — ignore.
    }
}
```

- [ ] **Step 3: Update callers**

```bash
grep -rn "fail_quest" include/astra/ src/
```
Thread `game` into every call site.

- [ ] **Step 4: Build**

```bash
cmake --build build -j
```
Expected: Clean compile.

- [ ] **Step 5: Commit**

```bash
git add include/astra/quest.h src/quest.cpp  # plus any callers touched
git commit -m "feat(quest): cascade-fail descendants when a quest fails"
```

---

## Task 8: Dialog integration — offer Available quests

**Files:**
- Modify: `src/dialog_manager.cpp`

- [ ] **Step 1: Extend interact menu**

In `src/dialog_manager.cpp` around line 595 (the "Offer new quests" block), replace the block that offers `data.quest` with one that ALSO checks available-pool quests for this role:

Before the existing `if (data.quest) { ... }` block, add:

```cpp
// Offer available story quests from this NPC role
{
    int rep = reputation_for(game.player(), npc.faction);
    bool rep_ok = npc.faction.empty() || reputation_tier(rep) >= ReputationTier::Neutral;
    if (rep_ok) {
        auto offers = game.quests().available_for_role(npc.role);
        for (const Quest* offer : offers) {
            add_option(hotkey++, "Tell me about " + offer->title + ".");
            interact_options_.push_back(InteractOption::Quest);
            pending_story_offers_.push_back(offer->id);  // see Step 2
        }
    }
}
```

- [ ] **Step 2: Track which offer corresponds to which option**

Add private member to `DialogManager` in `include/astra/dialog_manager.h`:

```cpp
std::vector<std::string> pending_story_offers_;  // parallel to Quest options of story type
```

Clear `pending_story_offers_` in `reset_content`.

- [ ] **Step 3: Accept the offer**

In the `case InteractOption::Quest:` handler (around line 996), detect whether this option maps to a standard `data.quest` offer or a story offer. Simplest approach: when adding story offers, push a marker to `interact_options_` (e.g., introduce a new enum value `InteractOption::StoryQuestOffer`). Track the index in `pending_story_offers_`.

Add to the enum in `dialog_manager.h`:
```cpp
enum class InteractOption : uint8_t {
    Talk, Shop, Quest, QuestTurnIn, Farewell, StoryQuestOffer
};
```

Change Step 1's push:
```cpp
interact_options_.push_back(InteractOption::StoryQuestOffer);
```

Add in `advance_dialog` a new case:
```cpp
case InteractOption::StoryQuestOffer: {
    // Which story offer was this? Count preceding StoryQuestOffer entries.
    int story_idx = 0;
    for (int i = 0; i < selected; ++i) {
        if (interact_options_[i] == InteractOption::StoryQuestOffer) ++story_idx;
    }
    if (story_idx < static_cast<int>(pending_story_offers_.size())) {
        const std::string& qid = pending_story_offers_[story_idx];
        game.quests().accept_available(qid, game, game.world_tick());
        close();
        game.log("Quest accepted.");
    }
    return;
}
```

- [ ] **Step 4: Build and test in dev console**

```bash
cmake --build build -j
./build/astra-dev
```

Dev-console checks (once demo arc is in — Task 11):
1. Force-complete Missing Hauler via `/quest complete story_missing_hauler`.
2. Talk to Station Keeper — new offer "Tell me about Track the Signal" should appear.

For now (without the arc), just verify no regressions: existing quest offering still works, no crashes on dialog open.

- [ ] **Step 5: Commit**

```bash
git add include/astra/dialog_manager.h src/dialog_manager.cpp
git commit -m "feat(dialog): offer Available story quests matching NPC role"
```

---

## Task 9: Save/load — version 27

**Files:**
- Modify: `include/astra/save_file.h`
- Modify: `src/save_file.cpp`
- Modify: `src/save_system.cpp`

- [ ] **Step 1: Bump version, add pools to `SaveData`**

In `include/astra/save_file.h`, change `version` default to 27 and add two vectors:

```cpp
uint32_t version = 27;   // v27: quest chains (locked/available pools, arc/prereq fields)

// ... existing ...
std::vector<Quest> locked_quests;
std::vector<Quest> available_quests;
std::vector<Quest> active_quests;
std::vector<Quest> completed_quests;
```

- [ ] **Step 2: Extend `write_quest` / `read_quest` for new fields**

In `src/save_file.cpp`, modify `write_quest`:

After `w.write_i32(q.target_body_index);`, add:
```cpp
// v27: chain fields
w.write_string(q.arc_id);
w.write_u32(static_cast<uint32_t>(q.prerequisite_ids.size()));
for (const auto& p : q.prerequisite_ids) w.write_string(p);
w.write_u8(static_cast<uint8_t>(q.reveal));
```

Modify `read_quest` — it needs the version passed in:

```cpp
static Quest read_quest(BinaryReader& r, uint32_t version) {
    // ... existing reads ...
    if (version >= 27) {
        q.arc_id = r.read_string();
        uint32_t pc = r.read_u32();
        q.prerequisite_ids.resize(pc);
        for (auto& p : q.prerequisite_ids) p = r.read_string();
        q.reveal = static_cast<RevealPolicy>(r.read_u8());
    }
    return q;
}
```

Thread `version` through every callsite of `read_quest`.

- [ ] **Step 3: Handle the `QuestStatus` re-numbering**

In v26 saves, `Available=0, Active=1, Completed=2, Failed=3`. In v27, `Locked=0, Available=1, Active=2, Completed=3, Failed=4`. The raw `u8` read in v26 maps **incorrectly** if loaded directly.

In `read_quest`, after reading status byte, map old → new when `version < 27`:

Replace:
```cpp
q.status = static_cast<QuestStatus>(r.read_u8());
```
with:
```cpp
uint8_t raw = r.read_u8();
if (version < 27) {
    // v26 order: Available=0, Active=1, Completed=2, Failed=3
    static constexpr QuestStatus v26_map[] = {
        QuestStatus::Available, QuestStatus::Active,
        QuestStatus::Completed, QuestStatus::Failed,
    };
    q.status = raw < 4 ? v26_map[raw] : QuestStatus::Failed;
} else {
    q.status = static_cast<QuestStatus>(raw);
}
```

- [ ] **Step 4: Serialize new pools in `QUST` section**

In `write_quest_section`, after completed quests are written, add:

```cpp
// v27: locked pool
w.write_u32(static_cast<uint32_t>(data.locked_quests.size()));
for (const auto& q : data.locked_quests) write_quest(w, q);

// v27: available pool
w.write_u32(static_cast<uint32_t>(data.available_quests.size()));
for (const auto& q : data.available_quests) write_quest(w, q);
```

In `read_quest_section`, find the equivalent point (after completed), and guard:

```cpp
if (version >= 27) {
    uint32_t lc = r.read_u32();
    data.locked_quests.resize(lc);
    for (auto& q : data.locked_quests) q = read_quest(r, version);

    uint32_t ac = r.read_u32();
    data.available_quests.resize(ac);
    for (auto& q : data.available_quests) q = read_quest(r, version);
}
```

- [ ] **Step 5: Wire pools through `save_system.cpp`**

Find where `SaveData` is populated from `QuestManager` (likely in `save_system.cpp` — grep `active_quests =` and `completed_quests =`). Populate the new pools:

```cpp
data.active_quests    = game.quests().active_quests();
data.completed_quests = game.quests().completed_quests();
data.locked_quests    = game.quests().locked_quests();
data.available_quests = game.quests().available_quests();
```

For load, call the new `restore(locked, available, active, completed)` overload, then call `reconcile_with_catalog(game)` for forward compat.

- [ ] **Step 6: Build, start new game, save, reload**

```bash
cmake --build build -j
./build/astra-dev
```

Test:
1. Start new game, verify no crash on save.
2. Save, quit, reload — verify active/completed quests intact.
3. If a v26 save is available, load it — verify the status remap (no spurious Failed/Locked).

- [ ] **Step 7: Commit**

```bash
git add include/astra/save_file.h src/save_file.cpp src/save_system.cpp
git commit -m "feat(save): v27 quest chains — add locked/available pools and chain fields"
```

---

## Task 10: Journal arc rendering

**Files:**
- Modify: `src/journal.cpp`
- Modify: `include/astra/journal.h` (if new render API needed)

- [ ] **Step 1: Read current journal rendering**

```bash
grep -n "Quest\|quest\|arc" include/astra/journal.h src/journal.cpp | head -40
```
Read `src/journal.cpp` around the quest-entry rendering to understand current structure.

- [ ] **Step 2: Add arc-grouping pass**

At the point where quest-category entries are rendered, group by `arc_id`:
1. Collect all visible entries: `completed_`, `active_` from `QuestManager`, plus synthetic entries derived from `locked_` and `available_` (per the reveal policy).
2. Group by `arc_id` (non-empty). Group entries by arc ID into a `map<string, vector<DisplayEntry>>`. Standalone entries (`arc_id == ""`) go into an "Other Quests" section.
3. Within each arc, order members using `quest_graph().arc_members(arc_id)`.

Implementation sketch in `src/journal.cpp`:

```cpp
struct DisplayEntry {
    std::string id;
    std::string title;
    std::string body;
    char marker;         // '✓' '•' '~' '?' '✗' rendered as [x], [•], [~], [?], [✗]
    uint8_t color;
};

static std::vector<DisplayEntry> build_display_entries(const QuestManager& qm) {
    std::vector<DisplayEntry> out;
    for (const auto& q : qm.completed_quests()) {
        DisplayEntry d{q.id, q.title, /*summary*/ "", 'C',
            q.status == QuestStatus::Failed ? /*red*/ 1 : /*green*/ 2};
        out.push_back(std::move(d));
    }
    for (const auto& q : qm.active_quests()) {
        out.push_back({q.id, q.title, /*objectives*/ "", 'A', /*yellow*/ 3});
    }
    for (const auto& q : qm.available_quests()) {
        StoryQuest* sq = find_story_quest(q.id);
        std::string hint = sq && !sq->offer_giver_role().empty()
            ? "Speak to " + sq->offer_giver_role() : "";
        out.push_back({q.id, q.title, hint, 'V', /*gray*/ 4});
    }
    for (const auto& q : qm.locked_quests()) {
        StoryQuest* sq = find_story_quest(q.id);
        RevealPolicy rev = sq ? sq->reveal_policy() : RevealPolicy::Full;
        std::string title = rev == RevealPolicy::Hidden ? "???" : q.title;
        std::string body  = rev == RevealPolicy::Full ? q.description : "Locked";
        out.push_back({q.id, title, body, 'L', /*dark gray*/ 5});
    }
    return out;
}
```

Then the existing renderer iterates entries by arc and emits the `── Arc Title ────` header followed by the entries in topological order (use `quest_graph().arc_members(arc_id)` as the ordering oracle).

- [ ] **Step 3: Build and visually verify**

```bash
cmake --build build -j
./build/astra-dev
```
Open the journal screen. Verify:
1. Existing active/completed quests still appear.
2. No crash.
3. No arc header yet (the demo arc comes in Task 11).

- [ ] **Step 4: Commit**

```bash
git add src/journal.cpp include/astra/journal.h
git commit -m "feat(journal): group quests by arc with reveal-policy rendering"
```

---

## Task 11: Convert existing story quests to catalog-driven offers

**Files:**
- Modify: `src/quests/missing_hauler.cpp`
- Modify: `src/quests/getting_airborne.cpp`

- [ ] **Step 1: Read existing quests**

```bash
cat src/quests/missing_hauler.cpp src/quests/getting_airborne.cpp
```
Note how they currently get into the active list (bespoke accept calls vs dialog offer).

- [ ] **Step 2: Add `offer_giver_role` to `MissingHaulerQuest`**

In `src/quests/missing_hauler.cpp`, inside the `MissingHaulerQuest` class body, add:

```cpp
std::string offer_giver_role() const override { return "Station Keeper"; }
OfferMode offer_mode() const override { return OfferMode::NpcOffer; }
```

- [ ] **Step 3: Add `offer_giver_role` to `GettingAirborneQuest`**

Open `src/quests/getting_airborne.cpp`. If this quest is tutorial-auto (likely), set:

```cpp
OfferMode offer_mode() const override { return OfferMode::Auto; }
```

If it should remain NPC-offered, set `offer_giver_role()` to the appropriate role.

- [ ] **Step 4: Remove bespoke new-game acceptance**

Wherever `accept_quest(MissingHaulerQuest().create_quest(), ...)` was called on new-game, remove it. Rely on `QuestManager::init_from_catalog(game)` (added in Task 5) to place the quest into `available_` and the dialog offer flow (Task 8) to let the player accept it.

- [ ] **Step 5: Build and test**

```bash
cmake --build build -j
./build/astra-dev
```

Start a new game. Talk to the Station Keeper. The Missing Hauler should now appear as an offer ("Tell me about The Missing Hauler.") instead of being pre-accepted.

- [ ] **Step 6: Commit**

```bash
git add src/quests/missing_hauler.cpp src/quests/getting_airborne.cpp src/game.cpp
git commit -m "refactor(quest): route existing story quests through catalog offer flow"
```

---

## Task 12: Demo arc — Track the Signal (B), Pirate Outpost (C), What They Found (D)

**Files:**
- Create: `src/quests/hauler_b.cpp`
- Create: `src/quests/hauler_c.cpp`
- Create: `src/quests/hauler_d.cpp`
- Modify: `src/quests/missing_hauler.cpp` (arc_id + catalog registration)
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Set `arc_id` on `MissingHaulerQuest`**

In `src/quests/missing_hauler.cpp`, inside the `MissingHaulerQuest` class, add:

```cpp
std::string arc_id() const override { return "hauler_arc"; }
std::string arc_title() const override { return "The Hauler Arc"; }
```

- [ ] **Step 2: Create `hauler_b.cpp`**

`src/quests/hauler_b.cpp`:

```cpp
#include "astra/quest.h"
#include "astra/game.h"
#include "astra/faction.h"

namespace astra {

class HaulerBQuest : public StoryQuest {
public:
    static constexpr const char* quest_id = "story_hauler_track_signal";

    Quest create_quest() override {
        Quest q;
        q.id = quest_id;
        q.title = "Track the Signal";
        q.description = "The cargo manifest revealed a pirate transmission source. "
                        "Trace it to its origin.";
        q.giver_npc = "Station Keeper";
        q.is_story = true;
        q.objectives.push_back({ObjectiveType::TalkToNpc,
            "Report back to the Station Keeper", 1, 0, "Station Keeper"});
        q.reward.xp = 250;
        q.reward.credits = 150;
        q.reward.faction_name = Faction_StellariConclave;
        q.reward.reputation_change = 8;
        return q;
    }

    std::string arc_id() const override { return "hauler_arc"; }
    std::string arc_title() const override { return "The Hauler Arc"; }
    std::vector<std::string> prerequisite_ids() const override {
        return {"story_missing_hauler"};
    }
    OfferMode offer_mode() const override { return OfferMode::NpcOffer; }
    std::string offer_giver_role() const override { return "Station Keeper"; }

    void on_unlocked(Game& game) override {
        game.log("New intel from the Station Keeper. [J]ournal.");
    }
};

void register_hauler_b(std::vector<std::unique_ptr<StoryQuest>>& c) {
    c.push_back(std::make_unique<HaulerBQuest>());
}

} // namespace astra
```

- [ ] **Step 3: Create `hauler_c.cpp` (combat branch)**

`src/quests/hauler_c.cpp`:

```cpp
#include "astra/quest.h"
#include "astra/game.h"

namespace astra {

class HaulerCQuest : public StoryQuest {
public:
    static constexpr const char* quest_id = "story_hauler_pirate_outpost";

    Quest create_quest() override {
        Quest q;
        q.id = quest_id;
        q.title = "The Pirate Outpost";
        q.description = "Assault the pirate base. They're excavating something ancient.";
        q.giver_npc = "Commander";
        q.is_story = true;
        q.objectives.push_back({ObjectiveType::KillNpc,
            "Clear the outpost", 5, 0, "Pirate"});
        q.objectives.push_back({ObjectiveType::TalkToNpc,
            "Report to the Commander", 1, 0, "Commander"});
        q.reward.xp = 400;
        q.reward.credits = 200;
        return q;
    }

    std::string arc_id() const override { return "hauler_arc"; }
    std::vector<std::string> prerequisite_ids() const override {
        return {"story_hauler_track_signal"};
    }
    RevealPolicy reveal_policy() const override { return RevealPolicy::TitleOnly; }
    OfferMode offer_mode() const override { return OfferMode::NpcOffer; }
    std::string offer_giver_role() const override { return "Commander"; }
};

void register_hauler_c(std::vector<std::unique_ptr<StoryQuest>>& c) {
    c.push_back(std::make_unique<HaulerCQuest>());
}

} // namespace astra
```

- [ ] **Step 4: Create `hauler_d.cpp` (exploration branch)**

`src/quests/hauler_d.cpp`:

```cpp
#include "astra/quest.h"
#include "astra/game.h"

namespace astra {

class HaulerDQuest : public StoryQuest {
public:
    static constexpr const char* quest_id = "story_hauler_what_they_found";

    Quest create_quest() override {
        Quest q;
        q.id = quest_id;
        q.title = "What They Found";
        q.description = "An Astronomer wants you to quietly inspect the site "
                        "and bring back any precursor artifacts.";
        q.giver_npc = "Astronomer";
        q.is_story = true;
        q.objectives.push_back({ObjectiveType::CollectItem,
            "Recover a precursor artifact", 1, 0, "Precursor Relic"});
        q.objectives.push_back({ObjectiveType::TalkToNpc,
            "Report to the Astronomer", 1, 0, "Astronomer"});
        q.reward.xp = 400;
        q.reward.credits = 150;
        return q;
    }

    std::string arc_id() const override { return "hauler_arc"; }
    std::vector<std::string> prerequisite_ids() const override {
        return {"story_hauler_track_signal"};
    }
    RevealPolicy reveal_policy() const override { return RevealPolicy::Hidden; }
    OfferMode offer_mode() const override { return OfferMode::NpcOffer; }
    std::string offer_giver_role() const override { return "Astronomer"; }
};

void register_hauler_d(std::vector<std::unique_ptr<StoryQuest>>& c) {
    c.push_back(std::make_unique<HaulerDQuest>());
}

} // namespace astra
```

- [ ] **Step 5: Register in catalog builder**

In `src/quests/missing_hauler.cpp`, update `build_catalog`:

```cpp
void register_getting_airborne(std::vector<std::unique_ptr<StoryQuest>>& catalog);
void register_hauler_b(std::vector<std::unique_ptr<StoryQuest>>& catalog);
void register_hauler_c(std::vector<std::unique_ptr<StoryQuest>>& catalog);
void register_hauler_d(std::vector<std::unique_ptr<StoryQuest>>& catalog);

static std::vector<std::unique_ptr<StoryQuest>> build_catalog() {
    std::vector<std::unique_ptr<StoryQuest>> catalog;
    catalog.push_back(std::make_unique<MissingHaulerQuest>());
    register_getting_airborne(catalog);
    register_hauler_b(catalog);
    register_hauler_c(catalog);
    register_hauler_d(catalog);
    return catalog;
}
```

- [ ] **Step 6: Add to CMake**

In `CMakeLists.txt`, add the three new files after `src/quests/getting_airborne.cpp`:

```
    src/quests/getting_airborne.cpp
    src/quests/hauler_b.cpp
    src/quests/hauler_c.cpp
    src/quests/hauler_d.cpp
```

- [ ] **Step 7: Build and end-to-end test**

```bash
cmake -B build -DDEV=ON && cmake --build build -j
./build/astra-dev
```

**Flow check:**
1. Start new game, journal shows only `[•] The Missing Hauler` + locked entries from the arc: `[?] ???` for B (Full policy — wait, B is Full default), then `[?] The Pirate Outpost` (TitleOnly), `[?] ???` (Hidden D).

   Actually B defaults to `Full` reveal, so it'll read `[?] Track the Signal` with full description. C is TitleOnly → `[?] The Pirate Outpost`, locked hint only. D is Hidden → `[?] ???`.

2. Use dev console `/quest complete story_missing_hauler` (or finish it legitimately).
3. Journal: A moves to `[✓]`. B moves to `[~]` with hint "Speak to Station Keeper".
4. Talk to Station Keeper → offer appears → accept → B is `[•]`.
5. Dev console `/quest complete story_hauler_track_signal`.
6. Journal: B moves to `[✓]`. C becomes `[~]` (Commander), D becomes `[~]` (Astronomer) — both simultaneously.
7. Both C and D independently offerable.

- [ ] **Step 8: Failure cascade test**

In dev console, force-fail B (`/quest fail story_hauler_track_signal`). C and D (still `[?]` locked at this point if run prior to completing B) — re-check: if B is active and we fail it, C and D are still in `locked_` (never unlocked). After fail: they transition to `Failed`. Arc renders with `✗` in header.

- [ ] **Step 9: Commit**

```bash
git add src/quests/missing_hauler.cpp src/quests/hauler_b.cpp src/quests/hauler_c.cpp src/quests/hauler_d.cpp CMakeLists.txt
git commit -m "feat(quest): add Hauler Arc demo — B unlocks {C, D} fan-out"
```

---

## Task 13: Update roadmap and formulas docs

**Files:**
- Modify: `docs/roadmap.md`

- [ ] **Step 1: Check off quest chain milestone**

In `docs/roadmap.md`, find the quest-related section. Add or check off:

```markdown
- [x] Quest chains (DAG prerequisites, reveal policies, NPC-offer flow) — 2026-04-13
- [x] Demo: Hauler Arc fan-out (A → B → {C, D})
```

No formula changes for this feature (no numeric rules introduced).

- [ ] **Step 2: Commit**

```bash
git add docs/roadmap.md
git commit -m "docs: check off quest chains milestone in roadmap"
```

---

## Final Verification

- [ ] **Full dev build & smoke test**

```bash
cmake -B build -DDEV=ON && cmake --build build -j
./build/astra-dev
```

Pass criteria:
1. New game starts without crash.
2. Journal shows Missing Hauler + locked arc members per reveal policies.
3. Full arc playthrough (A → B → accept C and D → both offerable) works via dev console fast-forward.
4. Save/load preserves all four pools correctly.
5. v26 save file (if available) loads with the status remap intact.
6. Invalid catalog (intentionally broken prereq) triggers the validator and aborts in dev build.

- [ ] **Prep for merge**

```bash
git log --oneline main..feat/quest-chains
```
Every commit should be logically self-contained and green. If any fix-upon-fix chains exist, interactive-rebase to squash before requesting merge (per the "clean commits" feedback memory).

---

## Notes for the executing engineer

- **Project has no unit test suite.** Verification is build + dev-console + manual play. Don't add a test framework.
- **Use `-DDEV=ON` for every build.** The dev console is the primary verification tool.
- **Don't invent file paths.** Every file referenced here either exists (check with `ls`) or is explicitly flagged for creation.
- **Follow the spec's authoring guide section** when writing `hauler_b/c/d.cpp` — it's the canonical reference.
- **Commit after every task.** Merge cleanup later is easier with small, focused commits.
- **Don't push** unless the user explicitly asks.
- **Don't merge to main** without the user verifying the build first.
