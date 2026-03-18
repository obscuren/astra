#include "astra/game.h"
#include "astra/boot_sequence.h"
#include "astra/debug_spawn.h"
#include "astra/item_defs.h"
#include "astra/map_generator.h"
#include "astra/map_properties.h"
#include "astra/npc_defs.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <ctime>

namespace astra {

static int sign(int v) { return (v > 0) - (v < 0); }

static int chebyshev_dist(int x1, int y1, int x2, int y2) {
    return std::max(std::abs(x1 - x2), std::abs(y1 - y2));
}

static const char* title_art[] = {
    R"(        .            *                .          |  )",
    R"(   *         .              .                .  -o- )",
    R"(        _        _                   *           |  )",
    R"(  .    / \   ___| |_ _ __ __ _           .         )",
    R"(      / _ \ / __| __| '__/ _` |   .                )",
    R"( *   / ___ \\__ \ |_| | | (_| |        *           )",
    R"(    /_/   \_\___/\__|_|  \__,_|  .                 )",
    R"(         .          *       .          .            )",
    R"(   .            .                 *                 )",
};
static constexpr int title_art_lines = 9;

static const char* menu_items[] = {
#ifdef ASTRA_DEV_MODE
    "Developer Mode (new character)",
#endif
    "New Game",
    "Load Game",
    "Hall of Fame",
    "Quit",
};

static const char* tab_names[] = {
    "Messages",
    "Inventory",
    "Equipment",
    "Ship",
};

Game::Game(std::unique_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)),
      pause_menu_("Menu") {
    pause_menu_.add_option("Return to Game");
    pause_menu_.add_option("Save Game");
    pause_menu_.add_option("Load Game");
    pause_menu_.add_option("Options");
    pause_menu_.add_option("Save and Quit");
}

void Game::run() {
    renderer_->init();
    running_ = true;
    compute_layout();

    render();

    while (running_) {
        int key = targeting_ ? renderer_->wait_input_timeout(300)
                             : renderer_->wait_input();
        if (key == -1) {
            // Timeout — toggle blink phase for reticule
            ++blink_phase_;
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
    int main_h = screen_h_ - 5; // 1 stats + 2 bars + 1 effects + 1 abilities

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

    // Row 4 & 5: bottom bars
    effects_rect_ = {0, screen_h_ - 2, screen_w_, 1};
    abilities_rect_ = {0, screen_h_ - 1, screen_w_, 1};
}

// --- Input ---

void Game::handle_input(int key) {
    switch (state_) {
        case GameState::MainMenu:  handle_menu_input(key);     break;
        case GameState::Playing:   handle_play_input(key);     break;
        case GameState::GameOver:  handle_gameover_input(key);  break;
        case GameState::LoadMenu:  handle_load_input(key);     break;
        case GameState::HallOfFame: handle_hall_input(key);    break;
    }
}

void Game::handle_menu_input(int key) {
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
                new_game();
            } else if (menu_selection_ == off + 1) {
                save_slots_ = list_saves();
                // Filter to alive saves only
                save_slots_.erase(
                    std::remove_if(save_slots_.begin(), save_slots_.end(),
                                   [](const SaveSlot& s) { return s.dead; }),
                    save_slots_.end());
                load_selection_ = 0;
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

void Game::handle_play_input(int key) {
    // Pause menu intercepts all input when open
    if (pause_menu_.is_open()) {
        // Esc toggles the pause menu closed
        if (key == '\033') {
            pause_menu_.close();
            return;
        }
        DialogResult result = pause_menu_.handle_input(key);
        if (result == DialogResult::Selected) {
            switch (pause_menu_.selected()) {
                case 0: break; // Return to Game — just closes
                case 1:
                    if (dev_mode_) { log("Saving disabled in dev mode."); }
                    else { save_game(); log("Game saved."); }
                    break;
                case 2: {
                    save_slots_ = list_saves();
                    save_slots_.erase(
                        std::remove_if(save_slots_.begin(), save_slots_.end(),
                                       [](const SaveSlot& s) { return s.dead; }),
                        save_slots_.end());
                    load_selection_ = 0;
                    state_ = GameState::LoadMenu;
                    break;
                }
                case 3: log("Options not yet implemented."); break;
                case 4:
                    if (!dev_mode_) save_game();
                    running_ = false;
                    break; // Save and Quit (skip save in dev mode)
            }
        }
        return;
    }

    // NPC dialog intercepts input when open
    if (npc_dialog_.is_open()) {
        DialogResult result = npc_dialog_.handle_input(key);
        if (result == DialogResult::Selected) {
            advance_dialog(npc_dialog_.selected());
        } else if (result == DialogResult::Closed) {
            interacting_npc_ = nullptr;
            dialog_tree_ = nullptr;
            dialog_node_ = -1;
        }
        return;
    }

    // Item inspect overlay — any key closes
    if (inspecting_item_) {
        inspecting_item_ = false;
        return;
    }

    // Targeting mode intercept
    if (targeting_) {
        handle_targeting_input(key);
        return;
    }

    // Awaiting interact direction (e + direction)
    if (awaiting_interact_) {
        awaiting_interact_ = false;
        switch (key) {
            case 'k': case KEY_UP:    try_interact( 0, -1); return;
            case 'j': case KEY_DOWN:  try_interact( 0,  1); return;
            case 'h': case KEY_LEFT:  try_interact(-1,  0); return;
            case 'l': case KEY_RIGHT: try_interact( 1,  0); return;
            default:
                log("Cancelled.");
                return;
        }
    }

    switch (key) {
        case '\033': pause_menu_.open(); break;
        case 'e':
            awaiting_interact_ = true;
            log("Interact -- choose a direction.");
            break;
        case 8: // Ctrl+H
            panel_visible_ = !panel_visible_;
            compute_layout();
            compute_camera();
            break;
        case '\t':
        case KEY_SHIFT_TAB:
            if (!panel_visible_) {
                panel_visible_ = true;
                compute_layout();
                compute_camera();
            }
            if (key == KEY_SHIFT_TAB)
                active_tab_ = (active_tab_ - 1 + panel_tab_count) % panel_tab_count;
            else
                active_tab_ = (active_tab_ + 1) % panel_tab_count;
            inventory_cursor_ = 0;
            break;
        case '.':
            log("You wait...");
            advance_world(ActionCost::wait);
            break;
        case 't': begin_targeting(); break;
        case 's': shoot_target(); break;
        case 'r': reload_weapon(); break;
        case 'g': pickup_ground_item(); break;
        case '>':
            if (dev_mode_ && map_.get(player_.x, player_.y) == Tile::Portal) {
                warp_to_dungeon();
            } else {
                log("There is no portal here.");
            }
            break;
        case '+': case '=': {
            auto tab = static_cast<PanelTab>(active_tab_);
            if (tab == PanelTab::Inventory) {
                int count = static_cast<int>(player_.inventory.items.size());
                if (count > 0 && inventory_cursor_ < count - 1) ++inventory_cursor_;
            } else if (tab == PanelTab::Equipment) {
                if (inventory_cursor_ < equip_slot_count - 1) ++inventory_cursor_;
            }
            break;
        }
        case '-': {
            auto tab = static_cast<PanelTab>(active_tab_);
            if (tab == PanelTab::Inventory || tab == PanelTab::Equipment) {
                if (inventory_cursor_ > 0) --inventory_cursor_;
            }
            break;
        }
        case 'i': {
            // Auto-switch to Inventory tab if on a non-item tab
            auto tab = static_cast<PanelTab>(active_tab_);
            if (tab != PanelTab::Inventory && tab != PanelTab::Equipment) {
                active_tab_ = static_cast<int>(PanelTab::Inventory);
                inventory_cursor_ = 0;
                if (!panel_visible_) {
                    panel_visible_ = true;
                    compute_layout();
                    compute_camera();
                }
                break;
            }
            if (tab == PanelTab::Inventory) {
                int count = static_cast<int>(player_.inventory.items.size());
                if (count > 0 && inventory_cursor_ < count) {
                    inspected_item_ = player_.inventory.items[inventory_cursor_];
                    inspecting_item_ = true;
                }
            } else if (tab == PanelTab::Equipment) {
                auto slot = static_cast<EquipSlot>(inventory_cursor_);
                const auto& opt = player_.equipment.slot_ref(slot);
                if (opt) {
                    inspected_item_ = *opt;
                    inspecting_item_ = true;
                }
            }
            break;
        }
        case '\n': case '\r': {
            auto tab = static_cast<PanelTab>(active_tab_);
            if (tab == PanelTab::Inventory) {
                int count = static_cast<int>(player_.inventory.items.size());
                if (count > 0 && inventory_cursor_ < count) {
                    const auto& item = player_.inventory.items[inventory_cursor_];
                    if (item.type == ItemType::Equipment && item.slot) {
                        equip_item(inventory_cursor_);
                    } else if (item.usable) {
                        use_item(inventory_cursor_);
                    }
                    if (inventory_cursor_ >= static_cast<int>(player_.inventory.items.size()))
                        inventory_cursor_ = std::max(0, static_cast<int>(player_.inventory.items.size()) - 1);
                }
            } else if (tab == PanelTab::Equipment) {
                unequip_slot(inventory_cursor_);
            }
            break;
        }
        case 'd': {
            auto tab = static_cast<PanelTab>(active_tab_);
            if (tab == PanelTab::Inventory) {
                int count = static_cast<int>(player_.inventory.items.size());
                if (count > 0 && inventory_cursor_ < count) {
                    drop_item(inventory_cursor_);
                    if (inventory_cursor_ >= static_cast<int>(player_.inventory.items.size()))
                        inventory_cursor_ = std::max(0, static_cast<int>(player_.inventory.items.size()) - 1);
                }
            }
            break;
        }
        case 'k': case KEY_UP:    try_move( 0, -1); break;
        case 'j': case KEY_DOWN:  try_move( 0,  1); break;
        case 'h': case KEY_LEFT:  try_move(-1,  0); break;
        case 'l': case KEY_RIGHT: try_move( 1,  0); break;
    }
}

// --- Logic ---

void Game::new_game() {
    compute_layout();

    // Boot sequence for normal games, skip in dev mode
    if (!dev_mode_) {
        BootSequence boot(renderer_.get());
        boot.play();
    }

    seed_ = static_cast<unsigned>(std::time(nullptr));
    rng_.seed(seed_);

    auto props = default_properties(MapType::SpaceStation);
    map_ = TileMap(props.width, props.height, MapType::SpaceStation);
    auto gen = create_generator(MapType::SpaceStation);
    gen->generate(map_, props, seed_);
    map_.set_location_name("The Heavens Above");

    player_ = Player{};
    map_.find_open_spot(player_.x, player_.y);

    // Spawn NPCs in the player's starting room
    npcs_.clear();
    ground_items_.clear();
    std::mt19937 npc_rng(static_cast<unsigned>(std::time(nullptr)) ^ 0xA7C3u);
    std::vector<std::pair<int,int>> occupied = {{player_.x, player_.y}};

    auto spawn_near = [&](NpcRole role, Race race, int near_x, int near_y) {
        Npc npc = create_npc(role, race, npc_rng);
        if (map_.find_open_spot_near(near_x, near_y,
                                     npc.x, npc.y, occupied, &npc_rng)) {
            occupied.push_back({npc.x, npc.y});
            npcs_.push_back(std::move(npc));
        }
    };

    auto spawn_other_room = [&](NpcRole role, Race race) {
        Npc npc = create_npc(role, race, npc_rng);
        if (map_.find_open_spot_other_room(player_.x, player_.y,
                                           npc.x, npc.y, occupied, &npc_rng)) {
            occupied.push_back({npc.x, npc.y});
            npcs_.push_back(std::move(npc));
        }
    };

    spawn_near(NpcRole::StationKeeper, Race::Human, player_.x, player_.y);
    spawn_near(NpcRole::Merchant, Race::Veldrani, player_.x, player_.y);
    spawn_near(NpcRole::Drifter, Race::Sylphari, player_.x, player_.y);

    // Spawn Nova — unique Stellari NPC, always in starting room
    {
        Npc nova = build_nova();
        if (map_.find_open_spot_near(player_.x, player_.y,
                                      nova.x, nova.y, occupied, &npc_rng)) {
            occupied.push_back({nova.x, nova.y});
            npcs_.push_back(std::move(nova));
        }
    }

    debug_spawn(map_, npcs_, player_.x, player_.y, occupied, npc_rng);

    visibility_ = VisibilityMap(map_.width(), map_.height());
    recompute_fov();
    compute_camera();

    messages_.clear();
    awaiting_interact_ = false;
    targeting_ = false;
    target_npc_ = nullptr;
    inventory_cursor_ = 0;
    inspecting_item_ = false;
    current_region_ = -1;
    active_tab_ = 0; // Start on Messages tab
    if (dev_mode_) {
        log("--- DEVELOPER MODE --- Saving disabled.");
    }
    log("Welcome aboard, commander. Your journey to Sgr A* begins.");
    log("You are docked at The Heavens Above, the space station orbiting Jupiter.");
    check_region_change();

    // Starter gear: random ranged weapon + battery
    Item weapon = random_ranged_weapon(rng_);
    player_.equipment.ranged_weapon = weapon;
    log("You are armed with a " + weapon.name + ".");

    Item battery = build_battery();
    battery.stack_count = 3;
    player_.inventory.items.push_back(battery);

    // Dev mode: place a portal in the starting room for testing generators
    if (dev_mode_) {
        int px, py;
        std::vector<std::pair<int,int>> portal_exclude = {{player_.x, player_.y}};
        for (const auto& npc : npcs_) {
            portal_exclude.push_back({npc.x, npc.y});
        }
        if (map_.find_open_spot_near(player_.x, player_.y, px, py,
                                      portal_exclude, &rng_)) {
            map_.set(px, py, Tile::Portal);
            log("A shimmering portal appears nearby. (Dev: step on '>' to warp)");
        }
    }

    state_ = GameState::Playing;
}

void Game::warp_to_dungeon() {
    // Pick a random map type
    static constexpr MapType warp_types[] = {
        MapType::SpaceStation, MapType::Rocky, MapType::Asteroid};
    std::uniform_int_distribution<int> type_dist(0, 2);
    MapType dest_type = warp_types[type_dist(rng_)];

    unsigned warp_seed = rng_();
    auto props = default_properties(dest_type);
    map_ = TileMap(props.width, props.height, dest_type);
    auto gen = create_generator(dest_type);
    gen->generate(map_, props, warp_seed);

    const char* type_name = "Unknown";
    switch (dest_type) {
        case MapType::SpaceStation: type_name = "Space Station"; break;
        case MapType::Rocky:        type_name = "Open Cavern"; break;
        case MapType::Asteroid:     type_name = "Asteroid Tunnel"; break;
        default: break;
    }
    map_.set_location_name(type_name);

    // Reset entities
    npcs_.clear();
    ground_items_.clear();

    // Respawn player
    map_.find_open_spot(player_.x, player_.y);

    // Spawn hostile NPCs in other rooms
    std::mt19937 npc_rng(warp_seed ^ 0xD3ADu);
    std::vector<std::pair<int,int>> occupied = {{player_.x, player_.y}};
    debug_spawn(map_, npcs_, player_.x, player_.y, occupied, npc_rng);

    visibility_ = VisibilityMap(map_.width(), map_.height());
    current_region_ = -1;
    recompute_fov();
    compute_camera();
    check_region_change();

    log("You step through the portal...");
    log("You arrive at a " + std::string(type_name) + ".");

    // Place another portal for chaining
    int px, py;
    std::vector<std::pair<int,int>> portal_exclude = {{player_.x, player_.y}};
    for (const auto& npc : npcs_) {
        portal_exclude.push_back({npc.x, npc.y});
    }
    if (map_.find_open_spot_near(player_.x, player_.y, px, py,
                                  portal_exclude, &rng_)) {
        map_.set(px, py, Tile::Portal);
    }
}

void Game::try_move(int dx, int dy) {
    int nx = player_.x + dx;
    int ny = player_.y + dy;
    if (!map_.passable(nx, ny)) {
        auto msg = random_bump_message(map_.get(nx, ny), map_.map_type(), rng_);
        if (!msg.empty()) {
            log(std::string(msg));
        }
        return;
    }

    // Check NPC collision
    for (auto& npc : npcs_) {
        if (npc.alive() && npc.x == nx && npc.y == ny) {
            if (npc.disposition == Disposition::Hostile) {
                attack_npc(npc);
                advance_world(ActionCost::move);
                return;
            }
            log("You see " + npc.display_name() + ".");
            return;
        }
    }

    player_.x = nx;
    player_.y = ny;

    recompute_fov();
    compute_camera();
    check_region_change();
    advance_world(ActionCost::move);
}

void Game::check_region_change() {
    int rid = map_.region_id(player_.x, player_.y);
    if (rid == current_region_ || rid < 0) return;

    current_region_ = rid;
    const auto& reg = map_.region(rid);
    if (!reg.enter_message.empty()) {
        log(reg.enter_message);
    }
}

void Game::try_interact(int dx, int dy) {
    int tx = player_.x + dx;
    int ty = player_.y + dy;

    // Find NPC at target tile
    Npc* target = nullptr;
    for (auto& npc : npcs_) {
        if (npc.x == tx && npc.y == ty) {
            target = &npc;
            break;
        }
    }

    if (!target) {
        Tile t = map_.get(tx, ty);
        if (t == Tile::Wall) {
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
    open_npc_dialog(*target);
    advance_world(ActionCost::interact);
}

void Game::open_npc_dialog(Npc& npc) {
    interacting_npc_ = &npc;
    dialog_tree_ = nullptr;
    dialog_node_ = -1;
    interact_options_.clear();

    const auto& data = npc.interactions;
    std::string greeting;
    if (data.talk) {
        greeting = "\"" + data.talk->greeting + "\"";
    }

    npc_dialog_ = Dialog(npc.display_name(), greeting);
    int hotkey = '1';

    if (data.talk && !data.talk->nodes.empty()) {
        npc_dialog_.add_option("Talk.", hotkey++);
        interact_options_.push_back(InteractOption::Talk);
    }
    if (data.shop) {
        npc_dialog_.add_option("Show me your wares.", hotkey++);
        interact_options_.push_back(InteractOption::Shop);
    }
    if (data.quest) {
        npc_dialog_.add_option(data.quest->quest_intro, hotkey++);
        interact_options_.push_back(InteractOption::Quest);
    }

    npc_dialog_.add_option("Farewell.", hotkey);
    interact_options_.push_back(InteractOption::Farewell);

    npc_dialog_.open();
}

void Game::advance_dialog(int selected) {
    if (!interacting_npc_) return;

    // Currently navigating a dialog tree
    if (dialog_tree_ && dialog_node_ >= 0) {
        const auto& node = (*dialog_tree_)[dialog_node_];
        if (selected < 0 || selected >= static_cast<int>(node.choices.size())) {
            // Invalid selection, close
            interacting_npc_ = nullptr;
            dialog_tree_ = nullptr;
            dialog_node_ = -1;
            return;
        }

        int next = node.choices[selected].next_node;
        if (next < 0 || next >= static_cast<int>(dialog_tree_->size())) {
            // End of conversation — return to top-level menu
            dialog_tree_ = nullptr;
            dialog_node_ = -1;
            open_npc_dialog(*interacting_npc_);
            return;
        }

        // Advance to next node
        dialog_node_ = next;
        const auto& next_node = (*dialog_tree_)[dialog_node_];
        npc_dialog_ = Dialog(interacting_npc_->display_name(),
                             "\"" + next_node.text + "\"");
        int hotkey = '1';
        for (const auto& choice : next_node.choices) {
            npc_dialog_.add_option(choice.label, hotkey++);
        }
        npc_dialog_.open();
        return;
    }

    // Top-level menu selection
    if (selected < 0 || selected >= static_cast<int>(interact_options_.size())) {
        interacting_npc_ = nullptr;
        return;
    }

    switch (interact_options_[selected]) {
        case InteractOption::Talk: {
            dialog_tree_ = &interacting_npc_->interactions.talk->nodes;
            dialog_node_ = 0;
            const auto& node = (*dialog_tree_)[0];
            npc_dialog_ = Dialog(interacting_npc_->display_name(),
                                 "\"" + node.text + "\"");
            int hotkey = '1';
            for (const auto& choice : node.choices) {
                npc_dialog_.add_option(choice.label, hotkey++);
            }
            npc_dialog_.open();
            break;
        }
        case InteractOption::Shop:
            log("\"Have a look.\" [Shop not yet implemented]");
            interacting_npc_ = nullptr;
            dialog_tree_ = nullptr;
            dialog_node_ = -1;
            break;

        case InteractOption::Quest: {
            dialog_tree_ = &interacting_npc_->interactions.quest->nodes;
            dialog_node_ = 0;
            const auto& node = (*dialog_tree_)[0];
            npc_dialog_ = Dialog(interacting_npc_->display_name(),
                                 "\"" + node.text + "\"");
            int hotkey = '1';
            for (const auto& choice : node.choices) {
                npc_dialog_.add_option(choice.label, hotkey++);
            }
            npc_dialog_.open();
            break;
        }
        case InteractOption::Farewell:
            log("\"Safe travels, commander.\"");
            interacting_npc_ = nullptr;
            dialog_tree_ = nullptr;
            dialog_node_ = -1;
            break;
    }
}

void Game::compute_camera() {
    // Center camera on player, clamped to map edges
    camera_x_ = player_.x - map_rect_.w / 2;
    camera_y_ = player_.y - map_rect_.h / 2;

    if (camera_x_ < 0) camera_x_ = 0;
    if (camera_y_ < 0) camera_y_ = 0;
    if (camera_x_ + map_rect_.w > map_.width()) camera_x_ = map_.width() - map_rect_.w;
    if (camera_y_ + map_rect_.h > map_.height()) camera_y_ = map_.height() - map_rect_.h;
    if (camera_x_ < 0) camera_x_ = 0;
    if (camera_y_ < 0) camera_y_ = 0;
}

void Game::recompute_fov() {
    compute_fov(map_, visibility_, player_.x, player_.y, player_.view_radius);

    std::vector<bool> reveal(map_.region_count(), false);
    for (int y = 0; y < map_.height(); ++y) {
        for (int x = 0; x < map_.width(); ++x) {
            if (visibility_.get(x, y) == Visibility::Visible) {
                int rid = map_.region_id(x, y);
                if (rid >= 0 && map_.region(rid).lit) {
                    reveal[rid] = true;
                }
            }
        }
    }

    for (int y = 0; y < map_.height(); ++y) {
        for (int x = 0; x < map_.width(); ++x) {
            int rid = map_.region_id(x, y);
            if (rid >= 0 && reveal[rid]) {
                visibility_.set_visible(x, y);
            }
        }
    }
}

void Game::advance_world(int cost) {
    // Grant energy to all NPCs on the current map
    for (auto& npc : npcs_) {
        npc.energy += cost * npc.quickness / 100;
    }

    // Process NPC turns until no NPC can act
    bool acted = true;
    while (acted) {
        acted = false;
        for (auto& npc : npcs_) {
            while (npc.energy >= energy_threshold) {
                npc.energy -= energy_threshold;
                process_npc_turn(npc);
                acted = true;
            }
        }
    }

    remove_dead_npcs();
    check_player_death();
    ++world_tick_;

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
    if (npc.disposition == Disposition::Friendly || npc.quickness == 0)
        return;

    if (npc.disposition == Disposition::Hostile) {
        int dist = chebyshev_dist(npc.x, npc.y, player_.x, player_.y);

        // Adjacent — attack
        if (dist <= 1) {
            int damage = npc.attack_damage();
            if (damage < 1) damage = 1;
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
                if (map_.passable(nx, ny) && !tile_occupied(nx, ny)) {
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
    std::shuffle(dirs.begin(), dirs.end(), rng_);
    for (auto [dx, dy] : dirs) {
        int nx = npc.x + dx;
        int ny = npc.y + dy;
        if (map_.passable(nx, ny) && !tile_occupied(nx, ny)) {
            npc.x = nx;
            npc.y = ny;
            return;
        }
    }
}

bool Game::tile_occupied(int x, int y) const {
    if (player_.x == x && player_.y == y) return true;
    for (const auto& npc : npcs_) {
        if (npc.alive() && npc.x == x && npc.y == y) return true;
    }
    return false;
}

void Game::attack_npc(Npc& npc) {
    if (npc.invulnerable) {
        log("Your attack has no effect on " + npc.display_name() + ".");
        return;
    }
    int damage = player_.attack_value;
    if (damage < 1) damage = 1;
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
        }
    }
}

void Game::begin_targeting() {
    targeting_ = true;
    blink_phase_ = 0;

    // Find nearest visible hostile NPC
    Npc* nearest = nullptr;
    int best_dist = 9999;
    for (auto& npc : npcs_) {
        if (!npc.alive() || npc.disposition != Disposition::Hostile) continue;
        if (visibility_.get(npc.x, npc.y) != Visibility::Visible) continue;
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
            if (nx < 0 || nx >= map_.width() || ny < 0 || ny >= map_.height()) return;
            if (map_.passable(nx, ny) && visibility_.get(nx, ny) == Visibility::Visible) {
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
            for (auto& npc : npcs_) {
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
    auto& weapon = player_.equipment.ranged_weapon;
    if (!weapon || !weapon->ranged) {
        log("No ranged weapon equipped.");
        return;
    }

    if (!target_npc_ || !target_npc_->alive()) {
        target_npc_ = nullptr;
        log("No target selected. Press [t] to target.");
        return;
    }

    if (visibility_.get(target_npc_->x, target_npc_->y) != Visibility::Visible) {
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

    if (target_npc_->invulnerable) {
        log("Your shot has no effect on " + target_npc_->display_name() + ".");
        advance_world(ActionCost::shoot);
        return;
    }

    // Damage = weapon attack modifier + player base attack
    int damage = player_.attack_value + weapon->modifiers.attack;
    if (damage < 1) damage = 1;
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
        }
        target_npc_ = nullptr;
    }

    advance_world(ActionCost::shoot);
}

void Game::reload_weapon() {
    auto& weapon = player_.equipment.ranged_weapon;
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

void Game::pickup_ground_item() {
    for (auto it = ground_items_.begin(); it != ground_items_.end(); ++it) {
        if (it->x == player_.x && it->y == player_.y) {
            if (!player_.inventory.can_add(it->item)) {
                log("Too heavy to pick up " + it->item.name + ".");
                return;
            }
            log("You pick up " + it->item.name + ".");
            player_.inventory.items.push_back(std::move(it->item));
            ground_items_.erase(it);
            advance_world(ActionCost::move);
            return;
        }
    }
    log("Nothing here to pick up.");
}

void Game::drop_item(int index) {
    auto& items = player_.inventory.items;
    if (index < 0 || index >= static_cast<int>(items.size())) return;

    Item item = std::move(items[index]);
    items.erase(items.begin() + index);

    log("You drop " + item.name + ".");
    ground_items_.push_back({player_.x, player_.y, std::move(item)});
}

void Game::use_item(int index) {
    auto& items = player_.inventory.items;
    if (index < 0 || index >= static_cast<int>(items.size())) return;

    auto& item = items[index];
    switch (item.type) {
        case ItemType::Food: {
            int heal = 3;
            player_.hp = std::min(player_.hp + heal, player_.max_hp);
            if (player_.hunger > HungerState::Satiated)
                player_.hunger = static_cast<HungerState>(
                    static_cast<uint8_t>(player_.hunger) - 1);
            log("You eat the " + item.name + ". (+3 HP)");
            break;
        }
        case ItemType::Stim: {
            int heal = 5;
            player_.hp = std::min(player_.hp + heal, player_.max_hp);
            log("You inject the " + item.name + ". (+5 HP)");
            break;
        }
        case ItemType::Battery: {
            auto& eq = player_.equipment.ranged_weapon;
            if (!eq || !eq->ranged) {
                log("No ranged weapon equipped to recharge.");
                return;
            }
            auto& rd = *eq->ranged;
            if (rd.current_charge >= rd.charge_capacity) {
                log("Weapon is already fully charged.");
                return;
            }
            int added = std::min(5, rd.charge_capacity - rd.current_charge);
            rd.current_charge += added;
            log("You recharge " + eq->name + ". (+" + std::to_string(added) + " charge)");
            break;
        }
        default:
            log("You can't use " + item.name + ".");
            return;
    }

    // Consume the item
    if (item.stackable && item.stack_count > 1) {
        --item.stack_count;
    } else {
        items.erase(items.begin() + index);
    }
    advance_world(ActionCost::wait);
}

void Game::equip_item(int index) {
    auto& items = player_.inventory.items;
    if (index < 0 || index >= static_cast<int>(items.size())) return;

    auto& item = items[index];
    if (item.type != ItemType::Equipment || !item.slot) {
        log("Can't equip " + item.name + ".");
        return;
    }

    auto& slot = player_.equipment.slot_ref(*item.slot);
    Item to_equip = std::move(item);
    items.erase(items.begin() + index);

    if (slot) {
        // Swap: old equipped item goes to inventory
        items.push_back(std::move(*slot));
        log("You unequip " + items.back().name + ".");
    }
    log("You equip " + to_equip.name + ".");
    slot = std::move(to_equip);
}

void Game::unequip_slot(int index) {
    auto slot = static_cast<EquipSlot>(index);
    auto& opt = player_.equipment.slot_ref(slot);
    if (!opt) {
        log("Nothing equipped in that slot.");
        return;
    }

    Item item = std::move(*opt);
    opt.reset();

    if (!player_.inventory.can_add(item)) {
        log("Inventory too heavy. " + item.name + " stays equipped.");
        opt = std::move(item);
        return;
    }
    log("You unequip " + item.name + ".");
    player_.inventory.items.push_back(std::move(item));
}

void Game::remove_dead_npcs() {
    // Nullify target_npc_ if it died
    if (target_npc_ && !target_npc_->alive()) {
        target_npc_ = nullptr;
    }
    // Nullify interacting_npc_ if it died
    if (interacting_npc_ && !interacting_npc_->alive()) {
        npc_dialog_.close();
        interacting_npc_ = nullptr;
        dialog_tree_ = nullptr;
        dialog_node_ = -1;
    }
    npcs_.erase(
        std::remove_if(npcs_.begin(), npcs_.end(),
                        [](const Npc& n) { return !n.alive(); }),
        npcs_.end());
}

void Game::check_player_death() {
    if (player_.hp <= 0) {
        // Permadeath: save with dead flag
        SaveData data;
        data.version = 1;
        data.seed = seed_;
        data.world_tick = world_tick_;
        data.dead = true;
        data.player = player_;
        data.current_map_id = 0;
        data.current_region = current_region_;
        data.active_tab = active_tab_;
        data.panel_visible = panel_visible_;
        data.messages = messages_;
        data.death_message = death_message_;

        MapState ms;
        ms.map_id = 0;
        ms.tilemap = map_;
        ms.visibility = visibility_;
        ms.npcs = npcs_;
        data.maps.push_back(std::move(ms));

        write_save("save_" + std::to_string(seed_), data);
        state_ = GameState::GameOver;
    }
}

void Game::handle_gameover_input(int key) {
    switch (key) {
        case '\n': case '\r':
            state_ = GameState::MainMenu;
            menu_selection_ = 0;
            break;
        case 'q': case 'Q':
            running_ = false;
            break;
    }
}

void Game::handle_load_input(int key) {
    switch (key) {
        case '\033':
            state_ = GameState::MainMenu;
            menu_selection_ = 0;
            break;
        case 'w': case 'k': case KEY_UP:
            if (!save_slots_.empty()) {
                load_selection_ = (load_selection_ - 1 + static_cast<int>(save_slots_.size()))
                                  % static_cast<int>(save_slots_.size());
            }
            break;
        case 's': case 'j': case KEY_DOWN:
            if (!save_slots_.empty()) {
                load_selection_ = (load_selection_ + 1) % static_cast<int>(save_slots_.size());
            }
            break;
        case '\n': case '\r':
            if (!save_slots_.empty() &&
                load_selection_ >= 0 &&
                load_selection_ < static_cast<int>(save_slots_.size())) {
                if (load_game(save_slots_[load_selection_].filename)) {
                    log("Game loaded.");
                }
            }
            break;
    }
}

void Game::handle_hall_input(int key) {
    switch (key) {
        case '\033':
            state_ = GameState::MainMenu;
            menu_selection_ = 0;
            confirm_delete_ = false;
            break;
        case 'w': case 'k': case KEY_UP:
            confirm_delete_ = false;
            if (!save_slots_.empty()) {
                load_selection_ = (load_selection_ - 1 + static_cast<int>(save_slots_.size()))
                                  % static_cast<int>(save_slots_.size());
            }
            break;
        case 's': case 'j': case KEY_DOWN:
            confirm_delete_ = false;
            if (!save_slots_.empty()) {
                load_selection_ = (load_selection_ + 1) % static_cast<int>(save_slots_.size());
            }
            break;
        case 'd': case 'D':
            if (!save_slots_.empty()) {
                confirm_delete_ = !confirm_delete_;
            }
            break;
        case 'y': case 'Y':
            if (confirm_delete_ && !save_slots_.empty() &&
                load_selection_ >= 0 &&
                load_selection_ < static_cast<int>(save_slots_.size())) {
                delete_save(save_slots_[load_selection_].filename);
                save_slots_.erase(save_slots_.begin() + load_selection_);
                if (load_selection_ >= static_cast<int>(save_slots_.size())) {
                    load_selection_ = static_cast<int>(save_slots_.size()) - 1;
                }
                if (load_selection_ < 0) load_selection_ = 0;
                confirm_delete_ = false;
            }
            break;
        case 'n': case 'N':
            confirm_delete_ = false;
            break;
    }
}

void Game::save_game() {
    SaveData data;
    data.version = 1;
    data.seed = seed_;
    data.world_tick = world_tick_;
    data.dead = false;
    data.player = player_;
    data.current_map_id = 0;
    data.current_region = current_region_;
    data.active_tab = active_tab_;
    data.panel_visible = panel_visible_;
    data.messages = messages_;
    data.death_message = death_message_;

    MapState ms;
    ms.map_id = 0;
    ms.tilemap = map_;
    ms.visibility = visibility_;
    ms.npcs = npcs_;
    ms.ground_items = ground_items_;
    data.maps.push_back(std::move(ms));

    write_save("save_" + std::to_string(seed_), data);
}

bool Game::load_game(const std::string& filename) {
    SaveData data;
    if (!read_save(filename, data)) return false;
    if (data.dead) return false;
    if (data.maps.empty()) return false;

    dev_mode_ = false;
    seed_ = data.seed;
    rng_.seed(seed_);
    world_tick_ = data.world_tick;
    player_ = data.player;
    death_message_ = data.death_message;
    current_region_ = data.current_region;
    active_tab_ = data.active_tab;
    panel_visible_ = data.panel_visible;
    messages_ = data.messages;

    // Restore first map
    const auto& ms = data.maps[0];
    map_ = ms.tilemap;
    visibility_ = ms.visibility;
    npcs_ = ms.npcs;
    ground_items_ = ms.ground_items;

    // Reset interaction state
    awaiting_interact_ = false;
    targeting_ = false;
    target_npc_ = nullptr;
    inventory_cursor_ = 0;
    inspecting_item_ = false;
    interacting_npc_ = nullptr;
    dialog_tree_ = nullptr;
    dialog_node_ = -1;
    pause_menu_.close();

    compute_layout();
    compute_camera();
    state_ = GameState::Playing;
    return true;
}

void Game::update() {
    // Tick-based — world updates happen in response to player actions.
}

// --- Rendering ---

void Game::render() {
    renderer_->clear();

    switch (state_) {
        case GameState::MainMenu:   render_menu();          break;
        case GameState::Playing:    render_play();          break;
        case GameState::GameOver:   render_gameover();      break;
        case GameState::LoadMenu:   render_load_menu();     break;
        case GameState::HallOfFame: render_hall_of_fame();  break;
    }

    renderer_->present();
}

// Deterministic star at any world coordinate
static char star_at(int x, int y) {
    // Hash the coordinate to get a deterministic pseudo-random value
    unsigned h = static_cast<unsigned>(x * 374761393 + y * 668265263);
    h = (h ^ (h >> 13)) * 1274126177;
    h ^= h >> 16;
    if ((h % 100) >= 3) return '\0'; // ~3% chance of a star
    unsigned st = (h >> 8) % 10;
    if (st < 6) return '.';
    if (st < 9) return '*';
    return '+';
}

void Game::render_menu() {
    DrawContext ctx(renderer_.get(), screen_rect_);

    // Starfield backdrop
    for (int sy = 0; sy < screen_h_; ++sy) {
        for (int sx = 0; sx < screen_w_; ++sx) {
            char star = star_at(sx, sy);
            if (star) {
                Color c = (star == '+') ? Color::Yellow
                        : (star == '*') ? Color::White
                                        : Color::Cyan;
                ctx.put(sx, sy, star, c);
            }
        }
    }

    int art_start_y = screen_h_ / 2 - title_art_lines - 2;

    for (int i = 0; i < title_art_lines; ++i) {
        ctx.text_center(art_start_y + i, title_art[i], Color::White);
    }

    int menu_y = art_start_y + title_art_lines + 2;
    for (int i = 0; i < menu_item_count_; ++i) {
        if (i == menu_selection_) {
            // ·::|  Label  |::·
            std::string item = menu_items[i];
            std::string padded = "  " + item + "  ";
            std::string full = ".::|" + padded + "|::.";
            int x = (screen_w_ - static_cast<int>(full.size())) / 2;
            ctx.text(x, menu_y + i, ".::", Color::Red);
            ctx.put(x + 3, menu_y + i, '|', Color::Cyan);
            ctx.text(x + 4, menu_y + i, padded, Color::Yellow);
            ctx.put(x + 4 + static_cast<int>(padded.size()), menu_y + i, '|', Color::Cyan);
            ctx.text(x + 5 + static_cast<int>(padded.size()), menu_y + i, "::.", Color::Red);
        } else {
            std::string label = "     " + std::string(menu_items[i]) + "     ";
            ctx.text_center(menu_y + i, label, Color::DarkGray);
        }
    }
}

void Game::render_play() {
    render_stats_bar();
    render_bars();

    render_tabs();

    DrawContext sep_ctx(renderer_.get(), separator_rect_);
    sep_ctx.vline(0, '|');

    render_map();

    if (panel_visible_) {
        render_side_panel();
    }
    render_effects_bar();
    render_abilities_bar();

    // Overlay windows
    if (inspecting_item_) render_item_inspect();
    npc_dialog_.draw(renderer_.get(), screen_w_, screen_h_);
    pause_menu_.draw(renderer_.get(), screen_w_, screen_h_);
}

void Game::render_stats_bar() {
    DrawContext ctx(renderer_.get(), stats_bar_rect_);

    // Dev mode indicator
    int lx = 1;
    if (dev_mode_) {
        ctx.text(lx, 0, "[DEV]", Color::Red);
        lx += 6;
    }

    // Left side: level :: temp :: hunger :: money
    lx = ctx.label_value(lx, 0, "LVL:", Color::DarkGray,
        std::to_string(player_.level), Color::White);

    ctx.text(lx, 0, " :: ", Color::DarkGray); lx += 4;
    lx = ctx.label_value(lx, 0, "T:", Color::DarkGray,
        std::to_string(player_.temperature) + "~", Color::White);

    const char* hname = hunger_name(player_.hunger);
    if (hname[0] != '\0') {
        ctx.text(lx, 0, " :: ", Color::DarkGray); lx += 4;
        ctx.text(lx, 0, hname, hunger_color());
        lx += static_cast<int>(std::string_view(hname).size());
    }

    ctx.text(lx, 0, " :: ", Color::DarkGray); lx += 4;
    lx = ctx.label_value(lx, 0, "", Color::DarkGray,
        std::to_string(player_.money) + "$", Color::Yellow);

    // Right side: stats, calendar, location — measure total width for right-alignment
    std::string right;
    right += " QN:";  right += std::to_string(player_.quickness);
    right += " :: MS:"; right += std::to_string(player_.move_speed);
    right += " :: AV:"; right += std::to_string(player_.attack_value);
    right += " :: DV:"; right += std::to_string(player_.defense_value);
    right += " :: Cycle 1, Day 1";
    right += " :: ";    right += map_.location_name();
    right += " ";

    int rx = ctx.width() - static_cast<int>(right.size());
    if (rx < lx + 2) rx = lx + 2;

    // Fill gap between left items and right items with repeating <<>>
    {
        const char pattern[] = "<<>>";
        int gap_start = lx + 1;
        int gap_end = rx - 1;
        for (int i = gap_start; i < gap_end; ++i) {
            ctx.put(i, 0, pattern[(i - gap_start) % 4], Color::Cyan);
        }
    }

    // Render right side with per-segment colors
    int x = rx;
    x = ctx.label_value(x, 0, "QN:", Color::DarkGray,
        std::to_string(player_.quickness), Color::White);

    ctx.text(x, 0, " :: ", Color::DarkGray); x += 4;
    x = ctx.label_value(x, 0, "MS:", Color::DarkGray,
        std::to_string(player_.move_speed), Color::White);

    ctx.text(x, 0, " :: ", Color::DarkGray); x += 4;
    x = ctx.label_value(x, 0, "AV:", Color::DarkGray,
        std::to_string(player_.attack_value), Color::Blue);

    ctx.text(x, 0, " :: ", Color::DarkGray); x += 4;
    x = ctx.label_value(x, 0, "DV:", Color::DarkGray,
        std::to_string(player_.defense_value), Color::Blue);

    ctx.text(x, 0, " :: ", Color::DarkGray); x += 4;
    ctx.text(x, 0, "Cycle 1, Day 1", Color::DarkGray); x += 14;

    ctx.text(x, 0, " :: ", Color::DarkGray); x += 4;
    ctx.text(x, 0, map_.location_name(), Color::White);
}

void Game::render_bars() {
    // Format value strings and find max width for alignment
    std::string hp_val = std::to_string(player_.hp) + "/" + std::to_string(player_.max_hp);
    std::string xp_val = std::to_string(player_.xp) + "/" + std::to_string(player_.max_xp);
    int val_w = static_cast<int>(std::max(hp_val.size(), xp_val.size()));

    // Right-justify values by padding with spaces
    while (static_cast<int>(hp_val.size()) < val_w) hp_val = " " + hp_val;
    while (static_cast<int>(xp_val.size()) < val_w) xp_val = " " + xp_val;

    // "HP: " and "XP: " are both 4 chars — labels already align
    // bar_start = 1 (margin) + 4 (label) + val_w + 1 (space)
    int bar_start = 1 + 4 + val_w + 1;

    // HP bar
    {
        DrawContext ctx(renderer_.get(), hp_bar_rect_);
        ctx.text(1, 0, "HP:", Color::DarkGray);
        ctx.text(4, 0, hp_val, hp_color());
        int bar_w = ctx.width() - bar_start - 2;
        if (bar_w > 0) {
            ctx.bar(bar_start, 0, bar_w, player_.hp, player_.max_hp, hp_color());
        }
    }

    // XP bar
    {
        DrawContext ctx(renderer_.get(), xp_bar_rect_);
        ctx.text(1, 0, "XP:", Color::DarkGray);
        ctx.text(4, 0, xp_val, Color::Cyan);
        int bar_w = ctx.width() - bar_start - 2;
        if (bar_w > 0) {
            ctx.bar(bar_start, 0, bar_w, player_.xp, player_.max_xp, Color::Cyan);
        }
    }
}

void Game::render_tabs() {
    DrawContext ctx(renderer_.get(), tabs_rect_);
    int x = 1;

    for (int i = 0; i < panel_tab_count; ++i) {
        bool active = (i == active_tab_);
        std::string label = std::string("[") + tab_names[i] + "]";
        Color fg = active ? Color::Yellow : Color::DarkGray;
        ctx.text(x, 0, label, fg);
        x += static_cast<int>(label.size()) + 1;
    }

    // Horizontal separator below tabs (row 2 on right side)
    DrawContext sep(renderer_.get(), {tabs_rect_.x, tabs_rect_.y + 1, tabs_rect_.w, 1});
    sep.hline(0, '-');
}

void Game::render_map() {
    DrawContext ctx(renderer_.get(), map_rect_);

    for (int sy = 0; sy < map_rect_.h; ++sy) {
        for (int sx = 0; sx < map_rect_.w; ++sx) {
            int mx = camera_x_ + sx;
            int my = camera_y_ + sy;

            // Starfield backdrop — covers entire viewport
            bool has_tile = (map_.get(mx, my) != Tile::Empty);
            if (!has_tile) {
                char star = star_at(mx, my);
                if (star) {
                    Color c = (star == '*' || star == '+') ? Color::White : Color::Cyan;
                    ctx.put(sx, sy, star, c);
                }
            }

            // Tiles respect FOV
            Visibility v = visibility_.get(mx, my);
            if (v == Visibility::Unexplored) continue;

            char g = tile_glyph(map_.get(mx, my));
            if (g == ' ') continue;

            if (v == Visibility::Visible) {
                Tile t = map_.get(mx, my);
                Color c = Color::Default;
                if (t == Tile::Wall) c = Color::White;
                else if (t == Tile::Portal) c = Color::Magenta;
                ctx.put(sx, sy, g, c);
            } else {
                ctx.put(sx, sy, g, Color::Blue);
            }
        }
    }

    // Draw visible ground items
    for (const auto& gi : ground_items_) {
        if (visibility_.get(gi.x, gi.y) == Visibility::Visible) {
            ctx.put(gi.x - camera_x_, gi.y - camera_y_,
                    gi.item.glyph, gi.item.color);
        }
    }

    // Draw visible NPCs
    for (const auto& npc : npcs_) {
        if (npc.alive() && visibility_.get(npc.x, npc.y) == Visibility::Visible) {
            ctx.put(npc.x - camera_x_, npc.y - camera_y_, npc.glyph, npc.color);
        }
    }

    // Draw player relative to camera
    ctx.put(player_.x - camera_x_, player_.y - camera_y_, '@', Color::Yellow);

    // Draw targeting line and reticule
    if (targeting_) {
        // Bresenham line from player to reticule
        int x0 = player_.x, y0 = player_.y;
        int x1 = target_x_, y1 = target_y_;
        int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
        int sx = (x0 < x1) ? 1 : -1;
        int sy = (y0 < y1) ? 1 : -1;
        int err = dx - dy;

        // Determine weapon range for coloring
        int weapon_range = 0;
        const auto& rw = player_.equipment.ranged_weapon;
        if (rw && rw->ranged) weapon_range = rw->ranged->max_range;

        int lx = x0, ly = y0;
        while (lx != x1 || ly != y1) {
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; lx += sx; }
            if (e2 <  dx) { err += dx; ly += sy; }
            if (lx == x1 && ly == y1) break; // don't draw on reticule pos
            int scx = lx - camera_x_, scy = ly - camera_y_;
            if (scx >= 0 && scx < map_rect_.w && scy >= 0 && scy < map_rect_.h) {
                int tile_dist = chebyshev_dist(x0, y0, lx, ly);
                Color line_color = (weapon_range > 0 && tile_dist <= weapon_range)
                    ? Color::Green : Color::Red;
                ctx.put(scx, scy, '*', line_color);
            }
        }

        // Reticule: blink only when over something interesting (NPC, item, etc.)
        int rx = target_x_ - camera_x_, ry = target_y_ - camera_y_;
        if (rx >= 0 && rx < map_rect_.w && ry >= 0 && ry < map_rect_.h) {
            bool has_entity = false;
            for (const auto& npc : npcs_) {
                if (npc.alive() && npc.x == target_x_ && npc.y == target_y_) {
                    has_entity = true;
                    break;
                }
            }
            if (!has_entity || blink_phase_ % 2 == 0) {
                int target_dist = chebyshev_dist(player_.x, player_.y, target_x_, target_y_);
                Color ret_color = (weapon_range > 0 && target_dist <= weapon_range)
                    ? Color::Green : Color::Red;
                ctx.put(rx, ry, '+', ret_color);
            }
            // else: let the underlying NPC/item glyph show through
        }
    }
}

void Game::render_side_panel() {
    DrawContext ctx(renderer_.get(), side_panel_rect_);

    switch (static_cast<PanelTab>(active_tab_)) {
        case PanelTab::Messages: {
            int max_w = ctx.width();
            int visible = ctx.height();
            if (max_w <= 0 || visible <= 0) break;

            // Word-wrap all messages into display lines
            // Continuation lines are indented to align past the ":: " prefix
            static constexpr int indent = 3; // matches ":: " prefix
            struct WrappedLine {
                std::string_view text;
                int x; // 0 for first line, indent for continuations
            };
            std::vector<WrappedLine> lines;

            for (const auto& msg : messages_) {
                std::string_view remaining = msg;
                bool first = true;
                while (!remaining.empty()) {
                    int line_w = first ? max_w : max_w - indent;
                    if (line_w <= 0) line_w = 1;
                    int x = first ? 0 : indent;
                    first = false;

                    if (static_cast<int>(remaining.size()) <= line_w) {
                        lines.push_back({remaining, x});
                        break;
                    }
                    // Find last space within line_w
                    int cut = line_w;
                    while (cut > 0 && remaining[cut] != ' ') --cut;
                    if (cut == 0) cut = line_w;
                    lines.push_back({remaining.substr(0, cut), x});
                    remaining = remaining.substr(cut);
                    if (!remaining.empty() && remaining[0] == ' ')
                        remaining = remaining.substr(1);
                }
            }

            // Auto-scroll: show the last 'visible' lines
            int total = static_cast<int>(lines.size());
            int start = (total > visible) ? total - visible : 0;
            int y = 0;
            for (int i = start; i < total && y < visible; ++i, ++y) {
                ctx.text(lines[i].x, y, lines[i].text, Color::Default);
            }
            break;
        }
        case PanelTab::Inventory: {
            const auto& inv = player_.inventory;
            if (inv.items.empty()) {
                ctx.text(1, 1, "Inventory is empty.", Color::DarkGray);
            } else {
                int y = 0;
                for (int idx = 0; idx < static_cast<int>(inv.items.size()); ++idx) {
                    if (y >= ctx.height() - 2) break; // reserve space for weight + hints
                    const auto& item = inv.items[idx];
                    bool selected = (idx == inventory_cursor_);
                    if (selected) ctx.text(0, y, ">", Color::Yellow);
                    ctx.put(1, y, item.glyph, item.color);
                    std::string label = " " + item.name;
                    if (item.stackable && item.stack_count > 1)
                        label += " x" + std::to_string(item.stack_count);
                    Color fg = selected ? Color::White : rarity_color(item.rarity);
                    ctx.text(2, y, label, fg);
                    ++y;
                }
                // Weight summary
                int wy = ctx.height() - 2;
                if (wy > static_cast<int>(inv.items.size())) {
                    std::string wt = "Weight: " + std::to_string(inv.total_weight())
                                   + "/" + std::to_string(inv.max_carry_weight);
                    ctx.text(1, wy, wt, Color::DarkGray);
                }
            }
            // Key hints at bottom — context-sensitive
            int hy = ctx.height() - 1;
            if (!inv.items.empty() && inventory_cursor_ < static_cast<int>(inv.items.size())) {
                const auto& sel = inv.items[inventory_cursor_];
                std::string hints = "+/- ";
                if (sel.type == ItemType::Equipment && sel.slot)
                    hints += "[Enter]equip ";
                else if (sel.usable)
                    hints += "[Enter]use ";
                hints += "[i]nfo [d]rop";
                ctx.text(1, hy, hints, Color::DarkGray);
            } else {
                ctx.text(1, hy, "+/- navigate", Color::DarkGray);
            }
            break;
        }
        case PanelTab::Equipment: {
            const auto& eq = player_.equipment;
            int y = 0;
            int slot_idx = 0;
            auto draw_slot = [&](const char* label, const std::optional<Item>& slot) {
                if (y >= ctx.height() - 2) return;
                bool selected = (slot_idx == inventory_cursor_);
                if (selected) {
                    ctx.text(0, y, ">", Color::Yellow);
                }
                ctx.text(1, y, label, Color::DarkGray);
                int lx = 1 + static_cast<int>(std::string_view(label).size());
                if (slot) {
                    ctx.put(lx, y, slot->glyph, slot->color);
                    Color fg = selected ? Color::White : rarity_color(slot->rarity);
                    ctx.text(lx + 1, y, " " + slot->name, fg);
                } else {
                    ctx.text(lx, y, "---", Color::DarkGray);
                }
                ++y;
                ++slot_idx;
            };
            draw_slot("Head:    ", eq.head);
            draw_slot("Chest:   ", eq.chest);
            draw_slot("Legs:    ", eq.legs);
            draw_slot("Feet:    ", eq.feet);
            draw_slot("Hands:   ", eq.hands);
            draw_slot("Melee:   ", eq.melee_weapon);
            draw_slot("Ranged:  ", eq.ranged_weapon);
            draw_slot("Special: ", eq.special_slot);

            // Stat bonuses summary
            y++;
            if (y < ctx.height() - 1) {
                auto mods = eq.total_modifiers();
                ctx.text(1, y, "Bonuses:", Color::DarkGray);
                ++y;
                if (mods.attack && y < ctx.height() - 1) {
                    ctx.text(2, y, "ATK +" + std::to_string(mods.attack), Color::Red);
                    ++y;
                }
                if (mods.defense && y < ctx.height() - 1) {
                    ctx.text(2, y, "DEF +" + std::to_string(mods.defense), Color::Blue);
                    ++y;
                }
                if (mods.max_hp && y < ctx.height() - 1) {
                    ctx.text(2, y, "HP  +" + std::to_string(mods.max_hp), Color::Green);
                    ++y;
                }
                if (mods.view_radius && y < ctx.height() - 1) {
                    ctx.text(2, y, "VIS +" + std::to_string(mods.view_radius), Color::Cyan);
                    ++y;
                }
                if (mods.quickness && y < ctx.height() - 1) {
                    std::string sign = mods.quickness > 0 ? "+" : "";
                    ctx.text(2, y, "QCK " + sign + std::to_string(mods.quickness), Color::Yellow);
                    ++y;
                }
            }
            // Key hints at bottom
            int hy = ctx.height() - 1;
            auto sel_slot = static_cast<EquipSlot>(inventory_cursor_);
            const auto& sel_opt = eq.slot_ref(sel_slot);
            if (sel_opt) {
                ctx.text(1, hy, "+/- [Enter]unequip [i]nfo", Color::DarkGray);
            } else {
                ctx.text(1, hy, "+/- navigate", Color::DarkGray);
            }
            break;
        }
        case PanelTab::Ship:
            ctx.text(1, 1, "Ship: Docked", Color::DarkGray);
            ctx.text(1, 2, "Hull: 100%", Color::DarkGray);
            break;
    }
}

void Game::render_item_inspect() {
    const auto& item = inspected_item_;

    int win_w = 44;
    int win_h = 18;
    if (win_w > screen_w_ - 4) win_w = screen_w_ - 4;
    if (win_h > screen_h_ - 4) win_h = screen_h_ - 4;

    Window win(renderer_.get(), screen_w_, screen_h_, win_w, win_h, item.name);
    win.set_footer("[any key] Close");
    win.draw();

    DrawContext ctx = win.content();
    int y = 0;

    // Rarity + type
    ctx.put(0, y, item.glyph, item.color);
    std::string rarity_type = std::string(" ") + rarity_name(item.rarity);
    ctx.text(1, y, rarity_type, rarity_color(item.rarity));
    ++y;

    // Description (word-wrapped)
    if (!item.description.empty()) {
        ++y;
        std::string_view desc = item.description;
        int max_w = ctx.width();
        while (!desc.empty() && y < ctx.height()) {
            if (static_cast<int>(desc.size()) <= max_w) {
                ctx.text(0, y++, desc, Color::Default);
                break;
            }
            int cut = max_w;
            while (cut > 0 && desc[cut] != ' ') --cut;
            if (cut == 0) cut = max_w;
            ctx.text(0, y++, desc.substr(0, cut), Color::Default);
            desc = desc.substr(cut);
            if (!desc.empty() && desc[0] == ' ') desc = desc.substr(1);
        }
    }

    ++y;

    // Stat modifiers
    const auto& m = item.modifiers;
    if (m.attack) {
        ctx.label_value(0, y, "Attack:    ", Color::DarkGray,
            (m.attack > 0 ? "+" : "") + std::to_string(m.attack), Color::Red);
        ++y;
    }
    if (m.defense) {
        ctx.label_value(0, y, "Defense:   ", Color::DarkGray,
            (m.defense > 0 ? "+" : "") + std::to_string(m.defense), Color::Blue);
        ++y;
    }
    if (m.max_hp) {
        ctx.label_value(0, y, "Max HP:    ", Color::DarkGray,
            (m.max_hp > 0 ? "+" : "") + std::to_string(m.max_hp), Color::Green);
        ++y;
    }
    if (m.view_radius) {
        ctx.label_value(0, y, "Vision:    ", Color::DarkGray,
            (m.view_radius > 0 ? "+" : "") + std::to_string(m.view_radius), Color::Cyan);
        ++y;
    }
    if (m.quickness) {
        ctx.label_value(0, y, "Quickness: ", Color::DarkGray,
            (m.quickness > 0 ? "+" : "") + std::to_string(m.quickness), Color::Yellow);
        ++y;
    }

    // Ranged weapon charge
    if (item.ranged) {
        const auto& rd = *item.ranged;
        ctx.text(0, y, "Charge: ", Color::DarkGray);
        int bar_w = std::min(16, ctx.width() - 10);
        if (bar_w > 0) {
            ctx.bar(8, y, bar_w, rd.current_charge, rd.charge_capacity,
                    Color::Cyan, Color::DarkGray);
        }
        std::string charge_str = std::to_string(rd.current_charge) + "/"
                               + std::to_string(rd.charge_capacity);
        ctx.text(8 + bar_w + 3, y, charge_str, Color::Cyan);
        ++y;
        if (y < ctx.height()) {
            ctx.label_value(0, y, "Range:     ", Color::DarkGray,
                std::to_string(rd.max_range), Color::White);
            ++y;
        }
    }

    // Durability
    if (item.max_durability > 0 && y < ctx.height()) {
        ctx.text(0, y, "Durabl: ", Color::DarkGray);
        int bar_w = std::min(16, ctx.width() - 10);
        if (bar_w > 0) {
            Color dur_color = (item.durability * 3 > item.max_durability) ? Color::Green : Color::Red;
            ctx.bar(8, y, bar_w, item.durability, item.max_durability,
                    dur_color, Color::DarkGray);
        }
        std::string dur_str = std::to_string(item.durability) + "/"
                            + std::to_string(item.max_durability);
        ctx.text(8 + bar_w + 3, y, dur_str, Color::Green);
        ++y;
    }

    // Weight + value
    if (y < ctx.height()) {
        ++y;
        std::string info = "Wt:" + std::to_string(item.weight);
        info += "  Buy:" + std::to_string(item.buy_value);
        info += "  Sell:" + std::to_string(item.sell_value);
        ctx.text(0, y, info, Color::DarkGray);
    }
}

void Game::render_effects_bar() {
    DrawContext ctx(renderer_.get(), effects_rect_);
    int x = 1;
    ctx.text(x, 0, "EFFECTS:", Color::DarkGray);
    x += 8;
    ctx.text(x, 0, " [none]", Color::DarkGray);

    int mid = ctx.width() / 3;
    ctx.text(mid, 0, "TARGET:", Color::DarkGray);
    if (target_npc_ && target_npc_->alive()) {
        std::string info = " " + target_npc_->display_name() +
            " (" + std::to_string(target_npc_->hp) + "/" +
            std::to_string(target_npc_->max_hp) + ")";
        Color tc = Color::DarkGray;
        switch (target_npc_->disposition) {
            case Disposition::Hostile:  tc = Color::Red; break;
            case Disposition::Neutral:  tc = Color::Yellow; break;
            case Disposition::Friendly: tc = Color::Green; break;
        }
        ctx.text(mid + 7, 0, info, tc);
    } else {
        ctx.text(mid + 7, 0, " [none]", Color::DarkGray);
    }

    // Ranged weapon hints (right-aligned)
    const auto& rw = player_.equipment.ranged_weapon;
    if (rw && rw->ranged) {
        const auto& rd = *rw->ranged;
        std::string keys = "[t]arget [s]hoot [r]eload ";
        std::string charge = std::to_string(rd.current_charge) + "/" +
                             std::to_string(rd.charge_capacity);
        std::string full = keys + charge;
        int ix = ctx.width() - static_cast<int>(full.size()) - 1;
        Color charge_color = (rd.current_charge >= rd.charge_per_shot)
            ? Color::Cyan : Color::Red;
        ctx.text(ix, 0, keys, Color::DarkGray);
        ctx.text(ix + static_cast<int>(keys.size()), 0, charge, charge_color);
    }
}

void Game::render_abilities_bar() {
    DrawContext ctx(renderer_.get(), abilities_rect_);
    ctx.text(1, 0, "ABILITIES:", Color::DarkGray);
    ctx.text(12, 0, "[reserved]", Color::DarkGray);
}

void Game::render_gameover() {
    DrawContext ctx(renderer_.get(), screen_rect_);

    int cy = screen_h_ / 2 - 4;
    ctx.text_center(cy,     "YOU HAVE DIED", Color::Red);
    cy += 2;
    if (!death_message_.empty()) {
        ctx.text_center(cy, death_message_);
        cy += 2;
    }
    ctx.text_center(cy,     "Survived " + std::to_string(world_tick_) + " ticks");
    ctx.text_center(cy + 1, "Reached level " + std::to_string(player_.level));
    cy += 3;
    ctx.text_center(cy,     "[Enter] Main Menu    [Q] Quit", Color::DarkGray);
}

void Game::render_load_menu() {
    int win_w = std::min(screen_w_ - 4, 60);
    int win_h = std::min(screen_h_ - 4, static_cast<int>(save_slots_.size()) + 6);
    if (win_h < 8) win_h = 8;

    Window win(renderer_.get(), screen_w_, screen_h_, win_w, win_h, "Load Game");
    win.set_footer("[Enter] Load  [Esc] Back");
    win.draw();
    DrawContext ctx = win.content();

    if (save_slots_.empty()) {
        ctx.text_center(ctx.height() / 2, "No saved games found.", Color::DarkGray);
        return;
    }

    for (int i = 0; i < static_cast<int>(save_slots_.size()); ++i) {
        if (i >= ctx.height()) break;
        const auto& slot = save_slots_[i];
        bool selected = (i == load_selection_);

        std::string line;
        if (selected) line += "> "; else line += "  ";
        line += slot.location;
        line += "  LVL:" + std::to_string(slot.player_level);
        line += "  T:" + std::to_string(slot.world_tick);

        Color fg = selected ? Color::Yellow : Color::Default;
        ctx.text(0, i, line, fg);
    }
}

void Game::render_hall_of_fame() {
    int win_w = std::min(screen_w_ - 4, 70);
    int win_h = std::min(screen_h_ - 4, static_cast<int>(save_slots_.size()) + 6);
    if (win_h < 8) win_h = 8;

    Window win(renderer_.get(), screen_w_, screen_h_, win_w, win_h, "Hall of Fame");
    if (confirm_delete_) {
        win.set_footer("Delete this entry? [Y] Yes  [N] No", Color::Red);
    } else {
        win.set_footer("[D] Delete  [Esc] Back");
    }
    win.draw();
    DrawContext ctx = win.content();

    if (save_slots_.empty()) {
        ctx.text_center(ctx.height() / 2, "No fallen heroes yet.", Color::DarkGray);
        return;
    }

    for (int i = 0; i < static_cast<int>(save_slots_.size()); ++i) {
        if (i >= ctx.height()) break;
        const auto& slot = save_slots_[i];
        bool selected = (i == load_selection_);

        std::string line;
        if (selected) line += "> "; else line += "  ";

        if (!slot.death_message.empty()) {
            line += slot.death_message;
        } else {
            line += "Unknown cause";
        }
        line += "  LVL:" + std::to_string(slot.player_level);
        line += "  T:" + std::to_string(slot.world_tick);
        line += "  XP:" + std::to_string(slot.xp);
        line += "  $" + std::to_string(slot.money);
        line += "  K:" + std::to_string(slot.kills);

        Color fg = selected ? Color::Yellow : Color::DarkGray;
        ctx.text(0, i, line, fg);
    }
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

} // namespace astra
