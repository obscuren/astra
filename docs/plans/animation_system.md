# Plan: Animation System

## Context

The game world is frozen between keypresses — no visual life. Adding animations makes combat feel impactful (damage flash, projectile travel) and the world more alive (blinking consoles, water shimmer). Animations are purely visual — game state is already resolved before they play. Player input is never blocked.

Two categories:
1. **Fixture animations** — persistent, looping (console blink, water shimmer)
2. **Effect animations** — transient, one-shot (damage flash, projectile, heal pulse, level up)

---

## Phase 1: AnimationManager Class

### New file: `include/astra/animation.h`

```cpp
struct AnimationFrame {
    char glyph = ' ';
    const char* utf8 = nullptr;  // null = use char glyph
    Color color = Color::White;
    int duration_ms = 150;
};

struct AnimationDef {
    std::vector<AnimationFrame> frames;
    bool looping = false;
};

struct ActiveAnimation {
    int x, y;                    // world coordinates
    const AnimationDef* def;
    int current_frame = 0;
    int elapsed_ms = 0;
    int delay_ms = 0;            // countdown before animation starts (for staggered projectiles)
    bool finished = false;
};

class AnimationManager {
public:
    void tick();  // uses wall-clock delta — call every frame (keypress or timeout)
    const AnimationFrame* query(int mx, int my) const;

    void spawn_effect(const AnimationDef& def, int x, int y);
    void spawn_effect_line(const AnimationDef& def, int x0, int y0, int x1, int y1);
    void spawn_fixture_anims(const TileMap& map, const VisibilityMap& vis);

    bool has_active_effects() const;
    void clear();

private:
    std::vector<ActiveAnimation> fixture_anims_;
    std::vector<ActiveAnimation> effect_anims_;
    std::chrono::steady_clock::time_point last_tick_ = std::chrono::steady_clock::now();
};
```

### New file: `src/animation.cpp`

**tick()**: compute `delta_ms` from wall-clock (`steady_clock`), advance elapsed_ms on each animation, step frames, mark finished, erase dead effects. Animations run at real-time speed regardless of input frequency.

**query()**: effects checked first (priority over fixtures), linear scan — typically <50 entries.

**spawn_fixture_anims()**: scan visible tiles for Water, Console, Viewport fixtures. Stagger start phase with position hash so adjacent tiles don't blink in sync.

**spawn_effect_line()**: Bresenham line from (x0,y0) to (x1,y1), each cell gets the animation with `delay_ms = i * 60` for travel effect.

**Static animation definitions:**

| Name | Frames | Duration | Looping |
|------|--------|----------|---------|
| `anim_console_blink` | Cyan `#` / DarkGray `#` | 500ms each | Yes |
| `anim_water_shimmer` | Blue `~` / Cyan `≈` / Blue `~` | 400ms each | Yes |
| `anim_viewport_shimmer` | Cyan `"` / DarkCyan `"` | 800ms each | Yes |
| `anim_damage_flash` | Red `*` / (end) | 100ms + 100ms | No |
| `anim_heal_pulse` | Green `+` / BrightGreen `+` / (end) | 120ms each | No |
| `anim_projectile` | Yellow `*` | 80ms | No |
| `anim_level_up` | Yellow `!` / BrightYellow `!` / Yellow `!` | 150ms each | No |

---

## Phase 2: Main Loop Integration

### `src/game.cpp` — timeout handling

```cpp
bool needs_timeout = combat_.targeting() || input_.looking()
                   || quit_confirm_.is_open()
                   || auto_walking_ || auto_exploring_
                   || animations_.has_active_effects();

int timeout_ms = (auto_walking_ || auto_exploring_) ? 50
               : animations_.has_active_effects() ? 80
               : 300;
```

**Wall-clock timing:** `tick()` is called every frame — both on keypress AND on timeout. Internally it computes `delta_ms = now - last_tick_` using `steady_clock`, so animation speed is consistent regardless of input rate. Walking fast doesn't speed up animations; idling doesn't slow them down.

```cpp
// In the main loop, BEFORE render(), every iteration:
animations_.tick();
```

Fixture animations only advance when the timeout loop is already active (targeting, auto-walk, or effects playing). When idle with no effects, `wait_input()` blocks and no tick occurs — fixtures freeze. This matches the "frozen between keypresses" philosophy.

---

## Phase 3: Map Renderer Integration

### `include/astra/map_renderer.h`
Add `const AnimationManager* animations = nullptr;` to `MapRenderContext`.

### `src/map_renderer.cpp`
After computing default glyph/color for a visible tile, before `ctx.put()`:
```cpp
if (rc.animations) {
    if (auto* frame = rc.animations->query(mx, my)) {
        // override glyph and color
    }
}
```

### `src/game_rendering.cpp`
Pass `&animations_` in the `render_map()` call.

---

## Phase 4: Spawn Animations from Game Systems

### `src/game_combat.cpp`
- `attack_npc()` — after damage: `game.animations().spawn_effect(anim_damage_flash, npc.x, npc.y)`
- `shoot_target()` — projectile line + damage flash at target
- `process_npc_turn()` — when NPC hits player: damage flash at player pos
- `check_level_up()` — level up flash at player pos

### `src/game_world.cpp`
- After `recompute_fov()`: `animations_.spawn_fixture_anims(world_.map(), world_.visibility())`
- Map transitions (`enter_ship`, `exit_ship_to_station`, etc.): `animations_.clear()`

---

## Files Modified/Created

| File | Action |
|------|--------|
| `include/astra/animation.h` | **New** — structs + AnimationManager |
| `src/animation.cpp` | **New** — implementation + static defs |
| `include/astra/game.h` | Add `AnimationManager animations_` + accessor |
| `include/astra/map_renderer.h` | Add `AnimationManager*` to context |
| `src/game.cpp` | Timeout logic + tick call |
| `src/game_rendering.cpp` | Pass animations to render_map |
| `src/map_renderer.cpp` | Animation override in tile rendering |
| `src/game_combat.cpp` | Spawn effects on hit/shoot/levelup |
| `src/game_world.cpp` | Fixture rebuild after FOV, clear on transitions |
| `CMakeLists.txt` | Add `src/animation.cpp` |

---

## Verification

1. Build compiles
2. Melee attack → red `*` flash on target for ~200ms
3. Ranged shot → yellow `*` travels from player to target, then red flash
4. Level up → yellow `!` flash on player
5. NPC hits player → red flash on player
6. Enter targeting mode → consoles/water start shimmering
7. Animations don't block input — can move/act while effects play
8. Map transition → animations cleared, fixture anims rebuilt
9. Projectile line correctly follows Bresenham path
10. Adjacent water tiles shimmer out of phase (not in sync)
