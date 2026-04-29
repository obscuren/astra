#include "astra/pda_screen.h"
#include "terminal_theme.h"

#include <string>

namespace astra {

void PdaScreen::draw_ship(UIContext& ctx) {
    int w = ctx.width();
    int half = w / 2;
    auto& ship = player_->ship;

    // Header: ship name + type
    std::string title = ship.name;
    if (!ship.type.empty()) title += " (" + ship.type + ")";
    ctx.text({.x = 2, .y = 0, .content = title, .tag = UITag::TextAccent});

    // Status
    std::string status = ship.operational() ? "Operational" : "GROUNDED";
    UITag status_tag = ship.operational() ? UITag::TextSuccess : UITag::TextDanger;
    ctx.text({.x = w - 2 - static_cast<int>(status.size()), .y = 0,
              .content = status, .tag = status_tag});

    int y = 2;

    // Actions section (Board Ship) — only rendered on the left column.
    draw_section_header(ctx, y, "ACTIONS");
    y += 2;
    {
        bool selected = (ship_focus_ == ShipFocus::Actions && ship_action_cursor_ == 0);
        if (selected && can_board_ship_) ctx.put(1, y, '>', Color::Yellow);
        if (can_board_ship_) {
            ctx.text({.x = 3, .y = y, .content = "Board Ship",
                      .tag = selected ? UITag::TextWarning : UITag::TextBright});
            ctx.text({.x = 3 + 12, .y = y,
                      .content = "  [Enter]",
                      .tag = UITag::TextDim});
        } else {
            ctx.text({.x = 3, .y = y,
                      .content = "Board Ship — must be on a planet",
                      .tag = UITag::TextDim});
        }
    }
    y += 2;

    // Component slots on the left
    draw_section_header(ctx, y, "COMPONENTS");
    y += 2;

    for (int i = 0; i < ship_slot_count; ++i) {
        if (y >= ctx.height() - 1) break;
        auto slot = static_cast<ShipSlot>(i);
        const auto& item = ship.slot_ref(slot);
        bool selected = (ship_focus_ == ShipFocus::Equipment && ship_equip_cursor_ == i);
        bool is_critical = (slot == ShipSlot::Engine || slot == ShipSlot::Hull
                         || slot == ShipSlot::NaviComputer);

        if (selected) ctx.put(1, y, '>', Color::Yellow);

        std::string slot_label = std::string(ship_slot_name(slot)) + ": ";
        ctx.text({.x = 3, .y = y, .content = slot_label,
                  .tag = selected ? UITag::TextWarning : UITag::TextBright});

        int name_x = 3 + static_cast<int>(slot_label.size());
        if (item) {
            auto ship_vis = item_visual(item->item_def_id);
            ctx.put(name_x, y, ship_vis.glyph, rarity_color(item->rarity));
            ctx.text({.x = name_x + 2, .y = y, .content = item->name,
                      .tag = selected ? UITag::TextBright : UITag::TextDefault});
        } else {
            std::string empty_label = is_critical ? "OFFLINE" : "(empty)";
            UITag empty_tag = is_critical ? UITag::TextDanger : UITag::TextDim;
            ctx.text({.x = name_x, .y = y, .content = empty_label, .tag = empty_tag});
        }
        y++;
    }

    // Diagnostics section below components
    y++;
    draw_section_header(ctx, y, "DIAGNOSTICS");
    y += 2;
    auto mods = ship.total_modifiers();
    if (mods.hull_hp > 0) {
        ctx.label_value({.x = 3, .y = y, .label = "Hull: ", .label_tag = UITag::TextDim,
                         .value = std::to_string(mods.hull_hp) + " HP", .value_tag = UITag::StatHealth});
        y++;
    }
    if (mods.shield_hp > 0) {
        ctx.label_value({.x = 3, .y = y, .label = "Shield: ", .label_tag = UITag::TextDim,
                         .value = std::to_string(mods.shield_hp) + " HP", .value_tag = UITag::StatDefense});
        y++;
    }
    if (mods.warp_range > 0) {
        ctx.label_value({.x = 3, .y = y, .label = "Warp Range: ", .label_tag = UITag::TextDim,
                         .value = "+" + std::to_string(mods.warp_range), .value_tag = UITag::TextAccent});
        y++;
    }
    if (mods.hull_hp == 0 && mods.shield_hp == 0 && mods.warp_range == 0) {
        ctx.text({.x = 3, .y = y, .content = "No active systems.", .tag = UITag::TextDim});
        y++;
    }

    // Footer for interaction hint
    if (!on_ship_) {
        int footer_y = ctx.height() - 1;
        ctx.text({.x = 2, .y = footer_y, .content = "Board your ship to manage equipment.",
                  .tag = UITag::TextDim});
    }

    // Right side: ship cargo hold
    int ry = 2;
    int rx = half + 2;
    int rw = w - half - 3;

    auto& cargo = player_->ship.cargo;
    if (cargo.empty()) {
        ctx.text({.x = rx, .y = ry, .content = "Cargo hold empty.", .tag = UITag::TextDim});
    } else {
        for (int si = 0; si < static_cast<int>(cargo.size()); ++si) {
            if (ry >= ctx.height() - 1) break;
            const auto& item = cargo[si];
            bool selected = (ship_focus_ == ShipFocus::Inventory && ship_inv_cursor_ == si);

            if (selected) ctx.put(rx - 1, ry, '>', Color::Yellow);
            auto cargo_vis = item_visual(item.item_def_id);
            ctx.put(rx, ry, cargo_vis.glyph, rarity_color(item.rarity));

            std::string name = item.name;
            if (item.ship_slot) {
                name += " [" + std::string(ship_slot_name(*item.ship_slot)) + "]";
            }
            ctx.text({.x = rx + 2, .y = ry, .content = name,
                      .tag = selected ? UITag::TextBright : UITag::TextDefault});

            std::string price = std::to_string(item.sell_value) + "$";
            int px = half + rw - static_cast<int>(price.size());
            ctx.text({.x = px, .y = ry, .content = price, .tag = UITag::TextWarning});

            ry++;
        }
    }
}

} // namespace astra
