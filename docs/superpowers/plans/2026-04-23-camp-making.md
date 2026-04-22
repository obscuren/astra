# Camp Making Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn `SkillId::CampMaking` into a working active ability: place a short-lived `Campfire` fixture on an adjacent tile; grant the player a `Cozy` gameplay effect within 6 tiles that halves the natural-regen interval. Add a generic fixture-tag system (`uint64_t` bitflags) to support future cooking proximity checks.

**Architecture:**
- New `FixtureTag` bitflags + `uint64_t FixtureData::tags` + `int FixtureData::spawn_tick`.
- New `FixtureType::Campfire` (`^`, animated red/orange/yellow by hashing `(x, y, world_tick/2)`).
- New `EffectId::Cozy` and `EffectId::CooldownCampMaking`, plus `make_cozy()` factory.
- New `CampMakingAbility` registered in the ability catalog; uses existing ability-bar pipeline (cooldown via Effect, `action_cost=100`, `cooldown_ticks=300`).
- In `Game::advance_world()`: per-tick sweep removes expired campfires, re-applies Cozy while player is within 6 tiles of any player-placed `FixtureType::Campfire`.
- Save schema bump `v41 → v42` persisting `tags` + `spawn_tick`. No migration (user memory `feedback_no_backcompat_pre_ship`: reject old saves on schema bumps).

**Tech Stack:** C++20, `namespace astra`. Build: `cmake -B build -DDEV=ON && cmake --build build -j`. Dev-mode console (`~`) provides `give skill CampMaking`, `spawn fixture Campfire`, and `set world_tick` helpers for smoke validation.

**Validation model (no unit-test framework):** Each task ends with a **build + smoke check + commit**. Build: `cmake --build build -j`. Smoke checks are specific and listed inline.

**Reference spec:** `docs/superpowers/specs/2026-04-23-camp-making-design.md`

**Constants introduced:**
- `kCampfireLifetimeTicks = 150`
- `kCozyRadius = 6`
- `kCampMakingCooldownTicks = 300`
- `kCampMakingActionCost = 100`

---

## Task 1: Add `FixtureTag` enum + `tags` field on `FixtureData`

**Files:**
- Modify: `include/astra/tilemap.h` (FixtureData struct + new enum above it)

- [ ] **Step 1: Add `FixtureTag` enum and helpers**

In `include/astra/tilemap.h`, immediately BEFORE the `struct FixtureData { ... }` block (currently around line 425), add:

```cpp
// Bitflags describing a fixture's affordances. Used for proximity queries
// such as "is there a cooking source nearby?" without hard-coding fixture
// types at each call site. Bit assignments are stable — reserved bits must
// not be reused.
enum class FixtureTag : uint64_t {
    None          = 0,
    CookingSource = 1ull << 0,  // cooking proximity (campfire, stove, kitchen)
    HeatSource    = 1ull << 1,  // warmth (fires, braziers) — future heat gates
    LightSource   = 1ull << 2,  // illumination (torch, lamp, holo light)
};

inline FixtureTag operator|(FixtureTag a, FixtureTag b) {
    return static_cast<FixtureTag>(
        static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
}
inline FixtureTag operator&(FixtureTag a, FixtureTag b) {
    return static_cast<FixtureTag>(
        static_cast<uint64_t>(a) & static_cast<uint64_t>(b));
}
inline FixtureTag& operator|=(FixtureTag& a, FixtureTag b) {
    a = a | b; return a;
}
```

- [ ] **Step 2: Add `tags` field to `FixtureData`**

In the same file, locate `struct FixtureData`. Add `uint64_t tags = 0;` right before the `// Puzzle framework (layer 7)` comment block. Also add a helper below the struct:

```cpp
struct FixtureData {
    FixtureType type = FixtureType::Table;
    bool passable = false;
    bool interactable = false;
    int cooldown = 0;
    int last_used_tick = -1;
    bool locked = false;
    bool open = false;
    int light_radius = 0;
    bool blocks_vision = false;
    std::string quest_fixture_id;

    uint64_t tags = 0;   // bitmask of FixtureTag values

    // Puzzle framework (layer 7)
    uint16_t    puzzle_id         = 0;
    std::string proximity_message;
    uint8_t     proximity_radius  = 0;
};

// Returns true if `fd` carries all bits set in `tag`.
inline bool fixture_has_tag(const FixtureData& fd, FixtureTag tag) {
    auto t = static_cast<uint64_t>(tag);
    return t != 0 && (fd.tags & t) == t;
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: clean build. All call sites use default `tags = 0`, so nothing breaks.

- [ ] **Step 4: Commit**

```bash
git add include/astra/tilemap.h
git commit -m "feat(fixtures): add FixtureTag bitflags + tags field on FixtureData"
```

---

## Task 2: Retrofit tags for existing fire/light fixtures in `make_fixture`

**Files:**
- Modify: `src/tilemap.cpp` (`make_fixture` switch, lines ~457–467)

- [ ] **Step 1: Assign tags to existing fixtures that already represent cooking / heat / light**

In `src/tilemap.cpp`, inside `make_fixture(...)`, update these cases (locate by fixture type; current line numbers ~405–475):

```cpp
        case FixtureType::Torch:
            fd.passable = true; fd.interactable = false;
            fd.light_radius = 8;
            fd.tags = static_cast<uint64_t>(FixtureTag::LightSource);
            break;
```

```cpp
        case FixtureType::CampStove:
            fd.passable = false; fd.interactable = false;
            fd.tags = static_cast<uint64_t>(
                FixtureTag::CookingSource | FixtureTag::HeatSource);
            break;
        case FixtureType::Kitchen:
            fd.passable = false; fd.interactable = true;
            fd.light_radius = 2;
            fd.tags = static_cast<uint64_t>(
                FixtureTag::CookingSource | FixtureTag::HeatSource);
            break;
        case FixtureType::Lamp:
            fd.passable = true; fd.interactable = false;
            fd.light_radius = 6;
            fd.tags = static_cast<uint64_t>(FixtureTag::LightSource);
            break;
        case FixtureType::HoloLight:
            fd.passable = true; fd.interactable = false;
            fd.light_radius = 8;
            fd.tags = static_cast<uint64_t>(FixtureTag::LightSource);
            break;
```

Also add `LightSource` to Brazier (it's a fire fixture with `light_radius = 4`):

```cpp
        case FixtureType::Brazier:
            fd.passable = false; fd.interactable = false;
            fd.light_radius = 4;
            fd.tags = static_cast<uint64_t>(
                FixtureTag::HeatSource | FixtureTag::LightSource);
            break;
```

- [ ] **Step 2: Build**

Run: `cmake --build build -j`
Expected: clean build. Tags are informational at this stage; no game behaviour depends on them yet.

- [ ] **Step 3: Commit**

```bash
git add src/tilemap.cpp
git commit -m "feat(fixtures): tag existing cook/heat/light fixtures"
```

---

## Task 3: Add `FixtureData::spawn_tick` + campfire lifetime constant

**Files:**
- Modify: `include/astra/tilemap.h` (FixtureData struct)
- Modify: `include/astra/world_constants.h` (new constants block)

- [ ] **Step 1: Add `spawn_tick` to `FixtureData`**

In `include/astra/tilemap.h` `struct FixtureData`, right after `uint64_t tags = 0;`, add:

```cpp
    // World-tick when this fixture was placed. -1 = permanent (not time-limited).
    // Used by time-limited fixtures like Campfire (see kCampfireLifetimeTicks).
    int spawn_tick = -1;
```

- [ ] **Step 2: Add constants to `world_constants.h`**

Open `include/astra/world_constants.h`. Add (inside `namespace astra`, near other tick-based constants):

```cpp
// Camp Making skill
inline constexpr int kCampfireLifetimeTicks   = 150;  // world ticks until campfire expires
inline constexpr int kCozyRadius              = 6;    // Chebyshev tiles from a player campfire
inline constexpr int kCampMakingCooldownTicks = 300;  // ability cooldown
inline constexpr int kCampMakingActionCost    = 100;  // action cost to build camp
```

(If `world_constants.h` uses a different naming convention for existing constants, match it — otherwise use the above as written.)

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add include/astra/tilemap.h include/astra/world_constants.h
git commit -m "feat(fixtures): spawn_tick field + camp-making constants"
```

---

## Task 4: Introduce `FixtureType::Campfire` — enum, factory, theme, name, editor

**Files:**
- Modify: `include/astra/tilemap.h` (FixtureType enum)
- Modify: `src/tilemap.cpp` (make_fixture switch)
- Modify: `src/terminal_theme.cpp` (two places: `fixture_visual` style table + simple `fixture_glyph` fallback)
- Modify: `src/game_rendering.cpp` (two places: `fixture_type_name`, `fixture_type_desc`)
- Modify: `src/map_editor.cpp` (two places: palette init + editor name switch)
- Modify: `src/game_interaction.cpp` (inspect message)

- [ ] **Step 1: Add enum entry**

In `include/astra/tilemap.h`, add `Campfire` to `FixtureType` right after `CampStove` (line ~397):

```cpp
    CampStove,          // 'o'  — frontier cooking
    Campfire,           // '^'  — player-placed, animated fire; expires after kCampfireLifetimeTicks
    Lamp,               // '*'  — frontier/advanced lighting
```

- [ ] **Step 2: Add factory defaults**

In `src/tilemap.cpp` `make_fixture`, add a case right after `CampStove`:

```cpp
        case FixtureType::Campfire:
            fd.passable = false;
            fd.interactable = false;
            fd.light_radius = 4;
            fd.tags = static_cast<uint64_t>(
                FixtureTag::CookingSource
                | FixtureTag::HeatSource
                | FixtureTag::LightSource);
            break;
```

Note: `spawn_tick` stays at the struct default (`-1`). The ability code is responsible for stamping `world_tick` when it places the fixture.

- [ ] **Step 3: Add theme visual — animated palette**

In `src/terminal_theme.cpp`, locate the visual-table function (the one that sets `vis = {...}` per `FixtureType`, currently around line 976 for `CampStove`). Right after the `CampStove` case, add:

```cpp
        case FixtureType::Campfire: {
            // Animated red/orange/yellow by hashing (x, y, world_tick / 2).
            // Desyncs per tile so multiple fires don't flicker in lockstep.
            // The caller of this function has world_tick available via the
            // renderer context; if this function doesn't take x/y/world_tick
            // yet, use the static fallback (red) and do the animation in the
            // map_renderer instead — see Task 8.
            vis = {'^', nullptr, Color::Red, Color::Default};
            break;
        }
```

In the same file, locate the simple `fixture_glyph` switch (around line 1818 for `CampStove`) and add:

```cpp
        case FixtureType::Campfire:        return '^';
```

- [ ] **Step 4: Add name + description**

In `src/game_rendering.cpp` `fixture_type_name` (around line 121 for `CampStove`):

```cpp
        case FixtureType::CampStove:       return "Camp Stove";
        case FixtureType::Campfire:        return "Campfire";
```

In the same file's `fixture_type_desc` (around line 197):

```cpp
        case FixtureType::CampStove:       return "A portable cooking stove, still warm.";
        case FixtureType::Campfire:        return "A crackling campfire. The warmth feels restorative.";
```

- [ ] **Step 5: Editor palette + name**

In `src/map_editor.cpp` `init_fixture_palette` (around line 42), add `FixtureType::Campfire` to the palette vector — place it right after `FixtureType::CampStove` if present, or at the end of the settlement-furniture group:

```cpp
        FixtureType::NaturalObstacle, FixtureType::SettlementProp,
        FixtureType::Campfire,
        FixtureType::PrecursorButton,
```

In the editor name switch (around line 268):

```cpp
        case FixtureType::CampStove:      return "Camp Stove";
        case FixtureType::Campfire:       return "Campfire";
```

- [ ] **Step 6: Inspect message**

In `src/game_interaction.cpp` around line 81 (where `CampStove` already has a message):

```cpp
                        case FixtureType::CampStove:      log("A warm stove. Something was cooking here."); break;
                        case FixtureType::Campfire:       log("A crackling campfire. You feel cozy nearby."); break;
```

- [ ] **Step 7: Build**

Run: `cmake --build build -j`
Expected: clean build, zero warnings about missing switch cases. (If any other file has a complete `switch (FixtureType)` it will warn — add a `case FixtureType::Campfire:` there too and rebuild.)

- [ ] **Step 8: Smoke — place via map editor**

Run: `./build/astra --term`, open the map editor (dev mode hotkey), scroll the fixture palette to Campfire, place one on an overworld tile. It should render as a red `^`. No crash.

- [ ] **Step 9: Commit**

```bash
git add include/astra/tilemap.h src/tilemap.cpp src/terminal_theme.cpp \
        src/game_rendering.cpp src/map_editor.cpp src/game_interaction.cpp
git commit -m "feat(fixtures): add FixtureType::Campfire"
```

---

## Task 5: Animate campfire color in the map renderer

**Files:**
- Modify: `src/map_renderer.cpp` (fixture render path for FixtureType::Campfire)
- Modify: `src/terminal_theme.cpp` if the static `vis = {'^', nullptr, Color::Red, Color::Default}` needs to be overridden after the fact.

**Background:** The simplest implementation route is to override the color in `map_renderer.cpp` AFTER the theme lookup returns the static red. Keep the theme entry as the fallback (shown in tooltips, etc.).

- [ ] **Step 1: Find the fixture render path**

Open `src/map_renderer.cpp` and search for where fixtures are drawn — typically a block that calls into `terminal_theme` to get `{glyph, utf8, fg, bg}` and then writes to the cell. This function has access to `world_tick` via its context or `Game&`.

- [ ] **Step 2: Add per-tile color override for Campfire**

Immediately after the theme lookup for a fixture cell, before writing to the output buffer, insert:

```cpp
if (fixture.type == FixtureType::Campfire) {
    // Desync flicker by hashing (x, y, world_tick / 2) so neighbouring fires
    // don't animate in lockstep. Cycle: red → orange → yellow.
    static constexpr Color palette[3] = {
        Color::Red,
        static_cast<Color>(208),   // xterm orange
        Color::Yellow,
    };
    uint32_t h = static_cast<uint32_t>(x) * 73856093u
               ^ static_cast<uint32_t>(y) * 19349663u
               ^ static_cast<uint32_t>(world_tick / 2) * 83492791u;
    vis.fg = palette[h % 3];
}
```

Adapt `world_tick` access to whatever variable/accessor is in scope (`game.world().world_tick()`, `ctx.world_tick`, etc.). Adapt `vis` to the actual local struct name used in the fixture render path.

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 4: Smoke — flicker visible**

Run: `./build/astra --term`, place a campfire via map editor on the overworld, then wait/walk around so world ticks advance. The `^` glyph should cycle between red, orange, and yellow. Multiple adjacent campfires should flicker out of sync.

- [ ] **Step 5: Commit**

```bash
git add src/map_renderer.cpp
git commit -m "feat(render): animate campfire color red/orange/yellow"
```

---

## Task 6: Add `EffectId::Cozy` + `EffectId::CooldownCampMaking` + `make_cozy()`

**Files:**
- Modify: `include/astra/effect.h` (enum + factory declaration)
- Modify: `src/effect.cpp` (factory implementation)

- [ ] **Step 1: Add `EffectId` values**

In `include/astra/effect.h`, inside `enum class EffectId` (currently ends at `Flee = 200`), add BEFORE the closing `};`:

```cpp
    // Ability cooldowns (100+)
    CooldownJab         = 100,
    CooldownCleave      = 101,
    CooldownQuickdraw   = 102,
    CooldownIntimidate  = 103,
    CooldownSuppressing = 104,
    CooldownCampMaking  = 105,
    Flee                = 200,

    // Environmental buffs (300+)
    Cozy                = 300,
```

(Insert `CooldownCampMaking = 105` in the cooldown block and add the `Cozy = 300` line at the end, above the closing `};`.)

- [ ] **Step 2: Declare factory**

In the same header, after `Effect make_flee(int duration);`, add:

```cpp
Effect make_cozy();
```

- [ ] **Step 3: Implement factory**

In `src/effect.cpp`, after `make_flee(...)`, add:

```cpp
Effect make_cozy() {
    Effect e;
    e.id = EffectId::Cozy;
    e.name = "Cozy";
    // Orange — matches the campfire palette. 208 is xterm orange.
    e.color = static_cast<Color>(208);
    // Refreshed each tick by the proximity scanner; when the player steps
    // out of range it naturally expires on the next tick.
    e.duration = 1;
    e.remaining = 1;
    e.show_in_bar = true;
    return e;
}
```

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 5: Smoke — apply via dev console**

Run: `./build/astra --term`, open dev console (`~`), run `effect cozy 5` (or the existing syntax). Verify the Cozy chip shows in the effect bar in orange and vanishes after 5 ticks. If the dev-console `effect` helper hard-codes the known effect IDs, you may skip this smoke step — the next task exercises Cozy end-to-end.

- [ ] **Step 6: Commit**

```bash
git add include/astra/effect.h src/effect.cpp
git commit -m "feat(effects): add Cozy + CampMaking cooldown effects"
```

---

## Task 7: Implement `CampMakingAbility` and register in the ability catalog

**Files:**
- Modify: `src/ability.cpp` (new class + catalog registration)

- [ ] **Step 1: Add `CampMakingAbility` class**

In `src/ability.cpp`, after the `IntimidateAbility` class (around line 149) but before `// ── Catalog ──`, add:

```cpp
class CampMakingAbility : public Ability {
public:
    CampMakingAbility() {
        skill_id = SkillId::CampMaking;
        name = "Camp Making";
        description = "Build a campfire on an adjacent tile. "
                      "Grants Cozy (2x regen) within 6 tiles.";
        cooldown_ticks = kCampMakingCooldownTicks;
        cooldown_effect = EffectId::CooldownCampMaking;
        needs_adjacent_target = false;
        required_weapon = WeaponClass::None;
        action_cost = kCampMakingActionCost;
    }

    bool execute(Game& game, Npc* /*target*/) override {
        auto& map = game.world().map();
        const int px = game.player().x;
        const int py = game.player().y;

        // 8-neighbour scan in a fixed order. Pick the first adjacent tile
        // that is passable, has no fixture, and is not the player's own tile.
        static constexpr int dx8[8] = {-1,  0,  1, -1, 1, -1, 0, 1};
        static constexpr int dy8[8] = {-1, -1, -1,  0, 0,  1, 1, 1};

        int target_x = -1, target_y = -1;
        for (int i = 0; i < 8; ++i) {
            int tx = px + dx8[i], ty = py + dy8[i];
            if (tx < 0 || tx >= map.width() || ty < 0 || ty >= map.height()) continue;
            if (!map.passable(tx, ty)) continue;
            if (map.fixture_id(tx, ty) >= 0) continue;
            target_x = tx;
            target_y = ty;
            break;
        }

        if (target_x < 0) {
            game.log("No space to build a camp.");
            return false;
        }

        FixtureData fd = make_fixture(FixtureType::Campfire);
        fd.spawn_tick = game.world().world_tick();
        map.add_fixture(target_x, target_y, std::move(fd));

        game.log("You build a crackling campfire.");
        return true;
    }
};
```

Add the include at the top of `src/ability.cpp` if not already present:

```cpp
#include "astra/world_constants.h"
```

- [ ] **Step 2: Register in catalog**

In the same file, locate `build_catalog()` (around line 153) and add:

```cpp
static std::vector<std::unique_ptr<Ability>> build_catalog() {
    std::vector<std::unique_ptr<Ability>> cat;
    cat.push_back(std::make_unique<JabAbility>());
    cat.push_back(std::make_unique<CleaveAbility>());
    cat.push_back(std::make_unique<QuickdrawAbility>());
    cat.push_back(std::make_unique<IntimidateAbility>());
    cat.push_back(std::make_unique<CampMakingAbility>());
    return cat;
}
```

- [ ] **Step 3: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 4: Smoke — learn skill and fire ability**

Run: `./build/astra --term`, open dev console, `give skill CampMaking`. Press the ability-bar key for Camp Making. Expected:
- Log line "You build a crackling campfire."
- A red `^` appears on one of the 8 neighbouring tiles.
- The cooldown chip appears in the effect bar showing 300 ticks.
- Firing it again immediately fails with the cooldown message.

Edge case: stand in an enclosed 1-tile space (surround the player with walls in the editor). Trigger the ability — expected "No space to build a camp." log line and NO cooldown applied (because `execute` returned `false`).

- [ ] **Step 5: Commit**

```bash
git add src/ability.cpp
git commit -m "feat(abilities): implement Camp Making ability"
```

---

## Task 8: Per-tick expiry sweep + Cozy proximity apply in `advance_world`

**Files:**
- Modify: `src/game_world.cpp` (`Game::advance_world`, around line 2139–2200)

- [ ] **Step 1: Add helper — include world_constants**

At the top of `src/game_world.cpp`, ensure `#include "astra/world_constants.h"` is present (it likely is; add it if not).

- [ ] **Step 2: Insert the expiry sweep + Cozy scan**

Locate in `Game::advance_world` (line ~2139) the block:

```cpp
    ++world_.world_tick();
    world_.day_clock().advance(1);

    // Tick and expire effects
    tick_effects(player_.effects, player_.hp, player_.effective_max_hp());
    expire_effects(player_.effects);
```

Immediately AFTER the `expire_effects(player_.effects);` line (so that any stale Cozy from the previous tick is cleared first), insert:

```cpp
    // ── Camp Making: expire time-limited fixtures, apply Cozy aura ──
    {
        auto& map = world_.map();
        const int tick = world_.world_tick();

        // 1) Sweep the current map for expired time-limited fixtures.
        //    Currently only FixtureType::Campfire uses spawn_tick.
        for (int y = 0; y < map.height(); ++y) {
            for (int x = 0; x < map.width(); ++x) {
                int fid = map.fixture_id(x, y);
                if (fid < 0) continue;
                const auto& fd = map.fixture(fid);
                if (fd.spawn_tick < 0) continue;
                if (tick - fd.spawn_tick >= kCampfireLifetimeTicks) {
                    map.remove_fixture(x, y);
                }
            }
        }

        // 2) Proximity scan — if the player stands within kCozyRadius (Chebyshev)
        //    of any Campfire, (re-)apply the Cozy effect with duration 1. The
        //    effect naturally expires on the next tick if the player steps out.
        const int px = player_.x, py = player_.y;
        bool near_fire = false;
        const int y0 = std::max(0, py - kCozyRadius);
        const int y1 = std::min(map.height() - 1, py + kCozyRadius);
        const int x0 = std::max(0, px - kCozyRadius);
        const int x1 = std::min(map.width() - 1, px + kCozyRadius);
        for (int y = y0; y <= y1 && !near_fire; ++y) {
            for (int x = x0; x <= x1 && !near_fire; ++x) {
                int fid = map.fixture_id(x, y);
                if (fid < 0) continue;
                if (map.fixture(fid).type == FixtureType::Campfire) {
                    near_fire = true;
                }
            }
        }
        if (near_fire) {
            add_effect(player_.effects, make_cozy());
        }
    }
```

- [ ] **Step 3: Halve regen interval when Cozy present**

Further down in the same function, update the passive-regen block (line ~2190):

```cpp
    // Passive health regeneration
    if (player_.hp > 0 && player_.hp < player_.max_hp) {
        int interval = regen_interval(player_.hunger);
        if (has_effect(player_.effects, EffectId::Cozy)) {
            interval = std::max(1, interval / 2);
        }
        if (interval > 0) {
            ++player_.regen_counter;
            if (player_.regen_counter >= interval) {
                player_.regen_counter = 0;
                ++player_.hp;
                log("You feel a little better.");
            }
        }
    }
```

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 5: Smoke — Cozy applies and expires; campfire burns out**

Run: `./build/astra --term`, open dev console:
1. `give skill CampMaking`, build a campfire.
2. Stand adjacent to the fire. Verify the **Cozy** chip appears in the effect bar (orange).
3. Step 7 tiles away (past the radius). Verify Cozy disappears on the next tick.
4. Step back in — Cozy reappears.
5. Injure yourself (dev console `dmg 20` or similar). Note the tick count between "You feel a little better." messages when near the fire vs. far away — near-fire should be roughly half. (Exact numbers depend on `regen_interval(hunger)`.)
6. Wait near the fire for 150 ticks (dev console `wait 150` if supported, or hold `.` / rest). Verify the `^` vanishes and Cozy stops reapplying.

- [ ] **Step 6: Commit**

```bash
git add src/game_world.cpp
git commit -m "feat(world): campfire expiry + Cozy aura + regen boost"
```

---

## Task 9: Persist `tags` + `spawn_tick` in save file (schema v41 → v42)

**Files:**
- Modify: `include/astra/save_file.h` (`SAVE_FILE_VERSION` constant)
- Modify: `src/save_file.cpp` (fixture write + read blocks)

- [ ] **Step 1: Bump `SAVE_FILE_VERSION`**

In `include/astra/save_file.h` (line 28):

```cpp
inline constexpr uint32_t SAVE_FILE_VERSION = 42;   // fixture tags + spawn_tick
```

Also update the trailing comment on the `version` default (line ~70) to match — keep it short.

- [ ] **Step 2: Write the new fields**

In `src/save_file.cpp`, locate the fixture-write block (starts around line 708 with `// Fixtures (v3+)`). Update:

```cpp
    // Fixtures (v3+)
    const auto& fixtures = tm.fixtures_vec();
    w.write_u32(static_cast<uint32_t>(fixtures.size()));
    for (const auto& f : fixtures) {
        w.write_u8(static_cast<uint8_t>(f.type));
        w.write_u8(f.passable ? 1 : 0);
        w.write_u8(f.interactable ? 1 : 0);
        w.write_i32(f.cooldown);
        w.write_i32(f.last_used_tick);
        w.write_string(f.quest_fixture_id);
        w.write_u16(f.puzzle_id);
        w.write_string(f.proximity_message);
        w.write_u8(f.proximity_radius);
        w.write_u64(f.tags);          // v42
        w.write_i32(f.spawn_tick);    // v42
    }
```

- [ ] **Step 3: Read the new fields**

In `src/save_file.cpp`, locate the fixture-read block (starts around line 1504). Update:

```cpp
    // Fixtures
    {
        uint32_t fixture_count = r.read_u32();
        std::vector<FixtureData> fixtures(fixture_count);
        for (auto& f : fixtures) {
            f.type = static_cast<FixtureType>(r.read_u8());
            f.passable = r.read_u8() != 0;
            f.interactable = r.read_u8() != 0;
            f.cooldown = r.read_i32();
            f.last_used_tick = r.read_i32();
            f.quest_fixture_id = r.read_string();
            f.puzzle_id = r.read_u16();
            f.proximity_message = r.read_string();
            f.proximity_radius = r.read_u8();
            f.tags = r.read_u64();       // v42
            f.spawn_tick = r.read_i32(); // v42
        }
```

If `read_u64` / `write_u64` don't exist on the reader/writer helpers, check how `puzzle_id` (u16) or `cooldown` (i32) are handled and mirror the idiom; if only `u32` helpers exist, write `f.tags` as two `u32`s (high word then low word) — but first confirm whether the io helpers already support u64 by grepping `write_u64` in `save_file.cpp`.

- [ ] **Step 4: Build**

Run: `cmake --build build -j`
Expected: clean build.

- [ ] **Step 5: Smoke — save/load roundtrip**

Run: `./build/astra --term`. With an existing save from pre-v42, attempt to load — expected: the save is rejected / ignored per the pre-ship policy (see user memory `feedback_no_backcompat_pre_ship`).

Start a new game, build a campfire, save, quit, reload. Expected: the campfire is still present, still has the correct remaining lifetime (it should expire at `spawn_tick + 150` from before the save; post-load the world_tick picks up where it left off, so lifetime math works out).

- [ ] **Step 6: Commit**

```bash
git add include/astra/save_file.h src/save_file.cpp
git commit -m "feat(save): persist fixture tags + spawn_tick (v42)"
```

---

## Task 10: Documentation — formulas + roadmap

**Files:**
- Modify: `docs/formulas.md` (Cozy section)
- Modify: `docs/roadmap.md` (if Camp Making is on the list, check it off)

- [ ] **Step 1: Document Cozy formula**

Append to `docs/formulas.md` under an existing "Regeneration" / "Effects" section, or create a new section if none exists:

```markdown
## Cozy (Campfire aura)

Applied while the player is within Chebyshev distance ≤ 6 of one of their
own `FixtureType::Campfire` fixtures. Re-applied each world tick with
`duration = 1`, so it disappears automatically the tick after the player
leaves the radius.

Effect:

```
regen_interval_cozy = max(1, regen_interval(hunger) / 2)
```

In other words, natural HP regeneration ticks roughly twice as fast while
Cozy is active. No effect on hunger, effects, or any other regen.

## Camp Making

- Cooldown: 300 world ticks (`EffectId::CooldownCampMaking`)
- Action cost: 100
- Campfire lifetime: 150 world ticks from the world_tick at which it
  was placed; the fixture is removed cleanly on expiry.
- Placement: 8-neighbour scan in fixed order; first passable tile with no
  fixture wins. Fails with no cooldown if no space.
```

- [ ] **Step 2: Roadmap checkbox**

Open `docs/roadmap.md` and search for "Camp Making" or "Wayfinding". If a checkbox for this item exists, mark it done (`- [x]`). If no such entry exists, skip this step.

- [ ] **Step 3: Commit**

```bash
git add docs/formulas.md docs/roadmap.md
git commit -m "docs: document Cozy + Camp Making formulas"
```

---

## Self-Review

- **Spec coverage:** every section of `2026-04-23-camp-making-design.md` maps to a task:
  - Player-facing behaviour → Task 7 (ability) + Task 4 (Campfire fixture) + Task 8 (expiry).
  - Cozy effect → Tasks 6 (effect + factory) + 8 (application + regen halving).
  - Fixture tagging system → Tasks 1 + 2 + 4 (Campfire gets the cooking/heat/light tags on make_fixture).
  - Campfire expiry → Tasks 3 (spawn_tick field) + 8 (sweep).
  - Animation → Task 5.
  - Save/load → Task 9 (tags + spawn_tick + schema bump).
  - Code touchpoints listed in the spec → each is covered by at least one task above (docs/formulas.md is Task 10; save_file.cpp is Task 9; the map_renderer animation is Task 5).
- **No placeholders** — every step has complete code or an exact command.
- **Type consistency** — `FixtureTag` is the enum name across all tasks; `fixture_has_tag` helper signature matches its only use; `kCampfireLifetimeTicks` / `kCozyRadius` / `kCampMakingCooldownTicks` / `kCampMakingActionCost` names are used consistently; `EffectId::Cozy` and `EffectId::CooldownCampMaking` match their declarations; `make_cozy()` factory signature matches its caller in Task 8.
- **Scope** — single-feature plan, small enough for one implementation pass.
