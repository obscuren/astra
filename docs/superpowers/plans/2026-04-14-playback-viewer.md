# Playback Viewer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a generic `PlaybackViewer` modal that reveals titled multi-line text character-by-character, wire it into the game as a sibling of `LoreViewer` / `DevConsole`, and plug it into quest fixtures via two new fields on `QuestFixtureDef` (`log_title`, `log_lines`).

**Architecture:** New `PlaybackViewer` class owns reveal-cursor + scroll state. Reveal timing uses `std::chrono::steady_clock` (the run loop has no dt); the run loop polls with a short timeout while the viewer is actively revealing so the UI refreshes at ~30 FPS for smooth typing. Input capture + overlay render mirror the existing `DevConsole`/`LoreViewer` patterns in `game_input.cpp` and `game_rendering.cpp`. Interacting with a `QuestFixture` that carries `log_lines` opens the viewer instead of logging a single line, and the quest hook is gated on `FixtureData::last_used_tick < 0` (count-once, replay-always).

**Tech Stack:** C++20, `<chrono>`, existing `Renderer::draw_panel` / `draw_ui_text` semantic draws. No new deps.

**Spec:** `docs/superpowers/specs/2026-04-14-playback-viewer-design.md`

**No save version bump** — all state is transient.

**Worktree:** run this plan in `.worktrees/playback-viewer` on branch `feat/playback-viewer`, forked from `main`.

---

## File Structure

| File | Kind | Responsibility |
|---|---|---|
| `include/astra/playback_viewer.h` | NEW | `PlaybackStyle` enum + `PlaybackViewer` class |
| `src/playback_viewer.cpp` | NEW | Reveal math, input, draw, style table |
| `include/astra/quest_fixture.h` | MODIFY | Add `log_title`, `log_lines` |
| `src/dialog_manager.cpp` | MODIFY | Rewrite `FixtureType::QuestFixture` case |
| `include/astra/game.h` | MODIFY | Own `PlaybackViewer`; add accessor; add loop-timeout hint |
| `src/game.cpp` | MODIFY | Shorter timeout when viewer is revealing |
| `src/game_input.cpp` | MODIFY | Route keys to viewer when open |
| `src/game_rendering.cpp` | MODIFY | Draw viewer after other overlays |
| `src/dev_console.cpp` | MODIFY | Smoke-test: attach `log_lines` to `dev_smoke_fixture` |
| `CMakeLists.txt` | MODIFY | Add `src/playback_viewer.cpp` to `ASTRA_SOURCES` |

Build command used throughout: `cmake --build build` (DEV build already configured). Run: `./build/astra-dev --term`.

Commit convention: `feat(ui):`, `feat(quests):`, `fix:` — match repo style.

---

### Task 1: `PlaybackViewer` skeleton (header + empty impl + CMake)

**Files:**
- Create: `include/astra/playback_viewer.h`
- Create: `src/playback_viewer.cpp`
- Modify: `CMakeLists.txt` — add near `src/lore_viewer.cpp` (currently line ~96) in `ASTRA_SOURCES`

- [ ] **Step 1: Write the header**

`include/astra/playback_viewer.h`:

```cpp
#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace astra {

class Renderer;

enum class PlaybackStyle : uint8_t {
    AudioLog,      // "[ TRANSMISSION ]" header
    Inscription,   // reserved for future ruins work
};

class PlaybackViewer {
public:
    PlaybackViewer() = default;

    bool is_open() const { return open_; }
    // True while revealing chars; the run loop uses this to poll fast.
    bool is_revealing() const;

    void open(PlaybackStyle style,
              std::string title,
              std::vector<std::string> lines);
    void close() { open_ = false; }

    // Returns true if the key was consumed.
    bool handle_input(int key);
    void draw(Renderer* r, int screen_w, int screen_h);

private:
    using Clock = std::chrono::steady_clock;

    bool open_ = false;
    PlaybackStyle style_ = PlaybackStyle::AudioLog;
    std::string title_;
    std::vector<std::string> lines_;
    int total_chars_ = 0;               // sum of line lengths, set at open()
    int skip_offset_ = 0;               // chars added to the reveal cursor on Space
    Clock::time_point start_time_;      // set at open()
    int scroll_ = 0;                    // vertical scroll (only when body overflows)

    int reveal_cursor() const;          // how many chars to draw now
};

} // namespace astra
```

- [ ] **Step 2: Write the empty implementation**

`src/playback_viewer.cpp`:

```cpp
#include "astra/playback_viewer.h"

#include "astra/renderer.h"

namespace astra {

static constexpr float kCharsPerSecond = 30.0f;

void PlaybackViewer::open(PlaybackStyle style,
                          std::string title,
                          std::vector<std::string> lines) {
    open_ = true;
    style_ = style;
    title_ = std::move(title);
    lines_ = std::move(lines);
    total_chars_ = 0;
    for (const auto& l : lines_) total_chars_ += static_cast<int>(l.size());
    skip_offset_ = 0;
    start_time_ = Clock::now();
    scroll_ = 0;
}

int PlaybackViewer::reveal_cursor() const {
    if (!open_) return 0;
    float elapsed = std::chrono::duration<float>(Clock::now() - start_time_).count();
    int cursor = static_cast<int>(elapsed * kCharsPerSecond) + skip_offset_;
    if (cursor > total_chars_) cursor = total_chars_;
    return cursor;
}

bool PlaybackViewer::is_revealing() const {
    return open_ && reveal_cursor() < total_chars_;
}

bool PlaybackViewer::handle_input(int /*key*/) {
    // Task 4 wires input.
    return open_;
}

void PlaybackViewer::draw(Renderer* /*r*/, int /*screen_w*/, int /*screen_h*/) {
    // Task 5 wires draw.
}

} // namespace astra
```

- [ ] **Step 3: Add to CMakeLists**

Find `src/lore_viewer.cpp` in the `ASTRA_SOURCES` block and add `src/playback_viewer.cpp` alphabetically (between `src/navigation.cpp` / `src/npc_definitions.cpp` and `src/poi_placement.cpp` depending on existing ordering — follow the pattern).

- [ ] **Step 4: Build**

Run: `cmake --build build`
Expected: clean build; the translation unit compiles and links into `astra-dev`.

- [ ] **Step 5: Commit**

```bash
git add include/astra/playback_viewer.h src/playback_viewer.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(ui): PlaybackViewer scaffolding

Header + empty impl. open() initializes state; is_revealing() and
reveal_cursor() compute from a steady_clock start-time since Game::update
has no dt parameter. handle_input/draw are stubs filled in subsequent tasks.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Input handling

**Files:**
- Modify: `src/playback_viewer.cpp` — flesh out `handle_input`

- [ ] **Step 1: Pull the key constants**

Open `include/astra/renderer.h` and confirm the symbolic names for arrow keys / PgUp / PgDn (`KEY_UP`, `KEY_DOWN`, `KEY_PGUP`, `KEY_PGDN` or similar) and Esc (`KEY_ESC` — or raw `27`). Use whatever names the project defines.

- [ ] **Step 2: Implement input**

Replace the `handle_input` stub in `src/playback_viewer.cpp`:

```cpp
bool PlaybackViewer::handle_input(int key) {
    if (!open_) return false;

    // Esc closes
    if (key == 27 /*KEY_ESC*/ || key == 'q' || key == 'Q') {
        close();
        return true;
    }

    // Space skips remaining reveal
    if (key == ' ') {
        skip_offset_ = total_chars_;   // clamps in reveal_cursor()
        return true;
    }

    // Scroll (only meaningful when body overflows; draw clamps scroll_)
    if (key == KEY_UP)    { --scroll_; if (scroll_ < 0) scroll_ = 0; return true; }
    if (key == KEY_DOWN)  { ++scroll_; return true; }
    if (key == KEY_PGUP)  { scroll_ -= 10; if (scroll_ < 0) scroll_ = 0; return true; }
    if (key == KEY_PGDN)  { scroll_ += 10; return true; }

    // Consume everything else (viewer has focus)
    return true;
}
```

If the project's key constants differ (e.g., `Key::Up`), use those. The `scroll_` upper bound is enforced in `draw()` (Task 5) — clamping up-front would require knowing the modal height here, which we don't.

- [ ] **Step 3: Add the `#include "astra/renderer.h"` if not already present**

Already added in Task 1.

- [ ] **Step 4: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add src/playback_viewer.cpp
git commit -m "$(cat <<'EOF'
feat(ui): playback viewer input handling

Esc/q closes, Space skips reveal, arrows/PgUp/PgDn scroll. Viewer
consumes all other keys while open.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Draw the modal

**Files:**
- Modify: `src/playback_viewer.cpp` — implement `draw`

- [ ] **Step 1: Study the LoreViewer draw as reference**

Open `src/lore_viewer.cpp`. Note:
- how it sizes its modal (`Rect`),
- how it calls `renderer->draw_panel(...)`,
- how it emits text lines with `draw_ui_text` or `draw_string`,
- how it handles scroll.

Mirror those calls — don't invent new rendering primitives.

- [ ] **Step 2: Add the style table and helpers**

At file scope in `src/playback_viewer.cpp` (above the class methods, after `kCharsPerSecond`):

```cpp
struct PlaybackStyleDef {
    const char* header_label;
    Color       header_color;
    Color       border_color;
};

static constexpr PlaybackStyleDef kStyles[] = {
    { "[ TRANSMISSION ]", Color::Cyan,   Color::Cyan   },   // AudioLog
    { "[ INSCRIPTION ]",  Color::Yellow, Color::Yellow },   // Inscription (reserved)
};

static const PlaybackStyleDef& style_def(PlaybackStyle s) {
    return kStyles[static_cast<size_t>(s)];
}

// Wrap a single logical line to a column width.
static std::vector<std::string> wrap_line(const std::string& in, int width) {
    std::vector<std::string> out;
    if (width <= 0) { out.push_back(in); return out; }
    size_t i = 0;
    while (i < in.size()) {
        size_t take = std::min<size_t>(width, in.size() - i);
        // Prefer to break at the last space within the window.
        if (i + take < in.size()) {
            size_t sp = in.rfind(' ', i + take);
            if (sp != std::string::npos && sp > i) take = sp - i;
        }
        out.emplace_back(in.substr(i, take));
        i += take;
        while (i < in.size() && in[i] == ' ') ++i;  // skip leading space
    }
    if (out.empty()) out.emplace_back("");
    return out;
}
```

Include `<algorithm>` and `<cstddef>` at the top if not already pulled in.

- [ ] **Step 3: Implement `draw`**

Replace the stub:

```cpp
void PlaybackViewer::draw(Renderer* r, int screen_w, int screen_h) {
    if (!open_ || !r) return;

    const auto& s = style_def(style_);

    // Box sizing: 60% of screen, centered; clamp to content.
    int box_w = std::max(40, screen_w * 6 / 10);
    int inner_w = box_w - 4;
    if (inner_w < 20) inner_w = 20;

    // Wrap body lines to the inner width; we need the flat character count for reveal.
    std::vector<std::string> wrapped;
    // Preserve the mapping from original-line boundaries by inserting blanks where needed.
    for (const auto& logical : lines_) {
        if (logical.empty()) { wrapped.emplace_back(""); continue; }
        auto pieces = wrap_line(logical, inner_w);
        for (auto& p : pieces) wrapped.push_back(std::move(p));
    }

    int header_rows = 3;   // header + title + rule
    int footer_rows = 2;   // blank + footer
    int body_budget = screen_h - 4 - header_rows - footer_rows;
    if (body_budget < 4) body_budget = 4;
    int box_h = header_rows + std::min<int>(static_cast<int>(wrapped.size()), body_budget) + footer_rows + 2;
    int bx = (screen_w - box_w) / 2;
    int by = (screen_h - box_h) / 2;

    // Clamp scroll so the last line is at least reachable.
    int overflow = static_cast<int>(wrapped.size()) - (box_h - header_rows - footer_rows - 2);
    if (overflow < 0) overflow = 0;
    if (scroll_ > overflow) scroll_ = overflow;

    // Panel
    PanelDesc pd;
    pd.border_color = s.border_color;
    r->draw_panel(Rect{bx, by, box_w, box_h}, pd);

    // Header (centered)
    int row = by + 1;
    {
        std::string h = s.header_label;
        int hx = bx + (box_w - static_cast<int>(h.size())) / 2;
        r->draw_string(hx, row, h);
    }

    // Title row + rule
    ++row;
    {
        int tx = bx + (box_w - static_cast<int>(title_.size())) / 2;
        if (tx < bx + 2) tx = bx + 2;
        r->draw_string(tx, row, title_);
    }
    ++row;
    {
        std::string rule(inner_w, '-');
        r->draw_string(bx + 2, row, rule);
    }

    // Body: emit chars up to reveal_cursor
    int cursor = reveal_cursor();
    int body_first = header_rows;                 // relative to box
    int body_rows = box_h - header_rows - footer_rows - 2;
    int emitted = 0;
    for (int i = 0; i < body_rows; ++i) {
        int wrapped_idx = i + scroll_;
        if (wrapped_idx >= static_cast<int>(wrapped.size())) break;
        const std::string& wl = wrapped[wrapped_idx];
        int available = cursor - emitted;
        if (available <= 0) {
            emitted += static_cast<int>(wl.size());  // still count toward cursor
            continue;
        }
        int take = std::min<int>(static_cast<int>(wl.size()), available);
        r->draw_string(bx + 2, by + body_first + 1 + i, wl.substr(0, take));
        // Trailing caret while this line is partially revealed
        if (take < static_cast<int>(wl.size())) {
            bool blink = (std::chrono::duration_cast<std::chrono::milliseconds>(
                              Clock::now().time_since_epoch()).count() / 400) & 1;
            if (blink) r->draw_char(bx + 2 + take, by + body_first + 1 + i, '_',
                                    Color::White, Color::Default);
        }
        emitted += static_cast<int>(wl.size());
    }

    // Footer
    {
        std::string footer = is_revealing() ? "[Space] Skip   [Esc] Close"
                                             : "[Esc] Close";
        int fx = bx + (box_w - static_cast<int>(footer.size())) / 2;
        r->draw_string(fx, by + box_h - 2, footer);
    }
}
```

**Note on `draw_string` / `draw_panel` / `draw_char` / `PanelDesc` / `Rect`:** these are the `Renderer` interface primitives confirmed by the survey (`include/astra/renderer.h:79-144`). If the actual API in the branch differs, adapt — do not invent new methods. If your call to `draw_panel(Rect, PanelDesc)` compiles as shown, you're good. If `PanelDesc` has no `border_color` field but uses something like `fg` or `color`, use that.

- [ ] **Step 4: Build and smoke test**

Run: `cmake --build build`
Expected: clean build. No runtime exercise yet — viewer isn't wired into Game.

- [ ] **Step 5: Commit**

```bash
git add src/playback_viewer.cpp
git commit -m "$(cat <<'EOF'
feat(ui): playback viewer rendering

Draws a centered panel with header, title, ruled body, and footer.
Body lines are wrapped to the inner width and revealed up to
reveal_cursor(); a blinking caret sits at the partial tail.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: Own the viewer on `Game`

**Files:**
- Modify: `include/astra/game.h` — new member + accessor
- Modify: `src/game.cpp` — include header

- [ ] **Step 1: Add the member and accessor**

In `include/astra/game.h`, near the existing `LoreViewer lore_viewer_;` member:

```cpp
    PlaybackViewer playback_viewer_;
```

And in the public section (near `open_lore_viewer()`):

```cpp
    PlaybackViewer& playback_viewer() { return playback_viewer_; }
    const PlaybackViewer& playback_viewer() const { return playback_viewer_; }
```

At the top of `game.h`, add:

```cpp
#include "astra/playback_viewer.h"
```

- [ ] **Step 2: Build**

Run: `cmake --build build`
Expected: clean build. The viewer is owned but not yet rendered or input-routed.

- [ ] **Step 3: Commit**

```bash
git add include/astra/game.h
git commit -m "$(cat <<'EOF'
feat(ui): Game owns PlaybackViewer

Member + public accessor, mirrors LoreViewer ownership.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: Route input to the viewer

**Files:**
- Modify: `src/game_input.cpp` — early-return if viewer is open

- [ ] **Step 1: Add the guard**

Find the existing modal guard block around lines 40-60 of `src/game_input.cpp`. Insert a new guard **before** the LoreViewer guard (order matters: topmost modal wins):

```cpp
    if (playback_viewer_.is_open()) {
        playback_viewer_.handle_input(key);
        return;
    }
```

(If `handle_input` in this file is a free function that takes `Game&`, adjust to `game.playback_viewer().handle_input(key);` — match the style used by the adjacent LoreViewer/DevConsole guards exactly.)

- [ ] **Step 2: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add src/game_input.cpp
git commit -m "$(cat <<'EOF'
feat(ui): route input to PlaybackViewer when open

Guards added to handle_play_input mirroring DevConsole / LoreViewer.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: Draw the viewer after world/overlays

**Files:**
- Modify: `src/game_rendering.cpp` — add draw call

- [ ] **Step 1: Locate the LoreViewer draw**

Around line 809 of `src/game_rendering.cpp` there's `lore_viewer_.draw(renderer_.get(), screen_w_, screen_h_)`. Right after that call (so the playback viewer renders on top if both were somehow open simultaneously — they shouldn't, but keep order explicit):

```cpp
    if (playback_viewer_.is_open()) {
        playback_viewer_.draw(renderer_.get(), screen_w_, screen_h_);
    }
```

- [ ] **Step 2: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add src/game_rendering.cpp
git commit -m "$(cat <<'EOF'
feat(ui): draw PlaybackViewer after other overlays

Rendered last so the typing modal sits on top of any world/HUD draws.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 7: Poll fast while revealing

**Files:**
- Modify: `src/game.cpp:41-48` — include playback viewer in timeout logic

- [ ] **Step 1: Update the timeout decision**

In `Game::run()` where `needs_timeout` and `timeout_ms` are computed (lines 41-48), extend both:

```cpp
        bool revealing = playback_viewer_.is_revealing();
        bool needs_timeout = combat_.targeting() || input_.looking()
                           || quit_confirm_.open
                           || auto_walking_ || auto_exploring_
                           || animations_.has_any()
                           || revealing;
        int timeout_ms = revealing                                 ? 33
                       : (auto_walking_ || auto_exploring_)         ? 50
                       : animations_.has_active_effects()           ? 80
                       : animations_.has_any()                      ? 200
                                                                    : 300;
```

33ms ≈ 30 FPS — matches the reveal rate. When revealing stops (cursor hits total), the viewer falls back to the idle 300 ms timeout.

- [ ] **Step 2: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add src/game.cpp
git commit -m "$(cat <<'EOF'
feat(ui): poll ~30fps while PlaybackViewer is revealing

Adds is_revealing() to the run-loop timeout decision so reveal
chars appear smoothly. Idle poll rate resumes once the log is fully
shown.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 8: Extend `QuestFixtureDef`

**Files:**
- Modify: `include/astra/quest_fixture.h` — add two optional fields

- [ ] **Step 1: Extend the struct**

In `include/astra/quest_fixture.h`, extend `QuestFixtureDef`:

```cpp
struct QuestFixtureDef {
    std::string id;
    char glyph = '?';
    int color = 7;
    std::string prompt;
    std::string log_message;
    std::string log_title;                        // NEW — viewer header title
    std::vector<std::string> log_lines;           // NEW — viewer body; empty = no playback
};
```

Add `#include <vector>` at the top if not already pulled in.

- [ ] **Step 2: Build**

Run: `cmake --build build`
Expected: clean build. No callers use the new fields yet.

- [ ] **Step 3: Commit**

```bash
git add include/astra/quest_fixture.h
git commit -m "$(cat <<'EOF'
feat(quests): add log_title and log_lines to QuestFixtureDef

Optional fields; when log_lines is non-empty the fixture plays an
audio log via PlaybackViewer on interact (wired in a later task).

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 9: Rewrite `QuestFixture` interact case

**Files:**
- Modify: `src/dialog_manager.cpp` — replace the existing case body

- [ ] **Step 1: Replace the case**

Find `case FixtureType::QuestFixture:` in `DialogManager::interact_fixture` (around line 532). Replace the entire case body with:

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

- [ ] **Step 2: Ensure includes**

At the top of `src/dialog_manager.cpp`, confirm `#include "astra/playback_viewer.h"` is present (or add it). `quest_fixture.h` is already included from Task 6 of the previous plan.

- [ ] **Step 3: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add src/dialog_manager.cpp
git commit -m "$(cat <<'EOF'
feat(dialog): open PlaybackViewer for quest fixtures with log_lines

Precedence: log_lines -> viewer, else log_message -> game log.
on_fixture_interacted fires only on first use (last_used_tick < 0);
subsequent interactions replay the log without double-counting.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 10: Smoke-test harness in dev console

**Files:**
- Modify: `src/dev_console.cpp` — attach `log_lines` to `dev_smoke_fixture`

- [ ] **Step 1: Extend the smoke fixture def**

In `src/dev_console.cpp`, find the `quest fixture` subcommand. Modify the def it registers (around lines 513-540):

```cpp
            QuestFixtureDef def;
            def.id = "dev_smoke_fixture";
            def.glyph = '*';
            def.color = 135;
            def.prompt = "Play debug transmission";
            def.log_message = "You nudge the debug fixture. It beeps.";
            def.log_title = "DEV SMOKE TRANSMISSION";
            def.log_lines = {
                "This is line one of the debug transmission.",
                "",
                "Line two. The reveal should advance at thirty chars per second.",
                "Press Space to skip; press Esc to close; re-interact to replay.",
            };
            register_quest_fixture(def);
```

- [ ] **Step 2: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 3: Manual smoke test**

Launch: `./build/astra-dev --term`

Flow:
1. Start a new game (or continue).
2. Press backtick to open the dev console.
3. Run: `quest fixture`
   - Expected: `Planted dev_smoke_fixture at (x,y)`
4. Close console (Esc). The `*` appears east of `@`.
5. Walk adjacent, press `e`. Expected:
   - Modal opens with `[ TRANSMISSION ]` header.
   - Title: `DEV SMOKE TRANSMISSION`.
   - Body reveals at roughly 30 chars/sec with a blinking cursor at the tail.
6. Press Space — body jumps to full.
7. Press Esc — modal closes.
8. Interact again — modal replays. No duplicate `on_fixture_interacted` (visible if a quest targeted this id; not visible here but verified by reading the branch).
9. Save via menu; reload. Walk back and interact. Modal opens; state persists (fixture visible, `last_used_tick` serialized).

Document any deviation in the commit message as a known limitation.

- [ ] **Step 4: Commit**

```bash
git add src/dev_console.cpp
git commit -m "$(cat <<'EOF'
feat(dev): attach audio-log lines to dev_smoke_fixture

Full end-to-end smoke test of the playback viewer: dev-console
plants a QuestFixture with log_title + log_lines; interacting
opens the viewer; Space skips; Esc closes; replay doesn't double-count.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Acceptance Criteria

- `cmake --build build` is clean at every commit.
- `./build/astra-dev --term` launches; interacting with `dev_smoke_fixture` opens the viewer, reveals lines at ~30 chars/sec, skips on Space, closes on Esc.
- Re-interacting replays the log; `on_fixture_interacted` fires only the first time (verified by reading the guard in `dialog_manager.cpp`).
- Save/reload preserves the fixture and its `last_used_tick`.
- No unrelated changes (no refactors outside the File Structure table).
- No save-file version bump (all playback state is transient).

---

## Out of Scope (explicitly deferred)

- Ruins inscriptions — `PlaybackStyle::Inscription` enum value exists but visuals are placeholder copies of `AudioLog` until the ruins work lands.
- Per-log speed override (`chars_per_second` on the def).
- Player-configurable reveal speed.
- Pagination / log history screen.
- SDL backend polish (terminal renderer only).
