#include "astra/dialog_manager.h"
#include "astra/game.h"
#include "astra/shop.h"

namespace astra {

void DialogManager::close() {
    npc_dialog_.close();
    interacting_npc_ = nullptr;
    dialog_tree_ = nullptr;
    dialog_node_ = -1;
}

bool DialogManager::handle_input(int key, Game& game) {
    if (!npc_dialog_.is_open()) return false;

    // Tab = trade shortcut
    if (key == '\t') {
        if (interacting_npc_ && interacting_npc_->interactions.shop) {
            npc_dialog_.close();
            game.trade_window().open(interacting_npc_, &game.player(), game.renderer());
        } else {
            game.log("They have nothing to sell.");
        }
        return true;
    }
    // [l] Look — enter look mode focused on NPC
    if (key == 'l' && interacting_npc_) {
        npc_dialog_.close();
        // Can't call input_.begin_look_at directly — Game handles this
        // Store NPC position for Game to pick up
        close();
        return true; // Game checks and starts look mode
    }
    MenuResult result = npc_dialog_.handle_input(key);
    if (result == MenuResult::Selected) {
        advance_dialog(npc_dialog_.selected(), game);
    } else if (result == MenuResult::Closed) {
        close();
    }
    return true;
}

void DialogManager::draw(Renderer* renderer, int screen_w, int screen_h) {
    npc_dialog_.draw(renderer, screen_w, screen_h);
}

void DialogManager::interact_fixture(int fid, Game& game) {
    auto& f = game.world().map().fixture_mut(fid);

    switch (f.type) {
        case FixtureType::HealPod: {
            if (f.last_used_tick >= 0 && f.cooldown > 0) {
                int elapsed = game.world().world_tick() - f.last_used_tick;
                if (elapsed < f.cooldown) {
                    int remaining = f.cooldown - elapsed;
                    game.log("The healing pod is recharging (" +
                        std::to_string(remaining) + " ticks remaining).");
                    return;
                }
            }
            if (game.player().hp >= game.player().max_hp) {
                game.log("You step into the healing pod. No injuries detected.");
                return;
            }
            game.player().hp = game.player().max_hp;
            f.last_used_tick = game.world().world_tick();
            game.log("Nanites flood the pod. Your wounds close and vitals normalize. Fully healed.");
            break;
        }
        case FixtureType::FoodTerminal: {
            auto menu = food_terminal_menu();
            npc_dialog_.close();
            npc_dialog_.set_title("Food Terminal");
            npc_dialog_body_ = "What'll it be?";
            npc_dialog_.set_body("\"What'll it be?\"");
            game.log("Food Terminal: What'll it be?");
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
            npc_dialog_.set_max_width_frac(0.45f);
            npc_dialog_.open();
            interacting_npc_ = nullptr;
            dialog_tree_ = nullptr;
            dialog_node_ = -2; // sentinel: food terminal mode
            break;
        }
        case FixtureType::RestPod: {
            if (f.last_used_tick >= 0 && f.cooldown > 0) {
                int elapsed = game.world().world_tick() - f.last_used_tick;
                if (elapsed < f.cooldown) {
                    int remaining = f.cooldown - elapsed;
                    game.log("The rest pod needs to reset (" +
                        std::to_string(remaining) + " ticks remaining).");
                    return;
                }
            }
            game.player().hp = game.player().max_hp;
            f.last_used_tick = game.world().world_tick();
            game.advance_world(20);
            game.log("You climb into the rest pod and sleep deeply. Fully restored.");
            break;
        }
        case FixtureType::RepairBench: {
            game.open_repair_bench();
            break;
        }
        case FixtureType::SupplyLocker: {
            npc_dialog_.close();
            npc_dialog_.set_title("Supply Locker");
            npc_dialog_body_ = "Stash (" + std::to_string(game.world().stash().size()) + "/" +
                std::to_string(WorldManager::max_stash_size) + ")";
            game.log(npc_dialog_body_);
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
            game.star_chart_viewer().open();
            game.log("The star chart hums to life, projecting a holographic galaxy map.");
            break;
        }
        case FixtureType::WeaponDisplay: {
            game.log("Weapons gleam behind reinforced glass. Talk to the Arms Dealer to browse.");
            break;
        }
        case FixtureType::ShipTerminal: {
            npc_dialog_.close();
            npc_dialog_.set_title("Shipping Terminal");
            npc_dialog_body_ = "Your starship is docked and ready.";
            npc_dialog_.set_body("\"" + npc_dialog_body_ + "\"");
            game.log(npc_dialog_body_);
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
                game.log("The door is locked.");
                break;
            }
            if (f.open) {
                f.open = false;
                f.passable = false;
                f.glyph = '+';
                f.utf8_glyph = nullptr;
                game.log("You close the door.");
            } else {
                f.open = true;
                f.passable = true;
                f.glyph = '/';
                f.utf8_glyph = nullptr;
                game.log("You open the door.");
            }
            break;
        }
        default:
            game.log("Nothing happens.");
            break;
    }
}

void DialogManager::open_npc_dialog(Npc& npc, Game& game) {
    interacting_npc_ = &npc;
    dialog_tree_ = nullptr;
    dialog_node_ = -1;
    interact_options_.clear();

    const auto& data = npc.interactions;
    npc_dialog_.close();
    npc_dialog_.set_title(npc.display_name());
    npc_dialog_body_ = data.talk ? data.talk->greeting : "";
    if (!npc_dialog_body_.empty()) {
        npc_dialog_.set_body("\"" + npc_dialog_body_ + "\"");
        game.log(npc.display_name() + ": \"" + npc_dialog_body_ + "\"");
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
    npc_dialog_.set_max_width_frac(0.45f);
    npc_dialog_.open();
}

void DialogManager::advance_dialog(int selected, Game& game) {
    // Food terminal dialog (no NPC involved)
    if (dialog_node_ == -2) {
        dialog_node_ = -1;
        dialog_tree_ = nullptr;
        auto menu = food_terminal_menu();
        if (selected >= 0 && selected < static_cast<int>(menu.size())) {
            auto result = buy_food_item(game.player(), menu[selected]);
            game.log(result.message);
        }
        // last option or out of range: Leave
        return;
    }

    // Stash main menu
    if (dialog_node_ == -3) {
        if (selected == 0) {
            // Store mode — show player inventory
            if (game.player().inventory.items.empty()) {
                game.log("You have nothing to store.");
                dialog_node_ = -1;
                return;
            }
            if (static_cast<int>(game.world().stash().size()) >= WorldManager::max_stash_size) {
                game.log("Stash is full! (" + std::to_string(WorldManager::max_stash_size) +
                    "/" + std::to_string(WorldManager::max_stash_size) + ")");
                dialog_node_ = -1;
                return;
            }
            npc_dialog_.close();
            npc_dialog_.set_title("Store Item");
            game.log("Select item to store (" + std::to_string(game.world().stash().size()) +
                "/" + std::to_string(WorldManager::max_stash_size) + "):");
            { char sk = '1';
            for (const auto& item : game.player().inventory.items) {
                npc_dialog_.add_option(sk++, item.name);
                if (sk > '9') sk = 'a';
            } }
            npc_dialog_.add_option('c', "Cancel");
            npc_dialog_.set_max_width_frac(0.45f);
            npc_dialog_.open();
            dialog_node_ = -4; // sentinel: store mode
        } else if (selected == 1) {
            // Retrieve mode — show stash contents
            if (game.world().stash().empty()) {
                game.log("The stash is empty.");
                dialog_node_ = -1;
                return;
            }
            npc_dialog_.close();
            npc_dialog_.set_title("Retrieve Item");
            game.log("Select item to retrieve (" + std::to_string(game.world().stash().size()) +
                "/" + std::to_string(WorldManager::max_stash_size) + "):");
            { char rk = '1';
            for (const auto& item : game.world().stash()) {
                npc_dialog_.add_option(rk++, item.name);
                if (rk > '9') rk = 'a';
            } }
            npc_dialog_.add_option('c', "Cancel");
            npc_dialog_.set_max_width_frac(0.45f);
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
        int item_count = static_cast<int>(game.player().inventory.items.size());
        if (selected >= 0 && selected < item_count) {
            Item stored = std::move(game.player().inventory.items[selected]);
            game.player().inventory.items.erase(
                game.player().inventory.items.begin() + selected);
            game.log("Stored " + stored.name + " in the stash.");
            game.world().stash().push_back(std::move(stored));
        }
        return;
    }

    // Stash retrieve mode — player selected a stash item to retrieve
    if (dialog_node_ == -5) {
        dialog_node_ = -1;
        int stash_count = static_cast<int>(game.world().stash().size());
        if (selected >= 0 && selected < stash_count) {
            Item retrieved = std::move(game.world().stash()[selected]);
            game.world().stash().erase(game.world().stash().begin() + selected);
            if (!game.player().inventory.can_add(retrieved)) {
                game.log("Too heavy! Can't carry " + retrieved.name + ".");
                game.world().stash().insert(game.world().stash().begin() + selected, std::move(retrieved));
            } else {
                game.log("Retrieved " + retrieved.name + " from the stash.");
                game.player().inventory.items.push_back(std::move(retrieved));
            }
        }
        return;
    }

    // Dev menu — main
    if (dialog_node_ == -7) {
        dialog_node_ = -1;
        dialog_tree_ = nullptr;
        if (selected == 0) {
            game.dev_command_warp_random();
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
                                   (has_effect(game.player().effects, EffectId::Invulnerable) ? "ON" : "OFF"));
            npc_dialog_.add_option('b', "Back");
            npc_dialog_.open();
            dialog_node_ = -8; // sentinel: dev stats
        } else if (selected == 3) {
            // Force level up
            game.player().xp = game.player().max_xp;
            game.dev_command_level_up();
        }
        return;
    }

    // Dev menu — character stats
    if (dialog_node_ == -8) {
        if (selected == 0) {
            if (has_effect(game.player().effects, EffectId::Invulnerable))
                remove_effect(game.player().effects, EffectId::Invulnerable);
            else
                add_effect(game.player().effects, make_invulnerable());
            bool inv = has_effect(game.player().effects, EffectId::Invulnerable);
            game.log(std::string("[DEV] Invulnerability: ") + (inv ? "ON" : "OFF"));
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
            game.dev_command_warp_stamp(poi_types[selected]);
            game.log(std::string("[DEV] Stamp Test: ") + poi_names[selected]);
        }
        return;
    }

    // Ship terminal dialog
    if (dialog_node_ == -6) {
        dialog_node_ = -1;
        dialog_tree_ = nullptr;
        if (selected == 0) {
            game.enter_ship();
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
            open_npc_dialog(*interacting_npc_, game);
            return;
        }

        // Advance to next node
        dialog_node_ = next;
        const auto& next_node = (*dialog_tree_)[dialog_node_];
        npc_dialog_.close();
        npc_dialog_.set_title(interacting_npc_->display_name());
        npc_dialog_body_ = next_node.text;
        npc_dialog_.set_body("\"" + next_node.text + "\"");
        game.log(interacting_npc_->display_name() + ": \"" + next_node.text + "\"");
        { char hk = '1';
        for (const auto& choice : next_node.choices) {
            npc_dialog_.add_option(hk++, choice.label);
        } }
        npc_dialog_.set_footer("[Space] Select  [Esc] Close");
        npc_dialog_.set_max_width_frac(0.45f);
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
            npc_dialog_.set_body("\"" + node.text + "\"");
            game.log(interacting_npc_->display_name() + ": \"" + node.text + "\"");
            { char hk = '1';
            for (const auto& choice : node.choices) {
                npc_dialog_.add_option(hk++, choice.label);
            } }
            npc_dialog_.set_footer("[Space] Select  [Esc] Close");
            npc_dialog_.set_max_width_frac(0.45f);
            npc_dialog_.open();
            break;
        }
        case InteractOption::Shop:
            game.trade_window().open(interacting_npc_, &game.player(), game.renderer());
            npc_dialog_.close();
            break;

        case InteractOption::Quest: {
            dialog_tree_ = &interacting_npc_->interactions.quest->nodes;
            dialog_node_ = 0;
            const auto& node = (*dialog_tree_)[0];
            npc_dialog_.close();
            npc_dialog_.set_title(interacting_npc_->display_name());
            npc_dialog_body_ = node.text;
            npc_dialog_.set_body("\"" + node.text + "\"");
            game.log(interacting_npc_->display_name() + ": \"" + node.text + "\"");
            { char hk = '1';
            for (const auto& choice : node.choices) {
                npc_dialog_.add_option(hk++, choice.label);
            } }
            npc_dialog_.set_footer("[Space] Select  [Esc] Close");
            npc_dialog_.set_max_width_frac(0.45f);
            npc_dialog_.open();
            break;
        }
        case InteractOption::Farewell:
            game.log("\"Safe travels, commander.\"");
            interacting_npc_ = nullptr;
            dialog_tree_ = nullptr;
            dialog_node_ = -1;
            break;
    }
}

} // namespace astra
