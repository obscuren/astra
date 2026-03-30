#include "astra/map_editor.h"
#include "astra/game.h"
#include "astra/map_generator.h"
#include "astra/map_properties.h"

#include <algorithm>
#include <filesystem>

namespace astra {

// ─────────────────────────────────────────────────────────────────
// Palette data
// ─────────────────────────────────────────────────────────────────

void MapEditor::init_overworld_palette() {
    palette_ = {
        Tile::OW_Plains, Tile::OW_Mountains, Tile::OW_Forest, Tile::OW_Desert,
        Tile::OW_Crater, Tile::OW_IceField, Tile::OW_LavaFlow, Tile::OW_Fungal,
        Tile::OW_River, Tile::OW_Lake, Tile::OW_Swamp,
        Tile::OW_CaveEntrance, Tile::OW_Ruins, Tile::OW_Settlement,
        Tile::OW_CrashedShip, Tile::OW_Outpost, Tile::OW_Landing,
    };
    palette_cursor_ = 0;
}

void MapEditor::init_detail_palette() {
    palette_ = {
        Tile::Floor, Tile::Wall, Tile::StructuralWall, Tile::IndoorFloor,
        Tile::Water, Tile::Ice, Tile::Portal, Tile::Empty,
    };
    palette_cursor_ = 0;
}

void MapEditor::init_dungeon_palette() {
    init_detail_palette(); // same tiles for now
}

void MapEditor::init_fixture_palette() {
    fixture_palette_ = {
        FixtureType::Door, FixtureType::Table, FixtureType::Console,
        FixtureType::Crate, FixtureType::Bunk, FixtureType::Rack,
        FixtureType::Conduit, FixtureType::ShuttleClamp, FixtureType::Shelf,
        FixtureType::Viewport, FixtureType::Window, FixtureType::Torch,
        FixtureType::Stool, FixtureType::Debris,
        FixtureType::HealPod, FixtureType::FoodTerminal, FixtureType::WeaponDisplay,
        FixtureType::RepairBench, FixtureType::SupplyLocker, FixtureType::StarChart,
        FixtureType::RestPod, FixtureType::ShipTerminal, FixtureType::CommandTerminal,
        FixtureType::DungeonHatch, FixtureType::StairsUp,
    };
    fixture_cursor_ = 0;
}

void MapEditor::init_npc_palette() {
    npc_palette_ = {
        {"Guard",     'G', Color::White,  "guard",     Disposition::Neutral},
        {"Merchant",  'M', Color::Yellow, "merchant",  Disposition::Neutral},
        {"Engineer",  'E', Color::Cyan,   "engineer",  Disposition::Neutral},
        {"Medic",     '+', Color::Green,  "medic",     Disposition::Neutral},
        {"Commander", 'C', Color::White,  "commander", Disposition::Neutral},
        {"Civilian",  'H', Color::White,  "civilian",  Disposition::Neutral},
        {"Hostile",   'x', Color::Red,    "hostile",   Disposition::Hostile},
    };
    npc_cursor_ = 0;
}

// ─────────────────────────────────────────────────────────────────
// Open / Close
// ─────────────────────────────────────────────────────────────────

void MapEditor::open(Game& game) {
    renderer_ = game.renderer();
    world_ = &game.world();
    open_ = true;
    playing_ = false;
    mode_ = Mode::Overworld;
    paint_mode_ = PaintMode::Tile;
    undo_stack_.clear();

    // Save return state
    return_x_ = game.player().x;
    return_y_ = game.player().y;
    return_surface_ = world_->surface_mode();

    // Teleport to overworld if not already there
    if (!world_->on_overworld()) {
        game.save_current_location();

        LocationKey ow_key = {world_->navigation().current_system_id,
                              world_->navigation().current_body_index,
                              world_->navigation().current_moon_index,
                              false, -1, -1, 0, -1, -1};

        if (world_->location_cache().count(ow_key)) {
            game.restore_location(ow_key);
        } else {
            game.log("No overworld available for editing.");
            open_ = false;
            return;
        }
        world_->set_surface_mode(SurfaceMode::Overworld);
        world_->visibility().reveal_all();
    }

    cursor_x_ = game.player().x;
    cursor_y_ = game.player().y;

    init_overworld_palette();
    init_fixture_palette();
    init_npc_palette();
}

void MapEditor::close(Game& game) {
    open_ = false;
    playing_ = false;
    // Stay on overworld — don't teleport back (edits are live)
}

// ─────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────

TileMap& MapEditor::active_map() {
    return world_->map();
}

const char* MapEditor::mode_name() const {
    switch (mode_) {
        case Mode::Overworld: return "OVERWORLD";
        case Mode::Detail:    return "DETAIL";
        case Mode::Dungeon:   return "DUNGEON";
    }
    return "?";
}

Tile MapEditor::active_tile() const {
    if (palette_.empty()) return Tile::Empty;
    return palette_[palette_cursor_];
}

FixtureType MapEditor::active_fixture() const {
    if (fixture_palette_.empty()) return FixtureType::Table;
    return fixture_palette_[fixture_cursor_];
}

static const char* tile_name(Tile t) {
    switch (t) {
        case Tile::Empty:          return "Empty";
        case Tile::Floor:          return "Floor";
        case Tile::Wall:           return "Wall";
        case Tile::StructuralWall: return "Structural Wall";
        case Tile::IndoorFloor:    return "Indoor Floor";
        case Tile::Portal:         return "Portal";
        case Tile::Water:          return "Water";
        case Tile::Ice:            return "Ice";
        case Tile::Fixture:        return "Fixture";
        case Tile::OW_Plains:      return "Plains";
        case Tile::OW_Mountains:   return "Mountains";
        case Tile::OW_Forest:      return "Forest";
        case Tile::OW_Desert:      return "Desert";
        case Tile::OW_Crater:      return "Crater";
        case Tile::OW_IceField:    return "Ice Field";
        case Tile::OW_LavaFlow:    return "Lava Flow";
        case Tile::OW_Fungal:      return "Fungal";
        case Tile::OW_River:       return "River";
        case Tile::OW_Lake:        return "Lake";
        case Tile::OW_Swamp:       return "Swamp";
        case Tile::OW_CaveEntrance:return "Cave Entrance";
        case Tile::OW_Ruins:       return "Ruins";
        case Tile::OW_Settlement:  return "Settlement";
        case Tile::OW_CrashedShip: return "Crashed Ship";
        case Tile::OW_Outpost:     return "Outpost";
        case Tile::OW_Landing:     return "Landing";
    }
    return "?";
}

static const char* fixture_name(FixtureType t) {
    auto fd = make_fixture(t);
    // Return a static name based on type
    switch (t) {
        case FixtureType::Door:           return "Door";
        case FixtureType::Table:          return "Table";
        case FixtureType::Console:        return "Console";
        case FixtureType::Crate:          return "Crate";
        case FixtureType::Bunk:           return "Bunk";
        case FixtureType::Rack:           return "Rack";
        case FixtureType::Conduit:        return "Conduit";
        case FixtureType::ShuttleClamp:   return "Shuttle Clamp";
        case FixtureType::Shelf:          return "Shelf";
        case FixtureType::Viewport:       return "Viewport";
        case FixtureType::Window:         return "Window";
        case FixtureType::Torch:          return "Torch";
        case FixtureType::Stool:          return "Stool";
        case FixtureType::Debris:         return "Debris";
        case FixtureType::HealPod:        return "Heal Pod";
        case FixtureType::FoodTerminal:   return "Food Terminal";
        case FixtureType::WeaponDisplay:  return "Weapon Display";
        case FixtureType::RepairBench:    return "Repair Bench";
        case FixtureType::SupplyLocker:   return "Supply Locker";
        case FixtureType::StarChart:      return "Star Chart";
        case FixtureType::RestPod:        return "Rest Pod";
        case FixtureType::ShipTerminal:   return "Ship Terminal";
        case FixtureType::CommandTerminal:return "Cmd Terminal";
        case FixtureType::DungeonHatch:   return "Dungeon Hatch";
        case FixtureType::StairsUp:       return "Stairs Up";
    }
    return "?";
}

// ─────────────────────────────────────────────────────────────────
// Painting
// ─────────────────────────────────────────────────────────────────

void MapEditor::paint(int x, int y) {
    auto& map = active_map();
    if (x < 0 || x >= map.width() || y < 0 || y >= map.height()) return;
    map.set(x, y, active_tile());
}

void MapEditor::paint_brush(int cx, int cy) {
    push_undo();
    int r = brush_size_ / 2;
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx)
            paint(cx + dx, cy + dy);
}

void MapEditor::place_fixture(int x, int y) {
    auto& map = active_map();
    if (x < 0 || x >= map.width() || y < 0 || y >= map.height()) return;
    push_undo();
    map.set(x, y, Tile::Fixture);
    map.add_fixture(x, y, make_fixture(active_fixture()));
}

void MapEditor::place_npc(int x, int y, Game& game) {
    if (npc_palette_.empty()) return;
    const auto& tmpl = npc_palette_[npc_cursor_];
    Npc npc;
    npc.x = x;
    npc.y = y;
    npc.glyph = tmpl.glyph;
    npc.color = tmpl.color;
    npc.name = tmpl.name;
    npc.role = tmpl.role;
    npc.disposition = tmpl.disposition;
    npc.hp = 10;
    npc.max_hp = 10;
    npc.level = 1;
    world_->npcs().push_back(std::move(npc));
}

void MapEditor::remove_at(int x, int y, Game& game) {
    auto& map = active_map();
    if (x < 0 || x >= map.width() || y < 0 || y >= map.height()) return;

    // Remove NPC at position
    auto& npcs = world_->npcs();
    npcs.erase(std::remove_if(npcs.begin(), npcs.end(),
        [x, y](const Npc& n) { return n.x == x && n.y == y; }), npcs.end());

    // Remove fixture at position
    if (map.get(x, y) == Tile::Fixture) {
        push_undo();
        map.set(x, y, Tile::Floor);
    }
}

void MapEditor::flood_fill(int x, int y, Tile old_tile, Tile new_tile) {
    auto& map = active_map();
    if (old_tile == new_tile) return;
    if (x < 0 || x >= map.width() || y < 0 || y >= map.height()) return;
    if (map.get(x, y) != old_tile) return;

    push_undo();

    // BFS flood fill
    std::vector<std::pair<int,int>> queue;
    queue.push_back({x, y});
    map.set(x, y, new_tile);

    while (!queue.empty()) {
        auto [cx, cy] = queue.back();
        queue.pop_back();
        static const int dx[] = {0, 0, -1, 1};
        static const int dy[] = {-1, 1, 0, 0};
        for (int d = 0; d < 4; ++d) {
            int nx = cx + dx[d], ny = cy + dy[d];
            if (nx >= 0 && nx < map.width() && ny >= 0 && ny < map.height() &&
                map.get(nx, ny) == old_tile) {
                map.set(nx, ny, new_tile);
                queue.push_back({nx, ny});
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────
// Undo
// ─────────────────────────────────────────────────────────────────

void MapEditor::push_undo() {
    auto& map = active_map();
    UndoState state;
    state.tiles = map.tiles();
    state.fixture_ids = map.fixture_ids();
    undo_stack_.push_back(std::move(state));
    if (static_cast<int>(undo_stack_.size()) > max_undo_)
        undo_stack_.erase(undo_stack_.begin());
}

void MapEditor::pop_undo() {
    if (undo_stack_.empty()) return;
    // Restoring tiles only — fixtures are complex, this is a simplified undo
    auto& map = active_map();
    auto& state = undo_stack_.back();
    // Direct tile restore via load_from would be heavy; just copy tiles back
    for (int i = 0; i < static_cast<int>(state.tiles.size()); ++i) {
        int x = i % map.width(), y = i / map.width();
        map.set(x, y, state.tiles[i]);
    }
    undo_stack_.pop_back();
}

// ─────────────────────────────────────────────────────────────────
// Mode transitions
// ─────────────────────────────────────────────────────────────────

void MapEditor::enter_detail(int ow_x, int ow_y, Game& game) {
    detail_ow_x_ = ow_x;
    detail_ow_y_ = ow_y;
    detail_zone_x_ = 1;
    detail_zone_y_ = 1;

    // Mark as custom
    active_map().set_custom_detail(ow_x, ow_y, true);

    // Save overworld and transition to detail
    game.save_current_location();
    world_->overworld_x() = ow_x;
    world_->overworld_y() = ow_y;
    world_->zone_x() = detail_zone_x_;
    world_->zone_y() = detail_zone_y_;

    auto props = default_properties(MapType::DetailMap);

    LocationKey detail_key = {world_->navigation().current_system_id,
                              world_->navigation().current_body_index,
                              world_->navigation().current_moon_index,
                              false, ow_x, ow_y, 0,
                              detail_zone_x_, detail_zone_y_};

    if (world_->location_cache().count(detail_key)) {
        game.restore_location(detail_key);
    } else {
        // Blank canvas
        world_->map() = TileMap(props.width, props.height, MapType::DetailMap);
        world_->npcs().clear();
        world_->ground_items().clear();
        world_->visibility() = VisibilityMap(props.width, props.height);
    }

    world_->set_surface_mode(SurfaceMode::DetailMap);
    world_->visibility().reveal_all();

    mode_ = Mode::Detail;
    cursor_x_ = props.width / 2;
    cursor_y_ = props.height / 2;
    undo_stack_.clear();
    init_detail_palette();
}

void MapEditor::switch_zone(int zx, int zy, Game& game) {
    if (zx < 0 || zx >= zones_per_tile || zy < 0 || zy >= zones_per_tile) return;
    if (zx == detail_zone_x_ && zy == detail_zone_y_) return;

    // Save current zone
    game.save_current_location();

    detail_zone_x_ = zx;
    detail_zone_y_ = zy;
    world_->zone_x() = zx;
    world_->zone_y() = zy;

    auto props = default_properties(MapType::DetailMap);

    LocationKey key = {world_->navigation().current_system_id,
                       world_->navigation().current_body_index,
                       world_->navigation().current_moon_index,
                       false, detail_ow_x_, detail_ow_y_, 0, zx, zy};

    if (world_->location_cache().count(key)) {
        game.restore_location(key);
    } else {
        world_->map() = TileMap(props.width, props.height, MapType::DetailMap);
        world_->npcs().clear();
        world_->ground_items().clear();
        world_->visibility() = VisibilityMap(props.width, props.height);
    }

    world_->set_surface_mode(SurfaceMode::DetailMap);
    world_->visibility().reveal_all();
    cursor_x_ = std::min(cursor_x_, props.width - 1);
    cursor_y_ = std::min(cursor_y_, props.height - 1);
    undo_stack_.clear();
}

void MapEditor::exit_detail(Game& game) {
    game.save_current_location();

    LocationKey ow_key = {world_->navigation().current_system_id,
                          world_->navigation().current_body_index,
                          world_->navigation().current_moon_index,
                          false, -1, -1, 0, -1, -1};

    if (world_->location_cache().count(ow_key)) {
        game.restore_location(ow_key);
    }

    world_->set_surface_mode(SurfaceMode::Overworld);
    world_->visibility().reveal_all();

    mode_ = Mode::Overworld;
    cursor_x_ = detail_ow_x_;
    cursor_y_ = detail_ow_y_;
    undo_stack_.clear();
    init_overworld_palette();
}

void MapEditor::enter_dungeon(Game& game) {
    game.save_current_location();

    auto props = default_properties(MapType::Rocky);

    LocationKey key = {world_->navigation().current_system_id,
                       world_->navigation().current_body_index,
                       world_->navigation().current_moon_index,
                       false, detail_ow_x_, detail_ow_y_, 1,
                       detail_zone_x_, detail_zone_y_};

    if (world_->location_cache().count(key)) {
        game.restore_location(key);
    } else {
        world_->map() = TileMap(props.width, props.height, MapType::Rocky);
        world_->npcs().clear();
        world_->ground_items().clear();
        world_->visibility() = VisibilityMap(props.width, props.height);
    }

    world_->set_surface_mode(SurfaceMode::Dungeon);
    world_->visibility().reveal_all();

    mode_ = Mode::Dungeon;
    cursor_x_ = props.width / 2;
    cursor_y_ = props.height / 2;
    undo_stack_.clear();
    init_dungeon_palette();
}

void MapEditor::exit_dungeon(Game& game) {
    game.save_current_location();

    LocationKey key = {world_->navigation().current_system_id,
                       world_->navigation().current_body_index,
                       world_->navigation().current_moon_index,
                       false, detail_ow_x_, detail_ow_y_, 0,
                       detail_zone_x_, detail_zone_y_};

    if (world_->location_cache().count(key)) {
        game.restore_location(key);
    }

    world_->set_surface_mode(SurfaceMode::DetailMap);
    world_->visibility().reveal_all();

    mode_ = Mode::Detail;
    undo_stack_.clear();
    init_detail_palette();
}

// ─────────────────────────────────────────────────────────────────
// Play / Stop
// ─────────────────────────────────────────────────────────────────

void MapEditor::start_play(Game& game) {
    playing_ = true;
    game.player().x = cursor_x_;
    game.player().y = cursor_y_;
    game.recompute_fov();
    game.compute_camera();
}

void MapEditor::stop_play(Game& game) {
    playing_ = false;
    world_->visibility().reveal_all();
}

// ─────────────────────────────────────────────────────────────────
// Camera
// ─────────────────────────────────────────────────────────────────

void MapEditor::compute_editor_camera(int map_w, int map_h, int vw, int vh) {
    camera_x_ = cursor_x_ - vw / 2;
    camera_y_ = cursor_y_ - vh / 2;
    if (camera_x_ < 0) camera_x_ = 0;
    if (camera_y_ < 0) camera_y_ = 0;
    if (camera_x_ + vw > map_w) camera_x_ = map_w - vw;
    if (camera_y_ + vh > map_h) camera_y_ = map_h - vh;
    if (camera_x_ < 0) camera_x_ = 0;
    if (camera_y_ < 0) camera_y_ = 0;
}

// ─────────────────────────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────────────────────────

bool MapEditor::handle_input(int key, Game& game) {
    if (!open_) return false;

    // Popup intercept
    if (popup_.is_open()) {
        MenuResult r = popup_.handle_input(key);
        if (r == MenuResult::Selected || r == MenuResult::Closed) {
            popup_.close();
        }
        return true;
    }

    // Play mode: F2 stops
    if (playing_) {
        if (key == KEY_F2) {
            stop_play(game);
            return true;
        }
        return false; // let normal game input handle it
    }

    switch (key) {
        case 27: // Esc
            switch (mode_) {
                case Mode::Dungeon:  exit_dungeon(game); break;
                case Mode::Detail:   exit_detail(game); break;
                case Mode::Overworld: close(game); break;
            }
            return true;

        case KEY_UP:    cursor_y_--; break;
        case KEY_DOWN:  cursor_y_++; break;
        case KEY_LEFT:  cursor_x_--; break;
        case KEY_RIGHT: cursor_x_++; break;

        case ' ': // Paint/place
            switch (paint_mode_) {
                case PaintMode::Tile:    paint_brush(cursor_x_, cursor_y_); break;
                case PaintMode::Fixture: place_fixture(cursor_x_, cursor_y_); break;
                case PaintMode::Npc:     place_npc(cursor_x_, cursor_y_, game); break;
            }
            return true;

        case 'x': case KEY_DELETE: // Delete
            remove_at(cursor_x_, cursor_y_, game);
            return true;

        case '\t': // Cycle paint mode
            if (mode_ == Mode::Overworld) break; // overworld = tile only
            paint_mode_ = static_cast<PaintMode>(
                (static_cast<int>(paint_mode_) + 1) % 3);
            return true;

        case '[': // Previous palette item
            switch (paint_mode_) {
                case PaintMode::Tile:
                    if (palette_cursor_ > 0) --palette_cursor_;
                    else palette_cursor_ = static_cast<int>(palette_.size()) - 1;
                    break;
                case PaintMode::Fixture:
                    if (fixture_cursor_ > 0) --fixture_cursor_;
                    else fixture_cursor_ = static_cast<int>(fixture_palette_.size()) - 1;
                    break;
                case PaintMode::Npc:
                    if (npc_cursor_ > 0) --npc_cursor_;
                    else npc_cursor_ = static_cast<int>(npc_palette_.size()) - 1;
                    break;
            }
            return true;

        case ']': // Next palette item
            switch (paint_mode_) {
                case PaintMode::Tile:
                    palette_cursor_ = (palette_cursor_ + 1) % static_cast<int>(palette_.size());
                    break;
                case PaintMode::Fixture:
                    fixture_cursor_ = (fixture_cursor_ + 1) % static_cast<int>(fixture_palette_.size());
                    break;
                case PaintMode::Npc:
                    npc_cursor_ = (npc_cursor_ + 1) % static_cast<int>(npc_palette_.size());
                    break;
            }
            return true;

        case 'b': // Brush size
            if (paint_mode_ == PaintMode::Tile) {
                if (brush_size_ == 1) brush_size_ = 3;
                else if (brush_size_ == 3) brush_size_ = 5;
                else brush_size_ = 1;
            }
            return true;

        case 'f': // Flood fill
            if (paint_mode_ == PaintMode::Tile) {
                Tile old_t = active_map().get(cursor_x_, cursor_y_);
                flood_fill(cursor_x_, cursor_y_, old_t, active_tile());
            }
            return true;

        case 'u': // Undo
            pop_undo();
            return true;

        case '\n': case '\r': // Enter — mode-specific
            if (mode_ == Mode::Overworld) {
                enter_detail(cursor_x_, cursor_y_, game);
            }
            return true;

        case 'c': // Toggle custom flag (overworld only)
            if (mode_ == Mode::Overworld) {
                bool cur = active_map().custom_detail(cursor_x_, cursor_y_);
                active_map().set_custom_detail(cursor_x_, cursor_y_, !cur);
            }
            return true;

        case 'd': // Enter dungeon (detail only)
            if (mode_ == Mode::Detail) {
                enter_dungeon(game);
            }
            return true;

        case KEY_F2: // Play test
            if (mode_ != Mode::Overworld) {
                start_play(game);
            }
            return true;

        case KEY_PAGE_UP: // Previous zone
            if (mode_ == Mode::Detail) {
                int idx = detail_zone_y_ * zones_per_tile + detail_zone_x_;
                idx = (idx - 1 + zones_per_tile * zones_per_tile) % (zones_per_tile * zones_per_tile);
                switch_zone(idx % zones_per_tile, idx / zones_per_tile, game);
            }
            return true;

        case KEY_PAGE_DOWN: // Next zone
            if (mode_ == Mode::Detail) {
                int idx = detail_zone_y_ * zones_per_tile + detail_zone_x_;
                idx = (idx + 1) % (zones_per_tile * zones_per_tile);
                switch_zone(idx % zones_per_tile, idx / zones_per_tile, game);
            }
            return true;

        // 1-9: jump to zone
        case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9':
            if (mode_ == Mode::Detail) {
                int idx = key - '1';
                switch_zone(idx % zones_per_tile, idx / zones_per_tile, game);
            }
            return true;

        default:
            return true; // consume all keys in editor
    }

    // Clamp cursor
    auto& map = active_map();
    if (cursor_x_ < 0) cursor_x_ = 0;
    if (cursor_y_ < 0) cursor_y_ = 0;
    if (cursor_x_ >= map.width()) cursor_x_ = map.width() - 1;
    if (cursor_y_ >= map.height()) cursor_y_ = map.height() - 1;

    return true;
}

// ─────────────────────────────────────────────────────────────────
// Drawing
// ─────────────────────────────────────────────────────────────────

void MapEditor::draw(int screen_w, int screen_h) {
    if (!open_ || !renderer_) return;
    if (playing_) return; // game renders normally during play

    auto& map = active_map();
    int panel_w = 22;
    int viewport_w = screen_w - panel_w - 1;
    int header_h = 2;
    int status_h = 2;
    int viewport_h = screen_h - header_h - status_h;

    compute_editor_camera(map.width(), map.height(), viewport_w, viewport_h);

    // Full screen clear
    DrawContext full(renderer_, {0, 0, screen_w, screen_h});
    full.fill(' ');

    // Header
    std::string header = " EDITOR: ";
    header += mode_name();
    if (mode_ == Mode::Detail || mode_ == Mode::Dungeon) {
        header += " (" + std::to_string(detail_ow_x_) + "," + std::to_string(detail_ow_y_) + ")";
        header += " Zone [" + std::to_string(detail_zone_x_) + "," + std::to_string(detail_zone_y_) + "]";
    }
    header += "  (" + std::to_string(map.width()) + "x" + std::to_string(map.height()) + ")";

    full.text(0, 0, header, Color::White);
    std::string hints = "[Esc] Back  [F1] Help";
    full.text(screen_w - static_cast<int>(hints.size()) - 1, 0, hints, Color::DarkGray);
    for (int x = 0; x < screen_w; ++x) full.put(x, 1, BoxDraw::H, Color::DarkGray);

    // Separator
    for (int y = header_h; y < screen_h - status_h; ++y)
        full.put(viewport_w, y, BoxDraw::V, Color::DarkGray);

    // Viewport
    DrawContext viewport(renderer_, {0, header_h, viewport_w, viewport_h});
    draw_viewport(viewport);

    // Palette panel
    DrawContext panel(renderer_, {viewport_w + 1, header_h, panel_w, viewport_h});
    draw_palette(panel);

    // Zone minimap (detail/dungeon mode)
    if (mode_ == Mode::Detail || mode_ == Mode::Dungeon) {
        draw_zone_minimap(panel);
    }

    // Status bar
    for (int x = 0; x < screen_w; ++x) full.put(x, screen_h - status_h, BoxDraw::H, Color::DarkGray);
    draw_status(full, screen_w);
}

void MapEditor::draw_viewport(DrawContext& ctx) {
    auto& map = active_map();
    bool is_ow = (mode_ == Mode::Overworld);

    for (int sy = 0; sy < ctx.height(); ++sy) {
        for (int sx = 0; sx < ctx.width(); ++sx) {
            int mx = camera_x_ + sx;
            int my = camera_y_ + sy;
            if (mx < 0 || mx >= map.width() || my < 0 || my >= map.height()) continue;

            Tile t = map.get(mx, my);
            char g = tile_glyph(t);
            Color c = Color::DarkGray;

            if (is_ow) {
                const char* og = overworld_glyph(t, mx, my);
                c = Color::White; // simplified coloring for editor
                ctx.put(sx, sy, og, c);
            } else {
                if (t == Tile::Fixture) {
                    int fid = map.fixture_id(mx, my);
                    if (fid >= 0) {
                        auto& f = map.fixture(fid);
                        g = f.glyph;
                        c = f.color;
                    }
                } else if (t == Tile::Wall || t == Tile::StructuralWall) {
                    c = Color::White;
                } else if (t == Tile::Floor || t == Tile::IndoorFloor) {
                    c = Color::DarkGray;
                } else if (t == Tile::Water) {
                    c = Color::Blue;
                } else if (t == Tile::Empty) {
                    g = ' ';
                } else {
                    c = Color::White;
                }
                ctx.put(sx, sy, g, c);
            }

            // Draw NPCs
            for (const auto& npc : world_->npcs()) {
                if (npc.x == mx && npc.y == my) {
                    ctx.put(sx, sy, npc.glyph, npc.color);
                }
            }
        }
    }

    // Cursor
    int csx = cursor_x_ - camera_x_;
    int csy = cursor_y_ - camera_y_;
    if (csx >= 0 && csx < ctx.width() && csy >= 0 && csy < ctx.height()) {
        if (csx > 0)
            ctx.put(csx - 1, csy, '[', Color::White);
        ctx.put(csx, csy, 'X', Color::Yellow);
        if (csx + 1 < ctx.width())
            ctx.put(csx + 1, csy, ']', Color::White);
    }
}

void MapEditor::draw_palette(DrawContext& ctx) {
    int y = 0;

    const char* mode_label = "TILES";
    if (paint_mode_ == PaintMode::Fixture) mode_label = "FIXTURES";
    else if (paint_mode_ == PaintMode::Npc) mode_label = "NPCS";
    ctx.text(1, y++, mode_label, Color::Cyan);
    y++;

    if (paint_mode_ == PaintMode::Tile) {
        for (int i = 0; i < static_cast<int>(palette_.size()) && y < ctx.height(); ++i) {
            bool sel = (i == palette_cursor_);
            char marker = sel ? '>' : ' ';
            Color fg = sel ? Color::White : Color::DarkGray;
            std::string line = std::string(1, marker) + " " + tile_glyph(palette_[i]) + " " + tile_name(palette_[i]);
            ctx.text(0, y++, line, fg);
        }
    } else if (paint_mode_ == PaintMode::Fixture) {
        for (int i = 0; i < static_cast<int>(fixture_palette_.size()) && y < ctx.height(); ++i) {
            bool sel = (i == fixture_cursor_);
            char marker = sel ? '>' : ' ';
            Color fg = sel ? Color::White : Color::DarkGray;
            auto fd = make_fixture(fixture_palette_[i]);
            std::string line = std::string(1, marker) + " " + fd.glyph + " " + fixture_name(fixture_palette_[i]);
            ctx.text(0, y++, line, fg);
        }
    } else if (paint_mode_ == PaintMode::Npc) {
        for (int i = 0; i < static_cast<int>(npc_palette_.size()) && y < ctx.height(); ++i) {
            bool sel = (i == npc_cursor_);
            char marker = sel ? '>' : ' ';
            Color fg = sel ? Color::White : Color::DarkGray;
            std::string line = std::string(1, marker) + " " + npc_palette_[i].glyph + " " + npc_palette_[i].name;
            ctx.text(0, y++, line, fg);
        }
    }
}

void MapEditor::draw_zone_minimap(DrawContext& ctx) {
    int y = ctx.height() - 8;
    if (y < 5) return;

    ctx.text(1, y++, "ZONE MAP", Color::Cyan);
    // 3x3 grid
    for (int zy = 0; zy < zones_per_tile; ++zy) {
        for (int zx = 0; zx < zones_per_tile; ++zx) {
            int px = 2 + zx * 2;
            bool current = (zx == detail_zone_x_ && zy == detail_zone_y_);
            if (current) {
                ctx.put(px, y, BoxDraw::FULL, Color::Yellow);
            } else {
                ctx.put(px, y, '.', Color::DarkGray);
            }
        }
        y++;
    }
}

void MapEditor::draw_status(DrawContext& ctx, int full_w) {
    int y = ctx.height() - 1;
    auto& map = active_map();
    Tile under = map.get(cursor_x_, cursor_y_);

    std::string left = " Cursor: (" + std::to_string(cursor_x_) + "," + std::to_string(cursor_y_) + ")";

    if (paint_mode_ == PaintMode::Tile) {
        left += "  Tile: " + std::string(tile_name(active_tile()));
        left += " [" + std::string(1, tile_glyph(active_tile())) + "]";
        left += "  Brush: " + std::to_string(brush_size_);
    } else if (paint_mode_ == PaintMode::Fixture) {
        left += "  Fixture: " + std::string(fixture_name(active_fixture()));
    } else {
        left += "  NPC: " + npc_palette_[npc_cursor_].name;
    }

    if (mode_ == Mode::Overworld && map.custom_detail(cursor_x_, cursor_y_)) {
        left += "  [CUSTOM]";
    }

    ctx.text(0, y, left, Color::White);

    std::string right;
    if (mode_ == Mode::Overworld) {
        right = "[Space] Paint  [Enter] Detail  [c] Custom";
    } else {
        right = "[Space] Paint  [Tab] Mode  [F2] Play";
    }
    ctx.text(full_w - static_cast<int>(right.size()) - 1, y, right, Color::DarkGray);
}

} // namespace astra
