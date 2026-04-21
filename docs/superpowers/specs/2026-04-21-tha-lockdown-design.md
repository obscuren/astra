# Stage 4 — The Heavens Above Lockdown (Design)

**Date:** 2026-04-21
**Arc:** The Stellar Signal (main storyline) — Stage 4
**Scope:** Bar the player from docking at The Heavens Above for the
duration of the Siege / Conclave Archive arc. Move ARIA's panic
transmission from "landing at THA" to "arriving in Sol system". Completes
the roadmap item *"Stage 4 — Station siege & lockdown"*.

---

## Goal

After the Conclave warning transmission, the Return quest should tell the
player to go home — but THA is under siege and refuses all dockings. The
player never physically lands at THA during the siege. Instead:

1. Warping into Sol fires ARIA's panic transmission, completes the Return
   quest, and cascades the Siege quest popup (Nova's instructions to Io).
2. Any attempt to dock THA from the star chart plays a short "Automated
   Response — THA Traffic Control" transmission denying approach. Player
   stays in their ship.
3. Once the Siege quest completes (Conclave Archive slice, next iteration),
   the lockdown flag is cleared and normal docking resumes.

---

## Architecture

### World flag

Reuse the existing `world_flags_` map on `WorldManager` (persisted via
`save_file.cpp`). Single new flag:

- `tha_lockdown` — `true` while THA is unlandable.

**Lifecycle:**
- Set `true` in `StellarSignalReturnQuest::on_accepted`. The Return quest
  already auto-accepts from the probe-completion cascade, so the flag
  flips on exactly when the Conclave warning ends.
- Cleared in `StellarSignalSiegeQuest::on_completed`. That hook doesn't
  exist yet; add it and have it clear the flag. The Archive iteration
  will add any other Siege-completion side effects alongside.

### Move Return completion off "land at THA"

Today the Return quest completes when the player physically docks at THA:
`on_location_entered("The Heavens Above")` inside `TravelToStation`. That
can no longer happen — the player never actually docks during the siege.

Drive Return completion from the Stage 4 scenario instead, subscribed to
`SystemEnteredEvent` with `system_id == 1` (Sol):

- Extend `src/scenarios/stage4_hostility.cpp`'s existing `SystemEntered`
  handler. The handler already runs on every warp; add a branch that,
  if the Return quest is active *and* the incoming system is Sol,
  completes it directly via
  `g.quests().complete_quest("story_stellar_signal_return", g, tick)`.
- `complete_quest` cascades to the Siege quest (existing behavior). Siege
  `on_accepted` fires ARIA's panic transmission. Popup queue opens Nova's
  text as the next modal.

### ARIA transmission timing

No move needed — `StellarSignalSiegeQuest::on_accepted` already opens
ARIA's panic transmission via `open_transmission`. Because Siege is
accepted via the Return cascade inside the scenario handler (which fires
on warp into Sol), ARIA now panics at the warp boundary instead of at
the THA dock. Verbatim text unchanged.

### Intercept in `TravelToStation`

`Game::travel_to_destination` in `src/game_world.cpp:1277` — at the very
top of the `TravelToStation` case, before `dest_key` / navigation state
mutation:

```cpp
if (target_sys.id == 1 /* Sol */ && world_.world_flag("tha_lockdown")) {
    open_transmission(*this,
        "AUTOMATED RESPONSE — THA TRAFFIC CONTROL",
        {
            "...this is The Heavens Above Traffic Control...",
            "...all docking permissions have been revoked...",
            "...station is under emergency lockdown...",
            "...do not attempt approach. Repeat: do not",
            "attempt approach...",
            "",
            "...end transmission...",
        });
    return;  // no nav mutation, no map swap — player stays on ship
}
```

No quest hooks, no cascade. The intercept is purely a denial. All Return
/ Siege wiring has moved upstream to the `SystemEntered` handler, so by
the time the player could attempt this the Siege quest is already active.

Sol's id is 1 throughout the codebase (see existing references in
`src/quests/stellar_signal_return.cpp`, `stellar_signal_siege.cpp`,
and `src/star_chart.cpp`).

### Return quest journal and objective

Update `StellarSignalReturnQuest` to reflect the new reality:

- Objective changes from
  `GoToLocation "The Heavens Above"` to
  `GoToLocation "Sol"` with description
  `"Warp back to the Sol system"`. The objective string is what the
  Quests panel shows, so it needs to read right even though the actual
  completion now runs through the scenario.
- `journal_on_complete` rewritten:
  `"Never made it aboard. Sol's inbound channels were already locked — ARIA broke through on the ship comms while THA Traffic Control looped an automated denial. The Heavens Above is under siege."`
- `on_accepted` gains a single line:
  `set_world_flag(game, "tha_lockdown", true);`

### Siege quest completion hook

Add `StellarSignalSiegeQuest::on_completed(Game& game)` — currently
not implemented. Body:

```cpp
void on_completed(Game& game) override {
    set_world_flag(game, "tha_lockdown", false);
}
```

Everything else for Siege completion (re-entry messaging, Nova NPC state
reset, reward population) lives in the Conclave Archive iteration.

---

## Flow (end-to-end)

1. Player completes Probe in Conclave space. Conclave warning plays.
   Cascade: Return auto-accepts. Return's `on_accepted` sets
   `tha_lockdown = true`. Return popup queued; player dismisses the
   warning, then the Return popup.
2. Player warps to Sol. `SystemEnteredEvent{system_id=1}` emits.
3. Stage 4 scenario handler sees `tha_lockdown` flag + active Return
   quest + Sol arrival → `complete_quest("story_stellar_signal_return")`.
4. Cascade: Siege auto-accepts. `Siege::on_accepted` opens ARIA's panic
   transmission. Siege popup queued.
5. Player dismisses ARIA's transmission. Idle drain opens the Siege popup
   with Nova's "*They came for me, commander...*" text.
6. Player navigates to THA via the star chart at any point during the
   siege (curiosity, habit, or quest misdirection). Intercept fires the
   Automated Response transmission. No dock, no map swap.
7. Player completes the Archive on Io (next iteration). `Siege::on_completed`
   clears `tha_lockdown`. Docking resumes.

---

## Files touched

- `src/quests/stellar_signal_return.cpp` — set `tha_lockdown` in
  `on_accepted`; update objective target and `journal_on_complete`.
- `src/quests/stellar_signal_siege.cpp` — add `on_completed` that
  clears `tha_lockdown`.
- `src/scenarios/stage4_hostility.cpp` — extend the `SystemEntered`
  handler to complete the Return quest on Sol arrival.
- `src/game_world.cpp` — add the lockdown intercept at the top of the
  `TravelToStation` branch of `Game::travel_to_destination`.
- `docs/roadmap.md` — tick *Stage 4 — Station siege & lockdown*.

No changes to:
- `WorldManager`, `EventBus`, `save_file.cpp` — all required plumbing
  exists.
- `DialogManager`, `PlaybackViewer`, `QuestManager` — sequencing already
  works via the idle-drain mechanism introduced in the 2026-04-20 slice.
- `StarChartViewer` — station still selectable; denial is at the travel
  boundary.

---

## Testing

Manual smoke test in dev mode:

1. New game, advance to Stage 3 completion:
   `quest finish story_stellar_signal_hook`,
   `quest finish story_stellar_signal_echoes`,
   `quest finish story_stellar_signal_beacon`.
2. Accept Probe (NPC offer), warp to a Conclave system → probe completes,
   Conclave transmission plays, Return popup appears. Accept.
3. Confirm `tha_lockdown` is set (inspect via dev console if a flag
   dumper exists, or trust the flow).
4. Warp back to Sol. Expected:
   - ARIA panic transmission plays.
   - Dismiss → Siege popup (Nova's text). Accept.
5. Open star chart, select THA, press `t`. Expected: Automated Response
   transmission. No map change. Quest panel unchanged.
6. Dismiss, repeat step 5. Expected: same denial every time.
7. Force-complete Siege via dev console:
   `quest finish story_stellar_signal_siege`.
8. Travel to THA. Expected: normal docking, full map load.

Save/load: save between steps 4 and 5, reload, confirm the flag
persists and the denial still fires.

Edge cases:
- Travel to a station in some *other* Conclave system during siege →
  no denial (flag is THA-specific, gated on `system_id == 1`).
- Warp to Sol with Return already completed → scenario handler's
  "active Return quest" guard prevents double-completion.

---

## Out of scope

- Conclave Archive on Io (next iteration — separate spec).
- Visual change to THA on the star chart (a `[locked]` marker or color
  shift). Nice polish; not required for this slice.
- Siege resolution cinematic / rescue sequence (Archive iteration).
- Alternate docking routes (emergency airlocks, Nova's hidden passage).
  These belong to the Archive's internal design.
