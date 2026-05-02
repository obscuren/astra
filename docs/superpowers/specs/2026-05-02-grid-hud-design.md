# Grid HUD Redesign (Plan 6) — Design Spec

**Date:** 2026-05-02
**Status:** Draft, awaiting user review
**Branch (target):** `feature/grid-hud`
**Parent specs:** `2026-05-01-grid-expansion-design.md` (Plan 5)
**Predecessor:** Plan 5 shipped on `main` (tag-driven LAN, procedural sectors, deep-grid base, WarpAnchors, nmap/ping/jack/lore CLI).

---

## 1. Concept

Plan 5 stabilised the *gameplay* shape of the Grid: real LAN sectors, multi-map persistence, IP-driven nmap, deep-Grid base. The *UI* still wears its Plan 4 stub — a 20-line right-pane sidebar reading `HP / RAM / Trace / Heat`, with the playfield filling the rest of the screen. Plan 6 redesigns the in-Grid HUD around the now-stable mechanics so the screen *reads* like cyberspace, not a game with a hacking minigame bolted on.

### Pillars

- **The Grid is a place you visit, not a mode you enter.** Render the world (overworld/dungeon + its UI) *behind* the Grid, in monochrome. The player's meat body is dimly visible at all times — atmospheric, not a gameplay system.
- **Tron Wireframe theme, distinct from the world UI.** Double-line box drawing, all-caps headers, block-bar gauges. Cyan = *you*; Magenta = *hostile*. No yellow/red — the palette stays pure.
- **The HUD is a parallel of the world UI, not a replica.** Same skeleton (top status, side strip, viewport, side pane, bottom action bar) so muscle memory transfers, but every slot carries Grid-specific content.
- **Reuse, don't reinvent.** Telegraph for targeting. MenuState for any future picker. Existing log API for messages. The Plan 6 blast radius is the in-Grid render + input + program targeting.

### Non-goals

- PDA Hacking tab redesign (the CLI surface stays as Plan 5 shipped it).
- Nmap widget redesign (rendering polish only if it conflicts with the Tron palette).
- New programs, new ICE kinds, new mechanics.
- Real-body damage / meatspace vulnerability while jacked in (Plan 7).
- F2/F3/F4 right-pane content beyond messages (Plan 7).
- Help overlay, in-Grid context-help (Plan 7 — `?` removed for now).
- Footer hint strip (dropped — players learn keys from external help).
- AI contacts UI, lore-archive viewer redesign, hostile-QH gating model (Plan 7).
- SDL renderer parity for the monochrome filter (terminal-only this cut).

---

## 2. Architecture

### New / extended subsystems

| Unit | Header | Responsibility |
|---|---|---|
| Monochrome render flag | `renderer.h` (extended) | `set_monochrome(bool)` — desaturate every subsequent `draw_glyph` / `draw_string` color to grey-scale. |
| Grid window chrome | `grid_renderer.cpp` (extended) | Tron double-line border, 70%×70% centered overlay rect, internal layout helpers. |
| Grid window layout helpers | `grid_renderer.cpp` (split) | `draw_top_status`, `draw_deck_strip`, `draw_playfield`, `draw_log_pane`, `draw_program_bar`, `draw_window_chrome`. |
| GridSession log | `grid_session.h/cpp` (extended) | Per-session ring buffer + `push_log` / `recent_lines` / `clear` API. |
| ProgramDef targeting | `program.h` (extended) | `TargetingMode { Self, Tile }`, `TelegraphSpec`, target predicate fn-ptr. |
| Program target predicates | `program_effects.cpp` (extended) | Per-program `valid_target` predicate next to its effect. |
| Dev toggle for monochrome | `dev_console.cpp` (extended) | `:mono on/off` (or similar) for verifying the filter standalone before the rest lands. |

### Existing systems reshaped

- `grid_renderer.cpp` — 220-line single function explodes into 6 helpers + an orchestrator. Window-rect-aware drawing replaces full-screen drawing.
- `grid_input.cpp` — numeric keys 1–8 wired through Telegraph for `Tile` programs, immediate fire for `Self` programs. `q` / `m` / `?` removed. `Q` (hard jack-out) and walk-onto-`⊙` (safe disconnect) preserved.
- `program_effects.cpp` — auto-pick paths retired; every `Tile` program receives a real `(tx, ty)` from Telegraph confirm. Self-targeted programs unchanged.
- `Game::render()` — when a Grid session is active, render order becomes: `set_monochrome(true)` → world + world UI → `set_monochrome(false)` → Grid window. When no session, normal world render only.
- `terminal_renderer.cpp` — color-write paths consult the monochrome flag and substitute a desaturated value.

### File-size discipline

`grid_renderer.cpp` target: ≤ 350 lines after the helper split. `program_effects.cpp` already at ~400; the predicates add ~50. `grid_input.cpp` (~315) gains the Telegraph-wiring code (~80). No extraction needed unless any file crosses 500.

---

## 3. Window & layout

### Geometry

- **Window size:** `win_w = floor(screen_w * 7 / 10)`, `win_h = floor(screen_h * 7 / 10)`. Re-computed every frame so terminal resize works.
- **Window position:** centered. Top-left = `((screen_w - win_w) / 2, (screen_h - win_h) / 2)`.
- **World behind:** rendered in monochrome at full screen. Visible around the Grid window's edges. World state is frozen — Grid takes its own ticks.

### Internal layout (top to bottom inside the window)

```
╔═══════════════════════════════════════════════╦══════════════════╗
║ ▶ GRID  HEAVENS.ABOVE › TURRET-13  10.55.42.7  TRACE ▮▮▮▮▯ 40%  ║   row: top status
╠═══════════════════════════════════════════════╣                  ║
║ HP ▮▮▮▮▮▮▮▮▮▯ 100/100  RAM ▮▮▮▯▯ 18/30  HEAT ▮▯▯ 4/40           ║   row: deck strip
╠═══════════════════════════════════════════════╩══════════════════╣
║                                              ║ > breach.exe     ║
║          .  .  .  ⌬ . . . . .                ║   firewall down  ║   playfield + log pane
║         .  ⊙  .  . . . . . . .               ║                  ║
║                                              ║ [F1] Messages    ║
╠══════════════════════════════════════════════╩═══════════════════╣
║  ┃1┃ICE  ┃2┃BRC  ┃3┃DEC  ┃4┃PUL•3  ┃5┃  ┃6┃  ┃7┃  ┃8┃           ║   row: program bar
╚══════════════════════════════════════════════════════════════════╝
```

**Row heights:**
- Top status: 1 row
- Deck strip: 1 row (with separator above and below: 3 rows total accounting for chrome)
- Playfield + log pane: fills the remainder
- Program bar: 1 row

**Column split for the playfield row:**
- Log pane: ~22 chars wide (right edge of window).
- Playfield: window inner width minus log pane minus chrome.

### Right pane

- Live message log only. Reads `GridSession::recent_lines(n)` where `n` = available rows in the pane.
- Header row inside the pane: `[F1] Messages` — labels which pane is active.
- F1 is reserved as the pane's identity key. **F1 is a no-op for now** — only one pane exists. Future panes (programs detail, ICE inspector, Atlas mini-view) can rotate behind F1/F2/F3 in Plan 7 without re-architecting.

### Top status — breadcrumb identity

Format: `▶ GRID  <REGION> › <SUB>  <IP>  TRACE <gauge> <pct>%`

- `<REGION>` = the LAN's display name (uppercase). E.g. `HEAVENS.ABOVE`.
- `<SUB>` = the current node's short label. When in the LAN root, `<SUB>` is omitted (separator hidden). When in a per-device Subnet, `<SUB>` is the device hostname (e.g. `TURRET-13`). When in the deep-Grid Anchor, the breadcrumb reads `DEEPGRID › ATLAS` or `DEEPGRID › YOUR.ANCHOR`.
- `<IP>` = the player's IP within the active LAN (already exists on `LanMetadata`).
- Trace gauge fills cyan → bright-cyan → magenta → bright-magenta as percentage climbs (see §5).

### Deck strip

Three values, each as `LABEL <bar> <num>/<max>`:
- `HP <bar> <hp>/<max_hp>` — avatar HP within the session.
- `RAM <bar> <current>/<max>` — current RAM available for program firing.
- `HEAT <bar> <current>/<max>` — current cyberdeck heat.

Bar segments: `▮` filled, `▯` empty. Bar width is fixed (10 segments for HP, 5 for RAM, 5 for HEAT) so labels align across sessions.

### Program bar

Up to 8 slots, drawn left-to-right. Each slot:

```
┃<n>┃<ABBREV>[•<cd>]
```

- `<n>` = hotkey 1–8.
- `<ABBREV>` = 3-letter program code: `ICE` icebreaker_lite, `BRC` breach, `DEC` decrypt, `PUL` pulse_hammer, `HIJ` daemon_hijack, `GHO` ghost_trace, `COD` cooldown.
- `[•<cd>]` = present only when slot is on cooldown; shows remaining turns.
- Empty slots render as `┃<n>┃   ` (label-only, dim).
- Slots whose program the player can't afford (RAM / heat / cooldown) render dim cyan instead of bright cyan.
- The slot whose Telegraph is currently open inverse-videos.

Slot order = load order in the cyberdeck. No rearrangement UI in this cut. Player rearranges via the PDA Hacking tab if needed.

---

## 4. Theme rules

### Palette

- **Cyan** — *you* and your stuff. Chrome borders, headers, labels. Your HP/RAM bars. Your avatar (`@`). Your loaded program slots when available. Your Trace gauge fill (low). The `>` log prefix for self-output.
- **Magenta** — *hostile / system*. ICE glyphs (white/gray/black variants from Plan 3 swap their hues into the magenta family). Firewall tiles. Trace gauge fill when high. Heat bar fill above 80%. The `>>` log prefix for system events.
- **Bright variants** — used for *escalation*. Trace 25–49% bright-cyan, 75–100% bright-magenta. Alerted ICE one-frame inverse-video on alert tick.
- **Greyscale (via monochrome flag)** — only the world behind the window. Inside the Grid window, every glyph is full-colour Cyan/Magenta family.

No yellow, no red, no green inside the Grid window. The Tron palette is pure.

### Border / chrome

- All Grid window borders use double-line box-drawing: `╔ ╗ ╚ ╝ ║ ═ ╠ ╣ ╦ ╩ ╬`.
- Inner separators (between top-status / deck strip / playfield / program bar) use `╠ ═ ╣` and `╦ ╩` for the playfield/log column split.
- Wall tiles inside the playfield keep their Plan 5 box-drawing-connected glyph (single-line `─ │ ┌ ...`). Visual distinction: chrome is double-line, sector walls are single-line.

### Gauges

Two gauge styles, used consistently:
- **Bar gauge** (deck strip, trace at top): `▮▮▮▮▮▯▯▯▯▯` — fixed-width segments, fill from left.
- **Pip gauge** (slot cooldown indicator): inline numeric `•3` — no bar, just the count.

### Selection / activity highlight

- Selected / active uses **inverse-video** or **bright variant**, never a colour shift. This preserves the cyan-vs-magenta meaning.
- The active program slot during Telegraph: inverse-video.
- The Telegraph cursor: bright-cyan (you), AoE preview: cyan dim background, Telegraph dest tile: bright-cyan.

### Log voice

All in-Grid log lines route through the GridSession log and follow this tone:

| Type | Format | Example |
|---|---|---|
| Self program output | `> <name>.exe` then `  <result>` | `> breach.exe` / `  firewall down` |
| Self program success (one-line) | `> <name>.exe — <result>` | `> ghost_trace.exe — trace concealed (4 cycles)` |
| Block / pre-flight fail | `[BLOCK] <name> — <why>` | `[BLOCK] icebreaker_lite — 18 RAM required, 12 available` |
| Invalid target | `[ERR] <why>` | `[ERR] no firewall at coordinates` |
| System event | `>> <event>` | `>> intrusion detected — trace +5` |
| Warning | `[WARN] <message>` | `[WARN] heat critical` |

Format is a tone guide, not a strict schema. Existing `GridSession::push_log` API is plain string in / out — call sites carry the convention.

---

## 5. Interaction model

### Key map

| Key | Action |
|---|---|
| `hjkl`, arrows | move avatar |
| `1`–`8` | fire program in slot N |
| `Esc` | cancel active Telegraph |
| `Q` (Shift-q) | hard jack-out |
| walk onto `⊙` | safe disconnect |

`q`, `m`, `?`, `f` — **all unbound**. Help, nmap-from-Grid, and any picker overlay are deferred to Plan 7.

### Program firing flow

Each loaded program declares a `TargetingMode`:

- `Self` — fires immediately on key press. Pre-flight gate first; success debits RAM/heat, sets cooldown.
- `Tile` — pre-flight gate; on pass, call `Telegraph::begin(spec, avatar_x, avatar_y, on_confirm)`. Telegraph owns cursor draw, AoE preview, hjkl/Enter/Esc input. The `on_confirm` callback validates the program's `valid_target` predicate against the dest tile, fires + debits on hit, logs `[ERR]` and re-opens Telegraph on miss.

### Per-program targeting table

| Program | Mode | Spec | Predicate |
|---|---|---|---|
| `icebreaker_lite.exe` | Tile | `Burst { radius=0, range=4 }` | tile contains an ICE |
| `breach.exe` | Tile | `Burst { radius=0, range=1 }` | tile is `Firewall` / `Gateway` / `DeepGridGateway` |
| `decrypt.exe` | Tile | `Burst { radius=0, range=1 }` | tile is `EncryptedFile` |
| `daemon_hijack.exe` | Tile | `Burst { radius=0, range=4 }` | tile contains an ICE |
| `pulse_hammer.exe` | Tile | `Burst { radius=1, range=4 }` | dest tile passable |
| `ghost_trace.exe` | Self | — | — |
| `cooldown.exe` | Self | — | — |

Ranges (4 / 1 / 1 / 4 / 4) are starting values — playtest may adjust them but they don't change the architecture.

### Pre-flight gating

Press a slot's number with insufficient RAM, heat over cap, or program on cooldown → no Telegraph opens. Log a `[BLOCK]` line. The slot's bar label flashes inverse-video for one frame.

### Esc precedence

When `Esc` is pressed:
1. If a Telegraph is active → cancel Telegraph, no other side effect.
2. Otherwise → no-op (system menu / world Esc are not reachable from inside the Grid in this cut).

### Input gating

`Game::handle_input` already dispatches to `grid_input::handle` exclusively when a session is active. Plan 6 audits this:
- World ability bar quickslots (1–8 in world mode) — already guarded; in Grid these go through Grid's program bar instead.
- Movement (hjkl/arrows) — already handled by Grid's `try_move`.
- F-key panel toggles in world mode — must not fire while a session is active. Audit in `game_input.cpp`.
- Dev console (`` ` ``) — explicitly allowed to pass through (development affordance).

---

## 6. State visualization

### Trace tier — top status gauge

Trace runs 0–100. Fill colour shifts:

| Range | Fill colour |
|---|---|
| 0–24% | `Color::Cyan` |
| 25–49% | `Color::BrightCyan` |
| 50–74% | `Color::Magenta` |
| 75–100% | `Color::BrightMagenta` |

The numeric percentage and `TRACE` label stay in chrome cyan. Only the *bar fill* shifts. This is consistent with "magenta = hostile presence rising."

### Heat — deck strip

| Range | Fill colour |
|---|---|
| 0–80% of cap | `Color::Cyan` |
| > 80% of cap | `Color::Magenta` |
| At cap on the over-cap log line | `HEAT` label inverse-videos for one frame |

### RAM low

When `current_ram < min(loaded_program_costs)`:
- The `RAM` label and bar fill render dim.
- Programs the player can't afford render dim on the program bar.

### Alerted ICE

When an ICE flips to an *alerted* state (existing Plan 3 mechanic), its glyph renders bright-magenta and inverse-videos for one frame on the alert tick. Subsequent frames return to its kind glyph (white/gray/black mapped into magenta family — see §4).

### Active Telegraph

While a Telegraph is open for slot X, slot X inverse-videos. On confirm/cancel, slot returns to normal.

---

## 7. Implementation surface

### Touched files

**Extended:**
- `include/astra/renderer.h`, `src/terminal_renderer.cpp` — `set_monochrome` flag + desaturation in color-write paths.
- `include/astra/grid_session.h`, `src/grid_session.cpp` — log ring buffer + API.
- `include/astra/program.h` — `TargetingMode`, `TelegraphSpec`, `valid_target` fn-ptr fields on `ProgramDef`.
- `src/program_effects.cpp` — per-program predicates; auto-pick paths retired.
- `src/grid_input.cpp` — number-key dispatch + Telegraph wiring; remove `q`/`m`/`?`.
- `src/grid_renderer.cpp` — split into helpers; window-rect-aware drawing; new layout sections (top status, deck strip, log pane, program bar).
- `src/dev_console.cpp` — `:mono on/off` toggle for the monochrome filter.

**Untouched:**
- `src/pda_hacking_tab.cpp` (CLI surface)
- `src/grid_nmap_widget.cpp` (nmap rendering) — only verify the existing widget still works on top of the Tron window when invoked from the PDA terminal.
- `src/dialog_manager.cpp` — fixture-menu Jack In flow already correct.
- `src/hacking_system.cpp` — `jack_in` / `jack_out` boundary log calls stay on world log.

### `Renderer::set_monochrome` semantics

- Single boolean flag on the renderer. Default false.
- When true, the next `draw_glyph(x, y, glyph, color)` and `draw_string(x, y, text)` calls compute a desaturated equivalent colour and emit that instead.
- Desaturation rule (terminal): map any colour to one of `BrightWhite` / `White` / `BrightBlack` based on perceptual brightness; preserve only luminance distinction. Specific table in `terminal_theme.cpp` or inline.
- SDL renderer parity is **not in scope** for this cut. SDL renderer's `set_monochrome` becomes a no-op for now (filed for later); the SDL build still compiles.

### `GridSession` log API

```cpp
// In GridSession (struct):
std::deque<std::string> log_lines_;     // ~64 lines max
void push_log(const std::string& line);
const std::deque<std::string>& log_lines() const;
void clear_log();
```

`push_log` enforces the cap (drop oldest when full). The log is session-scoped — destroyed with the `GridSession`.

### `ProgramDef` extension

```cpp
enum class TargetingMode : uint8_t { Self, Tile };

struct ProgramDef {
    // ... existing fields ...
    TargetingMode targeting = TargetingMode::Self;
    TelegraphSpec telegraph_spec;          // valid only when targeting == Tile
    bool (*valid_target)(const GridSession&, int x, int y) = nullptr;  // valid only when targeting == Tile
};
```

Predicate functions live in `program_effects.cpp` next to each program's effect, named `<program>_valid_target`. They take the session + tile coords and return `bool`.

### Telegraph hookup

The `grid_input::handle` numeric-key handler:

```cpp
void on_program_key(Game& game, GridSession& s, int slot_idx) {
    auto& cd = ...;  // cyberdeck
    auto* def = find_program(...);
    if (!def) return;
    if (!can_afford(s, *def)) { s.push_log("[BLOCK] ..."); return; }

    if (def->targeting == TargetingMode::Self) {
        fire_program(game, s, *def, /*tx=*/-1, /*ty=*/-1);
        return;
    }

    auto on_confirm = [&game, &s, def](const TelegraphResult& r) {
        if (!def->valid_target(s, r.dest_x, r.dest_y)) {
            s.push_log("[ERR] ...");
            return;  // Telegraph re-opens — see Telegraph's contract
        }
        fire_program(game, s, *def, r.dest_x, r.dest_y);
    };
    game.telegraph().begin(def->telegraph_spec, s.avatar_x, s.avatar_y, on_confirm);
}
```

Existing `Game::telegraph()` accessor + `Telegraph::begin` / `cancel` / `preview` are reused as-is.

---

## 8. Implementation order (cuts)

The plan is implemented as discrete cuts. Each cut ends with a green build *and* a user-verifiable behavior. Build before commit; commit after each cut so we can revert cleanly.

**Cut 1 — Monochrome filter + dev toggle.**
Standalone, self-contained. `Renderer::set_monochrome(bool)`, terminal desaturation, `:mono on/off` dev command. No Grid integration yet. *User verifies the world UI greys out correctly when the toggle is on.*

**Cut 2 — Grid window geometry + chrome.**
Replace the full-screen Grid render with a 70%×70% centered overlay window, double-line border. World rendered behind in monochrome (auto-set when session active). Layout slots empty-stubbed inside the window — top status, deck strip, playfield, log pane, program bar all draw a placeholder.

**Cut 3 — Layout slots populated.**
Top status (breadcrumb + IP + Trace gauge with tier-colour fill). Deck strip (HP/RAM/Heat bars). Log pane (reads from new `GridSession::log_lines_`). Program bar (loaded slots with abbrev + cooldown pip + dim-when-unaffordable). All non-interactive — just visual.

**Cut 4 — Targeting via Telegraph for programs.**
Wire each program's `TargetingMode` + `TelegraphSpec` + `valid_target` predicate. Number keys 1–8 dispatch via Telegraph. Auto-pick paths in `program_effects.cpp` retired. Pre-flight gating with `[BLOCK]` log lines. `[ERR]` log on invalid target.

**Cut 5 — GridSession log isolation.**
Move every Grid-scope `game.log(...)` call to `session.push_log(...)`. Boundary events (`jack_in` / `jack_out`) stay on `game.log`. Right log pane already reads from the new buffer (Cut 3); this cut just re-points the call sites.

**Cut 6 — Polish + audit pass.**
- Audit world keys leaking into Grid input.
- Verify nmap widget still renders cleanly when invoked from PDA Hacking tab on top of the Tron window.
- Pulse-hammer AoE preview visual check.
- Trace tier transitions visual check.
- Active-program-slot inverse-video check.
- Heat critical / RAM low transitions visual check.

---

## 9. What this spec does NOT decide

These are intentionally left for Plan 7 or follow-up polish, not omissions:

- **Footer hint strip / context help.** Help is a separate concern. `?` was dropped; Plan 7 may add a help overlay or persistent footer.
- **F2/F3/F4 right-pane content.** Only F1 = Messages exists in Plan 6. Future panes (programs detail, ICE inspector, mini-map) come in Plan 7.
- **`m` for nmap-from-Grid.** Today nmap is reachable only via the PDA terminal. Plan 7 may bind `m` once the gameplay shape settles.
- **AI contacts UI, lore-archive viewer redesign.** Plan 7.
- **Real-body damage, meatspace vulnerability while jacked.** Plan 7 — design decision, not UI.
- **SDL renderer monochrome support.** Filed for SDL revisit (currently deferred).
- **Telegraph cursor visual style differences in Grid vs world.** Reuse Telegraph's existing visual; only swap colours via the monochrome flag and ambient cyan/magenta.
- **Resize edge cases.** 70%×70% computed each frame handles resize cleanly. If `win_h` drops below ~14 rows the layout will run out of space — out of scope to handle that for now (the game's minimum supported terminal size is well above that).

---

## 10. Open questions

None. All decisions captured during brainstorming.

---

## 11. Cross-references

- Plan 5 spec: `docs/superpowers/specs/2026-05-01-grid-expansion-design.md`
- Plan 5 plan: `docs/superpowers/plans/2026-05-01-grid-expansion.md`
- Plan 5 handoff: `docs/plans/2026-05-02-plan-5-handoff.md`
- Telegraph API: `include/astra/telegraph.h`
- Existing Grid renderer: `src/grid_renderer.cpp`
- Existing PDA Hacking tab: `src/pda_hacking_tab.cpp`
- ProgramDef: `include/astra/program.h`, `src/program.cpp`
