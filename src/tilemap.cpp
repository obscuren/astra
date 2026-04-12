#include "astra/tilemap.h"

#include "astra/poi_budget.h"
#include "astra/poi_placement.h"

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
    if (t == Tile::Floor || t == Tile::IndoorFloor || t == Tile::Path || t == Tile::Portal || t == Tile::Water || t == Tile::Ice)
        return true;
    // Overworld tiles: all passable except lakes and glassed craters
    if (t == Tile::OW_Lake || t == Tile::OW_GlassedCrater) return false;
    if (t >= Tile::OW_Plains && t <= Tile::OW_Landing) return true;
    return false;
}

bool TileMap::opaque(int x, int y) const {
    Tile t = get(x, y);
    if (t >= Tile::OW_Plains && t <= Tile::OW_Landing) return false;
    if (t == Tile::Wall || t == Tile::StructuralWall || t == Tile::Empty) return true;
    // Vision-blocking fixtures (tall trees, large mushrooms)
    if (t == Tile::Fixture) {
        int fid = fixture_id(x, y);
        if (fid >= 0 && fixtures_[fid].blocks_vision) return true;
    }
    return false;
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

void TileMap::remove_fixture(int x, int y) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    int idx = y * width_ + x;
    fixture_ids_[idx] = -1;
    // Restore tile from Fixture back to Floor (add_fixture sets Tile::Fixture)
    if (tiles_[idx] == Tile::Fixture) {
        tiles_[idx] = Tile::Floor;
    }
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
    return (custom_flags_[y * width_ + x] & 0x01) != 0;
}

void TileMap::set_custom_detail(int x, int y, bool v) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    if (custom_flags_.empty()) {
        custom_flags_.resize(width_ * height_, 0);
    }
    auto& flags = custom_flags_[y * width_ + x];
    if (v) flags |= 0x01;
    else   flags &= ~0x01;
}

void TileMap::set_custom_flag(int x, int y, uint8_t bit) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    if (custom_flags_.empty()) custom_flags_.resize(width_ * height_, 0);
    custom_flags_[y * width_ + x] |= bit;
}

bool TileMap::has_custom_flag(int x, int y, uint8_t bit) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return false;
    if (custom_flags_.empty()) return false;
    return (custom_flags_[y * width_ + x] & bit) != 0;
}

uint8_t TileMap::get_custom_flags(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return 0;
    if (custom_flags_.empty()) return 0;
    return custom_flags_[y * width_ + x];
}

void TileMap::set_custom_flags_byte(int x, int y, uint8_t value) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    if (custom_flags_.empty()) custom_flags_.resize(width_ * height_, 0);
    custom_flags_[y * width_ + x] = value;
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
            fd.passable = false; fd.interactable = true; break;
        case FixtureType::Window:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::Table:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::Console:
            fd.passable = false; fd.interactable = false;
            fd.light_radius = 2; break;
        case FixtureType::Crate:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::Bunk:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::Rack:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::Conduit:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::ShuttleClamp:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::Shelf:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::Viewport:
            fd.passable = false; fd.interactable = false;
            fd.light_radius = 1; break;
        case FixtureType::Torch:
            fd.passable = true; fd.interactable = false;
            fd.light_radius = 8; break;
        case FixtureType::Stool:
            fd.passable = true; fd.interactable = false; break;
        case FixtureType::Debris:
            fd.passable = true; fd.interactable = false; break;
        case FixtureType::HealPod:
            fd.passable = false; fd.interactable = true;
            fd.cooldown = 50; break;
        case FixtureType::FoodTerminal:
            fd.passable = false; fd.interactable = true; break;
        case FixtureType::WeaponDisplay:
            fd.passable = false; fd.interactable = true; break;
        case FixtureType::RepairBench:
            fd.passable = false; fd.interactable = true; break;
        case FixtureType::SupplyLocker:
            fd.passable = false; fd.interactable = true; break;
        case FixtureType::StarChart:
            fd.passable = false; fd.interactable = true; break;
        case FixtureType::RestPod:
            fd.passable = false; fd.interactable = true;
            fd.cooldown = 50; break;
        case FixtureType::ShipTerminal:
            fd.passable = false; fd.interactable = true; break;
        case FixtureType::CommandTerminal:
            fd.passable = false; fd.interactable = true; break;
        case FixtureType::DungeonHatch:
            fd.passable = false; fd.interactable = true; break;
        case FixtureType::StairsUp:
            fd.passable = false; fd.interactable = true; break;
        case FixtureType::NaturalObstacle:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::ShoreDebris:
            fd.passable = true; fd.interactable = false; break;
        case FixtureType::SettlementProp:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::CampStove:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::Lamp:
            fd.passable = true; fd.interactable = false;
            fd.light_radius = 6; break;
        case FixtureType::HoloLight:
            fd.passable = true; fd.interactable = false;
            fd.light_radius = 8; break;
        case FixtureType::Locker:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::BookCabinet:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::DataTerminal:
            fd.passable = false; fd.interactable = false;
            fd.light_radius = 2; break;
        case FixtureType::Bench:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::Chair:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::Gate:
            fd.passable = true; fd.interactable = false; break;
        case FixtureType::BridgeRail:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::BridgeFloor:
            fd.passable = true; fd.interactable = false; break;
        case FixtureType::Planter:
            fd.passable = false; fd.interactable = false; break;
        case FixtureType::FloraFlower:
        case FixtureType::FloraHerb:
        case FixtureType::FloraMushroom:
        case FixtureType::FloraGrass:
        case FixtureType::FloraLichen:
        case FixtureType::MineralOre:
        case FixtureType::MineralCrystal:
        case FixtureType::ScrapComponent:
            fd.passable = true; fd.interactable = false; break;
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
        case Biome::Marsh:
            return {static_cast<Color>(23), static_cast<Color>(29), static_cast<Color>(33), static_cast<Color>(23)};
        case Biome::Mountains:
            return {static_cast<Color>(245), static_cast<Color>(243), Color::Blue, static_cast<Color>(240)};
        case Biome::MartianBarren:
            // wall, floor, water, remembered — rust tones
            return {static_cast<Color>(130), static_cast<Color>(94), Color::Blue, static_cast<Color>(52)};
        case Biome::MartianPolar:
            // icy rust — pale rose for ice over rust bedrock
            return {static_cast<Color>(225), static_cast<Color>(180), static_cast<Color>(39), static_cast<Color>(145)};
        case Biome::AlienCrystalline:
            return {Color::Cyan, static_cast<Color>(23), Color::White, Color::DarkGray};
        case Biome::AlienOrganic:
            return {Color::Red, static_cast<Color>(52), Color::Magenta, Color::DarkGray};
        case Biome::AlienGeometric:
            return {Color::Yellow, static_cast<Color>(58), Color::White, Color::DarkGray};
        case Biome::AlienVoid:
            return {Color::Magenta, Color::DarkGray, Color::Magenta, Color::DarkGray};
        case Biome::AlienLight:
            return {static_cast<Color>(228), Color::Yellow, Color::White, Color::DarkGray};
        case Biome::ScarredGlassed:
            return {static_cast<Color>(208), static_cast<Color>(94), static_cast<Color>(58), Color::DarkGray};
        case Biome::ScarredScorched:
            return {Color::DarkGray, static_cast<Color>(52), Color::Red, Color::DarkGray};
    }
    return {Color::White, Color::Default, Color::Blue, Color::Blue};
}

// --- POI budget / hidden POI / anchor hint helpers ---

namespace {
uint64_t hint_key(int x, int y, int width) {
    return static_cast<uint64_t>(y) * static_cast<uint64_t>(width)
         + static_cast<uint64_t>(x);
}
} // anonymous namespace

TileMap::TileMap() = default;
TileMap::~TileMap() = default;

TileMap::TileMap(TileMap&&) noexcept = default;
TileMap& TileMap::operator=(TileMap&&) noexcept = default;

TileMap::TileMap(const TileMap& other)
    : map_type_(other.map_type_),
      biome_(other.biome_),
      alien_biome_(other.alien_biome_),
      width_(other.width_),
      height_(other.height_),
      location_name_(other.location_name_),
      hub_(other.hub_),
      poi_bounds_(other.poi_bounds_),
      tiles_(other.tiles_),
      backdrop_(other.backdrop_),
      region_ids_(other.region_ids_),
      regions_(other.regions_),
      fixtures_(other.fixtures_),
      fixture_ids_(other.fixture_ids_),
      glyph_override_(other.glyph_override_),
      custom_flags_(other.custom_flags_),
      poi_budget_(other.poi_budget_ ? std::make_unique<PoiBudget>(*other.poi_budget_) : nullptr),
      hidden_pois_(other.hidden_pois_),
      anchor_hints_(other.anchor_hints_) {}

TileMap& TileMap::operator=(const TileMap& other) {
    if (this == &other) return *this;
    map_type_ = other.map_type_;
    biome_ = other.biome_;
    alien_biome_ = other.alien_biome_;
    width_ = other.width_;
    height_ = other.height_;
    location_name_ = other.location_name_;
    hub_ = other.hub_;
    poi_bounds_ = other.poi_bounds_;
    tiles_ = other.tiles_;
    backdrop_ = other.backdrop_;
    region_ids_ = other.region_ids_;
    regions_ = other.regions_;
    fixtures_ = other.fixtures_;
    fixture_ids_ = other.fixture_ids_;
    glyph_override_ = other.glyph_override_;
    custom_flags_ = other.custom_flags_;
    poi_budget_ = other.poi_budget_ ? std::make_unique<PoiBudget>(*other.poi_budget_) : nullptr;
    hidden_pois_ = other.hidden_pois_;
    anchor_hints_ = other.anchor_hints_;
    return *this;
}

const PoiBudget& TileMap::poi_budget() const {
    static const PoiBudget empty;
    return poi_budget_ ? *poi_budget_ : empty;
}

PoiBudget& TileMap::poi_budget_mut() {
    if (!poi_budget_) poi_budget_ = std::make_unique<PoiBudget>();
    return *poi_budget_;
}

void TileMap::set_poi_budget(PoiBudget b) {
    poi_budget_ = std::make_unique<PoiBudget>(std::move(b));
}

const std::vector<HiddenPoi>& TileMap::hidden_pois() const {
    return hidden_pois_;
}

std::vector<HiddenPoi>& TileMap::hidden_pois_mut() {
    return hidden_pois_;
}

const HiddenPoi* TileMap::find_hidden_poi(int x, int y) const {
    for (const auto& h : hidden_pois_) {
        if (h.x == x && h.y == y && !h.discovered) return &h;
    }
    return nullptr;
}

HiddenPoi* TileMap::find_hidden_poi_mut(int x, int y) {
    for (auto& h : hidden_pois_) {
        if (h.x == x && h.y == y && !h.discovered) return &h;
    }
    return nullptr;
}

const PoiAnchorHint* TileMap::anchor_hint(int x, int y) const {
    auto it = anchor_hints_.find(hint_key(x, y, width_));
    return (it != anchor_hints_.end()) ? &it->second : nullptr;
}

void TileMap::set_anchor_hint(int x, int y, const PoiAnchorHint& hint) {
    anchor_hints_[hint_key(x, y, width_)] = hint;
}

const std::unordered_map<uint64_t, PoiAnchorHint>& TileMap::anchor_hints() const {
    return anchor_hints_;
}

} // namespace astra
