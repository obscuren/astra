# Playback Viewer — Design

**Date:** 2026-04-14
**Status:** Draft — not yet implemented
**Companion:** `docs/plans/nova-stellar-signal-gap-analysis.md` (motivating use case: Nova arc Stage 2/3 audio logs)

## Summary

Add a generic `PlaybackViewer` modal that reveals titled multi-line text character-by-character at a fixed 30 chars/sec. Audio logs are the first caller (via `QuestFixtureDef`); ruin inscriptions will reuse it later. Each caller supplies a `(PlaybackStyle, title, lines)` triple; the viewer handles reveal pacing, skip-to-end, and close.

Quest fixtures gain two optional fields (`log_title`, `log_lines`). When non-empty, interacting opens the viewer instead of firing a single log line. The `on_fixture_interacted` hook fires on first play only; subsequent interactions replay the log but do not double-count quest progress.

---

## Goals

- One modal that services audio logs today and ruin inscriptions later (and any future narrative playback).
- Turn-paused game gets a real-time reveal feel without breaking the tick loop (game logic stays paused while the modal is open).
- Authoring is additive: drop `log_title` + `log_lines` into a `QuestFixtureDef`, nothing else.
- "Count once, replay always" — narrative is re-readable; quest progress is not.
- No asset loading, no DSL — text lives in the registry def.

## Non-goals

- Per-log speed override (fixed 30 chars/sec).
- Audio (text-only terminal).
- Branching / choose-your-own playback.
- Line-by-line pagination (scroll, don't page).
- A general text modal beyond titled reveal (lore viewer stays separate).
- `Inscription` style implementation details — the enum value is reserved; ruins work is out of scope here.

---

## Data Model

### `PlaybackStyle`

```cpp
enum class PlaybackStyle : uint8_t {
    AudioLog,      // "[ TRANSMISSION ]" header
    Inscription,   // reserved for future ruins work
};
```

### `QuestFixtureDef` extension

Add two optional fields to the existing struct:

```cpp
struct QuestFixtureDef {
    std::string id;
    char glyph = '?';
    int color = 7;
    std::string prompt;
    std::string log_message;          // existing; silent if empty
    std::string log_title;            // NEW — viewer header title
    std::vector<std::string> log_lines;  // NEW — viewer body; empty = no playback
};
```

**Precedence on interact:**

1. If `log_lines` is non-empty → open `PlaybackViewer(AudioLog, log_title, log_lines)`.
2. Else if `log_message` non-empty → `game.log(log_message)` (existing behavior).
3. In both cases fire `on_fixture_interacted(id)` only if `fixture.last_used_tick < 0`.
4. After interact, set `fixture.last_used_tick = world_tick()` so subsequent interacts don't re-count.

### `PlaybackViewer`

```cpp
class PlaybackViewer {
public:
    PlaybackViewer() = default;

    bool is_open() const { return open_; }
    void open(PlaybackStyle style,
              std::string title,
              std::vector<std::string> lines);
    void close() { open_ = false; }

    void tick(float dt_seconds);               // advance reveal cursor
    bool handle_input(int key);                // returns true if consumed
    void draw(Renderer* r, int screen_w, int screen_h);

private:
    bool open_ = false;
    PlaybackStyle style_ = PlaybackStyle::AudioLog;
    std::string title_;
    std::vector<std::string> lines_;
    float reveal_accum_ = 0.0f;                // fractional chars accumulated from dt
    int   reveal_cursor_ = 0;                  // total chars revealed across all lines
    int   total_chars_ = 0;                    // sum of all line lengths (precomputed)
    int   scroll_ = 0;                         // vertical scroll (only used if body overflows)
};
```

**Reveal rate constant:** `static constexpr float kCharsPerSecond = 30.0f;` inside the `.cpp`.

**Input mapping:**
- `Space` — jump `reveal_cursor_` to `total_chars_` (skip reveal).
- `Esc` — `close()`.
- Arrow keys / PgUp / PgDn — scroll if body overflows the modal box.
- All other keys — consumed (no passthrough while open).

---

## Rendering

Terminal-only for now (SDL deferred per project rules).

Layout rules:

- Modal box, centered, width ≈ 60% of screen or `max(line_length)+4`, height ≈ min(body_lines + 6, screen_h - 4).
- Border: Unicode box-drawing (`╔═╗║╚╝`), color from style table.
- Header row: `[ TRANSMISSION ]` for `AudioLog`. Color from style table.
- Title row: plain text, centered, followed by a rule `────...`.
- Body: wrap each logical line to the inner width. Emit only characters up to `reveal_cursor_`. When the last char of the *current* line is partially revealed, show a blinking `_` cursor at the tail.
- Footer: `[Space] Skip   [Esc] Close`.

Style table lives at file scope in `playback_viewer.cpp`:

```cpp
struct PlaybackStyleDef {
    std::string_view header_label;   // "[ TRANSMISSION ]"
    Color header_color;
    Color border_color;
};
static constexpr PlaybackStyleDef kStyles[] = {
    { "[ TRANSMISSION ]", Color::Cyan,    Color::Cyan    },  // AudioLog
    { "[ INSCRIPTION ]",  Color::Yellow,  Color::Yellow  },  // Inscription (reserved)
};
```

The `Inscription` row is filler — visuals will be refined when ruin work lands.

---

## Integration

### `Game` owns the viewer

```cpp
class Game {
    // ...
    PlaybackViewer& playback_viewer() { return playback_viewer_; }
private:
    PlaybackViewer playback_viewer_;
};
```

### Update / draw hooks

In `Game::update(dt)` (the existing per-frame update):

```cpp
if (playback_viewer_.is_open()) {
    playback_viewer_.tick(dt);
    return;  // freeze the rest of the world — turn-paused semantics preserved
}
```

The early return matters: the game is already turn-paused between keypresses, but while the viewer is open we still want `tick(dt)` to advance the reveal cursor in real time.

In `Game::render()`, after the normal world draw, add:

```cpp
if (playback_viewer_.is_open()) {
    playback_viewer_.draw(renderer_.get(), screen_w, screen_h);
}
```

### Input routing

In `InputManager` (or wherever keys are dispatched — mirror how `LoreViewer` and the dev console are handled):

```cpp
if (game.playback_viewer().is_open()) {
    game.playback_viewer().handle_input(key);
    return;  // consume
}
```

### Dialog / fixture wiring

`src/dialog_manager.cpp::interact_fixture`, `FixtureType::QuestFixture` case:

```cpp
case FixtureType::QuestFixture: {
    const QuestFixtureDef* def = find_quest_fixture(f.quest_fixture_id);
    bool first_use = (f.last_used_tick < 0);

    if (def && !def->log_lines.empty()) {
        game.playback_viewer().open(PlaybackStyle::AudioLog,
                                    def->log_title,
                                    def->log_lines);
    } else if (def && !def->log_message.empty()) {
        game.log(def->log_message);
    }

    if (first_use) {
        game.quests().on_fixture_interacted(f.quest_fixture_id);
    }
    f.last_used_tick = game.world().world_tick();
    break;
}
```

This replaces the existing case body.

---

## Save / Load

Nothing new. `FixtureData.last_used_tick` already round-trips (unchanged). `PlaybackViewer` state is entirely transient — closing the game while a log is open does not persist; on reload the fixture is simply interactable again. Intentional: no one expects to resume a mid-playback text scroll.

No save version bump.

---

## Authoring example (Nova Echo 3 fragment)

Inside `NovaStellarSignalQuest::register_fixtures()`:

```cpp
QuestFixtureDef echo3;
echo3.id = "nova_signal_node_echo3";
echo3.glyph = '*';
echo3.color = 135;
echo3.prompt = "Play fragment recording";
echo3.log_title = "FRAGMENT — STELLARI VOICE, UNKNOWN AGE";
echo3.log_lines = {
    "...find the one with green eyes. He always finds you.",
    "Don't forget him this time.",
    "",
    "And this time... try to stay.",
};
register_quest_fixture(std::move(echo3));
```

No other code changes per quest.

---

## File Map

| File | Kind | Responsibility |
|---|---|---|
| `include/astra/playback_viewer.h` | NEW | `PlaybackStyle`, `PlaybackViewer` class |
| `src/playback_viewer.cpp` | NEW | Reveal state, input, draw, style table |
| `include/astra/quest_fixture.h` | MODIFY | Add `log_title`, `log_lines` |
| `src/dialog_manager.cpp` | MODIFY | Replace `QuestFixture` interact case |
| `include/astra/game.h` | MODIFY | Own viewer; accessor |
| `src/game.cpp` | MODIFY | Wire `tick`, `render`, freeze update while open |
| `src/input_manager.cpp` | MODIFY | Route keys while open |
| `CMakeLists.txt` | MODIFY | Add `src/playback_viewer.cpp` |

---

## Implementation Checklist (for the forthcoming plan)

1. Add `PlaybackStyle` enum, `PlaybackViewer` class (`open` / `close` / `tick` / `handle_input` / `draw` / `is_open`).
2. Implement reveal accumulator (30 chars/sec), skip-to-end on Space, close on Esc, scroll on arrows/PgUp/PgDn.
3. Implement draw: box, header, title + rule, wrapped body with partial-char blinking cursor, footer.
4. Extend `QuestFixtureDef` with `log_title` + `log_lines`.
5. Own a `PlaybackViewer` on `Game`; add `playback_viewer()` accessor.
6. Freeze `Game::update(dt)` while open; still call `viewer.tick(dt)`.
7. Call `viewer.draw` after the normal render.
8. Route input to viewer while open (same pattern as `LoreViewer` / dev console).
9. Rewrite the `QuestFixture` case in `interact_fixture` per the design; guard the hook on `last_used_tick < 0`; write the tick back.
10. Register a debug log on the `dev_smoke_fixture` to smoke-test via `quest fixture`.
11. Smoke test: open log, watch reveal, Space to skip, Esc to close, re-interact to replay, save/reload to confirm `last_used_tick` persistence.

---

## Out of scope — explicitly deferred

- Per-log reveal speed override.
- Player-tunable reveal speed.
- Voice / SFX cues.
- Choice-based logs (branching dialogue playback).
- Ruin inscription visual polish (enum value reserved only).
- Log history screen (the journal may grow one later; not here).
