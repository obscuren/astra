#include "astra/ability.h"
#include "astra/game.h"

namespace astra {

void Game::handle_play_input(int key) {
    // Welcome screen — space dismisses
    if (show_welcome_) {
        if (key == ' ') {
            show_welcome_ = false;
            // Show tutorial choice dialog after welcome
            if (tutorial_pending_) {
                tutorial_pending_ = false;
                dialog_.show_tutorial_choice(*this);
            }
        }
        return;
    }

    // Map editor intercept
    if (map_editor_.is_open()) {
        if (map_editor_.playing()) {
            // During play-test, F2 stops — otherwise normal game input
            if (key == KEY_F2) {
                map_editor_.stop_play(*this);
                return;
            }
            // Fall through to normal play input
        } else {
            bool was_standalone = map_editor_.standalone();
            map_editor_.handle_input(key, *this);
            if (!map_editor_.is_open() && was_standalone) {
                state_ = GameState::MainMenu;
                menu_selection_ = 0;
            }
            return;
        }
    }

    // Dev console intercept
    if (console_.is_open()) {
        console_.handle_input(key, *this);
        return;
    }
    // Backtick opens console in dev mode
    if (key == '`') {
        console_.toggle();
        return;
    }

    // Help screen intercept
    if (help_screen_.is_open()) {
        help_screen_.handle_input(key);
        return;
    }

    // Pause menu intercepts all input when open
    if (pause_menu_.open) {
        MenuResult result = pause_menu_.handle_input(key);
        if (result == MenuResult::Selected) {
            char k = pause_menu_.selected_key();
            if (k == 'r') { /* Return to Game — just closes */ }
            else if (k == 'h') {
                help_screen_.open();
            }
            else if (k == 's') {
                if (dev_mode_) { log("Saving disabled in dev mode."); }
                else { save_system_.save(*this); log("Game saved."); }
            }
            else if (k == 'l') {
                save_slots_ = list_saves();
                save_slots_.erase(
                    std::remove_if(save_slots_.begin(), save_slots_.end(),
                                   [](const SaveSlot& s) { return s.dead; }),
                    save_slots_.end());
                load_selection_ = 0;
                prev_state_ = GameState::Playing;
                state_ = GameState::LoadMenu;
            }
            else if (k == 'o') { log("Options not yet implemented."); }
            else if (k == 'q') {
                // Save and quit (only shown in normal mode)
                save_system_.save(*this);
                running_ = false;
            }
            else if (k == 'x') {
                if (dev_mode_) {
                    running_ = false;
                } else {
                    // Confirm quit without saving
                    quit_confirm_.reset();
                    quit_confirm_.title = "Quit without saving?";
                    quit_confirm_.add_option('y', "Yes, quit without saving");
                    quit_confirm_.add_option('n', "No, keep playing");
                    quit_confirm_.selection = 0;
                    quit_confirm_.open = true;
                }
            }
        }
        return;
    }

    // Trade window intercepts input when open
    if (trade_window_.is_open()) {
        trade_window_.handle_input(key);
        if (!trade_window_.is_open()) {
            if (trade_window_.has_message()) log(trade_window_.consume_message());
            dialog_.close();
        }
        return;
    }

    // Character screen intercepts input when open
    if (character_screen_.is_open()) {
        character_screen_.handle_input(key);
        if (character_screen_.has_dropped_item()) {
            Item dropped = character_screen_.consume_dropped_item();
            log("You drop " + dropped.name + ".");
            world_.ground_items().push_back({player_.x, player_.y, std::move(dropped)});
        }
        auto installed_slot = character_screen_.consume_installed_ship_slot();
        if (!installed_slot.empty()) {
            quest_manager_.on_ship_component_installed(installed_slot);
            // ARIA reacts to each component installation
            if (installed_slot == "Engine")
                log("ARIA: \"Engine online. I can feel the hum again. Almost missed it.\"");
            else if (installed_slot == "Hull")
                log("ARIA: \"Hull integrity restored. I was getting tired of the draft.\"");
            else if (installed_slot == "Navi Computer")
                log("ARIA: \"Navigation online. The stars are mine again. Where shall we go?\"");
            else if (installed_slot == "Shield")
                log("ARIA: \"Shield generator active. That's a comfort.\"");
            else
                log("ARIA: \"Component installed. Systems updated.\"");
        }
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

    // Repair bench intercepts when open
    if (repair_bench_.is_open()) {
        repair_bench_.handle_input(key);
        return;
    }

    // Lost popup intercepts when open
    if (lost_popup_.open) {
        MenuResult r = lost_popup_.handle_input(key);
        if (r == MenuResult::Selected || r == MenuResult::Closed) {
            lost_popup_.reset();
            if (lost_pending_) {
                enter_lost_detail();
            }
        }
        return;
    }

    // NPC dialog intercepts input when open
    if (dialog_.is_open()) {
        dialog_.handle_input(key, *this);
        // Check ARIA command terminal outputs
        if (dialog_.consume_aria_ship_tab()) {
            character_screen_.open(&player_, renderer_.get(), &quest_manager_,
                                   world_.navigation().on_ship, CharTab::Ship);
        }
        if (dialog_.consume_aria_star_chart()) {
            star_chart_viewer_.set_view_only(false);
            star_chart_viewer_.open();
        }
        // Tutorial follow-up: show guidance after tutorial choice dialog closes
        if (!dialog_.is_open() && dialog_.consume_aria_tutorial_followup()) {
            dialog_.show_tutorial_followup();
        }
        if (dialog_.consume_aria_disembark()) {
            exit_ship_to_station();
        }
        if (dialog_.consume_aria_open_datapad()) {
            character_screen_.open(&player_, renderer_.get(), &quest_manager_,
                                   world_.navigation().on_ship);
        }
        return;
    }


    // Look mode intercept
    if (input_.looking()) {
        input_.handle_look_input(key, world_.map().width(), world_.map().height());
        compute_camera(); // follow look cursor, or snap back to player on exit
        return;
    }

    // Targeting mode intercept
    if (combat_.targeting()) {
        combat_.handle_targeting_input(key, *this);
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

    // Awaiting auto-walk direction (w + direction or ww)
    if (awaiting_autowalk_) {
        awaiting_autowalk_ = false;
        switch (key) {
            case 'k': case KEY_UP:
                auto_walking_ = true; auto_walk_dx_ = 0; auto_walk_dy_ = -1;
                auto_walk_hp_ = player_.hp;
                log("Auto-walking north...");
                return;
            case 'j': case KEY_DOWN:
                auto_walking_ = true; auto_walk_dx_ = 0; auto_walk_dy_ = 1;
                auto_walk_hp_ = player_.hp;
                log("Auto-walking south...");
                return;
            case 'h': case KEY_LEFT:
                auto_walking_ = true; auto_walk_dx_ = -1; auto_walk_dy_ = 0;
                auto_walk_hp_ = player_.hp;
                log("Auto-walking west...");
                return;
            case KEY_RIGHT:
                auto_walking_ = true; auto_walk_dx_ = 1; auto_walk_dy_ = 0;
                auto_walk_hp_ = player_.hp;
                log("Auto-walking east...");
                return;
            case 'w':
                // ww = auto-explore
                auto_exploring_ = true;
                explore_goal_x_ = explore_goal_y_ = -1;
                auto_walk_hp_ = player_.hp;
                log("Auto-exploring...");
                return;
            default:
                log("Cancelled.");
                return;
        }
    }

    switch (key) {
        case '\033':
            pause_menu_.reset();
            pause_menu_.title = "Game Menu";
            pause_menu_.add_option('r', "return to game");
            pause_menu_.add_option('o', "options");
            pause_menu_.add_option('h', "help");
            pause_menu_.add_option('s', "save game");
            pause_menu_.add_option('l', "load game");
            if (!dev_mode_) {
                pause_menu_.add_option('q', "save and quit");
            }
            pause_menu_.add_option('x', "quit without saving");
            pause_menu_.selection = 0;
            pause_menu_.open = true;
            break;
        case ' ':
            if (world_.on_overworld()) {
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
        case 'w':
            awaiting_autowalk_ = true;
            log("Auto-walk: choose direction, or press w again to explore.");
            break;
        case 't': combat_.begin_targeting(*this); break;
        case 's': combat_.shoot_target(*this); break;
        case 'r': combat_.reload_weapon(*this); break;
        case 'g': pickup_ground_item(); break;
        case 'c': character_screen_.open(&player_, renderer_.get(), &quest_manager_,
                                        world_.navigation().on_ship); break;
        case '?': help_screen_.open(); break;
        case 'm':
            if (dev_mode_) {
                star_chart_viewer_.open();
            }
            break;
        case '+': case '=': {
            auto tab = static_cast<PanelTab>(active_tab_);
            if (tab == PanelTab::Wait) {
                if (wait_cursor_ < 5) ++wait_cursor_;
            } else if (tab == PanelTab::Messages && message_scroll_ > 0) {
                message_scroll_--;
            }
            break;
        }
        case '-': {
            auto tab = static_cast<PanelTab>(active_tab_);
            if (tab == PanelTab::Wait) {
                if (wait_cursor_ > 0) --wait_cursor_;
            } else if (tab == PanelTab::Messages) {
                message_scroll_++;
            }
            break;
        }
        case '>': {
            if (world_.on_overworld()) {
                Tile t = world_.map().get(player_.x, player_.y);
                if (t == Tile::OW_Mountains || t == Tile::OW_Lake) {
                    log("This terrain cannot be explored on foot.");
                } else {
                    enter_detail_map();
                }
            } else if (world_.on_detail_map()) {
                // Check for Portal tile to enter dungeon
                Tile t = world_.map().get(player_.x, player_.y);
                if (t == Tile::Portal) {
                    enter_dungeon_from_detail();
                }
            }
            break;
        }
        case '<': {
            if (lost_ && world_.on_detail_map()) {
                log("You're lost. Keep moving to regain your bearings.");
                break;
            }
            if (world_.on_detail_map()) {
                exit_detail_to_overworld();
            } else if (world_.surface_mode() == SurfaceMode::Dungeon &&
                       !world_.navigation().at_station && !world_.navigation().on_ship) {
                exit_dungeon_to_detail();
            }
            break;
        }
        case '\n': case '\r':
        case '1': case '2': case '3': case '4': case '5': case '6': {
            auto tab = static_cast<PanelTab>(active_tab_);
            // Number keys 1-5: abilities (unless on Wait tab)
            if (key >= '1' && key <= '5' && tab != PanelTab::Wait) {
                use_ability(key - '1', *this);
                break;
            }
            if (key == '6' && tab != PanelTab::Wait) break;
            // Overworld: enter detail map for the tile underneath the player
            if (world_.on_overworld() && (key == '\n' || key == '\r')) {
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
                        if (world_.day_clock().phase() == TimePhase::Day) break;
                        int limit = world_.day_clock().local_ticks_per_day + 10;
                        while (world_.day_clock().phase() != TimePhase::Day && limit-- > 0) {
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
            break;
        }
        case 'k': case KEY_UP:    try_move( 0, -1); break;
        case 'j': case KEY_DOWN:  try_move( 0,  1); break;
        case 'h': case KEY_LEFT:  try_move(-1,  0); break;
        case KEY_RIGHT:           try_move( 1,  0); break;
    }
}


} // namespace astra
