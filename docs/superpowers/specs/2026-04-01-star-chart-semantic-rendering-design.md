# Star Chart Viewer — Semantic Rendering Migration

## Goal

Migrate the StarChartViewer from terminal-specific rendering to the semantic UI architecture using a two-layer approach: a reusable galaxy map primitive and a composed star chart UI.

## Architecture — Two Layers

### Layer 1: Galaxy Map Primitive (reusable)

A standalone rendering primitive that draws the star field / orbital diagram into any arbitrary rect. No panel chrome, no info sidebar.

```
UIContext::galaxy_map(GalaxyMapDesc)
  └─ Renderer::draw_galaxy_map(bounds, desc)  [virtual]
       └─ TerminalRenderer: projection, star glyphs, orbital diagrams
       └─ SdlRenderer: (future: 2D star map with sprites)
```

This is analogous to how `render_map()` + `MapRenderContext` works for the game world — it can render a world map preview into any rect. The galaxy map primitive enables the same: quest previews, dialog illustrations, minimaps.

### Layer 2: Star Chart UI (composition)

The full star chart experience composes Layer 1 with existing semantic UI components:

```
StarChartViewer::draw()
  └─ ctx.panel({title, footer})           — existing semantic panel
  └─ content.columns({fill, fixed(1), fill}) — existing layout
  └─ map_area.galaxy_map(map_desc)        — Layer 1 primitive
  └─ divider.separator({vertical})        — existing separator
  └─ info.styled_text() / label_value()   — existing UI components
  └─ scan message overlay                 — existing text()
```

## GalaxyMapDesc

```cpp
struct GalaxyMapDesc {
    ChartZoom zoom;
    std::span<const StarSystem> systems;
    std::span<const GVArmLabel> arm_labels;

    // Player location
    int player_system_index = -1;
    int player_body_index = -1;
    int player_moon_index = -1;
    bool at_station = false;
    bool on_ship = false;

    // Viewport center (galactic coordinates)
    float view_cx = 0.0f;
    float view_cy = 0.0f;

    // Cursor (interactive mode, -1 = none)
    int cursor_system_index = -1;
    int body_cursor = -1;
    int sub_cursor = -1;

    // Highlight (non-interactive, e.g. quest preview)
    int highlight_system_index = -1;

    // Quest markers
    std::vector<uint32_t> quest_system_ids;
    std::vector<GVBodyQuest> quest_body_targets;

    // Navigation range
    float navi_range = 1.0f;

    // System zoom: precomputed station host body
    int station_host_body_index = -1;
};
```

The desc contains only what the renderer needs to draw the map. Panel chrome (`title`, `footer`), overlay text (`scan_message`), and input state (`view_only`) stay in the StarChartViewer.

## Info Sidebar

The info sidebar (system details, body stats, danger bars) is rendered by `StarChartViewer::draw()` using existing semantic UI components (`label_value()`, `styled_text()`, `progress_bar()`). This code stays in `star_chart_viewer.cpp` — it doesn't go into the renderer.

## Design Decisions

1. **Two-layer split** — renderer owns only map visualization; UI composition uses existing semantic toolkit
2. **Single virtual method** — `draw_galaxy_map()`, not 4 per-zoom methods; renderer dispatches internally
3. **`std::span<const StarSystem>`** — zero-copy reference to live NavigationData; valid for frame duration
4. **Separate header** — `galaxy_map_desc.h` keeps `ui_types.h` clean of game-specific types
5. **highlight_system_index** — new field enabling non-interactive use (quest previews show a target without cursor navigation)

## Reuse Example

```cpp
// Quest log: small galaxy preview showing target location
auto preview = ctx.sub({x, y, 20, 10});
preview.galaxy_map({
    .zoom = ChartZoom::Region,
    .systems = nav.systems,
    .view_cx = quest_system.gx,
    .view_cy = quest_system.gy,
    .highlight_system_index = target_idx,
    .player_system_index = current_idx,
});
```
