# Scenario Graph — Long-Term Vision

**Status:** Vision document. Not a commitment. Written 2026-04-18 as the user and assistant agreed on the shape of in-code narrative orchestration. Future sessions should treat this as an architectural north star, not an implementation plan — promote subsystems only when real content demands it.

**Scope:** How narrative sequencing, multi-step events, cross-cutting scenarios, and branching consequences should work in Astra as the game matures past the Stellar Signal arc. Complements `2026-04-18-stage4-hostility-event-bus.md` (the first concrete use of this architecture).

---

## Core constraint

**Everything stays in C++.** Astra ships as a single standalone binary for the foreseeable future. No YAML, no JSON scripts, no Lua, no hot-reload. Content is authored in C++ and baked into the executable.

This constraint is load-bearing, not a stylistic preference:
- It preserves distribution as one file per platform.
- It keeps refactors sharp — renaming a function cascades through the compiler, not through silent string lookups.
- It forces every scenario through the same type system as the rest of the game, so combat, factions, quests, and items can't desync from narrative content.
- It eliminates the entire class of "the game ships fine but the content pack is broken" bugs.

The door stays open for an *optional* data layer later (e.g. a loader that reads YAML in debug builds, still compiled to C++ constants for release). But the vision doc does **not** anticipate that. If it ever happens it's a separate decision.

---

## Three-layer architecture

The scenario system matures into three distinct layers. Each layer should only depend on the one below it.

### Layer 1 — Event Bus (producers)

A typed, in-process message bus. The only job is: *when something interesting happens in the game, tell anyone who's listening.*

**Events are game-world facts**, not narrative beats:
- `SystemEntered`, `BodyEntered`, `MapGenerated`
- `NpcKilled`, `NpcSpawned`, `PlayerDamaged`, `PlayerLeveledUp`
- `FixtureInteracted`, `ItemPickedUp`, `ItemEquipped`
- `QuestStageCompleted`, `FactionStandingChanged`
- `WorldTick`, `DayTransitioned`, `TurnPassed`
- `DialogChoiceTaken`, `ShopTransactionCompleted`

**Rules:**
- Events are value types (structs inside a `std::variant`). No heap allocation, no reference semantics.
- Producers emit; they do not know who listens. The warp handler emits `SystemEntered`; it does not know about Stellar Signal.
- The bus itself is not serialized. Handlers are re-registered deterministically from world state on save/load.
- Handlers may subscribe/unsubscribe during emit. The bus copies its handler list per emit to make this safe.
- Emit is synchronous. No deferred queues, no async. A handler that needs to fire later should set a `WorldTick` handler with a counter, not enqueue a future event.

**The grower:** every time a new piece of narrative wants to react to something, ask first "is there a relevant event?" If not, add the producer before the scenario. Events are cheap; scenarios reach-around is expensive.

### Layer 2 — Effect primitives (reusable verbs)

Free functions in `scenario_effects.h` that perform a concrete game mutation. They are the **verbs** of the scenario graph.

Early additions (Stage 4 slice):
```
shift_faction_standing(game, faction, delta)
set_world_flag(game, flag, value)
open_transmission(game, header, lines)
inject_location_encounter(game, system, body, station, roles, tag)
```

Likely Phase-4/5 additions:
```
start_timer(game, timer_id, turns, on_expire)
cancel_timer(game, timer_id)
lock_location(game, location_key, reason)
unlock_location(game, location_key)
spawn_boss(game, location_key, npc_role, modifiers)
grant_item(game, item_id, qty)
grant_lore_entry(game, lore_id)
queue_dialog(game, npc, node_id)
play_cinematic(game, cinematic_id)
set_npc_mood(game, npc_role, mood)
reveal_system(game, system_id)
hide_system(game, system_id)
```

**Rules:**
- Each effect is one function, one responsibility. No "run this effect with these sub-effects" wrappers — compose at the scenario level.
- Effects never throw on valid input. They are idempotent where it makes sense (setting a flag twice is fine).
- Effects are thin — they call into the real subsystems (faction.cpp, quest_fixture.cpp, world_manager.cpp). The effect file should not contain gameplay logic.
- Effects must be discoverable. One header. Grouped by category (economy, combat, dialog, world, UI).

**The grower:** the effect vocabulary is the slowest-changing, most shared piece of the architecture. When a scenario is reaching directly into a subsystem (e.g. pushing to `quest_locations` directly), consider lifting it into an effect. When two scenarios do the same mutation, definitely lift.

### Layer 3 — Scenarios (the story glue)

A scenario is a C++ function (or class) that registers handlers on the bus, composes effects, and encodes narrative logic.

```cpp
void register_stage4_hostility_scenario(Game& game) {
    game.event_bus().subscribe(EventKind::SystemEntered,
        [](Game& g, const Event& ev) {
            // read world state, call effects
        });
}
```

**Rules:**
- One file per scenario in `src/scenarios/`.
- A scenario owns its own handler ids and world-flag keys (prefixed with the scenario name).
- Scenarios read world state freely but mutate **only** through effect primitives.
- Scenarios may depend on quests (read quest state), but quests must not depend on scenarios (no circular knowledge).
- Scenarios end themselves. If "Stage 4 is over" means "the stage4 scenario stops listening," the scenario flips its own flag and unsubscribes its handlers.
- Each scenario is testable in isolation conceptually: given a world state and an event, does it produce the expected effect calls? (Astra has no test framework yet — if/when it gets one, scenarios are the right first target.)

**The grower:** scenarios are disposable. It's fine to write a one-off scenario for a single arc and delete it when the arc ends. What accumulates is Layer 1 (events) and Layer 2 (effects). Layer 3 churns.

---

## Scenarios vs. quests

These are **different abstractions**. Don't collapse them.

- **A quest** is a visible player-facing objective with a tracked state machine — accept, progress, complete, reward. It has a title, a journal entry, prerequisites, and lives in the character screen.
- **A scenario** is invisible orchestration — what the world does in response to events. Scenarios can drive quests (start one, fail one), but they themselves are never listed in the UI.

Rough test: if the player should see "Three Echoes ▸ 2/3 drones planted" in their journal, it's a quest. If the player should notice "Conclave just ambushed me again," it's a scenario.

A complex arc is usually *one or more quests* plus *one or more scenarios*. Stellar Signal Stage 4 is: a quest ("Run the Gauntlet"), plus a scenario (Conclave hostility), plus another scenario (the station siege). Stage 5 is a quest (endgame) plus scenarios for each of the three endings.

---

## Scenario catalogue — future vision

What the scenario list might look like at v1.0:

**Stellar Signal arc (story):**
- `stage4_hostility` — the slice this plan implements
- `stage4_siege` — station siege state and lockdown
- `stage4_archon_bloom` — increased Archon Remnant spawning
- `stage5_ending_reset` — Ending A flow (reach Sgr A*, NG+ trigger)
- `stage5_ending_silence` — Ending B flow (destroy beacon, post-cycle epilogue)
- `stage5_ending_save_nova` — Ending C flow (extract core, companion unlock)

**Emergent / non-arc:**
- `faction_war` — dynamic faction vs. faction hostilities based on standings
- `bounty_hunter` — scenario that spawns hunter NPCs when player crosses certain faction thresholds
- `pirate_raid` — periodic raids on friendly stations
- `distress_beacon` — random derelict ship encounters
- `civilization_rise_fall` — galaxy-sim driven events surfacing to the player
- `npc_grudge` — spawned NPCs remember the player if they fled a fight and pursue

**Tutorial / accessibility:**
- `tutorial_first_warp` — hint text on first warp
- `tutorial_first_combat` — combat explainer on first dice roll
- `tutorial_reputation` — shows the Reputation tab after first standing change

The emergent scenarios are where the architecture earns its keep. They're not tied to any single arc; they shape the galaxy's texture.

---

## Save / load model

Handlers do not serialize. On load:

1. `WorldManager` restores flags, ambushed-systems set, timers, faction standings, quest states — all via normal serialization.
2. `Game` clears `event_bus_` and calls `register_all_scenarios(*this)`.
3. Each scenario's registration function is deterministic — given the same world state, the same handlers subscribe.
4. Scenarios may introspect world flags on registration and skip subscribing if they're done (e.g. `if (world.world_flag("stage4_resolved")) return;`).

This keeps the save file decoupled from scenario code. Refactoring the scenario set (adding, removing, renaming) never breaks old saves. The only thing that breaks saves is changing the world-flag *keys* or the *meaning* of existing flags — so treat flag keys like a public API.

---

## Determinism & testing

Scenarios should be deterministic given the same world state and input event sequence. This matters for:

- **Save/load fidelity** — replay must produce the same behavior.
- **Debugging** — when a player reports "the ambush didn't trigger," we can reconstruct from the save.
- **Future tests** — once Astra has a test framework, scenario tests pass a fake `Game` + event stream and assert on effect calls.

To preserve determinism:
- Scenarios read from `Game` and `WorldManager` only. No clocks, no `rand()`.
- RNG flows through `world.rng()` which is seeded from the world.
- Handlers do not capture mutable external state (lambda captures should be empty or `this`-only).

---

## When to escalate beyond this architecture

Don't. Or rather — escalate only when you can point to at least two real pain points caused by the current shape. Specifically:

- If you find scenarios copy-pasting 20+ lines of the same event-handler boilerplate, extract a helper (not a framework).
- If you want conditional branching ("if player chose A, do X, else Y"), use normal C++ `if`. Resist adding a declarative condition type until you've written five scenarios with branching and felt real authoring pain.
- If you want sequences ("wait 50 turns, then do X, then wait for player to enter system Y, then do Z"), build a small `SequencePlayer` utility that takes a vector of step functors. Still C++, still in-process.
- If you want cross-scenario communication, use world flags. Only lift to a proper event if the pattern repeats.
- If you ever genuinely need scripting — mod support, community-authored content, live events — that is the moment to add a data layer. Not before.

The temptation to build a scenario DSL before you have ten real scenarios is the single biggest risk to this architecture. The second biggest is building effects that are too abstract ("do_thing(type, subject, modifier)") instead of specific ("shift_faction_standing(faction, delta)"). Concrete effects compose; abstract effects collapse.

---

## Milestones

The scenario graph matures in three milestones, each triggered by real content need — not by a calendar.

### Milestone M1 — Minimum viable bus (current slice)

**Triggered by:** Stellar Signal Stage 4 hostility.
**Delivers:** EventBus with 3 event types; 4 effect primitives; 1 scenario; save/load for world flags.
**Done when:** the Stage 4 hostility slice ships and passes smoke tests.

### Milestone M2 — Expanded vocabulary

**Triggered by:** Stellar Signal Stage 4 siege + Stage 5 endings landing.
**Delivers:** ~8 more effect primitives (timers, spawns, lore grants, location locks); 2-3 new event types (NpcKilled, WorldTick fan-out, DialogChoiceTaken); 5+ scenarios live.
**Done when:** Stage 5 endings all run through the scenario graph and share ≥2 effect primitives with Stage 4 scenarios.

### Milestone M3 — Emergent scenarios

**Triggered by:** design wants non-arc-driven galaxy events (faction wars, bounty hunters, distress calls).
**Delivers:** Scenarios that run independent of quest state; driven purely by world events + flags. Probably requires a `SequencePlayer` helper and a handful more events (ShopTransactionCompleted, PlayerEnteredStation, etc.).
**Done when:** at least two emergent scenarios ship and have observable in-game behavior across a full run.

Anything past M3 — scripting, authoring tools, replay systems, visual scenario editors — is deliberately out of scope for this vision doc. Re-open the question when M3 lands.

---

## Summary

Three layers, one rule per layer:

- **Events** — what the world did. Add producers before scenarios want them.
- **Effects** — what scenarios can do. Keep concrete; grow slowly.
- **Scenarios** — how the world reacts. C++ only; disposable; one file each.

Single binary. No scripts. Compose with C++ primitives. Let the abstractions earn themselves through repeated content, not anticipation.
