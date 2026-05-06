# Animation System

Reference documentation for Astra's lightweight animation layer. The system is purely visual — it never blocks input, never affects game state, and game state is always resolved before any animation plays.

**Code:** `include/astra/animation.h`, `src/animation.cpp`, integration points in `game.cpp`, `map_renderer.cpp`, `game_combat.cpp`, `game_world.cpp`.

---

## Overview

Astra's world is **frozen between keypresses.** Without animation, this makes combat feel inert and the map static. The animation system adds visual life in two flavors:

1. **Fixture animations** — persistent, looping. Console blink, water shimmer, viewport flicker. Tied to fixture tiles in the visible map.
2. **Effect animations** — transient, one-shot. Damage flash, projectile travel, heal pulse, level-up glow. Spawned in response to game events.

Animations exist alongside the normal tile rendering pass; the renderer queries the animation manager per visible tile and overrides the glyph/color when an active animation is present.

---

## Design rationale

### Wall-clock timing

Animations advance in **real time**, not per turn. `AnimationManager::tick()` is called every frame and computes a delta from `std::chrono::steady_clock`. Animation speed is consistent regardless of how fast the player presses keys — walking quickly doesn't fast-forward; idling doesn't pause.

### Frozen-between-keypresses philosophy

When the input loop is fully blocked on `wait_input()` (no targeting, no auto-walk, no active effects), `tick()` is not called and **fixture animations freeze.** This matches the rest of the game — nothing moves until the player does. The exceptions that opt into the timeout loop are: targeting mode, auto-walk / auto-explore, and any active effect animation.

### Effects priority

When both a fixture animation and an effect animation occupy the same tile, the **effect wins.** Effects are short-lived; fixtures are ambient.

### No game-state coupling

Animations never trigger gameplay or AI side-effects. Combat resolves first; animations spawn afterward as a side-channel. Removing the animation layer entirely would not change game outcomes.

---

## Data model

```cpp
struct AnimationFrame {
    char glyph = ' ';
    const char* utf8 = nullptr;   // null = use char glyph
    Color color = Color::White;
    int duration_ms = 150;
};

struct AnimationDef {
    std::vector<AnimationFrame> frames;
    bool looping = false;
};

struct ActiveAnimation {
    int x, y;                     // world coordinates
    const AnimationDef* def;
    int current_frame = 0;
    int elapsed_ms = 0;
    int delay_ms = 0;             // countdown before animation starts (line effects)
    bool finished = false;
};
```

`AnimationDef` instances are static and shared. `ActiveAnimation` is the runtime state for one playing instance — owned by the manager, dropped when finished.

---

## AnimationManager API

```cpp
class AnimationManager {
public:
    void tick();                                          // advance, drop finished
    const AnimationFrame* query(int mx, int my) const;    // current frame at tile, or null

    void spawn_effect(const AnimationDef& def, int x, int y);
    void spawn_effect_line(const AnimationDef& def, int x0, int y0, int x1, int y1);
    void spawn_fixture_anims(const TileMap& map, const VisibilityMap& vis);

    bool has_active_effects() const;
    void clear();
};
```

- **`tick()`** — call every frame. Computes wall-clock delta, advances `elapsed_ms`, steps frames, marks finished, prunes dead.
- **`query()`** — called by the renderer per visible tile. Effects checked first, then fixtures. Linear scan; total active count is typically <50.
- **`spawn_effect()`** — one-shot effect at a single tile.
- **`spawn_effect_line()`** — Bresenham line from `(x0,y0)` to `(x1,y1)`. Each cell gets the effect with `delay_ms = i * 60` so the animation visibly travels.
- **`spawn_fixture_anims()`** — scan visible tiles for animatable fixtures (Water, Console, Viewport) and seed loops. Start phase is staggered using a position hash so adjacent tiles don't blink in sync.
- **`has_active_effects()`** — used by the main loop to decide whether to enter the timeout loop.
- **`clear()`** — drop everything. Used on map transitions.

---

## Animation library

Static `AnimationDef` instances. Tunable in `src/animation.cpp`.

| Name | Frames | Per-frame duration | Looping |
|---|---|---|---|
| `anim_console_blink`     | Cyan `#` / DarkGray `#`                            | 500 ms           | yes |
| `anim_water_shimmer`     | Blue `~` / Cyan `≈` / Blue `~`                     | 400 ms           | yes |
| `anim_viewport_shimmer`  | Cyan `"` / DarkCyan `"`                            | 800 ms           | yes |
| `anim_damage_flash`      | Red `*` / (end)                                    | 100 ms + 100 ms  | no  |
| `anim_heal_pulse`        | Green `+` / BrightGreen `+` / (end)                | 120 ms each      | no  |
| `anim_projectile`        | Yellow `*`                                         | 80 ms            | no  |
| `anim_level_up`          | Yellow `!` / BrightYellow `!` / Yellow `!`         | 150 ms each      | no  |

Add new animation defs by appending to the static table and a corresponding `extern` declaration in the header.

---

## Integration

### Main loop (`src/game.cpp`)

The main loop blocks on input by default. When any of these conditions hold, it switches to a timed loop so animations can advance:

```cpp
bool needs_timeout = combat_.targeting() || input_.looking()
                   || quit_confirm_.is_open()
                   || auto_walking_ || auto_exploring_
                   || animations_.has_active_effects();

int timeout_ms = (auto_walking_ || auto_exploring_) ? 50
               : animations_.has_active_effects()   ? 80
                                                    : 300;
```

Every loop iteration, before `render()`:

```cpp
animations_.tick();
```

### Renderer (`src/map_renderer.cpp`)

`MapRenderContext` carries an `const AnimationManager* animations` pointer. After computing the default glyph/color for a visible tile, the renderer asks the manager whether to override:

```cpp
if (rc.animations) {
    if (auto* frame = rc.animations->query(mx, my)) {
        // override glyph and color from frame
    }
}
```

`game_rendering.cpp` passes `&animations_` when populating the context.

### Game systems

Animations are spawned from gameplay code at meaningful moments:

- **`game_combat.cpp`** — `attack_npc()` flashes red on hit. `shoot_target()` fires a projectile line then a flash on the target. NPC-on-player damage flashes the player. Level-up flashes yellow on the player.
- **`game_world.cpp`** — after `recompute_fov()`, `spawn_fixture_anims()` rebuilds the visible loop set. On map transitions (enter ship, exit station, etc.) `clear()` resets state.

---

## Files

| File | Role |
|---|---|
| `include/astra/animation.h` | structs + `AnimationManager` interface |
| `src/animation.cpp`         | implementation + static animation table |
| `include/astra/map_renderer.h` | `AnimationManager*` field on `MapRenderContext` |
| `src/map_renderer.cpp`      | per-tile override pass |
| `src/game.cpp`              | timeout-loop logic + `tick()` call |
| `src/game_combat.cpp`       | spawn calls on hit / shoot / level-up |
| `src/game_world.cpp`        | fixture-anim rebuild after FOV; clear on transitions |

---

## Extending

- **New looping fixture animation:** add a `FixtureType` check in `spawn_fixture_anims()`, append a static `AnimationDef`, declare in the header.
- **New one-shot effect:** add a static `AnimationDef`, then call `spawn_effect()` or `spawn_effect_line()` from the relevant gameplay code path.
- **Renderer overrides:** if you need an animation to override only certain layers (e.g., draw under a UI overlay), extend the renderer query, not the manager.
