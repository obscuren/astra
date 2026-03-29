#include "astra/trade_window.h"
#include "astra/character.h"
#include "astra/effect.h"
#include "astra/player.h"

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
    int effect_mod = effect_buy_price_pct(player_->effects);
    int faction_mod = reputation_price_pct(reputation_for(*player_, merchant_->faction));
    int cost = item.buy_value + (item.buy_value * (effect_mod + faction_mod) / 100);
    if (cost < 1) cost = 1;

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

    int effect_mod = effect_sell_price_pct(player_->effects);
    int faction_mod = -reputation_price_pct(reputation_for(*player_, merchant_->faction));
    int price = item.sell_value + (item.sell_value * (effect_mod + faction_mod) / 100);
    if (price < 0) price = 0;

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
    // Build combined player + cargo view for the sell side
    std::vector<std::pair<const Item*, bool>> sell_items; // {item, is_cargo}
    for (const auto& it : player_->inventory.items) sell_items.push_back({&it, false});
    for (const auto& it : player_->ship.cargo) sell_items.push_back({&it, true});
    draw_sell_list(right_list, sell_items,
                   player_cursor_, player_scroll_, !merchant_active);

    // Bottom separator
    int info_y = 2 + list_h;
    left_ctx.hline(info_y, BoxDraw::H, Color::DarkGray);
    right_ctx.hline(info_y, BoxDraw::H, Color::DarkGray);

    // Info lines
    int credits_y = info_y + 1;
    std::string credits_str = "Credits: " + std::to_string(player_->money) + "$";
    left_ctx.text_center(credits_y, credits_str, Color::Yellow);

    // Faction standing indicator
    int faction_pct = reputation_price_pct(reputation_for(*player_, merchant_->faction));
    if (faction_pct != 0) {
        std::string faction_str = merchant_->faction + ": ";
        if (faction_pct > 0) faction_str += "+" + std::to_string(faction_pct) + "% markup";
        else faction_str += std::to_string(faction_pct) + "% discount";
        Color fc = faction_pct > 0 ? Color::Red : Color::Green;
        left_ctx.text_center(credits_y + 1, faction_str, fc);
    }

    int total_w = player_->inventory.total_weight();
    int max_w = player_->inventory.max_carry_weight;
    std::string weight_str = "Weight: " + std::to_string(total_w) + "/" + std::to_string(max_w);
    right_ctx.text_center(credits_y, weight_str, Color::Cyan);

    // Status message
    int status_y = credits_y + (faction_pct != 0 ? 2 : 1);
    if (status_timer_ > 0 && !status_msg_.empty()) {
        ctx.text_center(status_y, status_msg_, Color::Green);
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

        // Name (rarity color) + stack count (white)
        draw_item_name(ctx, x, i, item, selected);

        // Price right-aligned (with Haggle + faction modifier)
        int base_price = show_buy_price ? item.buy_value : item.sell_value;
        int effect_pct = show_buy_price ? effect_buy_price_pct(player_->effects)
                                        : effect_sell_price_pct(player_->effects);
        int faction_pct = reputation_price_pct(reputation_for(*player_, merchant_->faction));
        if (!show_buy_price) faction_pct = -faction_pct;
        int price = base_price + (base_price * (effect_pct + faction_pct) / 100);
        if (price < (show_buy_price ? 1 : 0)) price = show_buy_price ? 1 : 0;
        std::string price_str = std::to_string(price) + "$";
        int px = w - static_cast<int>(price_str.size()) - 1;
        if (px > x + static_cast<int>(item.name.size()) + 2) {
            ctx.text(px, i, price_str, Color::Yellow);
        }
    }
}

void TradeWindow::draw_sell_list(DrawContext& ctx,
                                 const std::vector<std::pair<const Item*, bool>>& items,
                                 int cursor, int scroll, bool active) {
    int w = ctx.width();
    int h = ctx.height();
    int inv_count = static_cast<int>(player_->inventory.items.size());

    for (int i = 0; i < h; ++i) {
        int idx = scroll + i;
        if (idx >= static_cast<int>(items.size())) break;

        const auto& [item_ptr, is_cargo] = items[idx];
        const auto& item = *item_ptr;
        bool selected = active && (idx == cursor);
        int x = 1;

        // Separator between player inventory and cargo
        if (idx == inv_count && inv_count > 0 && idx > scroll) {
            ctx.text(1, i, "-- Ship Cargo --", Color::DarkGray);
            continue;  // skip this row for items — but this shifts indices...
        }

        if (selected) ctx.put(0, i, '>', Color::Yellow);
        ctx.put(x, i, item.glyph, rarity_color(item.rarity));
        x += 2;

        draw_item_name(ctx, x, i, item, selected);

        // Sell price
        int effect_pct = effect_sell_price_pct(player_->effects);
        int faction_pct = -reputation_price_pct(reputation_for(*player_, merchant_->faction));
        int price = item.sell_value + (item.sell_value * (effect_pct + faction_pct) / 100);
        if (price < 0) price = 0;
        std::string price_str = std::to_string(price) + "$";
        int px = w - static_cast<int>(price_str.size()) - 1;
        if (px > x + static_cast<int>(item.name.size()) + 2) {
            ctx.text(px, i, price_str, Color::Yellow);
        }
    }
}

} // namespace astra
