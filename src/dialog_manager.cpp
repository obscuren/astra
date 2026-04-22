#include "astra/dialog_manager.h"
#include "astra/character.h"
#include "astra/display_name.h"
#include "astra/dungeon/puzzles.h"
#include "astra/game.h"
#include "astra/item_defs.h"
#include "astra/playback_viewer.h"
#include "astra/player.h"
#include "astra/quest_fixture.h"
#include "astra/quest_ui.h"
#include "astra/shop.h"

namespace astra {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {
// Scan the fixture_ids grid to find the (x,y) position of a given fixture id.
static std::pair<int,int> fixture_xy_by_id(const TileMap& m, int fid) {
    const auto& ids = m.fixture_ids();
    const int w = m.width();
    const int h = m.height();
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (ids[y * w + x] == fid)
                return {x, y};
    return {-1, -1};
}
} // namespace

void DialogManager::reset_content(const std::string& title, float width_frac) {
    title_ = title;
    body_.clear();
    options_.clear();
    hotkeys_.clear();
    selected_ = 0;
    footer_.clear();
    max_width_frac_ = width_frac;
    entity_ = {};  // clear entity ref
}

void DialogManager::add_option(char key, const std::string& label) {
    hotkeys_.push_back(key);
    options_.push_back(label);
}

namespace {
std::string format_quest_offer(const Quest& q, const Npc& npc) {
    std::string s = display_name(npc) + " explains:\n\n";
    s += format_quest_body(q);
    return s;
}
} // namespace

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
            else selected_ = static_cast<int>(options_.size()) - 1; // wrap to last
            return true;
        case KEY_DOWN: case 'j':
            if (selected_ < static_cast<int>(options_.size()) - 1) selected_++;
            else selected_ = 0; // wrap to first
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

    // Compute inner width for word-wrap measurement
    // Panel border = 1 each side, so inner = win_w - 2
    // Body has 1 char padding each side, so wrap_w = inner - 2
    int inner_w = win_w - 2;
    int wrap_w = inner_w - 2;
    if (wrap_w < 10) wrap_w = 10;

    // Measure content height
    int content_h = 0;
    std::vector<std::string> body_lines;

    bool has_entity = entity_.has_value();
    if (has_entity) {
        content_h += 1;  // glyph row
        content_h += 1;  // name row
        content_h += 1;  // separator after header
    }
    if (!body_.empty()) {
        body_lines = word_wrap(body_, wrap_w);
        content_h += 1;  // blank line before body
        content_h += static_cast<int>(body_lines.size());
        content_h += 1;  // blank line after body
        content_h += 1;  // separator
    }
    content_h += 1;  // blank line before options
    content_h += static_cast<int>(options_.size()) * 2 - 1; // conversation spacing: blank line between options
    content_h += 1;  // padding after options

    // Panel chrome: when entity header is shown, panel has no title (entity replaces it)
    bool use_panel_title = !has_entity && !title_.empty();
    bool has_footer = true;
    int chrome_h = 2; // top + bottom border
    if (use_panel_title) chrome_h += 2; // title row + separator
    if (has_footer) chrome_h += 1; // footer embedded in separator line

    int win_h = content_h + chrome_h;
    int max_h = screen_h - margin * 2;
    if (win_h > max_h) win_h = max_h;
    if (win_h < 10) win_h = 10;

    // Center on screen
    int wx = (screen_w - win_w) / 2;
    int wy = (screen_h - win_h) / 2;

    UIContext full(renderer, Rect{wx, wy, win_w, win_h});
    auto panel_content = full.panel({
        .title = use_panel_title ? title_ : "",
        .footer = footer_.empty() ? "[Space] Select  [Esc] Close" : footer_,
    });

    int cw = panel_content.width();
    int ch = panel_content.height();
    int y = 0;

    // Entity header: glyph centered, name centered, separator
    if (has_entity) {
        // Glyph centered — rendered by the renderer from EntityRef
        int glyph_x = cw / 2;
        panel_content.styled_text({.x = glyph_x, .y = y, .segments = {
            {"?", UITag::TextDefault, entity_},  // renderer resolves glyph+color from entity
        }});
        y++;

        // Name centered
        int name_x = (cw - static_cast<int>(title_.size())) / 2;
        if (name_x < 1) name_x = 1;
        panel_content.text({.x = name_x, .y = y, .content = title_, .tag = UITag::TextBright});
        y++;

        // Separator
        panel_content.sub(Rect{0, y, cw, 1}).separator({});
        y++;
    }

    // Body text with word-wrap and COLOR_BEGIN/COLOR_END support
    if (!body_.empty()) {
        y++; // blank line before body
        for (const auto& line : body_lines) {
            if (y >= ch) break;
            panel_content.text_rich(1, y, line, Color::White);
            y++;
        }
        y++; // blank line after body

        // Separator between body and options
        if (y < ch) {
            panel_content.sub(Rect{0, y, cw, 1}).separator({});
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
        int scroll = 0;
        if (selected_ >= list_h) scroll = selected_ - list_h + 1;

        auto list_area = panel_content.sub(Rect{0, y, cw, list_h});
        list_area.list({.items = items, .scroll_offset = scroll, .tag = UITag::ConversationOption, .selected_tag = UITag::OptionSelected});
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
        case FixtureType::StarChart:
        case FixtureType::StarChartL:
        case FixtureType::StarChartR: {
            // The star chart on the player's ship is a live navigation console
            // (allow warping to another system); anywhere else (station
            // observatories, labs) it's a view-only projector.
            bool on_ship = (game.world().map().map_type() == MapType::Starship);
            game.star_chart_viewer().set_view_only(!on_ship);
            game.star_chart_viewer().open();
            game.log(on_ship
                ? "The navigation console boots up. Plot a course to another system."
                : "The star chart hums to life, projecting a holographic galaxy map.");
            break;
        }
        case FixtureType::WeaponDisplay: {
            game.log("Weapons gleam behind reinforced glass. Talk to the Arms Dealer to browse.");
            break;
        }
        case FixtureType::StairsUp: {
            if (game.world().navigation().current_depth > 0) {
                game.ascend_stairs();
                break;
            }
            // Pre-existing non-recipe behavior:
            if (game.world().map().location_name() == "Maintenance Tunnels") {
                game.exit_maintenance_tunnels();
            } else {
                game.exit_dungeon_to_detail();
            }
            break;
        }
        case FixtureType::StairsDown:
        case FixtureType::StairsDownPrecursor: {
            auto xy = fixture_xy_by_id(game.world().map(), fid);
            game.descend_stairs(xy);
            break;
        }
        case FixtureType::PrecursorButton: {
            const auto& f = game.world().map().fixture(fid);
            astra::dungeon::on_button_pressed(game, f.puzzle_id);
            break;
        }
        case FixtureType::DungeonHatch: {
            const auto& nav = game.world().navigation();
            if (nav.current_depth == 0) {
                LocationKey root{
                    nav.current_system_id,
                    nav.current_body_index,
                    nav.current_moon_index,
                    nav.at_station,
                    -1, -1, 0
                };
                if (game.world().find_dungeon_recipe(root)) {
                    auto xy = fixture_xy_by_id(game.world().map(), fid);
                    game.descend_stairs(xy);
                    break;
                }
            }
            // Pre-existing maintenance-tunnel behavior:
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
                game.quests().complete_quest("story_getting_airborne", game, game.world().world_tick());
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
        case FixtureType::QuestFixture: {
            const QuestFixtureDef* def = find_quest_fixture(f.quest_fixture_id);
            bool first_use = (f.last_used_tick < 0);

            if (def && !def->log_lines.empty()) {
                game.playback_viewer().open(PlaybackStyle::AudioLog,
                                            def->log_title,
                                            def->log_lines);
            } else if (def && !def->log_message.empty()) {
                game.log(def->log_message);
            }

            if (first_use) {
                game.quests().on_fixture_interacted(f.quest_fixture_id);
            }
            f.last_used_tick = game.world().world_tick();
            break;
        }
        case FixtureType::Altar: {
            game.log("A weathered Precursor altar. The stone is warm.");
            break;
        }
        case FixtureType::Inscription: {
            // Layer 6.iii stores per-fixture flavor text in quest_fixture_id
            // (overloaded as a generic per-fixture string for non-QuestFixture
            // types). Display it verbatim; fall back if unset.
            if (!f.quest_fixture_id.empty()) {
                game.log(f.quest_fixture_id);
            } else {
                game.log("The runes are worn smooth; you cannot decipher them.");
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

void DialogManager::show_auto_accept(Game& /*game*/, const Quest& q) {
    std::string header = "New Mission";
    if (!q.arc_id.empty()) {
        StoryQuest* sq = find_story_quest(q.id);
        std::string arc = sq ? sq->arc_title() : std::string{};
        if (!arc.empty()) header += " — " + arc;
        else               header += " — " + q.title;
    } else {
        header += " — " + q.title;
    }

    reset_content(header, 0.45f);
    interacting_npc_ = nullptr;
    dialog_tree_ = nullptr;
    dialog_node_ = -1;
    detail_offer_quest_id_.clear();
    pending_story_offers_.clear();
    interact_options_.clear();

    body_ = format_quest_body(q);

    add_option('a', "Accept");
    interact_options_.push_back(InteractOption::AutoAcceptAck);

    footer_ = "[Space] Accept";
    open_ = true;
}

// ---------------------------------------------------------------------------
// Open NPC dialog
// ---------------------------------------------------------------------------

void DialogManager::open_npc_dialog(Npc& npc, Game& game) {
    interacting_npc_ = &npc;
    dialog_tree_ = nullptr;
    dialog_node_ = -1;
    interact_options_.clear();
    pending_story_offers_.clear();
    detail_offer_quest_id_.clear();

    const auto& data = npc.interactions;
    reset_content(npc.label());
    entity_ = EntityRef{EntityRef::Kind::Npc, static_cast<uint16_t>(npc.npc_role), static_cast<uint8_t>(npc.race)};

    // Faction gate: Hated NPCs refuse all interaction
    if (!npc.faction.empty()) {
        int rep = reputation_for(game.player(), npc.faction);
        if (reputation_tier(rep) == ReputationTier::Hated) {
            body_ = "\"I have nothing to say to you. Get lost.\"";
            game.log(npc.label() + " refuses to speak with you.");
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
        game.log(npc.label() + ": \"" + greeting + "\"");
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
    bool has_nova_hook_active = false;
    for (const auto& q : game.quests().active_quests()) {
        if (q.id == "story_stellar_signal_hook" && q.giver_npc == npc.role) {
            has_nova_hook_active = true;
            continue;  // hook gets its own entry, not the generic turnin
        }
        if (q.giver_npc == npc.role && q.ready_for_turnin()) {
            has_turnin = true;
            break;
        }
    }
    // Nova's Stage 1 hook: special branching turn-in dialog.
    if (has_nova_hook_active) {
        add_option(hotkey++, "What's troubling you, Nova?");
        interact_options_.push_back(InteractOption::NovaHookEntry);
    }
    if (has_turnin) {
        add_option(hotkey++, "I have news about the job.");
        interact_options_.push_back(InteractOption::QuestTurnIn);
    }

    // Offer available story quests from this NPC role
    {
        int rep_st = reputation_for(game.player(), npc.faction);
        bool rep_ok_st = npc.faction.empty() || reputation_tier(rep_st) >= ReputationTier::Neutral;
        if (rep_ok_st) {
            auto offers = game.quests().available_for_role(npc.role);
            for (const Quest* offer : offers) {
                add_option(hotkey++,
                    colored("(Quest)", Color::Blue) + " " + offer->title);
                interact_options_.push_back(InteractOption::StoryQuestOffer);
                pending_story_offers_.push_back(offer->id);
            }
        }
    }

    // Offer new quests if none active from this NPC and reputation is Neutral+
    if (data.quest) {
        bool has_active_from_npc = false;
        for (const auto& q : game.quests().active_quests()) {
            if (q.giver_npc == npc.role && !q.is_story) { has_active_from_npc = true; break; }
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
        if (selected < 5) {
            static constexpr Tile poi_types[] = {
                Tile::OW_Ruins,
                Tile::OW_CrashedShip,
                Tile::OW_Outpost,
                Tile::OW_CaveEntrance,
                Tile::OW_Settlement,
            };
            static constexpr const char* poi_names[] = {
                "Ruins", "Crashed Ship", "Outpost",
                "Cave Entrance", "Settlement",
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
            // Play tutorial -- promote tutorial quest from available pool
            if (game.quests().accept_available(
                    "story_getting_airborne", game, game.world().world_tick())) {
                game.log("Quest accepted: Getting Airborne");
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
            // Skip tutorial -- equip ship with starter components and mark
            // Getting Airborne complete so downstream arcs (Stellar Signal, etc.)
            // unlock via the quest DAG.
            game.player().ship.engine = build_engine_coil_mk1();
            game.player().ship.hull = build_hull_plate();
            game.player().ship.navi_computer = build_navi_computer_mk2();
            if (game.quests().accept_available(
                    "story_getting_airborne", game, game.world().world_tick())) {
                game.quests().complete_quest(
                    "story_getting_airborne", game, game.world().world_tick());
            }
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

    // Auto-accept quest popup: no NPC interaction, Space/Enter just dismisses.
    if (selected >= 0 && selected < static_cast<int>(interact_options_.size())
     && interact_options_[selected] == InteractOption::AutoAcceptAck) {
        open_ = false;
        interact_options_.clear();
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
            else {
                // Generate a random quest based on NPC role + world state
                auto q = game.quests().generate_quest_for_role(
                    interacting_npc_->role, interacting_npc_->label(),
                    game.world().navigation(), game.world().rng());
                q.giver_npc = interacting_npc_->role;
                // Register map marker if quest has a target location
                if (q.target_system_id != 0) {
                    LocationKey mk = {q.target_system_id, q.target_body_index, -1, false, -1, -1, 0};
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
        reset_content(interacting_npc_->label());
        body_ = "\"" + next_node.text + "\"";
        game.log(interacting_npc_->label() + ": \"" + next_node.text + "\"");
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
            reset_content(interacting_npc_->label());
            body_ = "\"" + node.text + "\"";
            game.log(interacting_npc_->label() + ": \"" + node.text + "\"");
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
            reset_content(interacting_npc_->label());
            body_ = "\"" + node.text + "\"";
            game.log(interacting_npc_->label() + ": \"" + node.text + "\"");
            { char hk = '1';
            for (const auto& choice : node.choices) {
                add_option(hk++, choice.label);
            } }
            footer_ = "[Space] Select  [Esc] Close";
            open_ = true;
            break;
        }
        case InteractOption::StoryQuestOffer: {
            // Count which StoryQuestOffer this selection refers to
            int story_idx = 0;
            for (int i = 0; i < selected; ++i) {
                if (interact_options_[i] == InteractOption::StoryQuestOffer) ++story_idx;
            }
            if (story_idx < 0 || story_idx >= static_cast<int>(pending_story_offers_.size()))
                return;

            const std::string qid = pending_story_offers_[story_idx];
            const Quest* offer = game.quests().find_quest(qid).quest;
            if (!offer || !interacting_npc_) { open_ = false; return; }

            reset_content(interacting_npc_->label());
            entity_ = EntityRef{EntityRef::Kind::Npc,
                                static_cast<uint16_t>(interacting_npc_->npc_role),
                                static_cast<uint8_t>(interacting_npc_->race)};
            body_ = format_quest_offer(*offer, *interacting_npc_);
            interact_options_.clear();
            pending_story_offers_.clear();
            detail_offer_quest_id_ = qid;
            add_option('a', "Accept");
            interact_options_.push_back(InteractOption::StoryQuestAccept);
            add_option('d', "Decline");
            interact_options_.push_back(InteractOption::StoryQuestDecline);
            return;
        }
        case InteractOption::NovaHookEntry: {
            // Opening monologue + three branching responses.
            reset_content(interacting_npc_ ? interacting_npc_->label() : "");
            body_ =
                "\"Commander... do you ever hear things that "
                "shouldn't be there?\n\n"
                "Static in the dark. Not interference — pattern. "
                "Like someone speaking just below the frequency you "
                "can actually hear.\n\n"
                "Stellari don't hear the way you do. We listen to the "
                "galaxy the way you listen to music. And there's a "
                "song playing out there that shouldn't exist. It's "
                "older than the gate network. Older than the Precursors.\n\n"
                "It's calling me by name.\"";
            interact_options_.clear();
            add_option('1', "Calling you how? Are you okay?");
            interact_options_.push_back(InteractOption::NovaHookCare);
            add_option('2', "Sounds like background radiation.");
            interact_options_.push_back(InteractOption::NovaHookSkeptic);
            add_option('3', "What do you need me to do?");
            interact_options_.push_back(InteractOption::NovaHookAction);
            footer_ = "[Space] Select  [Esc] Close";
            open_ = true;
            return;
        }
        case InteractOption::NovaHookCare:
        case InteractOption::NovaHookSkeptic:
        case InteractOption::NovaHookAction: {
            InteractOption which = interact_options_[selected];
            reset_content(interacting_npc_ ? interacting_npc_->label() : "");
            if (which == InteractOption::NovaHookCare) {
                body_ =
                    "\"I'm fine. I'm Stellari, we don't break, we just... "
                    "resonate.\n\n"
                    "But this signal knows me. Specifically me. And I don't "
                    "know how. I've never left The Heavens Above. I've been "
                    "here as long as I can remember.\n\n"
                    "Which, come to think of it, I can't actually remember "
                    "how long.\n\n"
                    "...That's a problem, isn't it.\"";
            } else if (which == InteractOption::NovaHookSkeptic) {
                body_ =
                    "\"Background radiation doesn't match Stellari "
                    "resonance frequency to three decimal places. This is "
                    "modulated. This is intentional. Someone is reaching "
                    "out.\"";
            } else {
                body_ =
                    "\"I've triangulated three systems where the signal is "
                    "strongest. I need you to go to each and plant a "
                    "receiver drone. My station instruments are too "
                    "dampened by Jupiter's magnetosphere — I need clean "
                    "recordings from the void.\n\n"
                    "I'd go myself. But Stellari... we don't leave. I don't "
                    "know why. I've never really wondered until now.\"";
            }
            interact_options_.clear();
            add_option('a', "I'll go.");
            interact_options_.push_back(InteractOption::NovaHookConfirm);
            footer_ = "[Space] Select  [Esc] Close";
            open_ = true;
            return;
        }
        case InteractOption::NovaHookConfirm: {
            // Complete Stage 1. DAG cascade unlocks Stage 2 so Nova's next
            // dialog offers it.
            game.quests().complete_quest("story_stellar_signal_hook", game,
                                          game.world().world_tick());
            game.log("Quest completed: " +
                     colored("Static in the Dark", Color::Green));
            open_ = false;
            return;
        }
        case InteractOption::StoryQuestAccept: {
            const std::string qid = detail_offer_quest_id_;
            detail_offer_quest_id_.clear();
            // Capture the title BEFORE accept_available runs — some quests
            // self-complete inside on_accepted (e.g. narrative-only stages),
            // which would move them out of active_ before we can look them up.
            std::string title = qid;
            if (const Quest* q = game.quests().find_quest(qid).quest) {
                title = q->title;
            }
            if (!qid.empty() &&
                game.quests().accept_available(qid, game, game.world().world_tick())) {
                game.log("Quest accepted: " + colored(title, Color::Yellow));
            }
            open_ = false;
            return;
        }
        case InteractOption::StoryQuestDecline: {
            detail_offer_quest_id_.clear();
            if (interacting_npc_) open_npc_dialog(*interacting_npc_, game);
            else open_ = false;
            return;
        }
        case InteractOption::AutoAcceptAck:
            // Handled above the NPC-required guard; unreachable here.
            return;
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
                game.quests().complete_quest(turn_in_id, game, game.world().world_tick());

                // Log reward details
                game.log("Quest completed: " + colored(quest_title, Color::Green));
                std::string reward_msg = "Received:";
                if (reward.xp > 0) reward_msg += " " + colored(std::to_string(reward.xp) + " XP", Color::Cyan);
                if (reward.credits > 0) reward_msg += " " + colored(std::to_string(reward.credits) + " credits", Color::Yellow);
                if (reward.skill_points > 0) reward_msg += " " + colored(std::to_string(reward.skill_points) + " SP", Color::Cyan);
                for (const auto& fr : reward.factions) {
                    if (fr.faction_name.empty() || fr.reputation_change == 0) continue;
                    reward_msg += " " + colored("+" + std::to_string(fr.reputation_change) + " " + fr.faction_name + " rep", Color::Green);
                }
                for (const auto& ri : reward.items)
                    reward_msg += " " + display_name(ri);
                game.log(reward_msg);

                // Clean up quest location markers by quest_id
                auto& ql = game.world().quest_locations();
                for (auto it = ql.begin(); it != ql.end(); ) {
                    if (it->second.quest_id == turn_in_id)
                        it = ql.erase(it);
                    else
                        ++it;
                }

                // Show NPC response
                reset_content(interacting_npc_->label());
                std::string reply = "Well done, commander. You've earned your pay.";
                body_ = "\"" + reply + "\"";
                game.log(interacting_npc_->label() + ": \"" + reply + "\"");
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
