#pragma once

#include "astra/renderer.h"
#include "astra/tilemap.h"
#include "astra/npc.h"
#include "astra/ui.h"
#include "astra/world_manager.h"

#include <string>
#include <vector>

namespace astra {

class Game;

class MapEditor {
public:
    MapEditor() = default;

    bool is_open() const { return open_; }
    bool playing() const { return playing_; }
    bool standalone() const { return standalone_; }

    void open(Game& game);             // open from gameplay (teleport to overworld)
    void open_standalone(Game& game);   // open from main menu (blank world)
    void close(Game& game);
    bool handle_input(int key, Game& game);
    void draw(int screen_w, int screen_h);
    void stop_play(Game& game);

    enum class Mode { Overworld, Detail, Dungeon };
    enum class PaintMode { Tile, Fixture, Npc };

private:
    Renderer* renderer_ = nullptr;
    WorldManager* world_ = nullptr;
    bool open_ = false;
    bool standalone_ = false;  // true when opened from main menu
    Mode mode_ = Mode::Overworld;
    PaintMode paint_mode_ = PaintMode::Tile;

    // Return state
    LocationKey return_key_ = {0, 0, 0, false, 0, 0, 0};
    int return_x_ = 0, return_y_ = 0;
    SurfaceMode return_surface_ = SurfaceMode::Dungeon;

    // Cursor & camera
    int cursor_x_ = 0, cursor_y_ = 0;
    int camera_x_ = 0, camera_y_ = 0;

    // Tile palette
    std::vector<Tile> palette_;
    int palette_cursor_ = 0;
    int brush_size_ = 1;

    // Fixture palette
    std::vector<FixtureType> fixture_palette_;
    int fixture_cursor_ = 0;

    // NPC palette
    struct NpcTemplate {
        std::string name;
        NpcRole npc_role;
        std::string role;
    };
    std::vector<NpcTemplate> npc_palette_;
    int npc_cursor_ = 0;

    // Detail context
    int detail_ow_x_ = -1, detail_ow_y_ = -1;
    int detail_zone_x_ = 1, detail_zone_y_ = 1;

    // Play/Stop
    bool playing_ = false;

    // Undo
    struct UndoState { std::vector<Tile> tiles; std::vector<int> fixture_ids; };
    std::vector<UndoState> undo_stack_;
    static constexpr int max_undo_ = 20;

    // Popups
    MenuState popup_;
    bool pending_generate_ = false;
    int pending_biome_ = -1;  // biome index for generate

    // Paint-while-moving mode
    bool painting_ = false;

    // Cursor blink
    int blink_counter_ = 0;

    // Palette init
    void init_overworld_palette();
    void init_detail_palette();
    void init_dungeon_palette();
    void init_fixture_palette();
    void init_npc_palette();

    // Painting
    void paint(int x, int y);
    void paint_brush(int cx, int cy);
    void place_fixture(int x, int y);
    void place_npc(int x, int y, Game& game);
    void remove_at(int x, int y, Game& game);
    void flood_fill(int x, int y, Tile old_tile, Tile new_tile);

    // Mode transitions
    void enter_detail(int ow_x, int ow_y, Game& game);
    void enter_dungeon(Game& game);
    void switch_zone(int zx, int zy, Game& game);
    void exit_detail(Game& game);
    void exit_dungeon(Game& game);

    // Play/Stop
    void start_play(Game& game);

    // Generate
    void generate_zone(Game& game);

    // Undo
    void push_undo();
    void pop_undo();

    // Editor-specific save/restore — copies instead of moves (non-destructive)
    void editor_save_current(Game& game);
    void editor_restore(const LocationKey& key, Game& game);

    // Camera
    void compute_editor_camera(int map_w, int map_h, int viewport_w, int viewport_h);

    // Drawing
    void draw_viewport(UIContext& ctx);
    void draw_palette(UIContext& ctx);
    void draw_zone_minimap(UIContext& ctx);
    void draw_status(UIContext& ctx, int full_w);

    // Helpers
    TileMap& active_map();
    const char* mode_name() const;
    Tile active_tile() const;
    FixtureType active_fixture() const;
};

} // namespace astra
