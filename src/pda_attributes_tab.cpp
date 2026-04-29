#include "astra/pda_screen.h"

#include <string>

namespace astra {

bool PdaScreen::has_pending() const {
    for (int i = 0; i < 6; ++i) if (pending_points_[i] > 0) return true;
    return false;
}

int PdaScreen::total_pending() const {
    int t = 0;
    for (int i = 0; i < 6; ++i) t += pending_points_[i];
    return t;
}

void PdaScreen::commit_pending() {
    if (!has_pending()) return;
    int spent = total_pending();
    auto& a = player_->attributes;
    int* attrs[] = {&a.strength, &a.agility, &a.toughness,
                    &a.intelligence, &a.willpower, &a.luck};
    for (int i = 0; i < 6; ++i) {
        *attrs[i] += pending_points_[i];
        pending_points_[i] = 0;
    }
    player_->attribute_points -= spent;
    // Recalculate derived stats
    player_->max_hp = player_->effective_max_hp();
    if (player_->hp > player_->max_hp) player_->hp = player_->max_hp;
}

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

void PdaScreen::draw_attributes(UIContext& ctx) {
    int w = ctx.width();
    int half = w / 2;

    // Character identity header
    int y = 1;
    ctx.put(2, y, '@', Color::White);
    ctx.text({.x = 4, .y = y, .content = player_->name, .tag = UITag::TextBright});
    y++;
    std::string subtitle = std::string(race_name(player_->race)) + " "
                         + class_name(player_->player_class);
    ctx.text({.x = 4, .y = y, .content = subtitle, .tag = UITag::TextDim});
    y++;
    std::string info = "Level: " + std::to_string(player_->level)
        + " \xc2\xb7 HP: " + std::to_string(player_->hp) + "/" + std::to_string(player_->effective_max_hp())
        + " \xc2\xb7 XP: " + std::to_string(player_->xp) + "/" + std::to_string(player_->max_xp);
    ctx.text({.x = 4, .y = y, .content = info, .tag = UITag::TextDim});
    y += 2;



    // ──┤ MAIN ATTRIBUTES ├────┤ Attribute Points: 0 ├──
    draw_section_header(ctx, y, "MAIN ATTRIBUTES");
    {
        // Draw second label right-aligned before divider
        int divider_x = ctx.width() / 2;
        int remaining = player_->attribute_points - total_pending();
        std::string pts = std::to_string(remaining);
        std::string label = " Attribute Points: ";
        // Position: ──┤ label N ├──  ending at divider_x
        int total_len = 2 + 1 + static_cast<int>(label.size()) + static_cast<int>(pts.size()) + 1 + 1;
        int start_x = divider_x - total_len;
        if (start_x > 0) {
            ctx.put(start_x, y, BoxDraw::RT, Color::DarkGray);
            ctx.text({.x = start_x + 1, .y = y, .content = label, .tag = UITag::TextBright});
            int num_x = start_x + 1 + static_cast<int>(label.size());
            ctx.text({.x = num_x, .y = y, .content = pts, .tag = UITag::TextSuccess});
            ctx.put(num_x + static_cast<int>(pts.size()), y, ' ');
            ctx.put(num_x + static_cast<int>(pts.size()) + 1, y, BoxDraw::LT, Color::DarkGray);
        }
    }
    y += 2;

    // Primary attribute boxes: single row of 6
    int box_x = 2;
    int box_spacing = 8; // 7 wide + 1 gap
    const auto& a = player_->attributes;
    int primary_base[] = {a.strength, a.agility, a.toughness,
                          a.intelligence, a.willpower, a.luck};
    int remaining_pts = player_->attribute_points - total_pending();

    for (int i = 0; i < 6; ++i) {
        int bx = box_x + i * box_spacing;
        int by = y;
        int display_val = primary_base[i] + pending_points_[i];
        int modifier = (display_val - 10) / 2;
        bool selected = (cursor_ == i);
        draw_stat_box(ctx, bx, by, primary_labels[i], display_val,
                      selected, modifier, pending_points_[i],
                      remaining_pts > 0);
    }

    // Description text below primary boxes — "Name determines ..."
    // Shared description renderer: attribute name in Yellow, rest in DarkGray
    int desc_y = y + 6 + 1;
    auto draw_desc = [&](int dy, const char* attr_name, const char* desc_text) {
        int dx = 2;
        // Draw attribute name in yellow
        std::string name_str(attr_name);
        ctx.text({.x = dx, .y = dy, .content = name_str, .tag = UITag::TextWarning});
        dx += static_cast<int>(name_str.size()) + 1;
        // Draw rest of description in dim, with simple word wrap
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
        player_->effective_av(DamageType::Kinetic),
        player_->effective_dv(),
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

} // namespace astra
