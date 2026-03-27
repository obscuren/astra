#pragma once

#include "astra/player.h"
#include "astra/renderer.h"
#include "astra/ui.h"

#include <optional>

namespace astra {

class RepairBench {
public:
    RepairBench() = default;

    bool is_open() const { return open_; }
    void open(Player* player, Renderer* renderer);
    void close();

    bool handle_input(int key);
    void draw(int screen_w, int screen_h);

private:
    bool open_ = false;
    Player* player_ = nullptr;
    Renderer* renderer_ = nullptr;

    int cursor_ = 0;           // inventory cursor for item selection
    int bench_item_ = -1;      // inventory index of item on bench, -1 = empty
    bool picking_ = false;     // true = showing item picker popup
    PopupMenu picker_;

    int repair_cost() const;
    std::vector<int> damaged_items() const;
};

} // namespace astra
