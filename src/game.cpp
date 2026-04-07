#include "astra/game.h"
#include "astra/boot_sequence.h"
#include "astra/debug_spawn.h"
#include "astra/item_defs.h"
#include "astra/item_gen.h"
#include "astra/journal.h"
#include "astra/galaxy_sim.h"
#include "astra/lore_generator.h"
#include "astra/biome_profile.h"
#include "astra/map_generator.h"
#include "astra/map_properties.h"
#include "astra/overworld_stamps.h"
#include "astra/npc_defs.h"
#include "astra/npc_spawner.h"
#include "astra/shop.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <ctime>

namespace astra {


// Map overworld terrain to detail/dungeon biome, falling back to planet biome for POIs

Game::Game(std::unique_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
}

void Game::run() {
    renderer_->init();
    running_ = true;
    compute_layout();

    render();

    while (running_) {
        bool needs_timeout = combat_.targeting() || input_.looking()
                           || quit_confirm_.open
                           || auto_walking_ || auto_exploring_
                           || animations_.has_any();
        int timeout_ms = (auto_walking_ || auto_exploring_) ? 50
                       : animations_.has_active_effects() ? 80
                       : animations_.has_any() ? 200
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
            input_.tick_look_blink();
            // Auto-walk/explore step
            if (auto_walking_ || auto_exploring_) {
                auto_step();
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
        update();
        render();
    }

    renderer_->shutdown();
}

void Game::compute_layout() {
    screen_w_ = renderer_->get_width();
    screen_h_ = renderer_->get_height();

    screen_rect_ = {0, 0, screen_w_, screen_h_};

    // Vertical layout: stats | HP/tabs | XP/tab-sep | main | bottom-sep | effects | abilities
    UIContext root(renderer_.get(), screen_rect_);
    auto vrows = root.rows({
        fixed(1),    // [0] stats bar
        fixed(1),    // [1] HP bar / tabs row
        fixed(1),    // [2] XP bar / tab separator row
        fill(),      // [3] main content
        fixed(1),    // [4] bottom separator
        fixed(1),    // [5] effects
        fixed(1),    // [6] abilities
    });

    stats_bar_rect_ = vrows[0].bounds();
    bottom_sep_rect_ = vrows[4].bounds();
    effects_rect_ = vrows[5].bounds();
    abilities_rect_ = vrows[6].bounds();

    // Panel width calculation
    int panel_w = screen_w_ * 35 / 100;
    if (panel_w < 30) panel_w = 30;
    if (panel_w > screen_w_ / 2) panel_w = screen_w_ / 2;

    int left_w = screen_w_ - panel_w - 1;
    int sep_x = left_w;

    // Tabs always visible in top-right
    tabs_rect_ = {sep_x + 1, vrows[1].bounds().y, panel_w, 1};

    // Bars always stop before the tab column
    hp_bar_rect_ = {0, vrows[1].bounds().y, left_w, 1};
    xp_bar_rect_ = {0, vrows[2].bounds().y, left_w, 1};

    if (panel_visible_) {
        auto main_cols = vrows[3].columns({fill(), fixed(1), fixed(panel_w)});
        map_rect_ = main_cols[0].bounds();
        separator_rect_ = {sep_x, vrows[1].bounds().y, 1, screen_h_ - 3};
        side_panel_rect_ = main_cols[2].bounds();
    } else {
        map_rect_ = vrows[3].bounds();
        separator_rect_ = {sep_x, vrows[1].bounds().y, 1, 2}; // only rows 1-2 (tabs + separator)
        side_panel_rect_ = {0, 0, 0, 0};
    }
}

// --- Input ---

void Game::handle_input(int key) {
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
    compute_camera();
    check_region_change();
}

void Game::dev_command_level_up() {
    player_.xp = player_.max_xp;
    combat_.check_level_up(*this);
}

void Game::dev_command_kill_hostiles() {
    for (auto& npc : world_.npcs()) {
        if (npc.alive() && npc.disposition == Disposition::Hostile) {
            npc.hp = 0;
        }
    }
    combat_.remove_dead_npcs(*this);
}

// Forward declare v2 generator factory
std::unique_ptr<MapGenerator> make_detail_map_generator_v2();

void Game::dev_command_biome_test(Biome biome, int layer, bool settlement) {
    (void)layer; // Phase 1: only elevation exists
    animations_.clear();
    unsigned seed = static_cast<unsigned>(std::time(nullptr));

    auto props = default_properties(MapType::DetailMap);
    props.biome = biome;
    props.width = 360;
    props.height = 150;
    props.light_bias = 100;

    if (settlement) {
        props.detail_has_poi = true;
        props.detail_poi_type = Tile::OW_Settlement;
        props.lore_tier = 1;
    }

    world_.map() = TileMap(props.width, props.height, MapType::DetailMap);
    auto gen = make_detail_map_generator_v2();
    gen->generate(world_.map(), props, seed);
    world_.map().set_biome(biome);
    std::string loc_name = "[DEV] Biome Test: " + biome_profile(biome).name;
    if (settlement) loc_name += " + Settlement";
    world_.map().set_location_name(loc_name);

    world_.map().find_open_spot(player_.x, player_.y);
    world_.npcs().clear();
    world_.ground_items().clear();

    world_.visibility() = VisibilityMap(props.width, props.height);
    recompute_fov();
    compute_camera();
    world_.current_region() = -1;
    world_.set_surface_mode(SurfaceMode::Dungeon);

    check_region_change();
}

void Game::new_game() {
    compute_layout();

    // Boot sequence for normal games, skip in dev mode
    if (!dev_mode_) {
        BootSequence boot(renderer_.get());
        boot.play();
    }

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
        add_effect(player_.effects, make_invulnerable());
        player_.name = "Dev Commander";
        player_.race = Race::Human;
        player_.player_class = PlayerClass::DevCommander;

        // Apply class template
        const auto& tmpl = class_template(player_.player_class);
        player_.attributes = tmpl.attributes;
        player_.resistances = tmpl.resistances;
        player_.max_hp += tmpl.bonus_hp;
        player_.inventory.max_carry_weight += tmpl.bonus_carry_weight;
        player_.learned_skills = tmpl.starting_skills;
        player_.skill_points = tmpl.starting_sp;
        player_.money += tmpl.starting_money;
        player_.attribute_points = 10;

        player_.max_hp = player_.effective_max_hp();
        player_.hp = player_.max_hp;

        player_.reputation.push_back({"Stellari Conclave", 10});
        player_.reputation.push_back({"Kreth Mining Guild", 0});
        player_.reputation.push_back({"Xytomorph Hive", -50});
    }
    // Always start in the Docking Bay (region 0)
    if (!world_.map().find_open_spot_in_region(0, player_.x, player_.y, {})) {
        world_.map().find_open_spot(player_.x, player_.y);
    }

    // Spawn NPCs in rooms based on room flavor
    world_.npcs().clear();
    world_.ground_items().clear();
    std::mt19937 npc_rng(static_cast<unsigned>(std::time(nullptr)) ^ 0xA7C3u);
    spawn_hub_npcs(world_.map(), world_.npcs(), player_.x, player_.y, npc_rng, &player_);

    world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    recompute_fov();
    compute_camera();

    messages_.clear();
    awaiting_interact_ = false;
    combat_.reset();
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
        player_.ship.engine = build_engine_coil_mk1();
        player_.ship.hull = build_hull_plate();
        player_.ship.navi_computer = build_navi_computer_mk2();
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
        player_.equipment.head = build_tactical_helmet();
        player_.equipment.body = build_composite_armor();
        player_.equipment.feet = build_mag_lock_boots();
        player_.equipment.left_arm = build_arm_guard();
        auto right_arm = build_arm_guard();
        right_arm.slot = EquipSlot::RightArm;
        player_.equipment.right_arm = right_arm;
        player_.equipment.right_hand = build_vibro_blade();
        player_.equipment.missile = build_ion_blaster();
        player_.equipment.face = build_recon_visor();
        player_.equipment.back = build_jetpack();
        player_.equipment.thrown = build_frag_grenade();
        player_.equipment.thrown->stack_count = 5;

        // Inventory: consumables + crafting mats + extras
        auto stack = [](Item it, int n) { it.stack_count = n; return it; };
        player_.inventory.items.push_back(stack(build_battery(), 5));
        player_.inventory.items.push_back(stack(build_ration_pack(), 10));
        player_.inventory.items.push_back(stack(build_combat_stim(), 3));
        player_.inventory.items.push_back(stack(build_nano_fiber(), 10));
        player_.inventory.items.push_back(stack(build_power_core(), 10));
        player_.inventory.items.push_back(stack(build_circuit_board(), 10));
        player_.inventory.items.push_back(stack(build_alloy_ingot(), 10));
        player_.inventory.items.push_back(build_combat_knife());
        player_.inventory.items.push_back(build_plasma_pistol());
        player_.inventory.items.push_back(stack(build_emp_grenade(), 2));

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

    Item weapon = random_ranged_weapon(world_.rng());
    if (!player_.equipment.missile) {
        player_.equipment.missile = weapon;
    } else {
        player_.inventory.items.push_back(weapon);
    }
    log("You are armed with a " + weapon.name + ".");

    Item battery = build_battery();
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
    world_.navigation() = generate_galaxy(world_.seed());
    apply_lore_to_galaxy(world_.navigation(), world_.lore());
    world_.navigation().at_station = true;
    world_.navigation().current_body_index = -1;
    star_chart_viewer_ = StarChartViewer(&world_.navigation(), renderer_.get(), &world_);

    apply_passive_skill_effects();
    state_ = GameState::Playing;
}

void Game::new_game(const CreationResult& cr) {
    compute_layout();

    // Boot sequence
    BootSequence boot(renderer_.get());
    boot.play();

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
    player_.learned_skills = tmpl.starting_skills;
    player_.skill_points = tmpl.starting_sp;
    player_.money += tmpl.starting_money;

    player_.max_hp = player_.effective_max_hp();
    player_.hp = player_.max_hp;

    // Starting reputation
    player_.reputation.push_back({"Stellari Conclave", 0});
    player_.reputation.push_back({"Kreth Mining Guild", 0});
    player_.reputation.push_back({"Xytomorph Hive", 0});

    // Spawn
    if (!world_.map().find_open_spot_in_region(0, player_.x, player_.y, {})) {
        world_.map().find_open_spot(player_.x, player_.y);
    }

    world_.npcs().clear();
    world_.ground_items().clear();
    std::mt19937 npc_rng(static_cast<unsigned>(std::time(nullptr)) ^ 0xA7C3u);
    spawn_hub_npcs(world_.map(), world_.npcs(), player_.x, player_.y, npc_rng, &player_);

    world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    recompute_fov();
    compute_camera();

    messages_.clear();
    awaiting_interact_ = false;
    combat_.reset();
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
    Item weapon = random_ranged_weapon(world_.rng());
    if (!player_.equipment.missile) {
        player_.equipment.missile = weapon;
    } else {
        player_.inventory.items.push_back(weapon);
    }
    log("You are armed with a " + weapon.name + ".");

    Item battery = build_battery();
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
    world_.navigation() = generate_galaxy(world_.seed());
    apply_lore_to_galaxy(world_.navigation(), world_.lore());
    world_.navigation().at_station = true;
    world_.navigation().current_body_index = -1;
    star_chart_viewer_ = StarChartViewer(&world_.navigation(), renderer_.get(), &world_);

    apply_passive_skill_effects();
    state_ = GameState::Playing;
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

void Game::rebuild_star_chart_viewer() {
    star_chart_viewer_ = StarChartViewer(&world_.navigation(), renderer_.get(), &world_);
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
    compute_layout();
    recompute_fov();
    compute_camera();
    state_ = GameState::Playing;
}

void Game::apply_passive_skill_effects() {
    if (player_has_skill(player_, SkillId::Haggle) &&
        !has_effect(player_.effects, EffectId::Haggle)) {
        add_effect(player_.effects, make_haggle());
    }
    if (player_has_skill(player_, SkillId::ThickSkin) &&
        !has_effect(player_.effects, EffectId::ThickSkin)) {
        add_effect(player_.effects, make_thick_skin());
    }
}

} // namespace astra
