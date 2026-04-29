#include "astra/pda_screen.h"
#include "terminal_theme.h"

#include <string>

namespace astra {

void PdaScreen::draw_equipment(UIContext& ctx) {
    int w = ctx.width();
    int half = w / 2;



    // Right side header: credits and weight
    std::string money_str = std::to_string(player_->money) + "$";
    std::string weight_str = std::to_string(player_->inventory.total_weight())
                           + "/" + std::to_string(player_->inventory.max_carry_weight) + " lb";
    ctx.text({.x = w - 1 - static_cast<int>(weight_str.size()), .y = 0,
              .content = weight_str, .tag = UITag::TextAccent});
    ctx.text({.x = w - 1 - static_cast<int>(weight_str.size()) - 3 - static_cast<int>(money_str.size()),
              .y = 0, .content = money_str, .tag = UITag::TextWarning});

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
        // Shield: tethered to left of Back
        {col_l,  dy + slot_h * 3, EquipSlot::Shield,    "Shield"},
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

    // Horizontal connector on Back row: Shield─Back
    {
        int row_y = positions[7].y + bh / 2; // middle of Back row
        for (int hx = col_l + bw; hx < col_c; ++hx)
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
            auto vis = item_visual(item->item_def_id);
            ctx.put(mid, by + 1, vis.glyph, rarity_color(item->rarity));
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
        ctx.text({.x = label_x, .y = by + 3, .content = label,
                  .tag = selected ? UITag::TextWarning : UITag::TextAccent});
    }

    // Bonuses at the bottom of left side
    int bonus_y = dy + slot_h * 6 + 1;
    if (bonus_y < ctx.height() - 3) {
        draw_section_header(ctx, bonus_y, "BONUSES");
        auto mods = player_->equipment.total_modifiers();
        ctx.styled_text({.x = 2, .y = bonus_y + 1, .segments = {
            {"AV +", UITag::StatAttack}, {std::to_string(mods.av), UITag::StatAttack},
            {"  DV +", UITag::StatDefense}, {std::to_string(mods.dv), UITag::StatDefense},
            {"  HP +", UITag::StatHealth}, {std::to_string(mods.max_hp), UITag::StatHealth},
        }});
        ctx.styled_text({.x = 2, .y = bonus_y + 2, .segments = {
            {"VIS +", UITag::StatVision}, {std::to_string(mods.view_radius), UITag::StatVision},
            {"  QCK +", UITag::StatSpeed}, {std::to_string(mods.quickness), UITag::StatSpeed},
        }});
    }

    // Right side: categorized inventory
    int ry = 2;
    int rx = half + 2;
    int rw = w - half - 3;

    auto& items = player_->inventory.items;
    if (items.empty()) {
        ctx.text({.x = rx, .y = ry, .content = "Inventory empty.", .tag = UITag::TextDim});
        return;
    }

    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (ry >= ctx.height() - 1) break;
        const auto& item = items[i];
        bool selected = (equip_focus_ == EquipFocus::Inventory && inv_cursor_ == i);

        if (selected) ctx.put(rx - 1, ry, '>', Color::Yellow);

        // display_name (called by draw_item_name) embeds the glyph in its
        // natural color, so we no longer draw a separate glyph here.
        draw_item_name(ctx, rx, ry, item, selected);

        std::string price = std::to_string(item.sell_value) + "$";
        int px = half + rw - static_cast<int>(price.size());
        ctx.text({.x = px, .y = ry, .content = price, .tag = UITag::TextWarning});

        ry++;
    }
}

} // namespace astra
