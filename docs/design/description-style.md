# Item Description Styling Convention

How to write and render item descriptions in Astra. Applies to all implants,
cortex variants, cyberdecks, and any future item whose tooltip needs mechanical
clarity beyond a raw stat block.

---

## 1. Structure

Every description that carries mechanics uses a two-part layout:

```
<flavor text — uncolored prose, one or more sentences>

<mechanical line — tag prefix + body>
```

Flavor and mechanics are separated by **two newlines** (`\n\n`), which the
renderer turns into a blank line between the two blocks.

Items that are pure stat bumps (e.g. Standard Plate, Servo Grip) use flavor
only — the structured stat block in the item panel already shows the numbers.

---

## 2. Mechanical-line tag prefixes

Three white-colored prefixes identify what kind of mechanic follows:

| Tag | When to use |
|---|---|
| `Passive:` | Always-on effect, or one that procs automatically on a recurring action (every melee hit, every ranged hit, etc.) |
| `Active:` | Player-triggered ability — requires a key press to fire |
| `Trigger:` | Conditional auto-fire on a specific game event (HP threshold, first hit of a level, etc.) |

Multiple mechanical lines on a single item are each their own paragraph
(separated by `\n\n`), each with its own tag.

---

## 3. Inline color rules

| Content | Color | How to produce |
|---|---|---|
| `Passive:` / `Active:` / `Trigger:` prefix | White | `desc_passive()` / `desc_active()` / `desc_trigger()` helpers |
| Dice notation (`1d4`, `2d6+1`) | Yellow | `desc_dice("1d4")` |
| Damage type names (`plasma`, `kinetic`, …) | Damage-type color | `display_name(DamageType::X)` — already wraps in the correct color |
| Effect names (`Bleed`, `EmpDisabled`, `Burn`, …) | Effect's canonical color | `desc_effect("Bleed", Color::Red)` |
| Keybind references (`[d]`) | Yellow | `desc_key('d')` — produces `[d]` in yellow |
| Percentages and durations (`8%`, `5 turns`) | Uncolored | plain string |

Effect canonical colors:
- `Bleed` → Red
- `EmpDisabled` → Cyan
- `Burn` → Yellow
- `Slow` → Cyan
- `Stunned` → Yellow

---

## 4. Helper functions

All helpers live in `include/astra/desc_style.h` (include it from any item
builder that needs it).

```cpp
// White tag prefix + body
std::string desc_passive(const std::string& body);
std::string desc_active(const std::string& body);
std::string desc_trigger(const std::string& body);

// Yellow dice notation, e.g. "1d4"
std::string desc_dice(const std::string& dice);

// Yellow "[d]" keybind bracket
std::string desc_key(char k);

// Named effect in an explicit color
std::string desc_effect(const std::string& name, Color c);
```

Use plain string concatenation to compose the mechanical body:

```cpp
it.description =
    "Flavor sentence here.\n\n"
    + desc_passive("8% chance per ranged hit to fire a rocket dealing "
                   + desc_dice("1d4") + " " + display_name(DamageType::Plasma)
                   + " to the target and 4 cardinal neighbors.");
```

---

## 5. Example descriptions by tag type

### Passive (always-on / proc-on-action)

```cpp
// Spike Cortex
it.description =
    "Tuned for offensive net-running. Channels more deck RAM and absorbs more heat.\n\n"
    + desc_passive("+2 RAM cap, +1 Heat capacity while jacked in.");

// Vibro-Tip Fingers (proc with inline effect color)
it.description =
    "Micro-vibrating fingertip implants. Adds kinetic edge to melee, occasionally rending soft tissue.\n\n"
    + desc_passive("Melee +1 " + display_name(DamageType::Kinetic) + " damage. "
                   "10% chance per melee hit to apply "
                   + desc_effect("Bleed", Color::Red) + " (3 turns @ 1 kinetic/turn).");
```

### Active (player-triggered)

```cpp
// Burst Pistons
it.description =
    "Spring-loaded leg pistons. Explosive burst of speed in a single stride.\n\n"
    + desc_active("Press " + desc_key('d') + " then a cardinal direction to dash up to 3 tiles. "
                  "Costs a turn. 8-turn cooldown; resets when combat ends.");
```

### Trigger (conditional auto-fire)

```cpp
// Adrenal Pump
it.description =
    "A subdermal stim reservoir. Triggers once per fight when health drops critically low.\n\n"
    + desc_trigger("When HP drops below 30% of maximum, gain +1 Quickness for 5 turns. "
                   "Fires once per combat.");

// EMP Buffer (with inline effect color)
it.description =
    "Sacrificial Faraday weave around the heart. Absorbs the first electric or EMP strike each level.\n\n"
    + desc_trigger("First incoming electric or EMP attack each level is fully absorbed (damage and "
                   + desc_effect("EmpDisabled", Color::Cyan) + " both blocked).");
```

---

## 6. Renderer notes

The description block in `draw_item_info` (`src/ui.cpp`) renders descriptions
with `ctx.text_rich()` and measures line width with
`UIContext::rich_visible_length()`. The word-wrap loop:

- Splits first on explicit `\n` characters (paragraph breaks).
- Word-wraps each paragraph byte-by-byte, counting visible columns.
- Never bisects a `COLOR_BEGIN <byte> ... COLOR_END` marker triplet.
- Blank paragraphs (from `\n\n`) produce a blank separator line.

Keep mechanical lines reasonably short. The panel is typically ~30–40 columns
wide — a single `Passive:` sentence should fit in one or two wrapped lines.

---

See also: [items.md](items.md) for the item catalog, [mechanics.md](mechanics.md)
for effect definitions and stat formulas.
