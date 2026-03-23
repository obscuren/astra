#include "astra/character_screen.h"
#include "astra/character.h"
#include "astra/race.h"

#include <algorithm>
#include <string>

namespace astra {

static const char* tab_names[] = {
    "Skills", "Attributes", "Equipment", "Tinkering",
    "Journal", "Quests", "Reputation", "Ship",
};

bool CharacterScreen::is_open() const { return open_; }

void CharacterScreen::open(Player* player, Renderer* renderer) {
    player_ = player;
    renderer_ = renderer;
    open_ = true;
    active_tab_ = CharTab::Attributes;
    cursor_ = 0;
    scroll_ = 0;
    equip_focus_ = EquipFocus::PaperDoll;
    equip_cursor_ = 0;
    inv_cursor_ = 0;
}

void CharacterScreen::close() { open_ = false; }

bool CharacterScreen::handle_input(int key) {
    if (!open_) return false;

    if (key == 27) { // ESC
        close();
        return true;
    }

    // Tab switching with q/e
    if (key == 'q') {
        int t = static_cast<int>(active_tab_);
        t = (t - 1 + char_tab_count) % char_tab_count;
        active_tab_ = static_cast<CharTab>(t);
        cursor_ = 0;
        scroll_ = 0;
        return true;
    }
    if (key == 'e') {
        int t = static_cast<int>(active_tab_);
        t = (t + 1) % char_tab_count;
        active_tab_ = static_cast<CharTab>(t);
        cursor_ = 0;
        scroll_ = 0;
        return true;
    }

    // Look overlay — any key closes
    if (look_open_) {
        look_open_ = false;
        look_item_ = nullptr;
        return true;
    }

    // Context menu intercepts input when open
    if (context_open_) {
        handle_context_key(key);
        return true;
    }

    if (context_msg_timer_ > 0) --context_msg_timer_;

    // Tab-specific input
    if (active_tab_ == CharTab::Attributes) {
        int max_cursor = 13;
        if (key == KEY_UP && cursor_ > 0) --cursor_;
        if (key == KEY_DOWN && cursor_ < max_cursor) ++cursor_;
    } else if (active_tab_ == CharTab::Equipment) {
        if (key == '\t') {
            equip_focus_ = (equip_focus_ == EquipFocus::PaperDoll)
                ? EquipFocus::Inventory : EquipFocus::PaperDoll;
            return true;
        }
        if (equip_focus_ == EquipFocus::PaperDoll) {
            if (key == KEY_UP && equip_cursor_ > 0) --equip_cursor_;
            if (key == KEY_DOWN && equip_cursor_ < equip_slot_count - 1) ++equip_cursor_;
        } else {
            int count = static_cast<int>(player_->inventory.items.size());
            if (key == KEY_UP && inv_cursor_ > 0) --inv_cursor_;
            if (key == KEY_DOWN && inv_cursor_ < count - 1) ++inv_cursor_;
        }
        if (key == ' ') {
            open_context_menu();
            return true;
        }
    } else if (active_tab_ == CharTab::Skills) {
        int count = static_cast<int>(player_->skills.size());
        if (count > 0) {
            if (key == KEY_UP && cursor_ > 0) --cursor_;
            if (key == KEY_DOWN && cursor_ < count - 1) ++cursor_;
        }
    } else if (active_tab_ == CharTab::Reputation) {
        int count = static_cast<int>(player_->reputation.size());
        if (count > 0) {
            if (key == KEY_UP && cursor_ > 0) --cursor_;
            if (key == KEY_DOWN && cursor_ < count - 1) ++cursor_;
        }
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────
// Context menu
// ─────────────────────────────────────────────────────────────────

void CharacterScreen::open_context_menu() {
    context_options_.clear();
    context_selection_ = 0;

    if (equip_focus_ == EquipFocus::PaperDoll) {
        auto slot = static_cast<EquipSlot>(equip_cursor_);
        const auto& item = player_->equipment.slot_ref(slot);
        if (!item) return;
        context_options_.push_back({'l', "look"});
        context_options_.push_back({'r', "remove"});
        if (item->ranged) {
            context_options_.push_back({'u', "unload"});
        }
    } else {
        if (player_->inventory.items.empty()) return;
        if (inv_cursor_ < 0 || inv_cursor_ >= static_cast<int>(player_->inventory.items.size())) return;
        const auto& item = player_->inventory.items[inv_cursor_];
        context_options_.push_back({'l', "look"});
        if (item.type == ItemType::Equipment && item.slot) {
            context_options_.push_back({'e', "equip"});
        }
        if (item.ranged) {
            context_options_.push_back({'r', "reload"});
            context_options_.push_back({'u', "unload"});
        }
    }

    if (!context_options_.empty()) {
        context_open_ = true;
    }
}

void CharacterScreen::handle_context_key(int key) {
    if (key == 27) {
        context_open_ = false;
        return;
    }

    int count = static_cast<int>(context_options_.size());

    // Arrow navigation
    if (key == KEY_UP && context_selection_ > 0) { --context_selection_; return; }
    if (key == KEY_DOWN && context_selection_ < count - 1) { ++context_selection_; return; }

    // Enter/space selects current
    if (key == '\n' || key == '\r' || key == ' ') {
        char k = context_options_[context_selection_].key;
        context_open_ = false;
        execute_context_action(k);
        return;
    }

    // Hotkey press
    for (const auto& opt : context_options_) {
        if (key == opt.key) {
            context_open_ = false;
            execute_context_action(opt.key);
            return;
        }
    }
}

void CharacterScreen::execute_context_action(char key) {
    if (equip_focus_ == EquipFocus::PaperDoll) {
        auto slot = static_cast<EquipSlot>(equip_cursor_);
        auto& equipped = player_->equipment.slot_ref(slot);
        if (!equipped) return;

        if (key == 'l') {
            look_item_ = &(*equipped);
            look_open_ = true;
        } else if (key == 'r') {
            if (!player_->inventory.can_add(*equipped)) {
                context_message_ = "Inventory too heavy.";
                context_msg_timer_ = 3;
                return;
            }
            context_message_ = "Removed " + equipped->name + ".";
            context_msg_timer_ = 3;
            player_->inventory.items.push_back(std::move(*equipped));
            equipped.reset();
        } else if (key == 'u') {
            if (equipped->ranged && equipped->ranged->current_charge > 0) {
                context_message_ = "Unloaded " + std::to_string(equipped->ranged->current_charge) + " charge.";
                equipped->ranged->current_charge = 0;
            } else {
                context_message_ = "Nothing to unload.";
            }
            context_msg_timer_ = 3;
        }
    } else {
        auto& items = player_->inventory.items;
        if (inv_cursor_ < 0 || inv_cursor_ >= static_cast<int>(items.size())) return;

        if (key == 'l') {
            look_item_ = &items[inv_cursor_];
            look_open_ = true;
        } else if (key == 'e') {
            auto& item = items[inv_cursor_];
            if (item.slot) {
                auto& sl = player_->equipment.slot_ref(*item.slot);
                Item to_equip = std::move(item);
                items.erase(items.begin() + inv_cursor_);
                if (sl) items.push_back(std::move(*sl));
                sl = std::move(to_equip);
                context_message_ = "Equipped " + sl->name + ".";
                context_msg_timer_ = 3;
                if (inv_cursor_ >= static_cast<int>(items.size()) && inv_cursor_ > 0)
                    --inv_cursor_;
            }
        } else if (key == 'r') {
            context_message_ = "Reload not yet implemented.";
            context_msg_timer_ = 3;
        } else if (key == 'u') {
            auto& item = items[inv_cursor_];
            if (item.ranged && item.ranged->current_charge > 0) {
                context_message_ = "Unloaded " + std::to_string(item.ranged->current_charge) + " charge.";
                item.ranged->current_charge = 0;
            } else {
                context_message_ = "Nothing to unload.";
            }
            context_msg_timer_ = 3;
        }
    }
}

void CharacterScreen::draw_context_menu(DrawContext& ctx) {
    if (!context_open_ || context_options_.empty()) return;

    // Calculate menu size
    int menu_w = 0;
    for (const auto& opt : context_options_) {
        int entry_w = 4 + static_cast<int>(opt.label.size());
        if (entry_w > menu_w) menu_w = entry_w;
    }
    menu_w += 6; // padding + cursor

    int option_count = static_cast<int>(context_options_.size());
    // Window: title ornament(3) + options + footer(3)
    int win_h = option_count + 6;
    int win_w = menu_w + 2;

    // Center the window
    int mx = (ctx.width() - win_w) / 2;
    int my = (ctx.height() - win_h) / 2;

    Window win(renderer_, Rect{ctx.bounds().x + mx, ctx.bounds().y + my, win_w, win_h}, "");
    win.set_footer("[Esc] Cancel");
    win.draw();

    DrawContext mc = win.content();

    for (int i = 0; i < option_count; ++i) {
        const auto& opt = context_options_[i];
        bool selected = (context_selection_ == i);
        int oy = i;
        int ox = 2;

        // > cursor for selected
        if (selected) {
            ctx.put(mx + 1, my + 3 + i, '>', Color::Yellow);
        }

        // [X] — bracket white, key yellow
        mc.put(ox, oy, '[', Color::White);
        mc.put(ox + 1, oy, opt.key, Color::Yellow);
        mc.put(ox + 2, oy, ']', Color::White);
        ox += 4;

        // Label with hotkey char highlighted yellow
        bool highlighted = false;
        for (int ci = 0; ci < static_cast<int>(opt.label.size()); ++ci) {
            char ch = opt.label[ci];
            if (!highlighted && (ch == opt.key || ch == (opt.key - 32) || ch == (opt.key + 32))) {
                mc.put(ox + ci, oy, ch, Color::Yellow);
                highlighted = true;
            } else {
                mc.put(ox + ci, oy, ch, selected ? Color::White : Color::DarkGray);
            }
        }
    }
}

void CharacterScreen::draw_look_overlay(DrawContext& ctx) {
    if (!look_open_ || !look_item_) return;

    const auto& item = *look_item_;

    // Sized window centered on screen
    int win_w = 50;
    int win_h = 20;
    int mx = (ctx.width() - win_w) / 2;
    int my = (ctx.height() - win_h) / 2;

    Window win(renderer_, Rect{ctx.bounds().x + mx, ctx.bounds().y + my, win_w, win_h}, "");
    win.set_footer("[Space] Continue");
    win.draw();

    DrawContext lc = win.content();
    int y = 0;

    // Item glyph centered
    lc.put(lc.width() / 2, y, item.glyph, rarity_color(item.rarity));
    y += 2;

    // Item name + summary line
    std::string summary = item.name;
    if (item.ranged) {
        summary += "  ATK+" + std::to_string(item.modifiers.attack)
                 + "  Charge:" + std::to_string(item.ranged->current_charge)
                 + "/" + std::to_string(item.ranged->charge_capacity)
                 + "  Range:" + std::to_string(item.ranged->max_range);
    } else if (item.modifiers.attack) {
        summary += "  ATK+" + std::to_string(item.modifiers.attack);
    }
    if (item.modifiers.defense) {
        summary += "  DEF+" + std::to_string(item.modifiers.defense);
    }
    lc.text_center(y, summary, Color::White);
    y += 2;

    // Description
    if (!item.description.empty()) {
        // Simple word wrap
        int max_w = lc.width() - 2;
        int dx = 1;
        int line_x = 0;
        for (size_t i = 0; i < item.description.size(); ++i) {
            if (item.description[i] == ' ' && line_x >= max_w) {
                y++;
                line_x = 0;
                continue;
            }
            lc.put(dx + line_x, y, item.description[i], Color::DarkGray);
            line_x++;
            if (line_x >= max_w) {
                y++;
                line_x = 0;
            }
        }
        y += 2;
    }

    // Stats
    if (item.slot) {
        lc.text(1, y, "Slot:", Color::White);
        lc.text(7, y, equip_slot_name(*item.slot), Color::Cyan);
        y++;
    }
    lc.text(1, y, "Rarity:", Color::White);
    lc.text(9, y, rarity_name(item.rarity), rarity_color(item.rarity));
    y++;

    if (item.max_durability > 0) {
        std::string dur = std::to_string(item.durability) + "/" + std::to_string(item.max_durability);
        lc.text(1, y, "Durability:", Color::White);
        lc.text(13, y, dur, Color::Cyan);
        y++;
    }

    std::string wt = std::to_string(item.weight) + " lbs.";
    lc.text(1, y, "Weight:", Color::White);
    lc.text(9, y, wt, Color::DarkGray);
    y++;

    if (item.buy_value > 0) {
        std::string val = std::to_string(item.buy_value) + "$";
        lc.text(1, y, "Value:", Color::White);
        lc.text(8, y, val, Color::Yellow);
    }
}

void CharacterScreen::draw(int screen_w, int screen_h) {
    if (!open_ || !renderer_) return;

    Window win(renderer_, Rect{0, 0, screen_w, screen_h}, "Character");
    win.set_footer("ESC Close  \xe2\x86\x91\xe2\x86\x93 Navigate");
    win.draw();

    DrawContext ctx = win.content();

    draw_tab_bar(ctx);

    // Content area below tab bar (row 0 = tab bar, row 1 = separator)
    DrawContext content = ctx.sub(Rect{0, 2, ctx.width(), ctx.height() - 2});

    switch (active_tab_) {
        case CharTab::Attributes: draw_attributes(content); break;
        case CharTab::Skills:     draw_skills(content); break;
        case CharTab::Equipment:  draw_equipment(content); break;
        case CharTab::Reputation: draw_reputation(content); break;
        case CharTab::Tinkering:  draw_stub(content, "Tinkering bench not available."); break;
        case CharTab::Journal:    draw_stub(content, "No entries found."); break;
        case CharTab::Quests:     draw_stub(content, "No active quests."); break;
        case CharTab::Ship:       draw_stub(content, "Ship systems not available."); break;
    }

    // Draw vertical divider only for tabs that use a split layout
    bool needs_divider = (active_tab_ == CharTab::Attributes
                       || active_tab_ == CharTab::Skills
                       || active_tab_ == CharTab::Equipment);
    if (needs_divider) {
        int divider_x = content.width() / 2;
        int last = content.height() - 1;
        ctx.put(divider_x, 1, BoxDraw::TT, Color::DarkGray);  // ┬ connects to tab separator
        for (int vy = 0; vy < last; ++vy) {
            content.put(divider_x, vy, BoxDraw::V, Color::DarkGray);
        }
        content.put(divider_x, last, BoxDraw::BT, Color::DarkGray); // ┴ at bottom
    }

    // Context menu overlay (equipment tab)
    draw_context_menu(content);

    // Look overlay
    draw_look_overlay(content);

    // Status message at bottom of content
    if (context_msg_timer_ > 0 && !context_message_.empty()) {
        content.text_center(content.height() - 1, context_message_, Color::Green);
    }
}

// ─────────────────────────────────────────────────────────────────
// Tab bar
// ─────────────────────────────────────────────────────────────────

void CharacterScreen::draw_tab_bar(DrawContext& ctx) {
    // Calculate total width: [Q] + space + tab labels with gaps + space + [E]
    int total_w = 4 + 1; // "[Q] "
    for (int i = 0; i < char_tab_count; ++i) {
        std::string label = tab_names[i];
        bool active = (static_cast<int>(active_tab_) == i);
        total_w += (active ? static_cast<int>(label.size()) + 2 : static_cast<int>(label.size()));
        if (i < char_tab_count - 1) total_w += 2; // gap
    }
    total_w += 1 + 3; // " [E]"

    int x = (ctx.width() - total_w) / 2;
    if (x < 0) x = 0;

    // [Q]
    ctx.put(x, 0, '[', Color::White);
    ctx.put(x + 1, 0, 'Q', Color::Green);
    ctx.put(x + 2, 0, ']', Color::White);
    x += 4;

    // Tab labels
    for (int i = 0; i < char_tab_count; ++i) {
        bool active = (static_cast<int>(active_tab_) == i);
        std::string label = tab_names[i];
        if (active) label = "[" + label + "]";

        Color fg = active ? Color::White : Color::DarkGray;
        ctx.text(x, 0, label, fg);
        x += static_cast<int>(label.size()) + 2;
    }

    // [E]
    ctx.put(x - 1, 0, '[', Color::White);
    ctx.put(x, 0, 'E', Color::Green);
    ctx.put(x + 1, 0, ']', Color::White);

    ctx.hline(1, BoxDraw::H, Color::DarkGray);
}

// ─────────────────────────────────────────────────────────────────
// Stat box drawing helper
// ─────────────────────────────────────────────────────────────────

void CharacterScreen::draw_stat_box(DrawContext& ctx, int x, int y,
                                     const char* label, int value,
                                     bool selected, int modifier) {
    // Box is 7 wide. Height: 3 (label + value) or 4 (+ modifier row)
    bool has_mod = (modifier != -999);
    int h = has_mod ? 4 : 3;
    Color border_color = selected ? Color::Yellow : Color::DarkGray;

    // Top border
    ctx.put(x, y, BoxDraw::TL, border_color);
    for (int i = 1; i < 6; ++i) ctx.put(x + i, y, BoxDraw::H, border_color);
    ctx.put(x + 6, y, BoxDraw::TR, border_color);

    // Label row
    ctx.put(x, y + 1, BoxDraw::V, border_color);
    std::string lbl(label);
    // Center the label in 5 chars
    int pad = static_cast<int>(5 - lbl.size()) / 2;
    ctx.text(x + 1 + pad, y + 1, lbl, selected ? Color::Yellow : Color::Cyan);
    ctx.put(x + 6, y + 1, BoxDraw::V, border_color);

    // Value row
    ctx.put(x, y + 2, BoxDraw::V, border_color);
    std::string val = std::to_string(value);
    int vpad = static_cast<int>(5 - val.size()) / 2;
    ctx.text(x + 1 + vpad, y + 2, val, Color::White);
    ctx.put(x + 6, y + 2, BoxDraw::V, border_color);

    // Modifier row (primary attributes only)
    if (has_mod) {
        ctx.put(x, y + 3, BoxDraw::V, border_color);
        std::string mod_str;
        Color mod_color;
        if (modifier > 0) {
            mod_str = "[+" + std::to_string(modifier) + "]";
            mod_color = Color::Green;
        } else if (modifier < 0) {
            mod_str = "[" + std::to_string(modifier) + "]";
            mod_color = Color::Red;
        } else {
            mod_str = "[ 0]";
            mod_color = Color::DarkGray;
        }
        int mpad = static_cast<int>(5 - mod_str.size()) / 2;
        ctx.text(x + 1 + mpad, y + 3, mod_str, mod_color);
        ctx.put(x + 6, y + 3, BoxDraw::V, border_color);
    }

    // Bottom border
    int bot = y + h;
    ctx.put(x, bot, BoxDraw::BL, border_color);
    for (int i = 1; i < 6; ++i) ctx.put(x + i, bot, BoxDraw::H, border_color);
    ctx.put(x + 6, bot, BoxDraw::BR, border_color);
}

void CharacterScreen::draw_section_header(DrawContext& ctx, int y,
                                           const char* title, int left_margin) {
    // ── TITLE ────────── (stops before the vertical divider)
    int divider_x = ctx.width() / 2;
    ctx.put(left_margin, y, BoxDraw::H, Color::DarkGray);
    ctx.put(left_margin + 1, y, BoxDraw::H, Color::DarkGray);
    ctx.text(left_margin + 3, y, title, Color::White);
    int after = left_margin + 3 + static_cast<int>(std::string(title).size()) + 1;
    for (int x = after; x < divider_x; ++x) {
        ctx.put(x, y, BoxDraw::H, Color::DarkGray);
    }
}

// ─────────────────────────────────────────────────────────────────
// Attributes tab
// ─────────────────────────────────────────────────────────────────

static const char* primary_labels[] = {"STR", "AGI", "TOU", "INT", "WIL", "LUC"};
static const char* primary_names[] = {"Strength", "Agility", "Toughness", "Intelligence", "Willpower", "Luck"};
static const char* primary_descriptions[] = {
    "determines melee damage and carry capacity.",
    "affects dodge value, move speed, and ranged accuracy.",
    "increases max HP and physical resistance.",
    "improves tinkering, hacking, and XP gain.",
    "strengthens mental resistance and energy regen.",
    "influences critical hits and loot quality.",
};

static const char* sec_labels[] = {"QN", "MS", "AV", "DV"};
static const char* sec_names[] = {"Quickness", "Move Speed", "Armor Value", "Dodge Value"};
static const char* sec_descriptions[] = {
    "determines how often you act relative to others.",
    "affects how fast you move across the map.",
    "reduces incoming physical damage.",
    "determines your chance to avoid attacks entirely.",
};

static const char* res_labels[] = {"AR", "ER", "CR", "HR"};
static const char* res_names[] = {"Acid Resistance", "Electrical Resistance", "Cold Resistance", "Heat Resistance"};
static const char* res_descriptions[] = {
    "reduces damage from corrosive and acidic sources.",
    "reduces damage from electrical and ion attacks.",
    "reduces damage from cold and cryo effects.",
    "reduces damage from heat, fire, and plasma.",
};

void CharacterScreen::draw_attributes(DrawContext& ctx) {
    int w = ctx.width();
    int half = w / 2;

    // Character identity header
    int y = 1;
    ctx.put(2, y, '@', Color::White);
    ctx.text(4, y, player_->name, Color::White);
    y++;
    std::string subtitle = std::string(race_name(player_->race)) + " "
                         + class_name(player_->player_class);
    ctx.text(4, y, subtitle, Color::DarkGray);
    y++;
    std::string info = "Level: " + std::to_string(player_->level)
        + " \xc2\xb7 HP: " + std::to_string(player_->hp) + "/" + std::to_string(player_->effective_max_hp())
        + " \xc2\xb7 XP: " + std::to_string(player_->xp) + "/" + std::to_string(player_->max_xp);
    ctx.text(4, y, info, Color::DarkGray);
    y += 2;



    // ── MAIN ATTRIBUTES ──
    draw_section_header(ctx, y, "MAIN ATTRIBUTES");
    y += 2;

    // Primary attribute boxes: 2 rows of 3
    int box_x = 2;
    int box_spacing = 8; // 7 wide + 1 gap
    const auto& a = player_->attributes;
    int primary_values[] = {a.strength, a.agility, a.toughness,
                            a.intelligence, a.willpower, a.luck};

    for (int i = 0; i < 6; ++i) {
        int row = i / 3;
        int col = i % 3;
        int bx = box_x + col * box_spacing;
        int by = y + row * 6; // 5 tall + 1 gap
        int modifier = (primary_values[i] - 10) / 2;
        bool selected = (cursor_ == i);
        draw_stat_box(ctx, bx, by, primary_labels[i], primary_values[i],
                      selected, modifier);
    }

    // Description text below primary boxes — "Name determines ..."
    // Shared description renderer: attribute name in Yellow, rest in DarkGray
    int desc_y = y + 12 + 1;
    auto draw_desc = [&](int dy, const char* attr_name, const char* desc_text) {
        int dx = 2;
        // Draw attribute name in yellow
        std::string name_str(attr_name);
        ctx.text(dx, dy, name_str, Color::Yellow);
        dx += static_cast<int>(name_str.size()) + 1;
        // Draw rest of description in DarkGray, with simple word wrap
        std::string desc(desc_text);
        int max_w = half - 4;
        int line_x = dx;
        size_t i = 0;
        while (i < desc.size()) {
            if (desc[i] == ' ' && line_x - 2 >= max_w) {
                dy++;
                line_x = 2;
                i++; // skip the space
                continue;
            }
            ctx.put(line_x, dy, desc[i], Color::DarkGray);
            line_x++;
            if (line_x - 2 >= max_w) {
                dy++;
                line_x = 2;
            }
            i++;
        }
    };

    if (cursor_ < 6) {
        draw_desc(desc_y, primary_names[cursor_], primary_descriptions[cursor_]);
    }

    // ── SECONDARY ATTRIBUTES ──
    int sec_y = desc_y + 3;
    draw_section_header(ctx, sec_y, "SECONDARY ATTRIBUTES");
    sec_y += 2;

    int sec_values[] = {
        player_->quickness + (a.agility - 10) / 2,
        player_->move_speed + (a.agility - 10) / 4,
        player_->effective_defense(),
        player_->effective_dodge(),
    };

    for (int i = 0; i < 4; ++i) {
        int bx = box_x + i * box_spacing;
        bool selected = (cursor_ == 6 + i);
        draw_stat_box(ctx, bx, sec_y, sec_labels[i], sec_values[i], selected);
    }

    // Description for secondary
    int sec_desc_y = sec_y + 5;
    if (cursor_ >= 6 && cursor_ < 10) {
        int si = cursor_ - 6;
        draw_desc(sec_desc_y, sec_names[si], sec_descriptions[si]);
    }

    // ── RESISTANCES ──
    int res_y = sec_desc_y + 3;
    draw_section_header(ctx, res_y, "RESISTANCES");
    res_y += 2;

    int res_values[] = {
        player_->resistances.acid,
        player_->resistances.electrical,
        player_->resistances.cold,
        player_->resistances.heat,
    };

    for (int i = 0; i < 4; ++i) {
        int bx = box_x + i * box_spacing;
        bool selected = (cursor_ == 10 + i);
        draw_stat_box(ctx, bx, res_y, res_labels[i], res_values[i], selected);
    }

    // Description for resistance
    int res_desc_y = res_y + 5;
    if (cursor_ >= 10 && cursor_ <= 13) {
        int ri = cursor_ - 10;
        draw_desc(res_desc_y, res_names[ri], res_descriptions[ri]);
    }
}

// ─────────────────────────────────────────────────────────────────
// Skills tab
// ─────────────────────────────────────────────────────────────────

void CharacterScreen::draw_skills(DrawContext& ctx) {
    int w = ctx.width();
    int half = w / 2;



    ctx.text(2, 1, "Skill Points: 0", Color::DarkGray);

    if (player_->skills.empty()) {
        ctx.text(2, 3, "No skills learned.", Color::DarkGray);
        return;
    }

    int y = 3;
    for (int i = 0; i < static_cast<int>(player_->skills.size()); ++i) {
        if (y >= ctx.height() - 1) break;
        const auto& sk = player_->skills[i];
        bool selected = (cursor_ == i);

        if (selected) ctx.put(1, y, '>', Color::Yellow);
        ctx.text(3, y, sk.name, selected ? Color::White : Color::Default);

        std::string lvl = "Lv " + std::to_string(sk.level);
        int lx = half - 2 - static_cast<int>(lvl.size());
        ctx.text(lx, y, lvl, Color::DarkGray);
        y++;
    }

    // Detail panel on right
    if (cursor_ < static_cast<int>(player_->skills.size())) {
        const auto& sk = player_->skills[cursor_];
        int rx = half + 2;
        ctx.text(rx, 1, sk.name, Color::White);
        ctx.text(rx, 2, sk.passive ? "[Passive]" : "[Active]", Color::Cyan);
        ctx.text(rx, 3, "Level " + std::to_string(sk.level), Color::DarkGray);
        // Description
        ctx.text(rx, 5, sk.description, Color::DarkGray);
    }
}

// ─────────────────────────────────────────────────────────────────
// Equipment tab
// ─────────────────────────────────────────────────────────────────

void CharacterScreen::draw_equipment(DrawContext& ctx) {
    int w = ctx.width();
    int half = w / 2;



    // Right side header: credits and weight
    std::string money_str = std::to_string(player_->money) + "$";
    std::string weight_str = std::to_string(player_->inventory.total_weight())
                           + "/" + std::to_string(player_->inventory.max_carry_weight) + " lb";
    ctx.text(w - 1 - static_cast<int>(weight_str.size()), 0, weight_str, Color::Cyan);
    ctx.text(w - 1 - static_cast<int>(weight_str.size()) - 3 - static_cast<int>(money_str.size()),
             0, money_str, Color::Yellow);

    // Paper doll on the left — CoQ-style with connector lines
    // Each box: 7 wide × 3 tall (border + item glyph centered)
    // Slot name centered below the box
    // Vertical lines connect: Face → Head → Body → Back → Feet (center spine)

    // Box dimensions
    constexpr int bw = 7;  // box width
    constexpr int bh = 3;  // box height
    constexpr int slot_h = 5; // box (3) + label (1) + gap (1)
    int cx = (half - 1) / 2; // center x of left half
    int dy = 1; // start y

    // Slot positions: {x, y, slot} — x is left edge of box
    // Center column: cx - bw/2
    // Left column: cx - bw/2 - bw - 2
    // Right column: cx - bw/2 + bw + 2
    int col_c = cx - bw / 2;
    int col_l = col_c - bw - 2;
    int col_r = col_c + bw + 2;
    int col_ll = col_l - bw - 2; // far left
    int col_rr = col_r + bw + 2; // far right

    struct SlotPos { int x; int y; EquipSlot slot; const char* label; };
    SlotPos positions[] = {
        // Row 0: Face (center)
        {col_c,  dy,              EquipSlot::Face,      "Face"},
        // Row 1: Head (center)
        {col_c,  dy + slot_h,     EquipSlot::Head,      "Head"},
        // Row 2: L.Hand, L.Arm, Body, R.Arm, R.Hand
        {col_ll, dy + slot_h * 2, EquipSlot::LeftHand,  "L.Hand"},
        {col_l,  dy + slot_h * 2, EquipSlot::LeftArm,   "L.Arm"},
        {col_c,  dy + slot_h * 2, EquipSlot::Body,      "Body"},
        {col_r,  dy + slot_h * 2, EquipSlot::RightArm,  "R.Arm"},
        {col_rr, dy + slot_h * 2, EquipSlot::RightHand, "R.Hand"},
        // Row 3: Back (center)
        {col_c,  dy + slot_h * 3, EquipSlot::Back,      "Back"},
        // Row 4: Feet (center)
        {col_c,  dy + slot_h * 4, EquipSlot::Feet,      "Feet"},
        // Row 5: Thrown, Missile
        {col_l,  dy + slot_h * 5, EquipSlot::Thrown,    "Thrown"},
        {col_r,  dy + slot_h * 5, EquipSlot::Missile,   "Missile"},
    };

    // Draw connector lines (center spine: between Face→Head→Body→Back→Feet)
    Color line_color = Color::DarkGray;
    auto draw_vconn = [&](int row_top, int row_bot) {
        // Vertical line from bottom of top box to top of bottom box
        int x = col_c + bw / 2;
        int y_start = positions[row_top].y + bh;     // just below top box
        int y_end = positions[row_bot].y;             // just above bottom box
        for (int vy = y_start; vy < y_end; ++vy) {
            ctx.put(x, vy, BoxDraw::V, line_color);
        }
    };
    // Face(0) → Head(1) → Body(4) → Back(7) → Feet(8)
    draw_vconn(0, 1);
    draw_vconn(1, 4);
    draw_vconn(4, 7);
    draw_vconn(7, 8);

    // Horizontal connectors on Body row: L.Hand─L.Arm─Body─R.Arm─R.Hand
    {
        int row_y = positions[4].y + bh / 2; // middle of Body row
        // L.Hand to L.Arm
        for (int hx = col_ll + bw; hx < col_l; ++hx)
            ctx.put(hx, row_y, BoxDraw::H, line_color);
        // L.Arm to Body
        for (int hx = col_l + bw; hx < col_c; ++hx)
            ctx.put(hx, row_y, BoxDraw::H, line_color);
        // Body to R.Arm
        for (int hx = col_c + bw; hx < col_r; ++hx)
            ctx.put(hx, row_y, BoxDraw::H, line_color);
        // R.Arm to R.Hand
        for (int hx = col_r + bw; hx < col_rr; ++hx)
            ctx.put(hx, row_y, BoxDraw::H, line_color);
    }

    // Draw each slot box
    for (int i = 0; i < equip_slot_count; ++i) {
        const auto& sp = positions[i];
        bool selected = (equip_focus_ == EquipFocus::PaperDoll && equip_cursor_ == i);
        Color border_color = selected ? Color::Yellow : Color::DarkGray;
        const auto& item = player_->equipment.slot_ref(sp.slot);

        int bx = sp.x;
        int by = sp.y;

        // Box border (7 wide × 3 tall)
        ctx.put(bx, by, BoxDraw::TL, border_color);
        for (int j = 1; j < bw - 1; ++j) ctx.put(bx + j, by, BoxDraw::H, border_color);
        ctx.put(bx + bw - 1, by, BoxDraw::TR, border_color);

        ctx.put(bx, by + 1, BoxDraw::V, border_color);
        // Content: item glyph centered, or empty
        if (item) {
            int mid = bx + bw / 2;
            ctx.put(mid, by + 1, item->glyph, rarity_color(item->rarity));
        } else {
            ctx.text(bx + 2, by + 1, "   ", Color::DarkGray);
        }
        ctx.put(bx + bw - 1, by + 1, BoxDraw::V, border_color);

        ctx.put(bx, by + 2, BoxDraw::BL, border_color);
        for (int j = 1; j < bw - 1; ++j) ctx.put(bx + j, by + 2, BoxDraw::H, border_color);
        ctx.put(bx + bw - 1, by + 2, BoxDraw::BR, border_color);

        // Label centered below box
        std::string label(sp.label);
        int label_x = bx + (bw - static_cast<int>(label.size())) / 2;
        ctx.text(label_x, by + 3, label, selected ? Color::Yellow : Color::Cyan);
    }

    // Bonuses at the bottom of left side
    int bonus_y = dy + slot_h * 6 + 1;
    if (bonus_y < ctx.height() - 3) {
        draw_section_header(ctx, bonus_y, "BONUSES");
        auto mods = player_->equipment.total_modifiers();
        std::string line1 = "ATK +" + std::to_string(mods.attack)
                          + "  DEF +" + std::to_string(mods.defense)
                          + "  HP +" + std::to_string(mods.max_hp);
        std::string line2 = "VIS +" + std::to_string(mods.view_radius)
                          + "  QCK +" + std::to_string(mods.quickness);
        ctx.text(2, bonus_y + 1, line1, Color::DarkGray);
        ctx.text(2, bonus_y + 2, line2, Color::DarkGray);
    }

    // Right side: categorized inventory
    int ry = 2;
    int rx = half + 2;
    int rw = w - half - 3;

    auto& items = player_->inventory.items;
    if (items.empty()) {
        ctx.text(rx, ry, "Inventory empty.", Color::DarkGray);
        return;
    }

    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (ry >= ctx.height() - 1) break;
        const auto& item = items[i];
        bool selected = (equip_focus_ == EquipFocus::Inventory && inv_cursor_ == i);

        if (selected) ctx.put(rx - 1, ry, '>', Color::Yellow);

        ctx.put(rx, ry, item.glyph, rarity_color(item.rarity));

        std::string name = item.name;
        if (item.stackable && item.stack_count > 1) {
            name += " x" + std::to_string(item.stack_count);
        }
        int max_name = rw - 6;
        if (static_cast<int>(name.size()) > max_name) name = name.substr(0, max_name);
        ctx.text(rx + 2, ry, name, selected ? Color::White : Color::Default);

        std::string price = std::to_string(item.sell_value) + "$";
        int px = half + rw - static_cast<int>(price.size());
        ctx.text(px, ry, price, Color::Yellow);

        ry++;
    }
}

// ─────────────────────────────────────────────────────────────────
// Reputation tab
// ─────────────────────────────────────────────────────────────────

void CharacterScreen::draw_reputation(DrawContext& ctx) {
    if (player_->reputation.empty()) {
        ctx.text(2, 2, "No faction standings.", Color::DarkGray);
        return;
    }

    int y = 2;
    for (int i = 0; i < static_cast<int>(player_->reputation.size()); ++i) {
        if (y >= ctx.height() - 2) break;
        const auto& f = player_->reputation[i];
        bool selected = (cursor_ == i);

        if (selected) ctx.put(1, y, '>', Color::Yellow);
        ctx.text(3, y, f.faction_name, selected ? Color::White : Color::Default);

        std::string rep = "Reputation: " + std::to_string(f.reputation);
        Color rep_color = f.reputation > 0 ? Color::Green
                        : f.reputation < 0 ? Color::Red
                        : Color::DarkGray;
        ctx.text(ctx.width() - 2 - static_cast<int>(rep.size()), y, rep, rep_color);

        // Flavor text
        y++;
        std::string flavor;
        if (f.reputation >= 50) flavor = "They consider you a trusted ally.";
        else if (f.reputation > 0) flavor = "They view you with curiosity.";
        else if (f.reputation == 0) flavor = "They are indifferent toward you.";
        else if (f.reputation > -50) flavor = "They are wary of you.";
        else flavor = "They are hostile toward you.";
        ctx.text(5, y, flavor, Color::DarkGray);
        y += 2;
    }
}

// ─────────────────────────────────────────────────────────────────
// Stub tab
// ─────────────────────────────────────────────────────────────────

void CharacterScreen::draw_stub(DrawContext& ctx, const char* message) {
    ctx.text_center(ctx.height() / 2, message, Color::DarkGray);
}

} // namespace astra
