#include "astra/trade_window.h"
#include "astra/character.h"
#include "astra/effect.h"
#include "astra/player.h"
#include "terminal_theme.h"

#include <algorithm>

namespace astra {

bool TradeWindow::is_open() const { return open_; }

void TradeWindow::open(Npc* merchant, Player* player, Renderer* renderer) {
    merchant_ = merchant;
    player_ = player;
    renderer_ = renderer;
    open_ = true;
    active_side_ = Side::Merchant;
    merchant_cursor_ = 0;
    player_cursor_ = 0;
    merchant_scroll_ = 0;
    player_scroll_ = 0;
    status_msg_.clear();
    status_timer_ = 0;
}

void TradeWindow::close() { open_ = false; }

bool TradeWindow::has_message() const { return !status_msg_.empty(); }

std::string TradeWindow::consume_message() {
    std::string msg = std::move(status_msg_);
    status_msg_.clear();
    return msg;
}

bool TradeWindow::handle_input(int key) {
    if (!open_) return false;

    if (status_timer_ > 0) --status_timer_;

    if (key == 27) { // ESC
        close();
        return true;
    }

    if (key == '\t') {
        active_side_ = (active_side_ == Side::Merchant) ? Side::Player : Side::Merchant;
        return true;
    }

    auto& items = (active_side_ == Side::Merchant)
        ? merchant_->interactions.shop->inventory
        : player_->inventory.items;
    auto& cursor = (active_side_ == Side::Merchant) ? merchant_cursor_ : player_cursor_;

    int count = static_cast<int>(items.size());
    // Player side includes ship cargo
    if (active_side_ == Side::Player)
        count += static_cast<int>(player_->ship.cargo.size());

    if (key == KEY_UP) {
        if (cursor > 0) --cursor;
        return true;
    }
    if (key == KEY_DOWN) {
        if (cursor < count - 1) ++cursor;
        return true;
    }

    if (key == ' ') {
        if (active_side_ == Side::Merchant) {
            buy_selected();
        } else {
            sell_selected();
        }
        return true;
    }

    return true;
}

void TradeWindow::buy_selected() {
    auto& inv = merchant_->interactions.shop->inventory;
    if (inv.empty()) return;
    if (merchant_cursor_ < 0 || merchant_cursor_ >= static_cast<int>(inv.size())) return;

    auto& item = inv[merchant_cursor_];
    int cost = compute_price(item, true);

    if (player_->money < cost) {
        status_msg_ = "Not enough credits. (Need " + std::to_string(cost) + "$)";
        status_timer_ = 3;
        return;
    }

    if (!player_->inventory.can_add(item)) {
        status_msg_ = "Too heavy to carry.";
        status_timer_ = 3;
        return;
    }

    player_->money -= cost;

    // Build single item to add to player
    Item bought = item;
    bought.stack_count = 1;

    if (bought.type == ItemType::ShipComponent) {
        // Ship components go to ship cargo
        player_->ship.cargo.push_back(std::move(bought));
    } else {
        // Merge into existing stack if possible
        bool merged = false;
        if (bought.stackable) {
            for (auto& pi : player_->inventory.items) {
                if (pi.id == bought.id) {
                    pi.stack_count += 1;
                    merged = true;
                    break;
                }
            }
        }
        if (!merged) {
            player_->inventory.items.push_back(std::move(bought));
        }
    }

    // Remove from merchant
    if (item.stackable && item.stack_count > 1) {
        item.stack_count -= 1;
    } else {
        inv.erase(inv.begin() + merchant_cursor_);
        if (merchant_cursor_ >= static_cast<int>(inv.size()) && merchant_cursor_ > 0) {
            --merchant_cursor_;
        }
    }

    status_msg_ = "Bought " + bought.name + " for " + std::to_string(cost) + "$.";
    status_timer_ = 3;
}

void TradeWindow::sell_selected() {
    // Combined sell list: player inventory + ship cargo
    auto& inv = player_->inventory.items;
    auto& cargo = player_->ship.cargo;
    int inv_count = static_cast<int>(inv.size());
    int total = inv_count + static_cast<int>(cargo.size());
    if (total == 0) return;
    if (player_cursor_ < 0 || player_cursor_ >= total) return;

    bool from_cargo = (player_cursor_ >= inv_count);
    Item& item = from_cargo ? cargo[player_cursor_ - inv_count] : inv[player_cursor_];

    int price = compute_price(item, false);

    player_->money += price;

    std::string sold_name = item.name;

    // Build single item to add to merchant
    Item sold = item;
    sold.stack_count = 1;

    // Merge into merchant stock if possible
    auto& shop_inv = merchant_->interactions.shop->inventory;
    bool merged = false;
    if (sold.stackable) {
        for (auto& si : shop_inv) {
            if (si.id == sold.id) {
                si.stack_count += 1;
                merged = true;
                break;
            }
        }
    }
    if (!merged) {
        shop_inv.push_back(std::move(sold));
    }

    // Remove from source
    if (item.stackable && item.stack_count > 1) {
        item.stack_count -= 1;
    } else {
        if (from_cargo) {
            int ci = player_cursor_ - inv_count;
            cargo.erase(cargo.begin() + ci);
        } else {
            inv.erase(inv.begin() + player_cursor_);
        }
        int new_total = static_cast<int>(inv.size()) + static_cast<int>(cargo.size());
        if (player_cursor_ >= new_total && player_cursor_ > 0)
            --player_cursor_;
    }

    status_msg_ = "Sold " + sold_name + " for " + std::to_string(price) + "$.";
    status_timer_ = 3;
}

int TradeWindow::compute_price(const Item& item, bool buy) const {
    int base = buy ? item.buy_value : item.sell_value;
    int effect_pct = buy ? effect_buy_price_pct(player_->effects)
                         : effect_sell_price_pct(player_->effects);
    int faction_pct = reputation_price_pct(reputation_for(*player_, merchant_->faction));
    if (!buy) faction_pct = -faction_pct;
    int price = base + (base * (effect_pct + faction_pct) / 100);
    if (price < (buy ? 1 : 0)) price = buy ? 1 : 0;
    return price;
}

void TradeWindow::draw(int screen_w, int screen_h) {
    if (!open_ || !renderer_) return;

    // Outer panel (centered with margin)
    int margin = 4;
    Rect bounds{margin, margin, screen_w - margin * 2, screen_h - margin * 2};
    UIContext full(renderer_, bounds);
    auto content = full.panel({
        .title = "Trade",
        .footer = "[ESC] Close  [TAB] Switch  [\xe2\x86\x91\xe2\x86\x93] Move  [SPACE] Buy/Sell",
    });

    int w = content.width();
    int h = content.height();

    // Two columns with vertical divider
    auto cols = content.columns({fill(), fixed(1), fill()});
    auto& buy_col = cols[0];
    auto& divider = cols[1];
    auto& sell_col = cols[2];

    divider.separator({.vertical = true});

    bool merchant_active = (active_side_ == Side::Merchant);

    // --- Buy column layout: header, sep, items, sep, info ---
    int faction_pct = reputation_price_pct(reputation_for(*player_, merchant_->faction));
    int buy_info_rows = (faction_pct != 0) ? 2 : 1;
    auto buy_rows = buy_col.rows({fixed(1), fixed(1), fill(), fixed(1), fixed(buy_info_rows)});
    auto& buy_header = buy_rows[0];
    auto& buy_hsep   = buy_rows[1];
    auto& buy_list   = buy_rows[2];
    auto& buy_bsep   = buy_rows[3];
    auto& buy_info   = buy_rows[4];

    std::string shop_name = merchant_->interactions.shop->shop_name;
    buy_header.text({
        .x = std::max(0, (buy_header.width() - static_cast<int>(shop_name.size())) / 2),
        .y = 0,
        .content = shop_name,
        .tag = merchant_active ? UITag::TextBright : UITag::TextDim,
    });
    buy_hsep.separator({});

    draw_buy_items(buy_list, buy_list.width());

    buy_bsep.separator({});

    // Credits display centered
    std::string credits_str = std::to_string(player_->money) + "$";
    buy_info.label_value({
        .x = std::max(0, (buy_info.width() - static_cast<int>(credits_str.size()) - 9) / 2),
        .y = 0,
        .label = "Credits: ",
        .label_tag = UITag::TextDim,
        .value = credits_str,
        .value_tag = UITag::TextWarning,
    });

    if (faction_pct != 0) {
        std::string faction_str = merchant_->faction + ": ";
        if (faction_pct > 0) faction_str += "+" + std::to_string(faction_pct) + "% markup";
        else faction_str += std::to_string(faction_pct) + "% discount";
        UITag fc = faction_pct > 0 ? UITag::TextDanger : UITag::TextSuccess;
        buy_info.text({
            .x = std::max(0, (buy_info.width() - static_cast<int>(faction_str.size())) / 2),
            .y = 1,
            .content = faction_str,
            .tag = fc,
        });
    }

    // --- Sell column layout: header, sep, items, sep, info ---
    auto sell_rows = sell_col.rows({fixed(1), fixed(1), fill(), fixed(1), fixed(1)});
    auto& sell_header = sell_rows[0];
    auto& sell_hsep   = sell_rows[1];
    auto& sell_list   = sell_rows[2];
    auto& sell_bsep   = sell_rows[3];
    auto& sell_info   = sell_rows[4];

    std::string player_title = "Your Inventory";
    sell_header.text({
        .x = std::max(0, (sell_header.width() - static_cast<int>(player_title.size())) / 2),
        .y = 0,
        .content = player_title,
        .tag = !merchant_active ? UITag::TextBright : UITag::TextDim,
    });
    sell_hsep.separator({});

    draw_sell_items(sell_list, sell_list.width());

    sell_bsep.separator({});

    // Weight display centered
    int total_w = player_->inventory.total_weight();
    int max_w = player_->inventory.max_carry_weight;
    std::string weight_str = std::to_string(total_w) + "/" + std::to_string(max_w);
    UITag weight_tag = (total_w > max_w) ? UITag::TextDanger : UITag::TextAccent;
    sell_info.label_value({
        .x = std::max(0, (sell_info.width() - static_cast<int>(weight_str.size()) - 8) / 2),
        .y = 0,
        .label = "Weight: ",
        .label_tag = UITag::TextDim,
        .value = weight_str,
        .value_tag = weight_tag,
    });

    // Status message (overlaid at bottom of content area)
    if (status_timer_ > 0 && !status_msg_.empty()) {
        int sy = h - 1;
        content.text({
            .x = std::max(0, (w - static_cast<int>(status_msg_.size())) / 2),
            .y = sy,
            .content = status_msg_,
            .tag = UITag::TextSuccess,
        });
    }
}

void TradeWindow::draw_buy_items(UIContext& area, int list_w) {
    auto& items = merchant_->interactions.shop->inventory;
    int list_h = area.height();
    bool active = (active_side_ == Side::Merchant);

    // Adjust scroll
    if (merchant_cursor_ < merchant_scroll_) merchant_scroll_ = merchant_cursor_;
    if (merchant_cursor_ >= merchant_scroll_ + list_h) merchant_scroll_ = merchant_cursor_ - list_h + 1;

    for (int i = 0; i < list_h; ++i) {
        int idx = merchant_scroll_ + i;
        if (idx >= static_cast<int>(items.size())) break;

        const auto& item = items[idx];
        bool selected = active && (idx == merchant_cursor_);
        int price = compute_price(item, true);
        std::string price_str = std::to_string(price) + "$";

        auto vis = item_visual(item.item_def_id);

        // Build name with optional stack count
        std::string name = item.name;
        if (item.stackable && item.stack_count > 1)
            name += " x" + std::to_string(item.stack_count);

        // Pad between name and price
        int prefix_len = 3; // "> G " or "  G "
        int used = prefix_len + static_cast<int>(name.size());
        int price_len = static_cast<int>(price_str.size());
        int gap = list_w - used - price_len - 1; // 1 for trailing margin
        std::string padding;
        if (gap > 0) padding.assign(gap, ' ');

        std::vector<TextSegment> segs;
        // Cursor prefix
        segs.push_back({selected ? "> " : "  ",
                         selected ? UITag::OptionSelected : UITag::TextDefault});
        // Glyph
        segs.push_back({std::string(1, vis.glyph), rarity_tag(item.rarity),
                         EntityRef{EntityRef::Kind::Item, item.item_def_id}});
        segs.push_back({" ", UITag::TextDefault});
        // Name
        segs.push_back({name, selected ? UITag::OptionSelected : rarity_tag(item.rarity)});
        // Padding + price
        if (gap > 0) {
            segs.push_back({padding + price_str, UITag::TextWarning});
        }

        area.styled_text({.x = 0, .y = i, .segments = std::move(segs)});
    }
}

void TradeWindow::draw_sell_items(UIContext& area, int list_w) {
    // Build combined sell list
    auto& inv = player_->inventory.items;
    auto& cargo = player_->ship.cargo;
    int inv_count = static_cast<int>(inv.size());
    int list_h = area.height();
    bool active = (active_side_ == Side::Player);

    // Total items
    int total = inv_count + static_cast<int>(cargo.size());

    // Adjust scroll
    if (player_cursor_ < player_scroll_) player_scroll_ = player_cursor_;
    if (player_cursor_ >= player_scroll_ + list_h) player_scroll_ = player_cursor_ - list_h + 1;

    for (int i = 0; i < list_h; ++i) {
        int idx = player_scroll_ + i;
        if (idx >= total) break;

        // Cargo separator label
        if (idx == inv_count && inv_count > 0) {
            area.text({.x = 1, .y = i, .content = "-- Ship Cargo --", .tag = UITag::TextDim});
            continue;
        }

        bool from_cargo = (idx >= inv_count);
        const Item& item = from_cargo ? cargo[idx - inv_count] : inv[idx];
        bool selected = active && (idx == player_cursor_);
        int price = compute_price(item, false);
        std::string price_str = std::to_string(price) + "$";

        auto vis = item_visual(item.item_def_id);

        std::string name = item.name;
        if (item.stackable && item.stack_count > 1)
            name += " x" + std::to_string(item.stack_count);

        // Tag cargo items
        if (from_cargo) name += " [cargo]";

        int prefix_len = 3;
        int used = prefix_len + static_cast<int>(name.size());
        int price_len = static_cast<int>(price_str.size());
        int gap = list_w - used - price_len - 1;
        std::string padding;
        if (gap > 0) padding.assign(gap, ' ');

        std::vector<TextSegment> segs;
        segs.push_back({selected ? "> " : "  ",
                         selected ? UITag::OptionSelected : UITag::TextDefault});
        segs.push_back({std::string(1, vis.glyph), rarity_tag(item.rarity),
                         EntityRef{EntityRef::Kind::Item, item.item_def_id}});
        segs.push_back({" ", UITag::TextDefault});
        segs.push_back({name, selected ? UITag::OptionSelected : rarity_tag(item.rarity)});
        if (gap > 0) {
            segs.push_back({padding + price_str, UITag::TextWarning});
        }

        area.styled_text({.x = 0, .y = i, .segments = std::move(segs)});
    }
}

} // namespace astra
