#include "astra/pda_screen.h"
#include "terminal_theme.h"

#include <array>
#include <string>

namespace astra {

// Single source of truth for paper-doll cursor → EquipSlot. The visual
// layout (left → right, top → bottom) is independent of the EquipSlot
// enum's declaration order, so action sites must look up the slot for a
// cursor through this table — `static_cast<EquipSlot>(equip_cursor_)` is
// a bug.
// Visual order for the implant paper-doll: top-to-bottom, left-to-right per
// row. Cursor 0 starts on L.Hnd. Keep in lockstep with positions[] in
// draw_implant_paperdoll().
static constexpr ImplantSlot kImplantPaperdollSlots[] = {
    ImplantSlot::LeftHand,   // 0
    ImplantSlot::Eyes,       // 1
    ImplantSlot::Head,       // 2
    ImplantSlot::RightHand,  // 3
    ImplantSlot::LeftArm,    // 4
    ImplantSlot::Spine,      // 5
    ImplantSlot::RightArm,   // 6
    ImplantSlot::Chest,      // 7
    ImplantSlot::LeftLeg,    // 8
    ImplantSlot::RightLeg,   // 9
};
static constexpr int kImplantPaperdollSlotCount =
    static_cast<int>(std::size(kImplantPaperdollSlots));

ImplantSlot implant_paperdoll_slot_at_cursor(int cursor) {
    if (cursor < 0 || cursor >= kImplantPaperdollSlotCount) return ImplantSlot::Head;
    return kImplantPaperdollSlots[cursor];
}

int implant_paperdoll_slot_count() { return kImplantPaperdollSlotCount; }

static constexpr EquipSlot kPaperdollSlots[] = {
    EquipSlot::Face,       // 0
    EquipSlot::Head,       // 1
    EquipSlot::LeftHand,   // 2
    EquipSlot::LeftArm,    // 3
    EquipSlot::Body,       // 4
    EquipSlot::RightArm,   // 5
    EquipSlot::RightHand,  // 6
    EquipSlot::Back,       // 7
    EquipSlot::Feet,       // 8
    EquipSlot::Thrown,     // 9
    EquipSlot::Missile,    // 10
    EquipSlot::Shield,     // 11
    EquipSlot::Utility1,   // 12
    EquipSlot::Utility2,   // 13
};
static constexpr int kPaperdollSlotCount =
    static_cast<int>(std::size(kPaperdollSlots));

EquipSlot paperdoll_slot_at_cursor(int cursor) {
    if (cursor < 0 || cursor >= kPaperdollSlotCount) return EquipSlot::Face;
    return kPaperdollSlots[cursor];
}

int paperdoll_slot_count() { return kPaperdollSlotCount; }

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

    struct SlotPos { int x; int y; const char* label; };
    // The slot for index i is kPaperdollSlots[i] — same order. Keep these
    // two arrays in lockstep; if you reorder the visual layout, update
    // kPaperdollSlots above to match.
    SlotPos positions[] = {
        {col_c,  dy,              "Face"},   // 0  Face
        {col_c,  dy + slot_h,     "Head"},   // 1  Head
        {col_ll, dy + slot_h * 2, "L.Hand"}, // 2  LeftHand
        {col_l,  dy + slot_h * 2, "L.Arm"},  // 3  LeftArm
        {col_c,  dy + slot_h * 2, "Body"},   // 4  Body
        {col_r,  dy + slot_h * 2, "R.Arm"},  // 5  RightArm
        {col_rr, dy + slot_h * 2, "R.Hand"}, // 6  RightHand
        {col_c,  dy + slot_h * 3, "Back"},   // 7  Back
        {col_c,  dy + slot_h * 4, "Feet"},   // 8  Feet
        {col_l,  dy + slot_h * 5, "Thrown"}, // 9  Thrown
        {col_r,  dy + slot_h * 5, "Missile"},// 10 Missile
        {col_l,  dy + slot_h * 3, "Shield"}, // 11 Shield
        {col_r,  dy + slot_h * 3, "Util 1"}, // 12 Utility1
        {col_rr, dy + slot_h * 3, "Util 2"}, // 13 Utility2
    };
    static_assert(std::size(positions) == kPaperdollSlotCount,
                  "positions[] and kPaperdollSlots[] must stay in lockstep");

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
        const auto& item = player_->equipment.slot_ref(kPaperdollSlots[i]);

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

// ---- Implant paper-doll (10-slot anatomical humanoid layout) ---------------

void PdaScreen::draw_implant_paperdoll(UIContext& ctx, int dy) {
    int w    = ctx.width();
    int half = w / 2;

    constexpr int bw     = 7;   // box width  (matches equipment paperdoll)
    constexpr int bh     = 3;   // box height
    constexpr int slot_h = 5;   // row stride

    int cx = (half - 1) / 2;

    // Column positions — mirrored from draw_equipment_paperdoll style.
    // col_c   : Head / Spine / Chest (center column)
    // col_l   : Eyes (just left of center)
    // col_arm_l / col_arm_r : L.Hand + L.Arm / R.Hand + R.Arm (far flanks)
    // col_leg_l / col_leg_r : legs tight under Chest
    int col_c     = cx - bw / 2;
    int col_l     = col_c - bw - 2;
    int col_arm_l = col_l - bw - 2;
    int col_arm_r = col_c + bw + 2;
    // col_leg_l/r: legs sit just inside center. bw/2 (=3) + 1 gap = 4 cols from
    // the center-column origin keeps leg boxes flush below Chest without overlap.
    int col_leg_l = col_c - 4;
    int col_leg_r = col_c + 4;

    // Position table — parallel to kImplantPaperdollSlots[]. Slot identity for
    // index i is kImplantPaperdollSlots[i]; don't repeat it here.
    struct SlotPos { int x; int y; const char* label; };
    SlotPos positions[] = {
        {col_arm_l, dy,                  "L.Hnd"},  // 0  LeftHand
        {col_l,     dy,                  "Eyes"},   // 1  Eyes
        {col_c,     dy,                  "Head"},   // 2  Head
        {col_arm_r, dy,                  "R.Hnd"},  // 3  RightHand
        {col_arm_l, dy + slot_h,         "L.Arm"},  // 4  LeftArm
        {col_c,     dy + slot_h,         "Spine"},  // 5  Spine
        {col_arm_r, dy + slot_h,         "R.Arm"},  // 6  RightArm
        {col_c,     dy + slot_h * 2,     "Chest"},  // 7  Chest
        {col_leg_l, dy + slot_h * 3 + 1, "L.Leg"},  // 8  LeftLeg
        {col_leg_r, dy + slot_h * 3 + 1, "R.Leg"},  // 9  RightLeg
    };
    static_assert(std::size(positions) == static_cast<size_t>(kImplantPaperdollSlotCount),
                  "positions[] and kImplantPaperdollSlots[] must stay in lockstep");

    Color line_color = Color::DarkGray;

    // Draws a vertical connector between the bottom of one box and the top of
    // another, along an arbitrary x column — mirrors draw_equipment_paperdoll.
    auto draw_vconn = [&](int col_x, int y_top_box, int y_bot_box) {
        for (int vy = y_top_box + bh; vy < y_bot_box; ++vy)
            ctx.put(col_x + bw / 2, vy, BoxDraw::V, line_color);
    };

    // ---- Connector lines ----

    // Eyes ── Head: single '─' in the gap between the two boxes at top row.
    {
        int conn_y = dy + bh / 2;
        for (int hx = col_l + bw; hx < col_c; ++hx)
            ctx.put(hx, conn_y, BoxDraw::H, line_color);
    }

    draw_vconn(col_arm_l, dy,          dy + slot_h);      // L.Hnd → L.Arm
    draw_vconn(col_c,     dy,          dy + slot_h);      // Head  → Spine
    draw_vconn(col_arm_r, dy,          dy + slot_h);      // R.Hnd → R.Arm
    draw_vconn(col_c,     dy + slot_h, dy + slot_h * 2);  // Spine → Chest

    // Horizontal: L.Arm ── Spine ── R.Arm
    {
        int row_y = dy + slot_h + bh / 2;
        for (int hx = col_arm_l + bw; hx < col_c; ++hx)
            ctx.put(hx, row_y, BoxDraw::H, line_color);
        for (int hx = col_c + bw; hx < col_arm_r; ++hx)
            ctx.put(hx, row_y, BoxDraw::H, line_color);
    }

    // Chest → Legs T-junction.
    // Chest box bottom edge is at: dy + slot_h*2 + bh - 1 = dy + slot_h*2 + 2
    // Leg box top is at: dy + slot_h*3 + 1
    // We place the T-junction row directly below Chest bottom.
    {
        // Center of Chest box (x)
        int chest_cx = col_c + bw / 2;
        int tjunc_y  = dy + slot_h * 2 + bh;  // one row below Chest box bottom

        // Left leg center and right leg center
        int ll_cx = col_leg_l + bw / 2;
        int rl_cx = col_leg_r + bw / 2;

        // Draw: ┌─┴─┐ centered on chest_cx
        ctx.put(ll_cx,     tjunc_y, BoxDraw::TL, line_color);
        for (int hx = ll_cx + 1; hx < chest_cx; ++hx)
            ctx.put(hx, tjunc_y, BoxDraw::H, line_color);
        ctx.put(chest_cx,  tjunc_y, BoxDraw::BT, line_color);   // ┴
        for (int hx = chest_cx + 1; hx < rl_cx; ++hx)
            ctx.put(hx, tjunc_y, BoxDraw::H, line_color);
        ctx.put(rl_cx,     tjunc_y, BoxDraw::TR, line_color);

        // Vertical drops from T-junction corners down to leg box tops.
        int leg_top = dy + slot_h * 3 + 1;
        for (int vy = tjunc_y + 1; vy < leg_top; ++vy) {
            ctx.put(ll_cx, vy, BoxDraw::V, line_color);
            ctx.put(rl_cx, vy, BoxDraw::V, line_color);
        }
    }

    // ---- Slot boxes ----
    for (int i = 0; i < kImplantPaperdollSlotCount; ++i) {
        const auto& sp = positions[i];
        bool selected = (equip_focus_ == EquipFocus::PaperDoll && equip_cursor_ == i);
        Color border_color = selected ? Color::Yellow : Color::DarkGray;

        int bx = sp.x;
        int by = sp.y;

        ctx.put(bx, by, BoxDraw::TL, border_color);
        for (int j = 1; j < bw - 1; ++j) ctx.put(bx + j, by, BoxDraw::H, border_color);
        ctx.put(bx + bw - 1, by, BoxDraw::TR, border_color);

        ctx.put(bx, by + 1, BoxDraw::V, border_color);
        const auto& implant = player_->implant_at(kImplantPaperdollSlots[i]);
        if (implant) {
            int mid = bx + bw / 2;
            auto vis = item_visual(implant->item_def_id);
            ctx.put(mid, by + 1, vis.glyph, rarity_color(implant->rarity));
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

    // Empty-slot hint to the right of the paperdoll; filled slots show the
    // glyph in the box itself and rely on the inventory look view for details.
    {
        int cursor = (equip_focus_ == EquipFocus::PaperDoll) ? equip_cursor_ : 0;
        if (cursor < 0 || cursor >= kImplantPaperdollSlotCount)
            cursor = 0;
        ImplantSlot sel_slot = kImplantPaperdollSlots[cursor];
        if (!player_->implant_at(sel_slot)) {
            int info_x = col_arm_r + bw + 2;
            int info_y = dy + slot_h;
            ctx.text({.x = info_x, .y = info_y,
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
