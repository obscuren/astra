# Plan: In-Game Map Editor (Dev Mode) — Revised

## Context

Dev-mode map editor for hand-crafting overworld tiles, detail zones (3x3 grid of 120x50), and dungeon layers. Supports tile/fixture painting, NPC placement, play/stop testing, and biome-based generation. Custom maps integrate with the live game world.

---

## Editor Modes

1. **Overworld** — paint terrain tiles on the 120x60 overworld
2. **Detail** — edit 3x3 grid of 120x50 zones per overworld tile
3. **Dungeon** — underground layer below a detail zone (caves, ruins basements)

---

## Key Design Decisions (from review)

| Issue | Resolution |
|-------|-----------|
| Overworld access from anywhere | Teleport player to overworld on editor open (save current location first) |
| Detail pre-population | Blank canvas. Optional "Generate" command runs a biome generator with confirmation if canvas is non-empty |
| Fixture serialization | Use `make_fixture(type)` for reconstruction + store locked/open as extra fields |
| Save format | Version 17 for `custom_flags_` on TileMap |
| Editor directory | `create_directories(save_directory() / "editor/")` before writes |
| NPCs on custom zones | Only manually placed NPCs appear — no random spawns on custom zones |
| Play/Stop | Play spawns player at cursor in current zone. F2 stops and returns to editor |

---

## UI Mockup: Overworld Editor

```
▐▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▌
▐ EDITOR: OVERWORLD  (120x60)  Biome: Grassland          [Esc] Exit  [F1] Help ▌
▐──────────────────────────────────────────────────────────┬────────────────────▌
▐ . . . . T T . . . . . . ^ ^ . . . T . . . . . . . . .  │ TILE PALETTE       ▌
▐ . . . T T T T . . . . . ^ ^ ^ . . T T . . . . . . . .  │                    ▌
▐ . . . . T T . . ~ ~ . . . ^ . . . . . . . . . . . . .  │ > . Plains         ▌
▐ . . . . . . . ~ ~ ~ ~ . . . . . . . . # # . . . . . .  │   ^ Mountains      ▌
▐ . . . . . . . . ~ ~ . . . . . . . . . # # # . . . . .  │   T Forest         ▌
▐ . . . . . . . . . . . . . . . . . .[#]. . . . . . . .  │   ~ River          ▌
▐ . . . * . . . . . . . . . . . . . . . . . . . . . . .  │   ~ Lake           ▌
▐ . . . . . . . . . . . . . . . . . . . . . . . . . . .  │   ...              ▌
▐──────────────────────────────────────────────────────────┼────────────────────▌
▐ Cursor: (34,5) Tile: Ruins [#]  Brush: 1  [CUSTOM]      │ [Space] Paint      ▌
▐ Under cursor: OW_Plains                                  │ [Enter] Detail     ▌
▐▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▌
```

## UI Mockup: Detail Zone Editor

```
▐▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▌
▐ EDITOR: DETAIL (34,5) Ruins  Zone [1,1]  (120x50)     [Esc] Back  [F1] Help ▌
▐──────────────────────────────────────────────────────────┬────────────────────▌
▐ . . . . . . . . . . . . . . . . . . . . . . . . . . .  │ TILE PALETTE       ▌
▐ . . . # # # # # # # # . . . . . . . . . . . . . . . .  │ > . Floor          ▌
▐ . . . # . . . . . . # . . . . . . . . . . . . . . . .  │   # Wall           ▌
▐ . . . # . .[.]. . . # . . . . . . . . . . . . . . . .  │   ...              ▌
▐ . . . # # # # + # # # . . . . . . . . . . . . . . . .  │                    ▌
▐ . . . . . . . . . . . . . . . . . . . . . . . . . . .  │ ZONE MAP  ┌─┬─┬─┐ ▌
▐ . . . . . . . . . . . . . . . . . . . . . . . . . . .  │           │ │ │ │ ▌
▐ . . . . . . . . . . . . . . . . . . . . . . . . . . .  │           ├─┼─┼─┤ ▌
▐ . . . . . . . . . . . . . . . . . . . . . . . . . . .  │           │ │█│ │ ▌
▐ . . . . . . . . . . . . . . . . . . . . . . . . . . .  │           ├─┼─┼─┤ ▌
▐ . . . . . . . . . . . . . . . . . . . . . . . . . . .  │           │ │ │ │ ▌
▐ . . . . . . . . . . . . . . . . . . . . . . . . . . .  │           └─┴─┴─┘ ▌
▐──────────────────────────────────────────────────────────┼────────────────────▌
▐ Cursor: (6,5) Tile: Floor [.]  Brush: 1  Mode: TILE     │ [F2] Play  [G] Gen ▌
▐ Parent: Ruins (34,5)  Zone: 1,1 (center)                │ [Tab] Fixtures/NPC ▌
▐▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▌
```

---

## MapEditor Class

### `include/astra/map_editor.h`

```cpp
class MapEditor {
public:
    bool is_open() const;
    void open(Game& game);          // teleports to overworld, enters editor
    void close(Game& game);         // returns to previous location
    bool handle_input(int key, Game& game);
    void draw(int screen_w, int screen_h);

    bool playing() const;           // true during play-test mode

    enum class Mode { Overworld, Detail, Dungeon };
    enum class PaintMode { Tile, Fixture, Npc };

private:
    Renderer* renderer_ = nullptr;
    WorldManager* world_ = nullptr;
    bool open_ = false;
    Mode mode_ = Mode::Overworld;
    PaintMode paint_mode_ = PaintMode::Tile;

    // Return state — where to teleport back on close
    LocationKey return_key_;
    int return_x_ = 0, return_y_ = 0;
    SurfaceMode return_surface_;

    // Cursor & camera
    int cursor_x_ = 0, cursor_y_ = 0;
    int camera_x_ = 0, camera_y_ = 0;

    // Tile palette
    std::vector<Tile> palette_;
    int palette_cursor_ = 0;
    Tile active_tile_;
    int brush_size_ = 1;

    // Fixture palette
    std::vector<FixtureType> fixture_palette_;
    int fixture_cursor_ = 0;
    FixtureType active_fixture_;

    // NPC palette
    struct NpcTemplate { std::string name; char glyph; Color color; std::string role; };
    std::vector<NpcTemplate> npc_palette_;
    int npc_cursor_ = 0;

    // Detail context
    int detail_ow_x_ = -1, detail_ow_y_ = -1;
    int detail_zone_x_ = 1, detail_zone_y_ = 1;
    TileMap detail_map_;
    bool detail_dirty_ = false;

    // Dungeon context
    TileMap dungeon_map_;
    bool dungeon_dirty_ = false;

    // Play/Stop
    bool playing_ = false;

    // Undo
    struct UndoState { std::vector<Tile> tiles; };
    std::vector<UndoState> undo_stack_;
    static constexpr int max_undo_ = 20;

    // Generate confirmation
    PopupMenu confirm_popup_;

    // Methods
    void init_overworld_palette();
    void init_detail_palette();
    void init_fixture_palette();
    void init_npc_palette();
    void paint(int x, int y);
    void place_fixture(int x, int y);
    void place_npc(int x, int y, Game& game);
    void remove_at(int x, int y, Game& game);  // eraser: remove fixture/NPC
    void flood_fill(int x, int y, Tile target);
    void enter_detail(int ow_x, int ow_y);
    void enter_dungeon();
    void switch_zone(int zx, int zy);
    void exit_detail();
    void exit_dungeon();
    void generate_zone(Game& game);  // run generator on current zone (with confirm)
    void start_play(Game& game);     // enter play-test mode
    void stop_play(Game& game);      // return to editor
    void save_zone(int ow_x, int ow_y, int zx, int zy);
    void load_zone(int ow_x, int ow_y, int zx, int zy);
    void save_dungeon(int ow_x, int ow_y, int zx, int zy);
    void load_dungeon(int ow_x, int ow_y, int zx, int zy);
    void push_undo();
    void pop_undo();

    // Drawing
    void draw_viewport(DrawContext& ctx);
    void draw_palette(DrawContext& ctx);
    void draw_zone_minimap(DrawContext& ctx);
    void draw_status(DrawContext& ctx);
};
```

---

## Input Bindings

### All modes
- Arrow keys: move cursor (camera follows)
- `[`/`]`: cycle palette (tile, fixture, or NPC depending on paint mode)
- `Space`: paint/place at cursor
- `Delete`/`x`: erase fixture or NPC at cursor
- `Tab`: cycle paint mode (Tile → Fixture → NPC → Tile)
- `b`: cycle brush size (1→3→5→1) — tiles only
- `f`: flood fill with active tile
- `u`: undo
- `F1`: help overlay
- `Esc`: back (detail→overworld, overworld→close editor)

### Overworld mode
- `Enter`: enter detail editor for tile under cursor (marks custom)
- `c`: toggle custom flag

### Detail mode
- `PgUp`/`PgDn`: cycle zones
- `1`-`9`: jump to zone
- `d`: enter dungeon layer for this zone
- `G`: generate terrain (confirm popup if non-empty)
- `F2`: play-test (spawn at cursor)

### Dungeon mode
- Same painting/fixture/NPC controls
- `G`: generate cave/dungeon (confirm popup if non-empty)
- `F2`: play-test
- `Esc`: back to detail mode

### Play-test mode
- Normal game controls (move, fight, interact)
- `F2`: stop play-test, return to editor

---

## Data Model Changes

### `include/astra/tilemap.h`
- Add `std::vector<uint8_t> custom_flags_` to TileMap
- `bool custom_detail(int x, int y) const` / `void set_custom_detail(int x, int y, bool v)`
- Only allocated for Overworld map type

### `include/astra/map_properties.h`
- Add `bool detail_is_custom = false;`

### `src/game_world.cpp` — `enter_detail_map()` / `build_detail_props()`
- Check `custom_detail(ow_x, ow_y)` → set `detail_is_custom`
- If custom and not cached: load from `.amap` file or create blank canvas
- Custom zones: skip random NPC spawning

### `src/save_file.cpp`
- Version 17: serialize `custom_flags_` in map section (overworld only)

---

## Editor Map Storage

**Directory:** `save_directory() / "editor/"` (create on first write)

**Detail zone:** `detail_{sys}_{body}_{moon}_{ow_x}_{ow_y}_z{zx}{zy}.amap`
**Dungeon:** `dungeon_{sys}_{body}_{moon}_{ow_x}_{ow_y}_z{zx}{zy}.amap`

**Binary format:**
- Header: `AMAP` magic, version u8, MapType u8, Biome u8, width u16, height u16
- Tile data: w*h bytes
- Fixture count u16, then per fixture: x u16, y u16, type u8, locked u8, open u8
- NPC count u16, then per NPC: x u16, y u16, glyph u8, color u8, role string, name string, disposition u8

---

## Play/Stop Mode

**Play (F2):**
1. Save editor state (cursor, mode, zone)
2. Set player position to cursor location
3. Set `playing_ = true`
4. Run `recompute_fov()`, `compute_camera()`
5. Game loop runs normally — world ticks, NPCs act, player moves
6. Only manually placed NPCs exist on custom zones

**Stop (F2 again):**
1. Set `playing_ = false`
2. Restore editor state
3. Reload zone from last save (undo any play-test changes)
4. Return to editor view

---

## Generate Command (G key)

1. If current zone has any non-empty tiles: show confirmation popup
   - "This will overwrite the current zone. Continue?"
   - Yes/No
2. If confirmed (or zone is empty): show biome selection popup
   - List available biomes (Rocky, Forest, Grassland, Sandy, Ice, Volcanic, Aquatic, Fungal, Station)
3. Run `create_generator(MapType::DetailMap)` with selected biome props
4. Replace zone tiles/fixtures with generated content
5. Push to undo stack before generating

---

## Game Integration

### `include/astra/game.h`
- Add `MapEditor map_editor_;` + accessor
- Add `open_map_editor()` / `close_map_editor()` public methods

### `src/game_input.cpp`
- Intercept when editor is open and not playing
- When playing: normal input, but F2 stops play

### `src/game_rendering.cpp`
- When editor open and not playing: `map_editor_.draw()`
- When playing: normal `render_play()`

### `src/dev_console.cpp`
- `editor` command: `game.open_map_editor()`

### `CMakeLists.txt`
- Add `src/map_editor.cpp`

---

## Files Modified/Created

| File | Action |
|------|--------|
| `include/astra/map_editor.h` | **New** |
| `src/map_editor.cpp` | **New** (~800-1000 lines) |
| `include/astra/tilemap.h` | Add custom_flags_ |
| `include/astra/map_properties.h` | Add detail_is_custom |
| `include/astra/game.h` | Add MapEditor member |
| `src/game_world.cpp` | Custom zone loading, editor teleport |
| `src/game_input.cpp` | Editor intercept + play/stop |
| `src/game_rendering.cpp` | Editor draw |
| `src/save_file.cpp` | Version 17: custom flags |
| `src/dev_console.cpp` | `editor` command |
| `CMakeLists.txt` | Add source |

---

## Verification

1. `editor` command → teleports to overworld, editor opens
2. Paint overworld tiles, cycle palette
3. Mark tile custom, Enter → blank detail zone at (1,1)
4. Paint tiles, place fixtures, place NPCs
5. `G` → biome picker → generates terrain (confirm if non-empty)
6. Switch zones with PgUp/PgDn, zone minimap updates
7. `d` → dungeon layer, paint cave layout
8. `F2` → play-test: walk around, NPCs present, world ticks
9. `F2` again → back to editor, zone restored to pre-play state
10. `Esc` chain → dungeon→detail→overworld→close editor, return to original location
11. Walk to custom tile in game, enter → hand-crafted zone loads
12. Custom zones have only manually placed NPCs, no random spawns
13. Save/reload → custom flags + .amap files persist
14. Undo works across paint, fixture, NPC operations
