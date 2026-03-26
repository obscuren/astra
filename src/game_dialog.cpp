#include "astra/game.h"
#include "astra/shop.h"

namespace astra {

void Game::interact_fixture(int fid) {
    auto& f = world_.map().fixture_mut(fid);

    switch (f.type) {
        case FixtureType::HealPod: {
            if (f.last_used_tick >= 0 && f.cooldown > 0) {
                int elapsed = world_.world_tick() - f.last_used_tick;
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
            f.last_used_tick = world_.world_tick();
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
                int elapsed = world_.world_tick() - f.last_used_tick;
                if (elapsed < f.cooldown) {
                    int remaining = f.cooldown - elapsed;
                    log("The rest pod needs to reset (" +
                        std::to_string(remaining) + " ticks remaining).");
                    return;
                }
            }
            player_.hp = player_.max_hp;
            f.last_used_tick = world_.world_tick();
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
            npc_dialog_body_ = "Stash (" + std::to_string(world_.stash().size()) + "/" +
                std::to_string(WorldManager::max_stash_size) + ")";
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
            if (static_cast<int>(world_.stash().size()) >= WorldManager::max_stash_size) {
                log("Stash is full! (" + std::to_string(WorldManager::max_stash_size) +
                    "/" + std::to_string(WorldManager::max_stash_size) + ")");
                dialog_node_ = -1;
                return;
            }
            npc_dialog_.close();
            npc_dialog_.set_title("Store Item");
            log("Select item to store (" + std::to_string(world_.stash().size()) +
                "/" + std::to_string(WorldManager::max_stash_size) + "):");
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
            if (world_.stash().empty()) {
                log("The stash is empty.");
                dialog_node_ = -1;
                return;
            }
            npc_dialog_.close();
            npc_dialog_.set_title("Retrieve Item");
            log("Select item to retrieve (" + std::to_string(world_.stash().size()) +
                "/" + std::to_string(WorldManager::max_stash_size) + "):");
            { char rk = '1';
            for (const auto& item : world_.stash()) {
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
            world_.stash().push_back(std::move(stored));
        }
        return;
    }

    // Stash retrieve mode — player selected a stash item to retrieve
    if (dialog_node_ == -5) {
        dialog_node_ = -1;
        int stash_count = static_cast<int>(world_.stash().size());
        if (selected >= 0 && selected < stash_count) {
            Item retrieved = std::move(world_.stash()[selected]);
            world_.stash().erase(world_.stash().begin() + selected);
            if (!player_.inventory.can_add(retrieved)) {
                log("Too heavy! Can't carry " + retrieved.name + ".");
                world_.stash().insert(world_.stash().begin() + selected, std::move(retrieved));
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

} // namespace astra
