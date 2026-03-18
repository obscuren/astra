#include "astra/map_generator.h"

#include <algorithm>

namespace astra {

class StationGenerator : public MapGenerator {
protected:
    void generate_layout(std::mt19937& rng) override;
    void connect_rooms(std::mt19937& rng) override;
};

void StationGenerator::generate_layout(std::mt19937& rng) {
    int max_rooms = props_->room_count_max;
    int min_size = 4;
    int max_size = std::min(map_->width() / 3, map_->height() / 3);
    if (max_size < min_size + 1) max_size = min_size + 1;

    std::uniform_int_distribution<int> size_dist(min_size, max_size);
    std::uniform_int_distribution<int> lit_chance(0, 99);

    for (int i = 0; i < max_rooms * 4 && static_cast<int>(rooms_.size()) < max_rooms; ++i) {
        int w = size_dist(rng);
        int h = size_dist(rng);
        int total_w = w + 2;
        int total_h = h + 2;
        if (total_w >= map_->width() || total_h >= map_->height()) continue;

        std::uniform_int_distribution<int> x_dist(0, map_->width() - total_w);
        std::uniform_int_distribution<int> y_dist(0, map_->height() - total_h);
        int x = x_dist(rng);
        int y = y_dist(rng);

        RoomRect candidate{x, y, x + total_w - 1, y + total_h - 1};

        // Check overlap with existing rooms (with 1-tile gap)
        bool overlaps = false;
        for (const auto& r : rooms_) {
            if (candidate.x1 - 1 <= r.x2 && candidate.x2 + 1 >= r.x1 &&
                candidate.y1 - 1 <= r.y2 && candidate.y2 + 1 >= r.y1) {
                overlaps = true;
                break;
            }
        }
        if (overlaps) continue;

        // Create region for this room
        Region reg;
        reg.type = RegionType::Room;
        reg.lit = lit_chance(rng) < props_->light_bias;
        int rid = map_->add_region(reg);

        carve_rect(candidate.x1, candidate.y1, candidate.x2, candidate.y2, rid);
        rooms_.push_back(candidate);
    }

    // Ensure at least one room exists
    if (rooms_.empty()) {
        Region reg;
        reg.type = RegionType::Room;
        reg.lit = true;
        int rid = map_->add_region(reg);

        int x1 = 1, y1 = 1;
        int x2 = map_->width() - 2, y2 = map_->height() - 2;
        carve_rect(x1, y1, x2, y2, rid);
        rooms_.push_back({x1, y1, x2, y2});
    }
}

void StationGenerator::connect_rooms(std::mt19937& rng) {
    for (size_t i = 1; i < rooms_.size(); ++i) {
        int cx1 = (rooms_[i - 1].x1 + rooms_[i - 1].x2) / 2;
        int cy1 = (rooms_[i - 1].y1 + rooms_[i - 1].y2) / 2;
        int cx2 = (rooms_[i].x1 + rooms_[i].x2) / 2;
        int cy2 = (rooms_[i].y1 + rooms_[i].y2) / 2;

        // Create corridor region (unlit by default)
        Region creg;
        creg.type = RegionType::Corridor;
        creg.lit = false;
        int crid = map_->add_region(creg);

        if (rng() % 2 == 0) {
            carve_corridor_h(cx1, cx2, cy1, crid);
            carve_corridor_v(cy1, cy2, cx2, crid);
        } else {
            carve_corridor_v(cy1, cy2, cx1, crid);
            carve_corridor_h(cx1, cx2, cy2, crid);
        }
    }
}

std::unique_ptr<MapGenerator> make_station_generator() {
    return std::make_unique<StationGenerator>();
}

} // namespace astra
