#pragma once

#include "astra/npc.h"
#include "astra/player.h"
#include "astra/ui.h"

#include <string>
#include <utility>
#include <vector>

namespace astra {

class TradeWindow {
public:
    TradeWindow() = default;

    bool is_open() const;
    void open(Npc* merchant, Player* player, Renderer* renderer);
    void close();
    bool handle_input(int key);
    void draw(int screen_w, int screen_h);
    bool has_message() const;
    std::string consume_message();

private:
    enum class Side { Merchant, Player };

    void buy_selected();
    void sell_selected();
    void draw_buy_items(UIContext& area, int list_w);
    void draw_sell_items(UIContext& area, int list_w);
    int compute_price(const Item& item, bool buy) const;

    Npc* merchant_ = nullptr;
    Player* player_ = nullptr;
    Renderer* renderer_ = nullptr;
    bool open_ = false;
    Side active_side_ = Side::Merchant;
    int merchant_cursor_ = 0;
    int player_cursor_ = 0;
    int merchant_scroll_ = 0;
    int player_scroll_ = 0;
    std::string status_msg_;
    int status_timer_ = 0;
};

} // namespace astra
