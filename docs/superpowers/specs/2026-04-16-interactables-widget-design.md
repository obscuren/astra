# Interactables Widget — Design

**Date:** 2026-04-16
**Status:** Draft — not yet implemented

## Summary

Add a fourth side-panel widget, **Interactables**, that lists the NPCs, interactable fixtures, and ground items currently within the player's field of view. Matches the visual footprint of the existing Minimap widget (~10 rows), toggled via F4, renders one entry per row as `<glyph> <name>`. Sorted by Chebyshev distance ascending; overflow shown as `… N more`. Empty list displays a dim `(nothing nearby)`.

The widget's job: tell the player at a glance what's around to interact with, without forcing them to pan the map or guess from glyphs.

---

## Goals

- A new toggleable side-panel widget that fits the existing bitfield/F-key conventions.
- Populated from live FOV — never leaks information from unseen tiles.
- Three entry types: NPCs, interactable fixtures, ground items.
- Sorted closest-first. Deterministic tie-break so the list doesn't flicker turn-to-turn.
- Row format uses existing display helpers: `display_name(Npc)`, `display_name(Item)`, and a fixture-name helper.
- Graceful overflow and empty states.

## Non-goals

- Click / select / press-a-key-to-interact from the widget (interact remains walk-adjacent + Space).
- Keyboard focus or selection within the widget.
- Stairs, hatches, POI tiles, doors that aren't flagged `interactable`.
- Cardinal-direction or distance columns.
- Persistence of a selected row between frames.
- Sorting by quest marker / importance.
- Any save-file changes.

---

## Data Model

### Widget enum extension

`include/astra/game.h`:

```cpp
enum class Widget : uint8_t {
    Messages      = 0,
    Wait          = 1,
    Minimap       = 2,
    Interactables = 3,
};
static constexpr int widget_count = 4;

struct WidgetKeys {
    int keys[widget_count] = { KEY_F1, KEY_F2, KEY_F3, KEY_F4 };
    const char* labels[widget_count] = { "F1", "F2", "F3", "F4" };
};
```

### Widget height

`src/game_rendering.cpp` — `widget_desired_height`:

```cpp
case Widget::Interactables: return 10;
```

Same as Minimap. Gives ~8 inner rows for entries after frame chrome.

---

## Scan Algorithm

Called once per render from `render_interactables_widget`. Collects entries then sorts.

```cpp
struct InteractableEntry {
    int dist;            // Chebyshev from player
    int category;        // 0 = Npc, 1 = Fixture, 2 = GroundItem (tie-breaker)
    char glyph;
    Color color;
    std::string text;    // pre-composed display_name string
};
```

### NPC pass

```cpp
for (const auto& npc : world.npcs()) {
    if (!npc.alive()) continue;
    if (vis.get(npc.x, npc.y) != Visibility::Visible) continue;
    int d = std::max(std::abs(npc.x - px), std::abs(npc.y - py));
    entries.push_back({d, 0, npc_glyph(npc.npc_role, npc.race),
                       npc_color(npc), display_name(npc)});
}
```

### Fixture pass

Iterate `map.fixtures_vec()` paired with `map.fixture_ids()` — a fixture is placed iff `fixture_ids()[y*w + x] == its index`. To avoid scanning the whole map, iterate tiles within a conservative view-radius box around the player (e.g. player ±20 tiles) and check each tile's fixture id.

```cpp
for (int y = py - 20; y <= py + 20; ++y) {
    for (int x = px - 20; x <= px + 20; ++x) {
        if (!map.in_bounds(x, y)) continue;
        if (vis.get(x, y) != Visibility::Visible) continue;
        int fid = map.fixture_id(x, y);
        if (fid < 0) continue;
        const FixtureData& f = map.fixture(fid);
        if (!f.interactable) continue;
        int d = std::max(std::abs(x - px), std::abs(y - py));
        std::string name = (f.type == FixtureType::QuestFixture
                            && !f.quest_fixture_id.empty())
                           ? quest_fixture_display_name(f.quest_fixture_id)
                           : fixture_type_name(f.type);
        entries.push_back({d, 1, fixture_glyph(f.type),
                           fixture_color(f.type), name});
    }
}
```

### Ground item pass

```cpp
for (const auto& gi : world.ground_items()) {
    if (vis.get(gi.x, gi.y) != Visibility::Visible) continue;
    int d = std::max(std::abs(gi.x - px), std::abs(gi.y - py));
    entries.push_back({d, 2, gi.item.glyph(), gi.item.color(),
                       display_name(gi.item)});
}
```

### Sort

```cpp
std::sort(entries.begin(), entries.end(),
          [](const auto& a, const auto& b) {
    if (a.dist != b.dist) return a.dist < b.dist;
    return a.category < b.category;
});
```

Distance ascending; NPC > Fixture > Item on ties. Deterministic — no flicker.

---

## Rendering

Each row: `<glyph> <name>` using existing `UIContext::put` for the glyph cell and `text_rich` (or `text`) for the name. Name color default white unless the entry's display_name already contains inline color markers (items do).

### Overflow

Visible row count = `panel_h - 2` (frame) or whatever `widget_desired_height - chrome` works out to. If `entries.size()` exceeds visible rows, render the first `N-1` entries then a final row reading `… X more` in `Color::DarkGray`, where `X = total - (N-1)`.

### Empty state

If `entries.empty()`:

```
(nothing nearby)
```

Rendered in `Color::DarkGray` centered horizontally (or left-aligned at column 1 — the plan picks the cheaper option).

### Header / frame

Match the pattern Minimap uses (whatever `render_minimap_widget` does — the widget either brings its own panel frame with a title or renders raw into the region; plan inspects and follows suit).

---

## Helpers

### `fixture_type_name`

Currently `static` in `src/game_rendering.cpp:76`. Promote to non-static with a declaration in `include/astra/display_name.h` (natural home alongside the other display helpers), move definition to a matching .cpp if none exists. Any call site that shadows or re-declares it is updated.

### `quest_fixture_display_name(const std::string& id)`

Optional quality-of-life for quest fixtures — returns `QuestFixtureDef::prompt` (or the registered def's id if no prompt). If the prompt is e.g. "Plant receiver drone", the widget shows a meaningful string instead of generic "Quest Fixture".

If promoting this function across files is too invasive, fallback to `fixture_type_name(f.type)` → "Quest Fixture" and defer quest-fixture naming to a follow-up.

---

## File Map

| File | Kind | Responsibility |
|---|---|---|
| `include/astra/game.h` | MODIFY | `Widget::Interactables`, bump `widget_count`, F4 key + label, decl `render_interactables_widget` |
| `src/game_rendering.cpp` | MODIFY | `widget_desired_height` case, `render_side_panel` dispatch, implementation of `render_interactables_widget` |
| `include/astra/display_name.h` | MODIFY | Declare `fixture_type_name(FixtureType)` (promote from static); optional `quest_fixture_display_name(const std::string&)` |
| `src/game_rendering.cpp` (or `src/display_name.cpp`) | MODIFY/NEW | Where `fixture_type_name` body lives after promotion |

---

## Implementation Checklist (for the forthcoming plan)

1. Add `Interactables` enum value, bump `widget_count`, extend `WidgetKeys`.
2. Promote `fixture_type_name` to an externally-visible function. Confirm call sites still compile.
3. Add `widget_desired_height` case = 10.
4. Implement `render_interactables_widget` with NPC / fixture / ground-item passes, sort, and overflow.
5. Dispatch in `render_side_panel`.
6. Smoke test: toggle F4 on the station, see Station Keeper + other NPCs + food terminal + heal pod. Drop an item, see it appear. Walk away, watch it drop off the list when out of FOV.

---

## Out of scope

- Select-and-walk-toward action from the widget.
- Any change to `Game::handle_play_input` beyond the existing widget-toggle plumbing.
- Per-entry quest markers or highlights.
- A pinned / starred entry.
- Performance concerns beyond the 40×40 fixture scan window — fine for the current map sizes.
