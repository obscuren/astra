#include "astra/map_generator.h"
#include "astra/station_type.h"

#include <algorithm>

namespace astra {

// =========================================================================
// Doorway-safe fixture placement (same pattern as hub_station_generator)
// =========================================================================

struct DerelictRoomContext {
    int ix1, iy1, ix2, iy2;
    int wx1, wy1, wx2, wy2;
    std::vector<bool> blocked;
    int iw, ih;
    TileMap* map;

    DerelictRoomContext(TileMap& m, const MapGenerator::RoomRect& r)
        : ix1(r.x1 + 1), iy1(r.y1 + 1), ix2(r.x2 - 1), iy2(r.y2 - 1),
          wx1(r.x1), wy1(r.y1), wx2(r.x2), wy2(r.y2),
          iw(r.x2 - r.x1 - 1), ih(r.y2 - r.y1 - 1), map(&m) {
        if (iw <= 0 || ih <= 0) { iw = ih = 0; return; }
        blocked.resize(iw * ih, false);

        auto mark_door_zone = [&](int door_ix, int door_iy) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int bx = door_ix + dx;
                    int by = door_iy + dy;
                    if (bx >= 0 && bx < iw && by >= 0 && by < ih) {
                        blocked[by * iw + bx] = true;
                    }
                }
            }
        };

        for (int x = r.x1; x <= r.x2; ++x) {
            if (m.get(x, r.y1) == Tile::Floor) mark_door_zone(x - ix1, 0);
        }
        for (int x = r.x1; x <= r.x2; ++x) {
            if (m.get(x, r.y2) == Tile::Floor) mark_door_zone(x - ix1, ih - 1);
        }
        for (int y = r.y1; y <= r.y2; ++y) {
            if (m.get(r.x1, y) == Tile::Floor) mark_door_zone(0, y - iy1);
        }
        for (int y = r.y1; y <= r.y2; ++y) {
            if (m.get(r.x2, y) == Tile::Floor) mark_door_zone(iw - 1, y - iy1);
        }
    }

    bool is_door_zone(int x, int y) const {
        int lx = x - ix1, ly = y - iy1;
        if (lx < 0 || lx >= iw || ly < 0 || ly >= ih) return true;
        return blocked[ly * iw + lx];
    }

    bool place(int x, int y, FixtureData fd) {
        if (x < ix1 || x > ix2 || y < iy1 || y > iy2) return false;
        if (map->get(x, y) != Tile::Floor) return false;
        if (!fd.passable && is_door_zone(x, y)) return false;
        map->add_fixture(x, y, fd);
        return true;
    }

    bool too_small() const { return iw < 3 || ih < 3; }
};

// =========================================================================
// Derelict Station Generator
// =========================================================================

class DerelictStationGenerator : public MapGenerator {
public:
    // When constructed with a StationContext, scatter extra loot containers
    // seeded from ctx.keeper_seed (Abandoned-station variant only).
    explicit DerelictStationGenerator(bool place_loot = false, uint64_t keeper_seed = 0)
        : has_station_context_(place_loot), keeper_seed_(keeper_seed) {}

protected:
    void generate_layout(std::mt19937& rng) override;
    void connect_rooms(std::mt19937& rng) override;
    void place_features(std::mt19937& rng) override;
    void assign_regions(std::mt19937& rng) override;

private:
    bool     has_station_context_ = false;
    uint64_t keeper_seed_         = 0;
};

void DerelictStationGenerator::generate_layout(std::mt19937& rng) {
    int max_rooms = props_->room_count_max;
    int min_size = 4;
    int max_size = std::min(map_->width() / 3, map_->height() / 3);
    if (max_size < min_size + 1) max_size = min_size + 1;

    std::uniform_int_distribution<int> size_dist(min_size, max_size);

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

        bool overlaps = false;
        for (const auto& r : rooms_) {
            if (candidate.x1 - 1 <= r.x2 && candidate.x2 + 1 >= r.x1 &&
                candidate.y1 - 1 <= r.y2 && candidate.y2 + 1 >= r.y1) {
                overlaps = true;
                break;
            }
        }
        if (overlaps) continue;

        Region reg;
        reg.type = RegionType::Room;
        reg.lit = false; // all dark
        int rid = map_->add_region(reg);

        carve_rect(candidate.x1, candidate.y1, candidate.x2, candidate.y2, rid);
        rooms_.push_back(candidate);
    }

    // Ensure at least one room
    if (rooms_.empty()) {
        Region reg;
        reg.type = RegionType::Room;
        reg.lit = false;
        int rid = map_->add_region(reg);

        int x1 = 1, y1 = 1;
        int x2 = map_->width() - 2, y2 = map_->height() - 2;
        carve_rect(x1, y1, x2, y2, rid);
        rooms_.push_back({x1, y1, x2, y2});
    }
}

void DerelictStationGenerator::connect_rooms(std::mt19937& rng) {
    for (size_t i = 1; i < rooms_.size(); ++i) {
        int cx1 = (rooms_[i - 1].x1 + rooms_[i - 1].x2) / 2;
        int cy1 = (rooms_[i - 1].y1 + rooms_[i - 1].y2) / 2;
        int cx2 = (rooms_[i].x1 + rooms_[i].x2) / 2;
        int cy2 = (rooms_[i].y1 + rooms_[i].y2) / 2;

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

void DerelictStationGenerator::place_features(std::mt19937& rng) {
    std::uniform_int_distribution<int> percent(0, 99);

    for (size_t ri = 0; ri < rooms_.size(); ++ri) {
        DerelictRoomContext ctx(*map_, rooms_[ri]);
        if (ctx.too_small()) continue;

        // Scatter debris across 30-50% of floor tiles
        int debris_pct = std::uniform_int_distribution<int>(30, 50)(rng);
        for (int y = ctx.iy1; y <= ctx.iy2; ++y) {
            for (int x = ctx.ix1; x <= ctx.ix2; ++x) {
                if (percent(rng) < debris_pct) {
                    ctx.place(x, y, make_fixture(FixtureType::Debris));
                }
            }
        }

        // Broken consoles (1-2 per room)
        int console_count = std::uniform_int_distribution<int>(1, 2)(rng);
        for (int c = 0; c < console_count; ++c) {
            int cx = std::uniform_int_distribution<int>(ctx.ix1, ctx.ix2)(rng);
            int cy = std::uniform_int_distribution<int>(ctx.iy1, ctx.iy2)(rng);
            ctx.place(cx, cy, make_fixture(FixtureType::Console));
        }

        // Toppled racks and crates in some rooms
        if (percent(rng) < 40) {
            int count = std::uniform_int_distribution<int>(1, 3)(rng);
            for (int c = 0; c < count; ++c) {
                int fx = std::uniform_int_distribution<int>(ctx.ix1, ctx.ix2)(rng);
                int fy = std::uniform_int_distribution<int>(ctx.iy1, ctx.iy2)(rng);
                if (percent(rng) < 50) {
                    ctx.place(fx, fy, make_fixture(FixtureType::Rack));
                } else {
                    ctx.place(fx, fy, make_fixture(FixtureType::Crate));
                }
            }
        }
    }

    // Abandoned-station variant: scatter 3-6 additional lootable crates
    // seeded from keeper_seed_ so placement is deterministic per-station.
    if (has_station_context_ && !rooms_.empty()) {
        std::mt19937 loot_rng(static_cast<uint32_t>(keeper_seed_ ^ (keeper_seed_ >> 32)));
        int crate_count = std::uniform_int_distribution<int>(3, 6)(loot_rng);
        // Spread across non-first rooms where possible
        int region_start = rooms_.size() > 1 ? 1 : 0;
        for (int c = 0; c < crate_count; ++c) {
            int ri2 = std::uniform_int_distribution<int>(
                region_start, static_cast<int>(rooms_.size()) - 1)(loot_rng);
            DerelictRoomContext dctx(*map_, rooms_[ri2]);
            if (dctx.too_small()) continue;
            for (int attempt = 0; attempt < 8; ++attempt) {
                int fx = std::uniform_int_distribution<int>(dctx.ix1, dctx.ix2)(loot_rng);
                int fy = std::uniform_int_distribution<int>(dctx.iy1, dctx.iy2)(loot_rng);
                if (dctx.place(fx, fy, make_fixture(FixtureType::Crate))) break;
            }
        }
    }
}

void DerelictStationGenerator::assign_regions(std::mt19937& rng) {
    struct FlavorInfo {
        RoomFlavor flavor;
        const char* name;
        const char* enter_message;
    };

    static const FlavorInfo derelict_flavors[] = {
        {RoomFlavor::DerelictBay, "Derelict Bay",
            "Twisted metal and shattered hull plating. The station groans."},
        {RoomFlavor::HullBreach, "Hull Breach",
            "Stars are visible through a gaping tear in the hull. Atmosphere vents into space."},
        {RoomFlavor::StorageBay, "Ruined Storage Bay",
            "Crates lie scattered and crushed. Whatever was stored here is long gone."},
        {RoomFlavor::EmptyRoom, "Dark Compartment",
            "An empty compartment. The silence is absolute."},
    };
    static constexpr int flavor_count = 4;

    static const FlavorInfo corridor_flavors[] = {
        {RoomFlavor::CorridorDamaged, "Wrecked Corridor",
            "Scorch marks and buckled panels. The corridor has seen catastrophic damage."},
        {RoomFlavor::CorridorDimLit, "Dark Passage",
            "Emergency lighting has failed. Only your lamp cuts through the void."},
    };

    std::uniform_int_distribution<int> flavor_dist(0, flavor_count - 1);
    std::uniform_int_distribution<int> corr_dist(0, 1);
    std::uniform_int_distribution<int> percent(0, 99);

    int count = map_->region_count();
    int room_index = 0;

    for (int i = 0; i < count; ++i) {
        Region reg = map_->region(i);
        reg.lit = false; // all dark

        if (reg.type == RegionType::Room) {
            // ~20% of rooms are hull breaches
            const FlavorInfo* info;
            if (percent(rng) < 20) {
                info = &derelict_flavors[1]; // HullBreach
            } else {
                int pick = flavor_dist(rng);
                // Avoid double-picking HullBreach when not intended
                if (pick == 1 && percent(rng) >= 20) pick = 0;
                info = &derelict_flavors[pick];
            }

            reg.flavor = info->flavor;
            reg.name = info->name;
            reg.enter_message = info->enter_message;

            // Hull breaches: open one wall edge to space
            if (reg.flavor == RoomFlavor::HullBreach &&
                room_index < static_cast<int>(rooms_.size())) {
                const auto& room = rooms_[room_index];
                // Open north wall to space
                for (int x = room.x1; x <= room.x2; ++x) {
                    Tile t = map_->get(x, room.y1);
                    int rid = map_->region_id(x, room.y1);
                    if (t == Tile::Wall && rid == i) {
                        map_->set(x, room.y1, Tile::Empty);
                        map_->set_region(x, room.y1, -1);
                    }
                }
            }

            ++room_index;
        } else {
            const auto& info = corridor_flavors[corr_dist(rng)];
            reg.flavor = info.flavor;
            reg.name = info.name;
            reg.enter_message = info.enter_message;
        }

        reg.features = default_features(reg.flavor);
        map_->update_region(i, reg);
    }
}

std::unique_ptr<MapGenerator> make_derelict_station_generator() {
    return std::make_unique<DerelictStationGenerator>();
}

std::unique_ptr<MapGenerator> make_derelict_station_generator(const StationContext& ctx) {
    return std::make_unique<DerelictStationGenerator>(true, ctx.keeper_seed);
}

} // namespace astra
