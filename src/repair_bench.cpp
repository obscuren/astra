#include "astra/repair_bench.h"
#include "terminal_theme.h"

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

    // Outer panel
    UIContext full(renderer_, bounds);
    auto content = full.panel({
        .title = "Repair Bench",
        .footer = "[Space] Place item  [r] Repair  [x] Clear  [Esc] Close",
    });

    // Split into left (workbench) and right (details) columns
    auto cols = content.columns({fill(), fill()});
    auto& left = cols[0];
    auto& right = cols[1];

    // --- Left half: workbench slot ---
    int wb_w = 30;
    int wb_x = (left.width() - wb_w) / 2;
    UIContext wb_area = left.sub(Rect{wb_x, 2, wb_w, 3});
    auto wb_content = wb_area.panel({});

    if (bench_item_ >= 0 && bench_item_ < static_cast<int>(player_->inventory.items.size())) {
        const auto& item = player_->inventory.items[bench_item_];
        auto bench_vis = item_visual(item.item_def_id);

        // Glyph + name in rarity color, centered
        std::string display = std::string(1, bench_vis.glyph) + " " + item.name;
        int max_w = wb_content.width();
        if (static_cast<int>(display.size()) > max_w)
            display = display.substr(0, max_w);
        int cx = (max_w - static_cast<int>(display.size())) / 2;

        wb_content.styled_text({.x = cx, .y = 0, .segments = {
            {std::string(1, bench_vis.glyph), rarity_tag(item.rarity),
             EntityRef{EntityRef::Kind::Item, item.item_def_id}},
            {" " + item.name, rarity_tag(item.rarity)},
        }});
    } else {
        std::string msg = "[Space] to place item";
        int cx = (wb_content.width() - static_cast<int>(msg.size())) / 2;
        if (cx < 0) cx = 0;
        wb_content.text({.x = cx, .y = 0,
            .content = msg,
            .tag = UITag::TextDim});
    }

    // --- Right half: item details ---
    int ry = 2;

    if (bench_item_ >= 0 && bench_item_ < static_cast<int>(player_->inventory.items.size())) {
        const auto& item = player_->inventory.items[bench_item_];
        int cost = repair_cost();

        // Item name
        right.text({.x = 0, .y = ry,
            .content = item.name,
            .tag = rarity_tag(item.rarity)});
        ry += 2;

        // Durability label
        right.text({.x = 0, .y = ry,
            .content = "Durability:",
            .tag = UITag::TextDim});
        ry++;

        // Durability bar
        int bar_w = right.width() - 4;
        right.progress_bar({.x = 0, .y = ry, .width = bar_w,
            .value = item.durability, .max = item.max_durability,
            .tag = UITag::DurabilityBar});
        ry++;

        // Durability fraction
        right.text({.x = 0, .y = ry,
            .content = std::to_string(item.durability) + " / " +
                       std::to_string(item.max_durability),
            .tag = UITag::TextBright});
        ry += 2;

        // Cost
        bool can_afford = player_->money >= cost;
        right.label_value({.x = 0, .y = ry,
            .label = "Repair cost: ",
            .label_tag = UITag::TextDim,
            .value = std::to_string(cost) + "$",
            .value_tag = can_afford ? UITag::TextSuccess : UITag::TextDanger});
        ry++;

        right.label_value({.x = 0, .y = ry,
            .label = "Your credits: ",
            .label_tag = UITag::TextDim,
            .value = std::to_string(player_->money) + "$",
            .value_tag = UITag::TextDim});
        ry += 2;

        if (can_afford) {
            right.text({.x = 0, .y = ry,
                .content = "[r] Repair to full",
                .tag = UITag::TextWarning});
        } else {
            right.text({.x = 0, .y = ry,
                .content = "Not enough credits.",
                .tag = UITag::TextDanger});
        }
    } else {
        auto damaged = damaged_items();
        if (damaged.empty()) {
            right.text({.x = 0, .y = ry,
                .content = "No items need repair.",
                .tag = UITag::TextDim});
        } else {
            right.text({.x = 0, .y = ry,
                .content = std::to_string(damaged.size()) + " item(s) need repair.",
                .tag = UITag::TextDim});
            ry++;
            right.text({.x = 0, .y = ry,
                .content = "Press [Space] to place one on the bench.",
                .tag = UITag::TextDim});
        }
    }

    // Draw item picker popup on top (stays on old API)
    picker_.draw(renderer_, screen_w, screen_h);
}

} // namespace astra
