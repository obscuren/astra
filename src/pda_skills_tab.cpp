#include "astra/pda_screen.h"

#include <string>

namespace astra {

std::vector<PdaScreen::SkillVisItem> PdaScreen::build_skill_vis() const {
    std::vector<SkillVisItem> vis;
    const auto& catalog = skill_catalog();
    for (int ci = 0; ci < static_cast<int>(catalog.size()); ++ci) {
        vis.push_back({true, ci, -1});
        if (ci < static_cast<int>(skill_cat_expanded_.size()) && skill_cat_expanded_[ci]) {
            for (int si = 0; si < static_cast<int>(catalog[ci].skills.size()); ++si)
                vis.push_back({false, ci, si});
        }
    }
    return vis;
}

void PdaScreen::draw_skills(UIContext& ctx) {
    int w = ctx.width();
    int half = w / 2;
    const auto& catalog = skill_catalog();



    // Header bar: ──┤ STR:14 AGI:12 ... ├───┤ Skill Points: 200 ├──
    {
        int divider_x = half;
        // Fill entire line with ─
        for (int x = 0; x < divider_x; ++x)
            ctx.put(x, 0, BoxDraw::H, Color::DarkGray);

        // Left label: attribute overview
        const auto& a = player_->attributes;
        const char* labels[] = {"STR", "AGI", "TOU", "INT", "WIL", "LUC"};
        int vals[] = {a.strength, a.agility, a.toughness,
                      a.intelligence, a.willpower, a.luck};

        int lx = 1;
        ctx.put(lx, 0, BoxDraw::H, Color::DarkGray);
        ctx.put(lx + 1, 0, BoxDraw::RT, Color::DarkGray);
        ctx.put(lx + 2, 0, ' ');
        // Draw attribute overview with semantic styled_text
        int ax = lx + 3;
        for (int i = 0; i < 6; ++i) {
            if (i > 0) { ctx.put(ax, 0, ' '); ax++; }
            std::string lbl = std::string(labels[i]) + ":";
            ctx.text({.x = ax, .y = 0, .content = lbl, .tag = UITag::TextDim});
            ax += static_cast<int>(lbl.size());
            std::string val = std::to_string(vals[i]);
            ctx.text({.x = ax, .y = 0, .content = val, .tag = UITag::TextBright});
            ax += static_cast<int>(val.size());
        }
        ctx.put(ax, 0, ' ');
        ctx.put(ax + 1, 0, BoxDraw::LT, Color::DarkGray);

        // Right label: skill points
        std::string pts = std::to_string(player_->skill_points);
        std::string sp_label = " Skill Points: ";
        int sp_len = 1 + static_cast<int>(sp_label.size()) + static_cast<int>(pts.size()) + 2;
        int sp_x = divider_x - sp_len;
        if (sp_x > ax + 2) {
            ctx.put(sp_x, 0, BoxDraw::RT, Color::DarkGray);
            ctx.text({.x = sp_x + 1, .y = 0, .content = sp_label, .tag = UITag::TextBright});
            int num_x = sp_x + 1 + static_cast<int>(sp_label.size());
            ctx.text({.x = num_x, .y = 0, .content = pts, .tag = UITag::TextSuccess});
            ctx.put(num_x + static_cast<int>(pts.size()), 0, ' ');
            ctx.put(num_x + static_cast<int>(pts.size()) + 1, 0, BoxDraw::LT, Color::DarkGray);
        }
    }

    auto visible = build_skill_vis();

    // Clamp cursor
    if (skill_cursor_ >= static_cast<int>(visible.size()))
        skill_cursor_ = static_cast<int>(visible.size()) - 1;
    if (skill_cursor_ < 0) skill_cursor_ = 0;

    // Draw list on left side
    int list_h = ctx.height() - 2;

    // Scroll so cursor is visible
    if (skill_cursor_ < skill_scroll_) skill_scroll_ = skill_cursor_;
    if (skill_cursor_ >= skill_scroll_ + list_h)
        skill_scroll_ = skill_cursor_ - list_h + 1;
    if (skill_scroll_ < 0) skill_scroll_ = 0;

    int y = 2;
    const SkillDef* selected_skill = nullptr;
    int selected_cat_idx = -1; // category index if a category row is selected

    for (int i = skill_scroll_; i < static_cast<int>(visible.size()); ++i) {
        if (y - 2 >= list_h) break;
        const auto& ve = visible[i];
        bool selected = (skill_cursor_ == i);

        if (ve.is_cat) {
            const auto& cat = catalog[ve.ci];
            bool unlocked = false;
            for (auto sid : player_->learned_skills)
                if (sid == cat.unlock_id) { unlocked = true; break; }

            // Category header: solid background-color bar
            if (selected) selected_cat_idx = ve.ci;

            Color bar_bg = selected ? static_cast<Color>(235) : static_cast<Color>(233);
            Color arrow_fg = Color::DarkGray;
            Color name_fg;
            if (selected) name_fg = Color::Yellow;
            else if (unlocked) name_fg = Color::Green;
            else name_fg = Color::White;

            // Fill entire row with background
            for (int fx = 0; fx < half; ++fx)
                ctx.put(fx, y, ' ', bar_bg, bar_bg);

            // Expand/collapse triangle
            const char* triangle = skill_cat_expanded_[ve.ci]
                ? "\xe2\x96\xbe" : "\xe2\x96\xb8"; // ▾ or ▸
            int lx = 1;
            ctx.put(lx, y, triangle, arrow_fg);
            lx += 2;
            ctx.put(lx++, y, BoxDraw::V, Color::Black);
            lx++; // space before name (bg already filled)

            // Category name
            for (char ch : cat.name)
                ctx.put(lx++, y, ch, name_fg, bar_bg);

            // Cost right-aligned (only for locked categories)
            if (!unlocked) {
                std::string cost = std::to_string(cat.sp_cost) + " SP";
                int cx_pos = half - 1 - static_cast<int>(cost.size());
                for (int ci = 0; ci < static_cast<int>(cost.size()); ++ci)
                    ctx.put(cx_pos + ci, y, cost[ci], Color::Yellow, bar_bg);
            }
        } else {
            // Skill entry
            const auto& sk = catalog[ve.ci].skills[ve.si];
            bool learned = false;
            for (auto sid : player_->learned_skills) {
                if (sid == sk.id) { learned = true; break; }
            }

            if (selected) selected_skill = &sk;

            // Check if affordable/meets requirements
            bool can_afford = player_->skill_points >= sk.sp_cost;
            bool meets_req = true;
            if (sk.attribute_req > 0 && sk.attribute_name) {
                const auto& a = player_->attributes;
                std::string attr(sk.attribute_name);
                int val = 0;
                if (attr == "Agility") val = a.agility;
                else if (attr == "Strength") val = a.strength;
                else if (attr == "Toughness") val = a.toughness;
                else if (attr == "Intelligence") val = a.intelligence;
                else if (attr == "Willpower") val = a.willpower;
                else if (attr == "Luck") val = a.luck;
                if (val < sk.attribute_req) meets_req = false;
            }

            // Cursor
            if (selected) ctx.put(3, y, '>', Color::Yellow);

            // : prefix + skill name
            Color colon_color = learned ? Color::Green : Color::DarkGray;
            ctx.put(5, y, ':', colon_color);

            UITag name_tag;
            if (learned) name_tag = UITag::TextSuccess;
            else if (selected) name_tag = UITag::TextBright;
            else if (!can_afford || !meets_req) name_tag = UITag::TextDim;
            else name_tag = UITag::TextDefault;

            ctx.text({.x = 6, .y = y, .content = sk.name, .tag = name_tag});

            // SP cost right-aligned
            std::string cost = std::to_string(sk.sp_cost) + " SP";
            int cx = half - 2 - static_cast<int>(cost.size());
            if (sk.attribute_req > 0 && sk.attribute_name) {
                std::string req = std::to_string(sk.attribute_req)
                                + std::string(sk.attribute_name).substr(0, 3);
                ctx.text({.x = cx - static_cast<int>(req.size()) - 1, .y = y, .content = req,
                          .tag = meets_req ? UITag::TextDim : UITag::TextDanger});
            }
            UITag cost_tag = learned ? UITag::TextDim : (can_afford ? UITag::TextWarning : UITag::TextDanger);
            ctx.text({.x = cx, .y = y, .content = cost, .tag = cost_tag});
        }
        y++;
    }

    // Detail panel on right
    int rx = half + 2;
    int rw = w - half - 3;

    // Helper: word-wrap text
    auto wrap_text = [&](int start_y, const std::string& text, Color default_color) {
        int dy = start_y;
        int line_x = 0;
        Color cur = default_color;
        for (size_t i = 0; i < text.size(); ++i) {
            unsigned char ch = static_cast<unsigned char>(text[i]);
            if (ch == static_cast<unsigned char>(COLOR_BEGIN) && i + 1 < text.size()) {
                cur = static_cast<Color>(static_cast<uint8_t>(text[i + 1]));
                ++i;
                continue;
            }
            if (ch == static_cast<unsigned char>(COLOR_END)) {
                cur = default_color;
                continue;
            }
            if (text[i] == '\n') {
                dy++;
                line_x = 0;
                continue;
            }
            if (text[i] == ' ' && line_x >= rw) {
                dy++;
                line_x = 0;
                continue;
            }
            ctx.put(rx + line_x, dy, text[i], cur);
            line_x++;
            if (line_x >= rw) {
                dy++;
                line_x = 0;
            }
        }
        return dy;
    };

    SkillId detail_id = SkillId::Cat_Acrobatics;
    bool have = false;
    if (selected_cat_idx >= 0 && !selected_skill) {
        detail_id = catalog[selected_cat_idx].unlock_id;
        have = true;
    } else if (selected_skill) {
        detail_id = selected_skill->id;
        have = true;
    }
    if (!have) return;

    SkillDetail det = skill_detail(detail_id);
    int dy = 2;
    ctx.text({.x = rx, .y = dy++, .content = det.header, .tag = UITag::TextBright});
    if (!det.cost_line.empty())
        ctx.text({.x = rx, .y = dy++, .content = det.cost_line, .tag = UITag::TextWarning});
    if (!det.requirement_line.empty())
        ctx.text({.x = rx, .y = dy++, .content = det.requirement_line, .tag = UITag::TextDim});
    dy++;
    wrap_text(dy, det.body, Color::DarkGray);
}

} // namespace astra
