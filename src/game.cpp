#include "astra/game.h"
#include "astra/boot_sequence.h"
#include "astra/debug_spawn.h"
#include "astra/item_defs.h"
#include "astra/item_gen.h"
#include "astra/journal.h"
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

static int sign(int v) { return (v > 0) - (v < 0); }

// Map overworld terrain to detail/dungeon biome, falling back to planet biome for POIs
static Biome detail_biome_for_terrain(Tile terrain, Biome planet_biome) {
    switch (terrain) {
        case Tile::OW_Forest:    return Biome::Forest;
        case Tile::OW_Plains:    return Biome::Grassland;
        case Tile::OW_Desert:    return Biome::Sandy;
        case Tile::OW_IceField:  return Biome::Ice;
        case Tile::OW_LavaFlow:  return Biome::Volcanic;
        case Tile::OW_Swamp:     return Biome::Aquatic;
        case Tile::OW_Fungal:    return Biome::Fungal;
        case Tile::OW_Mountains: return Biome::Rocky;
        case Tile::OW_Crater:    return Biome::Rocky;
        case Tile::OW_River:     return Biome::Aquatic;
        case Tile::OW_Lake:      return Biome::Aquatic;
        default:                 return planet_biome;
    }
}

static int chebyshev_dist(int x1, int y1, int x2, int y2) {
    return std::max(std::abs(x1 - x2), std::abs(y1 - y2));
}

Game::Game(std::unique_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
}

void Game::run() {
    renderer_->init();
    running_ = true;
    compute_layout();

    render();

    while (running_) {
        int key = (targeting_ || input_.looking() || quit_confirm_.is_open())
                      ? renderer_->wait_input_timeout(300)
                      : renderer_->wait_input();

        // Check for Ctrl+C quit request (signal fires during read, returning -1)
        if (renderer_->consume_quit_request()) {
            if (!quit_confirm_.is_open()) {
                quit_confirm_.close();
                quit_confirm_.set_title("Quit without saving?");
                quit_confirm_.add_option('y', "Yes, quit");
                quit_confirm_.add_option('n', "No, keep playing");
                quit_confirm_.open();
            }
            // Skip normal input handling — fall through to render
        } else if (key == -1) {
            // Timeout — toggle blink phase for reticule
            ++blink_phase_;
            input_.tick_look_blink();
        } else {
            handle_input(key);
        }

        int w = renderer_->get_width();
        int h = renderer_->get_height();
        if (w != screen_w_ || h != screen_h_) {
            compute_layout();
            if (state_ == GameState::Playing) {
                compute_camera();
            }
        }

        update();
        render();
    }

    renderer_->shutdown();
}

void Game::compute_layout() {
    screen_w_ = renderer_->get_width();
    screen_h_ = renderer_->get_height();

    screen_rect_ = {0, 0, screen_w_, screen_h_};

    int panel_w = screen_w_ * 35 / 100;
    if (panel_w < 30) panel_w = 30;
    if (panel_w > screen_w_ / 2) panel_w = screen_w_ / 2;

    int left_w = screen_w_ - panel_w - 1;
    int sep_x = left_w;
    int main_h = screen_h_ - 6; // 1 stats + 2 bars + 1 separator + 1 effects + 1 abilities

    // Tabs always visible in top-right
    tabs_rect_ = {sep_x + 1, 1, panel_w, 1};

    // Bars always stop before the tab column
    hp_bar_rect_ = {0, 1, left_w, 1};
    xp_bar_rect_ = {0, 2, left_w, 1};

    if (panel_visible_) {
        map_rect_ = {0, 3, left_w, main_h};
        separator_rect_ = {sep_x, 1, 1, screen_h_ - 3};
        side_panel_rect_ = {sep_x + 1, 3, panel_w, main_h};
    } else {
        map_rect_ = {0, 3, screen_w_, main_h};
        separator_rect_ = {sep_x, 1, 1, 2}; // only rows 1-2 (tabs + separator)
        side_panel_rect_ = {0, 0, 0, 0};
    }

    // Row 1: stats bar (full width)
    stats_bar_rect_ = {0, 0, screen_w_, 1};

    // Bottom: separator + effects + abilities
    bottom_sep_rect_ = {0, screen_h_ - 3, screen_w_, 1};
    effects_rect_ = {0, screen_h_ - 2, screen_w_, 1};
    abilities_rect_ = {0, screen_h_ - 1, screen_w_, 1};
}

// --- Input ---

void Game::handle_input(int key) {
    switch (state_) {
        case GameState::MainMenu:
            // Quit confirm intercepts on menu too
            if (quit_confirm_.is_open()) {
                auto qr = quit_confirm_.handle_input(key);
                if (qr == MenuResult::Selected && quit_confirm_.selected_key() == 'y') {
                    running_ = false;
                } else if (qr == MenuResult::Selected || qr == MenuResult::Closed) {
                    quit_confirm_.close();
                }
                break;
            }
            handle_menu_input(key);
            break;
        case GameState::Playing:
            // Quit confirm takes priority
            if (quit_confirm_.is_open()) {
                auto qr = quit_confirm_.handle_input(key);
                if (qr == MenuResult::Selected && quit_confirm_.selected_key() == 'y') {
                    running_ = false;
                } else if (qr == MenuResult::Selected || qr == MenuResult::Closed) {
                    quit_confirm_.close();
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
            } else if (menu_selection_ == menu_item_count_ - 1) {
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
        spawn_settlement_npcs(world_.map(), world_.npcs(), player_.x, player_.y, npc_rng);
    } else if (dev_warp_stamp_test_poi_ == Tile::OW_Outpost) {
        spawn_outpost_npcs(world_.map(), world_.npcs(), player_.x, player_.y, npc_rng);
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

void Game::dev_command_level_up() {
    player_.xp = player_.max_xp;
    check_level_up();
}

void Game::dev_command_kill_hostiles() {
    for (auto& npc : world_.npcs()) {
        if (npc.alive() && npc.disposition == Disposition::Hostile) {
            npc.hp = 0;
        }
    }
    remove_dead_npcs();
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
    player_.money = 10;
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
    spawn_hub_npcs(world_.map(), world_.npcs(), player_.x, player_.y, npc_rng);

    world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    recompute_fov();
    compute_camera();

    messages_.clear();
    awaiting_interact_ = false;
    targeting_ = false;
    input_.cancel_look();
    target_npc_ = nullptr;
    inventory_cursor_ = 0;
    inspecting_item_ = false;
    world_.current_region() = -1;
    active_tab_ = 0; // Start on Messages tab
    world_.set_surface_mode(SurfaceMode::Dungeon);
    world_.overworld_x() = 0;
    world_.overworld_y() = 0;
    world_.world_tick() = 0;
    world_.day_clock() = DayClock{};  // station day = 200 ticks
    world_.location_cache().clear();
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

    // Generate the galaxy
    world_.navigation() = generate_galaxy(world_.seed());
    world_.navigation().at_station = true;
    world_.navigation().current_body_index = -1;
    star_chart_viewer_ = StarChartViewer(&world_.navigation(), renderer_.get());

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
    player_.money = 10;

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
    spawn_hub_npcs(world_.map(), world_.npcs(), player_.x, player_.y, npc_rng);

    world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    recompute_fov();
    compute_camera();

    messages_.clear();
    awaiting_interact_ = false;
    targeting_ = false;
    input_.cancel_look();
    target_npc_ = nullptr;
    inventory_cursor_ = 0;
    inspecting_item_ = false;
    world_.current_region() = -1;
    active_tab_ = 0;
    world_.set_surface_mode(SurfaceMode::Dungeon);
    world_.overworld_x() = 0;
    world_.overworld_y() = 0;
    world_.world_tick() = 0;
    world_.day_clock() = DayClock{};
    world_.location_cache().clear();

    log("Welcome aboard, " + cr.name + ". Your journey to Sgr A* begins.");
    log("You are docked at The Heavens Above, the space station orbiting Jupiter.");
    show_welcome_ = true;
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

    // Generate the galaxy
    world_.navigation() = generate_galaxy(world_.seed());
    world_.navigation().at_station = true;
    world_.navigation().current_body_index = -1;
    star_chart_viewer_ = StarChartViewer(&world_.navigation(), renderer_.get());

    state_ = GameState::Playing;
}

void Game::save_current_location() {
    LocationKey key;
    if (world_.navigation().on_ship) {
        key = WorldManager::ship_key;
    } else if (world_.navigation().at_station) {
        key = {world_.navigation().current_system_id, -1, -1, true, -1, -1, 0};
    } else if (world_.on_overworld()) {
        key = {world_.navigation().current_system_id, world_.navigation().current_body_index,
               world_.navigation().current_moon_index, false, -1, -1, 0};
    } else if (world_.on_detail_map()) {
        key = {world_.navigation().current_system_id, world_.navigation().current_body_index,
               world_.navigation().current_moon_index, false, world_.overworld_x(), world_.overworld_y(), 0};
    } else {
        // Dungeon (depth=1)
        key = {world_.navigation().current_system_id, world_.navigation().current_body_index,
               world_.navigation().current_moon_index, false, world_.overworld_x(), world_.overworld_y(), 1};
    }
    LocationState& state = world_.location_cache()[key];
    state.map = std::move(world_.map());
    state.visibility = std::move(world_.visibility());
    state.npcs = std::move(world_.npcs());
    state.ground_items = std::move(world_.ground_items());
    state.player_x = player_.x;
    state.player_y = player_.y;
}

void Game::restore_location(const LocationKey& key) {
    auto it = world_.location_cache().find(key);
    if (it == world_.location_cache().end()) return;
    LocationState& state = it->second;
    world_.map() = std::move(state.map);
    world_.visibility() = std::move(state.visibility);
    world_.npcs() = std::move(state.npcs);
    world_.ground_items() = std::move(state.ground_items);

    if (key == WorldManager::ship_key) {
        // Restore cached position on the ship
        player_.x = state.player_x;
        player_.y = state.player_y;
    } else {
        // Spawn at the map's starting area (docking bay / entrance), not cached position
        std::vector<std::pair<int,int>> exclude;
        for (const auto& npc : world_.npcs()) {
            exclude.push_back({npc.x, npc.y});
        }
        if (!world_.map().find_open_spot_in_region(0, player_.x, player_.y, exclude)) {
            world_.map().find_open_spot(player_.x, player_.y);
        }
    }

    world_.location_cache().erase(it);
}

void Game::enter_ship() {
    save_current_location();
    world_.navigation().on_ship = true;
    world_.set_surface_mode(SurfaceMode::Dungeon);

    if (world_.location_cache().count(WorldManager::ship_key)) {
        restore_location(WorldManager::ship_key);
    } else {
        // Generate the ship for the first time
        unsigned ship_seed = world_.seed() ^ 0x5B1Bu;
        auto props = default_properties(MapType::Starship);
        world_.map() = TileMap(props.width, props.height, MapType::Starship);
        auto gen = create_starship_generator();
        gen->generate(world_.map(), props, ship_seed);
        world_.map().set_location_name("Your Starship");

        world_.npcs().clear();
        world_.ground_items().clear();
        // Spawn in region 0 (cockpit)
        if (!world_.map().find_open_spot_in_region(0, player_.x, player_.y, {})) {
            world_.map().find_open_spot(player_.x, player_.y);
        }

        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    world_.visibility().reveal_all();
    world_.current_region() = -1;
    compute_camera();
    check_region_change();
    log("You board your starship.");
}

void Game::enter_overworld_tile() {
    Tile tile = world_.map().get(player_.x, player_.y);
    world_.overworld_x() = player_.x;
    world_.overworld_y() = player_.y;

    // Determine detail map type + biome from overworld tile
    MapType detail_type = MapType::Rocky;
    Biome detail_biome = detail_biome_for_terrain(tile, world_.map().biome());

    const char* enter_msg = "You explore the area.";

    switch (tile) {
        case Tile::OW_CaveEntrance:
            detail_type = MapType::Rocky;
            enter_msg = "You descend into the cavern.";
            break;
        case Tile::OW_Ruins:
            detail_type = MapType::DerelictStation;
            detail_biome = Biome::Corroded;
            enter_msg = "You enter the ancient ruins.";
            break;
        case Tile::OW_Settlement:
            detail_type = MapType::SpaceStation;
            detail_biome = Biome::Station;
            enter_msg = "You enter the small settlement.";
            break;
        case Tile::OW_CrashedShip:
            detail_type = MapType::DerelictStation;
            detail_biome = Biome::Station;
            enter_msg = "You climb into the wrecked hull.";
            break;
        case Tile::OW_Outpost:
            detail_type = MapType::SpaceStation;
            detail_biome = Biome::Station;
            enter_msg = "You enter the abandoned outpost.";
            break;
        case Tile::OW_Plains:
        case Tile::OW_Desert:
            detail_type = MapType::Rocky;
            enter_msg = "You survey the terrain closely.";
            break;
        case Tile::OW_IceField:
            detail_type = MapType::Rocky;
            enter_msg = "You traverse the frozen landscape.";
            break;
        case Tile::OW_LavaFlow:
            detail_type = MapType::Lava;
            enter_msg = "You carefully navigate the volcanic terrain.";
            break;
        case Tile::OW_Fungal:
            detail_type = MapType::Rocky;
            enter_msg = "You push through the alien growth.";
            break;
        case Tile::OW_Crater:
            detail_type = MapType::Asteroid;
            enter_msg = "You descend into the impact crater.";
            break;
        case Tile::OW_Forest:
            detail_type = MapType::Rocky;
            enter_msg = "You push into the dense forest.";
            break;
        case Tile::OW_River:
            detail_type = MapType::Rocky;
            enter_msg = "You ford the rushing river.";
            break;
        case Tile::OW_Swamp:
            detail_type = MapType::Rocky;
            enter_msg = "You wade into the murky swamp.";
            break;
        default:
            return; // OW_Landing, OW_Mountains, OW_Lake — can't enter
    }

    // Remember the body name for the detail map
    std::string body_name = world_.map().location_name();

    // Detail map LocationKey uses overworld coords (depth=1 for legacy dungeon entry)
    LocationKey detail_key = {world_.navigation().current_system_id,
                              world_.navigation().current_body_index,
                              world_.navigation().current_moon_index,
                              false, world_.overworld_x(), world_.overworld_y(), 1};

    // Save overworld before entering detail
    save_current_location();
    world_.set_surface_mode(SurfaceMode::Dungeon);

    if (world_.location_cache().count(detail_key)) {
        restore_location(detail_key);
    } else {
        // Generate detail map
        unsigned detail_seed = world_.seed()
            ^ (world_.navigation().current_system_id * 7919u)
            ^ (static_cast<unsigned>(world_.navigation().current_body_index) * 6271u)
            ^ (static_cast<unsigned>(world_.navigation().current_moon_index + 1) * 3571u)
            ^ (static_cast<unsigned>(world_.overworld_x()) * 1013u)
            ^ (static_cast<unsigned>(world_.overworld_y()) * 2039u);

        auto props = default_properties(detail_type);
        props.biome = detail_biome;
        world_.map() = TileMap(props.width, props.height, detail_type);

        // Smaller maps for minor POIs
        if (tile == Tile::OW_CrashedShip || tile == Tile::OW_Outpost) {
            props.room_count_min = 3;
            props.room_count_max = 5;
        }

        auto gen = create_generator(detail_type);
        gen->generate(world_.map(), props, detail_seed);
        world_.map().set_location_name(body_name);

        world_.npcs().clear();
        world_.ground_items().clear();
        world_.map().find_open_spot(player_.x, player_.y);

        // Spawn NPCs
        std::mt19937 npc_rng(detail_seed ^ 0xD3ADu);
        std::vector<std::pair<int,int>> occupied = {{player_.x, player_.y}};
        debug_spawn(world_.map(), world_.npcs(), player_.x, player_.y, occupied, npc_rng);

        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    world_.current_region() = -1;
    recompute_fov();
    compute_camera();
    check_region_change();
    log(enter_msg);
}

void Game::exit_to_overworld() {
    // Save detail map
    save_current_location();

    // Restore overworld
    LocationKey ow_key = {world_.navigation().current_system_id,
                          world_.navigation().current_body_index,
                          world_.navigation().current_moon_index,
                          false, -1, -1, 0};

    if (world_.location_cache().count(ow_key)) {
        restore_location(ow_key);
    }

    player_.x = world_.overworld_x();
    player_.y = world_.overworld_y();
    world_.set_surface_mode(SurfaceMode::Overworld);
    world_.visibility().reveal_all();
    world_.current_region() = -1;
    compute_camera();
    log("You return to the surface.");
}

MapProperties Game::build_detail_props(int ow_x, int ow_y) {
    auto props = default_properties(MapType::DetailMap);

    // Read overworld from cache
    LocationKey ow_key = {world_.navigation().current_system_id,
                          world_.navigation().current_body_index,
                          world_.navigation().current_moon_index,
                          false, -1, -1, 0};

    const TileMap* ow_map = nullptr;
    if (world_.on_overworld()) {
        ow_map = &world_.map();
    } else {
        auto it = world_.location_cache().find(ow_key);
        if (it != world_.location_cache().end())
            ow_map = &it->second.map;
    }
    if (!ow_map) return props;

    props.detail_terrain = ow_map->get(ow_x, ow_y);
    props.biome = detail_biome_for_terrain(props.detail_terrain, ow_map->biome());

    // Sample neighbors
    if (ow_y > 0) props.detail_neighbor_n = ow_map->get(ow_x, ow_y - 1);
    if (ow_y < ow_map->height() - 1) props.detail_neighbor_s = ow_map->get(ow_x, ow_y + 1);
    if (ow_x > 0) props.detail_neighbor_w = ow_map->get(ow_x - 1, ow_y);
    if (ow_x < ow_map->width() - 1) props.detail_neighbor_e = ow_map->get(ow_x + 1, ow_y);

    // Check for POI
    Tile t = props.detail_terrain;
    if (t == Tile::OW_CaveEntrance || t == Tile::OW_Settlement ||
        t == Tile::OW_Ruins || t == Tile::OW_CrashedShip ||
        t == Tile::OW_Outpost || t == Tile::OW_Landing) {
        props.detail_has_poi = true;
        props.detail_poi_type = t;
    }

    return props;
}

void Game::enter_detail_map() {
    world_.overworld_x() = player_.x;
    world_.overworld_y() = player_.y;

    auto props = build_detail_props(world_.overworld_x(), world_.overworld_y());
    std::string body_name = world_.map().location_name();

    LocationKey detail_key = {world_.navigation().current_system_id,
                              world_.navigation().current_body_index,
                              world_.navigation().current_moon_index,
                              false, world_.overworld_x(), world_.overworld_y(), 0};

    save_current_location();
    world_.set_surface_mode(SurfaceMode::DetailMap);

    if (world_.location_cache().count(detail_key)) {
        restore_location(detail_key);
    } else {
        unsigned detail_seed = world_.seed()
            ^ (world_.navigation().current_system_id * 7919u)
            ^ (static_cast<unsigned>(world_.navigation().current_body_index) * 6271u)
            ^ (static_cast<unsigned>(world_.navigation().current_moon_index + 1) * 3571u)
            ^ (static_cast<unsigned>(world_.overworld_x()) * 1013u)
            ^ (static_cast<unsigned>(world_.overworld_y()) * 2039u);

        world_.map() = TileMap(props.width, props.height, MapType::DetailMap);
        auto gen = create_generator(MapType::DetailMap);
        gen->generate(world_.map(), props, detail_seed);
        world_.map().set_biome(props.biome);
        world_.map().set_location_name(body_name);

        world_.npcs().clear();
        world_.ground_items().clear();

        // Place player: spawn in cockpit for landing pad, center otherwise
        if (props.detail_poi_type == Tile::OW_Landing) {
            // Find the cockpit region (ShipCockpit flavor)
            bool found_cockpit = false;
            for (int i = 0; i < world_.map().region_count(); ++i) {
                if (world_.map().region(i).flavor == RoomFlavor::ShipCockpit) {
                    if (world_.map().find_open_spot_in_region(i, player_.x, player_.y, {})) {
                        found_cockpit = true;
                    }
                    break;
                }
            }
            if (!found_cockpit) {
                player_.x = world_.map().width() / 2;
                player_.y = world_.map().height() / 2;
                if (!world_.map().passable(player_.x, player_.y))
                    world_.map().find_open_spot(player_.x, player_.y);
            }
        } else {
            player_.x = world_.map().width() / 2;
            player_.y = world_.map().height() / 2;
            if (!world_.map().passable(player_.x, player_.y))
                world_.map().find_open_spot(player_.x, player_.y);
        }

        // Spawn NPCs in settlements and outposts (after player placement)
        std::mt19937 npc_rng(detail_seed ^ 0xC1A5u);
        if (props.detail_poi_type == Tile::OW_Settlement) {
            spawn_settlement_npcs(world_.map(), world_.npcs(), player_.x, player_.y, npc_rng);
        } else if (props.detail_poi_type == Tile::OW_Outpost) {
            spawn_outpost_npcs(world_.map(), world_.npcs(), player_.x, player_.y, npc_rng);
        }

        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    world_.current_region() = -1;
    recompute_fov();
    compute_camera();
    check_region_change();

    // Terrain-specific entry message
    const char* msg = "You explore the area.";
    switch (props.detail_terrain) {
        case Tile::OW_CaveEntrance: msg = "A cave entrance looms before you."; break;
        case Tile::OW_Settlement:   msg = "You enter the settlement area."; break;
        case Tile::OW_Ruins:        msg = "Ancient ruins surround you."; break;
        case Tile::OW_CrashedShip:  msg = "Wreckage of a starship lies scattered."; break;
        case Tile::OW_Landing:      msg = "You stand in your ship's cockpit. The landing pad stretches outside."; break;
        case Tile::OW_Outpost:      msg = "You approach the outpost."; break;
        case Tile::OW_Forest:       msg = "Dense forest surrounds you."; break;
        case Tile::OW_Desert:       msg = "Sand stretches in every direction."; break;
        case Tile::OW_IceField:     msg = "Ice formations glitter around you."; break;
        case Tile::OW_LavaFlow:     msg = "Heat radiates from the volcanic ground."; break;
        case Tile::OW_Swamp:        msg = "You wade into murky terrain."; break;
        case Tile::OW_Crater:       msg = "You descend into the impact crater."; break;
        case Tile::OW_Fungal:       msg = "Alien growths crowd around you."; break;
        case Tile::OW_Plains:       msg = "Open terrain stretches before you."; break;
        default: break;
    }
    log(msg);
}

void Game::exit_detail_to_overworld() {
    save_current_location();

    LocationKey ow_key = {world_.navigation().current_system_id,
                          world_.navigation().current_body_index,
                          world_.navigation().current_moon_index,
                          false, -1, -1, 0};

    if (world_.location_cache().count(ow_key)) {
        restore_location(ow_key);
    }

    player_.x = world_.overworld_x();
    player_.y = world_.overworld_y();
    world_.set_surface_mode(SurfaceMode::Overworld);
    world_.visibility().reveal_all();
    world_.current_region() = -1;
    compute_camera();
    log("You return to the surface view.");
}

void Game::enter_dungeon_from_detail() {
    // Save detail map (depth=0)
    save_current_location();

    // Determine dungeon type from overworld tile
    LocationKey ow_key = {world_.navigation().current_system_id,
                          world_.navigation().current_body_index,
                          world_.navigation().current_moon_index,
                          false, -1, -1, 0};

    Tile ow_tile = Tile::OW_CaveEntrance;
    Biome ow_biome = world_.map().biome();
    auto it = world_.location_cache().find(ow_key);
    if (it != world_.location_cache().end()) {
        ow_tile = it->second.map.get(world_.overworld_x(), world_.overworld_y());
        ow_biome = it->second.map.biome();
    }

    // Map overworld tile to dungeon type + biome
    MapType detail_type = MapType::Rocky;
    Biome detail_biome = detail_biome_for_terrain(ow_tile, ow_biome);
    const char* enter_msg = "You descend deeper.";

    switch (ow_tile) {
        case Tile::OW_CaveEntrance:
            detail_type = MapType::Rocky;
            enter_msg = "You descend into the cavern.";
            break;
        case Tile::OW_Ruins:
            detail_type = MapType::DerelictStation;
            detail_biome = Biome::Corroded;
            enter_msg = "You enter the ancient ruins.";
            break;
        case Tile::OW_Settlement:
            detail_type = MapType::SpaceStation;
            detail_biome = Biome::Station;
            enter_msg = "You enter the settlement interior.";
            break;
        case Tile::OW_CrashedShip:
            detail_type = MapType::DerelictStation;
            detail_biome = Biome::Station;
            enter_msg = "You climb into the wrecked hull.";
            break;
        case Tile::OW_Outpost:
            detail_type = MapType::SpaceStation;
            detail_biome = Biome::Station;
            enter_msg = "You enter the outpost interior.";
            break;
        default:
            detail_type = MapType::Rocky;
            enter_msg = "You descend underground.";
            break;
    }

    std::string body_name = world_.map().location_name();

    LocationKey dungeon_key = {world_.navigation().current_system_id,
                               world_.navigation().current_body_index,
                               world_.navigation().current_moon_index,
                               false, world_.overworld_x(), world_.overworld_y(), 1};

    world_.set_surface_mode(SurfaceMode::Dungeon);

    if (world_.location_cache().count(dungeon_key)) {
        restore_location(dungeon_key);
    } else {
        unsigned detail_seed = world_.seed()
            ^ (world_.navigation().current_system_id * 7919u)
            ^ (static_cast<unsigned>(world_.navigation().current_body_index) * 6271u)
            ^ (static_cast<unsigned>(world_.navigation().current_moon_index + 1) * 3571u)
            ^ (static_cast<unsigned>(world_.overworld_x()) * 1013u)
            ^ (static_cast<unsigned>(world_.overworld_y()) * 2039u)
            ^ 0xDE3Du;

        auto props = default_properties(detail_type);
        props.biome = detail_biome;
        world_.map() = TileMap(props.width, props.height, detail_type);
        auto gen = create_generator(detail_type);
        gen->generate(world_.map(), props, detail_seed);
        world_.map().set_location_name(body_name);

        world_.npcs().clear();
        world_.ground_items().clear();
        world_.map().find_open_spot(player_.x, player_.y);

        std::mt19937 npc_rng(detail_seed ^ 0xD3ADu);
        std::vector<std::pair<int,int>> occupied = {{player_.x, player_.y}};
        debug_spawn(world_.map(), world_.npcs(), player_.x, player_.y, occupied, npc_rng);

        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    world_.current_region() = -1;
    recompute_fov();
    compute_camera();
    check_region_change();
    log(enter_msg);
}

void Game::exit_dungeon_to_detail() {
    // Save dungeon (depth=1)
    save_current_location();

    // Restore detail map (depth=0)
    LocationKey detail_key = {world_.navigation().current_system_id,
                              world_.navigation().current_body_index,
                              world_.navigation().current_moon_index,
                              false, world_.overworld_x(), world_.overworld_y(), 0};

    world_.set_surface_mode(SurfaceMode::DetailMap);

    if (world_.location_cache().count(detail_key)) {
        restore_location(detail_key);
    } else {
        // Detail map was never cached — generate it
        auto props = build_detail_props(world_.overworld_x(), world_.overworld_y());
        unsigned detail_seed = world_.seed()
            ^ (world_.navigation().current_system_id * 7919u)
            ^ (static_cast<unsigned>(world_.navigation().current_body_index) * 6271u)
            ^ (static_cast<unsigned>(world_.navigation().current_moon_index + 1) * 3571u)
            ^ (static_cast<unsigned>(world_.overworld_x()) * 1013u)
            ^ (static_cast<unsigned>(world_.overworld_y()) * 2039u);

        world_.map() = TileMap(props.width, props.height, MapType::DetailMap);
        auto gen = create_generator(MapType::DetailMap);
        gen->generate(world_.map(), props, detail_seed);
        world_.map().set_biome(props.biome);

        world_.npcs().clear();
        world_.ground_items().clear();
        world_.map().find_open_spot(player_.x, player_.y);
        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    world_.current_region() = -1;
    recompute_fov();
    compute_camera();
    check_region_change();
    log("You return to the surface.");
}

void Game::transition_detail_edge(int dx, int dy) {
    // Find the overworld map from cache
    LocationKey ow_key = {world_.navigation().current_system_id,
                          world_.navigation().current_body_index,
                          world_.navigation().current_moon_index,
                          false, -1, -1, 0};

    auto ow_it = world_.location_cache().find(ow_key);
    if (ow_it == world_.location_cache().end()) {
        log("You can't go that way.");
        return;
    }

    const auto& ow_map = ow_it->second.map;
    int new_ow_x = world_.overworld_x() + dx;
    int new_ow_y = world_.overworld_y() + dy;

    // Bounds check
    if (new_ow_x < 0 || new_ow_x >= ow_map.width() ||
        new_ow_y < 0 || new_ow_y >= ow_map.height()) {
        log("You've reached the edge of this region.");
        return;
    }

    // Check passability on overworld
    Tile dest_tile = ow_map.get(new_ow_x, new_ow_y);
    if (dest_tile == Tile::OW_Mountains || dest_tile == Tile::OW_Lake) {
        log("Impassable terrain blocks your path.");
        return;
    }

    // Save current detail map
    save_current_location();

    // Update overworld position
    world_.overworld_x() = new_ow_x;
    world_.overworld_y() = new_ow_y;

    // Generate or restore the new detail map
    auto props = build_detail_props(new_ow_x, new_ow_y);

    LocationKey new_detail_key = {world_.navigation().current_system_id,
                                  world_.navigation().current_body_index,
                                  world_.navigation().current_moon_index,
                                  false, new_ow_x, new_ow_y, 0};

    if (world_.location_cache().count(new_detail_key)) {
        restore_location(new_detail_key);
    } else {
        unsigned detail_seed = world_.seed()
            ^ (world_.navigation().current_system_id * 7919u)
            ^ (static_cast<unsigned>(world_.navigation().current_body_index) * 6271u)
            ^ (static_cast<unsigned>(world_.navigation().current_moon_index + 1) * 3571u)
            ^ (static_cast<unsigned>(new_ow_x) * 1013u)
            ^ (static_cast<unsigned>(new_ow_y) * 2039u);

        world_.map() = TileMap(props.width, props.height, MapType::DetailMap);
        auto gen = create_generator(MapType::DetailMap);
        gen->generate(world_.map(), props, detail_seed);
        world_.map().set_biome(props.biome);

        world_.npcs().clear();
        world_.ground_items().clear();
        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    // Place player at opposite edge
    if (dx == -1) player_.x = world_.map().width() - 2;
    else if (dx == 1) player_.x = 1;
    else player_.x = world_.map().width() / 2;

    if (dy == -1) player_.y = world_.map().height() - 2;
    else if (dy == 1) player_.y = 1;
    else player_.y = world_.map().height() / 2;

    // Ensure we're on a passable tile
    if (!world_.map().passable(player_.x, player_.y)) {
        world_.map().find_open_spot(player_.x, player_.y);
    }

    // Also update player position on cached overworld
    ow_it = world_.location_cache().find(ow_key);
    if (ow_it != world_.location_cache().end()) {
        ow_it->second.player_x = new_ow_x;
        ow_it->second.player_y = new_ow_y;
    }

    world_.current_region() = -1;
    recompute_fov();
    compute_camera();
    check_region_change();
    advance_world(15);
}

void Game::travel_to_destination(const ChartAction& action) {
    if (action.system_index < 0 ||
        action.system_index >= static_cast<int>(world_.navigation().systems.size()))
        return;

    auto& target_sys = world_.navigation().systems[action.system_index];
    generate_system_bodies(target_sys);

    // Determine destination key and metadata
    LocationKey dest_key;
    MapType dest_type = MapType::SpaceStation;
    Biome dest_biome = Biome::Station;
    std::string location_name;

    switch (action.type) {
        case ChartActionType::WarpToSystem: {
            // Warp puts you on your ship in the new system
            save_current_location();
            world_.navigation().current_system_id = target_sys.id;
            discover_nearby(world_.navigation(), target_sys.id, 20.0f);
            world_.navigation().on_ship = true;
            world_.navigation().at_station = false;
            world_.navigation().current_body_index = -1;
            world_.navigation().current_moon_index = -1;

            if (world_.location_cache().count(WorldManager::ship_key)) {
                restore_location(WorldManager::ship_key);
            } else {
                // Generate ship for the first time
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
            log("Warp drive engaged...");
            log("You arrive at " + colored(target_sys.name, Color::Yellow)
                + ". Open the star chart to navigate.");
            return;
        }
        case ChartActionType::TravelToStation: {
            dest_key = {target_sys.id, -1, -1, true, -1, -1, 0};
            dest_type = target_sys.station.derelict ? MapType::DerelictStation
                                                    : MapType::SpaceStation;
            dest_biome = Biome::Station;
            location_name = target_sys.station.name;
            break;
        }
        case ChartActionType::TravelToBody: {
            if (action.body_index < 0 ||
                action.body_index >= static_cast<int>(target_sys.bodies.size()))
                return;

            dest_key = {target_sys.id, action.body_index, action.moon_index, false, -1, -1, 0};
            const auto& body = target_sys.bodies[action.body_index];

            // Map body type to map type
            switch (body.type) {
                case BodyType::Rocky:
                case BodyType::Terrestrial:
                    dest_type = MapType::Rocky;
                    break;
                case BodyType::DwarfPlanet:
                case BodyType::AsteroidBelt:
                    dest_type = MapType::Asteroid;
                    break;
                default:
                    dest_type = MapType::Rocky;
                    break;
            }

            // Determine biome from body or moon properties
            unsigned biome_seed = world_.seed() ^ (target_sys.id * 997u)
                                ^ (static_cast<unsigned>(action.body_index) * 6271u);

            if (action.moon_index >= 0) {
                // Generate independent moon body
                unsigned moon_seed = world_.seed() ^ (target_sys.id * 7919u)
                                   ^ (static_cast<unsigned>(action.body_index) * 6271u);
                auto moon = generate_moon_body(body, action.moon_index, moon_seed);
                biome_seed ^= (static_cast<unsigned>(action.moon_index) * 4219u);
                dest_biome = determine_biome(moon.type, moon.atmosphere, moon.temperature, biome_seed);
                location_name = moon.name;
            } else {
                dest_biome = determine_biome(body.type, body.atmosphere, body.temperature, biome_seed);
                location_name = body.name;
            }
            break;
        }
        default:
            return;
    }

    // Save current location before leaving
    save_current_location();

    // Update navigation state
    switch (action.type) {
        case ChartActionType::TravelToStation:
            world_.navigation().on_ship = false;
            world_.navigation().at_station = true;
            world_.navigation().current_body_index = -1;
            world_.navigation().current_moon_index = -1;
            world_.day_clock().set_body_day_length(200); // station standard day
            break;
        case ChartActionType::TravelToBody: {
            world_.navigation().on_ship = false;
            world_.navigation().at_station = false;
            world_.navigation().current_body_index = action.body_index;
            world_.navigation().current_moon_index = action.moon_index;
            const auto& body = target_sys.bodies[action.body_index];
            if (action.moon_index >= 0) {
                unsigned moon_seed = world_.seed() ^ (target_sys.id * 7919u)
                                   ^ (static_cast<unsigned>(action.body_index) * 6271u);
                auto moon = generate_moon_body(body, action.moon_index, moon_seed);
                world_.day_clock().set_body_day_length(moon.day_length);
            } else {
                world_.day_clock().set_body_day_length(body.day_length);
            }
            break;
        }
        default:
            break;
    }

    // Body destinations go to an overworld instead of directly to a cave
    if (action.type == ChartActionType::TravelToBody) {
        const auto& body = target_sys.bodies[action.body_index];

        // Check cache for overworld
        if (world_.location_cache().count(dest_key)) {
            restore_location(dest_key);
        } else {
            // Generate overworld from body or moon properties
            auto props = default_properties(MapType::Overworld);
            props.biome = dest_biome;

            if (action.moon_index >= 0) {
                unsigned moon_seed = world_.seed() ^ (target_sys.id * 7919u)
                                   ^ (static_cast<unsigned>(action.body_index) * 6271u);
                auto moon = generate_moon_body(body, action.moon_index, moon_seed);
                props.body_type = moon.type;
                props.body_atmosphere = moon.atmosphere;
                props.body_temperature = moon.temperature;
                props.body_has_dungeon = moon.has_dungeon;
                props.body_danger_level = moon.danger_level;
            } else {
                props.body_type = body.type;
                props.body_atmosphere = body.atmosphere;
                props.body_temperature = body.temperature;
                props.body_has_dungeon = body.has_dungeon;
                props.body_danger_level = body.danger_level;
            }

            unsigned ow_seed = world_.seed() ^ (target_sys.id * 7919u)
                             ^ (static_cast<unsigned>(action.body_index) * 6271u)
                             ^ (static_cast<unsigned>(action.moon_index + 1) * 3571u);

            world_.map() = TileMap(props.width, props.height, MapType::Overworld);
            auto gen = create_generator(MapType::Overworld);
            gen->generate(world_.map(), props, ow_seed);
            world_.map().set_biome(dest_biome);
            world_.map().set_location_name(location_name);

            world_.npcs().clear();
            world_.ground_items().clear();

            // Find landing tile for player spawn
            bool found_landing = false;
            for (int y = 0; y < world_.map().height() && !found_landing; ++y) {
                for (int x = 0; x < world_.map().width() && !found_landing; ++x) {
                    if (world_.map().get(x, y) == Tile::OW_Landing) {
                        player_.x = x;
                        player_.y = y;
                        found_landing = true;
                    }
                }
            }
            if (!found_landing) {
                player_.x = world_.map().width() / 2;
                player_.y = world_.map().height() / 2;
            }

            world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
        }

        world_.set_surface_mode(SurfaceMode::Overworld);
        world_.overworld_x() = 0;
        world_.overworld_y() = 0;
        world_.visibility().reveal_all();
        world_.current_region() = -1;
        compute_camera();
        log("You land on " + colored(location_name, Color::Cyan)
            + ". The surface stretches before you.");
        return;
    }

    // Station destinations
    if (world_.location_cache().count(dest_key)) {
        restore_location(dest_key);
    } else {
        // Generate fresh map
        unsigned travel_seed = world_.rng()();
        auto props = default_properties(dest_type);
        props.biome = dest_biome;
        world_.map() = TileMap(props.width, props.height, dest_type);
        auto gen = create_generator(dest_type);
        gen->generate(world_.map(), props, travel_seed);
        world_.map().set_location_name(location_name);

        world_.npcs().clear();
        world_.ground_items().clear();
        world_.map().find_open_spot(player_.x, player_.y);

        std::mt19937 npc_rng(travel_seed ^ 0xD3ADu);
        std::vector<std::pair<int,int>> occupied = {{player_.x, player_.y}};
        debug_spawn(world_.map(), world_.npcs(), player_.x, player_.y, occupied, npc_rng);

        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    // Place ShipTerminal at stations so the player can re-board
    {
        bool has_terminal = false;
        for (int i = 0; i < world_.map().fixture_count(); ++i) {
            if (world_.map().fixture(i).type == FixtureType::ShipTerminal) {
                has_terminal = true;
                break;
            }
        }
        if (!has_terminal) {
            std::vector<std::pair<int,int>> occupied = {{player_.x, player_.y}};
            for (const auto& npc : world_.npcs()) {
                occupied.push_back({npc.x, npc.y});
            }
            int tx, ty;
            if (world_.map().find_open_spot_near(player_.x, player_.y, tx, ty, occupied, &world_.rng())) {
                world_.map().add_fixture(tx, ty, make_fixture(FixtureType::ShipTerminal));
            }
        }
    }

    world_.set_surface_mode(SurfaceMode::Dungeon);
    world_.current_region() = -1;
    recompute_fov();
    compute_camera();
    check_region_change();
    log("You dock at " + colored(location_name, Color::Cyan) + ".");
}

void Game::try_move(int dx, int dy) {
    int nx = player_.x + dx;
    int ny = player_.y + dy;

    // Detail map: edge transitions
    if (world_.on_detail_map()) {
        if (nx < 0 || nx >= world_.map().width() || ny < 0 || ny >= world_.map().height()) {
            int ddx = (nx < 0) ? -1 : (nx >= world_.map().width()) ? 1 : 0;
            int ddy = (ny < 0) ? -1 : (ny >= world_.map().height()) ? 1 : 0;
            transition_detail_edge(ddx, ddy);
            return;
        }
        // Fall through to standard dungeon movement below
    }

    // Overworld: simplified movement
    if (world_.on_overworld()) {
        if (nx < 0 || nx >= world_.map().width() || ny < 0 || ny >= world_.map().height()) return;
        if (!world_.map().passable(nx, ny)) {
            log("Impassable terrain.");
            return;
        }
        Tile prev_tile = world_.map().get(player_.x, player_.y);
        player_.x = nx;
        player_.y = ny;
        // Walk-over messages for POI tiles (suppress when moving within same tile type)
        Tile stepped = world_.map().get(nx, ny);
        if (stepped != prev_tile) {
            switch (stepped) {
                case Tile::OW_Settlement:   log("A settlement. Press > to enter."); break;
                case Tile::OW_CaveEntrance: log("A cave entrance. Press > to descend."); break;
                case Tile::OW_Ruins:        log("Ancient ruins. Press > to explore."); break;
                case Tile::OW_CrashedShip:  log("Wreckage of a starship. Press > to investigate."); break;
                case Tile::OW_Outpost:      log("An outpost. Press > to enter."); break;
                case Tile::OW_Landing:      log("Your starship. Press 'e' to board."); break;
                default: break;
            }
        }
        compute_camera();
        advance_world(15);
        return;
    }

    if (!world_.map().passable(nx, ny)) {
        auto msg = random_bump_message(world_.map().get(nx, ny), world_.map().map_type(), world_.rng());
        if (!msg.empty()) {
            log(std::string(msg));
        }
        return;
    }

    // Check NPC collision
    for (auto& npc : world_.npcs()) {
        if (npc.alive() && npc.x == nx && npc.y == ny) {
            if (npc.disposition == Disposition::Hostile) {
                attack_npc(npc);
                advance_world(ActionCost::move);
                return;
            }
            // Swap positions with friendly/neutral NPC
            npc.return_x = npc.x;
            npc.return_y = npc.y;
            npc.x = player_.x;
            npc.y = player_.y;
            player_.x = nx;
            player_.y = ny;
            recompute_fov();
            compute_camera();
            advance_world(ActionCost::move);
            return;
        }
    }

    player_.x = nx;
    player_.y = ny;

    // Portal tile: return to detail map / overworld if in a dungeon on a body
    if (world_.map().get(nx, ny) == Tile::Portal &&
        world_.surface_mode() == SurfaceMode::Dungeon && !world_.navigation().at_station && !world_.navigation().on_ship) {
        exit_dungeon_to_detail();
        return;
    }

    recompute_fov();
    compute_camera();
    check_region_change();
    advance_world(ActionCost::move);
}

void Game::check_region_change() {
    int rid = world_.map().region_id(player_.x, player_.y);
    if (rid == world_.current_region() || rid < 0) return;

    world_.current_region() = rid;
    const auto& reg = world_.map().region(rid);
    if (!reg.enter_message.empty()) {
        log(reg.enter_message);
    }

    // Feature hints for hub rooms
    if (has_feature(reg.features, RoomFeature::Healing)) {
        log("Healing pods hum softly, ready for use. [e to interact]");
    }
    if (has_feature(reg.features, RoomFeature::FoodShop)) {
        log("A food terminal glows nearby. [e to interact]");
    }
    if (has_feature(reg.features, RoomFeature::Rest)) {
        log("A rest pod glows at the far end. [e to interact]");
    }
    if (has_feature(reg.features, RoomFeature::Repair)) {
        log("A repair bench sits against the wall. [e to interact]");
    }
}

void Game::try_interact(int dx, int dy) {
    int tx = player_.x + dx;
    int ty = player_.y + dy;

    // Find NPC at target tile
    Npc* target = nullptr;
    for (auto& npc : world_.npcs()) {
        if (npc.x == tx && npc.y == ty) {
            target = &npc;
            break;
        }
    }

    if (!target) {
        Tile t = world_.map().get(tx, ty);
        if (t == Tile::Fixture) {
            int fid = world_.map().fixture_id(tx, ty);
            if (fid >= 0 && world_.map().fixture(fid).interactable) {
                dialog_.interact_fixture(fid, *this);
                advance_world(ActionCost::interact);
                return;
            }
            // Non-interactable fixture
            log("Nothing useful here.");
            return;
        }
        if (t == Tile::Wall || t == Tile::StructuralWall) {
            log("You run your hand along the cold bulkhead. Nothing of interest.");
        } else if (t == Tile::Empty) {
            log("You stare into the void. It stares back.");
        } else {
            log("Nothing to interact with there.");
        }
        return;
    }

    if (target->disposition == Disposition::Hostile) {
        log(target->display_name() + " snarls at you.");
        advance_world(ActionCost::interact);
        return;
    }

    if (target->interactions.empty()) {
        log(target->display_name() + " has nothing to say.");
        return;
    }

    log("You approach " + target->display_name() + ".");
    dialog_.open_npc_dialog(*target, *this);
    advance_world(ActionCost::interact);
}

bool Game::is_interactable(int tx, int ty) const {
    // Check for NPC
    for (const auto& npc : world_.npcs()) {
        if (npc.x == tx && npc.y == ty && npc.disposition != Disposition::Hostile) return true;
    }
    // Check for interactable fixture (including doors)
    Tile t = world_.map().get(tx, ty);
    if (t == Tile::Fixture) {
        int fid = world_.map().fixture_id(tx, ty);
        if (fid >= 0 && world_.map().fixture(fid).interactable) return true;
    }
    // Check for ground items at player's own tile
    if (tx == player_.x && ty == player_.y) {
        for (const auto& gi : world_.ground_items()) {
            if (gi.x == tx && gi.y == ty) return true;
        }
        // Stairs/portals under player
        if (t == Tile::Portal) return true;
    }
    return false;
}

int Game::count_adjacent_interactables() const {
    static const int dx[] = {0, -1, 1, 0, 0};
    static const int dy[] = {0, 0, 0, -1, 1};
    int count = 0;
    for (int i = 0; i < 5; ++i) {
        if (is_interactable(player_.x + dx[i], player_.y + dy[i])) ++count;
    }
    return count;
}

void Game::use_action() {
    // Scan all 4 adjacent tiles + player tile for interactables
    struct Target { int x, y; };
    std::vector<Target> targets;

    static const int dx[] = {0, -1, 1, 0, 0};
    static const int dy[] = {0, 0, 0, -1, 1};
    for (int i = 0; i < 5; ++i) {
        int tx = player_.x + dx[i];
        int ty = player_.y + dy[i];
        if (is_interactable(tx, ty)) targets.push_back({tx, ty});
    }

    if (targets.empty()) {
        log("Nothing to interact with nearby.");
        return;
    }

    if (targets.size() == 1) {
        use_at(targets[0].x, targets[0].y);
        return;
    }

    // Multiple targets — prompt for direction
    awaiting_interact_ = true;
    log("Use -- choose a direction.");
}

void Game::use_at(int tx, int ty) {
    // Ground items at player position
    if (tx == player_.x && ty == player_.y) {
        // Check for ground items first
        for (size_t i = 0; i < world_.ground_items().size(); ++i) {
            if (world_.ground_items()[i].x == tx && world_.ground_items()[i].y == ty) {
                pickup_ground_item();
                return;
            }
        }
        // Portal / stairs
        Tile t = world_.map().get(tx, ty);
        if (t == Tile::Portal) {
            if (world_.on_detail_map()) {
                enter_dungeon_from_detail();
            } else if (world_.surface_mode() == SurfaceMode::Dungeon) {
                exit_dungeon_to_detail();
            }
            return;
        }
    }

    // Adjacent tile — delegate to try_interact logic
    int dx = tx - player_.x;
    int dy = ty - player_.y;
    try_interact(dx, dy);
}


void Game::compute_camera() {
    // Center camera on player, clamped to map edges
    camera_x_ = player_.x - map_rect_.w / 2;
    camera_y_ = player_.y - map_rect_.h / 2;

    if (camera_x_ < 0) camera_x_ = 0;
    if (camera_y_ < 0) camera_y_ = 0;
    if (camera_x_ + map_rect_.w > world_.map().width()) camera_x_ = world_.map().width() - map_rect_.w;
    if (camera_y_ + map_rect_.h > world_.map().height()) camera_y_ = world_.map().height() - map_rect_.h;
    if (camera_x_ < 0) camera_x_ = 0;
    if (camera_y_ < 0) camera_y_ = 0;
}

void Game::recompute_fov() {
    if (world_.navigation().on_ship) {
        world_.visibility().reveal_all();
        return;
    }

    // Determine effective view radius based on context
    int radius = player_.view_radius + player_.equipment.total_modifiers().view_radius;
    bool is_indoor = world_.map().map_type() == MapType::SpaceStation
                  || world_.map().map_type() == MapType::DerelictStation
                  || world_.map().map_type() == MapType::Starship;
    bool is_dungeon = !world_.on_overworld() && !world_.on_detail_map()
                      && world_.map().map_type() != MapType::DetailMap
                      && !is_indoor;
    if (is_indoor) {
        // Stations and ships: always fully lit, use base view_radius
    } else if (is_dungeon) {
        // Underground: always use light_radius (no sunlight)
        radius = player_.light_radius;
    } else if (!world_.on_overworld()) {
        // Surface detail maps: time of day affects view range
        int max_radius = std::max(world_.map().width(), world_.map().height());
        radius = world_.day_clock().effective_view_radius(max_radius, player_.light_radius);
    }

    compute_fov(world_.map(), world_.visibility(), player_.x, player_.y, radius);

    // Detail maps: shadowcast for lighting, but entire map stays revealed
    if (world_.on_detail_map() || world_.map().map_type() == MapType::DetailMap) {
        world_.visibility().explore_all();
        return;
    }

    std::vector<bool> reveal(world_.map().region_count(), false);
    for (int y = 0; y < world_.map().height(); ++y) {
        for (int x = 0; x < world_.map().width(); ++x) {
            if (world_.visibility().get(x, y) == Visibility::Visible) {
                int rid = world_.map().region_id(x, y);
                if (rid >= 0 && world_.map().region(rid).lit) {
                    reveal[rid] = true;
                }
            }
        }
    }

    for (int y = 0; y < world_.map().height(); ++y) {
        for (int x = 0; x < world_.map().width(); ++x) {
            int rid = world_.map().region_id(x, y);
            if (rid >= 0 && reveal[rid]) {
                world_.visibility().set_visible(x, y);
            }
        }
    }
}

void Game::advance_world(int cost) {
    // Grant energy to all NPCs on the current map
    for (auto& npc : world_.npcs()) {
        npc.energy += cost * npc.quickness / 100;
    }

    // Process NPC turns until no NPC can act
    bool acted = true;
    while (acted) {
        acted = false;
        for (auto& npc : world_.npcs()) {
            while (npc.energy >= energy_threshold) {
                npc.energy -= energy_threshold;
                process_npc_turn(npc);
                acted = true;
            }
        }
    }

    remove_dead_npcs();
    check_player_death();
    ++world_.world_tick();
    world_.day_clock().advance(1);

    // Tick and expire effects
    tick_effects(player_.effects, player_.hp, player_.effective_max_hp());
    expire_effects(player_.effects);
    for (auto& npc : world_.npcs()) {
        if (npc.alive()) {
            tick_effects(npc.effects, npc.hp, npc.max_hp);
            expire_effects(npc.effects);
        }
    }

    // Water damage
    if (player_.hp > 0 && world_.map().get(player_.x, player_.y) == Tile::Water) {
        int water_dmg = apply_damage_effects(player_.effects, 1);
        player_.hp -= water_dmg;
        if (player_.hp < 0) player_.hp = 0;
        switch (world_.map().biome()) {
            case Biome::Fungal:
                log("Spores sting as you wade through the pool. (-1 HP)");
                break;
            case Biome::Crystal:
                log("Sharp mineral deposits cut at your legs. (-1 HP)");
                break;
            case Biome::Corroded:
                log("The toxic sludge burns! (-1 HP)");
                break;
            case Biome::Aquatic:
                log("The underground current pulls at you. (-1 HP)");
                break;
            default:
                log("The dark water chills you to the bone. (-1 HP)");
                break;
        }
        if (player_.hp <= 0) {
            death_message_ = "Drowned";
        }
        check_player_death();
    }

    // Passive health regeneration
    if (player_.hp > 0 && player_.hp < player_.max_hp) {
        int interval = regen_interval(player_.hunger);
        if (interval > 0) {
            ++player_.regen_counter;
            if (player_.regen_counter >= interval) {
                player_.regen_counter = 0;
                ++player_.hp;
                log("You feel a little better.");
            }
        }
    }
}

void Game::process_npc_turn(Npc& npc) {
    if (!npc.alive()) return;

    // Displaced NPCs try to return to their original position
    if (npc.return_x >= 0 && npc.return_y >= 0) {
        int rx = npc.return_x, ry = npc.return_y;
        npc.return_x = -1;
        npc.return_y = -1;
        // Only move back if the spot is free
        if (world_.map().passable(rx, ry) &&
            !(player_.x == rx && player_.y == ry) &&
            !tile_occupied(rx, ry)) {
            npc.x = rx;
            npc.y = ry;
        }
        return;
    }

    if (npc.disposition == Disposition::Friendly || npc.quickness == 0)
        return;

    if (npc.disposition == Disposition::Hostile) {
        int dist = chebyshev_dist(npc.x, npc.y, player_.x, player_.y);

        // Adjacent — attack
        if (dist <= 1) {
            int raw_damage = npc.attack_damage();
            int defense = player_.effective_defense();
            int damage = raw_damage - defense;
            if (damage < 1) damage = 1;
            damage = apply_damage_effects(player_.effects, damage);
            if (damage <= 0) {
                log(npc.display_name() + " strikes you but deals no damage.");
                return;
            }
            player_.hp -= damage;
            if (player_.hp < 0) player_.hp = 0;
            log(npc.display_name() + " strikes you for " +
                std::to_string(damage) + " damage!");
            if (player_.hp <= 0) {
                death_message_ = "Slain by " + npc.display_name();
            }
            return;
        }

        // Within detection range — chase
        if (dist <= 8) {
            int dx = sign(player_.x - npc.x);
            int dy = sign(player_.y - npc.y);

            // Try diagonal, then each cardinal fallback
            struct { int x, y; } candidates[] = {
                {dx, dy}, {dx, 0}, {0, dy}
            };
            for (auto [cx, cy] : candidates) {
                if (cx == 0 && cy == 0) continue;
                int nx = npc.x + cx;
                int ny = npc.y + cy;
                if (world_.map().passable(nx, ny) && !tile_occupied(nx, ny)) {
                    npc.x = nx;
                    npc.y = ny;
                    return;
                }
            }
            return; // all blocked, skip turn
        }

        // Fall through to wander
    }

    // Wander: try random cardinal directions
    std::array<std::pair<int,int>, 4> dirs = {{{0,-1},{0,1},{-1,0},{1,0}}};
    std::shuffle(dirs.begin(), dirs.end(), world_.rng());
    for (auto [dx, dy] : dirs) {
        int nx = npc.x + dx;
        int ny = npc.y + dy;
        if (world_.map().passable(nx, ny) && !tile_occupied(nx, ny)) {
            npc.x = nx;
            npc.y = ny;
            return;
        }
    }
}

bool Game::tile_occupied(int x, int y) const {
    if (player_.x == x && player_.y == y) return true;
    for (const auto& npc : world_.npcs()) {
        if (npc.alive() && npc.x == x && npc.y == y) return true;
    }
    return false;
}

void Game::attack_npc(Npc& npc) {
    int damage = player_.effective_attack();
    if (damage < 1) damage = 1;
    damage = apply_damage_effects(npc.effects, damage);
    if (damage <= 0) {
        log("Your attack has no effect on " + npc.display_name() + ".");
        return;
    }
    npc.hp -= damage;
    if (npc.hp < 0) npc.hp = 0;
    log("You strike " + npc.display_name() + " for " +
        std::to_string(damage) + " damage!");
    if (!npc.alive()) {
        log(npc.display_name() + " is destroyed!");
        player_.kills++;
        int xp = npc.xp_reward();
        if (xp > 0) {
            player_.xp += xp;
            log("You gain " + std::to_string(xp) + " XP.");
            check_level_up();
        }
        int credits = npc.level * 2 + (npc.elite ? 5 : 0);
        if (credits > 0) {
            player_.money += credits;
            log("You salvage " + std::to_string(credits) + "$.");
        }
        // Loot drop (50% chance)
        if (std::uniform_int_distribution<int>(0, 1)(world_.rng()) == 0) {
            Item loot = generate_loot_drop(world_.rng(), npc.level);
            log("Dropped: " + loot.name);
            world_.ground_items().push_back({npc.x, npc.y, std::move(loot)});
        }
    }
}

void Game::begin_targeting() {
    targeting_ = true;
    blink_phase_ = 0;

    // Find nearest visible hostile NPC
    Npc* nearest = nullptr;
    int best_dist = 9999;
    for (auto& npc : world_.npcs()) {
        if (!npc.alive() || npc.disposition != Disposition::Hostile) continue;
        if (world_.visibility().get(npc.x, npc.y) != Visibility::Visible) continue;
        int d = chebyshev_dist(player_.x, player_.y, npc.x, npc.y);
        if (d < best_dist) {
            best_dist = d;
            nearest = &npc;
        }
    }

    if (nearest) {
        target_x_ = nearest->x;
        target_y_ = nearest->y;
    } else {
        target_x_ = player_.x;
        target_y_ = player_.y;
    }

    log("Targeting mode. Move cursor, [Enter] confirm, [Esc] cancel.");
}

void Game::handle_targeting_input(int key) {
    auto try_move_cursor = [&](int dx, int dy) {
        // Scan up to 20 tiles in direction to skip walls/unexplored gaps
        for (int i = 1; i <= 20; ++i) {
            int nx = target_x_ + dx * i;
            int ny = target_y_ + dy * i;
            if (nx < 0 || nx >= world_.map().width() || ny < 0 || ny >= world_.map().height()) return;
            if (world_.map().passable(nx, ny) && world_.visibility().get(nx, ny) == Visibility::Visible) {
                target_x_ = nx;
                target_y_ = ny;
                return;
            }
        }
    };
    switch (key) {
        case 'k': case KEY_UP:    try_move_cursor( 0, -1); break;
        case 'j': case KEY_DOWN:  try_move_cursor( 0,  1); break;
        case 'h': case KEY_LEFT:  try_move_cursor(-1,  0); break;
        case 'l': case KEY_RIGHT: try_move_cursor( 1,  0); break;
        case '\n': case '\r': {
            // Check for alive NPC at cursor
            Npc* found = nullptr;
            for (auto& npc : world_.npcs()) {
                if (npc.alive() && npc.x == target_x_ && npc.y == target_y_) {
                    found = &npc;
                    break;
                }
            }
            if (found) {
                target_npc_ = found;
                targeting_ = false;
                log("Targeted: " + found->display_name());
            } else {
                log("No target there.");
            }
            break;
        }
        case '\033': // Escape
            targeting_ = false;
            target_npc_ = nullptr;
            log("Targeting cancelled.");
            break;
        default:
            break;
    }
}

void Game::shoot_target() {
    // Check weapon equipped
    auto& weapon = player_.equipment.missile;
    if (!weapon || !weapon->ranged) {
        log("No ranged weapon equipped.");
        return;
    }

    if (!target_npc_ || !target_npc_->alive()) {
        target_npc_ = nullptr;
        log("No target selected. Press [t] to target.");
        return;
    }

    if (world_.visibility().get(target_npc_->x, target_npc_->y) != Visibility::Visible) {
        log("Target not visible.");
        return;
    }

    // Check range
    auto& rd = *weapon->ranged;
    int dist = chebyshev_dist(player_.x, player_.y, target_npc_->x, target_npc_->y);
    if (dist > rd.max_range) {
        log("Target out of range (" + std::to_string(dist) + "/" +
            std::to_string(rd.max_range) + ").");
        return;
    }

    // Check charge — auto-reload if empty
    if (rd.current_charge < rd.charge_per_shot) {
        // Try auto-reload from inventory
        bool reloaded = false;
        for (int i = 0; i < static_cast<int>(player_.inventory.items.size()); ++i) {
            if (player_.inventory.items[i].type == ItemType::Battery) {
                int added = std::min(5, rd.charge_capacity - rd.current_charge);
                rd.current_charge += added;
                log("Auto-reload: +" + std::to_string(added) + " charge.");
                auto& cell = player_.inventory.items[i];
                if (cell.stackable && cell.stack_count > 1) {
                    --cell.stack_count;
                } else {
                    player_.inventory.items.erase(player_.inventory.items.begin() + i);
                }
                reloaded = true;
                break;
            }
        }
        if (!reloaded) {
            log("Weapon empty. No energy cells to reload.");
            return;
        }
        if (rd.current_charge < rd.charge_per_shot) {
            log("Not enough charge to fire.");
            return;
        }
    }

    // Consume charge
    rd.current_charge -= rd.charge_per_shot;

    // Damage = effective attack (includes STR modifier + all equipment)
    int damage = player_.effective_attack();
    if (damage < 1) damage = 1;
    damage = apply_damage_effects(target_npc_->effects, damage);
    if (damage <= 0) {
        log("Your shot has no effect on " + target_npc_->display_name() + ".");
        advance_world(ActionCost::shoot);
        return;
    }
    target_npc_->hp -= damage;
    if (target_npc_->hp < 0) target_npc_->hp = 0;
    log("You shoot " + target_npc_->display_name() + " for " +
        std::to_string(damage) + " damage. [" +
        std::to_string(rd.current_charge) + "/" +
        std::to_string(rd.charge_capacity) + "]");

    if (!target_npc_->alive()) {
        log(target_npc_->display_name() + " is destroyed!");
        player_.kills++;
        int xp = target_npc_->xp_reward();
        if (xp > 0) {
            player_.xp += xp;
            log("You gain " + std::to_string(xp) + " XP.");
            check_level_up();
        }
        // Loot drop (50% chance)
        if (std::uniform_int_distribution<int>(0, 1)(world_.rng()) == 0) {
            Item loot = generate_loot_drop(world_.rng(), target_npc_->level);
            log("Dropped: " + loot.name);
            world_.ground_items().push_back({target_npc_->x, target_npc_->y, std::move(loot)});
        }
        target_npc_ = nullptr;
    }

    advance_world(ActionCost::shoot);
}

void Game::reload_weapon() {
    auto& weapon = player_.equipment.missile;
    if (!weapon || !weapon->ranged) {
        log("No ranged weapon equipped.");
        return;
    }

    auto& rd = *weapon->ranged;
    if (rd.current_charge >= rd.charge_capacity) {
        log(weapon->name + " is fully charged.");
        return;
    }

    for (int i = 0; i < static_cast<int>(player_.inventory.items.size()); ++i) {
        if (player_.inventory.items[i].type == ItemType::Battery) {
            int added = std::min(5, rd.charge_capacity - rd.current_charge);
            rd.current_charge += added;
            log("Reloaded " + weapon->name + ". (+" + std::to_string(added) +
                " charge, " + std::to_string(rd.current_charge) + "/" +
                std::to_string(rd.charge_capacity) + ")");
            auto& cell = player_.inventory.items[i];
            if (cell.stackable && cell.stack_count > 1) {
                --cell.stack_count;
            } else {
                player_.inventory.items.erase(player_.inventory.items.begin() + i);
            }
            advance_world(ActionCost::wait);
            return;
        }
    }

    log("No energy cells to reload.");
}



// --- Helpers ---

Color Game::hp_color() const {
    int pct = (player_.max_hp > 0) ? (100 * player_.hp / player_.max_hp) : 0;
    if (pct < 30) return Color::Red;
    if (pct < 80) return Color::Yellow;
    return Color::Green;
}

Color Game::hunger_color() const {
    switch (player_.hunger) {
        case HungerState::Satiated: return Color::Green;
        case HungerState::Normal:   return Color::Default;
        case HungerState::Hungry:   return Color::Yellow;
        case HungerState::Starving: return Color::Red;
    }
    return Color::Default;
}

void Game::log(const std::string& msg) {
    messages_.push_back(":: " + msg);
    if (messages_.size() > max_messages_) {
        messages_.pop_front();
    }
}

void Game::check_player_death() {
    if (player_.hp <= 0) {
        save_system_.save_death(*this);
        state_ = GameState::GameOver;
    }
}

void Game::rebuild_star_chart_viewer() {
    star_chart_viewer_ = StarChartViewer(&world_.navigation(), renderer_.get());
}

void Game::reset_interaction_state() {
    awaiting_interact_ = false;
    targeting_ = false;
    target_npc_ = nullptr;
    inventory_cursor_ = 0;
    inspecting_item_ = false;
    dialog_.close();
    pause_menu_.close();
}

void Game::post_load() {
    compute_layout();
    recompute_fov();
    compute_camera();
    state_ = GameState::Playing;
}

} // namespace astra
