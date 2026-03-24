#include "astra/trade_window.h"

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
    int cost = item.buy_value;

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
    auto& inv = player_->inventory.items;
    if (inv.empty()) return;
    if (player_cursor_ < 0 || player_cursor_ >= static_cast<int>(inv.size())) return;

    auto& item = inv[player_cursor_];
    int price = item.sell_value;

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

    // Remove from player
    if (item.stackable && item.stack_count > 1) {
        item.stack_count -= 1;
    } else {
        inv.erase(inv.begin() + player_cursor_);
        if (player_cursor_ >= static_cast<int>(inv.size()) && player_cursor_ > 0) {
            --player_cursor_;
        }
    }

    status_msg_ = "Sold " + sold_name + " for " + std::to_string(price) + "$.";
    status_timer_ = 3;
}

void TradeWindow::draw(int screen_w, int screen_h) {
    if (!open_ || !renderer_) return;

    Window win(renderer_, Rect{0, 0, screen_w, screen_h}, "Trade");
    win.set_footer("ESC Close  TAB Switch  \xe2\x86\x91\xe2\x86\x93 Move  SPACE Buy/Sell");
    win.draw();

    DrawContext ctx = win.content();
    int w = ctx.width();
    int h = ctx.height();
    int half = w / 2;

    // Vertical divider
    DrawContext left_ctx = ctx.sub(Rect{0, 0, half - 1, h});
    DrawContext right_ctx = ctx.sub(Rect{half, 0, w - half, h});

    // Draw divider line
    for (int y = 0; y < h; ++y) {
        ctx.put(half - 1, y, BoxDraw::V, Color::DarkGray);
    }

    // Headers
    bool merchant_active = (active_side_ == Side::Merchant);
    std::string merchant_title = merchant_->interactions.shop->shop_name;
    std::string player_title = "Your Inventory";

    left_ctx.text_center(0, merchant_title,
        merchant_active ? Color::White : Color::DarkGray);
    right_ctx.text_center(0, player_title,
        !merchant_active ? Color::White : Color::DarkGray);

    // Separators
    left_ctx.hline(1, BoxDraw::H, Color::DarkGray);
    right_ctx.hline(1, BoxDraw::H, Color::DarkGray);

    // Item list area
    int list_h = h - 5; // header(1) + sep(1) + items + sep(1) + info(1) + status(1)
    if (list_h < 1) list_h = 1;

    // Adjust scroll for merchant side
    if (merchant_cursor_ < merchant_scroll_) merchant_scroll_ = merchant_cursor_;
    if (merchant_cursor_ >= merchant_scroll_ + list_h) merchant_scroll_ = merchant_cursor_ - list_h + 1;

    // Adjust scroll for player side
    if (player_cursor_ < player_scroll_) player_scroll_ = player_cursor_;
    if (player_cursor_ >= player_scroll_ + list_h) player_scroll_ = player_cursor_ - list_h + 1;

    DrawContext left_list = left_ctx.sub(Rect{0, 2, left_ctx.width(), list_h});
    DrawContext right_list = right_ctx.sub(Rect{0, 2, right_ctx.width(), list_h});

    draw_item_list(left_list, merchant_->interactions.shop->inventory,
                   merchant_cursor_, merchant_scroll_, merchant_active, true);
    draw_item_list(right_list, player_->inventory.items,
                   player_cursor_, player_scroll_, !merchant_active, false);

    // Bottom separator
    int info_y = 2 + list_h;
    left_ctx.hline(info_y, BoxDraw::H, Color::DarkGray);
    right_ctx.hline(info_y, BoxDraw::H, Color::DarkGray);

    // Info lines
    int credits_y = info_y + 1;
    std::string credits_str = "Credits: " + std::to_string(player_->money) + "$";
    left_ctx.text_center(credits_y, credits_str, Color::Yellow);

    int total_w = player_->inventory.total_weight();
    int max_w = player_->inventory.max_carry_weight;
    std::string weight_str = "Weight: " + std::to_string(total_w) + "/" + std::to_string(max_w);
    right_ctx.text_center(credits_y, weight_str, Color::Cyan);

    // Status message
    if (status_timer_ > 0 && !status_msg_.empty()) {
        ctx.text_center(credits_y + 1, status_msg_, Color::Green);
    }
}

void TradeWindow::draw_item_list(DrawContext& ctx, const std::vector<Item>& items,
                                  int cursor, int scroll, bool active, bool show_buy_price) {
    int w = ctx.width();
    int h = ctx.height();

    for (int i = 0; i < h; ++i) {
        int idx = scroll + i;
        if (idx >= static_cast<int>(items.size())) break;

        const auto& item = items[idx];
        bool selected = active && (idx == cursor);
        int x = 1;

        // Cursor
        if (selected) {
            ctx.put(0, i, '>', Color::Yellow);
        }

        // Glyph in rarity color
        ctx.put(x, i, item.glyph, rarity_color(item.rarity));
        x += 2;

        // Name
        std::string name = item.name;
        if (item.stackable && item.stack_count > 1) {
            name += " x" + std::to_string(item.stack_count);
        }
        Color name_color = selected ? Color::White : rarity_color(item.rarity);
        int max_name_len = w - x - 6; // leave room for price
        if (static_cast<int>(name.size()) > max_name_len && max_name_len > 0) {
            name = name.substr(0, max_name_len);
        }
        ctx.text(x, i, name, name_color);

        // Price right-aligned
        int price = show_buy_price ? item.buy_value : item.sell_value;
        std::string price_str = std::to_string(price) + "$";
        int px = w - static_cast<int>(price_str.size()) - 1;
        if (px > x + static_cast<int>(name.size())) {
            ctx.text(px, i, price_str, Color::Yellow);
        }
    }
}

} // namespace astra
