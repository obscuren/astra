# Mines & Traps — Generalized Trap/Trigger Framework

**Date:** 2026-04-28
**Status:** Design

## Goals

1. Activate the 5 inert mine items shipped in the tinkering expansion (Proximity, EMP, Incendiary, Decoy, Caltrops) so they can be deployed and trigger on enemy step.
2. Build a generalized trap/trigger framework that also supports **pre-placed dungeon traps** (pressure plates, dart launchers, etc.) added by future specs.
3. Make traps **owner-aware**: a deployed mine triggers only on entities currently hostile to the placer (live faction eval, no snapshot). Dungeon traps default to "any entity."
4. Add a minimal AI noise-event signal so the Decoy Mine has real utility, without committing to a full sound-propagation system.
5. Add a **trap detection roll** keyed off a new `Player.trap_detection` attribute, so future buffs / INT scaling / equipment can modify spot chance.

## Non-Goals

- Perception / Awareness skill — the spec uses a simple `int` attribute on `Player`; a skill layer is a future spec.
- AI pathfinding around visible traps. Hostile NPCs walk into hidden *and* visible traps in v1.
- Disarm / re-pocket. Once placed, mines are spent (no pickup).
- Sound propagation system (walls block sound, falloff, source stacking).
- Throw-items (grenades, rocks). They will reuse `NoiseEvent` later but are out of scope here.
- Turret deployables (separate already-landed system).
- Save migration. v50 schema bump rejects v49 saves per project rule.

## High-Level Architecture

```
Player presses 'a' (apply) on Mine item
        │
        ▼
TrapSystem::begin_deploy(kind, player) ──► Telegraph (Burst, range 3 or 4)
        │
        │ on confirm — destination tile chosen
        ▼
TrapSystem::place(map, kind, dest, player_owner)
        │
        ▼
MapState.traps  ◀── new vector<Trap>
        │
        ▼ each player movement / NPC turn
TrapSystem::on_entity_enters_tile(stepper, x, y) ──► resolve_trigger(trap, stepper)
                                              │
                                              ├─► AoE damage (placer immune to splash)
                                              ├─► add_effect(victim, Burn / EmpDisabled / Slow)
                                              └─► (Decoy) push NoiseEvent → MapState.noise_events

NPC turn ──► consume_noise_events() ─► retarget if idle/wandering and hostile-to-emitter

Player movement ──► detection roll for hidden traps newly in reveal radius
```

**New module: `astra/trap.h` + `src/trap.cpp`** — owns `Trap`, `TrapKind`, `TrapTrigger`, `TrapSystem`, deploy + trigger pipeline.

**New module: `astra/noise_event.h` + `src/noise_event.cpp`** — owns `NoiseEvent` and the noise-queue helpers consumed by NPC turn logic. Distinct from the existing numerical `noise.h` (fBm noise generator).

`MapState` (in `save_file.h`) gains:
- `std::vector<Trap> traps;`
- `std::vector<NoiseEvent> noise_events;`

`Player` gains:
- `int trap_detection = 0;`

## Section 1 — Data model

### 1.1 `astra/trap.h`

```cpp
#pragma once

#include <string>
#include <cstdint>

namespace astra {

class Game;
struct Player;
struct Npc;
struct TileMap;

enum class TrapKind : uint8_t {
    ProximityMine,
    EmpMine,
    IncendiaryMine,
    DecoyMine,
    Caltrops,
    DungeonGeneric,   // catch-all for future dungeon-placed traps
};

enum class TrapTrigger : uint8_t {
    NonFriendlyToOwner,  // player- and NPC-deployed default
    AnyEntity,           // dungeon-placed default
    PlayerOnly,          // future
};

struct Trap {
    TrapKind kind = TrapKind::ProximityMine;
    int x = 0;
    int y = 0;

    // Visibility / detection
    bool hidden = true;
    int reveal_radius = 2;             // Chebyshev — within this, player rolls to detect
    int detection_dc = 12;             // 1d20 + player.trap_detection vs DC
    bool was_in_player_radius = false; // debounce: roll only on enter

    // Trigger logic
    TrapTrigger trigger_mode = TrapTrigger::NonFriendlyToOwner;
    std::string owner_faction;         // empty if player-placed or dungeon-placed
    bool placer_is_player = false;
    int placer_npc_id = -1;            // for NPC-placed traps; splash-immunity ref

    // State
    int activations_remaining = 1;     // Caltrops uses 3; others 1
    int placed_tick = 0;
};

const char* trap_kind_name(TrapKind k);
char trap_glyph(TrapKind k);
int trap_color(TrapKind k);

// Lifecycle
void place_trap(Game& game, Trap t);
void place_dungeon_trap(TileMap& map, int x, int y, TrapKind kind,
                        TrapTrigger trigger = TrapTrigger::AnyEntity,
                        bool hidden = true,
                        int detection_dc = 14);

// Called from player and NPC movement code paths
void on_entity_enters_tile(Game& game, int x, int y, bool is_player, int npc_id);

// Called once per world tick after movement, before render
void update_trap_detection(Game& game);
void tick_noise_events(Game& game);

} // namespace astra
```

### 1.2 `astra/noise_event.h`

```cpp
#pragma once

#include <string>

namespace astra {

struct NoiseEvent {
    int x = 0;
    int y = 0;
    int radius = 5;          // Chebyshev — 10 tile diameter (Decoy default)
    int ttl_ticks = 5;       // expires after N world ticks
    std::string emitter_owner_faction;  // empty if player-emitted
    bool emitter_is_player = false;
};

} // namespace astra
```

### 1.3 New `EffectId`

In `astra/effect.h`:

```cpp
EmpDisabled = 400,   // disables energy weapons, ability cooldown advance, active aura emitters
```

Factory `make_emp_disabled_ge(int duration)`. Aggregation hook: any code path that fires an energy-weapon attack, advances an ability cooldown, or emits an aura checks `has_effect(entity.effects, EffectId::EmpDisabled)` and short-circuits.

### 1.4 `Player.trap_detection`

In `astra/player.h`:

```cpp
int trap_detection = 0;   // additive bonus to 1d20 detection rolls.
                          // Future: scale with INT, buff via items / effects.
```

Saved alongside other player ints.

### 1.5 `MapState` extensions

In `astra/save_file.h`:

```cpp
struct MapState {
    // ... existing fields ...
    std::vector<Trap> traps;             // v50
    std::vector<NoiseEvent> noise_events; // v50 (transient but cheap to persist)
};
```

`SAVE_FILE_VERSION` bumps to `50` with comment `"v50: trap registry + noise events"`.

## Section 2 — Deploy pipeline

### 2.1 `use_item` dispatch

In `src/game_rendering.cpp` (or wherever `Game::use_item` lives), the `default` branch for `ItemType::Mine` is replaced with:

```cpp
case ItemType::Mine: {
    TrapKind kind = trap_kind_for_item_id(item.id);
    int range = (kind == TrapKind::Caltrops) ? 4 : 3;
    TelegraphSpec spec;
    spec.shape = TelegraphShape::Burst;
    spec.range = range;
    spec.width = (kind == TrapKind::Caltrops) ? 1 : 0; // burst radius
    spec.require_walkable_dest = true;
    telegraph_.begin(spec, player_.x, player_.y,
        [this, idx = index, kind](const TelegraphResult& r) {
            on_mine_throw_confirmed(idx, kind, r.dest_x, r.dest_y);
        });
    return; // do not consume yet — consumed on confirm
}
```

`on_mine_throw_confirmed`:
1. Re-fetch the item by index (defensive — inventory may have shifted).
2. For Caltrops: scatter 4 tiles inside the 3×3 around `(dest_x, dest_y)`, skipping impassable tiles and the player. Each tile becomes a separate `Trap` with `kind = Caltrops`, `hidden = false`, `activations_remaining = 3`.
3. For other mines: place a single `Trap` at `(dest_x, dest_y)`.
4. All player-placed traps: `placer_is_player = true`, `owner_faction = ""`, `placed_tick = world_tick`.
5. Consume one stack count (or remove the item if last).
6. `advance_world(ActionCost::wait)`.

### 2.2 Telegraph shape

The existing `TelegraphShape::Burst` (radius around target tile) is used:
- Single-tile mines: `range = 3`, burst `width = 0` (a 1-tile target).
- Caltrops: `range = 4`, burst `width = 1` (3×3 reticule). The 4-of-9 scatter is computed at confirm time, not by the telegraph — telegraph only previews the 3×3 for the player.

If `Burst` is not yet fully implemented (the telegraph header notes a `TODO(burst)`), this spec depends on its implementation. If incomplete at execution start, a small extension to telegraph is part of Phase 1 below.

## Section 3 — Trigger pipeline

### 3.1 Hooks

`on_entity_enters_tile(game, x, y, is_player, npc_id)` is called from:
- `Game::move_player()` after the player's position has been updated.
- The NPC turn loop after each successful NPC move.

### 3.2 Algorithm

```cpp
void on_entity_enters_tile(Game& g, int x, int y, bool is_player, int npc_id) {
    auto& traps = g.world().map_state().traps;
    for (auto it = traps.begin(); it != traps.end(); /* manual advance */) {
        if (it->x != x || it->y != y) { ++it; continue; }
        if (!should_trigger(*it, g, is_player, npc_id)) { ++it; continue; }

        resolve_trap(g, *it, x, y, is_player, npc_id);

        if (--it->activations_remaining <= 0) {
            it = traps.erase(it);
        } else {
            ++it;
        }
    }
}
```

### 3.3 `should_trigger`

```cpp
bool should_trigger(const Trap& t, const Game& g, bool stepper_is_player, int stepper_npc_id) {
    if (t.trigger_mode == TrapTrigger::AnyEntity) return true;
    if (t.trigger_mode == TrapTrigger::PlayerOnly) return stepper_is_player;

    // NonFriendlyToOwner — live faction evaluation, no snapshot.
    if (t.placer_is_player) {
        if (stepper_is_player) return false;                         // own trap
        const Npc& npc = g.world().npc(stepper_npc_id);
        return is_hostile_to_player(npc.faction, g.player());
    }
    // NPC-placed
    if (stepper_is_player) {
        return is_hostile_to_player(t.owner_faction, g.player());
    }
    if (stepper_npc_id == t.placer_npc_id) return false;             // own trap
    const Npc& stepper_npc = g.world().npc(stepper_npc_id);
    return is_hostile(stepper_npc.faction, t.owner_faction);
}
```

### 3.4 `resolve_trap` — per-kind effects

| Kind | Damage | Burst radius | Status applied | Activations | Hidden default | Throw range |
|---|---|---|---|---|---|---|
| Proximity Mine | 12 physical | 1 (3×3) | — | 1 | true | 3 |
| EMP Mine | 4 physical | 1 (3×3) | EmpDisabled, 5 ticks | 1 | true | 3 |
| Incendiary Mine | 8 fire (initial) | 1 (3×3) | Burn, 4 ticks @ 2 dmg/tick | 1 | true | 3 |
| Decoy Mine | 0 | — | NoiseEvent r=5, ttl=5 | 1 | false | 3 |
| Caltrops (per tile) | 3 physical | 0 | Slow, 3 ticks | 3 | false | 4 (3×3 reticule, scatter 4 of 9) |

**All values are tunable post-smoke-test. The implementation table lives in `trap.cpp` as a `static constexpr TrapDef` per-kind catalog.**

Splash damage rules:
- Stepping entity always takes full damage and full status.
- Other entities in burst radius take damage and status, **except**:
  - If `t.placer_is_player == true`, the player is exempt from splash.
  - If `t.placer_npc_id == stepper_npc_id` of any victim, that NPC is exempt.

After resolution, log a kind-appropriate message:
- `"The {kind} detonates!"` (Proximity / EMP / Incendiary)
- `"The decoy beeps loudly!"` (Decoy)
- `"You step on caltrops!"` / `"The {npc} steps on caltrops!"` (Caltrops)

## Section 4 — Decoy noise event system

### 4.1 Emission

When a Decoy Mine triggers:
1. Build a `NoiseEvent` at the trap's `(x, y)` with `radius = 5`, `ttl_ticks = 5`, `emitter_is_player = t.placer_is_player`, `emitter_owner_faction = t.owner_faction`.
2. Push it onto `MapState.noise_events`.
3. Decrement activations → 0 → trap is removed.

### 4.2 Consumption (NPC turn logic)

In the NPC turn handler, **before** move planning:

```cpp
for (const NoiseEvent& ev : map.noise_events) {
    if (npc.state != NpcState::Idle && npc.state != NpcState::Wandering) continue;
    if (chebyshev(npc.x, npc.y, ev.x, ev.y) > ev.radius) continue;
    bool hostile = ev.emitter_is_player
                   ? is_hostile_to_player(npc.faction, player)
                   : is_hostile(npc.faction, ev.emitter_owner_faction);
    if (!hostile) continue;
    npc.move_target_x = ev.x;
    npc.move_target_y = ev.y;
    npc.move_target_ttl = ev.ttl_ticks;
    break; // first matching event wins
}
```

The player ignores noise events. Only NPCs in idle/wandering states react. NPCs already in `Combat`/`Alert` continue their existing behavior.

### 4.3 Tick

`tick_noise_events` runs once per world tick:
- Decrement each event's `ttl_ticks`.
- Erase events where `ttl_ticks <= 0`.

## Section 5 — Visibility & detection

### 5.1 Player visibility rule

A trap is visible to the player if **any** of:
- `placer_is_player == true` (placer always sees own).
- `hidden == false` (was already revealed, or default-visible like Caltrops/Decoy).

Hidden traps that are `placer_is_player == false` render as the underlying tile.

### 5.2 Detection roll (player only)

`update_trap_detection(game)` runs after `Game::move_player()` and any teleport that changes player position:

```cpp
for (Trap& t : map.traps) {
    if (!t.hidden || t.placer_is_player) continue;

    bool now_in = chebyshev(player.x, player.y, t.x, t.y) <= t.reveal_radius;
    if (!t.was_in_player_radius && now_in) {
        int roll = 1 + rng() % 20;                  // 1d20
        if (roll + player.trap_detection >= t.detection_dc) {
            t.hidden = false;
            log("You spot a " + std::string(trap_kind_name(t.kind)) +
                " hidden " + relative_dir(player, t) + "!");
        }
    }
    t.was_in_player_radius = now_in;
}
```

`relative_dir` returns `"north"`, `"to the east"`, etc., based on offset from player.

NPCs do not roll — they walk in blindly.

### 5.3 Per-kind detection DCs (defaults)

| Kind | DC |
|---|---|
| Proximity Mine | 12 |
| EMP Mine | 13 |
| Incendiary Mine | 11 |
| Decoy Mine | n/a (visible) |
| Caltrops | n/a (visible) |
| Dungeon trap (default) | 14 (per-instance overridable) |

### 5.4 Rendering

A new render pass runs **between fixtures and ground items** in the map renderer:
1. For each trap visible to the player, render `trap_glyph(kind)` in `trap_color(kind)` at `(t.x, t.y)`.
2. Hidden traps render nothing (the underlying tile is shown).

Glyph palette (placeholder; final palette during smoke pass):
- Proximity Mine: `^` cyan
- EMP Mine: `^` bright blue
- Incendiary Mine: `^` orange
- Decoy Mine: `^` yellow
- Caltrops: `*` white
- Dungeon generic: `^` red

A dev console toggle (`reveal_traps`) flips a debug flag that forces all traps to render, irrespective of `hidden`.

## Section 6 — Save / load (v50)

`SAVE_FILE_VERSION` bumps from `49` to `50`. New fields serialized:

- `MapState.traps: std::vector<Trap>` — each trap writes `kind, x, y, hidden, reveal_radius, detection_dc, was_in_player_radius, trigger_mode, owner_faction, placer_is_player, placer_npc_id, activations_remaining, placed_tick`.
- `MapState.noise_events: std::vector<NoiseEvent>` — each writes `x, y, radius, ttl_ticks, emitter_owner_faction, emitter_is_player`.
- `Player.trap_detection: int`.

Per project rule: v49 saves are rejected on load. No migration code.

## Section 7 — Dungeon-placement integration

A single helper exposes the dungeon-trap path:

```cpp
void place_dungeon_trap(TileMap& map, int x, int y, TrapKind kind,
                        TrapTrigger trigger = TrapTrigger::AnyEntity,
                        bool hidden = true,
                        int detection_dc = 14);
```

Builds a `Trap`, sets `placer_is_player = false`, `placer_npc_id = -1`, `owner_faction = ""`. Pushes onto the map's traps vector.

This spec adds the API only; no specific generator integration. Generators that want to scatter traps will call this from a future spec.

## Section 8 — Smoke test plan

Verification is manual via the dev console (no unit tests per project convention):

1. Build clean: `cmake -B build -DDEV=ON && cmake --build build`.
2. Launch in `--term` mode.
3. Dev-spawn each mine kind into inventory; deploy each via the `a` key + telegraph.
4. Dev-spawn a hostile NPC (e.g., `Faction_Feral`); confirm it triggers each mine kind correctly.
5. Dev-spawn a friendly NPC; confirm it does **not** trigger player-placed mines.
6. Player walks own placed mine — confirm no trigger.
7. Player walks past hidden mine at distance 2 — confirm detection roll fires once; toggle `trap_detection` via dev console to verify the math.
8. Decoy: confirm an idle hostile NPC walks toward the decoy on next turn.
9. Caltrops: confirm 4 of 9 tiles scatter, each persists for 3 step-ons, then disappears.
10. Save / load round-trip: verify all traps and `was_in_player_radius` state survive.
11. `place_dungeon_trap` smoke via `dev_console`: confirm `AnyEntity` mode triggers on player.

## Section 9 — Out of scope (future specs)

- Perception/Awareness skill that scales `trap_detection`.
- AI pathfinding around visible traps.
- Disarm mechanics (skill-gated, with fail = trigger).
- Sound propagation (LoS-blocking walls, falloff, source stacking).
- Throw items (grenades, rocks) reusing `NoiseEvent`.
- Dungeon-generator catalog of new trap kinds (pressure plates, dart launchers, gas, teleport).
- Dynamic detection scaling: e.g., `detection_dc -= dim_light_penalty`.
- Pickup / re-pocket of own armed mines.

## Section 10 — Files touched

**New:**
- `include/astra/trap.h`
- `include/astra/noise_event.h`
- `src/trap.cpp`
- `src/noise_event.cpp` (or fold into trap.cpp if trivial)

**Modified:**
- `include/astra/effect.h` — `EmpDisabled` enum + factory.
- `src/effect.cpp` — factory body, EMP gating in `apply_damage_effects` if relevant.
- `include/astra/player.h` — `int trap_detection = 0`.
- `include/astra/save_file.h` — bump version, extend `MapState`.
- `src/save_system.cpp` — serialize new fields, reject v49.
- `src/game_rendering.cpp` (or wherever `use_item` lives) — `ItemType::Mine` dispatch.
- `src/game_world.cpp` — call `on_entity_enters_tile` after player and NPC movement; call `update_trap_detection` and `tick_noise_events` per world tick.
- `src/npc.cpp` — consume noise events in idle/wandering branch.
- `src/map_renderer.cpp` — new render pass for traps.
- `src/dev_console.cpp` — `reveal_traps` toggle, `spawn-trap <kind>` smoke command.
- Energy weapon attack site — `EmpDisabled` short-circuit (find via grep on existing energy weapon code).
- Ability cooldown advance site — `EmpDisabled` short-circuit.
- Aura system — suppress emitters when emitter has `EmpDisabled`.

## Section 11 — Phasing (for the plan that follows)

A reasonable execution split for the implementation plan:

1. **Phase 1 — Trap data model + storage + save bump.** `Trap`, `TrapKind`, `TrapTrigger`, `MapState.traps`, v50 save. No deploy yet, no triggers — just plumbing.
2. **Phase 2 — Deploy pipeline.** `use_item` for `ItemType::Mine` → telegraph → `place_trap`. Caltrops scatter logic. Verify visually only (no triggers yet).
3. **Phase 3 — Trigger pipeline + per-kind effect resolution.** `on_entity_enters_tile` hooked into player + NPC movement, splash damage, EMP / Burn / Slow application. New `EmpDisabled` EffectId.
4. **Phase 4 — Detection roll + rendering.** `Player.trap_detection`, `update_trap_detection`, render pass, dev-console toggle.
5. **Phase 5 — Decoy noise events.** `NoiseEvent`, `MapState.noise_events`, NPC consumer logic, `tick_noise_events`.
6. **Phase 6 — Dungeon API + smoke commands.** `place_dungeon_trap`, `spawn-trap` dev console, full smoke pass per Section 8.

Each phase is a single squashed commit on the feature branch.
