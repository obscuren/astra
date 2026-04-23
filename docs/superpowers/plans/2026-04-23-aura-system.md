# Aura System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Introduce a general aura system — fixtures, the player, and NPCs all can emit gameplay effects to entities in range. Replace the hard-coded Cozy proximity scan with a data-driven emitter registry. Downstream consumers query capability via `has_effect(...)`, no proximity logic at call sites.

**Architecture:**
- `Aura` is a pure-data struct: `template_effect` (duration baked in), `radius`, `target_mask`, `source`/`source_id`.
- Fixture auras live in two registries (`tag_auras` + `type_auras`) keyed by `FixtureTag` / `FixtureType`; `auras_for(FixtureData)` returns the union.
- `Player` / `Npc` carry a `std::vector<Aura> auras` populated by `rebuild_auras_from_sources()` from items/effects/skills, plus any `Manual` entries.
- `AuraSystem::tick()` runs inside `Game::advance_world()` after `expire_effects` and before passive regen. It iterates emitters, applies `template_effect` to receivers in range via `add_effect()` (ID-based replace gives free refresh semantics).

**Tech Stack:** C++20, `namespace astra`. Build: `cmake -B build -DDEV=ON && cmake --build build -j`. Dev-mode console (`~`) exercises dev-command paths.

**Validation model (no unit-test framework):** Each task ends with a **build + smoke check + commit**. Build: `cmake --build build -j`. Smoke checks are specific and listed inline.

**Reference spec:** `docs/superpowers/specs/2026-04-23-aura-system-design.md`

---

## Task 1: Rename all Effect factories to `make_*_ge`

**Files:**
- Modify: `include/astra/effect.h` (declarations lines 89–99)
- Modify: `src/effect.cpp` (definitions lines 102, 114, 126, 138, 150, 162, 174, 186, 199, 211, 222)
- Modify (call sites):
  - `src/ability.cpp:144` — `make_flee`
  - `src/character_screen.cpp:408,410` — `make_haggle`, `make_thick_skin`
  - `src/dev_console.cpp:469,492,496,500` — `make_invulnerable`, `make_burn`, `make_regen`, `make_poison`
  - `src/dialog_manager.cpp:924` — `make_invulnerable`
  - `src/game.cpp:801,1399,1403` — `make_invulnerable`, `make_haggle`, `make_thick_skin`
  - `src/game_world.cpp:2192` — `make_cozy`
  - `src/save_file.cpp:1398` — `make_invulnerable`
  - `src/npcs/black_market_vendor.cpp:18` — `make_invulnerable`
  - `src/npcs/hub_npcs.cpp:17,60,102,197,243,285` — `make_invulnerable`
  - `src/npcs/merchant.cpp:15` — `make_invulnerable`
  - `src/npcs/nova.cpp:15` — `make_invulnerable`
  - `src/npcs/scav_keeper.cpp:16` — `make_invulnerable`
  - `src/npcs/scav_merchant.cpp:15` — `make_invulnerable`
  - `src/npcs/station_keeper.cpp:28` — `make_invulnerable`

- [ ] **Step 1: Rename declarations in `include/astra/effect.h` lines 89–99**

```cpp
Effect make_invulnerable_ge(int duration = -1);
Effect make_burn_ge(int duration, int damage_per_tick);
Effect make_poison_ge(int duration, int damage_per_tick);
Effect make_regen_ge(int duration, int heal_per_tick);
Effect make_dodge_boost_ge(int duration, int amount);
Effect make_attack_boost_ge(int duration, int amount);
Effect make_defense_boost_ge(int duration, int amount);
Effect make_haggle_ge();
Effect make_thick_skin_ge();
Effect make_flee_ge(int duration);
Effect make_cozy_ge();
```

- [ ] **Step 2: Rename definitions in `src/effect.cpp`**

Apply the same rename to the 11 function definitions. The function bodies do not change — only the names on the signature lines:

```
Effect make_invulnerable(int duration) {    →  Effect make_invulnerable_ge(int duration) {
Effect make_burn(int duration, int damage_per_tick) {  →  Effect make_burn_ge(...) {
Effect make_poison(int duration, int damage_per_tick) {  →  Effect make_poison_ge(...) {
Effect make_regen(int duration, int heal_per_tick) {  →  Effect make_regen_ge(...) {
Effect make_dodge_boost(int duration, int amount) {  →  Effect make_dodge_boost_ge(...) {
Effect make_attack_boost(int duration, int amount) {  →  Effect make_attack_boost_ge(...) {
Effect make_defense_boost(int duration, int amount) {  →  Effect make_defense_boost_ge(...) {
Effect make_haggle() {  →  Effect make_haggle_ge() {
Effect make_thick_skin() {  →  Effect make_thick_skin_ge() {
Effect make_flee(int duration) {  →  Effect make_flee_ge(int duration) {
Effect make_cozy() {  →  Effect make_cozy_ge() {
```

- [ ] **Step 3: Update all call sites**

For each of the 11 factory names, sed each call-site file to replace `make_X(` with `make_X_ge(`. Use exact function-identifier boundary to avoid partial matches:

```bash
for name in invulnerable burn poison regen dodge_boost attack_boost \
            defense_boost haggle thick_skin flee cozy; do
    old="make_${name}("
    new="make_${name}_ge("
    # Project-wide replace, only under src/ and include/
    find src include -type f \( -name "*.cpp" -o -name "*.h" \) -print0 \
        | xargs -0 sed -i '' "s/${old}/${new}/g"
done
```

(On Linux, `sed -i''` not `-i ''`.) After running, confirm zero hits remain for the old names:

```bash
grep -rE 'make_(invulnerable|burn|poison|regen|dodge_boost|attack_boost|defense_boost|haggle|thick_skin|flee|cozy)\(' src include
```

Expected: only hits are the newly renamed `make_*_ge(` ones from the substitution above (which the regex won't match since the `(` now follows `_ge`), effectively zero output.

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: clean build. The rename is pure syntactic — any linker error means a call site was missed.

- [ ] **Step 5: Smoke — dev console triggers still work**

Run: `./build/astra --term`, open dev console (`~`), run `effect regen 5`. Expected: Regen chip appears in the effect bar. This exercises `make_regen_ge` end-to-end.

- [ ] **Step 6: Commit**

```bash
git add include/astra/effect.h src/effect.cpp src/ability.cpp \
        src/character_screen.cpp src/dev_console.cpp \
        src/dialog_manager.cpp src/game.cpp src/game_world.cpp \
        src/save_file.cpp src/npcs/
git commit -m "refactor(effects): rename make_* factories to make_*_ge"
```

---

## Task 2: Scaffolding — `Aura`, `AuraSource`, `AuraTarget`, empty registries

**Files:**
- Create: `include/astra/aura.h`
- Create: `src/aura.cpp`
- Modify: `CMakeLists.txt` (add `src/aura.cpp` to the source list)

- [ ] **Step 1: Write `include/astra/aura.h`**

Full file contents:

```cpp
#pragma once

#include "astra/effect.h"
#include "astra/tilemap.h"    // FixtureType, FixtureTag, FixtureData
#include "astra/skill_defs.h" // SkillId

#include <cstdint>
#include <vector>

namespace astra {

// Source of a player/NPC aura. Drives surgical removal when the
// originating system changes state (item unequip, effect expire, etc.).
enum class AuraSource : uint8_t {
    Manual  = 0,  // dev console / scripting; survives save-load
    Item,         // source_id = Item::id
    Effect,       // source_id = static_cast<uint32_t>(EffectId)
    Skill,        // source_id = static_cast<uint32_t>(SkillId)
    Fixture,      // not stored on entities; reserved for clarity
};

// Receiver classes — bitflags; combine with |.
namespace AuraTarget {
    constexpr uint32_t Player      = 1u << 0;
    constexpr uint32_t FriendlyNpc = 1u << 1;
    constexpr uint32_t HostileNpc  = 1u << 2;
    constexpr uint32_t AllNpcs     = FriendlyNpc | HostileNpc;
    constexpr uint32_t Everyone    = Player | AllNpcs;
}

struct Aura {
    Effect     template_effect;                       // duration baked in
    int        radius       = 1;                      // Chebyshev
    uint32_t   target_mask  = AuraTarget::Player;
    AuraSource source       = AuraSource::Manual;
    uint32_t   source_id    = 0;
};

// Fixture aura registry — union of tag-derived and type-specific.
std::vector<Aura> auras_for(const FixtureData& fd);

// Skill aura registry — called by rebuild_auras_from_sources.
std::vector<Aura> skill_auras(SkillId id);

struct Player;
struct Npc;

// Wipe all non-Manual entries from entity.auras and re-populate from
// items (player), effects, and skills (player). NPCs only re-derive
// effect-sourced auras for now (no equipment or skills).
void rebuild_auras_from_sources(Player& p);
void rebuild_auras_from_sources(Npc& n);

} // namespace astra
```

- [ ] **Step 2: Write `src/aura.cpp`**

Full file contents — registries stay empty until Task 4:

```cpp
#include "astra/aura.h"

#include "astra/item.h"
#include "astra/npc.h"
#include "astra/player.h"

namespace astra {

// ── Fixture aura registries ────────────────────────────────────────

// Tag-driven: any fixture whose `tags` includes this FixtureTag emits
// the listed auras. Keep this list small and generic — prefer type
// auras for anything fixture-specific.
static const std::vector<std::pair<FixtureTag, Aura>>& tag_auras() {
    static const std::vector<std::pair<FixtureTag, Aura>> table = {
        // populated by later tasks
    };
    return table;
}

// Type-specific: exact FixtureType → auras. Use this when a fixture
// emits something its tag class shouldn't universally emit.
static const std::vector<std::pair<FixtureType, std::vector<Aura>>>& type_auras() {
    static const std::vector<std::pair<FixtureType, std::vector<Aura>>> table = {
        // populated by later tasks
    };
    return table;
}

std::vector<Aura> auras_for(const FixtureData& fd) {
    std::vector<Aura> out;
    for (const auto& [tag, aura] : tag_auras()) {
        if (fixture_has_tag(fd, tag)) out.push_back(aura);
    }
    for (const auto& [type, list] : type_auras()) {
        if (fd.type == type) {
            for (const auto& a : list) out.push_back(a);
        }
    }
    return out;
}

// ── Skill aura registry ────────────────────────────────────────────

std::vector<Aura> skill_auras(SkillId /*id*/) {
    // No skill-sourced auras yet.
    return {};
}

// ── Rebuild from sources ───────────────────────────────────────────

namespace {

void strip_non_manual(std::vector<Aura>& v) {
    v.erase(std::remove_if(v.begin(), v.end(),
              [](const Aura& a) { return a.source != AuraSource::Manual; }),
            v.end());
}

} // anonymous

void rebuild_auras_from_sources(Player& /*p*/) {
    // Wired in Task 6 once the vector field exists on Player.
}

void rebuild_auras_from_sources(Npc& /*n*/) {
    // Wired in Task 6 once the vector field exists on Npc.
}

} // namespace astra
```

Add `#include <algorithm>` at the top if the compiler complains about `std::remove_if` — keep it out otherwise.

- [ ] **Step 3: Register `src/aura.cpp` in CMake**

Open `CMakeLists.txt`. Find the existing `add_executable(astra ...)` (or the project's source-list variable). Add `src/aura.cpp` in the same place other `src/*.cpp` are listed. Example (adjust to actual syntax):

```cmake
    src/ability.cpp
    src/animation.cpp
    src/aura.cpp        # NEW
    src/body_presets.cpp
    ...
```

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: clean build. `src/aura.cpp` should show up as a compiled object.

- [ ] **Step 5: Commit**

```bash
git add include/astra/aura.h src/aura.cpp CMakeLists.txt
git commit -m "feat(aura): scaffold Aura struct + empty registries"
```

---

## Task 3: `AuraSystem` skeleton + wire into `advance_world`

**Files:**
- Create: `include/astra/aura_system.h`
- Create: `src/aura_system.cpp`
- Modify: `CMakeLists.txt`
- Modify: `include/astra/game.h` (add `AuraSystem aura_system_` member)
- Modify: `src/game_world.cpp` (call `aura_system_.tick(*this)` inside `advance_world`)

- [ ] **Step 1: Write `include/astra/aura_system.h`**

```cpp
#pragma once

namespace astra {

class Game;

// Scans every emitter on the current map and applies their auras to
// receivers in range. Run once per world tick inside advance_world,
// after tick_effects + expire_effects and before passive regen.
class AuraSystem {
public:
    void tick(Game& game);
};

} // namespace astra
```

- [ ] **Step 2: Write `src/aura_system.cpp` — skeleton only**

```cpp
#include "astra/aura_system.h"

#include "astra/aura.h"
#include "astra/effect.h"
#include "astra/faction.h"
#include "astra/game.h"
#include "astra/npc.h"
#include "astra/player.h"
#include "astra/tilemap.h"

#include <algorithm>

namespace astra {

namespace {

bool receiver_matches_mask(uint32_t mask,
                           bool is_player,
                           bool is_hostile) {
    if (is_player) return (mask & AuraTarget::Player) != 0;
    if (is_hostile) return (mask & AuraTarget::HostileNpc) != 0;
    return (mask & AuraTarget::FriendlyNpc) != 0;
}

// Apply all of `auras` emitted from (ex, ey) to in-range receivers.
// `self_npc` (if not null) is skipped to prevent self-application from
// NPC emitters; the player self-exclusion is handled at the call site.
void apply_auras_at(const std::vector<Aura>& auras,
                    int ex, int ey,
                    Game& game,
                    bool emitter_is_player,
                    Npc* self_npc) {
    if (auras.empty()) return;

    auto& player = game.player();
    auto& npcs   = game.world().npcs();

    for (const Aura& a : auras) {
        const int r = a.radius;
        if (r <= 0) continue;

        // Player receiver
        if (!emitter_is_player) {
            int dx = std::abs(player.x - ex);
            int dy = std::abs(player.y - ey);
            if (std::max(dx, dy) <= r
                && receiver_matches_mask(a.target_mask, /*is_player*/true, /*hostile*/false)) {
                add_effect(player.effects, a.template_effect);
            }
        }

        // NPC receivers
        for (auto& npc : npcs) {
            if (!npc.alive()) continue;
            if (&npc == self_npc) continue;
            int dx = std::abs(npc.x - ex);
            int dy = std::abs(npc.y - ey);
            if (std::max(dx, dy) > r) continue;
            bool hostile = is_hostile_to_player(npc.faction, player);
            if (!receiver_matches_mask(a.target_mask, /*is_player*/false, hostile)) continue;
            add_effect(npc.effects, a.template_effect);
        }
    }
}

} // anonymous

void AuraSystem::tick(Game& game) {
    auto& map = game.world().map();

    // 1) Fixture emitters — iterate the map once, look up auras per fixture.
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            int fid = map.fixture_id(x, y);
            if (fid < 0) continue;
            const auto& fd = map.fixture(fid);
            auto auras = auras_for(fd);
            apply_auras_at(auras, x, y, game, /*emitter_is_player*/false, /*self_npc*/nullptr);
        }
    }

    // 2) Player emitter
    auto& player = game.player();
    apply_auras_at(player.auras, player.x, player.y, game,
                   /*emitter_is_player*/true, /*self_npc*/nullptr);

    // 3) NPC emitters
    for (auto& npc : game.world().npcs()) {
        if (!npc.alive()) continue;
        apply_auras_at(npc.auras, npc.x, npc.y, game,
                       /*emitter_is_player*/false, /*self_npc*/&npc);
    }
}

} // namespace astra
```

**Note:** `player.auras` and `npc.auras` fields don't exist yet — they're added in Task 6. Until then, the compiler will fail on those lines. We fix this in Step 3 by temporarily commenting those two blocks.

- [ ] **Step 3: Comment out the entity-emitter blocks until Task 6**

In `src/aura_system.cpp`, comment out the Player and NPC emitter blocks (steps 2 and 3 of `tick`) with a `TODO(task-6)` marker:

```cpp
    // 2) Player emitter — wired in Task 6 (player.auras field).
    // apply_auras_at(player.auras, player.x, player.y, game,
    //                /*emitter_is_player*/true, /*self_npc*/nullptr);

    // 3) NPC emitters — wired in Task 6 (npc.auras field).
    // for (auto& npc : game.world().npcs()) { ... }
```

Keep the fixture-emitter loop active — it only uses `auras_for(fd)` which is already valid.

- [ ] **Step 4: Register `src/aura_system.cpp` in CMake**

Add `src/aura_system.cpp` to the source list next to `src/aura.cpp`.

- [ ] **Step 5: Add `AuraSystem` to `Game`**

In `include/astra/game.h`, add the include near the other astra headers:

```cpp
#include "astra/aura_system.h"
```

Add a private member. Find the private section (`private:` at line ~179) and add:

```cpp
    AuraSystem aura_system_;
```

- [ ] **Step 6: Call `aura_system_.tick(*this)` inside `advance_world`**

In `src/game_world.cpp`, inside `Game::advance_world`, locate the block (currently ~line 2151) immediately after the effect tick/expire loop:

```cpp
    for (auto& npc : world_.npcs()) {
        if (npc.alive()) {
            tick_effects(npc.effects, npc.hp, npc.max_hp);
            expire_effects(npc.effects);
        }
    }
```

Immediately after that block, insert:

```cpp
    // Aura system — emitters push GEs to receivers in range. Runs after
    // effect tick/expire so duration=1 auras are cleanly refreshed here.
    aura_system_.tick(*this);
```

- [ ] **Step 7: Build**

Run: `cmake --build build -j`
Expected: clean build. `AuraSystem::tick` compiles; no behaviour change (fixture auras table is empty, entity blocks are commented).

- [ ] **Step 8: Smoke — existing Cozy still works (inline scan is still live)**

Run: `./build/astra --term`, build a campfire via dev console (`give skill CampMaking` + use it). Stand adjacent. Expected: **Cozy still applies** via the existing inline scan in `advance_world` — the new `AuraSystem::tick` is a no-op next to it. This proves insertion is non-disruptive.

- [ ] **Step 9: Commit**

```bash
git add include/astra/aura_system.h src/aura_system.cpp CMakeLists.txt \
        include/astra/game.h src/game_world.cpp
git commit -m "feat(aura): AuraSystem skeleton wired into advance_world"
```

---

## Task 4: Port Cozy — register Campfire in `type_auras`, delete inline scan

**Files:**
- Modify: `src/aura.cpp` (register Campfire → Cozy in `type_auras()`)
- Modify: `src/game_world.cpp` (delete inline Cozy block from `advance_world`)

- [ ] **Step 1: Register Campfire → Cozy in `type_auras()`**

In `src/aura.cpp`, replace the empty `type_auras()` table with:

```cpp
static const std::vector<std::pair<FixtureType, std::vector<Aura>>>& type_auras() {
    static const std::vector<std::pair<FixtureType, std::vector<Aura>>> table = [] {
        std::vector<std::pair<FixtureType, std::vector<Aura>>> t;

        // Campfire: emits Cozy to the player within 6 tiles. Cozy is
        // deliberately campfire-specific (not HeatSource-wide), so it
        // lives in type_auras rather than tag_auras.
        Aura cozy;
        cozy.template_effect = make_cozy_ge();   // duration=1 baked in
        cozy.radius          = astra::world::cozy_radius;
        cozy.target_mask     = AuraTarget::Player;
        cozy.source          = AuraSource::Fixture;
        cozy.source_id       = 0;
        t.push_back({FixtureType::Campfire, {cozy}});

        return t;
    }();
    return table;
}
```

Add includes at the top of `src/aura.cpp` if not already present:

```cpp
#include "astra/world_constants.h"
```

- [ ] **Step 2: Delete the inline Cozy scan in `advance_world`**

Open `src/game_world.cpp`. Locate the block (currently around line 2141–2194) that begins with:

```cpp
    // ── Camp Making: expire time-limited fixtures, apply Cozy aura ──
    {
        auto& map = world_.map();
        const int tick = world_.world_tick();

        // 1) Sweep the current map for expired time-limited fixtures.
        ...
        // 2) Proximity scan — if the player stands within cozy_radius ...
        ...
        if (near_fire) {
            add_effect(player_.effects, make_cozy_ge());
        }
    }
```

**Delete only the proximity scan (part 2), leaving the fixture-expiry sweep (part 1) in place** — Task 5 extracts the sweep cleanly. The result should look like:

```cpp
    // ── Camp Making: expire time-limited fixtures ──
    {
        auto& map = world_.map();
        const int tick = world_.world_tick();

        // Sweep the current map for expired time-limited fixtures.
        // Currently only FixtureType::Campfire uses spawn_tick.
        for (int y = 0; y < map.height(); ++y) {
            for (int x = 0; x < map.width(); ++x) {
                int fid = map.fixture_id(x, y);
                if (fid < 0) continue;
                const auto& fd = map.fixture(fid);
                if (fd.spawn_tick < 0) continue;
                if (tick - fd.spawn_tick >= world::campfire_lifetime_ticks) {
                    map.remove_fixture(x, y);
                }
            }
        }
    }
```

(The Cozy is now applied by `AuraSystem::tick` that runs right before this block; we preserve the expiry loop here for now and extract it in Task 5.)

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 4: Smoke — Cozy now delivered by AuraSystem**

Run: `./build/astra --term`. Dev console: `give skill CampMaking`, use the ability, stand adjacent to the fire. Expected: Cozy chip appears in effect bar exactly as before. Step 7+ tiles away: Cozy disappears on the next tick. Behavior identical to pre-task but the source is now the aura registry, not the inline scan.

Edge case: stand adjacent to **two** campfires. Expected: still just one Cozy chip (add_effect replaces by id).

- [ ] **Step 5: Commit**

```bash
git add src/aura.cpp src/game_world.cpp
git commit -m "feat(aura): port Cozy to AuraSystem, remove inline scan"
```

---

## Task 5: Extract campfire expiry into `TileMap::sweep_expired_fixtures`

**Files:**
- Modify: `include/astra/tilemap.h` (declare method)
- Modify: `src/tilemap.cpp` (implement method)
- Modify: `src/game_world.cpp` (replace inline loop with call)

- [ ] **Step 1: Declare `sweep_expired_fixtures` on `TileMap`**

In `include/astra/tilemap.h`, inside `class TileMap`, near other fixture methods (there's `remove_fixture(int, int)`), add:

```cpp
    // Remove any time-limited fixture (spawn_tick >= 0) that has exceeded
    // its lifetime. `lifetime_ticks` is compared against
    // `current_tick - spawn_tick`. Only call on the currently-active map.
    void sweep_expired_fixtures(int current_tick, int lifetime_ticks);
```

- [ ] **Step 2: Implement in `src/tilemap.cpp`**

Add anywhere in `src/tilemap.cpp` — a natural spot is near `remove_fixture`:

```cpp
void TileMap::sweep_expired_fixtures(int current_tick, int lifetime_ticks) {
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            int fid = fixture_id(x, y);
            if (fid < 0) continue;
            const auto& fd = fixtures_[fid];
            if (fd.spawn_tick < 0) continue;
            if (current_tick - fd.spawn_tick >= lifetime_ticks) {
                remove_fixture(x, y);
            }
        }
    }
}
```

- [ ] **Step 3: Replace inline sweep in `advance_world`**

In `src/game_world.cpp`, replace the Camp Making block currently reading:

```cpp
    // ── Camp Making: expire time-limited fixtures ──
    {
        auto& map = world_.map();
        const int tick = world_.world_tick();
        for (int y = 0; y < map.height(); ++y) {
            for (int x = 0; x < map.width(); ++x) {
                int fid = map.fixture_id(x, y);
                if (fid < 0) continue;
                const auto& fd = map.fixture(fid);
                if (fd.spawn_tick < 0) continue;
                if (tick - fd.spawn_tick >= world::campfire_lifetime_ticks) {
                    map.remove_fixture(x, y);
                }
            }
        }
    }
```

With a single call:

```cpp
    // Time-limited fixtures (e.g. campfires) — remove once expired.
    world_.map().sweep_expired_fixtures(world_.world_tick(),
                                        world::campfire_lifetime_ticks);
```

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 5: Smoke — campfire still expires after 150 ticks**

Run: `./build/astra --term`. Dev console: `give skill CampMaking`, build a fire. Wait 150+ ticks (press `.` rapidly or `wait` command). Expected: `^` vanishes from the map and Cozy stops reapplying.

- [ ] **Step 6: Commit**

```bash
git add include/astra/tilemap.h src/tilemap.cpp src/game_world.cpp
git commit -m "refactor(tilemap): extract sweep_expired_fixtures method"
```

---

## Task 6: Entity auras — `Player::auras`, `Npc::auras`, `Item`/`Effect::granted_auras`, rebuild

**Files:**
- Modify: `include/astra/effect.h` (add `granted_auras` field on `Effect`)
- Modify: `include/astra/item.h` (add `granted_auras` field on `Item`)
- Modify: `include/astra/player.h` (add `std::vector<Aura> auras`)
- Modify: `include/astra/npc.h` (add `std::vector<Aura> auras`)
- Modify: `src/aura.cpp` (implement `rebuild_auras_from_sources`)
- Modify: `src/game_rendering.cpp:546,569` (call rebuild after equip/unequip)
- Modify: `src/effect.cpp` (`add_effect` / `remove_effect` currently in effect.cpp — rebuild hooks if applicable)
- Modify: `src/aura_system.cpp` (un-comment the entity emitter blocks)

### Why one big task: the struct changes must land together so the build stays green.

- [ ] **Step 1: Add `granted_auras` field on `Effect`**

In `include/astra/effect.h`, inside `struct Effect`, add at the end of the field list (after `sell_price_pct` around line 59):

```cpp
    // Auras this effect contributes to its holder's `auras` vector. The
    // aggregator in rebuild_auras_from_sources owns adding/removing.
    std::vector<Aura> granted_auras;
```

Forward-declare `Aura` near the top of the file. `Aura` itself lives in `aura.h`, but `aura.h` already includes `effect.h` (for `Effect template_effect`), so we have a cycle. Break it: declare a forward at the top of `effect.h`:

```cpp
namespace astra { struct Aura; }
```

Include `<vector>` if not already present.

Wait — `granted_auras` is `std::vector<Aura>`, which requires the complete type. So forward-declaration isn't enough. Restructure: split `Aura` into `include/astra/aura.h` as planned, but move only the *struct definition* to a tiny header `include/astra/aura_fwd.h` that `effect.h` and `item.h` include, then `aura.h` includes both. Cleaner: move the `Aura` struct and enums into `aura.h`, and have `effect.h`/`item.h` include `aura.h` directly. That's fine because `aura.h` depends on `effect.h` (Effect field)... cycle.

**Resolution:** `Effect::granted_auras` cannot hold `Aura` by value if `Aura::template_effect` holds `Effect` by value (circular size). Break by holding `std::vector<Aura>` via `std::unique_ptr` or by splitting `Aura` to refer to a `MakeEffectFn*` — but the spec says template (pure data). Concretely: `Aura` contains `Effect` by value, so `Effect` cannot contain `Aura` by value.

**Fix:** change the representation. `Effect::granted_auras` becomes `std::vector<Aura>` stored in a separate side table instead of directly on the struct. Simpler: make `Effect::granted_auras` a `std::vector<Aura>` but break the cycle by storing `Aura*` pointers or by making `Aura::template_effect` a `std::unique_ptr<Effect>`.

**Simplest solution used here:** define a sibling struct `AuraGrant` that holds the aura *fields* (radius, target_mask, source, source_id, and a *factory* pointer to produce the `Effect`). `AuraGrant` is cycle-free, and `auras_for`/`rebuild` materialise real `Aura` objects on demand by calling the factory.

**Plan update inline:** deviate from the spec here — `Aura` keeps its full form (with `Effect template_effect`) because it's stored outside Effect/Item. `Effect::granted_auras` and `Item::granted_auras` use a new struct:

```cpp
// In include/astra/aura.h, near Aura:
struct AuraGrant {
    Effect (*make_effect)();             // factory — avoids Effect-in-Effect cycle
    int        radius       = 1;
    uint32_t   target_mask  = AuraTarget::Player;
};
```

And in `effect.h`:

```cpp
    std::vector<AuraGrant> granted_auras;   // copied into real Auras during rebuild
```

A side effect: fields in `granted_auras` use a function pointer, not the template. That's fine — grants are a narrow case (item/effect declares "I grant this aura"), and function pointers here avoid a massive refactor of Effect.

**Cycle-free layout:**

- `include/astra/aura.h` — `AuraGrant`, `AuraSource`, `AuraTarget`, `Aura`. (`Aura::template_effect` requires full `Effect`.)
- `include/astra/effect.h` — `Effect` (full) + `std::vector<AuraGrant> granted_auras` — only needs `AuraGrant`, which doesn't reference `Effect` by value.

So: `aura.h` includes `effect.h`. `effect.h` forward-declares or includes `aura_grant.h` if we split, or just declares `AuraGrant` inline near the top of `effect.h` (no `Effect` dependency).

**Concrete split to land in Step 1:** move `AuraGrant` into a tiny standalone header `include/astra/aura_grant.h` with no dependencies. Both `effect.h` and `item.h` include it.

Create `include/astra/aura_grant.h`:

```cpp
#pragma once

#include <cstdint>

namespace astra {

struct Effect;

// Declaration that a source (item, effect, skill) contributes one aura
// to its holder. Uses a factory rather than a stored Effect to avoid
// Effect-in-Effect cycles. The factory is invoked once per rebuild.
struct AuraGrant {
    Effect (*make_effect)() = nullptr;
    int      radius       = 1;
    uint32_t target_mask  = 1u;   // defaults to AuraTarget::Player
};

} // namespace astra
```

Now in `include/astra/effect.h` add:

```cpp
#include "astra/aura_grant.h"
```

and inside `struct Effect`, after the existing fields:

```cpp
    std::vector<AuraGrant> granted_auras;
```

- [ ] **Step 2: Add `granted_auras` field on `Item`**

In `include/astra/item.h`, add at top:

```cpp
#include "astra/aura_grant.h"
```

Inside `struct Item` (around line 187), after `enhancements`:

```cpp
    std::vector<AuraGrant> granted_auras;   // auras contributed while equipped
```

- [ ] **Step 3: Add `auras` field on `Player`**

In `include/astra/player.h`, add at top:

```cpp
#include "astra/aura.h"
```

In `struct Player`, add after the `EffectList effects;` field (around line 84):

```cpp
    // Auras this entity currently emits. Rebuilt from equipment /
    // effects / skills by rebuild_auras_from_sources; Manual entries
    // persist untouched.
    std::vector<Aura> auras;
```

- [ ] **Step 4: Add `auras` field on `Npc`**

In `include/astra/npc.h`, add at top:

```cpp
#include "astra/aura.h"
```

In `struct Npc`, after `EffectList effects;` (around line 57):

```cpp
    std::vector<Aura> auras;
```

- [ ] **Step 5: Implement `rebuild_auras_from_sources` in `src/aura.cpp`**

Replace the two stub bodies with real implementations:

```cpp
namespace {

Aura materialise(const AuraGrant& g, AuraSource source, uint32_t source_id) {
    Aura a;
    a.template_effect = g.make_effect ? g.make_effect() : Effect{};
    a.radius          = g.radius;
    a.target_mask     = g.target_mask;
    a.source          = source;
    a.source_id       = source_id;
    return a;
}

void strip_non_manual(std::vector<Aura>& v) {
    v.erase(std::remove_if(v.begin(), v.end(),
              [](const Aura& a) { return a.source != AuraSource::Manual; }),
            v.end());
}

} // anonymous

void rebuild_auras_from_sources(Player& p) {
    strip_non_manual(p.auras);

    // Items — equipment slots
    auto add_item_grants = [&](const std::optional<Item>& it) {
        if (!it) return;
        for (const auto& g : it->granted_auras) {
            p.auras.push_back(materialise(g, AuraSource::Item, it->id));
        }
    };
    add_item_grants(p.equipment.face);
    add_item_grants(p.equipment.head);
    add_item_grants(p.equipment.body);
    add_item_grants(p.equipment.left_arm);
    add_item_grants(p.equipment.right_arm);
    add_item_grants(p.equipment.left_hand);
    add_item_grants(p.equipment.right_hand);
    add_item_grants(p.equipment.back);
    add_item_grants(p.equipment.feet);
    add_item_grants(p.equipment.thrown);
    add_item_grants(p.equipment.missile);
    add_item_grants(p.equipment.shield);

    // Effects
    for (const auto& e : p.effects) {
        for (const auto& g : e.granted_auras) {
            p.auras.push_back(materialise(g,
                                          AuraSource::Effect,
                                          static_cast<uint32_t>(e.id)));
        }
    }

    // Skills
    for (SkillId sid : p.learned_skills) {
        for (const Aura& sa : skill_auras(sid)) {
            Aura copy = sa;
            copy.source    = AuraSource::Skill;
            copy.source_id = static_cast<uint32_t>(sid);
            p.auras.push_back(std::move(copy));
        }
    }
}

void rebuild_auras_from_sources(Npc& n) {
    strip_non_manual(n.auras);

    // NPCs have no equipment / skills in the current model; only
    // effect-sourced auras for now.
    for (const auto& e : n.effects) {
        for (const auto& g : e.granted_auras) {
            n.auras.push_back(materialise(g,
                                          AuraSource::Effect,
                                          static_cast<uint32_t>(e.id)));
        }
    }
}
```

Ensure `<algorithm>` is included at the top of `src/aura.cpp`.

- [ ] **Step 6: Call rebuild on equip / unequip**

In `src/game_rendering.cpp`, at the end of `Game::equip_item` (line 546 body) and `Game::unequip_slot` (line 569 body), append:

```cpp
    rebuild_auras_from_sources(player_);
```

- [ ] **Step 7: Call rebuild on effect add / remove for the player**

In `src/effect.cpp`, `add_effect` and `remove_effect` are generic over `EffectList&` and don't know about the owning entity. We keep them untouched and instead rebuild at the **call sites that mutate the player's effects.**

Primary mutation paths:
- `src/game.cpp:801,1399,1403` — player effect adds.
- `src/character_screen.cpp:408,410` — haggle / thick skin apply.
- `src/dev_console.cpp:469,492,496,500` — dev effect adds.
- `src/dialog_manager.cpp:924` — invulnerable apply.
- `src/game_world.cpp` — after `expire_effects(player_.effects)` (to capture expirations).

Add one rebuild call immediately after each `add_effect(player_.effects, ...)` for the player (not for NPCs — NPCs currently carry no aura-granting effects; revisit if needed). The simplest pattern:

```cpp
add_effect(player_.effects, make_haggle_ge());
rebuild_auras_from_sources(player_);
```

And in `src/game_world.cpp`, after the player expire_effects call (around line 2144):

```cpp
    expire_effects(player_.effects);
    rebuild_auras_from_sources(player_);
```

For NPCs, effect mutations don't currently drive auras, so skip the rebuild calls there to avoid per-NPC overhead until we need it.

Include `astra/aura.h` in any file that gains a `rebuild_auras_from_sources(player_)` call.

- [ ] **Step 8: Un-comment the entity emitter blocks in `src/aura_system.cpp`**

Restore the player and NPC emitter blocks in `AuraSystem::tick`:

```cpp
    // 2) Player emitter
    auto& player = game.player();
    apply_auras_at(player.auras, player.x, player.y, game,
                   /*emitter_is_player*/true, /*self_npc*/nullptr);

    // 3) NPC emitters
    for (auto& npc : game.world().npcs()) {
        if (!npc.alive()) continue;
        apply_auras_at(npc.auras, npc.x, npc.y, game,
                       /*emitter_is_player*/false, /*self_npc*/&npc);
    }
```

- [ ] **Step 9: Build**

Run: `cmake --build build -j`
Expected: clean build. If `<optional>` or `<vector>` needs to be included in the new headers, add them.

- [ ] **Step 10: Smoke — rebuild wiring does no harm**

Run: `./build/astra --term`. Dev console:
- `give skill CampMaking`, `use ability campmaking` → Cozy appears. (Fixture path still works.)
- Open inventory, equip an item, unequip it. Expected: no crash. (`player.granted_auras` is empty on all current items; rebuild runs but adds nothing.)
- `effect regen 5` → Regen appears in effect bar. (No aura grants on Regen; just confirms the rebuild-after-add_effect call didn't break anything.)

- [ ] **Step 11: Commit**

```bash
git add include/astra/aura_grant.h include/astra/effect.h \
        include/astra/item.h include/astra/player.h include/astra/npc.h \
        src/aura.cpp src/aura_system.cpp src/game_rendering.cpp \
        src/game.cpp src/character_screen.cpp src/dev_console.cpp \
        src/dialog_manager.cpp src/game_world.cpp
git commit -m "feat(aura): entity auras + rebuild_from_sources"
```

---

## Task 7: Save / load — persist Manual-only entity auras, v42 → v43

**Files:**
- Modify: `include/astra/save_file.h` (`SAVE_FILE_VERSION = 43`)
- Modify: `src/save_file.cpp` (write/read blocks for Player and Npc effects)

- [ ] **Step 1: Bump the version**

In `include/astra/save_file.h` line 28:

```cpp
inline constexpr uint32_t SAVE_FILE_VERSION = 43;   // entity auras (Manual only)
```

- [ ] **Step 2: Write Manual-only auras for Player**

In `src/save_file.cpp`, locate the player-serialize block (grep for `p.regen_counter` around line 493 to anchor). After serializing existing fields and before the block ends, add:

```cpp
    // v43: manual-sourced auras (item/effect/skill-sourced re-derive on load)
    uint32_t manual_count = 0;
    for (const auto& a : p.auras) {
        if (a.source == AuraSource::Manual) ++manual_count;
    }
    w.write_u32(manual_count);
    for (const auto& a : p.auras) {
        if (a.source != AuraSource::Manual) continue;
        write_effect(w, a.template_effect);   // reuse existing effect serializer
        w.write_i32(a.radius);
        w.write_u32(a.target_mask);
        w.write_u32(a.source_id);             // usually 0 for Manual
    }
```

If `write_effect` doesn't exist, inline the fields that Effect already round-trips (see existing Player effect serialization for the pattern — match it field-for-field).

- [ ] **Step 3: Read Manual-only auras for Player**

In the player deserialize block, after reading the existing effect list, add:

```cpp
    // v43: manual-sourced auras; item/effect/skill auras rebuild after
    uint32_t manual_count = r.read_u32();
    p.auras.clear();
    p.auras.reserve(manual_count);
    for (uint32_t i = 0; i < manual_count; ++i) {
        Aura a;
        a.template_effect = read_effect(r);   // reuse existing effect reader
        a.radius          = r.read_i32();
        a.target_mask     = r.read_u32();
        a.source          = AuraSource::Manual;
        a.source_id       = r.read_u32();
        p.auras.push_back(std::move(a));
    }
```

- [ ] **Step 4: Do the same for NPCs**

Locate the NPC serialize/deserialize paths (grep `npc.effects` or `read_npc` / `write_npc`). Mirror Steps 2–3 for each NPC's `auras` vector. NPCs will almost always write `manual_count = 0` for now, but the format is ready for future NPCs with scripted Manual auras.

- [ ] **Step 5: Rebuild derived auras after load**

After the save-file reader finishes loading player + NPCs (find the top-level `load_save` / `load_file` function around the end of `src/save_file.cpp`), call rebuild for the player and every NPC:

```cpp
    rebuild_auras_from_sources(ms.player);
    for (auto& npc : ms.npcs) {
        rebuild_auras_from_sources(npc);
    }
```

Include `astra/aura.h` at the top of the file if not already included.

- [ ] **Step 6: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 7: Smoke — save/load roundtrip**

Run: `./build/astra --term`. With an existing v42 save, attempt to load — expected: rejected (schema mismatch is an error per pre-ship policy).

New game: build a campfire, stand next to it, save, quit, reload. Expected:
- Game loads cleanly.
- Campfire still present on the map.
- After one tick (press any move key), Cozy chip appears.

This proves: (a) the new save format round-trips, (b) Cozy re-derivation via `auras_for(Campfire)` on the fixture side still works after load, and (c) `rebuild_auras_from_sources` on the player does nothing harmful when `granted_auras` are empty on all current items/effects.

- [ ] **Step 8: Commit**

```bash
git add include/astra/save_file.h src/save_file.cpp
git commit -m "feat(save): persist manual entity auras (v43)"
```

---

## Task 8: Documentation

**Files:**
- Modify: `docs/formulas.md` (link aura section)
- Modify: `docs/roadmap.md` (if aura system has a line; otherwise skip)

- [ ] **Step 1: Document the aura system**

Append to `docs/formulas.md`:

```markdown

## Aura System

Fixtures, the player, and NPCs can emit gameplay effects to entities
in range each world tick. Auras are pure data:

```
Aura {
    template_effect   // Effect copied onto each receiver on apply
    radius            // Chebyshev
    target_mask       // Player | FriendlyNpc | HostileNpc (bitflags)
    source / source_id
}
```

Emission runs in `Game::advance_world` after effect tick/expire and
before passive regen. Each tick, every emitter's auras are applied to
every in-range receiver via `add_effect`, which replaces any existing
effect with the same `EffectId` — so repeated emission acts as a
refresh, and multiple overlapping emitters collapse to the most
recently written.

Default aura duration is 1 world tick, baked into the
`template_effect`; aura lifetime beyond a single tick is expressed as
a longer template duration (e.g. `duration = 5` gives a 5-tick grace
period after stepping out of range).

Cozy (campfire aura) is the first tenant: `FixtureType::Campfire`
emits Cozy to the player within 6 tiles. See
`docs/superpowers/specs/2026-04-23-aura-system-design.md` for full
design rationale.
```

- [ ] **Step 2: Commit**

```bash
git add docs/formulas.md
git commit -m "docs(aura): document aura system"
```

---

## Self-Review

### Spec coverage
Each section of `2026-04-23-aura-system-design.md` maps to a task:
- Data model (`Aura`, `AuraSource`, `AuraTarget`) → Task 2.
- Naming convention (`make_*_ge`) → Task 1.
- Fixture emitters (`auras_for`, `tag_auras`, `type_auras`) → Tasks 2 + 4.
- Entity emitters (`Player::auras`, `Npc::auras`, `rebuild_auras_from_sources`) → Task 6.
- AuraSystem tick → Tasks 3 + 6 (full version).
- Campfire expiry extraction → Task 5.
- Cozy migration → Task 4.
- Save/load → Task 7.
- File structure → Tasks 2, 3, 6.
- Phased rollout → matches the 8 tasks.

### Placeholder scan
No "TBD"/"TODO"/"fill in details" left. One intentional `TODO(task-6)` marker in Task 3 Step 3 that gets removed in Task 6 Step 8 — that's a plan-tracking marker, not a requirement gap.

### Type consistency
- `Aura` struct fields (`template_effect`, `radius`, `target_mask`, `source`, `source_id`) are consistent across Tasks 2, 3, 4, 6, 7.
- `AuraGrant` struct introduced in Task 6 Step 1 (necessary deviation from the spec to break the `Effect`↔`Aura` cycle) has fields used consistently in Task 6 Step 5 (`materialise`).
- `rebuild_auras_from_sources(Player&)` and `(Npc&)` signatures match between declaration (Task 2) and implementation (Task 6).
- `TileMap::sweep_expired_fixtures(int, int)` signature matches between declaration (Task 5 Step 1) and call site (Task 5 Step 3).
- `AuraSystem::tick(Game&)` signature consistent between Tasks 3 and 6.

### Deviation note
The spec says auras are "pure data (template_effect)" but `Effect::granted_auras` + `Item::granted_auras` cannot hold full `Aura` objects by value due to the cycle (`Aura` contains `Effect`, `Effect` would contain `Aura`). Task 6 introduces `AuraGrant` — a function-pointer-based sibling — for these grant-side declarations. Runtime `Aura` objects on `Player::auras` / `Npc::auras` remain pure data. This is a minor and well-contained compromise; I flagged it inline in Task 6 Step 1.
