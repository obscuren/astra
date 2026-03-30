#include "astra/tilemap.h"

#include <algorithm>
#include <random>

namespace astra {

TileMap::TileMap(int width, int height, MapType type)
    : map_type_(type), width_(width), height_(height),
      tiles_(width * height, Tile::Empty),
      backdrop_(width * height, '\0'),
      region_ids_(width * height, -1),
      fixture_ids_(width * height, -1),
      glyph_override_(width * height, 0) {}

char TileMap::backdrop(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return '\0';
    return backdrop_[y * width_ + x];
}

Tile TileMap::get(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return Tile::Empty;
    return tiles_[y * width_ + x];
}

void TileMap::set(int x, int y, Tile t) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        tiles_[y * width_ + x] = t;
    }
}

bool TileMap::passable(int x, int y) const {
    Tile t = get(x, y);
    if (t == Tile::Fixture) {
        int fid = fixture_id(x, y);
        return fid >= 0 && fixtures_[fid].passable;
    }
    if (t == Tile::Floor || t == Tile::IndoorFloor || t == Tile::Portal || t == Tile::Water || t == Tile::Ice)
        return true;
    // Overworld tiles: all passable except mountains and lakes
    if (t == Tile::OW_Mountains || t == Tile::OW_Lake) return false;
    if (t >= Tile::OW_Plains && t <= Tile::OW_Landing) return true;
    return false;
}

bool TileMap::opaque(int x, int y) const {
    Tile t = get(x, y);
    // Fixtures are never opaque (they don't block line of sight)
    // Overworld tiles are never opaque
    if (t >= Tile::OW_Plains && t <= Tile::OW_Landing) return false;
    return t == Tile::Wall || t == Tile::StructuralWall || t == Tile::Empty;
}

int TileMap::fixture_id(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return -1;
    if (fixture_ids_.empty()) return -1;
    return fixture_ids_[y * width_ + x];
}

int TileMap::add_fixture(int x, int y, FixtureData fd) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return -1;
    // Ensure fixture_ids_ is sized
    if (fixture_ids_.empty()) {
        fixture_ids_.resize(width_ * height_, -1);
    }
    int id = static_cast<int>(fixtures_.size());
    fixtures_.push_back(std::move(fd));
    fixture_ids_[y * width_ + x] = id;
    set(x, y, Tile::Fixture);
    return id;
}

int TileMap::region_id(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return -1;
    return region_ids_[y * width_ + x];
}

void TileMap::set_region(int x, int y, int rid) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        region_ids_[y * width_ + x] = rid;
    }
}

int TileMap::add_region(Region reg) {
    int id = static_cast<int>(regions_.size());
    regions_.push_back(std::move(reg));
    return id;
}

void TileMap::update_region(int id, const Region& reg) {
    if (id >= 0 && id < static_cast<int>(regions_.size())) {
        regions_[id] = reg;
    }
}

void TileMap::set_backdrop(int x, int y, char c) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        backdrop_[y * width_ + x] = c;
    }
}

void TileMap::clear_all() {
    std::fill(tiles_.begin(), tiles_.end(), Tile::Empty);
    std::fill(backdrop_.begin(), backdrop_.end(), '\0');
    std::fill(region_ids_.begin(), region_ids_.end(), -1);
    regions_.clear();
    fixtures_.clear();
    if (!fixture_ids_.empty()) {
        std::fill(fixture_ids_.begin(), fixture_ids_.end(), -1);
    }
    if (!glyph_override_.empty()) {
        std::fill(glyph_override_.begin(), glyph_override_.end(), static_cast<uint8_t>(0));
    }
}

uint8_t TileMap::glyph_override(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return 0;
    if (glyph_override_.empty()) return 0;
    return glyph_override_[y * width_ + x];
}

void TileMap::set_glyph_override(int x, int y, uint8_t idx) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    if (glyph_override_.empty()) {
        glyph_override_.resize(width_ * height_, 0);
    }
    glyph_override_[y * width_ + x] = idx;
}

void TileMap::load_glyph_overrides(std::vector<uint8_t> overrides) {
    overrides.resize(static_cast<size_t>(width_) * height_, 0);
    glyph_override_ = std::move(overrides);
}

bool TileMap::custom_detail(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return false;
    if (custom_flags_.empty()) return false;
    return custom_flags_[y * width_ + x] != 0;
}

void TileMap::set_custom_detail(int x, int y, bool v) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    if (custom_flags_.empty()) {
        custom_flags_.resize(width_ * height_, 0);
    }
    custom_flags_[y * width_ + x] = v ? 1 : 0;
}

void TileMap::find_open_spot(int& out_x, int& out_y) const {
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            if (get(x, y) == Tile::Floor || get(x, y) == Tile::IndoorFloor) {
                out_x = x;
                out_y = y;
                return;
            }
        }
    }
    out_x = width_ / 2;
    out_y = height_ / 2;
}

bool TileMap::find_open_spot_near(int near_x, int near_y, int& out_x, int& out_y,
                                  const std::vector<std::pair<int,int>>& exclude,
                                  std::mt19937* rng) const {
    int rid = region_id(near_x, near_y);
    if (rid < 0) return false;

    // Collect all valid candidates in the region
    std::vector<std::pair<int,int>> candidates;
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            if (region_id(x, y) != rid) continue;
            { Tile t_ = get(x, y); if (t_ != Tile::Floor && t_ != Tile::IndoorFloor) continue; }

            bool excluded = false;
            for (const auto& [ex, ey] : exclude) {
                if (x == ex && y == ey) { excluded = true; break; }
            }
            if (excluded) continue;

            // Fast path: return first match when no rng
            if (!rng) {
                out_x = x;
                out_y = y;
                return true;
            }
            candidates.push_back({x, y});
        }
    }

    if (candidates.empty()) return false;

    std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1);
    auto [cx, cy] = candidates[dist(*rng)];
    out_x = cx;
    out_y = cy;
    return true;
}

bool TileMap::find_open_spot_other_room(int avoid_x, int avoid_y, int& out_x, int& out_y,
                                        const std::vector<std::pair<int,int>>& exclude,
                                        std::mt19937* rng) const {
    int avoid_rid = region_id(avoid_x, avoid_y);

    std::vector<std::pair<int,int>> candidates;
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            { Tile t_ = get(x, y); if (t_ != Tile::Floor && t_ != Tile::IndoorFloor) continue; }
            int rid = region_id(x, y);
            if (rid < 0 || rid == avoid_rid) continue;
            if (regions_[rid].type != RegionType::Room) continue;

            bool excluded = false;
            for (const auto& [ex, ey] : exclude) {
                if (x == ex && y == ey) { excluded = true; break; }
            }
            if (excluded) continue;

            if (!rng) {
                out_x = x;
                out_y = y;
                return true;
            }
            candidates.push_back({x, y});
        }
    }

    if (candidates.empty()) return false;

    std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1);
    auto [cx, cy] = candidates[dist(*rng)];
    out_x = cx;
    out_y = cy;
    return true;
}

void TileMap::load_from(int w, int h, MapType type, Biome biome, std::string location,
                        std::vector<Tile> tiles, std::vector<int> rids,
                        std::vector<Region> regions, std::vector<char> backdrop) {
    width_ = w;
    height_ = h;
    map_type_ = type;
    biome_ = biome;
    location_name_ = std::move(location);
    tiles_ = std::move(tiles);
    region_ids_ = std::move(rids);
    regions_ = std::move(regions);
    backdrop_ = std::move(backdrop);
}

void TileMap::load_fixtures(std::vector<FixtureData> fixtures, std::vector<int> fids) {
    fixtures_ = std::move(fixtures);
    fixture_ids_ = std::move(fids);
}

bool TileMap::find_open_spot_in_region(int rid, int& out_x, int& out_y,
                                        const std::vector<std::pair<int,int>>& exclude,
                                        std::mt19937* rng) const {
    if (rid < 0) return false;

    std::vector<std::pair<int,int>> candidates;
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            if (region_id(x, y) != rid) continue;
            { Tile t_ = get(x, y); if (t_ != Tile::Floor && t_ != Tile::IndoorFloor) continue; }

            bool excluded = false;
            for (const auto& [ex, ey] : exclude) {
                if (x == ex && y == ey) { excluded = true; break; }
            }
            if (excluded) continue;

            if (!rng) {
                out_x = x;
                out_y = y;
                return true;
            }
            candidates.push_back({x, y});
        }
    }

    if (candidates.empty()) return false;

    std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1);
    auto [cx, cy] = candidates[dist(*rng)];
    out_x = cx;
    out_y = cy;
    return true;
}

// --- Fixture factory ---

FixtureData make_fixture(FixtureType type) {
    FixtureData fd;
    fd.type = type;
    fd.cooldown = 0;
    fd.last_used_tick = -1;

    switch (type) {
        case FixtureType::Door:
            fd.glyph = '+'; fd.utf8_glyph = nullptr;
            fd.color = static_cast<Color>(137);  // warm brown
            fd.passable = false; fd.interactable = true; break;
        case FixtureType::Window:
            fd.glyph = 'O'; fd.utf8_glyph = nullptr;
            fd.color = Color::Cyan;
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::Table:
            fd.glyph = 'o'; fd.utf8_glyph = "\xc2\xa4";       // ¤
            fd.color = Color::DarkGray;
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::Console:
            fd.glyph = '#'; fd.utf8_glyph = "\xe2\x95\xac";   // ╬
            fd.color = Color::Cyan;
            fd.passable = false; fd.interactable = false;
            fd.light_radius = 2; break;
        case FixtureType::Crate:
            fd.glyph = '='; fd.utf8_glyph = "\xe2\x96\xa0";   // ■
            fd.color = Color::Yellow;
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::Bunk:
            fd.glyph = '='; fd.utf8_glyph = "\xe2\x89\xa1";   // ≡
            fd.color = Color::DarkGray;
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::Rack:
            fd.glyph = '|'; fd.utf8_glyph = "\xe2\x95\x8f";   // ╏
            fd.color = Color::DarkGray;
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::Conduit:
            fd.glyph = '%'; fd.utf8_glyph = "\xe2\x95\xa3";   // ╣
            fd.color = Color::DarkGray;
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::ShuttleClamp:
            fd.glyph = '='; fd.utf8_glyph = "\xe2\x95\xa4";   // ╤
            fd.color = Color::White;
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::Shelf:
            fd.glyph = '['; fd.utf8_glyph = "\xe2\x95\x94";   // ╔
            fd.color = Color::DarkGray;
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::Viewport:
            fd.glyph = '"'; fd.utf8_glyph = "\xe2\x96\x91";   // ░
            fd.color = Color::Cyan;
            fd.passable = false; fd.interactable = false;
            fd.light_radius = 1; break;
        case FixtureType::Torch:
            fd.glyph = '*';
            fd.color = Color::Yellow;
            fd.passable = true; fd.interactable = false;
            fd.light_radius = 8; break;
        case FixtureType::Stool:
            fd.glyph = 'o'; fd.utf8_glyph = "\xc2\xb7";       // ·
            fd.color = Color::DarkGray;
            fd.passable = true; fd.interactable = false; break;
        case FixtureType::Debris:
            fd.glyph = ',';
            fd.color = Color::DarkGray;
            fd.passable = true; fd.interactable = false; break;
        case FixtureType::HealPod:
            fd.glyph = '+'; fd.utf8_glyph = "\xe2\x9c\x9a";   // ✚
            fd.color = Color::Green;
            fd.passable = false; fd.interactable = true;
            fd.cooldown = 50; break;
        case FixtureType::FoodTerminal:
            fd.glyph = '$';
            fd.color = Color::Yellow;
            fd.passable = false; fd.interactable = true; break;
        case FixtureType::WeaponDisplay:
            fd.glyph = '/'; fd.utf8_glyph = "\xe2\x80\xa0";   // †
            fd.color = Color::Red;
            fd.passable = false; fd.interactable = true; break;
        case FixtureType::RepairBench:
            fd.glyph = '%'; fd.utf8_glyph = "\xe2\x95\xaa";   // ╪
            fd.color = Color::Cyan;
            fd.passable = false; fd.interactable = true; break;
        case FixtureType::SupplyLocker:
            fd.glyph = '&'; fd.utf8_glyph = "\xe2\x96\xaa";   // ▪
            fd.color = Color::Yellow;
            fd.passable = false; fd.interactable = true; break;
        case FixtureType::StarChart:
            fd.glyph = '*';
            fd.color = Color::Cyan;
            fd.passable = false; fd.interactable = true; break;
        case FixtureType::RestPod:
            fd.glyph = '='; fd.utf8_glyph = "\xe2\x88\xa9";   // ∩
            fd.color = Color::Green;
            fd.passable = false; fd.interactable = true;
            fd.cooldown = 50; break;
        case FixtureType::ShipTerminal:
            fd.glyph = '>'; fd.utf8_glyph = "\xc2\xbb";       // »
            fd.color = Color::Yellow;
            fd.passable = false; fd.interactable = true; break;
        case FixtureType::CommandTerminal:
            fd.glyph = '#'; fd.utf8_glyph = "\xe2\x96\xa3";   // ▣
            fd.color = Color::Cyan;
            fd.passable = false; fd.interactable = true; break;
        case FixtureType::DungeonHatch:
            fd.glyph = 'v'; fd.utf8_glyph = "\xe2\x96\xbc";   // ▼
            fd.color = Color::Yellow;
            fd.passable = false; fd.interactable = true; break;
        case FixtureType::StairsUp:
            fd.glyph = '<'; fd.utf8_glyph = "\xe2\x96\xb2";   // ▲
            fd.color = Color::White;
            fd.passable = false; fd.interactable = true; break;
    }
    return fd;
}

// --- Room feature defaults ---

RoomFeature default_features(RoomFlavor flavor) {
    switch (flavor) {
        case RoomFlavor::Medbay:        return RoomFeature::Healing;
        case RoomFlavor::CrewQuarters:  return RoomFeature::Rest;
        case RoomFlavor::Cantina:       return RoomFeature::FoodShop;
        case RoomFlavor::Armory:        return RoomFeature::WeaponShop;
        case RoomFlavor::CommandCenter: return RoomFeature::QuestGiver;
        case RoomFlavor::Observatory:   return RoomFeature::LoreSource;
        case RoomFlavor::Engineering:   return RoomFeature::Repair;
        case RoomFlavor::StorageBay:    return RoomFeature::Storage;
        default:                        return RoomFeature::None;
    }
}

BiomeColors biome_colors(Biome b) {
    switch (b) {
        case Biome::Station:
            return {Color::White, Color::Default, Color::Blue, Color::Blue};
        case Biome::Rocky:
            return {Color::White, Color::DarkGray, Color::Blue, Color::Blue};
        case Biome::Volcanic:
            return {Color::Red, static_cast<Color>(52), Color::Red, static_cast<Color>(52)};
        case Biome::Ice:
            return {Color::Cyan, Color::White, static_cast<Color>(39), Color::Blue};
        case Biome::Sandy:
            return {Color::Yellow, static_cast<Color>(180), Color::Blue, static_cast<Color>(58)};
        case Biome::Aquatic:
            return {static_cast<Color>(30), static_cast<Color>(24), Color::Blue, static_cast<Color>(24)};
        case Biome::Fungal:
            return {Color::Green, static_cast<Color>(22), Color::Green, static_cast<Color>(22)};
        case Biome::Crystal:
            return {Color::BrightMagenta, Color::Magenta, Color::Magenta, static_cast<Color>(54)};
        case Biome::Corroded:
            return {static_cast<Color>(142), static_cast<Color>(58), static_cast<Color>(148), static_cast<Color>(58)};
        case Biome::Forest:
            return {Color::Green, static_cast<Color>(58), Color::Blue, static_cast<Color>(22)};
        case Biome::Grassland:
            return {Color::DarkGray, Color::Green, Color::Blue, static_cast<Color>(22)};
        case Biome::Jungle:
            return {static_cast<Color>(22), static_cast<Color>(22), static_cast<Color>(30), static_cast<Color>(22)};
    }
    return {Color::White, Color::Default, Color::Blue, Color::Blue};
}

} // namespace astra
