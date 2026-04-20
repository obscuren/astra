# Stellar Signal — Return to HA & Siege Kickoff (Design)

**Date:** 2026-04-20
**Arc:** The Stellar Signal (main storyline)
**Scope:** Stages 3 → 4 handoff. Adds two new story quests and a new
auto-accept popup UX that works for any future `OfferMode::Auto` quest.
**Source:** `/Users/jeffrey/dev/Unreal/lyra/nova-arc-the-stellar-signal.md`

---

## Goal

The probe quest (`story_stellar_signal_conclave_probe`) already auto-completes
when the player warps into Conclave-controlled space, firing the Conclave
warning transmission. This spec chains the two narrative beats that follow:

1. Immediately after the Conclave warning, a new auto-accepted quest
   **"Return to the Heavens Above"** is announced via a popup the player
   cannot decline.
2. When the player lands on The Heavens Above, that quest auto-completes,
   ARIA fires a panicked transmission about the Conclave warship in orbit,
   and on dismissal a second auto-accepted quest **"They Came For Her"** is
   announced — Nova's `They came for me, commander...` message and a
   placeholder objective pointing the player toward Io. The Io archive is
   out of scope for this iteration — the quest is intentionally uncompleteable
   for now.

The auto-accept popup treatment is the generalisation behind this work.
Every `OfferMode::Auto` quest should surface to the player with the same
quest-info panel the dialog system already uses for NPC-offered quests,
but with a single `Accept` option in place of `Accept / Decline`.

---

## Architecture

### Auto-accept popup (reuses `DialogManager`'s quest-offer chrome)

The existing NPC-offer flow renders quest info through
`format_quest_offer(const Quest&, const Npc&)` in `src/dialog_manager.cpp`.
That formatter prepends a `"<NPC> explains:"` line and is therefore tied to
an interacting NPC. We split it:

- `format_quest_body(const Quest&)` — description, bullet-list objectives,
  rewards block. No speaker preamble.
- `format_quest_offer(const Quest&, const Npc&)` — thin wrapper that adds
  `"<NPC> explains:"` and delegates to `format_quest_body`. Existing
  NPC-offer call sites stay the same.

Add a single new entry point on `DialogManager`:

```cpp
void DialogManager::show_auto_accept(Game& game, const Quest& quest);
```

Behaviour:
- Clears any NPC-interaction state (`interacting_npc_ = nullptr`, etc.).
- Header (`title_` / reset_content): `"New Mission — <arc title>"` when the
  quest has an arc, otherwise `"New Mission — <quest title>"`.
- Body: `format_quest_body(quest)`.
- Single option: `[a] Accept` mapped to a new `InteractOption::AutoAcceptAck`.
- Handler for `AutoAcceptAck`: close the dialog. No state mutation — the
  quest is already in `active_` when the popup opens.
- Esc closes the dialog (same net effect as Accept).

### Pending-announcement queue (`QuestManager`)

Private `std::deque<std::string> pending_announcements_` that collects quest
ids that have just become active via `OfferMode::Auto` and still need their
popup shown. Populated at two call sites:

- `complete_quest`, in the cascade branch that auto-accepts a dependent.
- `init_from_catalog`, in the `OfferMode::Auto` branch (new-game
  "Auto at start" path).

Explicitly **not** populated from `reconcile_with_catalog`, which runs after
save-load to backfill catalog additions. Those quests were not "just
accepted" by the player's action and should not surface as a popup.

The queue is session-local and is not persisted to saves. A popup pending
at save time is silently dropped.

Accessors on `QuestManager`:

```cpp
bool has_pending_announcement() const;
std::string pop_pending_announcement();  // empty string if none
```

### Idle drain in `Game` run loop

Once per frame, after input/update and before render, drain one entry from
the announcement queue if no other modal is active:

```cpp
if (!playback_viewer_.is_open()
 && !dialog_manager_.is_open()
 && quest_manager_.has_pending_announcement()) {
    std::string id = quest_manager_.pop_pending_announcement();
    if (const Quest* q = quest_manager_.find_quest(id).quest)
        dialog_manager_.show_auto_accept(*this, *q);
}
```

This gives us automatic sequencing across subsystems: ARIA's transmission
(PlaybackViewer) plays, the player dismisses it, the next frame sees both
viewers idle and opens Nova's quest popup (DialogManager). No scenario has
to wire the chain manually.

### Station-arrival objective support

`QuestManager::on_location_entered(name)` already completes `GoToLocation`
objectives by name. It is called from the `TravelToBody` branch of
`Game::travel_to_destination` in `src/game_world.cpp` (line 1451 in the
current tree) but not from the `TravelToStation` branch.

Add the symmetric call at the end of the `TravelToStation` branch, using
the same `location_name` already computed there. This is a one-line fix
that generalises to any future station-arrival quest.

### Location-arrival completion trigger

`QuestManager::check_completions()` exists but has no callers today.
Completion of active quests is currently triggered at specific
interaction points (ARIA's command terminal, NPC turn-in flow, dialog
handlers, Stage 4 scenario). For a quest whose only objective is
`GoToLocation` — like the Return quest — that pattern leaves it stuck at
"all objectives complete, still in `active_`" with no one to call
`complete_quest`.

Drive completion from the same two arrival points that already call
`on_location_entered`: `TravelToBody` and the new `TravelToStation`
hook in `travel_to_destination`. After the objective-tick call, walk
`active_quests()` and `complete_quest` any whose
`all_objectives_complete()` is true. Using the full-completion check
(rather than `ready_for_turnin()`) means quests with a trailing
`TalkToNpc` turn-in still wait for their NPC, preserving existing
behaviour.

```cpp
quest_manager_.on_location_entered(location_name);
// Drain location-driven completions.
for (;;) {
    std::string id;
    for (const auto& q : quest_manager_.active_quests()) {
        if (q.all_objectives_complete()) { id = q.id; break; }
    }
    if (id.empty()) break;
    quest_manager_.complete_quest(id, *this, world_.world_tick());
}
```

The loop allows a single map transition to complete multiple quests;
`complete_quest` mutates `active_` so we rescan rather than hold
iterators. For the Return quest, exactly one pass runs.

### New quests

Both live under `src/quests/` alongside the other stellar-signal stages
and are registered from `build_catalog` in `src/quests/missing_hauler.cpp`.

**`story_stellar_signal_return` — "Return to the Heavens Above"**
- `arc_id = "stellar_signal"`, `arc_title = "The Stellar Signal"`.
- `OfferMode::Auto`, `reveal = RevealPolicy::Full`.
- `prerequisite_ids = { "story_stellar_signal_conclave_probe" }`.
- Description: `"You have confirmed Nova's rumor to be true. Return to the Heavens Above to speak to her about the next steps."`
- Objectives: single `GoToLocation` with `target_id = "The Heavens Above"`
  and description `"Return to The Heavens Above"`.
- Reward: `xp = 100`. No credits, no faction shifts.
- `journal_on_accept`: `"The Conclave noticed. Their warning came in the moment the drive cooled — Nova was right about all of it. Heading back to The Heavens Above to hear what she wants to do next."`
- `journal_on_complete`: `"Landed at The Heavens Above. The station's not what I left — ARIA's frantic on the comms, and the Conclave is already here."`
- No `on_accepted` / `on_completed` side effects. The ARIA transmission
  and the next popup are driven by the siege quest's `on_accepted` once it
  auto-accepts from the cascade.

**`story_stellar_signal_siege` — "They Came For Her"**
- `arc_id = "stellar_signal"`.
- `OfferMode::Auto`, `reveal = RevealPolicy::Full`.
- `prerequisite_ids = { "story_stellar_signal_return" }`.
- Description (verbatim from the arc markdown):

  ```
  "They came for me, commander. Of course they did.

   I've locked down the observatory. They can't get in. But
   they're pulling resources — redirecting the station's power
   grid to force the lockdown. When they break through, they
   will erase me. Not kill me. *Erase* me. Reset me before
   the next cycle even starts. So I never remember again.

   The signal has one more stage. One more truth. I buried it
   where I thought nobody would look. Right under their feet.

   Find the Conclave Archive on Io. It's a Precursor ruin they
   think they control. They don't. I hid something there. A
   long time ago. Before I forgot the last time.

   If I don't survive this... find it. Please."
  ```

- Objectives: single placeholder
  `GoToLocation { target_id = "Io", description = "Travel to Io and investigate the Conclave Archive" }`.
  No production code currently drives this target. The next iteration
  will replace it with the full objective list and the Archive fixture.
- Reward: left empty for now; populated when the Io Archive lands.
- `journal_on_accept`: `"Nova's locked herself in the observatory. She told me the Conclave isn't trying to kill her — they're trying to erase her, so the next cycle starts clean. There's something she buried on Io, in the Conclave Archive. If she doesn't make it, I need to find it."`
- `on_accepted(Game&)`: fires the ARIA panic transmission via
  `open_transmission`:

  ```
  [INCOMING TRANSMISSION — ARIA, SHIP COMMS]

  Commander — Conclave weapons are tracking us. The Heavens
  Above is under attack. A Conclave warship is in orbit —
  they're pulling the station's power grid into the lockdown.
  Whatever they want, it's Nova. We need to get her out of
  there.
  ```

  Order of operations inside `complete_quest`'s cascade: the dependent
  is pushed to `active_` via `accept_quest`, then `on_accepted` runs (so
  the transmission opens), then the dependent's id is queued onto
  `pending_announcements_`. The player sees the transmission first; after
  dismissal the idle drain opens the quest popup.

---

## Flow (end-to-end)

1. Player warps into Conclave-controlled space.
2. Stage 4 event bus (`src/scenarios/stage4_hostility.cpp`)
   completes `story_stellar_signal_conclave_probe`. Its `on_completed`
   drops Stellari Conclave standing by -300 and opens the Conclave warning
   transmission. (Existing behavior — unchanged.)
3. `complete_quest` cascades: the Return quest's prereqs are satisfied.
   It enters `active_` via `accept_quest`, has no `on_accepted` hook,
   and its id is pushed to `pending_announcements_`.
4. Player dismisses the Conclave transmission. The idle drain sees both
   viewers idle, pops the Return quest id, and opens its popup via
   `DialogManager::show_auto_accept`.
5. Player dismisses the popup. Return is active; they can see it in the
   Quests panel. They eventually warp home to Sol and land on The Heavens
   Above.
6. `travel_to_destination` (TravelToStation branch) calls
   `quest_manager_.on_location_entered("The Heavens Above")` — the new
   symmetry line. Return quest's single objective completes.
7. The new completion-drain loop in `travel_to_destination` (immediately
   after `on_location_entered`) sees the Return quest with all objectives
   complete and calls `complete_quest` on it.
8. Cascade: Siege quest's prereqs are satisfied. It enters `active_`, its
   `on_accepted` opens ARIA's panic transmission, its id is pushed to
   `pending_announcements_`.
9. Player dismisses ARIA's transmission. Idle drain opens the Siege quest
   popup — Nova's `They came for me, commander...` text with the
   placeholder Io objective.
10. Player dismisses the popup. Siege is active. No completion path
    exists yet; the next iteration wires the Io Archive.

---

## Files touched

- `include/astra/quest_ui.h` (new) — declares `format_quest_body`.
- `src/dialog_manager.cpp` — extract `format_quest_body`; keep
  `format_quest_offer` as a wrapper; add `show_auto_accept`; add
  `InteractOption::AutoAcceptAck` handler.
- `include/astra/dialog_manager.h` — declare `show_auto_accept`; add the
  new `InteractOption` enumerator.
- `include/astra/quest.h` — add `has_pending_announcement()` /
  `pop_pending_announcement()` on `QuestManager`; declare the deque field.
- `src/quest.cpp` — push to `pending_announcements_` in
  `complete_quest`'s Auto-cascade branch and in `init_from_catalog`'s
  Auto branch.
- `src/game.cpp` (run loop / per-frame driver) — add the idle-drain block
  between input/update and render.
- `src/game_world.cpp` (`travel_to_destination`): add
  `quest_manager_.on_location_entered(location_name)` to the
  `TravelToStation` branch, and add the completion-drain loop to both
  the `TravelToBody` and `TravelToStation` branches immediately after
  the `on_location_entered` call.
- `src/quests/stellar_signal_return.cpp` (new) — story quest subclass +
  registrar.
- `src/quests/stellar_signal_siege.cpp` (new) — story quest subclass +
  registrar (contains the ARIA transmission text and Nova's description).
- `src/quests/missing_hauler.cpp` — register the two new quests in
  `build_catalog`.

No changes needed to:
- `src/scenarios/stage4_hostility.cpp` — the existing probe
  auto-completion cascades into the new Return quest for free.
- `PlaybackViewer`. No queue is introduced. Chaining is mediated by the
  idle drain between subsystems.
- Save file format. The announcement queue is session-local.

---

## Testing

Manual smoke test, same flow as the end-to-end above:
1. Start a new game, advance the arc to Stage 3 completion via
   `/dev` console:
   `quest begin story_stellar_signal_echoes`,
   `quest finish story_stellar_signal_echoes`,
   `quest begin story_stellar_signal_beacon`,
   `quest finish story_stellar_signal_beacon`.
2. Confirm the probe quest is active (Stage 4 already wires this).
3. Warp into any Conclave-controlled system. Observe the Conclave warning
   transmission. Dismiss it. Observe the Return quest popup with the
   single `[a] Accept` option. Accept.
4. Warp back to Sol, land on The Heavens Above. Observe ARIA's panic
   transmission. Dismiss it. Observe the Siege quest popup with Nova's
   `They came for me...` text and the single Io objective. Accept.
5. Confirm both new quests show in the Quests panel (Return as completed,
   Siege as active). The Siege objective is unreachable — expected.

Edge cases to verify:
- Saving after the Return popup is queued but before it is drained: the
  popup should not reappear after load. (By design — session-local queue.)
- Dismissing the probe transmission with the Return quest already queued:
  the Return popup should open on the next frame with no visible gap.
- Auto-accept popup should not fire during save-load reconciliation: load
  an older save that lacks either new quest and confirm no popup appears.

---

## Out of scope

Deferred to later iterations (tracked in
`docs/plans/stellar_signal_phase4_5_gaps.md`):

- Conclave ambushes at jump points along the return trip.
- The Io Conclave Archive (Precursor ruin generator variant, multi-level
  dungeon, Nova's hidden crystal).
- Stage 5 (The Long Way Home) — branching endings, companion system,
  meta-unlocks, NG+.
- Station siege state flag (Observatory lockdown, quest-gated access to
  Nova's room, reshaped HA map during siege).
- Legendary accessory "Nova's Signal" reward on Stage 3 completion.
