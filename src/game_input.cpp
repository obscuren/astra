#include "astra/ability_bar.h"
#include "astra/cyberdeck.h"
#include "astra/game.h"
#include "astra/grid_combat.h"
#include "astra/hackable.h"
#include "astra/item_defs.h"
#include "astra/program.h"
#include "astra/skill_defs.h"
#include "astra/skill_grant.h"

#include <algorithm>

namespace astra {

// ── Tether action (D2) ───────────────────────────────────────────────────
// Real-world Tether: marks a non-Crystal-bearing NPC for Mark projection on
// the next jack-in. Costs only Heat (Drift) — RAM (Channel) is a session-
// only resource; v2 in-Grid Tether can pay RAM at that time.
//
// Range: L3 → 8 tiles (AoE TODO), L2 → 8 tiles, L1 → 1 tile (adjacent only).
void Game::begin_tether_targeting() {
    // 1. Skill gate
    if (!player_.skill_tether_l1) {
        log("You don't know how to Tether a target.");
        return;
    }

    // 2. Cyberdeck gate
    auto* deck_slot = player_.equipment.equipped_cyberdeck();
    if (!deck_slot || !*deck_slot || !(*deck_slot)->deck) {
        log("No cyberdeck equipped \xe2\x80\x94 Tether requires a neural link.");
        return;
    }
    auto& cd = *(*deck_slot)->deck;

    // 3. Heat budget gate
    // v1 simplification: real-world Tether charges only Heat because RAM
    // is a session-only resource that doesn't exist outside an active Grid
    // session. v2 in-Grid Tether will pay RAM instead.
    if (cd.heat_current + kTetherHeatCost > cd.stats.heat_cap) {
        log("Heat over cap \xe2\x80\x94 Tether would overheat the deck.");
        return;
    }

    // 4. Enter look-cursor targeting mode so the player can pick a target.
    //    tether_targeting_ is cleared on confirm or cancel.
    tether_targeting_ = true;
    input_.begin_look(player_.x, player_.y);
    log("Tether target. Move cursor, [Enter] confirm, [Esc] cancel.");
}

void Game::confirm_tether_targeting() {
    tether_targeting_ = false;
    input_.cancel_look();

    int tx = input_.look_x();
    int ty = input_.look_y();

    // Determine range from highest learned tier.
    int tether_range = 1;  // L1 default: adjacent only
    if (player_.skill_tether_l3 || player_.skill_tether_l2) {
        tether_range = 8;
    }
    // TODO L3 AoE: future v2 should burst all visible targets within 3 tiles.

    // Find NPC at cursor
    Npc* target_npc = nullptr;
    for (auto& npc : world_.npcs()) {
        if (npc.x == tx && npc.y == ty && npc.alive()) {
            target_npc = &npc;
            break;
        }
    }

    if (!target_npc) {
        log("No target there.");
        return;
    }

    // Chebyshev range check
    int dist = std::max(std::abs(tx - player_.x),
                        std::abs(ty - player_.y));
    if (dist > tether_range) {
        log("Target out of range.");
        return;
    }

    // LoS check: the tile must be currently visible (FOV implies LoS from player).
    if (world_.visibility().get(tx, ty) != Visibility::Visible) {
        log("No line of sight to target.");
        return;
    }

    // Don't double-tether
    if (target_npc->force_tether && target_npc->anchor_id >= 0) {
        log(target_npc->name + " is already Tethered.");
        return;
    }

    // Commit: mark + pay Heat
    auto* deck_slot = player_.equipment.equipped_cyberdeck();
    if (!deck_slot || !*deck_slot || !(*deck_slot)->deck) {
        log("Deck disappeared.");
        return;
    }
    auto& cd = *(*deck_slot)->deck;
    target_npc->force_tether = true;
    cyberdeck_add_heat(cd, kTetherHeatCost);
    log("Tethered " + target_npc->name +
        " \xe2\x80\x94 projection ready next jack-in.  [Heat +"
        + std::to_string(kTetherHeatCost) + "]");
}

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

    // Playback viewer intercept
    if (playback_viewer_.is_open()) {
        playback_viewer_.handle_input(key);
        return;
    }

    // Lore viewer intercept
    if (lore_viewer_.is_open()) {
        lore_viewer_.handle_input(key);
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
                    quit_confirm_.selection = 1;   // default to "No" — too easy to mash Enter on "Yes"
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

    // Plan 7 §3a: real-world DeviceShell now renders inside the PDA's
    // Hacking tab and routes input through PdaScreen. The fullscreen
    // shell overlay/input intercept have been removed. The in-Grid shell
    // is handled in grid_input. The only post-shell side-effect we still
    // need at this layer is advancing the world by interact-cost when the
    // shell auto-closes (cmd_exit) — see below, after pda_screen_.handle_input.

    // PDA screen intercepts input when open
    if (pda_screen_.is_open()) {
        auto* dev_before = hacking_.device_shell();
        bool shell_open_before = dev_before &&
                                 dev_before->via() == ShellVia::RealWorld;
        pda_screen_.handle_input(key);
        // If the device shell auto-closed during this input (cmd_exit) advance
        // the world by interact-cost so shell time at the device costs the
        // same as a typical interact. Also fires when ESC yanks the cable.
        if (shell_open_before && !hacking_.device_shell_open()) {
            advance_world(ActionCost::interact);
        }
        if (pda_screen_.consume_board_ship_request()) {
            board_ship_from_overworld();
            return;
        }
        if (pda_screen_.has_dropped_item()) {
            Item dropped = pda_screen_.consume_dropped_item();
            log("You drop " + dropped.name + ".");
            world_.ground_items().push_back({player_.x, player_.y, std::move(dropped)});
        }
        if (pda_screen_.has_use_item_request()) {
            use_item(pda_screen_.consume_use_item_request());
        }
        if (auto idx = pda_screen_.recharge_request_idx(); idx >= 0) {
            open_cell_picker_for_item(idx);
            pda_screen_.clear_recharge_request();
        }
        if (auto req = pda_screen_.recharge_equipped_request(); req >= 0) {
            open_cell_picker(/*target_is_shield=*/req == 1);
            pda_screen_.clear_recharge_equipped_request();
        }
        if (uint32_t nid_v = pda_screen_.consume_jack_in_request(); nid_v != 0) {
            pda_screen_.close();
            GridNodeId nid;
            nid.value = nid_v;
            hacking_.jack_in(*this, nid);
            return;
        }
        // Plan 7: `pda> ssh ...` — open per-device shell.
        {
            bool as_root = true;
            if (uint32_t ip_v = pda_screen_.consume_ssh_request(as_root); ip_v != 0) {
                Hackable* h = world_.find_hackable_by_ip(ip_v);
                if (!h) {
                    log("ssh: host unreachable.");
                    return;
                }
                ShellTier tier = as_root ? ShellTier::Root : ShellTier::Guest;
                hacking_.open_device_shell(*this, *h, tier,
                                           ShellVia::RealWorld,
                                           /*manual_ssh=*/true,
                                           as_root ? "root" : "guest");
                return;
            }
        }
        if (auto br = pda_screen_.consume_breach_request(); br.valid()) {
            // Plan 5 Task 39: netmap-side breach. Find the matching edge
            // in the (mutable) world and flip cracked=true. No sector entry.
            //
            // TODO(Plan 6): charge breach.exe RAM/Heat from the equipped
            // deck. The widget can't reach the deck and Plan 6's HUD
            // redesign threads costing through HackingSystem; until then
            // the netmap-side breach is free.
            auto& net = world_.grid_network();
            bool ok = false;
            for (auto& e : net.edges_mut()) {
                if (e.from.value == br.from_id && e.to.value == br.to_id) {
                    if (e.gateway_tier > 0 && !e.cracked) {
                        e.cracked = true;
                        ok = true;
                    }
                    break;
                }
            }
            log(ok ? "breach: gateway cracked from netmap."
                   : "breach: gateway already open.");
        }
        if (uint32_t sid_v = pda_screen_.consume_skill_side_effect_request(); sid_v != 0) {
            apply_skill_side_effects(*this, static_cast<SkillId>(sid_v));
        }
        auto installed_slot = pda_screen_.consume_installed_ship_slot();
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

    // Cell picker intercepts when open
    if (handle_cell_picker_input(key)) return;

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
            pda_screen_.open(&player_, renderer_.get(), &quest_manager_,
                                   world_.navigation().on_ship, PdaTab::Ship,
                                   can_board_ship(), &world_, this, &hacking_);
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
            pda_screen_.open(&player_, renderer_.get(), &quest_manager_,
                                   world_.navigation().on_ship,
                                   PdaTab::Skills, can_board_ship(), &world_,
                                   this, &hacking_);
        }
        // Plan 7: (hack) Shell Access doorway autotypes `ssh <user>@<ip>`
        // into the PDA's pda> input buffer, which leaves an ssh request on
        // the PDA queue. Drain it now so the device shell opens immediately
        // without requiring a follow-up keypress.
        if (pda_screen_.is_open()) {
            bool as_root = true;
            if (uint32_t ip_v = pda_screen_.consume_ssh_request(as_root); ip_v != 0) {
                Hackable* h = world_.find_hackable_by_ip(ip_v);
                if (h) {
                    ShellTier tier = as_root ? ShellTier::Root : ShellTier::Guest;
                    hacking_.open_device_shell(*this, *h, tier,
                                               ShellVia::RealWorld,
                                               /*manual_ssh=*/true,
                                               as_root ? "root" : "guest");
                }
            }
        }
        return;
    }


    // Look mode intercept
    // When tether_targeting_ is active the look cursor is used for target pick.
    // Enter confirms the Tether; Esc cancels both tether mode and look mode.
    if (input_.looking()) {
        if (tether_targeting_) {
            if (key == '\n' || key == '\r') {
                confirm_tether_targeting();
                compute_camera();
                return;
            }
            if (key == '\033') {
                tether_targeting_ = false;
                input_.cancel_look();
                log("Tether cancelled.");
                compute_camera();
                return;
            }
        }
        input_.handle_look_input(key, world_.map().width(), world_.map().height());
        compute_camera(); // follow look cursor, or snap back to player on exit
        return;
    }

    // Telegraph mode intercept — consumes every key while active,
    // mirroring the combat-targeting pattern below so players can't
    // accidentally trigger other actions mid-telegraph.
    if (telegraph_.active()) {
        telegraph_.handle_input(key, *this);
        return;
    }

    // QH program picker intercept — handle BEFORE other key consumers.
    if (qh_picker_.open) {
        MenuResult r = qh_picker_.handle_input(key);
        if (r == MenuResult::Selected) {
            int slot_idx = qh_picker_slots_[qh_picker_.selection];
            auto* deck_slot_ptr = player_.equipment.equipped_cyberdeck();
            auto& deck = *(*deck_slot_ptr)->deck;
            const auto& slot = deck.loaded[slot_idx];
            Item probe = build_by_def_id(slot.program_def_id);

            int tx = qh_picker_target_x_, ty = qh_picker_target_y_;
            Hackable* hack = nullptr;
            if (world_.map().get(tx, ty) == Tile::Fixture) {
                int fid = world_.map().fixture_id(tx, ty);
                if (fid >= 0 && world_.map().fixture(fid).cyber) {
                    hack = &*world_.map().fixture_mut(fid).cyber;
                }
            }
            if (!hack) {
                for (auto& npc : world_.npcs()) {
                    if (npc.x == tx && npc.y == ty && npc.cyber && npc.alive()) {
                        hack = &*npc.cyber;
                        break;
                    }
                }
            }
            if (hack) {
                std::string msg = hacking_.execute_quickhack(*this, probe, *hack, tx, ty);
                log(msg);
                advance_world(ActionCost::interact);
            }
            qh_picker_.open = false;
        } else if (r == MenuResult::Closed) {
            qh_picker_.open = false;
        }
        return;
    }

    // Quickhack targeting mode intercept (mirrors combat targeting).
    if (hacking_.targeting()) {
        hacking_.handle_targeting_input(key, *this);
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

    // Plan 7: while body is wired into a device, block movement/attack/item-
    // use. The shell intercept above takes care of input while the shell is
    // open; if the shell closed but the wired-in flag wasn't cleared, we
    // still gate here so corrupted state can't drive the avatar around.
    if (player_.is_jacked_into >= 0) {
        // Esc still opens the pause menu; everything else logs and bails.
        if (key != '\033') {
            log("Body is wired in. Esc to yank cable.");
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
                log("Nothing to interact with here.");
                break;
            }
            use_action();
            break;
        case KEY_F1: case KEY_F2: case KEY_F3: case KEY_F4: {
            // Toggle widget on/off via configurable F-keys
            for (int i = 0; i < widget_count; ++i) {
                if (key == widget_keys_.keys[i]) {
                    widget_toggle(active_widgets_, static_cast<Widget>(i));
                    // Track enable order
                    if (widget_active(active_widgets_, static_cast<Widget>(i)))
                        widget_order_[i] = ++widget_order_seq_;
                    else
                        widget_order_[i] = 0;
                    // If we toggled off the focused widget, advance focus
                    if (!widget_active(active_widgets_, static_cast<Widget>(focused_widget_))) {
                        for (int step = 0; step < widget_count; ++step) {
                            focused_widget_ = (focused_widget_ + 1) % widget_count;
                            if (widget_active(active_widgets_, static_cast<Widget>(focused_widget_)))
                                break;
                        }
                    }
                    // Auto-show/hide panel based on active widgets
                    if (widget_active(active_widgets_, static_cast<Widget>(i)) && !panel_visible_) {
                        panel_visible_ = true;
                        compute_layout();
                        compute_camera();
                    } else if (active_widgets_ == 0 && panel_visible_) {
                        panel_visible_ = false;
                        compute_layout();
                        compute_camera();
                    }
                    break;
                }
            }
            break;
        }
        case 8: // Ctrl+H
            panel_visible_ = !panel_visible_;
            compute_layout();
            compute_camera();
            break;
        case '\t':
            // nullopt -> PdaScreen reopens on the last-used tab.
            pda_screen_.open(&player_, renderer_.get(), &quest_manager_,
                                   world_.navigation().on_ship,
                                   std::nullopt, can_board_ship(), &world_,
                                   this, &hacking_);
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
        case 'T': use_thrown(); break;
        case 's': combat_.shoot_target(*this); break;
        case 'H': hacking_.begin_quickhack_targeting(*this); break;
        case 'N': begin_tether_targeting(); break;   // D2: Tether action
        case 'r': combat_.recharge_weapon(*this); break;
        case 'b': combat_.recharge_shield(*this); break;
        case 'R': open_cell_picker(/*target_is_shield=*/false); break;
        case 'B': open_cell_picker(/*target_is_shield=*/true); break;
        case 'g': pickup_ground_item(); break;
        case '?': help_screen_.open(); break;
        case 'm':
            if (dev_mode_) {
                star_chart_viewer_.open();
            }
            break;
        case '+': case '=': {
            auto fw = static_cast<Widget>(focused_widget_);
            if (fw == Widget::Wait && widget_active(active_widgets_, Widget::Wait)) {
                if (wait_cursor_ < 5) ++wait_cursor_;
            } else if (fw == Widget::Messages && widget_active(active_widgets_, Widget::Messages)
                       && message_scroll_ > 0) {
                message_scroll_--;
            }
            break;
        }
        case '-': {
            auto fw = static_cast<Widget>(focused_widget_);
            if (fw == Widget::Wait && widget_active(active_widgets_, Widget::Wait)) {
                if (wait_cursor_ > 0) --wait_cursor_;
            } else if (fw == Widget::Messages && widget_active(active_widgets_, Widget::Messages)) {
                message_scroll_++;
            }
            break;
        }
        case '>': {
            if (world_.on_overworld()) {
                Tile t = world_.map().get(player_.x, player_.y);
                if (t == Tile::OW_Lake) {
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
            } else if (world_.surface_mode() == SurfaceMode::Dungeon) {
                // Descend: player must be standing on StairsDown or DungeonHatch.
                int fid = world_.map().fixture_id(player_.x, player_.y);
                if (fid >= 0) {
                    auto ft = world_.map().fixture(fid).type;
                    if (ft == FixtureType::StairsDown || ft == FixtureType::StairsDownPrecursor || ft == FixtureType::DungeonHatch) {
                        descend_stairs({player_.x, player_.y});
                        break;
                    }
                }
                log("There are no stairs down here.");
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
                // If standing on StairsUp at depth > 0, ascend within the
                // dungeon; otherwise exit to the detail map.
                int fid = world_.map().fixture_id(player_.x, player_.y);
                bool on_stairs_up = false;
                if (fid >= 0 &&
                    world_.map().fixture(fid).type == FixtureType::StairsUp) {
                    on_stairs_up = true;
                }
                if (on_stairs_up && world_.navigation().current_depth > 0) {
                    ascend_stairs();
                } else {
                    exit_dungeon_to_detail();
                }
            }
            break;
        }
        case KEY_PAGE_UP:
            ability_bar::page_up(ability_bar_row_, player_);
            break;
        case KEY_PAGE_DOWN:
            ability_bar::page_down(ability_bar_row_, player_);
            break;
        case '\n': case '\r':
        case '1': case '2': case '3': case '4': case '5': case '6':
        case '7': case '8': case '9': {
            bool wait_focused = static_cast<Widget>(focused_widget_) == Widget::Wait
                                && widget_active(active_widgets_, Widget::Wait);
            // Number keys 1..kSlotsPerRow: abilities (unless Wait widget is focused)
            if (key >= '1' && key <= ('0' + ability_bar::kSlotsPerRow) && !wait_focused) {
                ability_bar::use_slot(*this, ability_bar_row_, key - '1');
                break;
            }
            // Digits above kSlotsPerRow fall through to the wait-widget handler below
            // (so e.g. the wait widget can still use 1-6). Non-wait presses of those
            // digits are no-ops.
            if (key > ('0' + ability_bar::kSlotsPerRow) && key <= '9' && !wait_focused) break;
            // Overworld: enter detail map for the tile underneath the player
            if (world_.on_overworld() && (key == '\n' || key == '\r')) {
                Tile t = world_.map().get(player_.x, player_.y);
                if (t == Tile::OW_Lake) {
                    log("This terrain cannot be explored on foot.");
                } else {
                    enter_detail_map();
                }
                break;
            }
            if (wait_focused && (key == '\n' || key == '\r' || (key >= '1' && key <= '6'))) {
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


void Game::open_qh_picker(int tx, int ty, const std::vector<int>& menu_slots) {
    qh_picker_.reset();
    qh_picker_.title = "Quickhack:";
    auto* qdeck_slot = player_.equipment.equipped_cyberdeck();
    auto& deck = *(*qdeck_slot)->deck;
    for (size_t i = 0; i < menu_slots.size(); ++i) {
        const auto& slot = deck.loaded[menu_slots[i]];
        Item probe = build_by_def_id(slot.program_def_id);
        const ProgramDef* def = find_program(probe.program->id);
        char k = static_cast<char>('a' + i);
        std::string label = std::string(def->filename) + "  (" +
                            std::to_string(def->ram_cost) + " RAM)";
        qh_picker_.add_option(k, label);
    }
    qh_picker_.selection = 0;
    qh_picker_.open = true;
    qh_picker_target_x_ = tx;
    qh_picker_target_y_ = ty;
    qh_picker_slots_ = menu_slots;
}

} // namespace astra
