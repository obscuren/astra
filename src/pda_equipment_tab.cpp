#include "astra/pda_screen.h"
#include "terminal_theme.h"

#include <array>
#include <string>

namespace astra {

void PdaScreen::draw_equipment(UIContext& ctx) {
    int w = ctx.width();
    int half = w / 2;

    // Top-left hint — Tab swaps between paper-doll views.
    const char* hint = (equipment_tab_view_ == EquipmentTabView::Equipment)
        ? "[Tab] Implants" : "[Tab] Equipment";
    ctx.text({.x = 0, .y = 0, .content = hint, .tag = UITag::TextDim});

    // Right side header: credits and weight.
    std::string money_str  = std::to_string(player_->money) + "$";
    std::string weight_str = std::to_string(player_->inventory.total_weight())
                           + "/" + std::to_string(player_->inventory.max_carry_weight) + " lb";
    ctx.text({.x = w - 1 - static_cast<int>(weight_str.size()), .y = 0,
              .content = weight_str, .tag = UITag::TextAccent});
    ctx.text({.x = w - 1 - static_cast<int>(weight_str.size()) - 3 - static_cast<int>(money_str.size()),
              .y = 0, .content = money_str, .tag = UITag::TextWarning});

    // Left side: whichever paper-doll the view is on.
    int dy = 1;
    if (equipment_tab_view_ == EquipmentTabView::Equipment) {
        draw_equipment_paperdoll(ctx, dy);
    } else {
        draw_implant_paperdoll(ctx, dy);
    }

    // Combined bonuses + penalties panel — covers both equipment and implants.
    {
        constexpr int slot_h = 5;
        int bonus_y = dy + slot_h * 6 + 1;
        if (bonus_y < ctx.height() - 3) {
            draw_combined_bonuses(ctx, bonus_y);
        }
    }

    // Footer hint.
    {
        const char* foot = (equip_focus_ == EquipFocus::PaperDoll)
            ? "[\xe2\x86\x92] Inventory  [Space] Actions"
            : "[\xe2\x86\x90] Paper doll  [Space] Actions";
        ctx.text({.x = 0, .y = ctx.height() - 1,
                  .content = foot, .tag = UITag::TextDim});
    }

    // Right side: shared categorized inventory list — same in both views.
    draw_equipment_inventory(ctx, half);
}

// ---- Equipment paper-doll (CoQ-style with connector lines) -----------------

void PdaScreen::draw_equipment_paperdoll(UIContext& ctx, int dy) {
    int w    = ctx.width();
    int half = w / 2;

    constexpr int bw = 7;
    constexpr int bh = 3;
    constexpr int slot_h = 5;
    int cx = (half - 1) / 2;

    int col_c  = cx - bw / 2;
    int col_l  = col_c - bw - 2;
    int col_r  = col_c + bw + 2;
    int col_ll = col_l - bw - 2;
    int col_rr = col_r + bw + 2;

    struct SlotPos { int x; int y; EquipSlot slot; const char* label; };
    SlotPos positions[] = {
        {col_c,  dy,              EquipSlot::Face,      "Face"},
        {col_c,  dy + slot_h,     EquipSlot::Head,      "Head"},
        {col_ll, dy + slot_h * 2, EquipSlot::LeftHand,  "L.Hand"},
        {col_l,  dy + slot_h * 2, EquipSlot::LeftArm,   "L.Arm"},
        {col_c,  dy + slot_h * 2, EquipSlot::Body,      "Body"},
        {col_r,  dy + slot_h * 2, EquipSlot::RightArm,  "R.Arm"},
        {col_rr, dy + slot_h * 2, EquipSlot::RightHand, "R.Hand"},
        {col_c,  dy + slot_h * 3, EquipSlot::Back,      "Back"},
        {col_c,  dy + slot_h * 4, EquipSlot::Feet,      "Feet"},
        {col_l,  dy + slot_h * 5, EquipSlot::Thrown,    "Thrown"},
        {col_r,  dy + slot_h * 5, EquipSlot::Missile,   "Missile"},
        {col_l,  dy + slot_h * 3, EquipSlot::Shield,    "Shield"},
        {col_r,  dy + slot_h * 3, EquipSlot::Utility1,  "Util 1"},
        {col_rr, dy + slot_h * 3, EquipSlot::Utility2,  "Util 2"},
    };

    Color line_color = Color::DarkGray;
    auto draw_vconn = [&](int row_top, int row_bot) {
        int x = col_c + bw / 2;
        int y_start = positions[row_top].y + bh;
        int y_end   = positions[row_bot].y;
        for (int vy = y_start; vy < y_end; ++vy) {
            ctx.put(x, vy, BoxDraw::V, line_color);
        }
    };
    draw_vconn(0, 1);
    draw_vconn(1, 4);
    draw_vconn(4, 7);
    draw_vconn(7, 8);

    {
        int row_y = positions[4].y + bh / 2;
        for (int hx = col_ll + bw; hx < col_l;  ++hx) ctx.put(hx, row_y, BoxDraw::H, line_color);
        for (int hx = col_l  + bw; hx < col_c;  ++hx) ctx.put(hx, row_y, BoxDraw::H, line_color);
        for (int hx = col_c  + bw; hx < col_r;  ++hx) ctx.put(hx, row_y, BoxDraw::H, line_color);
        for (int hx = col_r  + bw; hx < col_rr; ++hx) ctx.put(hx, row_y, BoxDraw::H, line_color);
    }
    {
        int row_y = positions[7].y + bh / 2;
        for (int hx = col_l + bw; hx < col_c; ++hx) ctx.put(hx, row_y, BoxDraw::H, line_color);
    }

    for (int i = 0; i < static_cast<int>(std::size(positions)); ++i) {
        const auto& sp = positions[i];
        bool selected = (equip_focus_ == EquipFocus::PaperDoll && equip_cursor_ == i);
        Color border_color = selected ? Color::Yellow : Color::DarkGray;
        const auto& item = player_->equipment.slot_ref(sp.slot);

        int bx = sp.x;
        int by = sp.y;

        ctx.put(bx, by, BoxDraw::TL, border_color);
        for (int j = 1; j < bw - 1; ++j) ctx.put(bx + j, by, BoxDraw::H, border_color);
        ctx.put(bx + bw - 1, by, BoxDraw::TR, border_color);

        ctx.put(bx, by + 1, BoxDraw::V, border_color);
        if (item) {
            int mid = bx + bw / 2;
            auto vis = item_visual(item->item_def_id);
            ctx.put(mid, by + 1, vis.glyph, rarity_color(item->rarity));
        } else {
            ctx.text(bx + 2, by + 1, "   ", Color::DarkGray);
        }
        ctx.put(bx + bw - 1, by + 1, BoxDraw::V, border_color);

        ctx.put(bx, by + 2, BoxDraw::BL, border_color);
        for (int j = 1; j < bw - 1; ++j) ctx.put(bx + j, by + 2, BoxDraw::H, border_color);
        ctx.put(bx + bw - 1, by + 2, BoxDraw::BR, border_color);

        std::string label(sp.label);
        int label_x = bx + (bw - static_cast<int>(label.size())) / 2;
        ctx.text({.x = label_x, .y = by + 3, .content = label,
                  .tag = selected ? UITag::TextWarning : UITag::TextAccent});
    }
}

// ---- Implant paper-doll (2 vertical slots) ---------------------------------

void PdaScreen::draw_implant_paperdoll(UIContext& ctx, int dy) {
    int w    = ctx.width();
    int half = w / 2;

    constexpr int bw = 9;
    constexpr int bh = 3;
    constexpr int slot_h = 5;
    int cx = (half - 1) / 2;
    int col_c = cx - bw / 2;

    for (int s = 0; s < Player::IMPLANT_SLOTS; ++s) {
        bool selected = (equip_focus_ == EquipFocus::PaperDoll && equip_cursor_ == s);
        Color border_color = selected ? Color::Yellow : Color::DarkGray;
        int bx = col_c;
        int by = dy + s * slot_h;

        ctx.put(bx, by, BoxDraw::TL, border_color);
        for (int j = 1; j < bw - 1; ++j) ctx.put(bx + j, by, BoxDraw::H, border_color);
        ctx.put(bx + bw - 1, by, BoxDraw::TR, border_color);

        ctx.put(bx, by + 1, BoxDraw::V, border_color);
        const auto& implant = player_->implants[s];
        if (implant) {
            auto vis = item_visual(implant->item_def_id);
            ctx.put(bx + bw / 2, by + 1, vis.glyph, rarity_color(implant->rarity));
        }
        ctx.put(bx + bw - 1, by + 1, BoxDraw::V, border_color);

        ctx.put(bx, by + 2, BoxDraw::BL, border_color);
        for (int j = 1; j < bw - 1; ++j) ctx.put(bx + j, by + 2, BoxDraw::H, border_color);
        ctx.put(bx + bw - 1, by + 2, BoxDraw::BR, border_color);

        std::string slot_label = "Slot " + std::to_string(s + 1);
        int label_x = bx + (bw - static_cast<int>(slot_label.size())) / 2;
        ctx.text({.x = label_x, .y = by + 3,
                  .content = slot_label,
                  .tag = selected ? UITag::TextWarning : UITag::TextAccent});

        // Item name + per-stat modifiers to the right of the box.
        int name_x = bx + bw + 2;
        if (implant) {
            ctx.text({.x = name_x, .y = by + 1,
                      .content = implant->name,
                      .tag = rarity_tag(implant->rarity)});
            // Show the first non-zero modifier inline next to the name so
            // the user can see at a glance what this implant does. The full
            // bonus/penalty roll-up happens in the BONUSES section below.
            const auto& m = implant->modifiers;
            struct Mod { const char* label; int value; };
            const Mod fields[] = {
                {"WIL", m.willpower}, {"AV", m.av},  {"DV", m.dv},
                {"HP",  m.max_hp},    {"VIS", m.view_radius},
                {"QCK", m.quickness},
            };
            for (const auto& f : fields) {
                if (f.value == 0) continue;
                std::string mod_str = std::string(f.label) + " "
                    + (f.value > 0 ? "+" : "") + std::to_string(f.value);
                ctx.text({.x = name_x, .y = by + 2,
                          .content = mod_str,
                          .tag = f.value > 0 ? UITag::TextSuccess : UITag::TextDanger});
                break;
            }
        } else {
            ctx.text({.x = name_x, .y = by + 1,
                      .content = "(empty)", .tag = UITag::TextDim});
        }
    }
}

// ---- Combined bonuses + penalties roll-up ----------------------------------

void PdaScreen::draw_combined_bonuses(UIContext& ctx, int y) {
    auto eq = player_->equipment.total_modifiers();
    auto im = player_->implant_modifiers();

    struct Stat { const char* label; int total; UITag positive_tag; };
    Stat stats[] = {
        {"AV",  eq.av          + im.av,          UITag::StatAttack},
        {"DV",  eq.dv          + im.dv,          UITag::StatDefense},
        {"HP",  eq.max_hp      + im.max_hp,      UITag::StatHealth},
        {"VIS", eq.view_radius + im.view_radius, UITag::StatVision},
        {"QCK", eq.quickness   + im.quickness,   UITag::StatSpeed},
        {"WIL", eq.willpower   + im.willpower,   UITag::TextAccent},
    };

    // Section: BONUSES (positive totals).
    draw_section_header(ctx, y, "BONUSES");
    int row    = y + 1;
    int col    = 0;
    int max_col = 2;
    int col_w  = ctx.width() / 2 / (max_col + 1);
    bool any_positive = false;
    for (const auto& s : stats) {
        if (s.total <= 0) continue;
        any_positive = true;
        std::string label = std::string(s.label) + " +" + std::to_string(s.total);
        ctx.text({.x = 2 + col * col_w, .y = row,
                  .content = label, .tag = s.positive_tag});
        if (++col > max_col) { col = 0; ++row; }
    }
    if (!any_positive) {
        ctx.text({.x = 2, .y = row, .content = "(no active bonuses)",
                  .tag = UITag::TextDim});
        ++row;
    }
    if (col != 0) ++row;

    // Section: PENALTIES (negative totals — implants today, but anything
    // with a negative roll-up surfaces here).
    bool any_negative = false;
    for (const auto& s : stats) if (s.total < 0) { any_negative = true; break; }
    if (any_negative && row + 2 < ctx.height() - 1) {
        ++row;
        draw_section_header(ctx, row, "PENALTIES");
        ++row;
        col = 0;
        for (const auto& s : stats) {
            if (s.total >= 0) continue;
            std::string label = std::string(s.label) + " " + std::to_string(s.total);
            ctx.text({.x = 2 + col * col_w, .y = row,
                      .content = label, .tag = UITag::TextDanger});
            if (++col > max_col) { col = 0; ++row; }
        }
    }
}

// ---- Right-side categorized inventory --------------------------------------

void PdaScreen::draw_equipment_inventory(UIContext& ctx, int half) {
    int w  = ctx.width();
    int rw = w - half - 3;
    int rx = half + 2;
    int ry = 2;

    auto& items = player_->inventory.items;
    if (items.empty()) {
        ctx.text({.x = rx, .y = ry, .content = "Inventory empty.", .tag = UITag::TextDim});
        return;
    }

    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (ry >= ctx.height() - 2) break;
        const auto& item = items[i];
        bool selected = (equip_focus_ == EquipFocus::Inventory && inv_cursor_ == i);

        if (selected) ctx.put(rx - 1, ry, '>', Color::Yellow);

        draw_item_name(ctx, rx, ry, item, selected);

        std::string price = std::to_string(item.sell_value) + "$";
        int px = half + rw - static_cast<int>(price.size());
        ctx.text({.x = px, .y = ry, .content = price, .tag = UITag::TextWarning});
        ++ry;
    }
}

} // namespace astra
