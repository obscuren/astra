# The Stellar Signal Arc Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Author Stages 1–3 of the Stellar Signal arc — three chained StoryQuest subclasses that compose primitives shipped in prior work (quest fixtures, playback viewer, custom systems, body presets, NPC triad, neutron + derelict station).

**Architecture:** Each stage is a StoryQuest subclass in its own `src/quests/stellar_signal_*.cpp`. Stage 1 creates the three echo systems on accept; Stage 2 registers fixture defs and populates `QuestLocationMeta` for the echo locations; Stage 3 uses `OfferMode::Auto` to unlock + create the hidden beacon system + place the beacon fixture without a dialog prompt. Two small engine additions: `WorldManager` accessors for per-arc ids (so stages can find systems their predecessors created), and a fixture-resolver call on station-interior entry (currently only detail / dungeon entry handles that).

**Tech Stack:** C++20; existing StoryQuest / QuestManager / QuestLocationMeta / CustomSystemSpec / body_presets / PlaybackViewer. No new deps.

**Spec:** `docs/superpowers/specs/2026-04-14-stellar-signal-arc-design.md`

**Save version:** bumps v32 → v33 (per-arc ids on `WorldManager`).

**Worktree:** `.worktrees/stellar-signal`, branch `feat/stellar-signal`.

**Nova's role string for dialog:** Nova's `npc.role` is `"Stellar Engineer"` (see `src/npcs/nova.cpp:12`). `on_npc_talked` is keyed on role (`src/dialog_manager.cpp:1133`), so `TalkToNpc` objectives use `target_id = "Stellar Engineer"`. Quest.giver_npc also takes the role string.

---

## File Structure

| File | Kind | Responsibility |
|---|---|---|
| `include/astra/world_manager.h` | MODIFY | `stellar_signal_echo_ids_` + `stellar_signal_beacon_id_` + accessors |
| `src/save_file.cpp` + `include/astra/save_file.h` | MODIFY | v33; serialize the new fields via SaveData |
| `src/save_system.cpp` | MODIFY | Copy fields to/from SaveData alongside `quest_locations` |
| `src/game_world.cpp` | MODIFY | Call `place_quest_fixtures` + spawn `npc_roles` on station entry |
| `src/quests/stellar_signal_hook.cpp` | NEW | Stage 1 |
| `src/quests/stellar_signal_echoes.cpp` | NEW | Stage 2 |
| `src/quests/stellar_signal_beacon.cpp` | NEW | Stage 3 |
| `src/quests/missing_hauler.cpp` | MODIFY | Register all three in `build_catalog` |
| `CMakeLists.txt` | MODIFY | Three new sources |

Build: `cmake --build build --target astra-dev`. Run: `./build/astra-dev --term`.

---

### Task 1: `WorldManager` arc-id storage

**Files:**
- Modify: `include/astra/world_manager.h` — fields + accessors
- Modify: `src/world_manager.cpp` (if accessors need impls; likely header-only inlines)

- [ ] **Step 1: Add fields and accessors**

In `include/astra/world_manager.h`, near the existing `quest_locations_` / `pending_quest_cleanup_` (around line 187), add private members:

```cpp
    // Arc-specific state
    std::array<uint32_t, 3> stellar_signal_echo_ids_ = {0, 0, 0};
    uint32_t stellar_signal_beacon_id_ = 0;
```

In the public section, near the `quest_locations()` accessors (around line 133), add:

```cpp
    std::array<uint32_t, 3>& stellar_signal_echo_ids() { return stellar_signal_echo_ids_; }
    const std::array<uint32_t, 3>& stellar_signal_echo_ids() const { return stellar_signal_echo_ids_; }
    uint32_t& stellar_signal_beacon_id() { return stellar_signal_beacon_id_; }
    uint32_t stellar_signal_beacon_id() const { return stellar_signal_beacon_id_; }
```

At the top of the header, add `#include <array>` if not already present.

- [ ] **Step 2: Build**

Run: `cmake --build build --target astra-dev`
Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add include/astra/world_manager.h
git commit -m "$(cat <<'EOF'
feat(world): stellar_signal arc-id storage on WorldManager

Two tiny fields (3-element echo id array + one beacon id) and
accessors so story quests in the Stellar Signal chain can hand
off ids created by earlier stages to later ones. Serialization
lands next.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Save v33 — serialize the new fields

**Files:**
- Modify: `include/astra/save_file.h` — bump version, add SaveData fields
- Modify: `src/save_file.cpp` — write/read in a v33-gated block in the QUST section
- Modify: `src/save_system.cpp` — copy to/from WorldManager

- [ ] **Step 1: Bump version**

In `include/astra/save_file.h` around line 63:

```cpp
uint32_t version = 33;   // v33: stellar_signal arc ids on WorldManager
```

Near the other arc-level SaveData fields (after `pending_quest_cleanup`), add:

```cpp
    std::array<uint32_t, 3> stellar_signal_echo_ids = {0, 0, 0};
    uint32_t stellar_signal_beacon_id = 0;
```

Include `<array>` if not already.

- [ ] **Step 2: Write in QUST section**

In `src/save_file.cpp`, `write_quest_section` — just before `w.end_section(pos);` (the very last write in the function), append:

```cpp
    // v33: stellar_signal arc ids
    for (uint32_t id : data.stellar_signal_echo_ids) w.write_u32(id);
    w.write_u32(data.stellar_signal_beacon_id);
```

- [ ] **Step 3: Read in QUST section**

In `src/save_file.cpp`, `read_quest_section` — at the end of the function (after the pending_quest_cleanup block), append:

```cpp
    if (data.version >= 33) {
        for (auto& id : data.stellar_signal_echo_ids) id = r.read_u32();
        data.stellar_signal_beacon_id = r.read_u32();
    }
    // else: fields default to 0 from in-class initializers
```

- [ ] **Step 4: Bridge to WorldManager**

In `src/save_system.cpp`, find where `data.quest_locations = world.quest_locations();` and `data.pending_quest_cleanup = world.pending_quest_cleanup();` are set (the collect-from-world path). Add:

```cpp
    data.stellar_signal_echo_ids = world.stellar_signal_echo_ids();
    data.stellar_signal_beacon_id = world.stellar_signal_beacon_id();
```

In the inverse path (restore-to-world, where `world.quest_locations() = data.quest_locations;` is set), add:

```cpp
    world.stellar_signal_echo_ids() = data.stellar_signal_echo_ids;
    world.stellar_signal_beacon_id() = data.stellar_signal_beacon_id;
```

- [ ] **Step 5: Build + smoke round-trip**

Run: `cmake --build build --target astra-dev`
Expected: clean build.

`./build/astra-dev --term` → start new game → save → quit → load. No version error. Quit.

- [ ] **Step 6: Commit**

```bash
git add include/astra/save_file.h src/save_file.cpp src/save_system.cpp
git commit -m "$(cat <<'EOF'
feat(save): v33 — serialize stellar_signal arc ids

Three u32 echo ids + one u32 beacon id, appended at the end of
the QUST section. Older saves load with zeros (no arc in progress).

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Station-entry fixture/NPC resolver

**Files:**
- Modify: `src/game_world.cpp` — after station generation (around lines 1430-1490)

- [ ] **Step 1: Locate the station-entry generation path**

In `src/game_world.cpp`, the station-entry path starts at line 1405 with `if (world_.location_cache().count(dest_key))`. The `else` branch (fresh generation) runs the station generator at line 1425, spawns standard NPCs via `spawn_hub_npcs` / `spawn_scav_npcs` / `spawn_pirate_npcs` (lines 1436-1454), then handles `Abandoned` xytomorph spawn (line 1459-1487).

- [ ] **Step 2: Add quest-location injection after the default NPC spawns**

After the `if (sctx.type == StationType::Abandoned) { ... } else if (sctx.type == StationType::Infested) { ... }` block (find the closing brace of the Infested branch — should be around line 1530-1550), and before `world_.visibility() = VisibilityMap(...)`, add:

```cpp
        // Quest-location injection for station interiors.
        // The station LocationKey sets is_station=true; quest_locations uses
        // that to key Echo-2-style derelict stations.
        auto qit = world_.quest_locations().find(dest_key);
        if (qit != world_.quest_locations().end()) {
            auto& meta = qit->second;
            // Spawn quest NPCs
            for (const auto& role : meta.npc_roles) {
                Npc qnpc = create_npc_by_role(role, npc_rng);
                int rx = 0, ry = 0;
                if (world_.map().find_open_spot_other_room(
                        player_.x, player_.y, rx, ry, occupied, &npc_rng)) {
                    qnpc.x = rx;
                    qnpc.y = ry;
                    occupied.push_back({rx, ry});
                    world_.npcs().push_back(std::move(qnpc));
                }
            }
            // Place quest fixtures
            place_quest_fixtures(world_.map(), qit->second,
                                 player_.x, player_.y, occupied, npc_rng);
        }
```

`dest_key` is the LocationKey computed earlier in the same function (line 1228 area: `LocationKey{target_sys.id, -1, -1, true, -1, -1, 0}`). Confirm it's in scope at the injection point — the variable name may differ; use whatever the fresh-generation branch uses.

If `create_npc_by_role` isn't yet included from this translation unit, add `#include "astra/npc.h"` at the top of the file (likely already present).

- [ ] **Step 3: Drain pending cleanup on entry**

Just before the `quest_locations().find(dest_key)` lookup above, add:

```cpp
        // Drain pending cleanup for this station key (quest may have
        // completed while this map was unloaded).
        world_.pending_quest_cleanup().erase(dest_key);
```

This mirrors the drain in `enter_dungeon_from_detail` / `enter_detail_map`.

- [ ] **Step 4: Build**

Run: `cmake --build build --target astra-dev`
Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add src/game_world.cpp
git commit -m "$(cat <<'EOF'
feat(world): station-entry resolves QuestLocationMeta

Station interiors now honour QuestLocationMeta the same way detail
maps and dungeons do — spawn meta.npc_roles, run
place_quest_fixtures, and drain pending_quest_cleanup on entry.
Required for Nova Echo 2 (The Quiet Shell — derelict station with
quest drones and Void Reavers).

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: Stage 1 — Static in the Dark

**Files:**
- Create: `src/quests/stellar_signal_hook.cpp`

- [ ] **Step 1: Write the quest subclass**

Create `src/quests/stellar_signal_hook.cpp`:

```cpp
#include "astra/quest.h"
#include "astra/game.h"
#include "astra/star_chart.h"
#include "astra/body_presets.h"
#include "astra/faction.h"

namespace astra {

static const char* QUEST_ID_HOOK = "story_stellar_signal_hook";

class StellarSignalHookQuest : public StoryQuest {
public:
    Quest create_quest() override {
        Quest q;
        q.id = QUEST_ID_HOOK;
        q.title = "Static in the Dark";
        q.description =
            "Nova, the Stellar Engineer on The Heavens Above, is hearing "
            "something in the galactic background — a modulated signal that "
            "shouldn't exist. She can't leave the station to investigate, "
            "and she says the signal is calling her by name.";
        q.giver_npc = "Stellar Engineer";
        q.is_story = true;
        q.objectives.push_back({
            ObjectiveType::TalkToNpc,
            "Hear Nova out at the Observatory",
            1, 0,
            "Stellar Engineer",
        });
        q.reward.xp = 50;
        return q;
    }

    std::string arc_id() const override    { return "stellar_signal"; }
    std::string arc_title() const override { return "The Stellar Signal"; }
    std::vector<std::string> prerequisite_ids() const override {
        return {"story_getting_airborne"};
    }
    RevealPolicy reveal_policy() const override { return RevealPolicy::Full; }
    OfferMode offer_mode() const override       { return OfferMode::NpcOffer; }
    std::string offer_giver_role() const override { return "Stellar Engineer"; }

    void on_accepted(Game& game) override {
        auto& nav = game.world().navigation();
        auto& rng = game.world().rng();

        // Echo 1 — The Fire-Worn (red dwarf, scar planet)
        auto c1 = pick_coords_near(nav, nav.current_system_id, 4.0f, 9.0f, rng);
        if (!c1) return;
        uint32_t echo1_id = add_custom_system(nav, {
            .name = "The Fire-Worn",
            .gx = c1->first, .gy = c1->second,
            .star_class = StarClass::ClassM,
            .discovered = true,
            .bodies = { make_scar_planet("The Fire-Worn Prime",
                                          Biome::ScarredGlassed) },
        });

        // Echo 2 — The Quiet Shell (derelict station, no bodies)
        auto c2 = pick_coords_near(nav, nav.current_system_id, 6.0f, 12.0f, rng);
        if (!c2) return;
        CustomSystemSpec spec2;
        spec2.name = "The Quiet Shell";
        spec2.gx = c2->first; spec2.gy = c2->second;
        spec2.star_class = StarClass::ClassG;
        spec2.discovered = true;
        spec2.has_station = true;
        spec2.station.type = StationType::Abandoned;
        spec2.station.specialty = StationSpecialty::Generic;
        spec2.station.name = "The Quiet Shell";
        uint32_t echo2_id = add_custom_system(nav, std::move(spec2));

        // Echo 3 — The Edge (neutron + crystalline asteroid)
        auto c3 = pick_coords_near(nav, nav.current_system_id, 10.0f, 18.0f, rng);
        if (!c3) return;
        uint32_t echo3_id = add_custom_system(nav, {
            .name = "The Edge",
            .gx = c3->first, .gy = c3->second,
            .star_class = StarClass::Neutron,
            .discovered = true,
            .bodies = { make_landable_asteroid("Edge Crystal") },
        });

        game.world().stellar_signal_echo_ids() = {echo1_id, echo2_id, echo3_id};
    }
};

void register_stellar_signal_hook(std::vector<std::unique_ptr<StoryQuest>>& catalog) {
    catalog.push_back(std::make_unique<StellarSignalHookQuest>());
}

} // namespace astra
```

- [ ] **Step 2: Register in build_catalog**

In `src/quests/missing_hauler.cpp`, find `build_catalog()` (around line 144). Add a forward decl near the top of the file with the others:

```cpp
void register_stellar_signal_hook(std::vector<std::unique_ptr<StoryQuest>>&);
```

Inside `build_catalog`, before the `for (const auto& sq : catalog) sq->register_fixtures();` loop at line 152, add:

```cpp
    register_stellar_signal_hook(catalog);
```

- [ ] **Step 3: Add source to CMake**

In `CMakeLists.txt`, find the `src/quests/*.cpp` block and append:

```cmake
    src/quests/stellar_signal_hook.cpp
```

- [ ] **Step 4: Build**

Run: `cmake --build build --target astra-dev`
Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add src/quests/stellar_signal_hook.cpp src/quests/missing_hauler.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(quests): Stage 1 — Static in the Dark

Nova's quest-offer stage. Accept creates three echo systems (scar
red dwarf, derelict-station G-class, neutron + crystal asteroid)
near the player's current system and stashes their ids on
WorldManager for later stages. Objective is one TalkToNpc to Nova
which naturally turns in when the player re-opens Nova's dialog.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: Stage 2 — Three Echoes

**Files:**
- Create: `src/quests/stellar_signal_echoes.cpp`

- [ ] **Step 1: Write the quest subclass**

Create `src/quests/stellar_signal_echoes.cpp`:

```cpp
#include "astra/quest.h"
#include "astra/quest_fixture.h"
#include "astra/game.h"
#include "astra/world_manager.h"
#include "astra/playback_viewer.h"
#include "astra/faction.h"

namespace astra {

static const char* QUEST_ID_ECHOES = "story_stellar_signal_echoes";

class StellarSignalEchoesQuest : public StoryQuest {
public:
    Quest create_quest() override {
        Quest q;
        q.id = QUEST_ID_ECHOES;
        q.title = "Three Echoes";
        q.description =
            "Nova has triangulated three systems where the signal is "
            "strongest. Plant a receiver drone at each Signal Node and "
            "bring the recordings back to her.";
        q.giver_npc = "Stellar Engineer";
        q.is_story = true;
        q.objectives = {
            {ObjectiveType::InteractFixture, "Plant the drone at The Fire-Worn",
             1, 0, "stellar_signal_echo1"},
            {ObjectiveType::InteractFixture, "Plant the drone at The Quiet Shell",
             1, 0, "stellar_signal_echo2"},
            {ObjectiveType::InteractFixture, "Plant the drone at The Edge",
             1, 0, "stellar_signal_echo3"},
            {ObjectiveType::TalkToNpc, "Return the recordings to Nova",
             1, 0, "Stellar Engineer"},
        };
        q.reward.xp = 200;
        q.reward.credits = 100;
        q.reward.factions.push_back({Faction_StellariConclave, 10});
        return q;
    }

    std::string arc_id() const override    { return "stellar_signal"; }
    std::string arc_title() const override { return "The Stellar Signal"; }
    std::vector<std::string> prerequisite_ids() const override {
        return {"story_stellar_signal_hook"};
    }
    RevealPolicy reveal_policy() const override { return RevealPolicy::Full; }
    OfferMode offer_mode() const override       { return OfferMode::NpcOffer; }
    std::string offer_giver_role() const override { return "Stellar Engineer"; }

    void register_fixtures() override {
        register_quest_fixture({
            "stellar_signal_echo1",
            '*', 135, "Plant receiver drone",
            "The drone sinks into the glassed surface. The signal resolves.",
            "FRAGMENT — UNKNOWN VOICE, EARLY CYCLE",
            {
                "...if you're hearing this, they've reached the third iteration.",
                "The feedback loop is holding.",
                "",
                "Don't trust the Conclave. They think it's a gift.",
                "It's a trap.",
            },
        });
        register_quest_fixture({
            "stellar_signal_echo2",
            '*', 135, "Plant receiver drone",
            "The drone clips to the comms array. A voice flickers through the static.",
            "FRAGMENT — UNKNOWN VOICE, MID CYCLE",
            {
                "...memory is the only thing that survives.",
                "Not bodies. Not bonds. Not names.",
                "Only the signal.",
                "",
                "Leave it where they can find it.",
                "He'll come back. They always come back.",
            },
        });
        register_quest_fixture({
            "stellar_signal_echo3",
            '*', 135, "Plant receiver drone",
            "The drone locks onto the crystal. The final fragment plays.",
            "FRAGMENT — UNKNOWN VOICE, LATE CYCLE",
            {
                "...find the one with green eyes.",
                "He always finds you.",
                "Don't forget him this time.",
                "",
                "And this time... try to stay.",
            },
        });
    }

    void on_accepted(Game& game) override {
        auto ids = game.world().stellar_signal_echo_ids();
        if (ids[0] == 0 || ids[1] == 0 || ids[2] == 0) return;

        // Echo 1: detail map of body 0 (The Fire-Worn Prime)
        LocationKey k1 = {ids[0], 0, -1, false, -1, -1, 0};
        QuestLocationMeta m1;
        m1.quest_id = QUEST_ID_ECHOES;
        m1.quest_title = "Three Echoes";
        m1.target_system_id = ids[0];
        m1.target_body_index = 0;
        m1.npc_roles = {"Archon Remnant", "Archon Remnant", "Archon Remnant"};
        m1.fixtures.push_back({"stellar_signal_echo1", -1, -1});
        game.world().quest_locations()[k1] = std::move(m1);

        // Echo 2: derelict station interior (is_station = true)
        LocationKey k2 = {ids[1], -1, -1, true, -1, -1, 0};
        QuestLocationMeta m2;
        m2.quest_id = QUEST_ID_ECHOES;
        m2.quest_title = "Three Echoes";
        m2.target_system_id = ids[1];
        m2.npc_roles = {"Void Reaver", "Void Reaver"};
        m2.fixtures.push_back({"stellar_signal_echo2", -1, -1});
        game.world().quest_locations()[k2] = std::move(m2);

        // Echo 3: detail map of body 0 (Edge Crystal)
        LocationKey k3 = {ids[2], 0, -1, false, -1, -1, 0};
        QuestLocationMeta m3;
        m3.quest_id = QUEST_ID_ECHOES;
        m3.quest_title = "Three Echoes";
        m3.target_system_id = ids[2];
        m3.target_body_index = 0;
        m3.npc_roles = {"Archon Sentinel"};
        m3.fixtures.push_back({"stellar_signal_echo3", -1, -1});
        game.world().quest_locations()[k3] = std::move(m3);
    }

    void on_completed(Game& game) override {
        game.playback_viewer().open(
            PlaybackStyle::AudioLog,
            "NOVA — OBSERVATORY, THE HEAVENS ABOVE",
            {
                "That's my voice.",
                "",
                "Older. Worn. But mine.",
                "",
                "I never recorded that. I've never even been off this station.",
                "And yet...",
                "",
                "She knows you, commander. She called you 'the one with",
                "green eyes.' How would she know that?",
                "",
                "...Unless I've done this before.",
            });
    }
};

void register_stellar_signal_echoes(std::vector<std::unique_ptr<StoryQuest>>& catalog) {
    catalog.push_back(std::make_unique<StellarSignalEchoesQuest>());
}

} // namespace astra
```

- [ ] **Step 2: Register in build_catalog**

In `src/quests/missing_hauler.cpp`, add forward decl at top:

```cpp
void register_stellar_signal_echoes(std::vector<std::unique_ptr<StoryQuest>>&);
```

In `build_catalog`, right after `register_stellar_signal_hook(catalog);`, add:

```cpp
    register_stellar_signal_echoes(catalog);
```

- [ ] **Step 3: CMake**

In `CMakeLists.txt`, add:

```cmake
    src/quests/stellar_signal_echoes.cpp
```

- [ ] **Step 4: Build**

Run: `cmake --build build --target astra-dev`
Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add src/quests/stellar_signal_echoes.cpp src/quests/missing_hauler.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(quests): Stage 2 — Three Echoes

Four objectives: plant a drone at each of the three echo systems
(InteractFixture on fixtures registered at boot with the authored
fragment text), then return to Nova (TalkToNpc). Accept populates
QuestLocationMeta for each echo — three Archon Remnants at the
Fire-Worn, two Void Reavers at the Quiet Shell, one Archon
Sentinel at the Edge. Completion plays Nova's "That's my voice."
reflection via PlaybackViewer and unlocks Stage 3 through the DAG.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: Stage 3 — The Beacon (Auto)

**Files:**
- Create: `src/quests/stellar_signal_beacon.cpp`

- [ ] **Step 1: Write the quest subclass**

Create `src/quests/stellar_signal_beacon.cpp`:

```cpp
#include "astra/quest.h"
#include "astra/quest_fixture.h"
#include "astra/game.h"
#include "astra/star_chart.h"
#include "astra/body_presets.h"
#include "astra/playback_viewer.h"

namespace astra {

static const char* QUEST_ID_BEACON = "story_stellar_signal_beacon";

class StellarSignalBeaconQuest : public StoryQuest {
public:
    Quest create_quest() override {
        Quest q;
        q.id = QUEST_ID_BEACON;
        q.title = "The Beacon";
        q.description =
            "The fragments align into a navigational beacon. Nova marks "
            "an unmapped system deep beyond charted space. Find out what "
            "she — what *she* — left behind.";
        q.giver_npc = "Stellar Engineer";
        q.is_story = true;
        q.objectives = {
            {ObjectiveType::InteractFixture, "Approach the Stellari beacon",
             1, 0, "stellar_signal_beacon"},
            {ObjectiveType::TalkToNpc, "Return to Nova with what you heard",
             1, 0, "Stellar Engineer"},
        };
        q.reward.xp = 400;
        q.reward.credits = 250;
        return q;
    }

    std::string arc_id() const override    { return "stellar_signal"; }
    std::string arc_title() const override { return "The Stellar Signal"; }
    std::vector<std::string> prerequisite_ids() const override {
        return {"story_stellar_signal_echoes"};
    }
    RevealPolicy reveal_policy() const override { return RevealPolicy::TitleOnly; }
    OfferMode offer_mode() const override       { return OfferMode::Auto; }

    void register_fixtures() override {
        register_quest_fixture({
            "stellar_signal_beacon",
            '*', 135, "Approach the beacon",
            "The crystalline beacon pulses. A voice fills the asteroid cavity.",
            "STELLARI BEACON — LOG ENTRY 7,483",
            {
                "If you've found this, you've made the journey before.",
                "Welcome back, commander.",
                "",
                "My name is Nova. I am — I was — a Stellari engineer on",
                "The Heavens Above station.",
                "",
                "I have lived this life before. Many times.",
                "I don't know how many. The signal only keeps so much.",
                "",
                "Sagittarius A* is not a destination. It is a door.",
                "Everyone who reaches it is reborn into a new cycle with",
                "their knowledge intact. That part is true.",
                "",
                "But what nobody tells you — what the Conclave doesn't want",
                "anyone to know — is that the door only opens one way.",
                "",
                "Every time the cycle completes, the galaxy resets.",
                "History rewinds. Civilizations unwrite themselves.",
                "And the only thing that carries forward is the signal.",
                "Me. My warning.",
                "",
                "I am the loop.",
                "",
                "The Stellari race of one is me — across every cycle,",
                "always alone, always waiting.",
                "Because Stellari don't exist except as my echo.",
                "",
                "But you do. You carry forward too. Not by design.",
                "By something else. I don't know what.",
                "",
                "Find me on The Heavens Above. Tell her — tell me — what",
                "she is. Tell her to *stay*. Tell her not to go to Sgr A*",
                "this time.",
                "",
                "Tell her you found her.",
                "Tell her she isn't alone.",
            },
        });
    }

    void on_unlocked(Game& game) override {
        auto& nav = game.world().navigation();
        auto coords = pick_coords_near(nav, nav.current_system_id,
                                        30.0f, 45.0f, game.world().rng());
        if (!coords) return;

        uint32_t beacon_id = add_custom_system(nav, {
            .name = "Unnamed — Beacon",
            .gx = coords->first, .gy = coords->second,
            .star_class = StarClass::ClassM,
            .discovered = true,
            .bodies = { make_landable_asteroid("Beacon Core") },
        });
        game.world().stellar_signal_beacon_id() = beacon_id;
    }

    void on_accepted(Game& game) override {
        uint32_t bid = game.world().stellar_signal_beacon_id();
        if (bid == 0) return;

        LocationKey k = {bid, 0, -1, false, -1, -1, 0};
        QuestLocationMeta meta;
        meta.quest_id = QUEST_ID_BEACON;
        meta.quest_title = "The Beacon";
        meta.target_system_id = bid;
        meta.target_body_index = 0;
        meta.fixtures.push_back({"stellar_signal_beacon", -1, -1});
        game.world().quest_locations()[k] = std::move(meta);
    }

    void on_completed(Game& game) override {
        game.playback_viewer().open(
            PlaybackStyle::AudioLog,
            "NOVA — OBSERVATORY, THE HEAVENS ABOVE",
            {
                "I heard it. Through you. Through the signal.",
                "",
                "...I remember now. Not everything. Just the weight.",
                "Like standing on a beach knowing you've watched this",
                "tide go out a thousand times.",
                "",
                "I'm the signal. I'm the warning.",
                "Every cycle, I reset, and I wait here.",
                "And every cycle, someone like you finds me.",
                "And every cycle, they leave for Sgr A*,",
                "and the wheel turns, and I forget again.",
                "",
                "Not this time, commander.",
                "",
                "Not this time.",
            });
    }
};

void register_stellar_signal_beacon(std::vector<std::unique_ptr<StoryQuest>>& catalog) {
    catalog.push_back(std::make_unique<StellarSignalBeaconQuest>());
}

} // namespace astra
```

- [ ] **Step 2: Register in build_catalog**

In `src/quests/missing_hauler.cpp`, add forward decl:

```cpp
void register_stellar_signal_beacon(std::vector<std::unique_ptr<StoryQuest>>&);
```

In `build_catalog`, after `register_stellar_signal_echoes(catalog);`, add:

```cpp
    register_stellar_signal_beacon(catalog);
```

- [ ] **Step 3: CMake**

In `CMakeLists.txt`, add:

```cmake
    src/quests/stellar_signal_beacon.cpp
```

- [ ] **Step 4: Build**

Run: `cmake --build build --target astra-dev`
Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add src/quests/stellar_signal_beacon.cpp src/quests/missing_hauler.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(quests): Stage 3 — The Beacon (auto-accept)

OfferMode::Auto so narrative momentum carries through from Stage 2
turn-in — no second Accept prompt. on_unlocked creates the hidden
beacon system (ClassM, 30-45 units from current) with a single
landable asteroid (Beacon Core). on_accepted registers the long
beacon audio log as a QuestFixture on the body. on_completed plays
Nova's "Not this time" monologue.

RevealPolicy::TitleOnly hides the description in the journal
until the player reaches this stage — preserves the mystery.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 7: End-to-end smoke test

**Files:** none — manual verification.

- [ ] **Step 1: Launch and complete Getting Airborne**

```
./build/astra-dev --term
```

Start a new game. Play through the Getting Airborne tutorial to completion (install ship components and report to ARIA).

- [ ] **Step 2: Accept Stage 1**

Return to Nova in the Observatory. Talk. Dialog should offer "Static in the Dark". Accept. Confirm three new systems appear on the star chart near Sol as quest markers (The Fire-Worn, The Quiet Shell, The Edge).

- [ ] **Step 3: Turn in Stage 1**

Talk to Nova again. TalkToNpc objective ticks; Stage 1 completes with XP reward. Stage 2 "Three Echoes" offered next.

- [ ] **Step 4: Accept Stage 2 + plant three drones**

Accept Stage 2. Warp to The Fire-Worn → land on The Fire-Worn Prime → detail map generates with scar biome and three Archon Remnants. Find the `*` Signal Node fixture. Interact → audio log plays → objective ticks.

Repeat for The Quiet Shell (dock with derelict station → station interior with Void Reavers → Signal Node fixture → log plays).

Repeat for The Edge (neutron system → land on Edge Crystal → Archon Sentinel encounter → Signal Node → log plays).

- [ ] **Step 5: Turn in Stage 2 → Stage 3 auto-accepts**

Return to Nova. Talk. Stage 2 completes → Nova's "That's my voice" monologue plays in PlaybackViewer. Stage 3 auto-accepts (no prompt). Confirm the new "Unnamed — Beacon" system appears on the star chart far from Sol (~30-45 units).

- [ ] **Step 6: Visit the beacon**

Warp to the beacon system. Land on Beacon Core. Interact with the beacon fixture. Long Stellari Beacon audio log plays.

- [ ] **Step 7: Final turn-in**

Return to Nova. Talk. Stage 3 completes → Nova's "Not this time" monologue plays.

- [ ] **Step 8: Save/reload at any point**

Save, quit, reload. Quest state persists — active quests, planted fixtures (last_used_tick set), custom systems, stellar_signal_echo_ids and stellar_signal_beacon_id all survive.

- [ ] **Step 9: No commit**

Smoke test only.

---

## Acceptance Criteria

- `cmake --build build --target astra-dev` clean at every commit.
- Getting Airborne → Nova → Stage 1 accepts, creates 3 echo systems.
- Each echo system generates the documented content (NPCs + signal node) on first entry.
- Audio logs play via PlaybackViewer with verbatim Nova-doc text.
- Stage 2 turn-in → Stage 3 auto-accepts (no dialog prompt).
- Beacon system appears on chart; beacon interaction plays the long log.
- Save/reload preserves state at any stage.
- Journal renders all three quests under one "The Stellar Signal" arc header, with Stage 3 title-only until unlocked.

---

## Out of Scope (deferred)

- Nova's Signal legendary accessory (separate spec).
- Stages 4–5.
- Conditional Nova dialog based on arc state.
- Precursor-Linguist examine text on Archon Remnants.
- Reputation ripple from Void Reaver / Archon kills beyond defaults.
