# Semantic UI: Migrate render_play() — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate the main game screen (`render_play()` and all its helpers) from raw draw calls to the new semantic UI system. The screen looks identical after migration.

**Architecture:** Replace manual rect computation with `rows()`/`columns()` layout. Replace `ctx.text(..., Color::X)` with `ctx.text({..., .tag=UITag::X})`. Replace `ctx.bar(...)` with `ctx.progress_bar({..., .tag=UITag::HealthBar})`. Replace `ctx.hline()`/`ctx.vline()` with `ctx.separator()`. Replace hardcoded Panel usage with `ctx.panel()`.

**Tech Stack:** C++20

**Branch:** `semantic-ui` (worktree at `.worktrees/semantic-ui/`)

---

## Scope

This migrates these functions in `src/game_rendering.cpp`:
- `render_play()` — main layout orchestration
- `render_stats_bar()` — top stats bar
- `render_bars()` — HP and XP progress bars
- `render_tabs()` — tab bar
- `render_side_panel()` — message log, equipment, ship, wait tabs
- `render_effects_bar()` — bottom effects row
- `render_abilities_bar()` — bottom abilities row

NOT migrated (separate future work):
- Welcome screen overlay (stays on old API — it's a one-off panel)
- Overlay windows (dialog, pause menu, trade, character screen, etc.)
- `render_map()` (already uses WorldContext)

---

## Migration Approach

**Incremental per-function.** Each helper function migrates independently. The layout computation (`compute_layout()`) converts first, then each render function one at a time. After each step, the game compiles and looks identical.

---

## Task 1: Convert Layout to rows()/columns()

**Files:**
- Modify: `src/game_rendering.cpp`
- Modify: `include/astra/game.h` (if rect members change)

- [ ] **Step 1: Read the current rect computation in game.cpp**

The rects are computed in `Game::compute_layout()` or similar. Read it fully.

- [ ] **Step 2: Replace rect computation with layout calls**

Instead of manual arithmetic, use the semantic layout:

```cpp
void Game::compute_layout() {
    screen_w_ = renderer_->get_width();
    screen_h_ = renderer_->get_height();
    screen_rect_ = {0, 0, screen_w_, screen_h_};

    // Main vertical layout
    UIContext root(renderer_.get(), screen_rect_);
    auto rows = root.rows({
        fixed(1),    // stats bar
        fixed(1),    // HP bar row
        fixed(1),    // XP bar row
        fill(),      // main content (map + panel)
        fixed(1),    // bottom separator
        fixed(1),    // effects
        fixed(1),    // abilities
    });

    stats_bar_rect_ = rows[0].bounds();
    // HP/XP bars share row with tabs on the right
    // ... compute from rows[1], rows[2]

    auto main = rows[3];
    int panel_w = screen_w_ * 35 / 100;
    if (panel_w < 30) panel_w = 30;
    if (panel_w > screen_w_ / 2) panel_w = screen_w_ / 2;

    if (panel_visible_) {
        auto cols = main.columns({fill(), fixed(1), fixed(panel_w)});
        map_rect_ = cols[0].bounds();
        separator_rect_ = cols[1].bounds();
        side_panel_rect_ = cols[2].bounds();
    } else {
        map_rect_ = main.bounds();
        separator_rect_ = {0, 0, 0, 0};
        side_panel_rect_ = {0, 0, 0, 0};
    }

    // Tabs share the HP bar row space on the right
    tabs_rect_ = {map_rect_.x + map_rect_.w + 1, rows[1].bounds().y, panel_w, 1};
    hp_bar_rect_ = {0, rows[1].bounds().y, map_rect_.w, 1};
    xp_bar_rect_ = {0, rows[2].bounds().y, map_rect_.w, 1};

    bottom_sep_rect_ = rows[4].bounds();
    effects_rect_ = rows[5].bounds();
    abilities_rect_ = rows[6].bounds();
}
```

Note: the rects are still stored as member variables — the render functions need them. The layout system computes them declaratively instead of manual math.

- [ ] **Step 3: Build and verify**

Run: `cmake -B build -DDEV=ON && cmake --build build 2>&1 | tail -5`

- [ ] **Step 4: Run game, verify layout is identical**

- [ ] **Step 5: Commit**

```bash
git add src/game_rendering.cpp include/astra/game.h
git commit -m "Convert render_play layout to rows()/columns() system"
```

---

## Task 2: Migrate render_bars() to progress_bar()

**Files:**
- Modify: `src/game_rendering.cpp`

- [ ] **Step 1: Read current render_bars()**

- [ ] **Step 2: Replace with semantic progress bars**

```cpp
void Game::render_bars() {
    std::string hp_val = std::to_string(player_.hp) + "/" + std::to_string(player_.max_hp);
    std::string xp_val = std::to_string(player_.xp) + "/" + std::to_string(player_.max_xp);
    int val_w = static_cast<int>(std::max(hp_val.size(), xp_val.size()));
    while (static_cast<int>(hp_val.size()) < val_w) hp_val = " " + hp_val;
    while (static_cast<int>(xp_val.size()) < val_w) xp_val = " " + xp_val;

    int bar_start = 1 + 4 + val_w + 1;

    // HP bar
    {
        UIContext ctx(renderer_.get(), hp_bar_rect_);
        ctx.label_value({.x=1, .y=0, .label="HP:", .label_tag=UITag::TextDim, .value=hp_val, .value_tag=UITag::StatHealth});
        int bar_w = ctx.width() - bar_start - 2;
        if (bar_w > 0) {
            ctx.progress_bar({.x=bar_start, .y=0, .width=bar_w, .value=player_.hp, .max=player_.max_hp, .tag=UITag::HealthBar});
        }
    }

    // XP bar
    {
        UIContext ctx(renderer_.get(), xp_bar_rect_);
        ctx.label_value({.x=1, .y=0, .label="XP:", .label_tag=UITag::TextDim, .value=xp_val, .value_tag=UITag::XpBar});
        int bar_w = ctx.width() - bar_start - 2;
        if (bar_w > 0) {
            ctx.progress_bar({.x=bar_start, .y=0, .width=bar_w, .value=player_.xp, .max=player_.max_xp, .tag=UITag::XpBar});
        }
    }
}
```

- [ ] **Step 3: Build, run, verify bars look identical**

- [ ] **Step 4: Commit**

```bash
git add src/game_rendering.cpp
git commit -m "Migrate HP/XP bars to semantic progress_bar() and label_value()"
```

---

## Task 3: Migrate render_tabs() to tab_bar()

**Files:**
- Modify: `src/game_rendering.cpp`

- [ ] **Step 1: Replace with semantic tab bar**

```cpp
void Game::render_tabs() {
    UIContext ctx(renderer_.get(), tabs_rect_);
    ctx.tab_bar({
        .tabs = {tab_names, tab_names + panel_tab_count},
        .active = active_tab_,
    });

    // Separator below tabs
    UIContext sep(renderer_.get(), {tabs_rect_.x, tabs_rect_.y + 1, tabs_rect_.w, 1});
    sep.separator({});
}
```

Note: `tab_names` is a static array — check the exact type and convert to vector if needed.

- [ ] **Step 2: Build, run, verify tabs look identical**

- [ ] **Step 3: Commit**

```bash
git add src/game_rendering.cpp
git commit -m "Migrate tabs to semantic tab_bar()"
```

---

## Task 4: Migrate render_play() separators

**Files:**
- Modify: `src/game_rendering.cpp`

- [ ] **Step 1: Replace vertical and horizontal separators**

In `render_play()`, replace:
```cpp
DrawContext sep_ctx(renderer_.get(), separator_rect_);
sep_ctx.vline(0, BoxDraw::V, Color::DarkGray);
```
with:
```cpp
UIContext sep_ctx(renderer_.get(), separator_rect_);
// Vertical separator — need to add vertical separator support or use old path
```

Note: the current semantic separator is horizontal only. For vertical separators, either:
- Add a `vertical` flag to `SeparatorDesc`
- Or keep the old vline call for now

The pragmatic choice is to add an `orientation` field to SeparatorDesc. Update `ui_types.h`, `terminal_renderer_ui.cpp`, and the renderer interface if needed.

- [ ] **Step 2: Build, verify**

- [ ] **Step 3: Commit**

```bash
git add src/game_rendering.cpp include/astra/ui_types.h src/terminal_renderer_ui.cpp
git commit -m "Migrate separators to semantic separator() with vertical support"
```

---

## Task 5: Migrate render_stats_bar() to styled_text()

**Files:**
- Modify: `src/game_rendering.cpp`

This is the most complex migration — the stats bar uses many label_value pairs, a custom gap pattern, and a calendar progress bar.

- [ ] **Step 1: Read the full render_stats_bar()**

- [ ] **Step 2: Convert to semantic calls**

The left side and right side use `label_value()` and `styled_text()`. The gap pattern (`<<>>`) and calendar bar are custom — keep them on the old API for now or introduce a new semantic element if clean.

Strategy: migrate what maps cleanly to semantic calls, keep the gap pattern and calendar bar on old API. This is a transitional step — full migration happens when we have richer primitives.

- [ ] **Step 3: Build, run, verify**

- [ ] **Step 4: Commit**

```bash
git add src/game_rendering.cpp
git commit -m "Partially migrate stats bar to semantic label_value() and styled_text()"
```

---

## Task 6: Migrate render_effects_bar() and render_abilities_bar()

**Files:**
- Modify: `src/game_rendering.cpp`

- [ ] **Step 1: Convert effects bar to styled_text()**

Build segments from effect data:
```cpp
void Game::render_effects_bar() {
    UIContext ctx(renderer_.get(), effects_rect_);
    std::vector<TextSegment> segs;
    segs.push_back({"EFFECTS: ", UITag::TextDim});
    // ... build segments from player_.effects
    ctx.styled_text({.x=1, .y=0, .segments=segs});
}
```

- [ ] **Step 2: Convert abilities bar similarly**

- [ ] **Step 3: Build, run, verify**

- [ ] **Step 4: Commit**

```bash
git add src/game_rendering.cpp
git commit -m "Migrate effects and abilities bars to semantic styled_text()"
```

---

## Task 7: Verify Full Screen

- [ ] **Step 1: Clean build**
- [ ] **Step 2: Run game, thorough visual check of entire play screen**
- [ ] **Step 3: Resize terminal — verify layout adapts**
- [ ] **Step 4: Commit any fixups**

---

## Summary

After this plan:
- `render_play()` layout uses `rows()`/`columns()` instead of manual rect math
- HP/XP bars use `progress_bar()` with UITag
- Tab bar uses `tab_bar()`
- Separators use `separator()`
- Effects/abilities bars use `styled_text()`
- Stats bar partially migrated (complex custom elements kept on old API)
- Game looks identical throughout

**Next plan:** Migrate panel-based screens (trade, repair, help, character).
