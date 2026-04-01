#include "astra/dialog_manager.h"
#include "astra/character.h"
#include "astra/game.h"
#include "astra/item_defs.h"
#include "astra/player.h"
#include "astra/shop.h"

namespace astra {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void DialogManager::reset_content(const std::string& title, float width_frac) {
    title_ = title;
    body_.clear();
    options_.clear();
    hotkeys_.clear();
    selected_ = 0;
    footer_.clear();
    max_width_frac_ = width_frac;
}

void DialogManager::add_option(char key, const std::string& label) {
    hotkeys_.push_back(key);
    options_.push_back(label);
}

std::vector<std::string> DialogManager::word_wrap(const std::string& text, int width) {
    std::vector<std::string> lines;
    if (width < 4) width = 4;

    std::string line;
    int vis_len = 0;
    int last_space_pos = -1;

    for (size_t i = 0; i < text.size(); ++i) {
        char ch = text[i];
        if (ch == '\n') {
            lines.push_back(line);
            line.clear();
            vis_len = 0;
            last_space_pos = -1;
            continue;
        }
        if (ch == COLOR_BEGIN && i + 1 < text.size()) {
            line += ch;
            line += text[++i];
            continue;
        }
        if (ch == COLOR_END) {
            line += ch;
            continue;
        }
        if (static_cast<unsigned char>(ch) >= 0x80 &&
            (static_cast<unsigned char>(ch) & 0xC0) == 0x80) {
            // UTF-8 continuation byte
            line += ch;
        } else {
            line += ch;
            ++vis_len;
            if (ch == ' ') {
                last_space_pos = static_cast<int>(line.size()) - 1;
            }
        }
        if (vis_len >= width) {
            if (last_space_pos > 0) {
                std::string remainder = line.substr(last_space_pos + 1);
                line.resize(last_space_pos);
                lines.push_back(line);
                line = remainder;
                // Recount visible chars in remainder
                vis_len = 0;
                for (size_t j = 0; j < line.size(); ++j) {
                    if (line[j] == COLOR_BEGIN && j + 1 < line.size()) { ++j; continue; }
                    if (line[j] == COLOR_END) continue;
                    unsigned char uc = static_cast<unsigned char>(line[j]);
                    if (uc >= 0x80 && (uc & 0xC0) == 0x80) continue;
                    ++vis_len;
                }
            } else {
                lines.push_back(line);
                line.clear();
                vis_len = 0;
            }
            last_space_pos = -1;
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

// ---------------------------------------------------------------------------
// Close
// ---------------------------------------------------------------------------

void DialogManager::close() {
    open_ = false;
    interacting_npc_ = nullptr;
    dialog_tree_ = nullptr;
    dialog_node_ = -1;
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

bool DialogManager::handle_input(int key, Game& game) {
    if (!open_) return false;

    // Tab = trade shortcut
    if (key == '\t') {
        if (interacting_npc_ && interacting_npc_->interactions.shop) {
            open_ = false;
            game.trade_window().open(interacting_npc_, &game.player(), game.renderer());
        } else {
            game.log("They have nothing to sell.");
        }
        return true;
    }
    // [l] Look -- enter look mode focused on NPC
    if (key == 'l' && interacting_npc_) {
        close();
        return true; // Game checks and starts look mode
    }

    switch (key) {
        case 27: // Esc
            close();
            return true;
        case KEY_UP: case 'k':
            if (selected_ > 0) selected_--;
            return true;
        case KEY_DOWN: case 'j':
            if (selected_ < static_cast<int>(options_.size()) - 1) selected_++;
            return true;
        case ' ': case '\r': case '\n':
            advance_dialog(selected_, game);
            return true;
        default:
            // Check hotkeys
            for (int i = 0; i < static_cast<int>(hotkeys_.size()); ++i) {
                if (key == hotkeys_[i]) {
                    selected_ = i;
                    advance_dialog(i, game);
                    return true;
                }
            }
            break;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Draw — semantic UI rendering
// ---------------------------------------------------------------------------

void DialogManager::draw(Renderer* renderer, int screen_w, int screen_h) {
    if (!open_ || options_.empty()) return;

    int margin = 4;
    int max_w = static_cast<int>(screen_w * max_width_frac_);
    if (max_w < 30) max_w = 30;
    int win_w = max_w;
    int win_h = screen_h - margin * 2;
    int wx = (screen_w - win_w) / 2;
    int wy = margin;

    UIContext full(renderer, Rect{wx, wy, win_w, win_h});
    auto content = full.panel({
        .title = title_,
        .footer = footer_.empty() ? "[Space] Select  [Esc] Close" : footer_,
    });

    int cw = content.width();
    int ch = content.height();
    int y = 0;

    // Body text with word-wrap and COLOR_BEGIN/COLOR_END support
    if (!body_.empty()) {
        int wrap_w = cw - 2; // 1 char padding on each side
        if (wrap_w < 10) wrap_w = 10;
        auto lines = word_wrap(body_, wrap_w);
        y++; // blank line before body
        for (const auto& line : lines) {
            if (y >= ch) break;
            content.text_rich(1, y, line, Color::Cyan);
            y++;
        }
        y++; // blank line after body

        // Separator between body and options
        if (y < ch) {
            content.sub(Rect{0, y, cw, 1}).separator({});
            y++;
        }
    }

    y++; // blank line before options

    // Build list items from options
    std::vector<ListItem> items;
    for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
        std::string label = "[" + std::string(1, hotkeys_[i]) + "] " + options_[i];
        items.push_back({label, UITag::OptionNormal, i == selected_});
    }

    // Calculate how much vertical space the list gets
    int list_h = ch - y;
    if (list_h > 0) {
        // Compute scroll offset to keep selected item visible
        int scroll = 0;
        if (selected_ >= list_h) scroll = selected_ - list_h + 1;

        auto list_area = content.sub(Rect{0, y, cw, list_h});
        list_area.list({.items = items, .scroll_offset = scroll, .selected_tag = UITag::OptionSelected});
    }
}

// ---------------------------------------------------------------------------
// Fixture interaction
// ---------------------------------------------------------------------------

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
            reset_content("Food Terminal", 0.45f);
            body_ = "\"What'll it be?\"";
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
                add_option(fkey++, label);
            }
            add_option('q', "Leave");
            footer_ = "[Space] Select  [Esc] Close";
            open_ = true;
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
            reset_content("Supply Locker");
            body_ = "Stash (" + std::to_string(game.world().stash().size()) + "/" +
                std::to_string(WorldManager::max_stash_size) + ")";
            game.log(body_);
            add_option('s', "Store an item");
            add_option('r', "Retrieve an item");
            add_option('c', "Close");
            footer_ = "[Space] Select  [Esc] Close";
            open_ = true;
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
            if (game.world().map().location_name() == "Maintenance Tunnels") {
                game.exit_maintenance_tunnels();
            } else {
                game.exit_dungeon_to_detail();
            }
            break;
        }
        case FixtureType::DungeonHatch: {
            if (game.world().map().location_name() == "Maintenance Tunnels") {
                game.exit_maintenance_tunnels();
                break;
            }
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
                reset_content("ARIA", 0.5f);
                body_ =
                    "\"All primary systems restored. We're flight-ready, "
                    "commander.\n\n"
                    "The galaxy awaits. Plot a course from the " +
                    colored("Star Chart", Color::Cyan) +
                    " whenever you're ready. I'll be here.\"";
                add_option('f', "Let's fly.");
                footer_ = "[Space] Continue";
                open_ = true;
                interacting_npc_ = nullptr;
                dialog_tree_ = nullptr;
                dialog_node_ = -1;
                return;
            }

            reset_content("ARIA", 0.45f);
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
            body_ = "\"" + greeting + "\"";
            game.log("ARIA: \"" + greeting + "\"");
            char hotkey = '1';
            add_option(hotkey++, "Ship Systems");
            add_option(hotkey++, "Star Chart");
            if (game.world().navigation().at_station)
                add_option(hotkey++, "Disembark");
            add_option('f', "Close");
            footer_ = "[Space] Select  [Esc] Close";
            open_ = true;
            interacting_npc_ = nullptr;
            dialog_tree_ = nullptr;
            dialog_node_ = -10; // sentinel: ARIA terminal
            break;
        }
        case FixtureType::ShipTerminal: {
            reset_content("Shipping Terminal");
            body_ = "\"Your starship is docked and ready.\"";
            game.log("Your starship is docked and ready.");
            add_option('b', "Board ship");
            add_option('c', "Cancel");
            footer_ = "[Space] Select  [Esc] Close";
            open_ = true;
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
                game.log("You close the door.");
            } else {
                f.open = true;
                f.passable = true;
                game.log("You open the door.");
            }
            break;
        }
        default:
            game.log("Nothing happens.");
            break;
    }
}

// ---------------------------------------------------------------------------
// Tutorial
// ---------------------------------------------------------------------------

void DialogManager::show_tutorial_choice(Game& game) {
    reset_content("ARIA", 0.5f);
    body_ =
        "\"Systems critical. Multiple component failures detected. "
        "Engine, hull plating, and navigation computer are offline. "
        "We're grounded until repairs are complete, commander.\"";
    add_option('1', "I need to find parts to fix this ship. (Begin tutorial)");
    add_option('2', "I know what I'm doing. (Skip tutorial)");
    footer_ = "[Space] Select";
    open_ = true;
    interacting_npc_ = nullptr;
    dialog_tree_ = nullptr;
    dialog_node_ = -11; // sentinel: tutorial choice
}

void DialogManager::show_tutorial_followup() {
    reset_content("ARIA", 0.5f);
    body_ =
        "\"Understood. Let's get to work.\n\n"
        "I'd start with the " +
        colored("Station Keeper", Color::White) + " " +
        colored("(K)", Color::Green) +
        ". He's been here since before the Collapse -- he'll know "
        "where to find parts.\n\n"
        "Check your " +
        colored("Datapad", Color::Cyan) + " (" +
        colored("c", Color::Yellow) +
        ") to track your objectives.\"";
    add_option('f', "Got it, I'll check my Datapad.");
    footer_ = "[Space] Continue";
    open_ = true;
    interacting_npc_ = nullptr;
    dialog_tree_ = nullptr;
    dialog_node_ = -12; // sentinel: tutorial followup -- opens datapad on dismiss
}

// ---------------------------------------------------------------------------
// Open NPC dialog
// ---------------------------------------------------------------------------

void DialogManager::open_npc_dialog(Npc& npc, Game& game) {
    interacting_npc_ = &npc;
    dialog_tree_ = nullptr;
    dialog_node_ = -1;
    interact_options_.clear();

    const auto& data = npc.interactions;
    reset_content(npc.display_name());

    // Faction gate: Hated NPCs refuse all interaction
    if (!npc.faction.empty()) {
        int rep = reputation_for(game.player(), npc.faction);
        if (reputation_tier(rep) == ReputationTier::Hated) {
            body_ = "\"I have nothing to say to you. Get lost.\"";
            game.log(npc.display_name() + " refuses to speak with you.");
            add_option('f', "Leave");
            interact_options_.push_back(InteractOption::Farewell);
            footer_ = "[Space] Select  [Esc] Close";
            open_ = true;
            return;
        }
    }

    std::string greeting = data.talk ? data.talk->greeting : "";
    if (!greeting.empty()) {
        body_ = "\"" + greeting + "\"";
        game.log(npc.display_name() + ": \"" + greeting + "\"");
    }

    char hotkey = '1';
    if (data.talk && !data.talk->nodes.empty()) {
        add_option(hotkey++, "Talk");
        interact_options_.push_back(InteractOption::Talk);
    }
    if (data.shop) {
        add_option(hotkey++, "Trade");
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
        add_option(hotkey++, "I have news about the job.");
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
            add_option(hotkey++, data.quest->quest_intro);
            interact_options_.push_back(InteractOption::Quest);
        }
    }

    add_option('f', "Farewell");
    interact_options_.push_back(InteractOption::Farewell);

    footer_ = "[Space] Select  [Tab] Trade  [l] Look  [Esc] Close";
    open_ = true;
}

// ---------------------------------------------------------------------------
// Advance dialog
// ---------------------------------------------------------------------------

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
        open_ = false;
        // last option or out of range: Leave
        return;
    }

    // Stash main menu
    if (dialog_node_ == -3) {
        if (selected == 0) {
            // Store mode -- show player inventory
            if (game.player().inventory.items.empty()) {
                game.log("You have nothing to store.");
                dialog_node_ = -1;
                open_ = false;
                return;
            }
            if (static_cast<int>(game.world().stash().size()) >= WorldManager::max_stash_size) {
                game.log("Stash is full! (" + std::to_string(WorldManager::max_stash_size) +
                    "/" + std::to_string(WorldManager::max_stash_size) + ")");
                dialog_node_ = -1;
                open_ = false;
                return;
            }
            reset_content("Store Item");
            game.log("Select item to store (" + std::to_string(game.world().stash().size()) +
                "/" + std::to_string(WorldManager::max_stash_size) + "):");
            { char sk = '1';
            for (const auto& item : game.player().inventory.items) {
                add_option(sk++, item.name);
                if (sk > '9') sk = 'a';
            } }
            add_option('c', "Cancel");
            footer_ = "[Space] Select  [Esc] Close";
            open_ = true;
            dialog_node_ = -4; // sentinel: store mode
        } else if (selected == 1) {
            // Retrieve mode -- show stash contents
            if (game.world().stash().empty()) {
                game.log("The stash is empty.");
                dialog_node_ = -1;
                open_ = false;
                return;
            }
            reset_content("Retrieve Item");
            game.log("Select item to retrieve (" + std::to_string(game.world().stash().size()) +
                "/" + std::to_string(WorldManager::max_stash_size) + "):");
            { char rk = '1';
            for (const auto& item : game.world().stash()) {
                add_option(rk++, item.name);
                if (rk > '9') rk = 'a';
            } }
            add_option('c', "Cancel");
            footer_ = "[Space] Select  [Esc] Close";
            open_ = true;
            dialog_node_ = -5; // sentinel: retrieve mode
        } else {
            // Close
            dialog_node_ = -1;
            open_ = false;
        }
        return;
    }

    // Stash store mode -- player selected an inventory item to store
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
        open_ = false;
        return;
    }

    // Stash retrieve mode -- player selected a stash item to retrieve
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
        open_ = false;
        return;
    }

    // Dev menu -- main
    if (dialog_node_ == -7) {
        dialog_node_ = -1;
        dialog_tree_ = nullptr;
        if (selected == 0) {
            game.dev_command_warp_random();
            open_ = false;
        } else if (selected == 1) {
            // POI Stamp Test submenu
            reset_content("[DEV] POI Stamp Test");
            add_option('1', "Ruins");
            add_option('2', "Crashed Ship");
            add_option('3', "Outpost");
            add_option('4', "Cave Entrance");
            add_option('5', "Settlement");
            add_option('6', "Landing Pad");
            add_option('b', "Back");
            footer_ = "[Space] Select  [Esc] Close";
            open_ = true;
            dialog_node_ = -9; // sentinel: stamp test menu
        } else if (selected == 2) {
            // Open character stats submenu
            reset_content("[DEV] Character Stats");
            add_option('i', std::string("Invulnerability: ") +
                               (has_effect(game.player().effects, EffectId::Invulnerable) ? "ON" : "OFF"));
            add_option('b', "Back");
            footer_ = "[Space] Select  [Esc] Close";
            open_ = true;
            dialog_node_ = -8; // sentinel: dev stats
        } else if (selected == 3) {
            // Force level up
            game.player().xp = game.player().max_xp;
            game.dev_command_level_up();
            open_ = false;
        } else {
            open_ = false;
        }
        return;
    }

    // Dev menu -- character stats
    if (dialog_node_ == -8) {
        if (selected == 0) {
            if (has_effect(game.player().effects, EffectId::Invulnerable))
                remove_effect(game.player().effects, EffectId::Invulnerable);
            else
                add_effect(game.player().effects, make_invulnerable());
            bool inv = has_effect(game.player().effects, EffectId::Invulnerable);
            game.log(std::string("[DEV] Invulnerability: ") + (inv ? "ON" : "OFF"));
            // Re-open the menu with updated label
            reset_content("[DEV] Character Stats");
            add_option('i', std::string("Invulnerability: ") +
                               (inv ? "ON" : "OFF"));
            add_option('b', "Back");
            footer_ = "[Space] Select  [Esc] Close";
            open_ = true;
            dialog_node_ = -8;
        } else {
            dialog_node_ = -1;
            dialog_tree_ = nullptr;
            open_ = false;
        }
        return;
    }

    // Dev menu -- POI stamp test
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
        open_ = false;
        return;
    }

    // Tutorial followup -- "Got it, I'll check my Datapad" opens character screen
    if (dialog_node_ == -12) {
        open_ = false;
        dialog_node_ = -1;
        dialog_tree_ = nullptr;
        aria_open_datapad_ = true;
        return;
    }

    // Tutorial choice
    if (dialog_node_ == -11) {
        open_ = false;
        dialog_node_ = -1;
        dialog_tree_ = nullptr;
        if (selected == 0) {
            // Play tutorial -- accept quest, ship stays empty
            auto* sq = find_story_quest("story_getting_airborne");
            if (sq) {
                auto q = sq->create_quest();
                game.log("Quest accepted: " + q.title);
                game.quests().accept_quest(std::move(q), game.world().world_tick(), game.player());
            }
            game.log("ARIA: \"Understood. Let's get to work.\"");
            game.log("ARIA: \"I'd start with the " + colored("Station Keeper", Color::White)
                + " " + colored("(K)", Color::Green)
                + ". He's been here since before the Collapse -- he'll know where to find parts.\"");
            game.log("ARIA: \"Check your " + colored("Datapad", Color::Cyan) + " ("
                + colored("c", Color::Yellow) + ") to track your objectives.\"");
            // Queue follow-up dialog for next frame
            aria_tutorial_followup_ = true;
        } else {
            // Skip tutorial -- equip ship with starter components
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
        open_ = false;
        if (selected == 0) {
            // Ship Systems -> open character screen on Ship tab
            aria_open_ship_tab_ = true;
        } else if (selected == 1) {
            // Star Chart
            if (game.player().ship.operational()) {
                aria_open_star_chart_ = true;
            } else {
                game.log("ARIA: \"I need an engine to plot routes. I'm an AI, not a fortune teller.\"");
            }
        } else if (selected == 2) {
            // Disembark -- exit ship to station
            aria_disembark_ = true;
        }
        return;
    }

    if (dialog_node_ == -6) {
        dialog_node_ = -1;
        dialog_tree_ = nullptr;
        open_ = false;
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
            open_ = false;
            return;
        }

        int next = node.choices[selected].next_node;
        if (next < 0 || next >= static_cast<int>(dialog_tree_->size())) {
            // End of conversation -- return to top-level menu
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
                // Don't accept a quest -- this is a direct reward
            }
            // Try story quest (Station Keeper) -- only after tutorial
            else if (auto* sq = find_story_quest("story_missing_hauler");
                sq && interacting_npc_->role == "Station Keeper" &&
                !game.quests().has_active_quest("story_missing_hauler") &&
                !game.quests().has_active_quest("story_getting_airborne")) {
                auto q = sq->create_quest();
                game.quests().accept_quest(std::move(q), game.world().world_tick(), game.player());
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
                game.quests().accept_quest(std::move(q), game.world().world_tick(), game.player());
            }
        }

        // Advance to next node
        dialog_node_ = next;
        const auto& next_node = (*dialog_tree_)[dialog_node_];
        reset_content(interacting_npc_->display_name());
        body_ = "\"" + next_node.text + "\"";
        game.log(interacting_npc_->display_name() + ": \"" + next_node.text + "\"");
        { char hk = '1';
        for (const auto& choice : next_node.choices) {
            add_option(hk++, choice.label);
        } }
        footer_ = "[Space] Select  [Esc] Close";
        open_ = true;
        return;
    }

    // Top-level menu selection
    if (selected < 0 || selected >= static_cast<int>(interact_options_.size())) {
        interacting_npc_ = nullptr;
        open_ = false;
        return;
    }

    switch (interact_options_[selected]) {
        case InteractOption::Talk: {
            dialog_tree_ = &interacting_npc_->interactions.talk->nodes;
            dialog_node_ = 0;
            const auto& node = (*dialog_tree_)[0];
            reset_content(interacting_npc_->display_name());
            body_ = "\"" + node.text + "\"";
            game.log(interacting_npc_->display_name() + ": \"" + node.text + "\"");
            { char hk = '1';
            for (const auto& choice : node.choices) {
                add_option(hk++, choice.label);
            } }
            footer_ = "[Space] Select  [Esc] Close";
            open_ = true;
            break;
        }
        case InteractOption::Shop:
            game.trade_window().open(interacting_npc_, &game.player(), game.renderer());
            open_ = false;
            break;

        case InteractOption::Quest: {
            dialog_tree_ = &interacting_npc_->interactions.quest->nodes;
            dialog_node_ = 0;
            const auto& node = (*dialog_tree_)[0];
            reset_content(interacting_npc_->display_name());
            body_ = "\"" + node.text + "\"";
            game.log(interacting_npc_->display_name() + ": \"" + node.text + "\"");
            { char hk = '1';
            for (const auto& choice : node.choices) {
                add_option(hk++, choice.label);
            } }
            footer_ = "[Space] Select  [Esc] Close";
            open_ = true;
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
                reset_content(interacting_npc_->display_name());
                std::string reply = "Well done, commander. You've earned your pay.";
                body_ = "\"" + reply + "\"";
                game.log(interacting_npc_->display_name() + ": \"" + reply + "\"");
                add_option('1', "Thanks.");
                footer_ = "[Space] Select  [Esc] Close";
                open_ = true;
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
            open_ = false;
            break;
    }
}

} // namespace astra
