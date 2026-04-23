# Aura System — Design

Status: approved design, ready for implementation planning.
Related: `docs/superpowers/specs/2026-04-23-camp-making-design.md` (Cozy is
the first tenant of the new system and currently runs as an inline
proximity scan in `game_world.cpp`).

## Goal

Introduce a general **aura system** so that any positional entity —
fixtures, the player, NPCs — can emit gameplay effects to other entities
in range. Replace the hard-coded Cozy proximity scan with a data-driven
emitter model that cleanly supports future use cases (cooking gate,
burning aura, heat ward, lit area, etc.).

Downstream consumers then query capabilities by checking for the
relevant effect:

```cpp
if (has_effect(player.effects, EffectId::CookingFireAura)) { /* allow cooking */ }
```

No fixture lookups, no proximity helpers at consumer sites. The aura
system owns the "are you in range?" question and the consumer only
reads effect presence.

## Data model

```cpp
enum class AuraSource : uint8_t {
    Manual = 0,   // dev console / scripting; persists across saves
    Item,         // equipped item grants it; source_id = ItemId
    Effect,       // active Effect grants it;  source_id = EffectId
    Skill,        // learned skill grants it;  source_id = SkillId
    Fixture,      // emitted by a fixture (not stored on entities)
};

namespace AuraTarget {
    constexpr uint32_t Player       = 1u << 0;
    constexpr uint32_t FriendlyNpc  = 1u << 1;
    constexpr uint32_t HostileNpc   = 1u << 2;
    constexpr uint32_t AllNpcs      = FriendlyNpc | HostileNpc;
    constexpr uint32_t Everyone     = Player | AllNpcs;
}

struct Aura {
    Effect     template_effect;              // duration baked in; copied per apply
    int        radius       = 1;             // Chebyshev
    uint32_t   target_mask  = AuraTarget::Player;
    AuraSource source       = AuraSource::Manual;
    uint32_t   source_id    = 0;
};
```

Key choices, already settled in brainstorming:

- **Template effect**, not function pointer. Pure data, trivially
  copyable, easy to log/serialise.
- **Duration is encoded in the template effect**, not a separate field.
  Default Cozy-style behaviour uses `duration = 1` (auto-expires one
  tick after leaving range); longer-lived auras set their own duration
  (e.g. `duration = 5` gives a 5-tick grace period after stepping out).
- **Refresh semantics fall out of `add_effect()`**: applying an effect
  whose `EffectId` already exists replaces the existing one, so
  re-application each tick is idempotent, and the most recent emitter
  wins when multiple overlap.

## Naming convention

All Effect factory functions gain the `_ge` suffix. The rename applies
retroactively to the existing factories:

| Old                      | New                         |
|--------------------------|-----------------------------|
| `make_invulnerable()`    | `make_invulnerable_ge()`    |
| `make_burn()`            | `make_burn_ge()`            |
| `make_poison()`          | `make_poison_ge()`          |
| `make_regen()`           | `make_regen_ge()`           |
| `make_dodge_boost()`     | `make_dodge_boost_ge()`     |
| `make_attack_boost()`    | `make_attack_boost_ge()`    |
| `make_defense_boost()`   | `make_defense_boost_ge()`   |
| `make_haggle()`          | `make_haggle_ge()`          |
| `make_thick_skin()`      | `make_thick_skin_ge()`      |
| `make_flee()`            | `make_flee_ge()`            |
| `make_cozy()`            | `make_cozy_ge()`            |

The rename lands in one commit before any aura work so call-sites and
registries can reference the new names.

## Emitters

### Fixtures

Fixture auras live in a central registry — **not** on `FixtureData`.
Two sources, unioned at runtime:

- **Tag-derived** (`tag_auras`): `FixtureTag → Aura` template. Example:
  `FixtureTag::CookingSource → CookingFireAura`. Any fixture with the
  tag emits it.
- **Type-specific** (`type_auras`): `FixtureType → vector<Aura>`.
  Example: `FixtureType::Campfire → [Cozy]`. Explicit overrides for
  fixture-specific semantics (Cozy is deliberately not tag-driven — we
  don't want every HeatSource granting Cozy).

```cpp
std::vector<Aura> auras_for(const FixtureData& fd);
```

returns the union.

### Player / NPC

```cpp
struct Player { ... std::vector<Aura> auras; ... };
struct Npc    { ... std::vector<Aura> auras; ... };
```

Raw vector, mutated directly, with per-entry `source` / `source_id`
tags so removal is surgical (e.g. unequipping an item erases every
entry whose `source == Item && source_id == that_item_id`).

Entity auras are populated by `rebuild_auras_from_sources()`:

```cpp
void rebuild_auras_from_sources(Player& p);
void rebuild_auras_from_sources(Npc& n);
```

Wipes all non-`Manual` entries, then re-adds from:

- **Items**: each equipped item contributes `item.granted_auras` tagged
  `{Item, item.id}`.
- **Effects**: each active effect contributes `effect.granted_auras`
  tagged `{Effect, effect.id}`.
- **Skills**: `skill_auras(SkillId)` helper (keyed table in
  `src/aura.cpp`, same pattern as `ability_catalog`) contributes auras
  tagged `{Skill, skill.id}`.

`Manual` entries persist untouched — they're session state from the
dev console or scripting.

Rebuild is called whenever source state changes: equip/unequip,
effect add/remove, skill learn, and once per entity (the player plus
every loaded NPC) on save-load after Manual entries are read back.

## AuraSystem tick

New system owned by `Game`, runs once per `advance_world()` **after**
`tick_effects` + `expire_effects` and **before** the passive-regen
block. This is exactly where the current inline Cozy scan lives, so
the tick-order change is zero.

```cpp
class AuraSystem {
public:
    void tick(Game& game);
};
```

Per tick, iterate emitters — fixtures on the current map, the player,
all living NPCs — and apply each aura to in-range receivers:

```cpp
for (auto emitter : emitters) {
    auto auras = auras_of(emitter);             // registry or entity.auras
    for (const Aura& a : auras) {
        for (auto* receiver : receivers_in_box(emitter_pos, a.radius)) {
            if (receiver == emitter_entity) continue;              // no self-hit
            if (!target_matches(a.target_mask, *receiver, player)) continue;
            add_effect(receiver->effects, a.template_effect);      // refresh
        }
    }
}
```

`target_matches` resolves the mask against the receiver's identity:
player is `Player`; NPCs are `FriendlyNpc` or `HostileNpc` via
`is_hostile_to_player()` on their faction.

Performance: bounded-box scan keyed on each aura's radius. Radii are
small (Cozy = 6). With a typical map of ~10 aura-emitting fixtures and
~20 NPCs, the per-tick cost is negligible (~thousand tile checks).

## Campfire expiry

The campfire-expiry sweep is orthogonal to auras and moves to its own
method on `TileMap`:

```cpp
void TileMap::sweep_expired_fixtures(int current_tick);
```

Called from `advance_world()` alongside `AuraSystem::tick()`. Walks the
fixture list, removes any whose
`spawn_tick >= 0 && current_tick - spawn_tick >= campfire_lifetime_ticks`.
Generalises to any future time-limited fixture; no Campfire-specific
knowledge in the method.

## Migration of Cozy

Cozy becomes the first tenant:

1. Delete the inline proximity scan from `advance_world()` (the block
   that iterates the bounded box for `FixtureType::Campfire`).
2. Register `FixtureType::Campfire → Cozy` in `type_auras`. Template:
   `make_cozy_ge()` with `radius = 6`, `target_mask = Player`.
3. Replace the inline expiry loop with
   `map.sweep_expired_fixtures(world_tick)`.
4. Regen code continues to check `has_effect(player.effects, EffectId::Cozy)`
   verbatim — no change at the consumer site.

Post-migration, `game_world.cpp` no longer has any campfire-specific
code. The feature lives entirely in the aura + fixture registries.

## Save / load

- Player/Npc `auras` vector: on write, emit **only** entries whose
  `source == AuraSource::Manual`. All other entries are reconstructible
  from the enclosing entity's items/effects/skills.
- On read, populate the `Manual` entries as-is, then call
  `rebuild_auras_from_sources()` once to re-derive the rest.
- `Item::granted_auras` and `Effect::granted_auras` are **not
  serialised** — they are static data derived from the item/effect
  definition by id. Adding these fields does not bloat saves.
- Save-schema bump `v42 → v43`. Existing v42 saves are rejected on load
  per the project's pre-ship policy.

## File structure

New files:

- `include/astra/aura.h` — `Aura`, `AuraSource`, `AuraTarget`,
  `auras_for(const FixtureData&)`, `skill_auras(SkillId)`,
  `rebuild_auras_from_sources(...)` declarations.
- `src/aura.cpp` — registry tables (`tag_auras`, `type_auras`,
  `skill_auras`), rebuild implementation.
- `include/astra/aura_system.h` — `AuraSystem` class.
- `src/aura_system.cpp` — `AuraSystem::tick()`.

Modified files:

- `include/astra/effect.h`, `src/effect.cpp` — factory renames + new
  `std::vector<Aura> granted_auras` field on `Effect`.
- `include/astra/item.h` — new `std::vector<Aura> granted_auras`
  field on `Item`.
- `include/astra/player.h`, `include/astra/npc.h` — new
  `std::vector<Aura> auras` field.
- `include/astra/tilemap.h`, `src/tilemap.cpp` — new
  `TileMap::sweep_expired_fixtures(int)` method.
- `src/game_world.cpp` — delete inline Cozy scan + expiry loop;
  call `aura_system_.tick(*this)` and `map.sweep_expired_fixtures(...)`.
- `include/astra/game.h`, `src/game.cpp` — own the `AuraSystem`
  instance; wire equip/unequip/effect-add hooks to call rebuild.
- `include/astra/save_file.h`, `src/save_file.cpp` — v43 bump,
  serialise Manual-only entity auras.
- `docs/formulas.md` — document aura-driven Cozy.

## Non-goals (explicit YAGNI)

- **No aura stacking.** Multiple emitters applying the same `EffectId`
  replace rather than stack. Introduce stacking only when a real case
  needs it.
- **No exclusivity groups.** No "Cozy and Chilled cannot coexist"
  machinery; rely on EffectId-based replacement.
- **No distance falloff.** Auras are binary within radius.
- **No line-of-sight.** Auras pass through walls until a concrete
  gameplay need forces the check.
- **No NPC-authored auras in v1.** The infrastructure supports it
  (NPC has `auras` vector, emitter loop includes NPCs), but no NPC
  currently grants an aura. First use case will drive validation.
- **No priority on concurrent writers.** If two emitters race, last
  write wins. Re-visit if a gameplay case exposes the ordering.

## Scope / phased implementation

The work breaks into buildable phases, each a separate commit:

1. **Factory rename**. `make_* → make_*_ge` across the codebase. Pure
   refactor, no behaviour change.
2. **Scaffolding**. Introduce `Aura`, `AuraSource`, `AuraTarget`,
   empty `tag_auras` / `type_auras` / `skill_auras` registries. No
   system tick yet.
3. **AuraSystem skeleton**. Add the class, wire `AuraSystem::tick()`
   into `advance_world()`. Still a no-op (no registered auras).
4. **Port Cozy**. Register `Campfire → Cozy` in `type_auras`; delete
   the inline proximity scan.
5. **Extract expiry**. Move the inline campfire expiry into
   `TileMap::sweep_expired_fixtures()`; call from `advance_world()`.
6. **Entity auras**. Add `Aura`-vector field on `Player` and `Npc`;
   add `granted_auras` field to `Item` and `Effect`;
   implement `rebuild_auras_from_sources()`; wire hooks.
7. **Save/load**. v43 bump; serialise Manual-only entity auras.

Each phase leaves the game in a working state.

## Open questions

None — all design points resolved in brainstorming.
