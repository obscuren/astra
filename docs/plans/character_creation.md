# Plan: Character Creation Screen (Caves of Qud-inspired)

## Context

There is no character creation screen. "New Game" immediately initializes a fixed player and drops into gameplay. We want a multi-step wizard inspired by Caves of Qud — horizontal card selection, breadcrumb trail, point-buy attributes, and a build summary.

---

## Steps

1. **Choose Race** — 6 races shown as cards
2. **Choose Class** — 5 gameplay classes as cards
3. **Allocate Attributes** — point-buy (10 points)
4. **Name** — text input with random option
5. **Build Summary** — three-column review, then start game

---

## Data: 5 New Classes

Add to `PlayerClass` enum in `character.h`:

| Class | Focus | STR | AGI | TOU | INT | WIL | LUC | HP | Skills | SP | Credits |
|-------|-------|-----|-----|-----|-----|-----|-----|----|--------|----|---------|
| Voidwalker | Melee tank | 14 | 10 | 14 | 8 | 10 | 10 | +4 | LongBlade, LongBladeExpertise, Endurance, ThickSkin | 50 | 20 |
| Gunslinger | Ranged agility | 8 | 16 | 10 | 10 | 10 | 12 | +0 | Pistol, SteadyHand, Quickdraw, Acrobatics | 50 | 25 |
| Technomancer | Tinkering/intel | 8 | 10 | 8 | 16 | 14 | 10 | +0 | Tinkering, BasicRepair, Disassemble, Rifle | 100 | 30 |
| Operative | Stealth/social | 10 | 14 | 10 | 12 | 10 | 10 | +2 | ShortBlade, ShortBladeExpertise, Jab, Persuasion, Haggle | 75 | 40 |
| Marauder | Survivalist | 12 | 10 | 16 | 8 | 8 | 12 | +6 | Endurance, ThickSkin, IronWill, LongBlade | 25 | 15 |

---

## Data: Race Stat Modifiers

New `RaceTemplate` struct with per-race attribute modifiers (applied on top of class base):

| Race | STR | AGI | TOU | INT | WIL | LUC | Resistances | Tagline |
|------|-----|-----|-----|-----|-----|-----|-------------|---------|
| Human | +0 | +0 | +0 | +0 | +0 | +2 | — | Adaptable. Bonus luck. |
| Veldrani | -1 | +1 | -1 | +2 | +1 | +0 | Cold +3 | Tall blue-skinned diplomats. Intelligent and perceptive. |
| Kreth | +3 | -2 | +2 | +0 | +0 | -1 | Acid +5, Heat +3 | Stocky mineral-skinned engineers. Strong and durable. |
| Sylphari | -2 | +3 | -2 | +1 | +2 | +0 | Electrical +5 | Wispy luminescent wanderers. Graceful and willful. |
| Xytomorph | +2 | +2 | +1 | -2 | -2 | +1 | Acid +3 | Chitinous predators. Physically superior. |
| Stellari | -1 | -1 | +0 | +3 | +2 | -1 | Heat +5, Elec +3 | Luminous stellar engineers. Ancient minds, frail bodies. |

---

## Architecture: CharacterCreation Class

Follows the `CharacterScreen` pattern: `is_open()`, `open()`, `close()`, `handle_input(int key)`, `draw(int screen_w, int screen_h)`.

**NOT a new GameState.** Instead, a sub-state overlay within MainMenu (like `trade_window_` or `character_screen_`). Game checks `character_creation_.is_open()` in menu input/render handlers.

### Key types

```cpp
enum class CreationStep : uint8_t { Race, Class, Attributes, Name, Summary };

struct RaceTemplate {
    Race race;
    const char* name;
    const char* glyph;           // single char for card display
    const char* tagline;
    int attr_mods[6];            // STR, AGI, TOU, INT, WIL, LUC
    Resistances resist_mods;
    const char* bullets[4];      // description bullets (nullptr-terminated)
};

struct CreationResult {
    Race race;
    PlayerClass player_class;
    PrimaryAttributes attributes;
    Resistances resistances;
    std::string name;
    bool complete = false;
};
```

### Members

```
Renderer* renderer_
CreationStep step_
CreationResult result_
int race_cursor_, class_cursor_, attr_cursor_
int attr_points_remaining_ = 10
int attr_alloc_[6] = {}
std::string name_buffer_
std::mt19937 rng_
```

---

## UI Layout

### Common frame (every step)

```
Row 0:    Starfield backdrop (reuse star_at() from main menu)
Row 1:    Breadcrumb: "Race > Class > Attributes > Name > Summary"
          Completed=Green, Current=Yellow, Future=DarkGray
Row 2:    "CHARACTER CREATION" centered in Cyan
Row 3:    ":choose your race:" centered in DarkGray
Row 4:    ──── separator ────
Rows 5-N: Step content
Row H-2:  ──── separator ────
Row H-1:  "[Esc] Back    [R] Randomize    [Enter] Next"
```

### Step 1 & 2: Card Selection (Race / Class)

```
  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐  ...
  │   @    │  │   Λ    │  │   ◆    │  │   ~    │
  │ Human  │  │Veldrani│  │ Kreth  │  │Sylphari│
  └────────┘  └────────┘  └────────┘  └────────┘
  Selected: Cyan border + Yellow name
  Unselected: DarkGray border + White name

  Description below:
  · Adaptable and resourceful
  · Bonus to Luck (+2)
  · No resistances or weaknesses
  STR +0  AGI +0  TOU +0  INT +0  WIL +0  LUC +2
```

### Step 3: Attributes (point-buy)

```
  "Points remaining: 10"

  ┌─ STR ──┐  ┌─ AGI ──┐  ┌─ TOU ──┐
  │   14   │  │   10   │  │   14   │
  └────────┘  └────────┘  └────────┘
  ┌─ INT ──┐  ┌─ WIL ──┐  ┌─ LUC ──┐
  │    8   │  │   10   │  │   10   │
  └────────┘  └────────┘  └────────┘

  Selected box: Cyan border, Up/Down to adjust
  Breakdown: "Base(10) + Race(+3) + Class(+4) + You(+2) = 19"
```

- 10 point budget, max +8 per stat, min +0 per stat
- Final = class_base + race_mod + player_alloc

### Step 4: Name

```
  "Enter your name, commander."

  ┌────────────────────────────┐
  │  Jax_                      │
  └────────────────────────────┘

  [R] Random name    [Backspace] Delete    [Enter] Confirm
```

Random names: syllable combiner (Zar, Kae, Rix, Vel + on, ia, ex, us, ar).

### Step 5: Summary

```
  ┌─ Attributes ──┐    ┌─── Jax ────────┐    ┌─ Class ───────┐
  │ STR  14       │    │                 │    │ Voidwalker    │
  │ AGI  10       │    │      @          │    │               │
  │ TOU  14       │    │  Human          │    │ Starting:     │
  │ INT   8       │    │                 │    │  Long Blade   │
  │ WIL  10       │    │ HP: 18          │    │  Endurance    │
  │ LUC  10       │    │ Dodge: 3        │    │  Thick Skin   │
  │               │    │ Defense: 6      │    │               │
  │ Resistances:  │    │ Attack: 4       │    │ SP: 50        │
  │  Heat +3      │    │                 │    │ Credits: 20   │
  └───────────────┘    └─────────────────┘    └───────────────┘

  "[Esc] Back    [Enter] Begin Journey"
```

---

## Input Map

| Key | Race/Class | Attributes | Name | Summary |
|-----|-----------|------------|------|---------|
| LEFT/a/h | Prev card | Prev stat | — | — |
| RIGHT/d/l | Next card | Next stat | — | — |
| UP/w/k | — | +1 point | — | — |
| DOWN/s/j | — | -1 point | — | — |
| Enter/Space | Next step | Next (if 0 pts) | Confirm name | Start game |
| Escape | Back / exit | Back | Back | Back |
| R/r | Randomize | Randomize | Random name | Randomize all |
| Printable | — | — | Append char | — |
| Backspace | — | — | Delete char | — |

---

## Game Integration

### Flow

1. User selects "New Game" → `character_creation_.open(renderer_.get())`
2. `handle_menu_input`: if `character_creation_.is_open()`, forward input
3. `render_menu`: if `character_creation_.is_open()`, call `character_creation_.draw()`
4. On completion: `auto cr = character_creation_.consume_result(); new_game(cr);`
5. Escape on step 1 closes creation, returns to menu

### new_game() refactor

Split into:
- `new_game()` — dev mode path (unchanged, uses DevCommander)
- `new_game(const CreationResult& cr)` — applies player choices, then shared init

Shared init = map gen, NPC spawn, galaxy gen, starter gear, welcome screen.

---

## Files

| File | Action | What |
|------|--------|------|
| `include/astra/character.h` | Modify | Add 5 PlayerClass values, `all_gameplay_classes()` |
| `src/character.cpp` | Modify | 5 ClassTemplate definitions, class_name() cases |
| `include/astra/character_creation.h` | **New** | CharacterCreation class, RaceTemplate, CreationResult, CreationStep |
| `src/character_creation.cpp` | **New** | Full wizard implementation, race template data, rendering, input |
| `include/astra/game.h` | Modify | Add `#include`, `CharacterCreation` member, `new_game(CreationResult)` |
| `src/game.cpp` | Modify | Wire into menu input/render, refactor new_game() |
| `CMakeLists.txt` | Modify | Add `src/character_creation.cpp` |

---

## Verification

1. `cmake -B build -DDEV=ON && cmake --build build`
2. Run `./build/astra`, select "New Game"
3. Step through: Race → Class → Attributes → Name → Summary
4. Verify breadcrumbs update, Esc goes back, R randomizes
5. On Summary, press Enter — game starts on The Heavens Above
6. Open character screen (c) — verify name, race, class, attributes match selections
7. Dev Mode menu item still works and skips character creation
