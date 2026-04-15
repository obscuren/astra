# Neutron Star Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `StarClass::Neutron` — a quest-only pulsar remnant with distinct chart visuals (`+` BrightWhite) and no procedural bodies — reachable via `CustomSystemSpec.star_class`.

**Architecture:** Append one `StarClass` enum value, add four switch cases in `src/star_chart.cpp` (name, glyph, color, skip-body-generation), and extend the dev-console `chart create` kind argument. No save-format changes — `StarClass` is already serialized as `u8`.

**Tech Stack:** C++20; existing `star_chart.h/.cpp` and dev console.

**Spec:** `docs/superpowers/specs/2026-04-14-neutron-star-design.md`

**No save version bump.**

**Worktree:** run this plan in `.worktrees/neutron-star` on branch `feat/neutron-star`, forked from `main`.

---

## File Structure

| File | Kind | Responsibility |
|---|---|---|
| `include/astra/star_chart.h` | MODIFY | `StarClass::Neutron` enum value |
| `src/star_chart.cpp` | MODIFY | `star_class_name`/`_glyph`/`_color` cases; neutron early-return in `generate_system_bodies` |
| `src/dev_console.cpp` | MODIFY | `neutron` kind in `chart create` |

Build: `cmake --build build`. Run: `./build/astra-dev --term`.

---

### Task 1: Add `Neutron` enum and star-chart wiring

**Files:**
- Modify: `include/astra/star_chart.h:17-25` — enum
- Modify: `src/star_chart.cpp:245-270` — body-gen early return + switch cases at lines 420, 432, 445

- [ ] **Step 1: Extend enum**

In `include/astra/star_chart.h`, change:

```cpp
enum class StarClass : uint8_t {
    ClassM,  // Red dwarf (most common)
    ClassK,  // Orange
    ClassG,  // Yellow (Sol-like)
    ClassF,  // Yellow-white
    ClassA,  // White
    ClassB,  // Blue-white
    ClassO,  // Blue (rarest)
};
```

to:

```cpp
enum class StarClass : uint8_t {
    ClassM,  // Red dwarf (most common)
    ClassK,  // Orange
    ClassG,  // Yellow (Sol-like)
    ClassF,  // Yellow-white
    ClassA,  // White
    ClassB,  // Blue-white
    ClassO,  // Blue (rarest)
    Neutron, // Pulsar remnant (quest-only, never procedural)
};
```

- [ ] **Step 2: Skip body generation for neutron**

In `src/star_chart.cpp::generate_system_bodies` (around line 245), after the Sgr A* early return at line 256 and BEFORE the habitable-zone switch at line 262, insert:

```cpp
    // Neutron stars: no habitable zone, no procedural planets.
    // Custom systems pre-fill bodies; the idempotence guard at the top
    // prevents this branch from clobbering them.
    if (sys.star_class == StarClass::Neutron) {
        return;
    }
```

Note: the habitable-zone switch (lines 262-270) therefore never sees `Neutron`, so no case needed there.

- [ ] **Step 3: `star_class_name` case**

In `src/star_chart.cpp::star_class_name` (around line 419), before the closing `}`:

```cpp
        case StarClass::Neutron: return "Neutron (Pulsar Remnant)";
```

- [ ] **Step 4: `star_class_glyph` case**

In `src/star_chart.cpp::star_class_glyph` (around line 432), before the closing `}`:

```cpp
        case StarClass::Neutron: return '+';
```

- [ ] **Step 5: `star_class_color` case**

In `src/star_chart.cpp::star_class_color` (around line 445), before the closing `}`:

```cpp
        case StarClass::Neutron: return Color::BrightWhite;
```

- [ ] **Step 6: Build**

Run: `cmake --build build`

Expected: clean build. If `-Wswitch` warnings appear at any other site over `StarClass` (likely in `star_chart_viewer.cpp` or `dev_console.cpp`), add a `case StarClass::Neutron:` that mirrors the most-neutral existing case (`ClassM` or fallthrough). Report each site patched.

- [ ] **Step 7: Commit**

```bash
git add include/astra/star_chart.h src/star_chart.cpp
git commit -m "$(cat <<'EOF'
feat(starchart): StarClass::Neutron — pulsar remnant

Quest-only star class. '+' glyph BrightWhite. No habitable zone,
no procedural bodies — generate_system_bodies early-returns so
quest-supplied bodies in CustomSystemSpec survive intact. Never
selected by the procedural percentile roll.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

Add any other files the build required into the `git add` list.

---

### Task 2: Dev-console `neutron` kind

**Files:**
- Modify: `src/dev_console.cpp` — extend `chart create` kinds

- [ ] **Step 1: Add to the kind whitelist**

Find the block in `src/dev_console.cpp` that validates `kind` (introduced previously for the `chart create` command — looks like `if (a2 != "asteroid" && a2 != "scar" && a2 != "rock")`). Update to accept `neutron`:

```cpp
                if (a2 != "asteroid" && a2 != "scar" &&
                    a2 != "rock" && a2 != "neutron") {
                    log("chart create: unknown kind '" + a2 +
                        "' (expected asteroid|scar|rock|neutron)");
                    return;
                }
```

- [ ] **Step 2: Add the kind branch**

In the same handler, find the `if (kind == "asteroid") { ... } else if (kind == "scar") { ... } else { /* rock */ }` block. Add `neutron` before the `rock` else:

```cpp
            } else if (kind == "neutron") {
                spec.star_class = StarClass::Neutron;
                spec.bodies = { make_landable_asteroid(name + " Fragment") };
            } else { // "rock"
```

(Other kinds keep `StarClass::ClassM` from the earlier code block; the neutron branch overrides.)

- [ ] **Step 3: Update help line**

Find the earlier help log for `chart create`:

```cpp
log("  chart create [kind] [name] - create custom system (kind: asteroid|scar|rock)");
```

Replace with:

```cpp
log("  chart create [kind] [name] - create custom system (kind: asteroid|scar|rock|neutron)");
```

And the inline usage at the end of the chart block:

```cpp
log("Usage: chart create [kind] [name]|reveal <name>|hide <name>");
```

(no change needed; already generic).

- [ ] **Step 4: Build**

Run: `cmake --build build`
Expected: clean build.

- [ ] **Step 5: Smoke test**

```
./build/astra-dev --term
```

1. New game → backtick dev console → `chart create neutron Pulsara`.
2. Expect log: `Created custom neutron system 'Pulsara' id=... at (x, y)`.
3. Close console, open star chart (`m`). Pan to find `Pulsara` near Sol.
4. Expected: `+` in BrightWhite (brighter than the surrounding `*` stars).
5. Confirm the system name renders in the local-view name label.
6. Optional: save, quit to menu, load, confirm Pulsara still renders as Neutron.

- [ ] **Step 6: Commit**

```bash
git add src/dev_console.cpp
git commit -m "$(cat <<'EOF'
feat(dev): chart create neutron <name> — dev-console pulsar-system spawn

Adds 'neutron' to the kind whitelist. Creates a Neutron system
with one landable asteroid body. Smoke-tests the full pathway
from CustomSystemSpec.star_class through chart rendering.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Acceptance Criteria

- `cmake --build build` clean at every commit.
- `chart create neutron Pulsara` creates a system with `+` BrightWhite glyph on the chart.
- Neutron systems have no procedural bodies (only whatever the quest pre-fills).
- Existing non-neutron systems render exactly as before.
- No `-Wswitch` warnings anywhere in the codebase.

---

## Out of Scope (explicitly deferred)

- Pulsar animation.
- Procedural spawn weighting for neutron stars.
- Gravitational / travel effects.
- Neutron-binary systems.
- Nova arc wiring (happens in the quest spec once all primitives land).
