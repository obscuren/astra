#include "astra/repair_bench.h"

namespace astra {

void RepairBench::open(Player* player, Renderer* renderer) {
    player_ = player;
    renderer_ = renderer;
    open_ = true;
    bench_item_ = -1;
    picking_ = false;
    cursor_ = 0;
}

void RepairBench::close() {
    open_ = false;
    player_ = nullptr;
    renderer_ = nullptr;
    bench_item_ = -1;
    picking_ = false;
}

std::vector<int> RepairBench::damaged_items() const {
    std::vector<int> result;
    if (!player_) return result;
    for (int i = 0; i < static_cast<int>(player_->inventory.items.size()); ++i) {
        const auto& item = player_->inventory.items[i];
        if (item.max_durability > 0 && item.durability < item.max_durability) {
            result.push_back(i);
        }
    }
    return result;
}

int RepairBench::repair_cost() const {
    if (!player_ || bench_item_ < 0) return 0;
    if (bench_item_ >= static_cast<int>(player_->inventory.items.size())) return 0;
    const auto& item = player_->inventory.items[bench_item_];
    int missing = item.max_durability - item.durability;
    return std::max(1, missing * 2); // 2 credits per durability point
}

bool RepairBench::handle_input(int key) {
    if (!open_) return false;

    // Item picker popup active
    if (picker_.is_open()) {
        auto result = picker_.handle_input(key);
        if (result == MenuResult::Selected) {
            auto damaged = damaged_items();
            int sel = picker_.selected();
            if (sel >= 0 && sel < static_cast<int>(damaged.size())) {
                bench_item_ = damaged[sel];
            }
            picker_.close();
        } else if (result == MenuResult::Closed) {
            picker_.close();
        }
        return true;
    }

    switch (key) {
        case 27: // Esc
            close();
            return true;
        case ' ': { // Place/swap item on bench
            auto damaged = damaged_items();
            if (damaged.empty()) return true;
            picker_.close();
            picker_.set_title("Select Item to Repair");
            char hk = '1';
            for (int idx : damaged) {
                const auto& item = player_->inventory.items[idx];
                std::string label = item.name + " (" +
                    std::to_string(item.durability) + "/" +
                    std::to_string(item.max_durability) + ")";
                picker_.add_option(hk++, label);
                if (hk > '9') hk = 'a';
            }
            picker_.set_max_width_frac(0.4f);
            picker_.open();
            return true;
        }
        case 'r': { // Repair
            if (bench_item_ < 0) return true;
            int cost = repair_cost();
            if (player_->money < cost) return true;
            player_->money -= cost;
            auto& item = player_->inventory.items[bench_item_];
            item.durability = item.max_durability;
            bench_item_ = -1; // clear bench after repair
            return true;
        }
        case 'x': // Clear bench
            bench_item_ = -1;
            return true;
    }
    return true;
}

void RepairBench::draw(int screen_w, int screen_h) {
    if (!open_ || !renderer_) return;

    int margin = 4;
    Rect bounds{margin, margin, screen_w - margin * 2, screen_h - margin * 2};
    Panel panel(renderer_, bounds, "Repair Bench");
    panel.set_footer("[Space] Place item  [r] Repair  [x] Clear  [Esc] Close");
    panel.draw();
    DrawContext ctx = panel.content();

    int w = ctx.width();
    int half = w / 2;

    // Workbench slot (centered in left half)
    int wb_w = 30;
    int wb_x = (half - wb_w) / 2;
    int wb_y = 2;
    Color wb_border = Color::Yellow;

    ctx.put(wb_x, wb_y, BoxDraw::TL, wb_border);
    for (int i = 1; i < wb_w - 1; ++i) ctx.put(wb_x + i, wb_y, BoxDraw::H, wb_border);
    ctx.put(wb_x + wb_w - 1, wb_y, BoxDraw::TR, wb_border);

    ctx.put(wb_x, wb_y + 1, BoxDraw::V, wb_border);
    for (int i = 1; i < wb_w - 1; ++i) ctx.put(wb_x + i, wb_y + 1, ' ');
    ctx.put(wb_x + wb_w - 1, wb_y + 1, BoxDraw::V, wb_border);

    ctx.put(wb_x, wb_y + 2, BoxDraw::BL, wb_border);
    for (int i = 1; i < wb_w - 1; ++i) ctx.put(wb_x + i, wb_y + 2, BoxDraw::H, wb_border);
    ctx.put(wb_x + wb_w - 1, wb_y + 2, BoxDraw::BR, wb_border);

    if (bench_item_ >= 0 && bench_item_ < static_cast<int>(player_->inventory.items.size())) {
        const auto& item = player_->inventory.items[bench_item_];
        std::string display = std::string(1, item.glyph) + " " + item.name;
        if (static_cast<int>(display.size()) > wb_w - 4)
            display = display.substr(0, wb_w - 4);
        int nx = wb_x + (wb_w - static_cast<int>(display.size())) / 2;
        ctx.put(nx, wb_y + 1, item.glyph, rarity_color(item.rarity));
        ctx.text(nx + 2, wb_y + 1, item.name, rarity_color(item.rarity));
    } else {
        std::string msg = "[Space] to place item";
        int mx = wb_x + (wb_w - static_cast<int>(msg.size())) / 2;
        ctx.text(mx, wb_y + 1, msg, Color::DarkGray);
    }

    // Right side: item details
    int rx = half + 2;
    int ry = 2;

    if (bench_item_ >= 0 && bench_item_ < static_cast<int>(player_->inventory.items.size())) {
        const auto& item = player_->inventory.items[bench_item_];
        int cost = repair_cost();

        ctx.text(rx, ry, item.name, rarity_color(item.rarity));
        ry += 2;

        // Durability bar
        ctx.text(rx, ry, "Durability:", Color::DarkGray);
        ry++;
        int bar_w = half - 6;
        Color bar_color = Color::Green;
        int pct = item.max_durability > 0 ? (100 * item.durability / item.max_durability) : 0;
        if (pct < 30) bar_color = Color::Red;
        else if (pct < 60) bar_color = Color::Yellow;
        ctx.bar(rx, ry, bar_w, item.durability, item.max_durability, bar_color);
        ry++;
        ctx.text(rx, ry, std::to_string(item.durability) + " / " +
                 std::to_string(item.max_durability), Color::White);
        ry += 2;

        // Cost
        Color cost_color = (player_->money >= cost) ? Color::Green : Color::Red;
        ctx.text(rx, ry, "Repair cost: " + std::to_string(cost) + "$", cost_color);
        ry++;
        ctx.text(rx, ry, "Your credits: " + std::to_string(player_->money) + "$", Color::DarkGray);
        ry += 2;

        if (player_->money >= cost) {
            ctx.text(rx, ry, "[r] Repair to full", Color::Yellow);
        } else {
            ctx.text(rx, ry, "Not enough credits.", Color::Red);
        }
    } else {
        auto damaged = damaged_items();
        if (damaged.empty()) {
            ctx.text(rx, ry, "No items need repair.", Color::DarkGray);
        } else {
            ctx.text(rx, ry, std::to_string(damaged.size()) + " item(s) need repair.", Color::DarkGray);
            ry++;
            ctx.text(rx, ry, "Press [Space] to place one on the bench.", Color::DarkGray);
        }
    }

    // Draw item picker popup on top
    picker_.draw(renderer_, screen_w, screen_h);
}

} // namespace astra
