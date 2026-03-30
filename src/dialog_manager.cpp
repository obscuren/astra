#include "astra/dialog_manager.h"
#include "astra/character.h"
#include "astra/game.h"
#include "astra/item_defs.h"
#include "astra/player.h"
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
            game.star_chart_viewer().set_view_only(true);
            game.star_chart_viewer().open();
            game.log("The star chart hums to life, projecting a holographic galaxy map.");
            break;
        }
        case FixtureType::WeaponDisplay: {
            game.log("Weapons gleam behind reinforced glass. Talk to the Arms Dealer to browse.");
            break;
        }
        case FixtureType::StairsUp: {
            // General dungeon exit — return to previous location
            if (game.world().map().location_name() == "Maintenance Tunnels") {
                game.exit_maintenance_tunnels();
            } else {
                game.exit_dungeon_to_detail();
            }
            break;
        }
        case FixtureType::DungeonHatch: {
            // If we're in the tunnels, go back up
            if (game.world().map().location_name() == "Maintenance Tunnels") {
                game.exit_maintenance_tunnels();
                break;
            }
            // Gated by tutorial quest
            if (game.quests().has_active_quest("story_getting_airborne")) {
                game.enter_maintenance_tunnels();
            } else {
                game.log("Maintenance Tunnels -- Currently Under Maintenance.");
            }
            break;
        }
        case FixtureType::CommandTerminal: {
            // Trigger quest tracking for "talk to ARIA" objectives
            game.quests().on_npc_talked("ARIA");

            // Check if tutorial quest is ready for turn-in
            auto* tq = game.quests().find_active("story_getting_airborne");
            if (tq && tq->all_objectives_complete()) {
                // Grab reward info before complete_quest moves the quest
                int reward_xp = tq->reward.xp;
                int reward_credits = tq->reward.credits;
                game.quests().complete_quest("story_getting_airborne", game.player());
                auto* sq = find_story_quest("story_getting_airborne");
                if (sq) sq->on_completed(game);
                game.log("Quest complete: " + colored("Getting Airborne", Color::Yellow));
                std::string reward_msg = "Reward:";
                if (reward_xp > 0) reward_msg += " " + colored(std::to_string(reward_xp) + " XP", Color::Cyan);
                if (reward_credits > 0) reward_msg += " " + colored(std::to_string(reward_credits) + "$", Color::Yellow);
                game.log(reward_msg);

                // Show completion dialog
                npc_dialog_.close();
                npc_dialog_.set_title("ARIA");
                npc_dialog_.set_body(
                    "\"All primary systems restored. We're flight-ready, "
                    "commander.\n\n"
                    "The galaxy awaits. Plot a course from the " +
                    colored("Star Chart", Color::Cyan) +
                    " whenever you're ready. I'll be here.\"");
                npc_dialog_.add_option('f', "Let's fly.");
                npc_dialog_.set_footer("[Space] Continue");
                npc_dialog_.set_max_width_frac(0.5f);
                npc_dialog_.open();
                interacting_npc_ = nullptr;
                dialog_tree_ = nullptr;
                dialog_node_ = -1;
                return;
            }

            npc_dialog_.close();
            npc_dialog_.set_title("ARIA");
            // Greeting varies by ship state
            auto& ship = game.player().ship;
            bool has_engine = ship.operational();
            bool has_hull = ship.hull.has_value();
            bool has_navi = ship.has_navigation();
            std::string greeting;
            if (!has_engine && !has_hull && !has_navi)
                greeting = "Welcome back. I'd run diagnostics but half my systems are offline. Let's fix that.";
            else if (has_engine && has_hull && has_navi)
                greeting = "All systems nominal. Where to, commander?";
            else
                greeting = "Systems partially restored. We're getting there, commander.";
            npc_dialog_.set_body("\"" + greeting + "\"");
            game.log("ARIA: \"" + greeting + "\"");
            char hotkey = '1';
            npc_dialog_.add_option(hotkey++, "Ship Systems");
            npc_dialog_.add_option(hotkey++, "Star Chart");
            if (game.world().navigation().at_station)
                npc_dialog_.add_option(hotkey++, "Disembark");
            npc_dialog_.add_option('f', "Close");
            npc_dialog_.set_footer("[Space] Select  [Esc] Close");
            npc_dialog_.set_max_width_frac(0.45f);
            npc_dialog_.open();
            interacting_npc_ = nullptr;
            dialog_tree_ = nullptr;
            dialog_node_ = -10; // sentinel: ARIA terminal
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

void DialogManager::show_tutorial_choice(Game& game) {
    npc_dialog_.close();
    npc_dialog_.set_title("ARIA");
    npc_dialog_.set_body(
        "\"Systems critical. Multiple component failures detected. "
        "Engine, hull plating, and navigation computer are offline. "
        "We're grounded until repairs are complete, commander.\"");
    npc_dialog_.add_option('1', "I need to find parts to fix this ship. (Begin tutorial)");
    npc_dialog_.add_option('2', "I know what I'm doing. (Skip tutorial)");
    npc_dialog_.set_footer("[Space] Select");
    npc_dialog_.set_max_width_frac(0.5f);
    npc_dialog_.open();
    interacting_npc_ = nullptr;
    dialog_tree_ = nullptr;
    dialog_node_ = -11; // sentinel: tutorial choice
}

void DialogManager::show_tutorial_followup() {
    npc_dialog_.close();
    npc_dialog_.set_title("ARIA");
    npc_dialog_.set_body(
        "\"Understood. Let's get to work.\n\n"
        "I'd start with the " +
        colored("Station Keeper", Color::White) + " " +
        colored("(K)", Color::Green) +
        ". He's been here since before the Collapse -- he'll know "
        "where to find parts.\n\n"
        "Check your " +
        colored("Datapad", Color::Cyan) + " (" +
        colored("c", Color::Yellow) +
        ") to track your objectives.\"");
    npc_dialog_.add_option('f', "Got it, I'll check my Datapad.");
    npc_dialog_.set_footer("[Space] Continue");
    npc_dialog_.set_max_width_frac(0.5f);
    npc_dialog_.open();
    interacting_npc_ = nullptr;
    dialog_tree_ = nullptr;
    dialog_node_ = -12; // sentinel: tutorial followup — opens datapad on dismiss
}

void DialogManager::open_npc_dialog(Npc& npc, Game& game) {
    interacting_npc_ = &npc;
    dialog_tree_ = nullptr;
    dialog_node_ = -1;
    interact_options_.clear();

    const auto& data = npc.interactions;
    npc_dialog_.close();
    npc_dialog_.set_title(npc.display_name());

    // Faction gate: Hated NPCs refuse all interaction
    if (!npc.faction.empty()) {
        int rep = reputation_for(game.player(), npc.faction);
        if (reputation_tier(rep) == ReputationTier::Hated) {
            npc_dialog_.set_body("\"I have nothing to say to you. Get lost.\"");
            game.log(npc.display_name() + " refuses to speak with you.");
            npc_dialog_.add_option('f', "Leave");
            interact_options_.push_back(InteractOption::Farewell);
            npc_dialog_.set_footer("[Space] Select  [Esc] Close");
            npc_dialog_.set_max_width_frac(0.45f);
            npc_dialog_.open();
            return;
        }
    }

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

    // Check for turnable-in quests from this NPC
    bool has_turnin = false;
    for (const auto& q : game.quests().active_quests()) {
        if (q.giver_npc == npc.role && q.ready_for_turnin()) {
            has_turnin = true;
            break;
        }
    }
    if (has_turnin) {
        npc_dialog_.add_option(hotkey++, "I have news about the job.");
        interact_options_.push_back(InteractOption::QuestTurnIn);
    }

    // Offer new quests if none active from this NPC and reputation is Neutral+
    if (data.quest) {
        bool has_active_from_npc = false;
        for (const auto& q : game.quests().active_quests()) {
            if (q.giver_npc == npc.role) { has_active_from_npc = true; break; }
        }
        int rep = reputation_for(game.player(), npc.faction);
        bool rep_ok = npc.faction.empty() || reputation_tier(rep) >= ReputationTier::Neutral;
        if (!has_active_from_npc && rep_ok) {
            npc_dialog_.add_option(hotkey++, data.quest->quest_intro);
            interact_options_.push_back(InteractOption::Quest);
        }
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

    // Tutorial followup — "Got it, I'll check my Datapad" opens character screen
    if (dialog_node_ == -12) {
        npc_dialog_.close();
        dialog_node_ = -1;
        dialog_tree_ = nullptr;
        aria_open_datapad_ = true;
        return;
    }

    // Ship terminal dialog
    // Tutorial choice
    if (dialog_node_ == -11) {
        npc_dialog_.close();
        dialog_node_ = -1;
        dialog_tree_ = nullptr;
        if (selected == 0) {
            // Play tutorial — accept quest, ship stays empty
            auto* sq = find_story_quest("story_getting_airborne");
            if (sq) {
                auto q = sq->create_quest();
                game.log("Quest accepted: " + q.title);
                game.quests().accept_quest(std::move(q), game.world().world_tick());
            }
            game.log("ARIA: \"Understood. Let's get to work.\"");
            game.log("ARIA: \"I'd start with the " + colored("Station Keeper", Color::White)
                + " " + colored("(K)", Color::Green)
                + ". He's been here since before the Collapse — he'll know where to find parts.\"");
            game.log("ARIA: \"Check your " + colored("Datapad", Color::Cyan) + " ("
                + colored("c", Color::Yellow) + ") to track your objectives.\"");
            // Queue follow-up dialog for next frame
            aria_tutorial_followup_ = true;
        } else {
            // Skip tutorial — equip ship with starter components
            game.player().ship.engine = build_engine_coil_mk1();
            game.player().ship.hull = build_hull_plate();
            game.player().ship.navi_computer = build_navi_computer_mk2();
            game.log("ARIA: \"All systems nominal. Where to, commander?\"");
        }
        return;
    }

    // ARIA command terminal
    if (dialog_node_ == -10) {
        dialog_node_ = -1;
        dialog_tree_ = nullptr;
        if (selected == 0) {
            // Ship Systems → open character screen on Ship tab
            aria_open_ship_tab_ = true;
        } else if (selected == 1) {
            // Star Chart
            if (game.player().ship.operational()) {
                aria_open_star_chart_ = true;
            } else {
                game.log("ARIA: \"I need an engine to plot routes. I'm an AI, not a fortune teller.\"");
            }
        } else if (selected == 2) {
            // Disembark — exit ship to station
            aria_disembark_ = true;
        }
        return;
    }

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

        // Check for quest acceptance: transitioning from node 0 to node 1
        // in a quest dialog tree means the player accepted the quest
        if (dialog_tree_ == &interacting_npc_->interactions.quest->nodes &&
            dialog_node_ == 0 && next == 1) {
            // Station Commander during tutorial: reward is Nav Computer
            if (interacting_npc_->role == "Station Commander" &&
                game.quests().has_active_quest("story_getting_airborne")) {
                Item navi = build_navi_computer_mk2();
                game.player().ship.cargo.push_back(std::move(navi));
                game.log("The Commander hands you a " +
                    colored("Navi Computer Mk2", Color::White) + ".");
                game.log("Stored in ship cargo.");
                // Don't accept a quest — this is a direct reward
            }
            // Try story quest (Station Keeper) — only after tutorial
            else if (auto* sq = find_story_quest("story_missing_hauler");
                sq && interacting_npc_->role == "Station Keeper" &&
                !game.quests().has_active_quest("story_missing_hauler") &&
                !game.quests().has_active_quest("story_getting_airborne")) {
                auto q = sq->create_quest();
                game.quests().accept_quest(std::move(q), game.world().world_tick());
                sq->on_accepted(game);
                game.log("Quest accepted: " + colored("The Missing Hauler", Color::Yellow));
            } else {
                // Generate a random quest based on NPC role + world state
                auto q = game.quests().generate_quest_for_role(
                    interacting_npc_->role, interacting_npc_->display_name(),
                    game.world().navigation(), game.world().rng());
                q.giver_npc = interacting_npc_->role;
                // Register map marker if quest has a target location
                if (q.target_system_id != 0) {
                    LocationKey mk = {q.target_system_id, q.target_body_index, -1, false, -1, -1, 0, -1, -1};
                    QuestLocationMeta meta;
                    meta.quest_id = q.id;
                    meta.quest_title = q.title;
                    meta.target_system_id = q.target_system_id;
                    meta.target_body_index = q.target_body_index;
                    meta.remove_on_completion = true;
                    game.world().quest_locations()[mk] = std::move(meta);
                }
                game.log("Quest accepted: " + colored(q.title, Color::Yellow));
                game.quests().accept_quest(std::move(q), game.world().world_tick());
            }
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
        case InteractOption::QuestTurnIn: {
            // Find the completable quest from this NPC and turn it in
            std::string turn_in_id;
            std::string quest_title;
            for (const auto& q : game.quests().active_quests()) {
                if (q.giver_npc == interacting_npc_->role && q.ready_for_turnin()) {
                    turn_in_id = q.id;
                    quest_title = q.title;
                    break;
                }
            }
            if (!turn_in_id.empty()) {
                // Notify talk objective (now all prior are done, so it will complete)
                game.quests().on_npc_talked(interacting_npc_->role);

                // Find quest rewards before completing (for the log message)
                const Quest* qptr = game.quests().find_active(turn_in_id);
                QuestReward reward;
                if (qptr) reward = qptr->reward;

                // Complete the quest
                game.quests().complete_quest(turn_in_id, game.player());

                // Log reward details
                game.log("Quest completed: " + colored(quest_title, Color::Green));
                std::string reward_msg = "Received:";
                if (reward.xp > 0) reward_msg += " " + colored(std::to_string(reward.xp) + " XP", Color::Cyan);
                if (reward.credits > 0) reward_msg += " " + colored(std::to_string(reward.credits) + " credits", Color::Yellow);
                if (reward.skill_points > 0) reward_msg += " " + colored(std::to_string(reward.skill_points) + " SP", Color::Cyan);
                if (!reward.faction_name.empty() && reward.reputation_change != 0)
                    reward_msg += " " + colored("+" + std::to_string(reward.reputation_change) + " " + reward.faction_name + " rep", Color::Green);
                game.log(reward_msg);

                // Trigger story quest cleanup
                auto* sq = find_story_quest(turn_in_id);
                if (sq) sq->on_completed(game);

                // Clean up quest location markers by quest_id
                auto& ql = game.world().quest_locations();
                for (auto it = ql.begin(); it != ql.end(); ) {
                    if (it->second.quest_id == turn_in_id)
                        it = ql.erase(it);
                    else
                        ++it;
                }

                // Show NPC response
                npc_dialog_.close();
                npc_dialog_.set_title(interacting_npc_->display_name());
                std::string reply = "Well done, commander. You've earned your pay.";
                npc_dialog_.set_body("\"" + reply + "\"");
                game.log(interacting_npc_->display_name() + ": \"" + reply + "\"");
                npc_dialog_.add_option('1', "Thanks.");
                npc_dialog_.set_footer("[Space] Select  [Esc] Close");
                npc_dialog_.set_max_width_frac(0.45f);
                npc_dialog_.open();
                // Set dialog to return to top-level on next selection
                dialog_tree_ = nullptr;
                dialog_node_ = -1;
            }
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
