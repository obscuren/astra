#include "astra/game.h"
#include "astra/aura.h"
#include "astra/grid_input.h"
#include "astra/boot_sequence.h"
#include "astra/dungeon_level_generator.h"
#include "astra/dungeon_recipe.h"
#include "astra/scenarios.h"
#include "astra/faction.h"
#include "astra/faction_territory.h"
#include "astra/debug_spawn.h"
#include "astra/display_name.h"
#include "astra/item_defs.h"
#include "astra/item_ids.h"
#include "astra/journal.h"
#include "astra/galaxy_sim.h"
#include "astra/lore_generator.h"
#include "astra/biome_profile.h"
#include "astra/map_generator.h"
#include "astra/map_properties.h"
#include "astra/overworld_stamps.h"
#include "astra/npc_defs.h"
#include "astra/npc_spawner.h"
#include "astra/skill_grant.h"
#include "astra/ability_bar.h"
#include "astra/station_type.h"
#include "astra/shop.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <unordered_map>

namespace astra {


// Map overworld terrain to detail/dungeon biome, falling back to planet biome for POIs

Game::Game(std::unique_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
    hacking_.bind_game(this);
}

std::string Game::dominant_faction_in_current_map() const {
    std::unordered_map<std::string, int> counts;
    for (const auto& npc : world_.npcs()) {
        if (!npc.faction.empty()) counts[npc.faction]++;
    }
    std::string best;
    int best_n = 0;
    for (const auto& [k, v] : counts) {
        if (v > best_n) { best_n = v; best = k; }
    }
    return best;
}

void Game::run() {
    renderer_->init();
    running_ = true;
    compute_layout();

    render();

    while (running_) {
        bool revealing = playback_viewer_.is_revealing();
        // Plan 7: while the device shell is open, give the loop a short
        // timeout so the connection ritual streams char-by-char and the
        // optional inline progress bar redraws without keystrokes.
        bool shell_open = hacking_.device_shell_open();
        bool needs_timeout = combat_.targeting() || input_.looking()
                           || quit_confirm_.open
                           || auto_walking_ || auto_exploring_
                           || animations_.has_any()
                           || revealing
                           || shell_open;
        int timeout_ms = revealing                                 ? 33
                       : (auto_walking_ || auto_exploring_)         ? 50
                       : animations_.has_active_effects()           ? 80
                       : animations_.has_any()                      ? 200
                       : shell_open                                 ? 50
                                                                    : 300;
        int key = needs_timeout ? renderer_->wait_input_timeout(timeout_ms)
                                : renderer_->wait_input();

        // Check for Ctrl+C quit request (signal fires during read, returning -1)
        if (renderer_->consume_quit_request()) {
            if (!quit_confirm_.open) {
                quit_confirm_.reset();
                quit_confirm_.title = "Quit without saving?";
                quit_confirm_.add_option('y', "Yes, quit");
                quit_confirm_.add_option('n', "No, keep playing");
                quit_confirm_.selection = 0;
                quit_confirm_.open = true;
            }
            // Skip normal input handling — fall through to render
        } else if (key == -1) {
            // Timeout — toggle blink phase for reticule
            combat_.tick_blink();
            hacking_.tick_blink();
            input_.tick_look_blink();
            // Auto-walk/explore step
            if (auto_walking_ || auto_exploring_) {
                auto_step();
            }
            // Plan 7: drive the device shell's per-frame updater while it's
            // open so the connection ritual streams char-by-char even when
            // the player is idle. Long-channel progress rides world ticks
            // (HackingSystem::tick) so the player must spend an action to
            // advance it.
            if (hacking_.device_shell_open()) {
                hacking_.device_shell().tick_frame(*this);
            }
        } else {
            // Any keypress stops auto-walk/explore
            if (auto_walking_ || auto_exploring_) {
                auto_walking_ = false;
                auto_exploring_ = false;
                log("Stopped.");
            } else {
                handle_input(key);
            }
        }

        int w = renderer_->get_width();
        int h = renderer_->get_height();
        if (w != screen_w_ || h != screen_h_) {
            compute_layout();
            if (state_ == GameState::Playing) {
                compute_camera();
            }
        }

        animations_.tick();
        playback_viewer_.tick();
        update();
        render();
    }

    renderer_->shutdown();
}

void Game::compute_layout() {
    screen_w_ = renderer_->get_width();
    screen_h_ = renderer_->get_height();

    screen_rect_ = {0, 0, screen_w_, screen_h_};

    // Vertical layout: stats | HP/tabs | shield | XP/tab-sep | main | effects | abilities
    UIContext root(renderer_.get(), screen_rect_);
    auto vrows = root.rows({
        fixed(1),    // [0] stats bar
        fixed(1),    // [1] HP bar / tabs row
        fixed(1),    // [2] shield bar
        fixed(1),    // [3] XP bar / tab separator row
        fill(),      // [4] main content
        fixed(1),    // [5] effects
        fixed(3),    // [6] abilities (3 rows: arrows hint, hotbar, page indicator)
    });

    stats_bar_rect_ = vrows[0].bounds();
    effects_rect_ = vrows[5].bounds();
    abilities_rect_ = vrows[6].bounds();

    // Panel width calculation
    int panel_w = screen_w_ * 35 / 100;
    if (panel_w < 30) panel_w = 30;
    if (panel_w > screen_w_ / 2) panel_w = screen_w_ / 2;

    int left_w = screen_w_ - panel_w - 1;
    int sep_x = left_w;

    // Tabs sit two rows below the bars row (on the XP-bar row of the left
    // column) so they're visually paired with the widget panel rather than
    // floating up with the stats bar.
    tabs_rect_ = {sep_x + 1, vrows[3].bounds().y, panel_w, 1};

    // Bars always stop before the tab column
    hp_bar_rect_ = {0, vrows[1].bounds().y, left_w, 1};
    shield_bar_rect_ = {0, vrows[2].bounds().y, left_w, 1};
    xp_bar_rect_ = {0, vrows[3].bounds().y, left_w, 1};

    // Detection indicator — right column, HP-bar row (two rows above the
    // widget bar). Mirrors the visual weight of HP/SH/XP on the left.
    detection_indicator_rect_ = {sep_x + 1, vrows[1].bounds().y, panel_w, 1};

    if (panel_visible_) {
        auto main_cols = vrows[4].columns({fill(), fixed(1), fixed(panel_w)});
        map_rect_ = main_cols[0].bounds();
        // Separator spans from the HP-bar row down to the last row of main
        // content, stopping before the effects bar (which owns its own bg).
        int sep_y      = vrows[1].bounds().y;
        int sep_height = vrows[5].bounds().y - sep_y;
        separator_rect_ = {sep_x, sep_y, 1, sep_height};
        // Shift the side panel down one row so the tabs-row separator
        // doesn't overwrite the first widget row.
        auto sp = main_cols[2].bounds();
        side_panel_rect_ = {sp.x, sp.y + 1, sp.w, sp.h - 1};
    } else {
        map_rect_ = vrows[4].bounds();
        separator_rect_ = {sep_x, vrows[1].bounds().y, 1, 3};
        side_panel_rect_ = {0, 0, 0, 0};
    }
}

// --- Input ---

void Game::handle_input(int key) {
    // Rebirth modal/cinematic intercepts input regardless of state — except
    // MainMenu, where it can't have been triggered. Apply() can flip state_
    // mid-call, so we re-check before falling through.
    if (rebirth_.is_active()) {
        rebirth_.handle_key(*this, key);
        return;
    }
    switch (state_) {
        case GameState::MainMenu:
            // Quit confirm intercepts on menu too
            if (quit_confirm_.open) {
                auto qr = quit_confirm_.handle_input(key);
                if (qr == MenuResult::Selected && quit_confirm_.selected_key() == 'y') {
                    running_ = false;
                } else if (qr == MenuResult::Selected || qr == MenuResult::Closed) {
                    quit_confirm_.reset();
                }
                break;
            }
            handle_menu_input(key);
            break;
        case GameState::Playing:
            // Quit confirm takes priority
            if (quit_confirm_.open) {
                auto qr = quit_confirm_.handle_input(key);
                if (qr == MenuResult::Selected && quit_confirm_.selected_key() == 'y') {
                    running_ = false;
                } else if (qr == MenuResult::Selected || qr == MenuResult::Closed) {
                    quit_confirm_.reset();
                }
                break;
            }
            handle_play_input(key);
            break;
        case GameState::GameOver:  handle_gameover_input(key);  break;
        case GameState::LoadMenu:  handle_load_input(key);     break;
        case GameState::HallOfFame: handle_hall_input(key);    break;
        case GameState::Grid:
            if (console_.is_open()) {
                console_.handle_input(key, *this);
                break;
            }
            if (key == '`') {
                console_.toggle();
                break;
            }
            if (grid_input::handle(*this, key)) {
                advance_world(ActionCost::move);
            }
            break;
    }
}

void Game::handle_menu_input(int key) {
    // Character creation overlay takes priority
    if (character_creation_.is_open()) {
        character_creation_.handle_input(key);
        if (character_creation_.is_complete()) {
            auto cr = character_creation_.consume_result();
            new_game(cr);
        }
        return;
    }

    switch (key) {
        case 'w': case 'k': case KEY_UP:
            menu_selection_ = (menu_selection_ - 1 + menu_item_count_) % menu_item_count_;
            break;
        case 's': case 'j': case KEY_DOWN:
            menu_selection_ = (menu_selection_ + 1) % menu_item_count_;
            break;
        case '\n': case '\r': case ' ': {
#ifdef ASTRA_DEV_MODE
            // Dev mode is index 0; shift others by 1
            static constexpr int off = 1;
#else
            static constexpr int off = 0;
#endif
#ifdef ASTRA_DEV_MODE
            if (menu_selection_ == 0) {
                dev_mode_ = true;
                new_game();
            } else
#endif
            if (menu_selection_ == off + 0) {
                dev_mode_ = false;
                character_creation_.open(renderer_.get());
            } else if (menu_selection_ == off + 1) {
                save_slots_ = list_saves();
                // Filter to alive saves only
                save_slots_.erase(
                    std::remove_if(save_slots_.begin(), save_slots_.end(),
                                   [](const SaveSlot& s) { return s.dead; }),
                    save_slots_.end());
                load_selection_ = 0;
                prev_state_ = GameState::MainMenu;
                state_ = GameState::LoadMenu;
            } else if (menu_selection_ == off + 2) {
                save_slots_ = list_saves();
                // Filter to dead saves only, sort by level desc then ticks desc
                save_slots_.erase(
                    std::remove_if(save_slots_.begin(), save_slots_.end(),
                                   [](const SaveSlot& s) { return !s.dead; }),
                    save_slots_.end());
                std::sort(save_slots_.begin(), save_slots_.end(),
                          [](const SaveSlot& a, const SaveSlot& b) {
                              if (a.player_level != b.player_level)
                                  return a.player_level > b.player_level;
                              return a.world_tick > b.world_tick;
                          });
                load_selection_ = 0;
                confirm_delete_ = false;
                state_ = GameState::HallOfFame;
            }
#ifdef ASTRA_DEV_MODE
            else if (menu_selection_ == off + 3) {
                // Map Editor — standalone from main menu
                map_editor_.open_standalone(*this);
                if (map_editor_.is_open()) {
                    state_ = GameState::Playing;
                }
            }
#endif
            else if (menu_selection_ == menu_item_count_ - 1) {
                running_ = false;
            }
            break;
        }
        case 'q':
            running_ = false;
            break;
    }
}

// --- Logic ---

void Game::dev_warp_random() {
    animations_.clear();
    // All generator combinations: {MapType, Biome, label}
    struct DevMap {
        MapType type;
        Biome biome;
        const char* name;
    };
    static constexpr DevMap maps[] = {
        {MapType::SpaceStation,    Biome::Station,  "Space Station"},
        {MapType::DerelictStation, Biome::Station,  "Derelict Station"},
        {MapType::Rocky,           Biome::Rocky,    "Rocky Cave"},
        {MapType::Rocky,           Biome::Ice,      "Ice Cave"},
        {MapType::Rocky,           Biome::Crystal,  "Crystal Cave"},
        {MapType::Rocky,           Biome::Fungal,   "Fungal Cave"},
        {MapType::Rocky,           Biome::Corroded, "Corroded Cave"},
        {MapType::Rocky,           Biome::Sandy,    "Sandy Cave"},
        {MapType::Rocky,           Biome::Aquatic,  "Aquatic Cave"},
        {MapType::Lava,            Biome::Volcanic, "Volcanic Cave"},
        {MapType::Asteroid,        Biome::Rocky,    "Asteroid Tunnel (Rocky)"},
        {MapType::Asteroid,        Biome::Ice,      "Asteroid Tunnel (Ice)"},
        {MapType::Asteroid,        Biome::Crystal,  "Asteroid Tunnel (Crystal)"},
        {MapType::Starship,        Biome::Station,  "Starship Interior"},
    };
    constexpr int map_count = sizeof(maps) / sizeof(maps[0]);

    unsigned warp_seed = static_cast<unsigned>(std::time(nullptr));
    std::mt19937 rng(warp_seed);
    int pick = std::uniform_int_distribution<int>(0, map_count - 1)(rng);
    const auto& m = maps[pick];

    auto props = default_properties(m.type);
    props.biome = m.biome;
    world_.map() = TileMap(props.width, props.height, m.type);
    auto gen = create_generator(m.type);
    gen->generate(world_.map(), props, warp_seed);
    world_.map().set_location_name(m.name);

    world_.map().find_open_spot(player_.x, player_.y);
    world_.npcs().clear();
    world_.ground_items().clear();

    // Spawn enemies in dungeon-type maps
    if (m.type != MapType::SpaceStation && m.type != MapType::Starship) {
        std::mt19937 npc_rng(warp_seed ^ 0xD3ADu);
        std::vector<std::pair<int,int>> occupied = {{player_.x, player_.y}};
        debug_spawn(world_.map(), world_.npcs(), player_.x, player_.y, occupied, npc_rng);
    }

    world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    recompute_fov();
    on_map_loaded();
    compute_camera();
    world_.current_region() = -1;
    world_.set_surface_mode(SurfaceMode::Dungeon);

    log(std::string("[DEV] Warped to: ") + m.name);
    check_region_change();
}

void Game::dev_warp_stamp_test() {
    animations_.clear();
    unsigned warp_seed = static_cast<unsigned>(std::time(nullptr));

    auto props = default_properties(MapType::DetailMap);
    props.biome = Biome::Rocky;
    props.detail_terrain = Tile::OW_Plains;
    props.detail_has_poi = true;
    props.detail_poi_type = dev_warp_stamp_test_poi_;

    world_.map() = TileMap(props.width, props.height, MapType::DetailMap);
    auto gen = create_generator(MapType::DetailMap);
    gen->generate(world_.map(), props, warp_seed);
    world_.map().set_location_name("[DEV] Stamp Test");

    world_.map().find_open_spot(player_.x, player_.y);
    world_.npcs().clear();
    world_.ground_items().clear();

    // Spawn NPCs for settlement/outpost stamp tests
    std::mt19937 npc_rng(warp_seed ^ 0xC1A5u);
    if (dev_warp_stamp_test_poi_ == Tile::OW_Settlement) {
        spawn_settlement_npcs(world_.map(), world_.npcs(), player_.x, player_.y, npc_rng, &player_);
    } else if (dev_warp_stamp_test_poi_ == Tile::OW_Outpost) {
        spawn_outpost_npcs(world_.map(), world_.npcs(), player_.x, player_.y, npc_rng, &player_);
    }

    world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    recompute_fov();
    on_map_loaded();
    compute_camera();
    world_.current_region() = -1;
    world_.set_surface_mode(SurfaceMode::Dungeon);

    check_region_change();
}

// ── Dev console ─────────────────────────────────────────────────────

void Game::dev_command_warp_random() {
    dev_warp_random();
}

void Game::dev_command_warp_stamp(Tile poi) {
    dev_warp_stamp_test_poi_ = poi;
    dev_warp_stamp_test();
}

void Game::dev_command_warp_to_system(uint32_t system_id) {
    // Find the target system
    const StarSystem* target = nullptr;
    for (const auto& sys : world_.navigation().systems) {
        if (sys.id == system_id) { target = &sys; break; }
    }
    if (!target) return;

    // Reuse the same logic as WarpToSystem in game_world.cpp
    save_current_location();
    world_.navigation().current_system_id = target->id;
    discover_nearby(world_.navigation(), target->id, 20.0f);
    world_.navigation().on_ship = true;
    world_.navigation().at_station = false;
    world_.navigation().current_body_index = -1;
    world_.navigation().current_moon_index = -1;

    if (world_.location_cache().count(WorldManager::ship_key)) {
        restore_location(WorldManager::ship_key);
    } else {
        unsigned ship_seed = world_.seed() ^ 0x5B1Bu;
        auto props = default_properties(MapType::Starship);
        world_.map() = TileMap(props.width, props.height, MapType::Starship);
        auto gen = create_starship_generator();
        gen->generate(world_.map(), props, ship_seed);
        world_.map().set_location_name("Your Starship");
        world_.npcs().clear();
        world_.ground_items().clear();
        if (!world_.map().find_open_spot_in_region(0, player_.x, player_.y, {})) {
            world_.map().find_open_spot(player_.x, player_.y);
        }
        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    world_.visibility().reveal_all();
    world_.current_region() = -1;
    recompute_fov();
    on_map_loaded();
    compute_camera();
    check_region_change();
}

void Game::dev_command_level_up() {
    player_.xp = player_.max_xp;
    combat_.check_level_up(*this);
}

void Game::dev_command_kill_hostiles() {
    for (auto& npc : world_.npcs()) {
        if (npc.alive() && is_hostile_to_player(npc.faction, player_)) {
            npc.hp = 0;
        }
    }
    combat_.remove_dead_npcs(*this);
}

void Game::dev_command_biome_test(Biome biome, int layer,
                                  const std::string& poi_type,
                                  const std::string& poi_style,
                                  bool connected,
                                  const std::string& civ_name,
                                  float ruin_decay) {
    (void)layer;
    animations_.clear();
    unsigned seed = static_cast<unsigned>(std::time(nullptr));

    auto props = default_properties(MapType::DetailMap);
    props.biome = biome;
    props.light_bias = 100;

    if (poi_type == "settlement") {
        props.detail_has_poi = true;
        props.detail_poi_type = Tile::OW_Settlement;
        if (poi_style == "advanced") {
            props.lore_tier = 2;
            props.lore_alien_strength = 0.5f;
        } else if (poi_style == "ruined") {
            props.lore_tier = 1;
            props.lore_plague_origin = true;
        } else {
            // frontier (default)
            props.lore_tier = 1;
        }
    } else if (poi_type == "ruins") {
        props.detail_has_poi = true;
        props.detail_poi_type = Tile::OW_Ruins;
        props.detail_ruin_civ = civ_name;
        props.detail_ruin_decay = ruin_decay;
        props.lore_tier = 1;
        if (connected) {
            props.detail_neighbor_n = Tile::OW_Ruins;
            props.detail_neighbor_s = Tile::OW_Ruins;
            props.detail_neighbor_e = Tile::OW_Ruins;
            props.detail_neighbor_w = Tile::OW_Ruins;
        }
    } else if (poi_type == "outpost") {
        props.detail_has_poi = true;
        props.detail_poi_type = Tile::OW_Outpost;
        props.lore_tier = 1;
    } else if (poi_type == "ship") {
        props.detail_has_poi = true;
        props.detail_poi_type = Tile::OW_CrashedShip;
        props.detail_crashed_ship_class = poi_style;  // "" / pod / freighter / corvette
        props.lore_tier = 1;
    } else if (poi_type == "cave") {
        props.detail_has_poi = true;
        props.detail_poi_type = Tile::OW_CaveEntrance;
        props.detail_cave_variant = poi_style;  // "" / natural / mine / excavation
        props.lore_tier = 1;
    }

    world_.map() = TileMap(props.width, props.height, MapType::DetailMap);
    auto gen = create_generator(MapType::DetailMap);
    gen->generate(world_.map(), props, seed);
    world_.map().set_biome(biome);

    std::string loc_name = "[DEV] Biome Test: " + biome_profile(biome).name;
    if (poi_type == "settlement") {
        std::string style_display = poi_style.empty() ? "frontier" : poi_style;
        // Capitalize first letter
        if (!style_display.empty())
            style_display[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(style_display[0])));
        loc_name += " + " + style_display + " Settlement";
    } else if (poi_type == "ruins") {
        loc_name += " + Ruins";
        if (connected) loc_name += " (connected)";
    } else if (poi_type == "outpost") {
        loc_name += " + Outpost";
    } else if (poi_type == "ship") {
        loc_name += " + Crashed Ship";
        if (!poi_style.empty()) {
            std::string s = poi_style;
            s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
            loc_name += " (" + s + ")";
        }
    } else if (poi_type == "cave") {
        loc_name += " + Cave Entrance";
        if (!poi_style.empty()) {
            std::string s = poi_style;
            s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
            loc_name += " (" + s + ")";
        }
    }
    world_.map().set_location_name(loc_name);

    world_.map().find_open_spot(player_.x, player_.y);
    world_.npcs().clear();
    world_.ground_items().clear();

    if (poi_type == "settlement") {
        std::mt19937 npc_rng(seed ^ 0x4E5C5u);
        int size_cat = 0;
        bool harsh = (biome == Biome::Volcanic || biome == Biome::ScarredScorched
                   || biome == Biome::ScarredGlassed || biome == Biome::Ice);
        bool lush = (biome == Biome::Forest || biome == Biome::Jungle
                  || biome == Biome::Grassland || biome == Biome::Marsh);
        if (harsh || props.lore_tier == 0) size_cat = 0;
        else if (lush && props.lore_tier >= 2) size_cat = 2;
        else size_cat = 1;

        std::string sname = "Frontier";
        if (props.lore_plague_origin) sname = "Ruined";
        else if (props.lore_tier >= 2) sname = "Advanced";

        spawn_settlement_npcs_v2(world_.map(), world_.npcs(),
                                  player_.x, player_.y, npc_rng, &player_,
                                  size_cat, sname, biome);
    } else if (poi_type == "ruins") {
        std::mt19937 npc_rng(seed ^ 0x4E5C5u);
        spawn_settlement_npcs_v2(world_.map(), world_.npcs(),
                                  player_.x, player_.y, npc_rng, &player_,
                                  0, "Ruined", biome);
    } else if (poi_type == "outpost") {
        std::mt19937 npc_rng(seed ^ 0x4E5C5u);
        spawn_outpost_npcs(world_.map(), world_.npcs(),
                           player_.x, player_.y, npc_rng, &player_);
    }

    world_.visibility() = VisibilityMap(props.width, props.height);
    recompute_fov();
    on_map_loaded();
    compute_camera();
    world_.current_region() = -1;
    world_.set_surface_mode(SurfaceMode::Dungeon);

    check_region_change();
}

void Game::dev_command_dungen(dungeon::StyleId style_id,
                              const std::string& civ_name,
                              uint32_t seed) {
    animations_.clear();

    // Build a 3-level recipe and register it in the world so descend_stairs /
    // ascend_stairs can traverse between levels. The last level is a boss
    // level (no StairsDown) so the final floor is a dead-end.
    DungeonRecipe r;
    r.kind_tag    = "dev_dungen";
    r.level_count = 3;
    for (int d = 0; d < r.level_count; ++d) {
        DungeonLevelSpec s;
        s.style_id    = style_id;
        s.civ_name    = civ_name;
        s.decay_level = std::max(0, 3 - d);   // 3 at L1, 2 at L2, 0 at L3 (pristine)
        s.enemy_tier  = d + 1;
        s.is_boss_level = (d == r.level_count - 1);
        // Put the Nova resonance crystal on the final level so the
        // required_plinth / SanctumCenter mechanism is exercised by :dungen.
        if (s.is_boss_level) {
            s.fixtures.push_back(PlannedFixture{ "nova_resonance_crystal", "required_plinth" });
        }
        r.levels.push_back(s);
    }

    // Register the recipe at the moon-root LocationKey so descend_stairs'
    // fallback lookup finds it (moon_root uses ow_x = ow_y = -1).
    auto& nav = world_.navigation();
    LocationKey moon_root{
        nav.current_system_id,
        nav.current_body_index,
        nav.current_moon_index,
        nav.at_station,
        -1, -1, 0
    };
    world_.dungeon_recipes()[moon_root] = r;

    auto props = default_properties(MapType::DerelictStation);
    world_.map() = TileMap(props.width, props.height, MapType::DerelictStation);

    generate_dungeon_level(world_.map(), r, 1, seed, {-1, -1});

    const auto& sc = dungeon::style_config(style_id);
    world_.map().set_location_name(
        std::string("[DEV] dungen: ") + sc.debug_name + " / " + civ_name);

    world_.npcs().clear();
    world_.ground_items().clear();

    // Place player at StairsUp; fall back to any open spot.
    auto up_pos = find_stairs_up(world_.map());
    if (up_pos.first >= 0) {
        player_.x = up_pos.first;
        player_.y = up_pos.second;
    } else {
        world_.map().find_open_spot(player_.x, player_.y);
    }

    // Smoke-test hostiles on L1.
    {
        std::mt19937 npc_rng(seed ^ 0x5A5Au);
        std::vector<std::pair<int,int>> occupied;
        const int avoid_x = player_.x;
        const int avoid_y = player_.y;
        for (int i = 0; i < 4; ++i) {
            int nx = 0, ny = 0;
            if (!world_.map().find_open_spot_far_from(
                    avoid_x, avoid_y, /*min_dist*/ 6,
                    nx, ny, occupied, &npc_rng))
                break;
            Npc n = create_npc_by_role("rust_hound", npc_rng);
            n.x = nx;
            n.y = ny;
            occupied.push_back({nx, ny});
            world_.npcs().push_back(std::move(n));
        }
    }

    // Mark that we're on L1 of this dev dungeon so descend_stairs treats it
    // as a real dungeon chain (otherwise descend would think we're at depth 0
    // which expects a DungeonHatch entry rather than StairsDown).
    nav.current_depth = 1;

    world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    world_.set_surface_mode(SurfaceMode::Dungeon);
    world_.current_region() = -1;
    recompute_fov();
    on_map_loaded();
    compute_camera();
    check_region_change();
}

std::string Game::dev_command_dumpmap(const std::string& path_in) {
    const TileMap& m = world_.map();
    const int w = m.width();
    const int h = m.height();

    std::string path = path_in;
    if (path.empty()) path = "/tmp/astra_map.txt";

    std::ofstream out(path);
    if (!out) return {};

    out << "=== astra dumpmap ===\n";
    out << "size: " << w << "x" << h << "\n";
    out << "regions: " << m.region_count() << "\n";
    out << "fixtures: " << m.fixture_count() << "\n";
    out << "\n--- tiles (glyph view; fixtures overlay) ---\n";

    auto tile_glyph = [](Tile t) -> char {
        switch (t) {
            case Tile::Floor:          return '.';
            case Tile::Wall:           return '#';
            case Tile::StructuralWall: return 'H';
            case Tile::Water:          return '~';
            case Tile::Ice:            return 'i';
            case Tile::Portal:         return 'O';
            case Tile::Empty:          return ' ';
            default:                   return '?';
        }
    };

    auto fix_glyph = [](FixtureType ft) -> char {
        switch (ft) {
            case FixtureType::StairsUp:         return '<';
            case FixtureType::StairsDown:       return '>';
            case FixtureType::StairsDownPrecursor: return '>';
            case FixtureType::DungeonHatch:     return 'v';
            case FixtureType::Plinth:           return 'T';
            case FixtureType::Altar:            return 'A';
            case FixtureType::Inscription:      return 'i';
            case FixtureType::Pillar:           return 'I';
            case FixtureType::Brazier:          return '*';
            case FixtureType::ResonancePillar:  return '%';
            case FixtureType::ResonancePillarTop:return '^';
            case FixtureType::ResonancePillarBot:return 'v';
            case FixtureType::PrecursorBracketL:return '(';
            case FixtureType::PrecursorBracketR:return ')';
            case FixtureType::CrystalColumn:    return 'D';
            case FixtureType::QuestFixture:     return 'Q';
            case FixtureType::Door:             return '+';
            default:                            return 'F';  // generic fixture
        }
    };

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int fid = m.fixture_id(x, y);
            if (fid >= 0) {
                out << fix_glyph(m.fixture(fid).type);
            } else {
                out << tile_glyph(m.get(x, y));
            }
        }
        out << '\n';
    }

    out << "\n--- region ids ---\n";
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int rid = m.region_id(x, y);
            if (rid < 0)      out << '.';
            else if (rid < 10) out << static_cast<char>('0' + rid);
            else if (rid < 36) out << static_cast<char>('a' + (rid - 10));
            else              out << '+';
        }
        out << '\n';
    }

    out << "\n--- fixtures list ---\n";
    for (int i = 0; i < m.fixture_count(); ++i) {
        const auto& f = m.fixture(i);
        // Find first tile owning this fixture id.
        int fx = -1, fy = -1;
        for (int y = 0; y < h && fx < 0; ++y) {
            for (int x = 0; x < w && fx < 0; ++x) {
                if (m.fixture_id(x, y) == i) { fx = x; fy = y; }
            }
        }
        out << "  [" << i << "] type=" << static_cast<int>(f.type)
            << " at (" << fx << "," << fy << ")"
            << " passable=" << (f.passable ? 'y' : 'n')
            << " interactable=" << (f.interactable ? 'y' : 'n');
        if (!f.quest_fixture_id.empty()) out << " qid=\"" << f.quest_fixture_id << "\"";
        out << '\n';
    }

    out << "\n--- puzzles list ---\n";
    for (int i = 0; i < m.puzzle_count(); ++i) {
        const auto& p = m.puzzle(i);
        out << "  [" << i << "] id=" << p.id
            << " kind=" << static_cast<int>(p.kind)
            << " solved=" << (p.solved ? "y" : "n")
            << " button=(" << p.button_pos.first << "," << p.button_pos.second << ")"
            << " stairs=(" << p.stairs_pos.first << "," << p.stairs_pos.second << ")"
            << " sealed_tiles=" << p.sealed_tiles.size()
            << '\n';
    }

    out.close();
    return path;
}

void Game::new_game() {
    compute_layout();

    // Boot sequence for normal games, skip in dev mode
    if (!dev_mode_) {
        BootSequence boot(renderer_.get());
        boot.play();
    }

    // Wipe any leftover GridNetwork state from a previous session so the
    // Plan 5 LAN auto-registration (triggered by on_map_loaded() below)
    // populates a clean graph. The galaxy-gen block further down used to
    // do this clear() AFTER on_map_loaded ran, which silently wiped every
    // freshly-registered Subnet node.
    world_.grid_network().clear();
    // Plan 5.5: clear per-map LAN buckets too, otherwise leftovers from a
    // previous session would silently shadow the fresh registration.
    world_.lan_metadatas().clear();

    world_.seed() = static_cast<unsigned>(std::time(nullptr));
    world_.rng().seed(world_.seed());

    auto props = default_properties(MapType::SpaceStation);
    props.height = 80; // hub needs extra vertical space for 3-row grid
    world_.map() = TileMap(props.width, props.height, MapType::SpaceStation);
    auto gen = create_hub_generator();
    gen->generate(world_.map(), props, world_.seed());
    world_.map().set_location_name("The Heavens Above");

    player_ = Player{};
    player_.money = 50;
    if (dev_mode_) {
        add_effect(player_.effects, make_invulnerable_ge());
        rebuild_auras_from_sources(player_);
        player_.name = "Dev Commander";
        player_.race = Race::Human;
        player_.player_class = PlayerClass::DevCommander;

        // Apply class template
        const auto& tmpl = class_template(player_.player_class);
        player_.attributes = tmpl.attributes;
        player_.resistances = tmpl.resistances;
        player_.max_hp += tmpl.bonus_hp;
        player_.inventory.max_carry_weight += tmpl.bonus_carry_weight;
        // Dev Commander learns every skill and category for testing.
        player_.learned_skills.clear();
        player_.ability_slots.clear();
        for (const auto& cat : skill_catalog()) {
            grant_skill(player_, cat.unlock_id);
            for (const auto& sk : cat.skills) {
                grant_skill(player_, sk.id);
            }
        }
        ability_bar::reconcile_from_learned(player_);
        player_.skill_points = tmpl.starting_sp;
        player_.money += tmpl.starting_money;
        player_.attribute_points = 10;

        // Dev Commander: knows the three Basic recipes, carries a full
        // ingredient pantry, and gets one of each cookbook in inventory so
        // the whole discovery/read flow can be exercised in-game.
        player_.known_recipes = { 1, 2, 3 };
        auto stack_of = [](Item it, int n) { it.stack_count = n; return it; };
        player_.inventory.items.push_back(stack_of(build_by_def_id(ITEM_RAW_MEAT),       10));
        player_.inventory.items.push_back(stack_of(build_by_def_id(ITEM_CARROT),         10));
        player_.inventory.items.push_back(stack_of(build_by_def_id(ITEM_FLOUR),          10));
        player_.inventory.items.push_back(stack_of(build_by_def_id(ITEM_HERBS),          10));
        player_.inventory.items.push_back(stack_of(build_by_def_id(ITEM_SYNTH_PROTEIN), 10));
        player_.inventory.items.push_back(build_by_def_id(ITEM_COOKBOOK_HEARTY_STEW));
        player_.inventory.items.push_back(build_by_def_id(ITEM_COOKBOOK_PROTEIN_BAKE));
        player_.inventory.items.push_back(build_by_def_id(ITEM_COOKBOOK_HEROS_FEAST));

        player_.max_hp = player_.effective_max_hp();
        player_.hp = player_.max_hp;

        player_.reputation = {
            {Faction_StellariConclave,  10},
            {Faction_KrethMiningGuild,  0},
            {Faction_VeldraniAccord,    0},
            {Faction_SylphariWanderers, 0},
            {Faction_TerranFederation,  0},
            {Faction_XytomorphHive,     -400},
            {Faction_VoidReavers,       -350},
            {Faction_ArchonRemnants,    -400},
            {Faction_Feral,             -400},
            {Faction_DriftCollective,   0},
        };
    }
    // Always start in the Docking Bay (region 0)
    if (!world_.map().find_open_spot_in_region(0, player_.x, player_.y, {})) {
        world_.map().find_open_spot(player_.x, player_.y);
    }

    // Spawn NPCs in rooms based on room flavor
    world_.npcs().clear();
    world_.ground_items().clear();
    std::mt19937 npc_rng(static_cast<unsigned>(std::time(nullptr)) ^ 0xA7C3u);
    StationContext tha_ctx{ .is_tha = true };
    spawn_hub_npcs(world_.map(), world_.npcs(), player_.x, player_.y, npc_rng, &player_, tha_ctx);

    world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    recompute_fov();
    on_map_loaded();
    compute_camera();

    messages_.clear();
    awaiting_interact_ = false;
    combat_.reset();
    hacking_.reset();
    qh_picker_.open = false;
    qh_picker_slots_.clear();
    input_.cancel_look();
    ;
    inventory_cursor_ = 0;
    world_.current_region() = -1;
    active_widgets_ = widget_default;
    focused_widget_ = 0; // Start on Messages tab
    world_.set_surface_mode(SurfaceMode::Dungeon);
    world_.overworld_x() = 0;
    world_.overworld_y() = 0;
    world_.world_tick() = 0;
    world_.day_clock() = DayClock{};  // station day = 200 ticks
    world_.location_cache().clear();
    // Initialize ship with random name
    player_.ship.name = generate_ship_name(world_.rng());
    if (dev_mode_) {
        // Dev mode: ship fully equipped
        player_.ship.engine = build_engine_coil_mk1(); // sanctioned one-off
        player_.ship.hull = build_by_def_id(ITEM_HULL_PLATE);
        player_.ship.navi_computer = build_by_def_id(ITEM_NAVI_COMPUTER_MK2);
    }

    if (dev_mode_) {
        log("--- DEVELOPER MODE --- Saving disabled.");
    }
    log("Welcome aboard, commander. Your journey to Sgr A* begins.");
    log("You are docked at The Heavens Above, the space station orbiting Jupiter.");
    show_welcome_ = true;
    check_region_change();

    // Starter gear
    if (dev_mode_) {
        // Dev Commander gets a full loadout
        player_.equipment.head = build_by_def_id(ITEM_TACTICAL_HELMET);
        player_.equipment.body = build_by_def_id(ITEM_COMPOSITE_ARMOR);
        player_.equipment.feet = build_by_def_id(ITEM_MAG_LOCK_BOOTS);
        player_.equipment.left_arm = build_by_def_id(ITEM_ARM_GUARD);
        auto right_arm = build_by_def_id(ITEM_ARM_GUARD);
        right_arm.slot = EquipSlot::RightArm;
        player_.equipment.right_arm = right_arm;
        player_.equipment.right_hand = build_by_def_id(ITEM_VIBRO_BLADE);
        player_.equipment.missile = build_by_def_id(ITEM_ION_BLASTER);
        player_.equipment.face = build_by_def_id(ITEM_RECON_VISOR);
        player_.equipment.back = build_by_def_id(ITEM_JETPACK);
        player_.equipment.thrown = build_by_def_id(ITEM_FRAG_GRENADE);
        player_.equipment.thrown->stack_count = 5;

        // Inventory: consumables + crafting mats + extras
        auto stack = [](Item it, int n) { it.stack_count = n; return it; };
        player_.inventory.items.push_back(stack(build_by_def_id(ITEM_BATTERY), 5));
        player_.inventory.items.push_back(stack(build_by_def_id(ITEM_RATION_PACK), 10));
        player_.inventory.items.push_back(stack(build_by_def_id(ITEM_COMBAT_STIM), 3));
        player_.inventory.items.push_back(stack(build_by_def_id(ITEM_NANO_FIBER), 10));
        player_.inventory.items.push_back(stack(build_by_def_id(ITEM_POWER_CORE), 10));
        player_.inventory.items.push_back(stack(build_by_def_id(ITEM_CIRCUIT_BOARD), 10));
        player_.inventory.items.push_back(stack(build_by_def_id(ITEM_ALLOY_INGOT), 10));
        player_.inventory.items.push_back(build_by_def_id(ITEM_COMBAT_KNIFE));
        player_.inventory.items.push_back(build_by_def_id(ITEM_PLASMA_PISTOL));
        player_.inventory.items.push_back(stack(build_by_def_id(ITEM_EMP_GRENADE), 2));
        player_.inventory.items.push_back(build_by_def_id(ITEM_COMPOSITE_BARRIER));

        // Hacking + tinkering loadout: every Cat_Hacking skill (incl.
        // CodeCraft and the ConsciousnessAnchor capstone with its deep-Grid
        // base side effect), the four Tinkering skills, a T2 cyberdeck in
        // utility1, a Neural Backup implant, and a sampler of programs.
        const SkillId hack_skills[] = {
            SkillId::Cat_Hacking,
            SkillId::Intrusion,
            SkillId::IceBreaking,
            SkillId::DaemonMastery,
            SkillId::GhostProtocol,
            SkillId::DeepGridNavigator,
            SkillId::NeuralFortitude,
            SkillId::CodeCraft,
            SkillId::ConsciousnessAnchor,
            SkillId::Cat_Tinkering,
            SkillId::BasicRepair,
            SkillId::Disassemble,
            SkillId::Synthesize,
        };
        for (SkillId s : hack_skills) grant_skill(player_, s);
        apply_skill_side_effects(*this, SkillId::ConsciousnessAnchor);

        player_.equipment.utility1 = build_by_def_id(ITEM_POLYGLOT_DCK2);
        player_.implants[0]        = build_by_def_id(ITEM_NEURAL_BACKUP);

        const uint16_t hack_programs[] = {
            ITEM_PROG_ICEBREAKER_LITE,
            ITEM_PROG_GHOST_TRACE,
            ITEM_PROG_BREACH,
            ITEM_PROG_DECRYPT,
            ITEM_PROG_REBOOT_OPTICS,
            ITEM_PROG_DATA_LEECH,
            ITEM_PROG_FRIENDLY_FIRE,
        };
        for (uint16_t pid : hack_programs) {
            add_to_inventory_stacked(player_.inventory, build_by_def_id(pid));
        }

        // Pre-learn some blueprints for Synthesizer testing
        player_.learned_blueprints.push_back({1001, "Plasma Emitter", "A superheated plasma projection system."});
        player_.learned_blueprints.push_back({1101, "Blade Housing", "Structural frame for edged weapons."});
        player_.learned_blueprints.push_back({3001, "Plating Alloy", "Composite metal alloy for defensive plating."});
        player_.learned_blueprints.push_back({4001, "Optic Module", "Enhanced optical sensor array."});

        // Journal entries for all pre-learned blueprints
        player_.journal.push_back(make_blueprint_journal_entry(
            "Plasma Emitter", "A superheated plasma projection system.",
            "Plasma Pistol", 0, "Dawn"));
        player_.journal.push_back(make_blueprint_journal_entry(
            "Blade Housing", "Structural frame for edged weapons.",
            "Combat Knife", 0, "Dawn"));
        player_.journal.push_back(make_blueprint_journal_entry(
            "Plating Alloy", "Composite metal alloy for defensive plating.",
            "Padded Vest", 0, "Dawn"));
        player_.journal.push_back(make_blueprint_journal_entry(
            "Optic Module", "Enhanced optical sensor array.",
            "Recon Visor", 0, "Dawn"));

        log("Full loadout equipped.");
    }

    Item weapon = build_by_def_id(ITEM_PLASMA_PISTOL);
    if (!player_.equipment.missile) {
        player_.equipment.missile = weapon;
    } else {
        player_.inventory.items.push_back(weapon);
    }
    log("You are armed with a " + weapon.name + ".");

    Item battery = build_by_def_id(ITEM_BATTERY);
    battery.stack_count = 3;
    player_.inventory.items.push_back(battery);

    // Generate the galaxy — lore first, then star chart, then map lore onto systems
    // Show visual progress during generation
    {
        int sw = renderer_->get_width();
        int sh = renderer_->get_height();
        std::vector<std::string> event_log;
        std::string current_phase = "Initializing...";
        int bar_progress = 0;

        auto render_progress = [&]() {
            renderer_->clear();
            int margin = 4;
            int y = margin;

            // Title
            auto put_text = [&](int x, int yi, const std::string& s, Color c) {
                for (int ci = 0; ci < static_cast<int>(s.size()) && x + ci < sw; ++ci)
                    renderer_->draw_char(x + ci, yi, s[ci], c);
            };

            put_text(margin, y, "GENERATING UNIVERSE", Color::Cyan);
            y += 2;

            // Progress bar
            int bar_w = std::min(40, sw - margin * 2 - 10);
            std::string bar = "[";
            int filled = bar_progress * bar_w / 8000;
            for (int i = 0; i < bar_w; ++i)
                bar += (i < filled) ? "\xe2\x96\x88" : "\xe2\x96\x91";
            bar += "]";
            float bya = static_cast<float>(8000 - bar_progress) / 1000.0f;
            char time_str[32];
            std::snprintf(time_str, sizeof(time_str), " %.1f Bya", bya);

            // Draw bar character by character (mix of UTF-8)
            int bx = margin;
            renderer_->draw_char(bx++, y, '[', Color::DarkGray);
            for (int i = 0; i < bar_w; ++i) {
                if (i < filled)
                    renderer_->draw_glyph(bx++, y, "\xe2\x96\x88", Color::Cyan);
                else
                    renderer_->draw_glyph(bx++, y, "\xe2\x96\x91", Color::DarkGray);
            }
            renderer_->draw_char(bx++, y, ']', Color::DarkGray);
            put_text(bx + 1, y, time_str, Color::DarkGray);
            y += 2;

            // Current phase
            put_text(margin, y, current_phase, Color::Yellow);
            y += 2;

            // Event log — show last N events that fit
            int log_space = sh - y - 2;
            int start = std::max(0, static_cast<int>(event_log.size()) - log_space);
            for (int i = start; i < static_cast<int>(event_log.size()); ++i) {
                Color c = Color::DarkGray;
                const auto& line = event_log[i];
                if (line.find("EMERGED") != std::string::npos) c = Color::Green;
                else if (line.find("COLLAPSED") != std::string::npos ||
                         line.find("TRANSCENDED") != std::string::npos) c = Color::Red;
                else if (line.find("BATTLE") != std::string::npos ||
                         line.find("WAR") != std::string::npos) c = Color::Yellow;
                else if (line.find("BEACON") != std::string::npos ||
                         line.find("MEGASTRUCTURE") != std::string::npos) c = Color::Cyan;
                else if (line.find("BREAKTHROUGH") != std::string::npos) c = Color::Magenta;

                std::string display = line;
                if (static_cast<int>(display.size()) > sw - margin * 2)
                    display = display.substr(0, sw - margin * 2);
                put_text(margin + 1, y++, display, c);
            }

            renderer_->present();
        };

        world_.lore() = GalaxySim::run(world_.seed(), [&](const SimProgress& p) {
            bar_progress = p.tick;

            if (p.phase_complete) {
                current_phase = p.phase_name;
                render_progress();
                return;
            }

            // Log significant events
            if (!p.event_text.empty()) {
                float bya_f = static_cast<float>(8000 - p.tick) / 1000.0f;
                char prefix[32];
                std::snprintf(prefix, sizeof(prefix), "%.2f Bya ", bya_f);
                std::string line = prefix + p.civ_name + ": " + p.event_text;
                event_log.push_back(line);
            }

            // Render every 50 ticks to keep it smooth but not too slow
            if (p.tick % 50 == 0 || p.phase_complete) {
                current_phase = "Simulating deep time... (" +
                    std::to_string(p.active_civs) + " active, " +
                    std::to_string(p.dead_civs) + " fallen)";
                render_progress();
            }
        });

        // Show final phase
        current_phase = "Generating star chart...";
        render_progress();
    }
    // grid_network().clear() moved to the top of new_game() so it runs
    // BEFORE on_map_loaded() registers Plan 5 LAN nodes — clearing here
    // wiped the freshly-registered Subnets.
    world_.navigation() = generate_galaxy(world_.seed());
    apply_lore_to_galaxy(world_.navigation(), world_.lore());
    assign_system_factions(world_.navigation(), world_.seed());
    world_.navigation().at_station = true;
    world_.navigation().current_body_index = -1;
    star_chart_viewer_ = StarChartViewer(&world_.navigation(), renderer_.get(), &world_, &quest_manager_);
#ifdef ASTRA_DEV_MODE
    star_chart_viewer_.set_dev_mode(dev_mode_);
#endif

    quest_manager_ = QuestManager{};  // fresh manager for new game
    quest_manager_.init_from_catalog(*this);

#ifdef ASTRA_DEV_MODE
    // Dev commander only: skip the tutorial. Auto-accept + auto-complete
    // Getting Airborne so the DAG immediately unlocks downstream arcs
    // (Stellar Signal Stage 1, etc.) for iteration. Non-dev characters
    // still play the tutorial normally even in a dev-mode build.
    if (dev_mode_ &&
        quest_manager_.accept_available("story_getting_airborne", *this,
                                        world_.world_tick())) {
        quest_manager_.complete_quest("story_getting_airborne", *this,
                                      world_.world_tick());
        log("[DEV] Getting Airborne auto-completed — downstream arcs unlocked.");
    }
#endif

    apply_passive_skill_effects();
    state_ = GameState::Playing;

    event_bus_.clear();
    register_all_scenarios(*this);
}

void Game::new_game(const CreationResult& cr) {
    compute_layout();

    // Boot sequence
    BootSequence boot(renderer_.get());
    boot.play();

    // Wipe any leftover GridNetwork state from a previous session so the
    // Plan 5 LAN auto-registration (triggered by on_map_loaded() below)
    // populates a clean graph.
    world_.grid_network().clear();
    // Plan 5.5: clear per-map LAN buckets too.
    world_.lan_metadatas().clear();

    world_.seed() = static_cast<unsigned>(std::time(nullptr));
    world_.rng().seed(world_.seed());

    auto props = default_properties(MapType::SpaceStation);
    props.height = 80;
    world_.map() = TileMap(props.width, props.height, MapType::SpaceStation);
    auto gen = create_hub_generator();
    gen->generate(world_.map(), props, world_.seed());
    world_.map().set_location_name("The Heavens Above");

    player_ = Player{};
    player_.name = cr.name;
    player_.race = cr.race;
    player_.player_class = cr.player_class;
    player_.attributes = cr.attributes;
    player_.resistances = cr.resistances;
    player_.money = 50;

    // Apply class template for non-attribute bonuses
    const auto& tmpl = class_template(cr.player_class);
    player_.max_hp += tmpl.bonus_hp;
    player_.inventory.max_carry_weight += tmpl.bonus_carry_weight;
    player_.learned_skills.clear();
    player_.ability_slots.clear();
    for (SkillId id : tmpl.starting_skills) {
        grant_skill(player_, id);
    }
    ability_bar::reconcile_from_learned(player_);
    player_.skill_points = tmpl.starting_sp;
    player_.money += tmpl.starting_money;

    // All players start knowing the three Basic recipes.
    player_.known_recipes = { 1, 2, 3 };

    player_.max_hp = player_.effective_max_hp();
    player_.hp = player_.max_hp;

    // Initialize reputation for all factions
    auto race_faction = [](Race r) -> const char* {
        switch (r) {
            case Race::Stellari:   return Faction_StellariConclave;
            case Race::Kreth:      return Faction_KrethMiningGuild;
            case Race::Veldrani:   return Faction_VeldraniAccord;
            case Race::Sylphari:   return Faction_SylphariWanderers;
            case Race::Human:      return Faction_TerranFederation;
            case Race::Xytomorph:  return Faction_XytomorphHive;
            case Race::Mechanical: return "";  // machines carry no innate faction
        }
        return "";
    };
    const char* own_faction = race_faction(player_.race);

    for (const auto& fi : all_factions()) {
        int starting_rep = default_faction_standing(
            own_faction, fi.name);
        if (fi.name == std::string(own_faction)) {
            starting_rep = 100;
        }
        starting_rep = std::clamp(starting_rep, -600, 600);
        player_.reputation.push_back({fi.name, starting_rep});
    }

    // Spawn
    if (!world_.map().find_open_spot_in_region(0, player_.x, player_.y, {})) {
        world_.map().find_open_spot(player_.x, player_.y);
    }

    world_.npcs().clear();
    world_.ground_items().clear();
    std::mt19937 npc_rng(static_cast<unsigned>(std::time(nullptr)) ^ 0xA7C3u);
    StationContext tha_ctx{ .is_tha = true };
    spawn_hub_npcs(world_.map(), world_.npcs(), player_.x, player_.y, npc_rng, &player_, tha_ctx);

    world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    recompute_fov();
    on_map_loaded();
    compute_camera();

    messages_.clear();
    awaiting_interact_ = false;
    combat_.reset();
    hacking_.reset();
    qh_picker_.open = false;
    qh_picker_slots_.clear();
    input_.cancel_look();
    ;
    inventory_cursor_ = 0;
    world_.current_region() = -1;
    active_widgets_ = widget_default;
    focused_widget_ = 0;
    world_.set_surface_mode(SurfaceMode::Dungeon);
    world_.overworld_x() = 0;
    world_.overworld_y() = 0;
    world_.world_tick() = 0;
    world_.day_clock() = DayClock{};
    world_.location_cache().clear();

    // Initialize ship with random name (empty — tutorial will equip it)
    player_.ship.name = generate_ship_name(world_.rng());

    log("You barely made it. Pirates hit you hard in the outer belt.");
    log("Engine destroyed, hull breached, navigation fried.");
    log("You limped into The Heavens Above on emergency thrusters.");
    log("ARIA managed the docking sequence before going into low-power mode.");
    log("You need parts. You need credits. And you need to get off this station.");
    show_welcome_ = true;
    tutorial_pending_ = true;
    check_region_change();

    // Starter gear: random ranged weapon + batteries
    Item weapon = build_by_def_id(ITEM_PLASMA_PISTOL);
    if (!player_.equipment.missile) {
        player_.equipment.missile = weapon;
    } else {
        player_.inventory.items.push_back(weapon);
    }
    log("You are armed with a " + weapon.name + ".");

    Item battery = build_by_def_id(ITEM_BATTERY);
    battery.stack_count = 3;
    player_.inventory.items.push_back(battery);

    // Generate the galaxy — lore first, then star chart, then map lore onto systems
    // Show visual progress during generation
    {
        int sw = renderer_->get_width();
        int sh = renderer_->get_height();
        std::vector<std::string> event_log;
        std::string current_phase = "Initializing...";
        int bar_progress = 0;

        auto render_progress = [&]() {
            renderer_->clear();
            int margin = 4;
            int y = margin;

            // Title
            auto put_text = [&](int x, int yi, const std::string& s, Color c) {
                for (int ci = 0; ci < static_cast<int>(s.size()) && x + ci < sw; ++ci)
                    renderer_->draw_char(x + ci, yi, s[ci], c);
            };

            put_text(margin, y, "GENERATING UNIVERSE", Color::Cyan);
            y += 2;

            // Progress bar
            int bar_w = std::min(40, sw - margin * 2 - 10);
            std::string bar = "[";
            int filled = bar_progress * bar_w / 8000;
            for (int i = 0; i < bar_w; ++i)
                bar += (i < filled) ? "\xe2\x96\x88" : "\xe2\x96\x91";
            bar += "]";
            float bya = static_cast<float>(8000 - bar_progress) / 1000.0f;
            char time_str[32];
            std::snprintf(time_str, sizeof(time_str), " %.1f Bya", bya);

            // Draw bar character by character (mix of UTF-8)
            int bx = margin;
            renderer_->draw_char(bx++, y, '[', Color::DarkGray);
            for (int i = 0; i < bar_w; ++i) {
                if (i < filled)
                    renderer_->draw_glyph(bx++, y, "\xe2\x96\x88", Color::Cyan);
                else
                    renderer_->draw_glyph(bx++, y, "\xe2\x96\x91", Color::DarkGray);
            }
            renderer_->draw_char(bx++, y, ']', Color::DarkGray);
            put_text(bx + 1, y, time_str, Color::DarkGray);
            y += 2;

            // Current phase
            put_text(margin, y, current_phase, Color::Yellow);
            y += 2;

            // Event log — show last N events that fit
            int log_space = sh - y - 2;
            int start = std::max(0, static_cast<int>(event_log.size()) - log_space);
            for (int i = start; i < static_cast<int>(event_log.size()); ++i) {
                Color c = Color::DarkGray;
                const auto& line = event_log[i];
                if (line.find("EMERGED") != std::string::npos) c = Color::Green;
                else if (line.find("COLLAPSED") != std::string::npos ||
                         line.find("TRANSCENDED") != std::string::npos) c = Color::Red;
                else if (line.find("BATTLE") != std::string::npos ||
                         line.find("WAR") != std::string::npos) c = Color::Yellow;
                else if (line.find("BEACON") != std::string::npos ||
                         line.find("MEGASTRUCTURE") != std::string::npos) c = Color::Cyan;
                else if (line.find("BREAKTHROUGH") != std::string::npos) c = Color::Magenta;

                std::string display = line;
                if (static_cast<int>(display.size()) > sw - margin * 2)
                    display = display.substr(0, sw - margin * 2);
                put_text(margin + 1, y++, display, c);
            }

            renderer_->present();
        };

        world_.lore() = GalaxySim::run(world_.seed(), [&](const SimProgress& p) {
            bar_progress = p.tick;

            if (p.phase_complete) {
                current_phase = p.phase_name;
                render_progress();
                return;
            }

            // Log significant events
            if (!p.event_text.empty()) {
                float bya_f = static_cast<float>(8000 - p.tick) / 1000.0f;
                char prefix[32];
                std::snprintf(prefix, sizeof(prefix), "%.2f Bya ", bya_f);
                std::string line = prefix + p.civ_name + ": " + p.event_text;
                event_log.push_back(line);
            }

            // Render every 50 ticks to keep it smooth but not too slow
            if (p.tick % 50 == 0 || p.phase_complete) {
                current_phase = "Simulating deep time... (" +
                    std::to_string(p.active_civs) + " active, " +
                    std::to_string(p.dead_civs) + " fallen)";
                render_progress();
            }
        });

        // Show final phase
        current_phase = "Generating star chart...";
        render_progress();
    }
    // grid_network().clear() moved to the top of new_game() so it runs
    // BEFORE on_map_loaded() registers Plan 5 LAN nodes — clearing here
    // wiped the freshly-registered Subnets.
    world_.navigation() = generate_galaxy(world_.seed());
    apply_lore_to_galaxy(world_.navigation(), world_.lore());
    assign_system_factions(world_.navigation(), world_.seed());
    world_.navigation().at_station = true;
    world_.navigation().current_body_index = -1;
    star_chart_viewer_ = StarChartViewer(&world_.navigation(), renderer_.get(), &world_, &quest_manager_);
#ifdef ASTRA_DEV_MODE
    star_chart_viewer_.set_dev_mode(dev_mode_);
#endif

    quest_manager_ = QuestManager{};  // fresh manager for new game
    quest_manager_.init_from_catalog(*this);

#ifdef ASTRA_DEV_MODE
    if (dev_mode_ &&
        quest_manager_.accept_available("story_getting_airborne", *this,
                                        world_.world_tick())) {
        quest_manager_.complete_quest("story_getting_airborne", *this,
                                      world_.world_tick());
        log("[DEV] Getting Airborne auto-completed — downstream arcs unlocked.");
    }
#endif

    apply_passive_skill_effects();
    state_ = GameState::Playing;

    event_bus_.clear();
    register_all_scenarios(*this);
}

// Plan 5 Cut 3 — RebirthSequence::apply pipeline. Mirrors the galaxy-generation
// portion of new_game(): wipe LAN graph, reseed RNG, regenerate The Heavens
// Above, run the deep-time sim + star chart, and refresh quest state. Crucially
// it does NOT touch the player struct, equipment, money, learned skills, or
// consciousness.dat — those are governed by the rebirth's "what survives"
// rules, applied by the caller before/after this call.
void Game::start_new_galaxy(unsigned fresh_seed) {
    compute_layout();

    // Wipe leftover GridNetwork before on_map_loaded() registers fresh LAN
    // nodes — same fix as new_game().
    world_.grid_network().clear();
    // Plan 5.5: wipe per-map LAN buckets too — past galaxies' LAN state is
    // not survivable across rebirth.
    world_.lan_metadatas().clear();

    world_.seed() = fresh_seed;
    world_.rng().seed(fresh_seed);

    // Bump galaxy_id so freshly registered LANs / WarpAnchorRecords carry the
    // new generation index. Past-galaxy anchors keep their old id and remain
    // visible-but-unwarpable in the deep-Grid Atlas.
    world_.set_galaxy_id(static_cast<uint16_t>(world_.galaxy_id() + 1));

    auto props = default_properties(MapType::SpaceStation);
    props.height = 80;
    world_.map() = TileMap(props.width, props.height, MapType::SpaceStation);
    auto gen = create_hub_generator();
    gen->generate(world_.map(), props, fresh_seed);
    world_.map().set_location_name("The Heavens Above");

    // Always start in the Docking Bay (region 0). Player struct is preserved.
    if (!world_.map().find_open_spot_in_region(0, player_.x, player_.y, {})) {
        world_.map().find_open_spot(player_.x, player_.y);
    }

    world_.npcs().clear();
    world_.ground_items().clear();
    std::mt19937 npc_rng(fresh_seed ^ 0xA7C3u);
    StationContext tha_ctx{ .is_tha = true };
    spawn_hub_npcs(world_.map(), world_.npcs(), player_.x, player_.y,
                   npc_rng, &player_, tha_ctx);

    world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    recompute_fov();
    on_map_loaded();
    compute_camera();

    // Reset transient runtime state — but leave messages_ alone so the rebirth
    // log line ("You wake. The galaxy is new.") survives the transition.
    awaiting_interact_ = false;
    combat_.reset();
    hacking_.reset();
    qh_picker_.open = false;
    qh_picker_slots_.clear();
    input_.cancel_look();
    inventory_cursor_ = 0;
    world_.current_region() = -1;
    active_widgets_ = widget_default;
    focused_widget_ = 0;
    world_.set_surface_mode(SurfaceMode::Dungeon);
    world_.overworld_x() = 0;
    world_.overworld_y() = 0;
    world_.world_tick() = 0;
    world_.day_clock() = DayClock{};
    world_.location_cache().clear();

    // Galaxy lore + star chart (visual progress identical to new_game()).
    {
        int sw = renderer_->get_width();
        int sh = renderer_->get_height();
        std::vector<std::string> event_log;
        std::string current_phase = "Initializing...";
        int bar_progress = 0;

        auto render_progress = [&]() {
            renderer_->clear();
            int margin = 4;
            int y = margin;
            auto put_text = [&](int x, int yi, const std::string& s, Color c) {
                for (int ci = 0; ci < static_cast<int>(s.size()) && x + ci < sw; ++ci)
                    renderer_->draw_char(x + ci, yi, s[ci], c);
            };
            put_text(margin, y, "REGENERATING UNIVERSE", Color::Cyan);
            y += 2;

            int bar_w = std::min(40, sw - margin * 2 - 10);
            int filled = bar_progress * bar_w / 8000;
            float bya = static_cast<float>(8000 - bar_progress) / 1000.0f;
            char time_str[32];
            std::snprintf(time_str, sizeof(time_str), " %.1f Bya", bya);

            int bx = margin;
            renderer_->draw_char(bx++, y, '[', Color::DarkGray);
            for (int i = 0; i < bar_w; ++i) {
                if (i < filled)
                    renderer_->draw_glyph(bx++, y, "\xe2\x96\x88", Color::Cyan);
                else
                    renderer_->draw_glyph(bx++, y, "\xe2\x96\x91", Color::DarkGray);
            }
            renderer_->draw_char(bx++, y, ']', Color::DarkGray);
            put_text(bx + 1, y, time_str, Color::DarkGray);
            y += 2;

            put_text(margin, y, current_phase, Color::Yellow);
            y += 2;

            int log_space = sh - y - 2;
            int start = std::max(0, static_cast<int>(event_log.size()) - log_space);
            for (int i = start; i < static_cast<int>(event_log.size()); ++i) {
                Color c = Color::DarkGray;
                const auto& line = event_log[i];
                if (line.find("EMERGED") != std::string::npos) c = Color::Green;
                else if (line.find("COLLAPSED") != std::string::npos ||
                         line.find("TRANSCENDED") != std::string::npos) c = Color::Red;
                else if (line.find("BATTLE") != std::string::npos ||
                         line.find("WAR") != std::string::npos) c = Color::Yellow;
                else if (line.find("BEACON") != std::string::npos ||
                         line.find("MEGASTRUCTURE") != std::string::npos) c = Color::Cyan;
                else if (line.find("BREAKTHROUGH") != std::string::npos) c = Color::Magenta;

                std::string display = line;
                if (static_cast<int>(display.size()) > sw - margin * 2)
                    display = display.substr(0, sw - margin * 2);
                put_text(margin + 1, y++, display, c);
            }
            renderer_->present();
        };

        world_.lore() = GalaxySim::run(fresh_seed, [&](const SimProgress& p) {
            bar_progress = p.tick;
            if (p.phase_complete) {
                current_phase = p.phase_name;
                render_progress();
                return;
            }
            if (!p.event_text.empty()) {
                float bya_f = static_cast<float>(8000 - p.tick) / 1000.0f;
                char prefix[32];
                std::snprintf(prefix, sizeof(prefix), "%.2f Bya ", bya_f);
                event_log.push_back(std::string(prefix) + p.civ_name + ": " + p.event_text);
            }
            if (p.tick % 50 == 0 || p.phase_complete) {
                current_phase = "Simulating deep time... (" +
                    std::to_string(p.active_civs) + " active, " +
                    std::to_string(p.dead_civs) + " fallen)";
                render_progress();
            }
        });

        current_phase = "Generating star chart...";
        render_progress();
    }

    world_.navigation() = generate_galaxy(fresh_seed);
    apply_lore_to_galaxy(world_.navigation(), world_.lore());
    assign_system_factions(world_.navigation(), fresh_seed);
    world_.navigation().at_station = true;
    world_.navigation().current_body_index = -1;
    star_chart_viewer_ = StarChartViewer(&world_.navigation(), renderer_.get(),
                                         &world_, &quest_manager_);
#ifdef ASTRA_DEV_MODE
    star_chart_viewer_.set_dev_mode(dev_mode_);
#endif

    // Quests are galaxy-scoped. Reset the manager so the new galaxy's arc DAG
    // starts fresh. (Lore archives + capstones survive via consciousness.dat.)
    quest_manager_ = QuestManager{};
    quest_manager_.init_from_catalog(*this);

#ifdef ASTRA_DEV_MODE
    if (dev_mode_ &&
        quest_manager_.accept_available("story_getting_airborne", *this,
                                        world_.world_tick())) {
        quest_manager_.complete_quest("story_getting_airborne", *this,
                                      world_.world_tick());
        log("[DEV] Getting Airborne auto-completed — downstream arcs unlocked.");
    }
#endif

    apply_passive_skill_effects();
    state_ = GameState::Playing;

    event_bus_.clear();
    register_all_scenarios(*this);

    check_region_change();
}


void Game::log(const std::string& msg) {
    messages_.push_back(":: " + msg);
    if (messages_.size() > max_messages_) {
        messages_.pop_front();
    }
    message_scroll_ = 0; // auto-scroll to latest on new message
}

bool Game::tile_occupied(int x, int y) const {
    if (player_.x == x && player_.y == y) return true;
    for (const auto& npc : world_.npcs()) {
        if (npc.alive() && npc.x == x && npc.y == y) return true;
    }
    return false;
}

void Game::check_player_death() {
    if (player_.hp <= 0) {
        save_system_.save_death(*this);
        state_ = GameState::GameOver;
    }
}

void Game::open_repair_bench() {
    repair_bench_.open(&player_, renderer_.get());
}

void Game::open_cell_picker(bool target_is_shield) {
    // Validate target exists and isn't full
    EnergyStore* target = nullptr;
    if (target_is_shield) {
        target = player_.shield_energy();
    } else if (player_.equipment.missile && player_.equipment.missile->energy) {
        target = &*player_.equipment.missile->energy;
    }

    if (!target) {
        log(target_is_shield ? "No energy shield equipped." : "No ranged weapon equipped.");
        return;
    }
    if (is_full(*target)) {
        log(target_is_shield ? "Shield is at full charge." : "Weapon is fully charged.");
        return;
    }

    // Build option list: cells with current > 0, sorted by charge descending
    struct CellEntry { int inv_idx; int charge; std::string label; };
    std::vector<CellEntry> entries;
    for (int i = 0; i < (int)player_.inventory.items.size(); ++i) {
        const auto& it = player_.inventory.items[i];
        if (it.type == ItemType::Battery && it.energy && it.energy->current > 0) {
            entries.push_back({i, it.energy->current, display_name(it)});
        }
    }
    if (entries.empty()) {
        log("No charged cells in inventory.");
        return;
    }
    std::sort(entries.begin(), entries.end(),
              [](const CellEntry& a, const CellEntry& b) { return a.charge > b.charge; });

    cell_picker_.reset();
    cell_picker_.title = target_is_shield ? "Recharge Shield from..." : "Recharge Weapon from...";
    char hk = '1';
    for (const auto& e : entries) {
        cell_picker_.add_option(hk++, e.label);
        if (hk > '9') hk = 'a';
    }
    cell_picker_.selection = 0;
    cell_picker_.open = true;
    cell_picker_target_kind_ = target_is_shield
        ? CellPickerTarget::EquippedShield
        : CellPickerTarget::EquippedWeapon;
    cell_picker_target_inv_idx_ = -1;

    cell_picker_indices_.clear();
    for (const auto& e : entries) cell_picker_indices_.push_back(e.inv_idx);
}

void Game::open_cell_picker_for_item(int inventory_index) {
    if (inventory_index < 0 || inventory_index >= (int)player_.inventory.items.size()) return;
    auto& target_item = player_.inventory.items[inventory_index];
    if (!target_item.energy) return;
    if (is_full(*target_item.energy)) {
        log(target_item.label() + " is fully charged.");
        return;
    }

    struct CellEntry { int inv_idx; int charge; std::string label; };
    std::vector<CellEntry> entries;
    for (int i = 0; i < (int)player_.inventory.items.size(); ++i) {
        if (i == inventory_index) continue;
        const auto& it = player_.inventory.items[i];
        if (it.type == ItemType::Battery && it.energy && it.energy->current > 0) {
            entries.push_back({i, it.energy->current, display_name(it)});
        }
    }
    if (entries.empty()) {
        log("No charged cells in inventory.");
        return;
    }
    std::sort(entries.begin(), entries.end(),
              [](const CellEntry& a, const CellEntry& b) { return a.charge > b.charge; });

    cell_picker_.reset();
    cell_picker_.title = "Recharge " + target_item.name + " from...";
    char hk = '1';
    for (const auto& e : entries) {
        cell_picker_.add_option(hk++, e.label);
        if (hk > '9') hk = 'a';
    }
    cell_picker_.selection = 0;
    cell_picker_.open = true;
    cell_picker_target_kind_ = CellPickerTarget::InventoryItem;
    cell_picker_target_inv_idx_ = inventory_index;
    cell_picker_indices_.clear();
    for (const auto& e : entries) cell_picker_indices_.push_back(e.inv_idx);
}

bool Game::handle_cell_picker_input(int key) {
    if (!cell_picker_.open) return false;
    auto result = cell_picker_.handle_input(key);
    if (result == MenuResult::Selected) {
        int sel = cell_picker_.selection;
        if (sel >= 0 && sel < (int)cell_picker_indices_.size()) {
            int inv_idx = cell_picker_indices_[sel];
            auto& cell = player_.inventory.items[inv_idx];

            EnergyStore* target = nullptr;
            std::string target_name;
            switch (cell_picker_target_kind_) {
                case CellPickerTarget::EquippedShield:
                    if (player_.equipment.shield) {
                        target = player_.shield_energy();
                        target_name = display_name(*player_.equipment.shield);
                    }
                    break;
                case CellPickerTarget::EquippedWeapon:
                    if (player_.equipment.missile && player_.equipment.missile->energy) {
                        target = &*player_.equipment.missile->energy;
                        target_name = display_name(*player_.equipment.missile);
                    }
                    break;
                case CellPickerTarget::InventoryItem:
                    if (cell_picker_target_inv_idx_ >= 0 &&
                        cell_picker_target_inv_idx_ < (int)player_.inventory.items.size()) {
                        auto& t = player_.inventory.items[cell_picker_target_inv_idx_];
                        if (t.energy) {
                            target = &*t.energy;
                            target_name = display_name(t);
                        }
                    }
                    break;
            }
            if (target && cell.energy) {
                int eff = 0;
                for (const auto& enh : cell.enhancements)
                    if (enh.committed) eff += enh.energy_bonus.discharge_efficiency;
                int moved = transfer_energy(*cell.energy, *target, deficit(*target), eff);
                log("Recharged " + target_name
                    + ". (+" + std::to_string(moved) + " from " + display_name(cell) + ")");

                // Fire any cell proc.
                CombatSystem::RechargeTargetKind rk = CombatSystem::RechargeTargetKind::Generic;
                if (cell_picker_target_kind_ == CellPickerTarget::EquippedShield)
                    rk = CombatSystem::RechargeTargetKind::EquippedShield;
                else if (cell_picker_target_kind_ == CellPickerTarget::EquippedWeapon)
                    rk = CombatSystem::RechargeTargetKind::EquippedWeapon;
                if (moved > 0) apply_cell_proc(cell, moved, rk, target, *this);

                advance_world(ActionCost::wait);
            }
        }
        cell_picker_.reset();
    } else if (result == MenuResult::Closed) {
        cell_picker_.reset();
    }
    return true;
}

void Game::rebuild_star_chart_viewer() {
    star_chart_viewer_ = StarChartViewer(&world_.navigation(), renderer_.get(), &world_, &quest_manager_);
#ifdef ASTRA_DEV_MODE
    star_chart_viewer_.set_dev_mode(dev_mode_);
#endif
}

void Game::reset_interaction_state() {
    awaiting_interact_ = false;
    combat_.reset();
    ;
    inventory_cursor_ = 0;
    dialog_.close();
    pause_menu_.reset();
}

void Game::post_load() {
    // Apply passive skill effects
    apply_passive_skill_effects();
    ability_bar::reconcile_from_learned(player_);
    compute_layout();
    recompute_fov();
    // NOTE: on_map_loaded() is NOT called here because LAN metadata was
    // already deserialized from the save file. Calling lan_full_reset() would
    // wipe the persisted state (cracked firewalls, looted nodes, IPs).
    // on_map_loaded() is still called for map-enters during gameplay.
    compute_camera();
    state_ = GameState::Playing;

    event_bus_.clear();
    register_all_scenarios(*this);
}

void Game::apply_passive_skill_effects() {
    if (player_has_skill(player_, SkillId::Haggle) &&
        !has_effect(player_.effects, EffectId::Haggle)) {
        add_effect(player_.effects, make_haggle_ge());
    }
    if (player_has_skill(player_, SkillId::ThickSkin) &&
        !has_effect(player_.effects, EffectId::ThickSkin)) {
        add_effect(player_.effects, make_thick_skin_ge());
    }
    if (player_has_skill(player_, SkillId::Cat_Acrobatics) &&
        !has_effect(player_.effects, EffectId::Acrobatics)) {
        add_effect(player_.effects, make_acrobatics_ge());
    }
    rebuild_auras_from_sources(player_);
}

} // namespace astra
