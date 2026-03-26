# Game Class Refactoring — Full Roadmap

## Goal

Break the monolithic `Game` class (~5000 lines, 80+ methods, 60+ members) into focused subsystem classes. Game becomes a thin coordinator.

---

## Current State (after Phase 0)

Already extracted:
- ✅ **InputManager** — look mode cursor state
- ✅ **DevConsole** — dev commands, console UI (self-contained)
- ✅ **HelpScreen** — help content, help UI (self-contained)
- ✅ **CharacterScreen** — character sheet UI (pre-existing)
- ✅ **CharacterCreation** — character wizard (pre-existing)
- ✅ **TradeWindow** — trading UI (pre-existing)
- ✅ **StarChartViewer** — star chart UI (pre-existing, but doesn't own NavigationData)

---

## Phase 1: WorldManager

**The foundation.** Owns all world state. Everything else reads from this.

### Owns
- `TileMap map_`
- `VisibilityMap visibility_`
- `std::vector<Npc> npcs_`
- `std::vector<GroundItem> ground_items_`
- `std::vector<Item> stash_` + `max_stash_size_`
- `std::map<LocationKey, LocationState> location_cache_`
- `SurfaceMode surface_mode_`, `overworld_x_`, `overworld_y_`
- `int world_tick_`, `DayClock day_clock_`
- `int current_region_`
- `unsigned seed_`, `std::mt19937 rng_`

### Interface
```
map(), visibility(), npcs(), ground_items(), stash()
surface_mode(), world_tick(), day_clock(), rng()
on_overworld(), on_detail_map()
tile_occupied(x, y), npc_at(x, y)
advance_tick() — increments world_tick_ + day_clock_
```

### Transition methods (take Player& as parameter)
```
save_current_location(player, navigation)
restore_location(key, player)
enter_ship(player), enter_detail_map(player)
exit_detail_to_overworld(player), exit_dungeon_to_detail(player)
enter_dungeon_from_detail(player), transition_detail_edge(dx, dy, player)
recompute_fov(player)
check_region_change() — logs region entry messages
```

### Callbacks needed (Game provides)
```
std::function<void(const std::string&)> on_log
std::function<void()> on_compute_camera
```

### Dependencies moved FROM Game
- `NavigationData navigation_` stays in Game (shared with StarChartViewer)
- `Player player_` stays in Game (shared by all subsystems)

### Files
- `include/astra/world_manager.h`
- `src/world_manager.cpp`

### Difficulty: Very High
- 10+ transition functions, ~760 lines
- Touches player position, camera, FOV, generators
- Most interconnected piece

---

## Phase 2: CombatSystem

**Combat logic and NPC AI.** Depends on WorldManager for map/npcs.

### Owns
- `targeting_`, `target_x_`, `target_y_`, `blink_phase_`, `target_npc_`

### Interface
```
attack_npc(npc, player, world)
process_npc_turn(npc, player, world)
begin_targeting(player, world)
handle_targeting_input(key, player, world)
shoot_target(player, world)
remove_dead_npcs(world)
check_player_death(player) → bool
targeting(), target_x(), target_y(), blink_phase(), target_npc()
tick_blink()
```

### Files
- `include/astra/combat_system.h`
- `src/combat_system.cpp`

### Difficulty: Medium
- ~300 lines, clear boundaries
- Needs WorldManager for npc list, map passability
- Needs Player for stats, equipment, effects
- Loot drops need rng + item generation

---

## Phase 3: DialogManager

**NPC dialog state machine and fixture interactions.**

### Owns
- `PopupMenu npc_dialog_`, `std::string npc_dialog_body_`
- `Npc* interacting_npc_`
- `const std::vector<DialogNode>* dialog_tree_`
- `int dialog_node_` (sentinel system: -1 to -9)
- `std::vector<InteractOption> interact_options_`

### Interface
```
is_open()
open_npc_dialog(npc)
advance_dialog(selected, player, world)
handle_input(key, player, world) — Tab trade, l look, etc.
draw(renderer, screen_w, screen_h)
close()
interact_fixture(fixture_id, world, player)
interacting_npc()
```

### Files
- `include/astra/dialog_manager.h`
- `src/dialog_manager.cpp`

### Difficulty: High
- advance_dialog() is 289 lines with sentinel node state machine
- Fixture dialogs (food terminal, stash, ship terminal) are complex
- Needs WorldManager for stash, map fixtures
- Needs Player for inventory, money
- Needs TradeWindow reference for shop opening

---

## Phase 4: StarChart (expand existing)

**Expand StarChartViewer to own NavigationData.**

### Move to StarChartViewer (or new StarChart class)
- `NavigationData navigation_`
- `travel_to_destination()` — currently in Game, uses WorldManager

### Interface additions
```
navigation() — accessor
travel_to_destination(action, player, world)
```

### Files
- Modify `include/astra/star_chart_viewer.h`
- Modify `src/star_chart_viewer.cpp`

### Difficulty: Medium
- travel_to_destination is 263 lines and very interconnected
- Needs WorldManager for location cache, map generation
- Needs Player for position

---

## Phase 5: MapRenderer

**Extract map drawing from render_map().**

### Owns
- Nothing (stateless renderer)

### Interface
```
draw(ctx, map, visibility, npcs, ground_items, player,
     camera_x, camera_y, combat, input)
```

### What moves
- The ~250 line `render_map()` body: tile resolution, glyph selection, FOV shading, NPC/item drawing, targeting overlay, look cursor overlay

### Files
- `include/astra/map_renderer.h`
- `src/map_renderer.cpp`

### Difficulty: Low-Medium
- Pure rendering, no state mutation
- Just needs read access to world/combat/input state
- Can be done as a simple free function or small class

---

## Phase 6: UIManager

**Layout and all remaining render functions.**

### Owns
- All `*_rect_` layout members (screen, map, panel, stats, bars, tabs, effects, abilities)
- `screen_w_`, `screen_h_`
- `panel_visible_`, `active_tab_`
- `compute_layout()`

### Rendering methods that move here
```
render_play() — orchestrator
render_stats_bar(), render_bars(), render_tabs()
render_side_panel(), render_effects_bar(), render_abilities_bar()
render_item_inspect(), render_look_popup()
render_gameover(), render_load_menu(), render_hall_of_fame()
render_menu() — main menu with ASTRA logo
Welcome screen rendering
```

### Files
- `include/astra/ui_manager.h`
- `src/ui_manager.cpp`

### Difficulty: Medium
- Large amount of code (~1200 lines) but mostly straightforward
- Pure rendering — reads from all subsystems
- No state mutation (except layout computation)

---

## Post-Refactor Game Class

```cpp
class Game {
    // Core
    std::unique_ptr<Renderer> renderer_;
    GameState state_;
    bool running_;
    Player player_;

    // Subsystems
    WorldManager world_;
    CombatSystem combat_;
    DialogManager dialog_;
    InputManager input_;

    // Overlays (self-contained)
    DevConsole console_;
    HelpScreen help_screen_;
    CharacterScreen character_screen_;
    CharacterCreation character_creation_;
    TradeWindow trade_window_;
    StarChartViewer star_chart_viewer_;

    // UI
    UIManager ui_;
    MapRenderer map_renderer_;

    // Menus
    PopupMenu pause_menu_;
    PopupMenu quit_confirm_;

    // Save/load
    std::vector<SaveSlot> save_slots_;
    int load_selection_;
    bool confirm_delete_;
    std::string death_message_;

    // Message log
    std::deque<std::string> messages_;
};
```

Game.cpp shrinks to: `run()`, `handle_input()`, `update()`, `new_game()`, save/load, and thin delegation to subsystems. Target: ~800-1000 lines.

---

## Execution Strategy

- One phase per session/commit
- Build + test after each phase
- Commit before starting next phase
- If a phase is too large, split into sub-steps (like we did with InputManager)
