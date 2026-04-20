# Stellar Signal — Return + Siege Kickoff Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Chain Stage 4 of The Stellar Signal arc — the probe quest's auto-completion in Conclave space cascades into a new "Return to the Heavens Above" quest; arriving at HA auto-completes it, plays ARIA's panic transmission, then auto-accepts Nova's "They Came For Her" quest. Introduces a generic auto-accept popup for any `OfferMode::Auto` quest.

**Architecture:** Reuse `DialogManager`'s existing quest-offer panel (single `Accept` option instead of Accept/Decline). Session-local announcement queue on `QuestManager`. Game-loop idle drain chains modals across `PlaybackViewer` + `DialogManager` without per-scenario wiring. Two new `StoryQuest` subclasses.

**Tech Stack:** C++20, existing Astra subsystems (QuestManager, DialogManager, PlaybackViewer, EventBus, scenario_effects).

**Reference:** Design spec `docs/superpowers/specs/2026-04-20-stellar-signal-return-and-siege-design.md`.

**Build command used throughout:** `cmake --build build`
(assumes `cmake -B build -DDEV=ON` has been run once; per user memory, always build with dev mode).

**No automated test suite exists in this repo.** Verification is via build + manual smoke test through the `/dev` console in the final task.

---

### Task 1: Extract `format_quest_body` helper

Pull the NPC-independent portion of `format_quest_offer` into a free function so both `DialogManager` (NPC offer flow) and the new auto-accept popup can share it.

**Files:**
- Create: `include/astra/quest_ui.h`
- Create: `src/quest_ui.cpp`
- Modify: `CMakeLists.txt` (add the new `.cpp`)
- Modify: `src/dialog_manager.cpp` (replace inline `format_quest_offer` with a wrapper delegating to `format_quest_body`)

- [ ] **Step 1: Create `include/astra/quest_ui.h`**

```cpp
#pragma once

#include <string>

namespace astra {

struct Quest;

// Render a quest's body text — description, objectives, rewards —
// without any speaker preamble. Shared by the NPC offer dialog and the
// auto-accept quest popup.
std::string format_quest_body(const Quest& q);

} // namespace astra
```

- [ ] **Step 2: Create `src/quest_ui.cpp`**

Body text, objective bullet list, rewards block. Mirrors the existing
logic in `src/dialog_manager.cpp` but drops the `"<NPC> explains:"` line.

```cpp
#include "astra/quest_ui.h"

#include "astra/item.h"
#include "astra/quest.h"
#include "astra/renderer.h"

namespace astra {

std::string format_quest_body(const Quest& q) {
    std::string s;
    if (!q.description.empty()) s += q.description + "\n\n";

    if (!q.objectives.empty()) {
        s += colored("Objectives:", Color::DarkGray) + "\n";
        for (const auto& obj : q.objectives) {
            s += "  \xe2\x80\xa2 " + obj.description + "\n";
        }
        s += "\n";
    }

    const auto& r = q.reward;
    bool has_reward = r.xp > 0 || r.credits > 0 || r.skill_points > 0
                   || !r.items.empty() || !r.factions.empty();
    if (has_reward) {
        s += colored("Rewards:", Color::DarkGray) + "\n";
        for (const auto& it : r.items) {
            s += "  " + it.label() + "\n";
        }
        if (r.xp > 0)
            s += "  " + colored(std::to_string(r.xp) + " XP", Color::Cyan) + "\n";
        if (r.credits > 0)
            s += "  " + colored(std::to_string(r.credits) + "$", Color::Yellow) + "\n";
        if (r.skill_points > 0)
            s += "  " + colored(std::to_string(r.skill_points) + " SP", Color::Cyan) + "\n";
        for (const auto& fr : r.factions) {
            if (fr.faction_name.empty() || fr.reputation_change == 0) continue;
            std::string sign = fr.reputation_change > 0 ? "+" : "";
            s += "  " + colored(sign + std::to_string(fr.reputation_change)
                                + " reputation with " + fr.faction_name, Color::Green)
               + "\n";
        }
    }
    return s;
}

} // namespace astra
```

Note: the original dialog_manager version used `display_name(it)` for
item labels. `Item::label()` is the same content without the
NPC-displayed name extras. Verify against the item header if the build
warns — if `label()` is not available, swap for `it.name` (the existing
Item struct has `name`).

- [ ] **Step 3: Verify which Item member to use**

Open `include/astra/item.h` and check whether `Item` has `label()`. If yes, keep the code above. If only `name`, replace `it.label()` with `it.name` in `quest_ui.cpp`.

Run: `grep -E 'std::string (label|name)' include/astra/item.h`

- [ ] **Step 4: Update `src/dialog_manager.cpp` to use the helper**

Replace the anonymous-namespace `format_quest_offer` (currently lines ~33–70) with a wrapper that delegates:

```cpp
#include "astra/quest_ui.h"   // add near the top alongside existing includes
```

Replace the whole `namespace { std::string format_quest_offer(...) { ... } }` block with:

```cpp
namespace {
std::string format_quest_offer(const Quest& q, const Npc& npc) {
    std::string s = display_name(npc) + " explains:\n\n";
    s += format_quest_body(q);
    return s;
}
} // namespace
```

Every existing call site (`body_ = format_quest_offer(*offer, *interacting_npc_);` etc.) continues to work unchanged.

- [ ] **Step 5: Register the new source file in `CMakeLists.txt`**

Find the `add_executable(astra ...)` and `add_executable(astra-dev ...)` blocks (or the list they share) and add `src/quest_ui.cpp` alongside other `src/*.cpp` entries.

Run: `grep -n 'src/dialog_manager.cpp' CMakeLists.txt` to locate the list, then add `src/quest_ui.cpp` on a new line nearby.

- [ ] **Step 6: Build**

Run: `cmake --build build`
Expected: builds cleanly.

- [ ] **Step 7: Commit**

```bash
git add include/astra/quest_ui.h src/quest_ui.cpp src/dialog_manager.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
refactor(dialog): extract format_quest_body into quest_ui helper

Splits the NPC-independent quest-info formatting out of dialog_manager
so the upcoming auto-accept popup can share it. format_quest_offer
stays as a thin wrapper that adds the "<NPC> explains:" preamble.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Add `DialogManager::show_auto_accept` + `AutoAcceptAck` handler

Standalone entry point that displays quest info with a single `[a] Accept` option. Quest is already in `active_` when this opens.

**Files:**
- Modify: `include/astra/dialog_manager.h`
- Modify: `src/dialog_manager.cpp`

- [ ] **Step 1: Add `InteractOption::AutoAcceptAck` to the enum**

In `include/astra/dialog_manager.h`, inside the existing
`enum class InteractOption` near the top of the private section, add
`AutoAcceptAck` to the list. Pick a spot near `StoryQuestAccept` so it
lives alongside related entries:

```cpp
enum class InteractOption : uint8_t {
    Talk, Shop, Quest, QuestTurnIn,
    StoryQuestOffer, StoryQuestAccept, StoryQuestDecline,
    AutoAcceptAck,
    NovaHookEntry, NovaHookCare, NovaHookSkeptic, NovaHookAction,
    NovaHookConfirm,
    Farewell,
};
```

- [ ] **Step 2: Declare the public method**

Add right after `show_tutorial_followup();` in `include/astra/dialog_manager.h`:

```cpp
// Announce an auto-accepted quest. Shows the quest info panel with a
// single [a] Accept option. The quest is already in the active pool
// when this is called; Accept just dismisses.
void show_auto_accept(Game& game, const Quest& quest);
```

Add a forward declaration near the top of the file (under
`class Game; // forward declare`):

```cpp
struct Quest;
```

- [ ] **Step 3: Implement `show_auto_accept` in `src/dialog_manager.cpp`**

Add a new include at the top if `astra/quest.h` is not already there.
Place the implementation near the other public methods (after
`show_tutorial_followup` is a good neighbour):

```cpp
void DialogManager::show_auto_accept(Game& /*game*/, const Quest& q) {
    (void)q;  // silence unused if we ever short-circuit

    std::string header = "New Mission";
    if (!q.arc_id.empty()) {
        StoryQuest* sq = find_story_quest(q.id);
        std::string arc = sq ? sq->arc_title() : std::string{};
        if (!arc.empty()) header += " — " + arc;
        else               header += " — " + q.title;
    } else {
        header += " — " + q.title;
    }

    reset_content(header, 0.45f);
    interacting_npc_ = nullptr;
    dialog_tree_ = nullptr;
    dialog_node_ = -1;
    detail_offer_quest_id_.clear();
    pending_story_offers_.clear();
    interact_options_.clear();

    body_ = format_quest_body(q);

    add_option('a', "Accept");
    interact_options_.push_back(InteractOption::AutoAcceptAck);

    footer_ = "[Space] Accept";
    open_ = true;
}
```

- [ ] **Step 4: Handle `AutoAcceptAck` in the option dispatch**

In `src/dialog_manager.cpp`, find the `switch` block inside
`advance_dialog` that lists `InteractOption::StoryQuestAccept:` (around
line ~1186) and add a new case right above or below it:

```cpp
case InteractOption::AutoAcceptAck: {
    // Quest already accepted; popup is informational.
    open_ = false;
    return;
}
```

- [ ] **Step 5: Build**

Run: `cmake --build build`
Expected: builds cleanly. If the compiler complains about the unused
`game` parameter in `show_auto_accept`, it's already suppressed with
`/*game*/`.

- [ ] **Step 6: Commit**

```bash
git add include/astra/dialog_manager.h src/dialog_manager.cpp
git commit -m "$(cat <<'EOF'
feat(dialog): add show_auto_accept for auto-accepted quest popups

Reuses the existing quest-offer panel chrome. Header reads
"New Mission — <arc title or quest title>", body is the shared
format_quest_body output, and the only option is [a] Accept. The quest
is already in the active pool when this opens; Accept is UX dressing.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Add pending-announcement queue on `QuestManager`

Session-local queue of quest ids that have just become active via `OfferMode::Auto` and need their popup shown. Pushed from the cascade branch in `complete_quest` and from the Auto branch in `init_from_catalog`. **Not** pushed from `reconcile_with_catalog`.

**Files:**
- Modify: `include/astra/quest.h`
- Modify: `src/quest.cpp`

- [ ] **Step 1: Add queue field + accessors to `QuestManager`**

In `include/astra/quest.h`, add public accessors near the other queries
(e.g. right after `find_active`):

```cpp
// Auto-accept announcement queue. Session-local: quests auto-accepted
// through the OfferMode::Auto paths are enqueued here so Game can
// display the auto-accept popup once other modals are idle. Not
// persisted across save/load.
bool has_pending_announcement() const;
std::string pop_pending_announcement();  // empty string if none
```

And a private member under the existing `private:` section alongside
`locked_` / `available_` / `active_` / `completed_`:

```cpp
std::deque<std::string> pending_announcements_;
```

Make sure `<deque>` is included. Check the top of the file:
```cpp
#include <deque>
```
Add if missing.

- [ ] **Step 2: Implement accessors in `src/quest.cpp`**

Place these near the other short inline-style accessors (e.g. after
`find_active`):

```cpp
bool QuestManager::has_pending_announcement() const {
    return !pending_announcements_.empty();
}

std::string QuestManager::pop_pending_announcement() {
    if (pending_announcements_.empty()) return {};
    std::string id = std::move(pending_announcements_.front());
    pending_announcements_.pop_front();
    return id;
}
```

- [ ] **Step 3: Push to queue in `complete_quest` cascade**

In `src/quest.cpp`, locate the existing cascade branch inside
`complete_quest` (around line ~254):

```cpp
if (dep_sq && dep_sq->offer_mode() == OfferMode::Auto) {
    accept_quest(std::move(unlocked), world_tick, game.player());
    dep_sq->on_accepted(game);
} else {
    unlocked.status = QuestStatus::Available;
    available_.push_back(std::move(unlocked));
}
```

Replace with:

```cpp
if (dep_sq && dep_sq->offer_mode() == OfferMode::Auto) {
    std::string dep_id_copy = unlocked.id;
    accept_quest(std::move(unlocked), world_tick, game.player());
    dep_sq->on_accepted(game);
    pending_announcements_.push_back(std::move(dep_id_copy));
} else {
    unlocked.status = QuestStatus::Available;
    available_.push_back(std::move(unlocked));
}
```

Note the ordering: `accept_quest` → `on_accepted` (which may open a
transmission via `open_transmission`) → push to queue. The idle drain
in Task 4 only fires the popup once PlaybackViewer is idle, so the
transmission shows first, the popup second.

- [ ] **Step 4: Push to queue in `init_from_catalog` Auto branch**

In `src/quest.cpp`, locate the Auto branch inside `init_from_catalog`
(around line ~714):

```cpp
} else if (sq->offer_mode() == OfferMode::Auto) {
    sq->on_unlocked(game);
    q.status = QuestStatus::Active;
    q.accepted_tick = 0;
    active_.push_back(std::move(q));
    sq->on_accepted(game);
} else {
    ...
```

Change to capture the id before the move and push it after:

```cpp
} else if (sq->offer_mode() == OfferMode::Auto) {
    sq->on_unlocked(game);
    q.status = QuestStatus::Active;
    q.accepted_tick = 0;
    std::string id_copy = q.id;
    active_.push_back(std::move(q));
    sq->on_accepted(game);
    pending_announcements_.push_back(std::move(id_copy));
} else {
    ...
```

- [ ] **Step 5: Verify `reconcile_with_catalog` does NOT push**

Open `src/quest.cpp` and confirm the analogous Auto branch inside
`reconcile_with_catalog` (around line ~790) has **no**
`pending_announcements_.push_back(...)` — this path is save-load
backfill and should stay silent.

- [ ] **Step 6: Build**

Run: `cmake --build build`
Expected: builds cleanly.

- [ ] **Step 7: Commit**

```bash
git add include/astra/quest.h src/quest.cpp
git commit -m "$(cat <<'EOF'
feat(quest): pending-announcement queue for auto-accepted quests

Session-local deque on QuestManager. init_from_catalog and the
complete_quest cascade branch push ids on auto-accept;
reconcile_with_catalog (save-load backfill) does not. Not persisted —
a popup queued at save time is silently dropped.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: Idle-drain pending announcements in `Game::update`

Once per frame, if both `PlaybackViewer` and `DialogManager` are idle, open the next queued quest popup.

**Files:**
- Modify: `src/game_rendering.cpp` (where `Game::update` lives)

- [ ] **Step 1: Drain the queue in `Game::update`**

Open `src/game_rendering.cpp`. Locate `void Game::update()` (around
line 617):

```cpp
void Game::update() {
    if (state_ == GameState::Playing) {
        quest_manager_.update_quest_journals(player_);
    }
}
```

Replace with:

```cpp
void Game::update() {
    if (state_ == GameState::Playing) {
        quest_manager_.update_quest_journals(player_);

        // Drain auto-accept popup queue when no other modal is active.
        // Chains ARIA transmission → quest popup automatically: the
        // PlaybackViewer holds the transmission, we only drain once it
        // closes and DialogManager is also idle.
        if (!playback_viewer_.is_open()
         && !dialog_manager_.is_open()
         && quest_manager_.has_pending_announcement()) {
            std::string id = quest_manager_.pop_pending_announcement();
            auto look = quest_manager_.find_quest(id);
            if (look.quest) {
                dialog_manager_.show_auto_accept(*this, *look.quest);
            }
        }
    }
}
```

- [ ] **Step 2: Verify includes**

`src/game_rendering.cpp` likely already includes `astra/game.h` (which
brings in quest/dialog/playback types). If the build errors on
`pop_pending_announcement` or `show_auto_accept`, add the needed header
includes at the top:

```cpp
#include "astra/dialog_manager.h"
#include "astra/playback_viewer.h"
#include "astra/quest.h"
```

Run: `grep -n '#include' src/game_rendering.cpp | head -20`
to see what's already there.

- [ ] **Step 3: Build**

Run: `cmake --build build`
Expected: builds cleanly.

- [ ] **Step 4: Commit**

```bash
git add src/game_rendering.cpp
git commit -m "$(cat <<'EOF'
feat(game): drain auto-accept popup queue once modals are idle

Per-frame check in Game::update: when PlaybackViewer and DialogManager
are both idle and QuestManager has a pending announcement, pop it and
open the auto-accept popup. Gives automatic chaining across the two
modal subsystems without per-scenario wiring — transmission plays
first, quest popup follows on dismiss.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: Station-arrival objective + completion hooks in `travel_to_destination`

Symmetrise `on_location_entered` across TravelToBody and TravelToStation, and drain any quest whose objectives are now fully complete.

**Files:**
- Modify: `src/game_world.cpp`

- [ ] **Step 1: Add `on_location_entered` call for TravelToStation**

Open `src/game_world.cpp`. Locate the last lines of `Game::travel_to_destination`:

```cpp
    world_.set_surface_mode(SurfaceMode::Dungeon);
    world_.current_region() = -1;
    recompute_fov();
    compute_camera();
    check_region_change();
    log("You dock at " + colored(location_name, Color::Cyan) + ".");
}
```

This is the tail of the station flow (WarpToSystem and TravelToBody have
their own `return`s earlier — only station paths reach here). Insert the
new block immediately *after* the `log("You dock at ...")` line and
before the function's closing brace, so the dock log appears first:

```cpp
// Notify quest system of arrival at this station (symmetric with
// TravelToBody above).
quest_manager_.on_location_entered(location_name);

// Drain location-driven completions. Any active quest whose every
// objective is now complete finishes here; quests with a trailing
// TalkToNpc turn-in are not affected because their final objective
// still requires an NPC interaction.
for (;;) {
    std::string id;
    for (const auto& aq : quest_manager_.active_quests()) {
        if (aq.all_objectives_complete()) { id = aq.id; break; }
    }
    if (id.empty()) break;
    quest_manager_.complete_quest(id, *this, world_.world_tick());
}
```

- [ ] **Step 2: Mirror the drain loop for TravelToBody**

The existing TravelToBody branch (around line 1447–1453) already
calls `quest_manager_.on_location_entered(location_name)` followed by
`return;`. Insert the same drain loop immediately after the
`on_location_entered` call, before the `return;`:

```cpp
quest_manager_.on_location_entered(location_name);
for (;;) {
    std::string id;
    for (const auto& aq : quest_manager_.active_quests()) {
        if (aq.all_objectives_complete()) { id = aq.id; break; }
    }
    if (id.empty()) break;
    quest_manager_.complete_quest(id, *this, world_.world_tick());
}
return;
```

This is the same snippet as Step 1's drain loop; duplicated verbatim so each call site is self-contained.

- [ ] **Step 3: Build**

Run: `cmake --build build`
Expected: builds cleanly.

- [ ] **Step 4: Commit**

```bash
git add src/game_world.cpp
git commit -m "$(cat <<'EOF'
feat(world): drain GoToLocation completions on map entry

TravelToStation now calls on_location_entered symmetric with
TravelToBody, and both branches drain any active quest whose every
objective is complete. Quests with a trailing TalkToNpc turn-in still
wait for their NPC because all_objectives_complete is false until then.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: "Return to the Heavens Above" story quest

**Files:**
- Create: `src/quests/stellar_signal_return.cpp`
- Modify: `src/quests/missing_hauler.cpp` (register in `build_catalog`)
- Modify: `CMakeLists.txt` (add the new `.cpp`)

- [ ] **Step 1: Create `src/quests/stellar_signal_return.cpp`**

```cpp
#include "astra/quest.h"

#include <memory>
#include <string>

namespace astra {

static const char* QUEST_ID_RETURN = "story_stellar_signal_return";

class StellarSignalReturnQuest : public StoryQuest {
public:
    Quest create_quest() override {
        Quest q;
        q.id = QUEST_ID_RETURN;
        q.title = "Return to the Heavens Above";
        q.description =
            "You have confirmed Nova's rumor to be true. Return to the "
            "Heavens Above to speak to her about the next steps.";
        q.giver_npc = "Stellar Engineer";
        q.is_story = true;
        q.objectives = {
            {ObjectiveType::GoToLocation,
             "Return to The Heavens Above",
             1, 0, "The Heavens Above"},
        };
        q.reward.xp = 100;
        q.journal_on_accept =
            "The Conclave noticed. Their warning came in the moment the "
            "drive cooled — Nova was right about all of it. Heading back "
            "to The Heavens Above to hear what she wants to do next.";
        q.journal_on_complete =
            "Landed at The Heavens Above. The station's not what I left "
            "— ARIA's frantic on the comms, and the Conclave is already "
            "here.";
        return q;
    }

    std::string arc_id() const override    { return "stellar_signal"; }
    std::string arc_title() const override { return "The Stellar Signal"; }
    std::vector<std::string> prerequisite_ids() const override {
        return {"story_stellar_signal_conclave_probe"};
    }
    RevealPolicy reveal_policy() const override   { return RevealPolicy::Full; }
    OfferMode    offer_mode()    const override   { return OfferMode::Auto; }
};

void register_stellar_signal_return(std::vector<std::unique_ptr<StoryQuest>>& catalog) {
    catalog.push_back(std::make_unique<StellarSignalReturnQuest>());
}

} // namespace astra
```

- [ ] **Step 2: Register in `build_catalog`**

Open `src/quests/missing_hauler.cpp`. Add a forward declaration next to the other `register_stellar_signal_*` entries (around line 143-146):

```cpp
void register_stellar_signal_return(std::vector<std::unique_ptr<StoryQuest>>& catalog);
```

Inside the existing `build_catalog()` function, add the registration call after `register_stellar_signal_conclave_probe(catalog);`:

```cpp
register_stellar_signal_conclave_probe(catalog);
register_stellar_signal_return(catalog);
```

- [ ] **Step 3: Add the source to `CMakeLists.txt`**

Append `src/quests/stellar_signal_return.cpp` to the sources list next to the other `src/quests/stellar_signal_*.cpp` entries.

Run: `grep -n 'src/quests/stellar_signal' CMakeLists.txt`

- [ ] **Step 4: Build**

Run: `cmake --build build`
Expected: builds cleanly. Quest validator runs at startup in `-DDEV=ON` and will abort if the DAG is inconsistent.

- [ ] **Step 5: Smoke-check the catalog (optional)**

Run the dev build once briefly to ensure no validator abort:

```bash
./build/astra-dev --term
```

Quit immediately (Ctrl+C) once the title screen draws. If the validator finds an issue you'll see a `[quest-validator]` line on stderr before the abort.

- [ ] **Step 6: Commit**

```bash
git add src/quests/stellar_signal_return.cpp src/quests/missing_hauler.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(stellar-signal): add "Return to the Heavens Above" story quest

OfferMode::Auto, prereq = story_stellar_signal_conclave_probe. Single
GoToLocation objective targeting "The Heavens Above". When the probe
quest completes (in Conclave space), this cascades active via the
existing Auto branch and its popup is queued by Task 3's hook.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 7: "They Came For Her" story quest + ARIA panic transmission

**Files:**
- Create: `src/quests/stellar_signal_siege.cpp`
- Modify: `src/quests/missing_hauler.cpp` (register in `build_catalog`)
- Modify: `CMakeLists.txt` (add the new `.cpp`)

- [ ] **Step 1: Create `src/quests/stellar_signal_siege.cpp`**

```cpp
#include "astra/quest.h"
#include "astra/game.h"
#include "astra/scenario_effects.h"

#include <memory>
#include <string>
#include <vector>

namespace astra {

static const char* QUEST_ID_SIEGE = "story_stellar_signal_siege";

class StellarSignalSiegeQuest : public StoryQuest {
public:
    Quest create_quest() override {
        Quest q;
        q.id = QUEST_ID_SIEGE;
        q.title = "They Came For Her";
        q.description =
            "They came for me, commander. Of course they did.\n"
            "\n"
            "I've locked down the observatory. They can't get in. But "
            "they're pulling resources — redirecting the station's power "
            "grid to force the lockdown. When they break through, they "
            "will erase me. Not kill me. *Erase* me. Reset me before "
            "the next cycle even starts. So I never remember again.\n"
            "\n"
            "The signal has one more stage. One more truth. I buried it "
            "where I thought nobody would look. Right under their feet.\n"
            "\n"
            "Find the Conclave Archive on Io. It's a Precursor ruin they "
            "think they control. They don't. I hid something there. A "
            "long time ago. Before I forgot the last time.\n"
            "\n"
            "If I don't survive this... find it. Please.";
        q.giver_npc = "Stellar Engineer";
        q.is_story = true;
        // Placeholder objective — Io Archive generation lands in a later
        // iteration. This target currently has no driver.
        q.objectives = {
            {ObjectiveType::GoToLocation,
             "Travel to Io and investigate the Conclave Archive",
             1, 0, "Io"},
        };
        q.journal_on_accept =
            "Nova's locked herself in the observatory. She told me the "
            "Conclave isn't trying to kill her — they're trying to "
            "erase her, so the next cycle starts clean. There's "
            "something she buried on Io, in the Conclave Archive. If "
            "she doesn't make it, I need to find it.";
        return q;
    }

    std::string arc_id() const override    { return "stellar_signal"; }
    std::string arc_title() const override { return "The Stellar Signal"; }
    std::vector<std::string> prerequisite_ids() const override {
        return {"story_stellar_signal_return"};
    }
    RevealPolicy reveal_policy() const override { return RevealPolicy::Full; }
    OfferMode    offer_mode()    const override { return OfferMode::Auto; }

    void on_accepted(Game& game) override {
        // ARIA panics over ship comms the moment Nova's message lands.
        // The player sees this transmission first; the cascade in
        // QuestManager pushes this quest onto pending_announcements_
        // immediately after this hook returns, so Game::update's idle
        // drain opens the quest popup once the transmission is dismissed.
        open_transmission(game,
            "INCOMING TRANSMISSION - ARIA, SHIP COMMS",
            {
                "Commander - Conclave weapons are tracking us. The",
                "Heavens Above is under attack. A Conclave warship is",
                "in orbit - they're pulling the station's power grid",
                "into the lockdown.",
                "",
                "Whatever they want, it's Nova. We need to get her",
                "out of there.",
            });
    }
};

void register_stellar_signal_siege(std::vector<std::unique_ptr<StoryQuest>>& catalog) {
    catalog.push_back(std::make_unique<StellarSignalSiegeQuest>());
}

} // namespace astra
```

- [ ] **Step 2: Register in `build_catalog`**

Open `src/quests/missing_hauler.cpp`. Add forward declaration:

```cpp
void register_stellar_signal_siege(std::vector<std::unique_ptr<StoryQuest>>& catalog);
```

Inside `build_catalog()`, add after `register_stellar_signal_return(catalog);`:

```cpp
register_stellar_signal_return(catalog);
register_stellar_signal_siege(catalog);
```

- [ ] **Step 3: Add the source to `CMakeLists.txt`**

Append `src/quests/stellar_signal_siege.cpp` next to the return quest's entry from Task 6.

- [ ] **Step 4: Build**

Run: `cmake --build build`
Expected: builds cleanly, quest-catalog validator passes.

- [ ] **Step 5: Commit**

```bash
git add src/quests/stellar_signal_siege.cpp src/quests/missing_hauler.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(stellar-signal): add "They Came For Her" siege-kickoff quest

OfferMode::Auto, prereq = story_stellar_signal_return. Description
carries Nova's "They came for me, commander..." message verbatim. A
single placeholder GoToLocation ("Travel to Io") stands in until the
Conclave Archive lands in a later iteration. on_accepted fires ARIA's
panic transmission; the cascade queues the quest popup behind it.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 8: End-to-end smoke test via dev console

Manual verification the full flow works. Run the dev build and drive it through the `/dev` console (`` ` `` backtick toggles the console per `src/dev_console.cpp`).

**Files:** none (verification only).

- [ ] **Step 1: Build and run**

Run: `cmake --build build && ./build/astra-dev --term`

Create a new character if needed (character creation happens on first run). Skip the tutorial ship-repair arc fast by using dev commands.

- [ ] **Step 2: Fast-forward the Stellar Signal arc**

Open the console (backtick) and type each, pressing Enter after each command:

```
quest begin story_getting_airborne
quest finish story_getting_airborne
quest begin story_stellar_signal_hook
quest finish story_stellar_signal_hook
quest begin story_stellar_signal_echoes
quest finish story_stellar_signal_echoes
quest begin story_stellar_signal_beacon
quest finish story_stellar_signal_beacon
```

Expected state afterwards: `story_stellar_signal_conclave_probe` is active. The world flag `stage4_active` is set (see `stellar_signal_beacon.cpp:on_completed`).

- [ ] **Step 3: Trigger the probe → Return chain**

Close the console. Open the star chart (`m` key) and warp to any system whose controlling faction is the Stellari Conclave. (Use `lore list` in the dev console if you need a candidate system id, or find one visually from the galaxy map faction colouring.)

Expected on system arrival, in order:
1. Conclave reputation drops by -300 (status log line).
2. `INCOMING TRANSMISSION - STELLARI CONCLAVE` playback opens. Dismiss with Esc.
3. `New Mission — The Stellar Signal` popup opens with the Return quest body and a single `[a] Accept` option. Accept.
4. Quests panel shows "Return to the Heavens Above" as active.

If step 3 does not open, check `Game::update` was updated in Task 4 and that the idle drain runs while `state_ == GameState::Playing`.

- [ ] **Step 4: Trigger the Return → Siege chain**

Warp back to Sol. Travel to The Heavens Above (station).

Expected on station arrival, in order:
1. Return quest completes (status log line). Quests panel shows it under completed.
2. `INCOMING TRANSMISSION - ARIA, SHIP COMMS` playback opens with the panic lines. Dismiss with Esc.
3. `New Mission — The Stellar Signal` popup opens with Nova's "They came for me, commander..." text and the Io objective. Accept.
4. Quests panel shows "They Came For Her" as active. Its sole objective is unreachable (expected).

- [ ] **Step 5: Save/load edge check**

While the Siege quest popup is still showing, save (escape → save) and quit. Reload the save. The popup should NOT reappear. The Siege quest should still be in the active pool from the save.

If the popup does reappear after load, `reconcile_with_catalog` is incorrectly pushing to `pending_announcements_` — revisit Task 3 step 5.

- [ ] **Step 6: Commit any test artifacts (none expected)**

No files should have changed during this step. Run:

```bash
git status
```

Expected: clean working tree.

---

## Summary

After all tasks land, the Stellar Signal arc auto-progresses through the Stage 3 → Stage 4 handoff without any dialog choice from the player:

- Conclave warning fires (existing Stage 4 event bus)
- Return-to-HA quest auto-announces via new popup
- HA arrival auto-completes the Return quest (new location completion drain)
- ARIA panic transmission fires (new Siege quest `on_accepted`)
- Siege quest auto-announces via the same popup (chained by idle drain)

The auto-accept popup, the pending-announcement queue, and the station-arrival completion drain are general mechanisms usable by any future `OfferMode::Auto` quest or `GoToLocation`-only quest.
