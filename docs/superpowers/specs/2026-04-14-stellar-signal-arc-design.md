# The Stellar Signal Arc — Stages 1–3 Design

**Date:** 2026-04-14
**Status:** Draft — not yet implemented
**Companion:** `/Users/jeffrey/dev/Unreal/lyra/nova-arc-the-stellar-signal.md` (authored Nova arc) and `docs/plans/nova-stellar-signal-gap-analysis.md` (systems shipped).

## Summary

Wire Stages 1–3 of the "Stellar Signal" arc — the authored Nova narrative — into the game by composing primitives that have already landed. Three chained `StoryQuest` subclasses grouped under `arc_id = "stellar_signal"`, offered and turned in by Nova in the Observatory on The Heavens Above. The arc plays through a mystery hook (Stage 1), a three-echo investigation (Stage 2), and a revelation at a hidden beacon system (Stage 3).

Stage 3 auto-accepts on Stage 2 turn-in so narrative momentum carries the player forward; a second "Accept" prompt between revelation and follow-through would break the pacing. Signal-node fixtures are declared at quest accept but materialize on first map entry, matching the existing `place_quest_fixtures` resolver.

---

## Goals

- Three chained `StoryQuest` subclasses that consume existing primitives cleanly (no new engine work).
- Journal renders the three stages under one arc header ("The Stellar Signal") — hook full-reveal, echoes full-reveal, beacon title-only until unlocked.
- Nova dialog offers stages 1 and 2 on demand; stage 3 auto-accepts on stage 2 completion.
- Three audio-log fixtures with the authored Nova-doc text, playing via `PlaybackViewer`.
- Three combat encounters matching the Nova doc: Archon Remnants (Echo 1), Void Reavers (Echo 2), Archon Sentinel (Echo 3).
- Reward payloads per stage: XP, credits, and faction reputation. Stage 3 final reward ("Nova's Signal" accessory) is explicitly deferred.

## Non-goals

- "Nova's Signal" legendary accessory — separate spec.
- Stages 4–5 (Conclave siege, endings, NG+).
- Conditional/post-completion Nova dialog variants (uses the standard offer / turn-in flow).
- Precursor-Linguist dead-language examine text.
- System-specific overworld density tuning (encounter numbers are final; overworld uses existing generator output).
- Stage 2's "optional stealth path via ruined maintenance tunnels" from Echo 2.

---

## Arc Structure

All three quests live in `src/quests/` following the `missing_hauler.cpp` + `getting_airborne.cpp` pattern.

### `arc_id` / `arc_title`

```cpp
std::string arc_id() const override    { return "stellar_signal"; }
std::string arc_title() const override { return "The Stellar Signal"; }
```

### DAG

| quest_id | prereqs | offer_mode | offer_giver_role | reveal_policy |
|---|---|---|---|---|
| `story_stellar_signal_hook` | `story_getting_airborne` | NpcOffer | `Stellar Engineer` | Full |
| `story_stellar_signal_echoes` | `story_stellar_signal_hook` | NpcOffer | `Stellar Engineer` | Full |
| `story_stellar_signal_beacon` | `story_stellar_signal_echoes` | Auto | — | TitleOnly |

`Stellar Engineer` is Nova's `npc.role` string (confirmed in `src/npcs/nova.cpp:12`). The dialog system's existing `available_for_role` matches that to Nova automatically.

---

## Stage 1 — Static in the Dark

### Quest shape

```cpp
Quest create_quest() override {
    Quest q;
    q.id = "story_stellar_signal_hook";
    q.title = "Static in the Dark";
    q.description =
        "Nova, the Stellar Engineer on The Heavens Above, is hearing "
        "something in the galactic background — a modulated signal that "
        "shouldn't exist. She can't leave the station to investigate, "
        "and she says the signal is calling her by name.";
    q.giver_npc = "Nova";
    q.is_story = true;
    q.objectives.push_back({
        ObjectiveType::TalkToNpc,
        "Hear Nova out at the Observatory",
        1, 0,
        "Nova",
    });
    q.reward.xp = 50;
    return q;
}
```

### on_accepted

Creates the three echo systems and chart markers. The systems exist from accept-time on so the player can start planning routes; content (NPCs, fixtures) materializes on first entry via Stage 2's `QuestLocationMeta`.

```cpp
auto& nav = game.world().navigation();
auto& rng = game.world().rng();

// Echo 1 — The Fire-Worn (red dwarf, scar planet)
auto c1 = pick_coords_near(nav, nav.current_system_id, 4.0f, 9.0f, rng);
uint32_t echo1_id = add_custom_system(nav, {
    .name = "The Fire-Worn",
    .gx = c1->first, .gy = c1->second,
    .star_class = StarClass::ClassM,
    .discovered = true,
    .bodies = { make_scar_planet("The Fire-Worn Prime", Biome::ScarredGlassed) },
});

// Echo 2 — The Quiet Shell (derelict station)
auto c2 = pick_coords_near(nav, nav.current_system_id, 6.0f, 12.0f, rng);
uint32_t echo2_id = add_custom_system(nav, {
    .name = "The Quiet Shell",
    .gx = c2->first, .gy = c2->second,
    .star_class = StarClass::ClassG,
    .discovered = true,
    .has_station = true,
    .station = { StationType::Abandoned, StationSpecialty::Generic, 0, "The Quiet Shell" },
});

// Echo 3 — The Edge (neutron + crystalline asteroid)
auto c3 = pick_coords_near(nav, nav.current_system_id, 10.0f, 18.0f, rng);
uint32_t echo3_id = add_custom_system(nav, {
    .name = "The Edge",
    .gx = c3->first, .gy = c3->second,
    .star_class = StarClass::Neutron,
    .discovered = true,
    .bodies = { make_landable_asteroid("Edge Crystal") },
});

// Stash ids on the quest/world so Stage 2 can reference them.
game.world().stellar_signal_echo_ids() = {echo1_id, echo2_id, echo3_id};
```

**`stellar_signal_echo_ids()`** is a new accessor on `WorldManager` (a three-element array, default zero) that persists across stages so Stage 2 can look up the systems it needs to populate. Small addition to the save file (12 bytes).

### on_completed

Stage 1's turn-in is built-in via `TalkToNpc` — when the player talks to Nova while Stage 1 is active, the offer-response flow handles acceptance and the objective-complete hook flips the quest to Completed, firing DAG unlock for Stage 2.

No extra `on_completed` logic — reward XP applies automatically.

---

## Stage 2 — Three Echoes

### Quest shape

```cpp
Quest q;
q.id = "story_stellar_signal_echoes";
q.title = "Three Echoes";
q.description =
    "Nova has triangulated three systems where the signal is strongest. "
    "Plant a receiver drone at each Signal Node and bring the recordings "
    "back to her.";
q.giver_npc = "Nova";
q.is_story = true;
q.objectives = {
    {ObjectiveType::InteractFixture, "Plant the drone at The Fire-Worn",
     1, 0, "stellar_signal_echo1"},
    {ObjectiveType::InteractFixture, "Plant the drone at The Quiet Shell",
     1, 0, "stellar_signal_echo2"},
    {ObjectiveType::InteractFixture, "Plant the drone at The Edge",
     1, 0, "stellar_signal_echo3"},
    {ObjectiveType::TalkToNpc, "Return the recordings to Nova",
     1, 0, "Nova"},
};
q.reward.xp = 200;
q.reward.credits = 100;
q.reward.factions.push_back({Faction_StellariConclave, 10});
```

### register_fixtures

Called once at boot by `build_catalog`. Registers all three Signal-Node audio logs. Text lifted from the Nova doc verbatim.

```cpp
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
```

### on_accepted

Populates `QuestLocationMeta` for each echo's landing key. Reads back the ids stashed in Stage 1.

```cpp
auto ids = game.world().stellar_signal_echo_ids();

// Echo 1: detail map of The Fire-Worn Prime (body 0)
LocationKey k1 = {ids[0], 0, -1, false, -1, -1, 0};
QuestLocationMeta m1;
m1.quest_id = "story_stellar_signal_echoes";
m1.quest_title = "Three Echoes";
m1.target_system_id = ids[0];
m1.target_body_index = 0;
m1.npc_roles = {"Archon Remnant", "Archon Remnant", "Archon Remnant"};
m1.fixtures.push_back({"stellar_signal_echo1", -1, -1});
m1.remove_on_completion = false;
game.world().quest_locations()[k1] = std::move(m1);

// Echo 2: derelict station interior
LocationKey k2 = {ids[1], -1, -1, true, -1, -1, 0};    // is_station=true
QuestLocationMeta m2;
m2.quest_id = "story_stellar_signal_echoes";
m2.quest_title = "Three Echoes";
m2.target_system_id = ids[1];
m2.npc_roles = {"Void Reaver", "Void Reaver"};
m2.fixtures.push_back({"stellar_signal_echo2", -1, -1});
m2.remove_on_completion = false;
game.world().quest_locations()[k2] = std::move(m2);

// Echo 3: detail map of Edge Crystal (body 0)
LocationKey k3 = {ids[2], 0, -1, false, -1, -1, 0};
QuestLocationMeta m3;
m3.quest_id = "story_stellar_signal_echoes";
m3.quest_title = "Three Echoes";
m3.target_system_id = ids[2];
m3.target_body_index = 0;
m3.npc_roles = {"Archon Sentinel"};
m3.fixtures.push_back({"stellar_signal_echo3", -1, -1});
m3.remove_on_completion = false;
game.world().quest_locations()[k3] = std::move(m3);
```

**Station-interior LocationKey:** the spec sets `is_station = true` and relies on the existing station-entry hook invoking the meta lookup. This requires `enter_station` (or equivalent) to call the same `place_quest_fixtures` + NPC-spawn path that `enter_dungeon_from_detail` already does. The plan adds that call site if it doesn't exist; currently fixture placement is wired in detail-map and dungeon-map entry, not station entry.

### on_completed

Plays Nova's post-recording reflection via `PlaybackViewer`. Stage 3 unlocks automatically via DAG — and because Stage 3 is `OfferMode::Auto`, it fires `on_unlocked` then `on_accepted` without a dialog prompt.

```cpp
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
```

---

## Stage 3 — The Beacon (Auto)

### Quest shape

```cpp
Quest q;
q.id = "story_stellar_signal_beacon";
q.title = "The Beacon";
q.description =
    "The fragments align into a navigational beacon. Nova marks an "
    "unmapped system deep beyond charted space. Find out what she "
    "— what *she* — left behind.";
q.giver_npc = "Nova";
q.is_story = true;
q.objectives = {
    {ObjectiveType::InteractFixture, "Approach the Stellari beacon",
     1, 0, "stellar_signal_beacon"},
    {ObjectiveType::TalkToNpc, "Return to Nova with what you heard",
     1, 0, "Nova"},
};
q.reward.xp = 400;
q.reward.credits = 250;
```

`reveal_policy() = RevealPolicy::TitleOnly` — the journal shows "The Beacon" as locked after Stage 1 is accepted, with no description, preserving the mystery.

### register_fixtures

One fixture: the beacon itself, carrying the long Stage-3 audio log from the Nova doc.

```cpp
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
```

### on_unlocked

Creates the hidden beacon system far from current position, marks it revealed (so the waypoint shows), and stashes the id.

```cpp
void on_unlocked(Game& game) override {
    auto& nav = game.world().navigation();
    auto coords = pick_coords_near(nav, nav.current_system_id, 30.0f, 45.0f,
                                   game.world().rng());
    if (!coords) return;   // extremely unlikely — caller's problem

    uint32_t beacon_id = add_custom_system(nav, {
        .name = "Unnamed — Beacon",
        .gx = coords->first, .gy = coords->second,
        .star_class = StarClass::ClassM,
        .discovered = true,
        .bodies = { make_landable_asteroid("Beacon Core") },
    });
    game.world().stellar_signal_beacon_id() = beacon_id;
}
```

### on_accepted

Fires right after `on_unlocked` (Auto mode). Populates `QuestLocationMeta` for the beacon body.

```cpp
void on_accepted(Game& game) override {
    uint32_t bid = game.world().stellar_signal_beacon_id();
    if (bid == 0) return;

    LocationKey k = {bid, 0, -1, false, -1, -1, 0};
    QuestLocationMeta meta;
    meta.quest_id = "story_stellar_signal_beacon";
    meta.quest_title = "The Beacon";
    meta.target_system_id = bid;
    meta.target_body_index = 0;
    meta.fixtures.push_back({"stellar_signal_beacon", -1, -1});
    meta.remove_on_completion = false;
    game.world().quest_locations()[k] = std::move(meta);
}
```

### on_completed

Plays Nova's Stage-3 monologue via `PlaybackViewer`. No accessory reward yet (deferred).

```cpp
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
```

---

## Engine touch-ups required

The arc exposes two gaps in the existing scaffolding that the plan must close:

### 1. `WorldManager::stellar_signal_echo_ids` / `stellar_signal_beacon_id`

State storage for arc-specific ids so Stage 2 and Stage 3 can look up systems Stage 1 / Stage 2 created. Added as:

```cpp
// in WorldManager
std::array<uint32_t, 3>& stellar_signal_echo_ids() { return stellar_signal_echo_ids_; }
uint32_t& stellar_signal_beacon_id() { return stellar_signal_beacon_id_; }
```

Serialized in the same section as `quest_locations_` and friends (1 × `u32` + 3 × `u32` = 16 bytes). Save version bumps v32 → v33.

(A future spec can generalize this into a per-quest scratch dictionary; for now the arc-specific accessors are the cheapest shape that works.)

### 2. Station-interior quest-location hook

`QuestLocationMeta.fixtures` currently resolves in `enter_detail_map` and `enter_dungeon_from_detail` (surface and dungeon paths). Echo 2 is a derelict station interior, which uses a different entry path (likely `enter_station` in `game_world.cpp`). The plan adds the same resolver call there so fixture placement and NPC spawns work inside station interiors.

If the station-entry path already handles NPC spawns via `npc_roles`, only the fixture-resolver call needs adding. The plan verifies during implementation.

---

## Save / Load

- New state: `WorldManager::stellar_signal_echo_ids_` and `stellar_signal_beacon_id_`.
- Bump to v33; gate reads on version >= 33; older saves default to zeros (no active arc).
- Everything else rides existing serialization (`QuestManager`, `quest_locations_`, `FixtureData`).

---

## File Map

| File | Kind |
|---|---|
| `src/quests/stellar_signal_hook.cpp` | NEW |
| `src/quests/stellar_signal_echoes.cpp` | NEW |
| `src/quests/stellar_signal_beacon.cpp` | NEW |
| `src/quests/missing_hauler.cpp` | MODIFY — catalog registers all three |
| `include/astra/world_manager.h` | MODIFY — arc-id accessors, private fields |
| `src/save_file.cpp` | MODIFY — serialize new fields, v33 |
| `include/astra/save_file.h` | MODIFY — bump default version |
| `src/game_world.cpp` | MODIFY — station-entry fixture resolver hook (if missing) |
| `CMakeLists.txt` | MODIFY — add three new sources |

---

## Implementation Checklist (for the forthcoming plan)

1. Add `WorldManager::stellar_signal_echo_ids_` and `stellar_signal_beacon_id_` + accessors.
2. Bump save v33; serialize new fields.
3. Create `src/quests/stellar_signal_hook.cpp` with full Stage 1 subclass.
4. Create `src/quests/stellar_signal_echoes.cpp` with full Stage 2 subclass (`register_fixtures`, `on_accepted`, `on_completed`).
5. Create `src/quests/stellar_signal_beacon.cpp` with full Stage 3 subclass (`OfferMode::Auto`, `on_unlocked`, `on_accepted`, `on_completed`).
6. Register all three in `build_catalog()` via `register_stellar_signal_hook/echoes/beacon`.
7. Wire the station-entry path to call `place_quest_fixtures` and spawn `npc_roles` (verify or add).
8. CMake: add the three new sources.
9. Smoke test:
   - Complete Getting Airborne.
   - Talk to Nova → offer "Static in the Dark" → accept.
   - Confirm three echo systems appear as quest markers.
   - Talk to Nova again → turn in Stage 1 → Stage 2 "Three Echoes" offered.
   - Accept Stage 2 → warp to each echo → kill enemies → interact with signal node → audio log plays.
   - All three drones planted → talk to Nova → Stage 2 completes, Nova's reflection plays, Stage 3 **auto-accepts** (no prompt).
   - Beacon system appears on chart → warp → land → interact with beacon → long audio log plays.
   - Talk to Nova → Stage 3 completes with closing monologue.

---

## Out of scope

- Nova's Signal legendary accessory (Stage 3 final reward).
- Arc-aware Nova dialog mutations post-completion.
- Arc failure / retry semantics.
- Reputation cascade on Archon-Remnant / Void-Reaver kills (uses existing per-kill rep if any).
- Any Stage 4/5 groundwork (Conclave siege, endings, NG+, companion Nova).
