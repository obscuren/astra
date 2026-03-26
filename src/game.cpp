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

// Block-letter ASTRA logo using █ (U+2588 full block)
#define B "\xe2\x96\x88"  // █
static const char* title_letter_A[] = {
    "    " B B B "    ", "   " B B " " B B "   ", "  " B B "   " B B "  ",
    " " B B "     " B B " ", " " B B B B B B B B B " ", " " B B "     " B B " ", " " B B "     " B B " ",
};
static const char* title_letter_S[] = {
    "  " B B B B B B B "  ", " " B B "     " B B " ", " " B B "        ",
    "  " B B B B B B B "  ", "        " B B " ", " " B B "     " B B " ", "  " B B B B B B B "  ",
};
static const char* title_letter_T[] = {
    " " B B B B B B B B B " ", "    " B B B "    ", "    " B B B "    ",
    "    " B B B "    ", "    " B B B "    ", "    " B B B "    ", "    " B B B "    ",
};
static const char* title_letter_R[] = {
    " " B B B B B B B B "  ", " " B B "     " B B " ", " " B B "     " B B " ",
    " " B B B B B B B B "  ", " " B B "   " B B "   ", " " B B "    " B B "  ", " " B B "     " B B " ",
};
#undef B
static const char* const* title_letters[] = {
    title_letter_A, title_letter_S, title_letter_T, title_letter_R, title_letter_A,
};
static constexpr int title_letter_count = 5;
static constexpr int title_letter_height = 7;
static constexpr int title_letter_width = 11;
static constexpr int title_letter_gap = 2;

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
    "Wait",
};

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
    // Welcome screen — space dismisses
    if (show_welcome_) {
        if (key == ' ') show_welcome_ = false;
        return;
    }

    // Dev console intercept
    if (console_.is_open()) {
        console_.handle_input(key, *this);
        return;
    }
    // Backtick opens console in dev mode
    if (key == '`' && dev_mode_) {
        console_.toggle();
        return;
    }

    // Help screen intercept
    if (help_screen_.is_open()) {
        help_screen_.handle_input(key);
        return;
    }

    // Pause menu intercepts all input when open
    if (pause_menu_.is_open()) {
        MenuResult result = pause_menu_.handle_input(key);
        if (result == MenuResult::Selected) {
            char k = pause_menu_.selected_key();
            if (k == 'r') { /* Return to Game — just closes */ }
            else if (k == 'h') {
                help_screen_.open();
            }
            else if (k == 's') {
                if (dev_mode_) { log("Saving disabled in dev mode."); }
                else { save_game(); log("Game saved."); }
            }
            else if (k == 'l') {
                save_slots_ = list_saves();
                save_slots_.erase(
                    std::remove_if(save_slots_.begin(), save_slots_.end(),
                                   [](const SaveSlot& s) { return s.dead; }),
                    save_slots_.end());
                load_selection_ = 0;
                state_ = GameState::LoadMenu;
            }
            else if (k == 'o') { log("Options not yet implemented."); }
            else if (k == 'q') {
                if (!dev_mode_) save_game();
                running_ = false;
            }
        }
        return;
    }

    // Trade window intercepts input when open
    if (trade_window_.is_open()) {
        trade_window_.handle_input(key);
        if (!trade_window_.is_open()) {
            if (trade_window_.has_message()) log(trade_window_.consume_message());
            interacting_npc_ = nullptr;
            dialog_tree_ = nullptr;
            dialog_node_ = -1;
        }
        return;
    }

    // Character screen intercepts input when open
    if (character_screen_.is_open()) {
        character_screen_.handle_input(key);
        return;
    }

    // Star chart viewer intercepts input when open
    if (star_chart_viewer_.is_open()) {
        star_chart_viewer_.handle_input(key);
        if (star_chart_viewer_.has_pending_action()) {
            travel_to_destination(star_chart_viewer_.consume_action());
        }
        return;
    }

    // NPC dialog intercepts input when open
    if (npc_dialog_.is_open()) {
        // Tab = trade shortcut
        if (key == '\t') {
            if (interacting_npc_ && interacting_npc_->interactions.shop) {
                npc_dialog_.close();
                trade_window_.open(interacting_npc_, &player_, renderer_.get());
            } else {
                log("They have nothing to sell.");
            }
            return;
        }
        // [l] Look — enter look mode focused on NPC
        if (key == 'l' && interacting_npc_) {
            npc_dialog_.close();
            input_.begin_look_at(interacting_npc_->x, interacting_npc_->y);
            interacting_npc_ = nullptr;
            dialog_tree_ = nullptr;
            dialog_node_ = -1;
            return;
        }
        MenuResult result = npc_dialog_.handle_input(key);
        if (result == MenuResult::Selected) {
            advance_dialog(npc_dialog_.selected());
        } else if (result == MenuResult::Closed) {
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

    // Look mode intercept
    if (input_.looking()) {
        input_.handle_look_input(key, world_.map().width(), world_.map().height());
        return;
    }

    // Targeting mode intercept
    if (targeting_) {
        handle_targeting_input(key);
        return;
    }

    // Awaiting interact direction (space + direction)
    if (awaiting_interact_) {
        awaiting_interact_ = false;
        switch (key) {
            case 'k': case KEY_UP:    use_at(player_.x,     player_.y - 1); return;
            case 'j': case KEY_DOWN:  use_at(player_.x,     player_.y + 1); return;
            case 'h': case KEY_LEFT:  use_at(player_.x - 1, player_.y);     return;
            case 'l': case KEY_RIGHT: use_at(player_.x + 1, player_.y);     return;
            default:
                log("Cancelled.");
                return;
        }
    }

    switch (key) {
        case '\033':
            pause_menu_.close();
            pause_menu_.set_title("Menu");
            pause_menu_.add_option('r', "return to game");
            pause_menu_.add_option('h', "help");
            pause_menu_.add_option('s', "save game");
            pause_menu_.add_option('l', "load game");
            pause_menu_.add_option('o', "options");
            pause_menu_.add_option('q', "quit");
            pause_menu_.open();
            break;
        case ' ':
            if (on_overworld()) {
                Tile t = world_.map().get(player_.x, player_.y);
                if (t == Tile::OW_Landing) {
                    enter_detail_map();
                } else {
                    log("Nothing to interact with here.");
                }
                break;
            }
            use_action();
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
            recompute_fov();
            break;
        case 'l':
            input_.begin_look(player_.x, player_.y);
            log("Look mode. Move cursor to examine. [Esc] to exit.");
            break;
        case 't': begin_targeting(); break;
        case 's': shoot_target(); break;
        case 'r': reload_weapon(); break;
        case 'g': pickup_ground_item(); break;
        case 'c': character_screen_.open(&player_, renderer_.get()); break;
        case '?': help_screen_.open(); break;
        case 'm':
            if (dev_mode_) {
                star_chart_viewer_.open();
            }
            break;
        case '+': case '=': {
            auto tab = static_cast<PanelTab>(active_tab_);
            if (tab == PanelTab::Inventory) {
                int count = static_cast<int>(player_.inventory.items.size());
                if (count > 0 && inventory_cursor_ < count - 1) ++inventory_cursor_;
            } else if (tab == PanelTab::Equipment) {
                if (inventory_cursor_ < equip_slot_count - 1) ++inventory_cursor_;
            } else if (tab == PanelTab::Wait) {
                if (wait_cursor_ < 5) ++wait_cursor_;
            }
            break;
        }
        case '-': {
            auto tab = static_cast<PanelTab>(active_tab_);
            if (tab == PanelTab::Inventory || tab == PanelTab::Equipment) {
                if (inventory_cursor_ > 0) --inventory_cursor_;
            } else if (tab == PanelTab::Wait) {
                if (wait_cursor_ > 0) --wait_cursor_;
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
        case '>': {
            if (on_overworld()) {
                Tile t = world_.map().get(player_.x, player_.y);
                if (t == Tile::OW_Mountains || t == Tile::OW_Lake) {
                    log("This terrain cannot be explored on foot.");
                } else {
                    enter_detail_map();
                }
            } else if (on_detail_map()) {
                // Check for Portal tile to enter dungeon
                Tile t = world_.map().get(player_.x, player_.y);
                if (t == Tile::Portal) {
                    enter_dungeon_from_detail();
                }
            }
            break;
        }
        case '<': {
            if (on_detail_map()) {
                exit_detail_to_overworld();
            } else if (surface_mode_ == SurfaceMode::Dungeon &&
                       !navigation_.at_station && !navigation_.on_ship) {
                exit_dungeon_to_detail();
            }
            break;
        }
        case '\n': case '\r':
        case '1': case '2': case '3': case '4': case '5': case '6': {
            auto tab = static_cast<PanelTab>(active_tab_);
            // Number keys only apply to Wait tab
            if (key >= '1' && key <= '6' && tab != PanelTab::Wait) break;
            // Overworld: enter detail map for the tile underneath the player
            if (on_overworld() && (key == '\n' || key == '\r')) {
                Tile t = world_.map().get(player_.x, player_.y);
                if (t == Tile::OW_Mountains || t == Tile::OW_Lake) {
                    log("This terrain cannot be explored on foot.");
                } else {
                    enter_detail_map();
                }
                break;
            }
            if (tab == PanelTab::Wait && (key == '\n' || key == '\r' || (key >= '1' && key <= '6'))) {
                if (key >= '1' && key <= '6') wait_cursor_ = key - '1';
                int old_hp = player_.hp;
                int turns = 0;
                bool interrupted = false;
                auto do_wait = [&](int n) {
                    for (int i = 0; i < n; ++i) {
                        advance_world(ActionCost::wait);
                        ++turns;
                        if (player_.hp < old_hp) {
                            interrupted = true;
                            break;
                        }
                        old_hp = player_.hp;
                    }
                };
                switch (wait_cursor_) {
                    case 0: do_wait(1); break;
                    case 1: do_wait(10); break;
                    case 2: do_wait(50); break;
                    case 3: do_wait(100); break;
                    case 4: { // Wait until healed
                        int limit = 1000;
                        while (player_.hp < player_.max_hp && limit-- > 0) {
                            if (player_.hunger == HungerState::Starving) {
                                log("Too hungry to rest.");
                                break;
                            }
                            advance_world(ActionCost::wait);
                            ++turns;
                            if (player_.hp < old_hp) { interrupted = true; break; }
                            old_hp = player_.hp;
                        }
                        break;
                    }
                    case 5: { // Wait until morning (full daylight)
                        if (day_clock_.phase() == TimePhase::Day) break;
                        int limit = day_clock_.local_ticks_per_day + 10;
                        while (day_clock_.phase() != TimePhase::Day && limit-- > 0) {
                            advance_world(ActionCost::wait);
                            ++turns;
                            if (player_.hp < old_hp) { interrupted = true; break; }
                            old_hp = player_.hp;
                        }
                        break;
                    }
                }
                if (interrupted) {
                    log("Your rest is interrupted!");
                } else if (turns > 1) {
                    log("You wait " + std::to_string(turns) + " turns.");
                } else if (turns == 1) {
                    log("You wait...");
                }
                recompute_fov();
                break;
            }
            if (tab == PanelTab::Inventory) {
                int count = static_cast<int>(player_.inventory.items.size());
                if (count > 0 && inventory_cursor_ < count) {
                    const auto& item = player_.inventory.items[inventory_cursor_];
                    if (item.slot.has_value()) {
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
        case KEY_RIGHT:           try_move( 1,  0); break;
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
    ground_items_.clear();
    world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    recompute_fov();
    compute_camera();
    current_region_ = -1;
    surface_mode_ = SurfaceMode::Dungeon;

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
    ground_items_.clear();

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
    current_region_ = -1;
    surface_mode_ = SurfaceMode::Dungeon;

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

    seed_ = static_cast<unsigned>(std::time(nullptr));
    rng_.seed(seed_);

    auto props = default_properties(MapType::SpaceStation);
    props.height = 80; // hub needs extra vertical space for 3-row grid
    world_.map() = TileMap(props.width, props.height, MapType::SpaceStation);
    auto gen = create_hub_generator();
    gen->generate(world_.map(), props, seed_);
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
    ground_items_.clear();
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
    current_region_ = -1;
    active_tab_ = 0; // Start on Messages tab
    surface_mode_ = SurfaceMode::Dungeon;
    overworld_x_ = 0;
    overworld_y_ = 0;
    world_tick_ = 0;
    day_clock_ = DayClock{};  // station day = 200 ticks
    location_cache_.clear();
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

    Item weapon = random_ranged_weapon(rng_);
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
    navigation_ = generate_galaxy(seed_);
    navigation_.at_station = true;
    navigation_.current_body_index = -1;
    star_chart_viewer_ = StarChartViewer(&navigation_, renderer_.get());

    state_ = GameState::Playing;
}

void Game::new_game(const CreationResult& cr) {
    compute_layout();

    // Boot sequence
    BootSequence boot(renderer_.get());
    boot.play();

    seed_ = static_cast<unsigned>(std::time(nullptr));
    rng_.seed(seed_);

    auto props = default_properties(MapType::SpaceStation);
    props.height = 80;
    world_.map() = TileMap(props.width, props.height, MapType::SpaceStation);
    auto gen = create_hub_generator();
    gen->generate(world_.map(), props, seed_);
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
    ground_items_.clear();
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
    current_region_ = -1;
    active_tab_ = 0;
    surface_mode_ = SurfaceMode::Dungeon;
    overworld_x_ = 0;
    overworld_y_ = 0;
    world_tick_ = 0;
    day_clock_ = DayClock{};
    location_cache_.clear();

    log("Welcome aboard, " + cr.name + ". Your journey to Sgr A* begins.");
    log("You are docked at The Heavens Above, the space station orbiting Jupiter.");
    show_welcome_ = true;
    check_region_change();

    // Starter gear: random ranged weapon + batteries
    Item weapon = random_ranged_weapon(rng_);
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
    navigation_ = generate_galaxy(seed_);
    navigation_.at_station = true;
    navigation_.current_body_index = -1;
    star_chart_viewer_ = StarChartViewer(&navigation_, renderer_.get());

    state_ = GameState::Playing;
}

void Game::save_current_location() {
    LocationKey key;
    if (navigation_.on_ship) {
        key = ship_key_;
    } else if (navigation_.at_station) {
        key = {navigation_.current_system_id, -1, -1, true, -1, -1, 0};
    } else if (on_overworld()) {
        key = {navigation_.current_system_id, navigation_.current_body_index,
               navigation_.current_moon_index, false, -1, -1, 0};
    } else if (on_detail_map()) {
        key = {navigation_.current_system_id, navigation_.current_body_index,
               navigation_.current_moon_index, false, overworld_x_, overworld_y_, 0};
    } else {
        // Dungeon (depth=1)
        key = {navigation_.current_system_id, navigation_.current_body_index,
               navigation_.current_moon_index, false, overworld_x_, overworld_y_, 1};
    }
    LocationState& state = location_cache_[key];
    state.map = std::move(world_.map());
    state.visibility = std::move(world_.visibility());
    state.npcs = std::move(world_.npcs());
    state.ground_items = std::move(ground_items_);
    state.player_x = player_.x;
    state.player_y = player_.y;
}

void Game::restore_location(const LocationKey& key) {
    auto it = location_cache_.find(key);
    if (it == location_cache_.end()) return;
    LocationState& state = it->second;
    world_.map() = std::move(state.map);
    world_.visibility() = std::move(state.visibility);
    world_.npcs() = std::move(state.npcs);
    ground_items_ = std::move(state.ground_items);

    if (key == ship_key_) {
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

    location_cache_.erase(it);
}

void Game::enter_ship() {
    save_current_location();
    navigation_.on_ship = true;
    surface_mode_ = SurfaceMode::Dungeon;

    if (location_cache_.count(ship_key_)) {
        restore_location(ship_key_);
    } else {
        // Generate the ship for the first time
        unsigned ship_seed = seed_ ^ 0x5B1Bu;
        auto props = default_properties(MapType::Starship);
        world_.map() = TileMap(props.width, props.height, MapType::Starship);
        auto gen = create_starship_generator();
        gen->generate(world_.map(), props, ship_seed);
        world_.map().set_location_name("Your Starship");

        world_.npcs().clear();
        ground_items_.clear();
        // Spawn in region 0 (cockpit)
        if (!world_.map().find_open_spot_in_region(0, player_.x, player_.y, {})) {
            world_.map().find_open_spot(player_.x, player_.y);
        }

        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    world_.visibility().reveal_all();
    current_region_ = -1;
    compute_camera();
    check_region_change();
    log("You board your starship.");
}

void Game::enter_overworld_tile() {
    Tile tile = world_.map().get(player_.x, player_.y);
    overworld_x_ = player_.x;
    overworld_y_ = player_.y;

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
    LocationKey detail_key = {navigation_.current_system_id,
                              navigation_.current_body_index,
                              navigation_.current_moon_index,
                              false, overworld_x_, overworld_y_, 1};

    // Save overworld before entering detail
    save_current_location();
    surface_mode_ = SurfaceMode::Dungeon;

    if (location_cache_.count(detail_key)) {
        restore_location(detail_key);
    } else {
        // Generate detail map
        unsigned detail_seed = seed_
            ^ (navigation_.current_system_id * 7919u)
            ^ (static_cast<unsigned>(navigation_.current_body_index) * 6271u)
            ^ (static_cast<unsigned>(navigation_.current_moon_index + 1) * 3571u)
            ^ (static_cast<unsigned>(overworld_x_) * 1013u)
            ^ (static_cast<unsigned>(overworld_y_) * 2039u);

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
        ground_items_.clear();
        world_.map().find_open_spot(player_.x, player_.y);

        // Spawn NPCs
        std::mt19937 npc_rng(detail_seed ^ 0xD3ADu);
        std::vector<std::pair<int,int>> occupied = {{player_.x, player_.y}};
        debug_spawn(world_.map(), world_.npcs(), player_.x, player_.y, occupied, npc_rng);

        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    current_region_ = -1;
    recompute_fov();
    compute_camera();
    check_region_change();
    log(enter_msg);
}

void Game::exit_to_overworld() {
    // Save detail map
    save_current_location();

    // Restore overworld
    LocationKey ow_key = {navigation_.current_system_id,
                          navigation_.current_body_index,
                          navigation_.current_moon_index,
                          false, -1, -1, 0};

    if (location_cache_.count(ow_key)) {
        restore_location(ow_key);
    }

    player_.x = overworld_x_;
    player_.y = overworld_y_;
    surface_mode_ = SurfaceMode::Overworld;
    world_.visibility().reveal_all();
    current_region_ = -1;
    compute_camera();
    log("You return to the surface.");
}

MapProperties Game::build_detail_props(int ow_x, int ow_y) {
    auto props = default_properties(MapType::DetailMap);

    // Read overworld from cache
    LocationKey ow_key = {navigation_.current_system_id,
                          navigation_.current_body_index,
                          navigation_.current_moon_index,
                          false, -1, -1, 0};

    const TileMap* ow_map = nullptr;
    if (on_overworld()) {
        ow_map = &world_.map();
    } else {
        auto it = location_cache_.find(ow_key);
        if (it != location_cache_.end())
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
    overworld_x_ = player_.x;
    overworld_y_ = player_.y;

    auto props = build_detail_props(overworld_x_, overworld_y_);
    std::string body_name = world_.map().location_name();

    LocationKey detail_key = {navigation_.current_system_id,
                              navigation_.current_body_index,
                              navigation_.current_moon_index,
                              false, overworld_x_, overworld_y_, 0};

    save_current_location();
    surface_mode_ = SurfaceMode::DetailMap;

    if (location_cache_.count(detail_key)) {
        restore_location(detail_key);
    } else {
        unsigned detail_seed = seed_
            ^ (navigation_.current_system_id * 7919u)
            ^ (static_cast<unsigned>(navigation_.current_body_index) * 6271u)
            ^ (static_cast<unsigned>(navigation_.current_moon_index + 1) * 3571u)
            ^ (static_cast<unsigned>(overworld_x_) * 1013u)
            ^ (static_cast<unsigned>(overworld_y_) * 2039u);

        world_.map() = TileMap(props.width, props.height, MapType::DetailMap);
        auto gen = create_generator(MapType::DetailMap);
        gen->generate(world_.map(), props, detail_seed);
        world_.map().set_biome(props.biome);
        world_.map().set_location_name(body_name);

        world_.npcs().clear();
        ground_items_.clear();

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

    current_region_ = -1;
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

    LocationKey ow_key = {navigation_.current_system_id,
                          navigation_.current_body_index,
                          navigation_.current_moon_index,
                          false, -1, -1, 0};

    if (location_cache_.count(ow_key)) {
        restore_location(ow_key);
    }

    player_.x = overworld_x_;
    player_.y = overworld_y_;
    surface_mode_ = SurfaceMode::Overworld;
    world_.visibility().reveal_all();
    current_region_ = -1;
    compute_camera();
    log("You return to the surface view.");
}

void Game::enter_dungeon_from_detail() {
    // Save detail map (depth=0)
    save_current_location();

    // Determine dungeon type from overworld tile
    LocationKey ow_key = {navigation_.current_system_id,
                          navigation_.current_body_index,
                          navigation_.current_moon_index,
                          false, -1, -1, 0};

    Tile ow_tile = Tile::OW_CaveEntrance;
    Biome ow_biome = world_.map().biome();
    auto it = location_cache_.find(ow_key);
    if (it != location_cache_.end()) {
        ow_tile = it->second.map.get(overworld_x_, overworld_y_);
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

    LocationKey dungeon_key = {navigation_.current_system_id,
                               navigation_.current_body_index,
                               navigation_.current_moon_index,
                               false, overworld_x_, overworld_y_, 1};

    surface_mode_ = SurfaceMode::Dungeon;

    if (location_cache_.count(dungeon_key)) {
        restore_location(dungeon_key);
    } else {
        unsigned detail_seed = seed_
            ^ (navigation_.current_system_id * 7919u)
            ^ (static_cast<unsigned>(navigation_.current_body_index) * 6271u)
            ^ (static_cast<unsigned>(navigation_.current_moon_index + 1) * 3571u)
            ^ (static_cast<unsigned>(overworld_x_) * 1013u)
            ^ (static_cast<unsigned>(overworld_y_) * 2039u)
            ^ 0xDE3Du;

        auto props = default_properties(detail_type);
        props.biome = detail_biome;
        world_.map() = TileMap(props.width, props.height, detail_type);
        auto gen = create_generator(detail_type);
        gen->generate(world_.map(), props, detail_seed);
        world_.map().set_location_name(body_name);

        world_.npcs().clear();
        ground_items_.clear();
        world_.map().find_open_spot(player_.x, player_.y);

        std::mt19937 npc_rng(detail_seed ^ 0xD3ADu);
        std::vector<std::pair<int,int>> occupied = {{player_.x, player_.y}};
        debug_spawn(world_.map(), world_.npcs(), player_.x, player_.y, occupied, npc_rng);

        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    current_region_ = -1;
    recompute_fov();
    compute_camera();
    check_region_change();
    log(enter_msg);
}

void Game::exit_dungeon_to_detail() {
    // Save dungeon (depth=1)
    save_current_location();

    // Restore detail map (depth=0)
    LocationKey detail_key = {navigation_.current_system_id,
                              navigation_.current_body_index,
                              navigation_.current_moon_index,
                              false, overworld_x_, overworld_y_, 0};

    surface_mode_ = SurfaceMode::DetailMap;

    if (location_cache_.count(detail_key)) {
        restore_location(detail_key);
    } else {
        // Detail map was never cached — generate it
        auto props = build_detail_props(overworld_x_, overworld_y_);
        unsigned detail_seed = seed_
            ^ (navigation_.current_system_id * 7919u)
            ^ (static_cast<unsigned>(navigation_.current_body_index) * 6271u)
            ^ (static_cast<unsigned>(navigation_.current_moon_index + 1) * 3571u)
            ^ (static_cast<unsigned>(overworld_x_) * 1013u)
            ^ (static_cast<unsigned>(overworld_y_) * 2039u);

        world_.map() = TileMap(props.width, props.height, MapType::DetailMap);
        auto gen = create_generator(MapType::DetailMap);
        gen->generate(world_.map(), props, detail_seed);
        world_.map().set_biome(props.biome);

        world_.npcs().clear();
        ground_items_.clear();
        world_.map().find_open_spot(player_.x, player_.y);
        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    current_region_ = -1;
    recompute_fov();
    compute_camera();
    check_region_change();
    log("You return to the surface.");
}

void Game::transition_detail_edge(int dx, int dy) {
    // Find the overworld map from cache
    LocationKey ow_key = {navigation_.current_system_id,
                          navigation_.current_body_index,
                          navigation_.current_moon_index,
                          false, -1, -1, 0};

    auto ow_it = location_cache_.find(ow_key);
    if (ow_it == location_cache_.end()) {
        log("You can't go that way.");
        return;
    }

    const auto& ow_map = ow_it->second.map;
    int new_ow_x = overworld_x_ + dx;
    int new_ow_y = overworld_y_ + dy;

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
    overworld_x_ = new_ow_x;
    overworld_y_ = new_ow_y;

    // Generate or restore the new detail map
    auto props = build_detail_props(new_ow_x, new_ow_y);

    LocationKey new_detail_key = {navigation_.current_system_id,
                                  navigation_.current_body_index,
                                  navigation_.current_moon_index,
                                  false, new_ow_x, new_ow_y, 0};

    if (location_cache_.count(new_detail_key)) {
        restore_location(new_detail_key);
    } else {
        unsigned detail_seed = seed_
            ^ (navigation_.current_system_id * 7919u)
            ^ (static_cast<unsigned>(navigation_.current_body_index) * 6271u)
            ^ (static_cast<unsigned>(navigation_.current_moon_index + 1) * 3571u)
            ^ (static_cast<unsigned>(new_ow_x) * 1013u)
            ^ (static_cast<unsigned>(new_ow_y) * 2039u);

        world_.map() = TileMap(props.width, props.height, MapType::DetailMap);
        auto gen = create_generator(MapType::DetailMap);
        gen->generate(world_.map(), props, detail_seed);
        world_.map().set_biome(props.biome);

        world_.npcs().clear();
        ground_items_.clear();
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
    ow_it = location_cache_.find(ow_key);
    if (ow_it != location_cache_.end()) {
        ow_it->second.player_x = new_ow_x;
        ow_it->second.player_y = new_ow_y;
    }

    current_region_ = -1;
    recompute_fov();
    compute_camera();
    check_region_change();
    advance_world(15);
}

void Game::travel_to_destination(const ChartAction& action) {
    if (action.system_index < 0 ||
        action.system_index >= static_cast<int>(navigation_.systems.size()))
        return;

    auto& target_sys = navigation_.systems[action.system_index];
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
            navigation_.current_system_id = target_sys.id;
            discover_nearby(navigation_, target_sys.id, 20.0f);
            navigation_.on_ship = true;
            navigation_.at_station = false;
            navigation_.current_body_index = -1;
            navigation_.current_moon_index = -1;

            if (location_cache_.count(ship_key_)) {
                restore_location(ship_key_);
            } else {
                // Generate ship for the first time
                unsigned ship_seed = seed_ ^ 0x5B1Bu;
                auto props = default_properties(MapType::Starship);
                world_.map() = TileMap(props.width, props.height, MapType::Starship);
                auto gen = create_starship_generator();
                gen->generate(world_.map(), props, ship_seed);
                world_.map().set_location_name("Your Starship");
                world_.npcs().clear();
                ground_items_.clear();
                if (!world_.map().find_open_spot_in_region(0, player_.x, player_.y, {})) {
                    world_.map().find_open_spot(player_.x, player_.y);
                }
                world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
            }

            world_.visibility().reveal_all();
            current_region_ = -1;
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
            unsigned biome_seed = seed_ ^ (target_sys.id * 997u)
                                ^ (static_cast<unsigned>(action.body_index) * 6271u);

            if (action.moon_index >= 0) {
                // Generate independent moon body
                unsigned moon_seed = seed_ ^ (target_sys.id * 7919u)
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
            navigation_.on_ship = false;
            navigation_.at_station = true;
            navigation_.current_body_index = -1;
            navigation_.current_moon_index = -1;
            day_clock_.set_body_day_length(200); // station standard day
            break;
        case ChartActionType::TravelToBody: {
            navigation_.on_ship = false;
            navigation_.at_station = false;
            navigation_.current_body_index = action.body_index;
            navigation_.current_moon_index = action.moon_index;
            const auto& body = target_sys.bodies[action.body_index];
            if (action.moon_index >= 0) {
                unsigned moon_seed = seed_ ^ (target_sys.id * 7919u)
                                   ^ (static_cast<unsigned>(action.body_index) * 6271u);
                auto moon = generate_moon_body(body, action.moon_index, moon_seed);
                day_clock_.set_body_day_length(moon.day_length);
            } else {
                day_clock_.set_body_day_length(body.day_length);
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
        if (location_cache_.count(dest_key)) {
            restore_location(dest_key);
        } else {
            // Generate overworld from body or moon properties
            auto props = default_properties(MapType::Overworld);
            props.biome = dest_biome;

            if (action.moon_index >= 0) {
                unsigned moon_seed = seed_ ^ (target_sys.id * 7919u)
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

            unsigned ow_seed = seed_ ^ (target_sys.id * 7919u)
                             ^ (static_cast<unsigned>(action.body_index) * 6271u)
                             ^ (static_cast<unsigned>(action.moon_index + 1) * 3571u);

            world_.map() = TileMap(props.width, props.height, MapType::Overworld);
            auto gen = create_generator(MapType::Overworld);
            gen->generate(world_.map(), props, ow_seed);
            world_.map().set_biome(dest_biome);
            world_.map().set_location_name(location_name);

            world_.npcs().clear();
            ground_items_.clear();

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

        surface_mode_ = SurfaceMode::Overworld;
        overworld_x_ = 0;
        overworld_y_ = 0;
        world_.visibility().reveal_all();
        current_region_ = -1;
        compute_camera();
        log("You land on " + colored(location_name, Color::Cyan)
            + ". The surface stretches before you.");
        return;
    }

    // Station destinations
    if (location_cache_.count(dest_key)) {
        restore_location(dest_key);
    } else {
        // Generate fresh map
        unsigned travel_seed = rng_();
        auto props = default_properties(dest_type);
        props.biome = dest_biome;
        world_.map() = TileMap(props.width, props.height, dest_type);
        auto gen = create_generator(dest_type);
        gen->generate(world_.map(), props, travel_seed);
        world_.map().set_location_name(location_name);

        world_.npcs().clear();
        ground_items_.clear();
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
            if (world_.map().find_open_spot_near(player_.x, player_.y, tx, ty, occupied, &rng_)) {
                world_.map().add_fixture(tx, ty, make_fixture(FixtureType::ShipTerminal));
            }
        }
    }

    surface_mode_ = SurfaceMode::Dungeon;
    current_region_ = -1;
    recompute_fov();
    compute_camera();
    check_region_change();
    log("You dock at " + colored(location_name, Color::Cyan) + ".");
}

void Game::try_move(int dx, int dy) {
    int nx = player_.x + dx;
    int ny = player_.y + dy;

    // Detail map: edge transitions
    if (on_detail_map()) {
        if (nx < 0 || nx >= world_.map().width() || ny < 0 || ny >= world_.map().height()) {
            int ddx = (nx < 0) ? -1 : (nx >= world_.map().width()) ? 1 : 0;
            int ddy = (ny < 0) ? -1 : (ny >= world_.map().height()) ? 1 : 0;
            transition_detail_edge(ddx, ddy);
            return;
        }
        // Fall through to standard dungeon movement below
    }

    // Overworld: simplified movement
    if (on_overworld()) {
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
        auto msg = random_bump_message(world_.map().get(nx, ny), world_.map().map_type(), rng_);
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
        surface_mode_ == SurfaceMode::Dungeon && !navigation_.at_station && !navigation_.on_ship) {
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
    if (rid == current_region_ || rid < 0) return;

    current_region_ = rid;
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
                interact_fixture(fid);
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
    open_npc_dialog(*target);
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
        for (const auto& gi : ground_items_) {
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
        for (size_t i = 0; i < ground_items_.size(); ++i) {
            if (ground_items_[i].x == tx && ground_items_[i].y == ty) {
                pickup_ground_item();
                return;
            }
        }
        // Portal / stairs
        Tile t = world_.map().get(tx, ty);
        if (t == Tile::Portal) {
            if (on_detail_map()) {
                enter_dungeon_from_detail();
            } else if (surface_mode_ == SurfaceMode::Dungeon) {
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

void Game::interact_fixture(int fid) {
    auto& f = world_.map().fixture_mut(fid);

    switch (f.type) {
        case FixtureType::HealPod: {
            if (f.last_used_tick >= 0 && f.cooldown > 0) {
                int elapsed = world_tick_ - f.last_used_tick;
                if (elapsed < f.cooldown) {
                    int remaining = f.cooldown - elapsed;
                    log("The healing pod is recharging (" +
                        std::to_string(remaining) + " ticks remaining).");
                    return;
                }
            }
            if (player_.hp >= player_.max_hp) {
                log("You step into the healing pod. No injuries detected.");
                return;
            }
            player_.hp = player_.max_hp;
            f.last_used_tick = world_tick_;
            log("Nanites flood the pod. Your wounds close and vitals normalize. Fully healed.");
            break;
        }
        case FixtureType::FoodTerminal: {
            auto menu = food_terminal_menu();
            npc_dialog_.close();
            npc_dialog_.set_title("Food Terminal");
            npc_dialog_body_ = "What'll it be?";
            log("Food Terminal: What'll it be?");
            char fkey = '1';
            for (const auto& entry : menu) {
                std::string label = entry.label + " (" +
                    std::to_string(entry.cost) + "$";
                if (entry.to_inventory) {
                    label += ", for inventory";
                } else if (entry.heal < 0) {
                    label += ", full heal";
                } else {
                    label += ", +" + std::to_string(entry.heal) + " HP";
                }
                label += ")";
                npc_dialog_.add_option(fkey++, label);
            }
            npc_dialog_.add_option('q', "Leave");
            npc_dialog_.set_max_width_frac(0.4f);
            npc_dialog_.open();
            interacting_npc_ = nullptr;
            dialog_tree_ = nullptr;
            dialog_node_ = -2; // sentinel: food terminal mode
            break;
        }
        case FixtureType::RestPod: {
            if (f.last_used_tick >= 0 && f.cooldown > 0) {
                int elapsed = world_tick_ - f.last_used_tick;
                if (elapsed < f.cooldown) {
                    int remaining = f.cooldown - elapsed;
                    log("The rest pod needs to reset (" +
                        std::to_string(remaining) + " ticks remaining).");
                    return;
                }
            }
            player_.hp = player_.max_hp;
            f.last_used_tick = world_tick_;
            advance_world(20);
            log("You climb into the rest pod and sleep deeply. Fully restored.");
            break;
        }
        case FixtureType::RepairBench: {
            log("A sign reads: 'Under maintenance. Check back later.'");
            break;
        }
        case FixtureType::SupplyLocker: {
            npc_dialog_.close();
            npc_dialog_.set_title("Supply Locker");
            npc_dialog_body_ = "Stash (" + std::to_string(stash_.size()) + "/" +
                std::to_string(max_stash_size_) + ")";
            log(npc_dialog_body_);
            npc_dialog_.add_option('s', "Store an item");
            npc_dialog_.add_option('r', "Retrieve an item");
            npc_dialog_.add_option('c', "Close");
            npc_dialog_.open();
            interacting_npc_ = nullptr;
            dialog_tree_ = nullptr;
            dialog_node_ = -3; // sentinel: stash main menu
            break;
        }
        case FixtureType::StarChart: {
            star_chart_viewer_.open();
            log("The star chart hums to life, projecting a holographic galaxy map.");
            break;
        }
        case FixtureType::WeaponDisplay: {
            log("Weapons gleam behind reinforced glass. Talk to the Arms Dealer to browse.");
            break;
        }
        case FixtureType::ShipTerminal: {
            npc_dialog_.close();
            npc_dialog_.set_title("Shipping Terminal");
            npc_dialog_body_ = "Your starship is docked and ready.";
            log(npc_dialog_body_);
            npc_dialog_.add_option('b', "Board ship");
            npc_dialog_.add_option('c', "Cancel");
            npc_dialog_.open();
            interacting_npc_ = nullptr;
            dialog_tree_ = nullptr;
            dialog_node_ = -6; // sentinel: ship terminal
            break;
        }
        case FixtureType::Door: {
            if (f.locked) {
                log("The door is locked.");
                break;
            }
            if (f.open) {
                f.open = false;
                f.passable = false;
                f.glyph = '+';
                f.utf8_glyph = nullptr;
                log("You close the door.");
            } else {
                f.open = true;
                f.passable = true;
                f.glyph = '/';
                f.utf8_glyph = nullptr;
                log("You open the door.");
            }
            break;
        }
        default:
            log("Nothing happens.");
            break;
    }
}

void Game::open_npc_dialog(Npc& npc) {
    interacting_npc_ = &npc;
    dialog_tree_ = nullptr;
    dialog_node_ = -1;
    interact_options_.clear();

    const auto& data = npc.interactions;
    npc_dialog_.close();
    npc_dialog_.set_title(npc.display_name());
    npc_dialog_body_ = data.talk ? data.talk->greeting : "";
    if (!npc_dialog_body_.empty()) {
        log(npc.display_name() + ": \"" + npc_dialog_body_ + "\"");
    }

    char hotkey = '1';
    if (data.talk && !data.talk->nodes.empty()) {
        npc_dialog_.add_option(hotkey++, "Talk");
        interact_options_.push_back(InteractOption::Talk);
    }
    if (data.shop) {
        npc_dialog_.add_option(hotkey++, "Trade");
        interact_options_.push_back(InteractOption::Shop);
    }
    if (data.quest) {
        npc_dialog_.add_option(hotkey++, data.quest->quest_intro);
        interact_options_.push_back(InteractOption::Quest);
    }
    npc_dialog_.add_option('f', "Farewell");
    interact_options_.push_back(InteractOption::Farewell);

    npc_dialog_.set_footer("[Space] Select  [Tab] Trade  [l] Look  [Esc] Close");
    npc_dialog_.set_max_width_frac(0.35f);
    npc_dialog_.open();
}

void Game::advance_dialog(int selected) {
    // Food terminal dialog (no NPC involved)
    if (dialog_node_ == -2) {
        dialog_node_ = -1;
        dialog_tree_ = nullptr;
        auto menu = food_terminal_menu();
        if (selected >= 0 && selected < static_cast<int>(menu.size())) {
            auto result = buy_food_item(player_, menu[selected]);
            log(result.message);
        }
        // last option or out of range: Leave
        return;
    }

    // Stash main menu
    if (dialog_node_ == -3) {
        if (selected == 0) {
            // Store mode — show player inventory
            if (player_.inventory.items.empty()) {
                log("You have nothing to store.");
                dialog_node_ = -1;
                return;
            }
            if (static_cast<int>(stash_.size()) >= max_stash_size_) {
                log("Stash is full! (" + std::to_string(max_stash_size_) +
                    "/" + std::to_string(max_stash_size_) + ")");
                dialog_node_ = -1;
                return;
            }
            npc_dialog_.close();
            npc_dialog_.set_title("Store Item");
            log("Select item to store (" + std::to_string(stash_.size()) +
                "/" + std::to_string(max_stash_size_) + "):");
            { char sk = '1';
            for (const auto& item : player_.inventory.items) {
                npc_dialog_.add_option(sk++, item.name);
                if (sk > '9') sk = 'a';
            } }
            npc_dialog_.add_option('c', "Cancel");
            npc_dialog_.set_max_width_frac(0.35f);
            npc_dialog_.open();
            dialog_node_ = -4; // sentinel: store mode
        } else if (selected == 1) {
            // Retrieve mode — show stash contents
            if (stash_.empty()) {
                log("The stash is empty.");
                dialog_node_ = -1;
                return;
            }
            npc_dialog_.close();
            npc_dialog_.set_title("Retrieve Item");
            log("Select item to retrieve (" + std::to_string(stash_.size()) +
                "/" + std::to_string(max_stash_size_) + "):");
            { char rk = '1';
            for (const auto& item : stash_) {
                npc_dialog_.add_option(rk++, item.name);
                if (rk > '9') rk = 'a';
            } }
            npc_dialog_.add_option('c', "Cancel");
            npc_dialog_.set_max_width_frac(0.35f);
            npc_dialog_.open();
            dialog_node_ = -5; // sentinel: retrieve mode
        } else {
            // Close
            dialog_node_ = -1;
        }
        return;
    }

    // Stash store mode — player selected an inventory item to store
    if (dialog_node_ == -4) {
        dialog_node_ = -1;
        int item_count = static_cast<int>(player_.inventory.items.size());
        if (selected >= 0 && selected < item_count) {
            Item stored = std::move(player_.inventory.items[selected]);
            player_.inventory.items.erase(
                player_.inventory.items.begin() + selected);
            log("Stored " + stored.name + " in the stash.");
            stash_.push_back(std::move(stored));
        }
        return;
    }

    // Stash retrieve mode — player selected a stash item to retrieve
    if (dialog_node_ == -5) {
        dialog_node_ = -1;
        int stash_count = static_cast<int>(stash_.size());
        if (selected >= 0 && selected < stash_count) {
            Item retrieved = std::move(stash_[selected]);
            stash_.erase(stash_.begin() + selected);
            if (!player_.inventory.can_add(retrieved)) {
                log("Too heavy! Can't carry " + retrieved.name + ".");
                stash_.insert(stash_.begin() + selected, std::move(retrieved));
            } else {
                log("Retrieved " + retrieved.name + " from the stash.");
                player_.inventory.items.push_back(std::move(retrieved));
            }
        }
        return;
    }

    // Dev menu — main
    if (dialog_node_ == -7) {
        dialog_node_ = -1;
        dialog_tree_ = nullptr;
        if (selected == 0) {
            dev_warp_random();
        } else if (selected == 1) {
            // POI Stamp Test submenu
            npc_dialog_.close();
            npc_dialog_.set_title("[DEV] POI Stamp Test");
            npc_dialog_.add_option('1', "Ruins");
            npc_dialog_.add_option('2', "Crashed Ship");
            npc_dialog_.add_option('3', "Outpost");
            npc_dialog_.add_option('4', "Cave Entrance");
            npc_dialog_.add_option('5', "Settlement");
            npc_dialog_.add_option('6', "Landing Pad");
            npc_dialog_.add_option('b', "Back");
            npc_dialog_.open();
            dialog_node_ = -9; // sentinel: stamp test menu
        } else if (selected == 2) {
            // Open character stats submenu
            npc_dialog_.close();
            npc_dialog_.set_title("[DEV] Character Stats");
            npc_dialog_.add_option('i', std::string("Invulnerability: ") +
                                   (has_effect(player_.effects, EffectId::Invulnerable) ? "ON" : "OFF"));
            npc_dialog_.add_option('b', "Back");
            npc_dialog_.open();
            dialog_node_ = -8; // sentinel: dev stats
        } else if (selected == 3) {
            // Force level up
            player_.xp = player_.max_xp;
            check_level_up();
        }
        return;
    }

    // Dev menu — character stats
    if (dialog_node_ == -8) {
        if (selected == 0) {
            if (has_effect(player_.effects, EffectId::Invulnerable))
                remove_effect(player_.effects, EffectId::Invulnerable);
            else
                add_effect(player_.effects, make_invulnerable());
            bool inv = has_effect(player_.effects, EffectId::Invulnerable);
            log(std::string("[DEV] Invulnerability: ") + (inv ? "ON" : "OFF"));
            // Re-open the menu with updated label
            npc_dialog_.close();
            npc_dialog_.set_title("[DEV] Character Stats");
            npc_dialog_.add_option('i', std::string("Invulnerability: ") +
                                   (inv ? "ON" : "OFF"));
            npc_dialog_.add_option('b', "Back");
            npc_dialog_.open();
            dialog_node_ = -8;
        } else {
            dialog_node_ = -1;
            dialog_tree_ = nullptr;
        }
        return;
    }

    // Dev menu — POI stamp test
    if (dialog_node_ == -9) {
        dialog_node_ = -1;
        dialog_tree_ = nullptr;
        if (selected < 6) {
            static constexpr Tile poi_types[] = {
                Tile::OW_Ruins,
                Tile::OW_CrashedShip,
                Tile::OW_Outpost,
                Tile::OW_CaveEntrance,
                Tile::OW_Settlement,
                Tile::OW_Landing,
            };
            static constexpr const char* poi_names[] = {
                "Ruins", "Crashed Ship", "Outpost",
                "Cave Entrance", "Settlement", "Landing Pad",
            };
            dev_warp_stamp_test_poi_ = poi_types[selected];
            dev_warp_stamp_test();
            log(std::string("[DEV] Stamp Test: ") + poi_names[selected]);
        }
        return;
    }

    // Ship terminal dialog
    if (dialog_node_ == -6) {
        dialog_node_ = -1;
        dialog_tree_ = nullptr;
        if (selected == 0) {
            enter_ship();
        }
        return;
    }

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
        npc_dialog_.close();
        npc_dialog_.set_title(interacting_npc_->display_name());
        npc_dialog_body_ = next_node.text;
        log(interacting_npc_->display_name() + ": \"" + next_node.text + "\"");
        { char hk = '1';
        for (const auto& choice : next_node.choices) {
            npc_dialog_.add_option(hk++, choice.label);
        } }
        npc_dialog_.set_footer("[Space] Select  [Esc] Close");
        npc_dialog_.set_max_width_frac(0.35f);
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
            npc_dialog_.close();
            npc_dialog_.set_title(interacting_npc_->display_name());
            npc_dialog_body_ = node.text;
            log(interacting_npc_->display_name() + ": \"" + node.text + "\"");
            { char hk = '1';
            for (const auto& choice : node.choices) {
                npc_dialog_.add_option(hk++, choice.label);
            } }
            npc_dialog_.set_footer("[Space] Select  [Esc] Close");
            npc_dialog_.set_max_width_frac(0.35f);
            npc_dialog_.open();
            break;
        }
        case InteractOption::Shop:
            trade_window_.open(interacting_npc_, &player_, renderer_.get());
            npc_dialog_.close();
            break;

        case InteractOption::Quest: {
            dialog_tree_ = &interacting_npc_->interactions.quest->nodes;
            dialog_node_ = 0;
            const auto& node = (*dialog_tree_)[0];
            npc_dialog_.close();
            npc_dialog_.set_title(interacting_npc_->display_name());
            npc_dialog_body_ = node.text;
            log(interacting_npc_->display_name() + ": \"" + node.text + "\"");
            { char hk = '1';
            for (const auto& choice : node.choices) {
                npc_dialog_.add_option(hk++, choice.label);
            } }
            npc_dialog_.set_footer("[Space] Select  [Esc] Close");
            npc_dialog_.set_max_width_frac(0.35f);
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
    if (camera_x_ + map_rect_.w > world_.map().width()) camera_x_ = world_.map().width() - map_rect_.w;
    if (camera_y_ + map_rect_.h > world_.map().height()) camera_y_ = world_.map().height() - map_rect_.h;
    if (camera_x_ < 0) camera_x_ = 0;
    if (camera_y_ < 0) camera_y_ = 0;
}

void Game::recompute_fov() {
    if (navigation_.on_ship) {
        world_.visibility().reveal_all();
        return;
    }

    // Determine effective view radius based on context
    int radius = player_.view_radius + player_.equipment.total_modifiers().view_radius;
    bool is_indoor = world_.map().map_type() == MapType::SpaceStation
                  || world_.map().map_type() == MapType::DerelictStation
                  || world_.map().map_type() == MapType::Starship;
    bool is_dungeon = !on_overworld() && !on_detail_map()
                      && world_.map().map_type() != MapType::DetailMap
                      && !is_indoor;
    if (is_indoor) {
        // Stations and ships: always fully lit, use base view_radius
    } else if (is_dungeon) {
        // Underground: always use light_radius (no sunlight)
        radius = player_.light_radius;
    } else if (!on_overworld()) {
        // Surface detail maps: time of day affects view range
        int max_radius = std::max(world_.map().width(), world_.map().height());
        radius = day_clock_.effective_view_radius(max_radius, player_.light_radius);
    }

    compute_fov(world_.map(), world_.visibility(), player_.x, player_.y, radius);

    // Detail maps: shadowcast for lighting, but entire map stays revealed
    if (on_detail_map() || world_.map().map_type() == MapType::DetailMap) {
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
    ++world_tick_;
    day_clock_.advance(1);

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
    std::shuffle(dirs.begin(), dirs.end(), rng_);
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
        if (std::uniform_int_distribution<int>(0, 1)(rng_) == 0) {
            Item loot = generate_loot_drop(rng_, npc.level);
            log("Dropped: " + loot.name);
            ground_items_.push_back({npc.x, npc.y, std::move(loot)});
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
        if (std::uniform_int_distribution<int>(0, 1)(rng_) == 0) {
            Item loot = generate_loot_drop(rng_, target_npc_->level);
            log("Dropped: " + loot.name);
            ground_items_.push_back({target_npc_->x, target_npc_->y, std::move(loot)});
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



static const char* fixture_type_name(FixtureType type) {
    switch (type) {
        case FixtureType::Table:         return "Table";
        case FixtureType::Console:       return "Console";
        case FixtureType::Crate:         return "Crate";
        case FixtureType::Bunk:          return "Bunk";
        case FixtureType::Rack:          return "Rack";
        case FixtureType::Conduit:       return "Conduit";
        case FixtureType::ShuttleClamp:  return "Shuttle Clamp";
        case FixtureType::Shelf:         return "Shelf";
        case FixtureType::Viewport:      return "Viewport";
        case FixtureType::Door:          return "Door";
        case FixtureType::Window:        return "Window";
        case FixtureType::Stool:         return "Stool";
        case FixtureType::Debris:        return "Debris";
        case FixtureType::HealPod:       return "Healing Pod";
        case FixtureType::FoodTerminal:  return "Food Terminal";
        case FixtureType::WeaponDisplay: return "Weapon Display";
        case FixtureType::RepairBench:   return "Repair Bench";
        case FixtureType::SupplyLocker:  return "Supply Locker";
        case FixtureType::StarChart:     return "Star Chart";
        case FixtureType::RestPod:       return "Rest Pod";
        case FixtureType::ShipTerminal:  return "Ship Terminal";
    }
    return "Unknown";
}

static const char* fixture_type_desc(FixtureType type) {
    switch (type) {
        case FixtureType::Table:         return "A sturdy surface bolted to the deck.";
        case FixtureType::Console:       return "A terminal display scrolls with data.";
        case FixtureType::Crate:         return "A sealed cargo crate. Heavy and immovable.";
        case FixtureType::Bunk:          return "A narrow sleeping rack with a thin mattress.";
        case FixtureType::Rack:          return "A wall-mounted storage rack.";
        case FixtureType::Conduit:       return "Exposed piping hums with energy.";
        case FixtureType::ShuttleClamp:  return "Magnetic clamps securing a docked vessel.";
        case FixtureType::Shelf:         return "Metal shelving lined with containers.";
        case FixtureType::Viewport:      return "A reinforced window looking out into space.";
        case FixtureType::Door:          return "A reinforced bulkhead door.";
        case FixtureType::Window:        return "A transparent panel set into the wall.";
        case FixtureType::Stool:         return "A simple seat bolted to the floor.";
        case FixtureType::Debris:        return "Scattered wreckage and broken components.";
        case FixtureType::HealPod:       return "A medical pod that restores health using nanite technology.";
        case FixtureType::FoodTerminal:  return "An automated food dispensary serving synth-grub and rations.";
        case FixtureType::WeaponDisplay: return "Weapons gleam behind reinforced glass.";
        case FixtureType::RepairBench:   return "A workbench with tools for equipment maintenance.";
        case FixtureType::SupplyLocker:  return "A secure locker for storing personal equipment.";
        case FixtureType::StarChart:     return "A holographic star chart projector.";
        case FixtureType::RestPod:       return "A padded pod for deep restorative sleep.";
        case FixtureType::ShipTerminal:  return "A terminal for boarding your docked starship.";
    }
    return "";
}

std::string Game::look_tile_name(int mx, int my) const {
    // NPC
    for (const auto& npc : world_.npcs()) {
        if (npc.x == mx && npc.y == my) return npc.display_name();
    }
    // Player
    if (mx == player_.x && my == player_.y) return player_.name;
    // Ground item
    for (const auto& gi : ground_items_) {
        if (gi.x == mx && gi.y == my) return gi.item.name;
    }
    // Fixture
    Tile t = world_.map().get(mx, my);
    if (t == Tile::Fixture) {
        int fid = world_.map().fixture_id(mx, my);
        if (fid >= 0) return fixture_type_name(world_.map().fixture(fid).type);
    }
    // Overworld tiles
    switch (t) {
        case Tile::OW_Plains:       return "Plains";
        case Tile::OW_Mountains:    return "Mountains";
        case Tile::OW_Crater:       return "Crater";
        case Tile::OW_IceField:     return "Ice Field";
        case Tile::OW_LavaFlow:     return "Lava Flow";
        case Tile::OW_Desert:       return "Desert";
        case Tile::OW_Fungal:       return "Fungal Growth";
        case Tile::OW_Forest:       return "Dense Forest";
        case Tile::OW_River:        return "River";
        case Tile::OW_Lake:         return "Lake";
        case Tile::OW_Swamp:        return "Swamp";
        case Tile::OW_CaveEntrance: return "Cave Entrance";
        case Tile::OW_Ruins:        return "Ruins";
        case Tile::OW_Settlement:   return "Settlement";
        case Tile::OW_CrashedShip:  return "Crashed Ship";
        case Tile::OW_Outpost:      return "Outpost";
        case Tile::OW_Landing:      return "Landing Pad";
        case Tile::Wall: case Tile::StructuralWall: return "Wall";
        case Tile::Floor:           return "";
        case Tile::IndoorFloor:     return "";
        case Tile::Portal:          return "Stairs";
        case Tile::Water:           return "Water";
        case Tile::Ice:             return "Ice";
        case Tile::Empty:           return "Void";
        default: return "";
    }
}

std::string Game::look_tile_desc(int mx, int my) const {
    // NPC
    for (const auto& npc : world_.npcs()) {
        if (npc.x == mx && npc.y == my) {
            std::string desc = std::string(race_name(npc.race));
            if (!npc.role.empty()) desc += " " + npc.role;
            desc += ".";
            if (npc.interactions.talk) {
                desc += " \"" + npc.interactions.talk->greeting + "\"";
            }
            return desc;
        }
    }
    // Player
    if (mx == player_.x && my == player_.y) {
        return std::string(race_name(player_.race)) + " " +
               class_name(player_.player_class) + ". Level " +
               std::to_string(player_.level) + ".";
    }
    // Ground item
    for (const auto& gi : ground_items_) {
        if (gi.x == mx && gi.y == my) return gi.item.description;
    }
    // Fixture
    Tile t = world_.map().get(mx, my);
    if (t == Tile::Fixture) {
        int fid = world_.map().fixture_id(mx, my);
        if (fid >= 0) return fixture_type_desc(world_.map().fixture(fid).type);
    }
    // Overworld tiles
    switch (t) {
        case Tile::OW_Plains:       return "Open terrain stretches to the horizon.";
        case Tile::OW_Mountains:    return "Towering peaks of jagged rock pierce the atmosphere.";
        case Tile::OW_Crater:       return "A massive impact crater scarring the surface.";
        case Tile::OW_IceField:     return "A frozen expanse of crystalline ice.";
        case Tile::OW_LavaFlow:     return "Molten rock glows beneath a cracked surface.";
        case Tile::OW_Desert:       return "Barren dunes of fine dust stretch endlessly.";
        case Tile::OW_Fungal:       return "Strange luminescent fungi carpet the ground.";
        case Tile::OW_Forest:       return "Dense alien vegetation blocks the view.";
        case Tile::OW_River:        return "A rushing current of liquid cuts through the terrain.";
        case Tile::OW_Lake:         return "A vast body of liquid shimmers under the sky.";
        case Tile::OW_Swamp:        return "Murky pools and twisted growth make footing treacherous.";
        case Tile::OW_CaveEntrance: return "A dark opening leads underground. Press > to enter.";
        case Tile::OW_Ruins:        return "Crumbling remains of an ancient structure.";
        case Tile::OW_Settlement:   return "A small settlement. Signs of habitation are visible.";
        case Tile::OW_CrashedShip:  return "The twisted wreckage of a downed vessel.";
        case Tile::OW_Outpost:      return "A fortified outpost overlooks the terrain.";
        case Tile::OW_Landing:      return "A flat landing pad. Your ship is docked here.";
        case Tile::Wall: case Tile::StructuralWall:
            return "Solid construction blocks the way.";
        case Tile::Floor: case Tile::IndoorFloor:
            return "Smooth plating covers the deck.";
        case Tile::Portal:          return "Passage leading to another level.";
        case Tile::Water:           return "Murky liquid pools on the ground.";
        case Tile::Ice:             return "A slick frozen surface.";
        case Tile::Empty:           return "The void between the stars.";
        default: return "";
    }
}

void Game::render_look_popup() {
    if (!input_.looking()) return;

    Visibility v = world_.visibility().get(input_.look_x(), input_.look_y());
    if (v == Visibility::Unexplored) return;

    std::string name = look_tile_name(input_.look_x(), input_.look_y());
    if (name.empty()) return;

    std::string desc = look_tile_desc(input_.look_x(), input_.look_y());
    std::string glyph = std::string(input_.cached_glyph()); // cached from render_map (input_.look_x(), input_.look_y());
    Color glyph_color = input_.cached_color(); // cached from render_map (input_.look_x(), input_.look_y());

    // Word-wrap description to fit popup width
    int popup_w = std::max(36, std::min(static_cast<int>(name.size()) + 12, screen_w_ / 2));
    int inner_w = popup_w - 4; // padding

    std::vector<std::string> desc_lines;
    if (!desc.empty()) {
        std::string line;
        for (size_t i = 0; i < desc.size(); ++i) {
            if (desc[i] == ' ' && static_cast<int>(line.size()) >= inner_w) {
                desc_lines.push_back(line);
                line.clear();
            } else {
                line += desc[i];
                if (static_cast<int>(line.size()) >= inner_w + 5) {
                    desc_lines.push_back(line);
                    line.clear();
                }
            }
        }
        if (!line.empty()) desc_lines.push_back(line);
    }

    // Popup height: top(1) + glyph_row(1) + name_row(1) + sep(1) + desc_lines + bottom(1)
    int popup_h = 4 + static_cast<int>(desc_lines.size()) + (desc_lines.empty() ? 0 : 1);

    // Center popup on screen
    int px = (screen_w_ - popup_w) / 2;
    int py = (screen_h_ - popup_h) / 2;

    // Draw Panel-style popup
    Rect bounds{px, py, popup_w, popup_h};
    DrawContext ctx(renderer_.get(), bounds);
    ctx.fill(' ');

    Color border = Color::White;
    // Top: ▐▀▀▀▌
    ctx.put(0, 0, BoxDraw::RIGHT_HALF, border);
    for (int x = 1; x < popup_w - 1; ++x)
        ctx.put(x, 0, BoxDraw::UPPER_HALF, border);
    ctx.put(popup_w - 1, 0, BoxDraw::LEFT_HALF, border);
    // Sides
    for (int y = 1; y < popup_h - 1; ++y) {
        ctx.put(0, y, BoxDraw::RIGHT_HALF, border);
        ctx.put(popup_w - 1, y, BoxDraw::LEFT_HALF, border);
    }
    // Bottom: ▐▄▄▄▌
    ctx.put(0, popup_h - 1, BoxDraw::RIGHT_HALF, border);
    for (int x = 1; x < popup_w - 1; ++x)
        ctx.put(x, popup_h - 1, BoxDraw::LOWER_HALF, border);
    ctx.put(popup_w - 1, popup_h - 1, BoxDraw::LEFT_HALF, border);

    // Glyph centered
    int row = 1;
    ctx.put(popup_w / 2, row, glyph.c_str(), glyph_color);
    row++;

    // Name centered
    int name_x = (popup_w - static_cast<int>(name.size())) / 2;
    ctx.text(name_x, row, name, Color::White);
    row++;

    // Separator
    if (!desc_lines.empty()) {
        for (int x = 1; x < popup_w - 1; ++x)
            ctx.put(x, row, BoxDraw::H, Color::DarkGray);
        row++;

        // Description lines
        for (const auto& dl : desc_lines) {
            ctx.text(2, row, dl, Color::DarkGray);
            row++;
        }
    }
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
            int heal = item.buy_value * 5; // scale heal with item value
            if (heal < 5) heal = 5;
            player_.hp = std::min(player_.hp + heal, player_.max_hp);
            if (player_.hunger > HungerState::Satiated)
                player_.hunger = static_cast<HungerState>(
                    static_cast<uint8_t>(player_.hunger) - 1);
            log("You eat the " + item.name + ". (+" +
                std::to_string(heal) + " HP)");
            break;
        }
        case ItemType::Stim: {
            int heal = 5;
            player_.hp = std::min(player_.hp + heal, player_.max_hp);
            log("You inject the " + item.name + ". (+5 HP)");
            break;
        }
        case ItemType::Battery: {
            auto& eq = player_.equipment.missile;
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
    if (!item.slot.has_value()) {
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
    world_.npcs().erase(
        std::remove_if(world_.npcs().begin(), world_.npcs().end(),
                        [](const Npc& n) { return !n.alive(); }),
        world_.npcs().end());
}

// --- Level-up rewards (easy to balance) ---
static constexpr int attr_points_per_level = 2;
static constexpr int skill_points_per_level = 50;
static constexpr float xp_scale_factor = 1.5f;

void Game::check_level_up() {
    while (player_.xp >= player_.max_xp) {
        player_.xp -= player_.max_xp;
        player_.level++;
        player_.max_xp = static_cast<int>(player_.max_xp * xp_scale_factor);
        player_.attribute_points += attr_points_per_level;
        player_.skill_points += skill_points_per_level;

        // Heal to full on level up
        player_.max_hp = player_.effective_max_hp();
        player_.hp = player_.max_hp;

        log("LEVEL UP! You are now level " + std::to_string(player_.level) + ".");
        log("  +" + std::to_string(attr_points_per_level) + " attribute points, +"
            + std::to_string(skill_points_per_level) + " SP.");
    }
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
        ms.tilemap = world_.map();
        ms.visibility = world_.visibility();
        ms.npcs = world_.npcs();
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
    data.stash = stash_;
    data.navigation = navigation_;
    data.surface_mode = static_cast<uint8_t>(surface_mode_);
    data.overworld_x = overworld_x_;
    data.overworld_y = overworld_y_;
    data.local_tick = day_clock_.local_tick;
    data.local_ticks_per_day = day_clock_.local_ticks_per_day;

    MapState ms;
    ms.map_id = 0;
    ms.tilemap = world_.map();
    ms.visibility = world_.visibility();
    ms.npcs = world_.npcs();
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
    stash_ = data.stash;

    // Restore first map
    const auto& ms = data.maps[0];
    world_.map() = ms.tilemap;
    world_.visibility() = ms.visibility;
    world_.npcs() = ms.npcs;
    ground_items_ = ms.ground_items;

    // Restore navigation data (or bootstrap for old saves)
    if (!data.navigation.systems.empty()) {
        navigation_ = data.navigation;
    } else {
        navigation_ = generate_galaxy(seed_);
    }
    star_chart_viewer_ = StarChartViewer(&navigation_, renderer_.get());

    // Restore overworld state
    surface_mode_ = static_cast<SurfaceMode>(data.surface_mode);
    overworld_x_ = data.overworld_x;
    overworld_y_ = data.overworld_y;

    // Restore day clock
    day_clock_.local_tick = data.local_tick;
    day_clock_.local_ticks_per_day = data.local_ticks_per_day;

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
    // Character creation overlay
    if (character_creation_.is_open()) {
        character_creation_.draw(screen_w_, screen_h_);
        return;
    }

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

    // Block-letter ASTRA logo
    int logo_w = title_letter_count * title_letter_width
               + (title_letter_count - 1) * title_letter_gap;
    int logo_x = (screen_w_ - logo_w) / 2;
    int art_start_y = screen_h_ / 2 - title_letter_height - 2;

    for (int row = 0; row < title_letter_height; ++row) {
        int lx = logo_x;
        for (int li = 0; li < title_letter_count; ++li) {
            ctx.text(lx, art_start_y + row, title_letters[li][row], Color::White);
            lx += title_letter_width + title_letter_gap;
        }
    }

    int menu_y = art_start_y + title_letter_height + 2;
    for (int i = 0; i < menu_item_count_; ++i) {
        if (i == menu_selection_) {
            // ·::|  Label  |::·
            std::string item = menu_items[i];
            std::string padded = "  " + item + "  ";
            std::string full = ".::|" + padded + "|::.";
            int x = (screen_w_ - static_cast<int>(full.size())) / 2;
            ctx.text(x, menu_y + i, ".::", Color::Red);
            ctx.put(x + 3, menu_y + i, BoxDraw::V, Color::Cyan);
            ctx.text(x + 4, menu_y + i, padded, Color::Yellow);
            ctx.put(x + 4 + static_cast<int>(padded.size()), menu_y + i, BoxDraw::V, Color::Cyan);
            ctx.text(x + 5 + static_cast<int>(padded.size()), menu_y + i, "::.", Color::Red);
        } else {
            std::string label = "     " + std::string(menu_items[i]) + "     ";
            ctx.text_center(menu_y + i, label, Color::DarkGray);
        }
    }

    // Quit confirm overlay on menu
    quit_confirm_.draw(renderer_.get(), screen_w_, screen_h_);
}

void Game::render_play() {
    render_stats_bar();
    render_bars();

    render_tabs();

    DrawContext sep_ctx(renderer_.get(), separator_rect_);
    sep_ctx.vline(0, BoxDraw::V, Color::DarkGray);

    render_map();

    if (panel_visible_) {
        render_side_panel();
    }
    DrawContext bottom_sep(renderer_.get(), bottom_sep_rect_);
    bottom_sep.hline(0, BoxDraw::H, Color::DarkGray);

    render_effects_bar();
    render_abilities_bar();

    // Overlay windows
    if (inspecting_item_) render_item_inspect();
    render_look_popup();
    npc_dialog_.draw(renderer_.get(), screen_w_, screen_h_);
    pause_menu_.draw(renderer_.get(), screen_w_, screen_h_);
    quit_confirm_.draw(renderer_.get(), screen_w_, screen_h_);
    console_.draw(renderer_.get(), screen_w_, screen_h_);
    help_screen_.draw(renderer_.get(), screen_w_, screen_h_);
    trade_window_.draw(screen_w_, screen_h_);
    character_screen_.draw(screen_w_, screen_h_);
    star_chart_viewer_.draw(screen_w_, screen_h_);

    // Welcome screen overlay
    if (show_welcome_) {
        int ww = 60;
        int wh = 28;
        if (ww > screen_w_ - 8) ww = screen_w_ - 8;
        if (wh > screen_h_ - 6) wh = screen_h_ - 6;
        int wx = (screen_w_ - ww) / 2;
        int wy = (screen_h_ - wh) / 2;

        Panel welcome(renderer_.get(), Rect{wx, wy, ww, wh}, "A S T R A");
        welcome.set_footer("[Space] Continue");
        welcome.draw();

        DrawContext wctx = welcome.content();
        int y = 1;

        wctx.text_center(y, "Welcome, " + player_.name + ".", Color::White);
        y += 2;
        wctx.text_center(y, "Your journey to the center of the galaxy begins.", Color::DarkGray);
        y++;
        wctx.text_center(y, "The supermassive black hole Sagittarius A* awaits.", Color::DarkGray);
        y++;
        wctx.text_center(y, "But first, you must survive.", Color::DarkGray);
        y += 2;

        wctx.text_center(y, "You are docked at The Heavens Above,", Color::Cyan);
        y++;
        wctx.text_center(y, "a space station orbiting Jupiter.", Color::Cyan);
        y += 3;

        // Key bindings
        int kx = 6;
        wctx.text(kx, y, "CONTROLS", Color::White);
        y += 2;
        wctx.text(kx, y, "Arrow keys", Color::Yellow);
        wctx.text(kx + 22, y, "Move", Color::DarkGray);
        y++;
        wctx.text(kx, y, "Space", Color::Yellow);
        wctx.text(kx + 22, y, "Use / interact", Color::DarkGray);
        y++;
        wctx.text(kx, y, "l", Color::Yellow);
        wctx.text(kx + 22, y, "Look / examine", Color::DarkGray);
        y++;
        wctx.text(kx, y, "c", Color::Yellow);
        wctx.text(kx + 22, y, "Character screen", Color::DarkGray);
        y++;
        wctx.text(kx, y, "t / s", Color::Yellow);
        wctx.text(kx + 22, y, "Target / shoot", Color::DarkGray);
        y++;
        wctx.text(kx, y, "> / <", Color::Yellow);
        wctx.text(kx + 22, y, "Enter / exit", Color::DarkGray);
        y++;
        wctx.text(kx, y, "ESC", Color::Yellow);
        wctx.text(kx + 22, y, "Pause menu", Color::DarkGray);
    }
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

    // Build calendar string for width measurement: "C1 D3 [▓▓▓▒░░░░] ☀"
    // Progress bar is 8 chars + brackets = 10, icon = 1
    std::string cal = format_calendar(world_tick_);
    cal += " [--------] "; // placeholder for width calc (8-char bar + brackets + space)
    cal += phase_icon(day_clock_.phase());

    // Right side: stats, calendar, location — measure total width for right-alignment
    std::string right;
    int eff_qn = player_.quickness + player_.equipment.total_modifiers().quickness;
    right += " QN:";  right += std::to_string(eff_qn);
    right += " :: MS:"; right += std::to_string(player_.move_speed);
    right += " :: AV:"; right += std::to_string(player_.effective_attack());
    right += " :: DV:"; right += std::to_string(player_.effective_dodge());
    right += " :: ";    right += cal;
    right += " :: ";    right += world_.map().location_name();
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
        std::to_string(player_.quickness + player_.equipment.total_modifiers().quickness), Color::White);

    ctx.text(x, 0, " :: ", Color::DarkGray); x += 4;
    x = ctx.label_value(x, 0, "MS:", Color::DarkGray,
        std::to_string(player_.move_speed), Color::White);

    ctx.text(x, 0, " :: ", Color::DarkGray); x += 4;
    x = ctx.label_value(x, 0, "AV:", Color::DarkGray,
        std::to_string(player_.effective_attack()), Color::Blue);

    ctx.text(x, 0, " :: ", Color::DarkGray); x += 4;
    x = ctx.label_value(x, 0, "DV:", Color::DarkGray,
        std::to_string(player_.effective_dodge()), Color::Blue);

    // Calendar + day progress bar + phase icon
    ctx.text(x, 0, " :: ", Color::DarkGray); x += 4;
    {
        std::string cal_text = format_calendar(world_tick_) + " ";
        ctx.text(x, 0, cal_text, Color::DarkGray);
        x += static_cast<int>(cal_text.size());

        // Phase color
        Color phase_col;
        switch (day_clock_.phase()) {
            case TimePhase::Dawn: phase_col = Color::Yellow; break;
            case TimePhase::Day:  phase_col = Color::Yellow; break;
            case TimePhase::Dusk: phase_col = static_cast<Color>(130); break;
            case TimePhase::Night:phase_col = Color::Blue; break;
        }

        // Day progress bar: [▓▓▓▒░░░░]
        constexpr int bar_len = 8;
        float frac = day_clock_.day_fraction();
        int filled = static_cast<int>(frac * bar_len);
        if (filled > bar_len) filled = bar_len;

        ctx.put(x, 0, '[', Color::DarkGray); ++x;
        for (int i = 0; i < bar_len; ++i) {
            if (i < filled) {
                // ▓ filled
                ctx.text(x, 0, "\xe2\x96\x93", phase_col);
            } else if (i == filled) {
                // ▒ current position
                ctx.text(x, 0, "\xe2\x96\x92", phase_col);
            } else {
                // ░ empty
                ctx.text(x, 0, "\xe2\x96\x91", Color::DarkGray);
            }
            ++x;
        }
        ctx.put(x, 0, ']', Color::DarkGray); ++x;

        // Phase icon
        ctx.text(x, 0, " ", Color::Default); ++x;
        ctx.text(x, 0, phase_icon(day_clock_.phase()), phase_col);
        x += 1;
    }

    ctx.text(x, 0, " :: ", Color::DarkGray); x += 4;
    ctx.text(x, 0, world_.map().location_name(), Color::White);
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
    sep.hline(0, BoxDraw::H, Color::DarkGray);
}

// Floor scatter: biome-specific alternate glyphs for visual texture.
// Returns the base '.' if no scatter, or an alternate glyph.
// Uses a position hash so the result is stable across frames.
static char floor_scatter(int x, int y, Biome biome) {
    if (biome == Biome::Station) return '.';

    // Simple spatial hash
    unsigned h = static_cast<unsigned>(x * 374761393 + y * 668265263);
    h = (h ^ (h >> 13)) * 1274126177;
    h ^= h >> 16;
    int roll = static_cast<int>(h % 100);

    struct ScatterSet { int threshold; const char* glyphs; int count; };
    ScatterSet s;
    switch (biome) {
        case Biome::Rocky:    s = {15, ",:`",  3}; break;
        case Biome::Volcanic: s = {20, ",';" , 3}; break;
        case Biome::Ice:      s = {12, "'`,",  3}; break;
        case Biome::Sandy:    s = {20, ",`:",  3}; break;
        case Biome::Aquatic:  s = {10, ",:",   2}; break;
        case Biome::Fungal:   s = {18, "\",'", 3}; break;
        case Biome::Crystal:  s = {15, "*'`",  3}; break;
        case Biome::Corroded: s = {20, ",:;",  3}; break;
        case Biome::Forest:   s = {18, "\",'", 3}; break;
        case Biome::Grassland:s = {15, ",`.",  3}; break;
        case Biome::Jungle:   s = {22, "\",'", 3}; break;
        default: return '.';
    }
    if (roll >= s.threshold) return '.';
    return s.glyphs[h / 100 % s.count];
}

static Color overworld_tile_color(Tile tile, Biome biome) {
    switch (tile) {
        case Tile::OW_Plains:
            switch (biome) {
                case Biome::Ice:      return Color::White;
                case Biome::Rocky:    return Color::DarkGray;
                case Biome::Sandy:    return Color::Yellow;
                default:              return Color::Green;
            }
        case Tile::OW_Mountains:   return Color::White;
        case Tile::OW_Crater:      return Color::DarkGray;
        case Tile::OW_IceField:    return Color::Cyan;
        case Tile::OW_LavaFlow:    return Color::Red;
        case Tile::OW_Desert:      return Color::Yellow;
        case Tile::OW_Fungal:      return Color::Green;
        case Tile::OW_Forest:      return Color::Green;
        case Tile::OW_River:       return Color::Blue;
        case Tile::OW_Lake:        return Color::Cyan;
        case Tile::OW_Swamp:       return static_cast<Color>(58);
        case Tile::OW_CaveEntrance:return Color::Magenta;
        case Tile::OW_Ruins:       return Color::BrightMagenta;
        case Tile::OW_Settlement:  return Color::Yellow;
        case Tile::OW_CrashedShip: return Color::Cyan;
        case Tile::OW_Outpost:     return Color::Green;
        case Tile::OW_Landing:     return static_cast<Color>(14); // bright cyan
        default:                   return Color::White;
    }
}

void Game::render_map() {
    DrawContext ctx(renderer_.get(), map_rect_);

    for (int sy = 0; sy < map_rect_.h; ++sy) {
        for (int sx = 0; sx < map_rect_.w; ++sx) {
            int mx = camera_x_ + sx;
            int my = camera_y_ + sy;

            // Starfield backdrop — space stations only
            if (world_.map().biome() == Biome::Station && world_.map().get(mx, my) == Tile::Empty) {
                char star = star_at(mx, my);
                if (star) {
                    Color c = (star == '*' || star == '+') ? Color::White : Color::Cyan;
                    ctx.put(sx, sy, star, c);
                }
            }

            // Tiles respect FOV
            Visibility v = world_.visibility().get(mx, my);
            if (v == Visibility::Unexplored) continue;

            Tile tile_at = world_.map().get(mx, my);
            char g = tile_glyph(tile_at);
            if (g == ' ' && tile_at != Tile::Fixture) continue;

            // Overworld: no FOV dimming, use overworld colors + UTF-8 glyphs
            if (world_.map().map_type() == MapType::Overworld) {
                Color c = overworld_tile_color(tile_at, world_.map().biome());
                uint8_t gov = world_.map().glyph_override(mx, my);
                const char* og = (gov != 0) ? stamp_glyph(gov) : nullptr;
                if (!og) og = overworld_glyph(tile_at, mx, my);
                ctx.put(sx, sy, og, c);
                continue;
            }

            auto bc = biome_colors(world_.map().biome());
            Biome biome = world_.map().biome();
            if (v == Visibility::Visible) {
                Color c = bc.floor;
                const char* utf8 = nullptr;

                if (tile_at == Tile::StructuralWall) {
                    uint8_t mat = world_.map().glyph_override(mx, my);
                    switch (mat) {
                        case 1:  // Concrete
                            c = static_cast<Color>(245);  // medium gray
                            utf8 = "\xe2\x96\x93";        // ▓
                            break;
                        case 2:  // Wood
                            c = static_cast<Color>(137);   // brown/tan
                            utf8 = "\xe2\x96\x92";         // ▒
                            break;
                        case 3:  // Salvage
                            c = static_cast<Color>(240);   // dark gray
                            utf8 = "\xe2\x96\x91";         // ░
                            break;
                        default: // Metal (0)
                            c = Color::White;
                            utf8 = "\xe2\x96\x88";         // █
                            break;
                    }
                }
                else if (tile_at == Tile::Wall) {
                    c = bc.wall;
                    utf8 = dungeon_wall_glyph(biome, mx, my);
                }
                else if (tile_at == Tile::Portal) {
                    c = Color::Magenta;
                    utf8 = dungeon_portal_glyph();
                }
                else if (tile_at == Tile::Water) {
                    c = bc.water;
                    utf8 = dungeon_water_glyph(biome, mx, my);
                }
                else if (tile_at == Tile::Ice) {
                    c = static_cast<Color>(39);
                    utf8 = dungeon_water_glyph(biome, mx, my);
                }
                else if (tile_at == Tile::Fixture) {
                    int fid = world_.map().fixture_id(mx, my);
                    if (fid >= 0 && fid < world_.map().fixture_count()) {
                        const auto& f = world_.map().fixture(fid);
                        if (f.utf8_glyph) {
                            utf8 = f.utf8_glyph;
                        } else {
                            g = f.glyph;
                        }
                        c = f.color;
                    } else {
                        g = '?'; c = Color::Red;
                    }
                }
                else if (tile_at == Tile::IndoorFloor) {
                    c = static_cast<Color>(137);  // warm tan/brown — reads as plating
                    utf8 = "\xe2\x96\xaa";        // ▪ (small filled square)
                }
                else if (tile_at == Tile::Floor) {
                    char sg = floor_scatter(mx, my, biome);
                    if (sg != '.') {
                        g = sg;
                        c = bc.remembered; // dimmer shade for scatter
                    }
                }

                if (utf8) {
                    ctx.put(sx, sy, utf8, c);
                } else {
                    ctx.put(sx, sy, g, c);
                }
            } else {
                // Remembered tiles: use UTF-8 glyphs too
                const char* utf8 = nullptr;
                if (tile_at == Tile::StructuralWall) {
                    uint8_t mat = world_.map().glyph_override(mx, my);
                    switch (mat) {
                        case 1:  utf8 = "\xe2\x96\x93"; break; // ▓
                        case 2:  utf8 = "\xe2\x96\x92"; break; // ▒
                        case 3:  utf8 = "\xe2\x96\x91"; break; // ░
                        default: utf8 = "\xe2\x96\x88"; break; // █
                    }
                }
                else if (tile_at == Tile::Wall)
                    utf8 = dungeon_wall_glyph(biome, mx, my);
                else if (tile_at == Tile::IndoorFloor)
                    utf8 = "\xe2\x96\xaa";   // ▪
                else if (tile_at == Tile::Portal)
                    utf8 = dungeon_portal_glyph();
                else if (tile_at == Tile::Water || tile_at == Tile::Ice)
                    utf8 = dungeon_water_glyph(biome, mx, my);
                else if (tile_at == Tile::Fixture) {
                    int fid = world_.map().fixture_id(mx, my);
                    if (fid >= 0 && fid < world_.map().fixture_count()) {
                        const auto& f = world_.map().fixture(fid);
                        if (f.utf8_glyph) {
                            utf8 = f.utf8_glyph;
                        } else {
                            g = f.glyph;
                        }
                    }
                }

                if (utf8)
                    ctx.put(sx, sy, utf8, bc.remembered);
                else
                    ctx.put(sx, sy, g, bc.remembered);
            }
        }
    }

    // Draw visible ground items
    for (const auto& gi : ground_items_) {
        if (world_.visibility().get(gi.x, gi.y) == Visibility::Visible) {
            ctx.put(gi.x - camera_x_, gi.y - camera_y_,
                    gi.item.glyph, gi.item.color);
        }
    }

    // Draw visible NPCs
    for (const auto& npc : world_.npcs()) {
        if (npc.alive() && world_.visibility().get(npc.x, npc.y) == Visibility::Visible) {
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
        const auto& rw = player_.equipment.missile;
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
            for (const auto& npc : world_.npcs()) {
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

    // Draw look mode cursor — read cell BEFORE overwriting with reticule
    if (input_.looking()) {
        int lsx = input_.look_x() - camera_x_;
        int lsy = input_.look_y() - camera_y_;
        if (lsx >= 0 && lsx < map_rect_.w && lsy >= 0 && lsy < map_rect_.h) {
            // Cache the actual glyph/color at this position
            int screen_x = map_rect_.x + lsx;
            int screen_y = map_rect_.y + lsy;
            {
                char buf[5] = {};
                Color fg = Color::White;
                renderer_->read_cell(screen_x, screen_y, buf, fg);
                input_.cache_look_cell(buf, fg);
            }

            if (input_.look_blink() % 2 == 0) {
                ctx.put(lsx, lsy, 'X', Color::Yellow);
            }
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
            // Count visible (non-marker) characters in a string
            auto visible_len = [](std::string_view s) {
                int len = 0;
                for (size_t i = 0; i < s.size(); ++i) {
                    if (s[i] == COLOR_BEGIN) { ++i; continue; } // skip color byte
                    if (s[i] == COLOR_END) continue;
                    ++len;
                }
                return len;
            };

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

                    if (visible_len(remaining) <= line_w) {
                        lines.push_back({remaining, x});
                        break;
                    }
                    // Find cut point at line_w visible chars
                    int vis = 0;
                    int cut = 0;
                    int last_space = 0;
                    for (size_t i = 0; i < remaining.size() && vis < line_w; ++i) {
                        if (remaining[i] == COLOR_BEGIN) { ++i; continue; }
                        if (remaining[i] == COLOR_END) continue;
                        if (remaining[i] == ' ') last_space = static_cast<int>(i);
                        ++vis;
                        cut = static_cast<int>(i) + 1;
                    }
                    if (last_space > 0) cut = last_space;
                    if (cut == 0) cut = static_cast<int>(remaining.size());
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
                // Render with inline color markers
                int px = lines[i].x;
                Color cur = Color::Default;
                for (size_t ci = 0; ci < lines[i].text.size(); ++ci) {
                    char ch = lines[i].text[ci];
                    if (ch == COLOR_BEGIN && ci + 1 < lines[i].text.size()) {
                        cur = static_cast<Color>(
                            static_cast<uint8_t>(lines[i].text[ci + 1]));
                        ++ci;
                        continue;
                    }
                    if (ch == COLOR_END) {
                        cur = Color::Default;
                        continue;
                    }
                    ctx.put(px, y, ch, cur);
                    ++px;
                }
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
                    ctx.put(2, y, ' ');
                    draw_item_name(ctx, 3, y, item, selected);
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
                if (sel.slot.has_value())
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
            draw_slot("Face:    ", eq.face);
            draw_slot("Head:    ", eq.head);
            draw_slot("Body:    ", eq.body);
            draw_slot("L.Arm:   ", eq.left_arm);
            draw_slot("R.Arm:   ", eq.right_arm);
            draw_slot("L.Hand:  ", eq.left_hand);
            draw_slot("R.Hand:  ", eq.right_hand);
            draw_slot("Back:    ", eq.back);
            draw_slot("Feet:    ", eq.feet);
            draw_slot("Thrown:  ", eq.thrown);
            draw_slot("Missile: ", eq.missile);

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
        case PanelTab::Wait: {
            // Time info header
            int y = 0;
            {
                Color phase_col;
                switch (day_clock_.phase()) {
                    case TimePhase::Dawn: phase_col = Color::Yellow; break;
                    case TimePhase::Day:  phase_col = Color::Yellow; break;
                    case TimePhase::Dusk: phase_col = static_cast<Color>(130); break;
                    case TimePhase::Night:phase_col = Color::Blue; break;
                }
                std::string time_info = std::string(phase_icon(day_clock_.phase()))
                    + " " + phase_name(day_clock_.phase());
                ctx.text(1, y, time_info, phase_col);
                ++y;
                ctx.text(1, y, format_calendar(world_tick_), Color::DarkGray);
                y += 2;
            }

            // Wait options
            static const char* wait_options[] = {
                "Wait 1 turn",
                "Wait 10 turns",
                "Wait 50 turns",
                "Wait 100 turns",
                "Wait until healed",
                "Wait until morning",
            };
            static constexpr int wait_option_count = 6;
            for (int i = 0; i < wait_option_count && y < ctx.height() - 1; ++i) {
                bool selected = (i == wait_cursor_);
                std::string label = std::string("[") + std::to_string(i + 1) + "] " + wait_options[i];
                if (i == 5 && day_clock_.phase() == TimePhase::Day) {
                    // "Wait until morning" greyed out during day
                    ctx.text(1, y, label, Color::DarkGray);
                } else {
                    if (selected) ctx.text(0, y, ">", Color::Yellow);
                    ctx.text(1, y, label, selected ? Color::White : Color::Default);
                }
                ++y;
            }
            // Hints
            ctx.text(1, ctx.height() - 1, "+/- [Enter]wait [1-6]quick", Color::DarkGray);
            break;
        }
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
    draw_item_info(ctx, item);
}

void Game::render_effects_bar() {
    DrawContext ctx(renderer_.get(), effects_rect_);
    int x = 1;
    ctx.text(x, 0, "EFFECTS:", Color::DarkGray);
    x += 9;
    bool any_effect = false;
    for (const auto& e : player_.effects) {
        if (!e.show_in_bar) continue;
        any_effect = true;
        std::string label = e.name;
        if (e.remaining > 0) {
            label += "(" + std::to_string(e.remaining) + ")";
        }
        ctx.text(x, 0, label, e.color);
        x += static_cast<int>(label.size()) + 1;
    }
    if (!any_effect) {
        ctx.text(x, 0, "[none]", Color::DarkGray);
    }

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
    const auto& rw = player_.equipment.missile;
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
