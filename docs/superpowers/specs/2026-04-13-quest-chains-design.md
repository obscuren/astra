# Quest Chains & System Refinement — Design

**Date:** 2026-04-13
**Status:** Draft — not yet implemented

## Summary

Extend the existing quest system to support **hand-authored story arcs** structured as arbitrary DAGs (directed acyclic graphs) of quests. A quest may declare any number of prerequisite quests; it unlocks when **all** prerequisites are `Completed` (AND semantics). This covers linear chains (A → B → C), fan-out (A → B → {C, D}), diamond merges (A → B → {C, D} → E), and cross-arc dependencies.

Random/procedural quests are unchanged in this spec — they remain single-objective, single-shot. All DAG mechanics apply only to `StoryQuest` subclasses.

Companion context: `docs/quest_system_analysis.md` (broader system improvements; this spec implements Rec 5 with DAG branching and leaves the other recommendations for future work).

---

## Goals

- Hand-authored story arcs expressible as DAGs with multiple prerequisites per node.
- A diamond pattern (A → B → {C, D} → E) must work cleanly; arbitrary DAG shapes must be representable.
- Player sees an arc's progress in the journal, with per-quest reveal controls (full / title-only / hidden).
- Authoring is additive: a new `StoryQuest` subclass with a few overrides. No external data files, no DSL, no scripting engine.
- Failures cascade: if any quest in an arc fails, all downstream descendants auto-fail. Roguelike rule — no retries.
- Save/load preserves chain state across game versions (new quests added to the catalog can still slot in).

## Non-goals

- Procedural mini-chains from the random generator (deferred).
- Retry/re-offer of failed story quests.
- OR-semantics prerequisites ("E unlocks when C *or* D"). AND only in this revision.
- World-driven random generation, flavor pools, reward variety, DeliverItem wiring, quest expiry — all from `quest_system_analysis.md`, unchanged by this spec.
- A visual quest editor or graph renderer.

---

## Data Model

### `QuestStatus` (extended)

```cpp
enum class QuestStatus : uint8_t {
    Locked,     // NEW — prerequisites not yet satisfied
    Available,  // offered (by NPC) or pending auto-accept
    Active,
    Completed,
    Failed,
};
```

### `RevealPolicy` (new)

Controls how a `Locked` quest appears in the journal:

```cpp
enum class RevealPolicy : uint8_t {
    Hidden,     // "??? — ???", only a "Locked" hint
    TitleOnly,  // show title, hide description
    Full,       // show title + description
};
```

### `OfferMode` (new)

Controls what happens when a quest transitions from `Locked` → prerequisites satisfied:

```cpp
enum class OfferMode : uint8_t {
    Auto,       // becomes Active immediately; on_unlocked then on_accepted fire
    NpcOffer,   // enters Available; a named NPC role offers it via dialog
};
```

### `Quest` (extended)

```cpp
struct Quest {
    // ... all existing fields unchanged ...

    // Arc / DAG
    std::string arc_id;                          // groups quests into a narrative arc ("" = standalone)
    std::vector<std::string> prerequisite_ids;   // all must be Completed for unlock
    RevealPolicy reveal = RevealPolicy::Full;    // journal visibility while Locked
};
```

No separate `QuestChain` container — the graph is implicit in the edges.

### `StoryQuest` (extended base class)

```cpp
class StoryQuest {
public:
    virtual ~StoryQuest() = default;

    virtual Quest create_quest() = 0;

    // Arc / DAG declarations (defaults = standalone, no arc)
    virtual std::string arc_id() const { return ""; }
    virtual std::string arc_title() const { return ""; }   // display name; optional
    virtual std::vector<std::string> prerequisite_ids() const { return {}; }
    virtual RevealPolicy reveal_policy() const { return RevealPolicy::Full; }

    // Offer semantics
    virtual OfferMode offer_mode() const { return OfferMode::NpcOffer; }
    virtual std::string offer_giver_role() const { return ""; }  // NPC role that will offer it

    // Lifecycle hooks
    virtual void on_unlocked(Game& game) {}   // NEW — prereqs first satisfied
    virtual void on_accepted(Game& game) {}
    virtual void on_completed(Game& game) {}
    virtual void on_failed(Game& game) {}
};
```

### `QuestGraph` (new helper, runtime-only)

Built once at startup from `story_quest_catalog()`. Stores:

- `dependents_of(id) → vector<id>` — forward edges (who does this quest unlock?)
- `prerequisites_of(id) → vector<id>` — reverse edges
- `arc_of(id) → arc_id`
- `arc_members(arc_id) → vector<id>`, topologically sorted

`QuestGraph` holds no mutable state — it's pure lookup. The live status of each node lives in `QuestManager`.

---

## Runtime Flow (QuestManager)

`QuestManager` gains two new pools alongside `active_` / `completed_`:

- `locked_` — story quests whose prerequisites aren't yet satisfied.
- `available_` — unlocked, waiting for NPC dialog acceptance (only `NpcOffer` quests).

### New game initialization

For each `StoryQuest` in the catalog:
- No prerequisites + `OfferMode::Auto` → auto-accept at game start (or at the trigger defined by `on_unlocked`).
- No prerequisites + `OfferMode::NpcOffer` → enters `available_`; offered by the declared NPC role.
- Has prerequisites → enters `locked_`.

### Completion → unlock cascade

On `complete_quest(id)`, after moving the quest to `completed_`:

```
for each q in graph.dependents_of(id):
    if q in locked_ and all q.prerequisite_ids are Completed:
        call q.on_unlocked(game)
        if q.offer_mode == Auto:
            accept_quest(q, ...)           // fires on_accepted
        else:
            move q: locked_ → available_   // offered by its NPC role
```

### Failure cascade

On `fail_quest(id)`:

```
mark id Failed
walk graph.dependents transitively:
    for each descendant d in {locked_, available_, active_}:
        move d to completed_ with status Failed
        call d.on_failed(game)
```

Arc is dead. No retry. Matches roguelike conventions.

### Dialog integration

`DialogManager` already queries `QuestManager` for quests to offer. Add:

```cpp
std::vector<const Quest*> QuestManager::available_for_role(const std::string& role) const;
```

Returns every `available_` quest whose `offer_giver_role()` matches. Dialog presents an "Accept" option that promotes `Available → Active` and fires `on_accepted`.

### Startup validator

Runs once at `story_quest_catalog()` construction. Reports errors to the dev log and aborts in dev builds:

- Every referenced `prerequisite_id` must exist in the catalog.
- No cycles (DFS).
- A quest with `OfferMode::NpcOffer` must declare a non-empty `offer_giver_role()`.
- Warn (not error) if a quest's prereqs come from a different `arc_id`.

### Save / load

Serialize all four pools: `active_`, `completed_`, `locked_`, `available_`. On restore:

1. Rebuild `QuestGraph` from the current catalog.
2. Drop any serialized quest whose ID is no longer in the catalog (catalog shrank), with a journal warning.
3. For any catalog quest not present in any pool (catalog grew), synthesize a `locked_` entry and re-run the unlock cascade — handles new arcs slotting into an existing save.

---

## Journal UI

Story quests render grouped by `arc_id`. Standalone story and random quests render ungrouped below the arc section.

### Arc header

```
── <arc_title> ───────────────────────
```

Appended marks: `✓` when every member is `Completed`, `✗` when any member is `Failed`. Completed arcs collapse by default (expand with a keypress).

### Entries within an arc

Topologically ordered (roots first), stable by registration order within a rank.

| Status | Marker | Render |
|---|---|---|
| `Completed` | `[✓]` | Full title + reward summary |
| `Active` | `[•]` | Full title + objective checklist |
| `Available` | `[~]` | Title greyed; hint: `Speak to <giver_role> on <known location or ???>` |
| `Locked`, `reveal=Full` | `[?]` | Title + description; hint: `Locked — requires <prereq titles>` |
| `Locked`, `reveal=TitleOnly` | `[?]` | Title only; hint: `Locked` |
| `Locked`, `reveal=Hidden` | `[?]` | `??? — ???`; hint: `Locked` |
| `Failed` | `[✗]` | Title + "Lost with the arc." |

Ghost entries (`Locked` / `Available` / `Failed`) are **derived at render time** from `QuestManager` pools — they do not create persistent `JournalEntry` records. Only `accept_quest` / `complete_quest` write journal entries, as today.

---

## Authoring Guide — How to Create Quests

**This section is the canonical reference for future agent work.** Read it before adding any story quest.

### File layout

One file per quest (or per small arc), under `src/quests/`. Example: `src/quests/hauler_b.cpp`.

Each quest is a `StoryQuest` subclass. Register it by adding a line to the catalog builder in `src/quest.cpp` (`build_catalog`).

### Minimum viable standalone story quest

```cpp
#include "astra/quest.h"
#include "astra/game.h"

namespace astra {

class MyQuest : public StoryQuest {
public:
    static constexpr const char* quest_id = "story_my_quest";

    Quest create_quest() override {
        Quest q;
        q.id = quest_id;
        q.title = "My Quest";
        q.description = "Do the thing.";
        q.giver_npc = "Station Keeper";
        q.is_story = true;
        q.objectives.push_back({ObjectiveType::TalkToNpc, "Report back",
                                1, 0, "Station Keeper"});
        q.reward.xp = 100;
        return q;
    }

    std::string offer_giver_role() const override { return "Station Keeper"; }

    void on_accepted(Game& game) override { /* world setup on acceptance */ }
    void on_completed(Game& game) override { /* cleanup */ }
};

void register_my_quest(std::vector<std::unique_ptr<StoryQuest>>& c) {
    c.push_back(std::make_unique<MyQuest>());
}

} // namespace astra
```

### Adding a quest to an existing arc (parenting)

To declare that `QuestB` requires `QuestA`:

```cpp
class QuestB : public StoryQuest {
public:
    static constexpr const char* quest_id = "arc_hauler_b";

    Quest create_quest() override { /* ... */ }

    std::string arc_id() const override { return "hauler_arc"; }
    std::string arc_title() const override { return "The Hauler Arc"; }
    std::vector<std::string> prerequisite_ids() const override {
        return {"arc_hauler_a"};
    }

    OfferMode offer_mode() const override { return OfferMode::NpcOffer; }
    std::string offer_giver_role() const override { return "Station Keeper"; }

    void on_unlocked(Game& game) override {
        // The Station Keeper's dialog now offers this quest.
        // Optionally seed the world here (e.g. spawn a comm log, prime a POI).
    }

    void on_accepted(Game& game) override { /* world injection as today */ }
};
```

### Diamond / fan-out authoring

For `A → B → {C, D} → E`:

| Quest | `prerequisite_ids()` |
|---|---|
| A | `{}` |
| B | `{"arc_hauler_a"}` |
| C | `{"arc_hauler_b"}` |
| D | `{"arc_hauler_b"}` |
| E | `{"arc_hauler_c", "arc_hauler_d"}` |

E will only unlock when **both** C and D are `Completed`. All must declare the same `arc_id` to render under one header.

### Choosing `OfferMode`

- **`NpcOffer` (default)** — the quest waits for the player to visit an NPC. Use for arcs where the player travels back to a hub or seeks out a specific character. Requires a non-empty `offer_giver_role()`.
- **`Auto`** — the quest becomes active immediately on unlock. Use when a plot beat shouldn't require travel — an in-flight comms message, an ARIA intervention, a beacon pulse. Typically `on_unlocked()` logs a narrative line to the player.

### Choosing `RevealPolicy`

- **`Full`** — boring arcs, tutorial arcs. Player sees the whole path.
- **`TitleOnly`** — pivot quests you want to name-drop for intrigue ("The Ghost Ship") without spoiling details.
- **`Hidden`** — surprise reveals, twist beats. Player only learns of the quest when it unlocks.

Default is `Full`. Prefer `TitleOnly` or `Hidden` for story punch points; don't reveal-spam every node.

### Using `on_unlocked` vs `on_accepted`

| Hook | Fires when | Use for |
|---|---|---|
| `on_unlocked` | prereqs first satisfied | Priming the world so the quest *can* be accepted: spawn the offering NPC's trigger, place a comm log, mark a POI as discoverable. Safe to modify world even if the player never accepts. |
| `on_accepted` | player accepts (or auto-accept fires) | Committing to the quest: inject dungeons, register `QuestLocationMeta`, set waypoints, log coordinates. |

For `OfferMode::Auto`, both fire back-to-back.

### Registering the quest

Add to `build_catalog()` in `src/quest.cpp`:

```cpp
void register_hauler_b(std::vector<std::unique_ptr<StoryQuest>>&);

static std::vector<std::unique_ptr<StoryQuest>> build_catalog() {
    std::vector<std::unique_ptr<StoryQuest>> c;
    c.push_back(std::make_unique<MissingHaulerQuest>());
    register_getting_airborne(c);
    register_hauler_b(c);   // NEW
    return c;
}
```

The startup validator will catch typos in prerequisite IDs, cycles, and missing `offer_giver_role` on `NpcOffer` quests.

### Rules / gotchas

- **Quest IDs are forever.** Renaming an ID breaks saves. If an arc is in production, pick IDs carefully.
- **Prereqs must point to real IDs.** Validator aborts in dev builds. In release it drops the dangling quest with a warning.
- **Don't mutate another quest's fields.** Each quest owns its own data. Use `on_unlocked` / `on_accepted` to mutate world state, not quest state.
- **Don't set `status` manually in `create_quest()`.** The manager handles status. `create_quest()` returns a template; status is assigned when the quest enters a pool.
- **Failures cascade.** A quest mid-arc that fails dooms everything downstream. Design arcs with this in mind — critical path quests shouldn't have trivial fail conditions.
- **Cross-arc prereqs** are allowed (the validator warns but doesn't abort). Use sparingly — the journal groups by `arc_id`, so a quest whose prereq lives in another arc reads as a surprise unlock.

---

## Demo Arc (proof-of-concept scope)

Extend `MissingHaulerQuest` (A) with two follow-ups forming a small fan-out:

- **A — The Missing Hauler** (existing)
- **B — Track the Signal** (unlocks on A complete; Station Keeper offers)
  - Fan-out:
    - **C — The Pirate Outpost** (combat branch; Commander offers)
    - **D — What They Found** (exploration branch; Astronomer offers)

Diamond merge to an E is deferred to future content. This arc exercises: single-parent chain (B), fan-out (C, D), `NpcOffer` with different giver roles, at least one `TitleOnly` reveal (C), and failure cascade (failing B dooms C and D).

---

## Implementation Checklist (for the forthcoming plan)

1. Add `Locked` / `Available` to `QuestStatus`; add `RevealPolicy`, `OfferMode`.
2. Extend `Quest` struct with `arc_id`, `prerequisite_ids`, `reveal`.
3. Extend `StoryQuest` base with `arc_id`, `arc_title`, `prerequisite_ids`, `reveal_policy`, `offer_mode`, `offer_giver_role`, `on_unlocked`.
4. Build `QuestGraph` (catalog-driven, lookup-only).
5. Add `locked_` / `available_` pools + init-from-catalog in `QuestManager`.
6. Implement unlock cascade in `complete_quest`.
7. Implement failure cascade in `fail_quest`.
8. Startup validator (missing IDs, cycles, `NpcOffer` giver non-empty).
9. `DialogManager` integration: `available_for_role(role)` + "Accept" flow.
10. Journal arc grouping + reveal policy rendering.
11. Save/load for new pools + reconciliation on catalog changes.
12. Author the demo arc (B, C, D) and wire dialog for offer NPCs.

---

## Out of scope — explicitly deferred

- OR-semantics prerequisites.
- Procedural mini-chains from the random generator.
- Retry / re-offer of failed story quests.
- Quest expiry, DeliverItem wiring, flavor pools, world-driven random generation, reward variety, NPC role expansion — see `docs/quest_system_analysis.md`.
- Quest graph visual renderer (in-game arc map UI).
