#include "astra/outpost_planner.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace astra {

namespace {

// --- Geometry constants ---

// Fenced core: rectangle of palisade containing the main building.
constexpr int kFenceWidth  = 30;
constexpr int kFenceHeight = 22;

// Main building: fixed-size, centered in the fence.
constexpr int kBuildingWidth  = 13;
constexpr int kBuildingHeight = 9;

// Tent footprint (3 wide x 2 tall).
constexpr int kTentWidth  = 3;
constexpr int kTentHeight = 2;

// Gate opening half-width (3x3 zone in PerimeterBuilder).
constexpr int kGateHalfWidth = 1;

// Outward path length from the gate into the wilderness.
constexpr int kOutwardPathLen = 6;

// Minimum gap between tents and the fence / each other.
constexpr int kTentFenceGap = 2;
constexpr int kTentTentGap  = 1;

// --- Helpers ---

bool in_bounds(int x, int y, const TileMap& map) {
    return x >= 0 && x < map.width() && y >= 0 && y < map.height();
}

enum class GateSide { North, South, East, West };

GateSide pick_gate_side(std::mt19937& rng) {
    switch (rng() % 4) {
        case 0: return GateSide::North;
        case 1: return GateSide::South;
        case 2: return GateSide::East;
        default: return GateSide::West;
    }
}

// Returns the single gate position along the given side of the fence rect.
std::pair<int, int> gate_position(const Rect& fence, GateSide side) {
    int cx = fence.x + fence.w / 2;
    int cy = fence.y + fence.h / 2;
    switch (side) {
        case GateSide::North: return {cx, fence.y};
        case GateSide::South: return {cx, fence.y + fence.h - 1};
        case GateSide::East:  return {fence.x + fence.w - 1, cy};
        case GateSide::West:  return {fence.x, cy};
    }
    return {cx, cy};
}

// Build the main building shape with the door on the side facing the gate.
BuildingShape make_main_building_shape(int cx, int cy, GateSide gate_side) {
    BuildingShape shape;
    int px = cx - kBuildingWidth / 2;
    int py = cy - kBuildingHeight / 2;
    shape.primary = Rect{px, py, kBuildingWidth, kBuildingHeight};

    int dx = px + kBuildingWidth / 2;
    int dy = py + kBuildingHeight / 2;
    switch (gate_side) {
        case GateSide::North: dy = py; break;
        case GateSide::South: dy = py + kBuildingHeight - 1; break;
        case GateSide::East:  dx = px + kBuildingWidth - 1; break;
        case GateSide::West:  dx = px; break;
    }
    shape.door_positions.push_back({dx, dy});
    return shape;
}

// Glyph override slot used by set_glyph_override:
//   0 = metal, 1 = concrete, 2 = wood, 3 = salvage
uint8_t fence_material_glyph(Biome b) {
    switch (b) {
        case Biome::Forest:
        case Biome::Jungle:
        case Biome::Grassland:
        case Biome::Marsh:
        case Biome::Fungal:
            return 2; // wood palisade
        case Biome::Rocky:
        case Biome::Mountains:
            return 1; // stacked stone
        case Biome::Sandy:
        case Biome::Volcanic:
        case Biome::Aquatic:
        case Biome::ScarredScorched:
        case Biome::ScarredGlassed:
            return 3; // salvage plating
        case Biome::Ice:
        case Biome::Crystal:
            return 0; // metal / ice blocks
        default:
            return 2; // default to wood
    }
}

// Build a frontier-style CivStyle with decayed fence for the outpost. Interior
// fixture roles come from the caller's selected civ style so lore reactivity
// still works for the main building's furniture palette.
CivStyle make_outpost_style(const CivStyle& base) {
    CivStyle style = base;
    // Keep interior fixture roles from the base civ; override perimeter bits
    // so the fence looks makeshift and skips the auto corner towers.
    style.decay = 0.3f;                    // ~70% fence coverage
    style.perimeter_wall = Tile::Wall;     // plain Wall for glyph override post-pass
    if (style.name == "Advanced") {
        style.name = "Frontier";           // avoid PerimeterBuilder's 3x3 towers
    }
    return style;
}

// Test whether a candidate tent rect intersects the fence rect (with gap) or
// an already-placed tent.
bool tent_overlap(const Rect& cand,
                  const Rect& fence,
                  const std::vector<Rect>& placed) {
    // Gap to fence: cand must stay at least kTentFenceGap tiles away from
    // the fence perimeter (either outside it, not intersecting an expanded box).
    Rect fence_expanded{
        fence.x - kTentFenceGap,
        fence.y - kTentFenceGap,
        fence.w + kTentFenceGap * 2,
        fence.h + kTentFenceGap * 2
    };
    bool intersects_fence =
        !(cand.x + cand.w <= fence_expanded.x ||
          fence_expanded.x + fence_expanded.w <= cand.x ||
          cand.y + cand.h <= fence_expanded.y ||
          fence_expanded.y + fence_expanded.h <= cand.y);
    if (intersects_fence) return true;

    for (auto& p : placed) {
        Rect p_expanded{p.x - kTentTentGap, p.y - kTentTentGap,
                        p.w + kTentTentGap * 2, p.h + kTentTentGap * 2};
        bool hit =
            !(cand.x + cand.w <= p_expanded.x ||
              p_expanded.x + p_expanded.w <= cand.x ||
              cand.y + cand.h <= p_expanded.y ||
              p_expanded.y + p_expanded.h <= cand.y);
        if (hit) return true;
    }
    return false;
}

// Determine which side of the fence a tent is nearest to. The tent's door
// should open on the side facing the fence.
GateSide tent_facing_side(const Rect& tent, const Rect& fence) {
    int tent_cx = tent.x + tent.w / 2;
    int tent_cy = tent.y + tent.h / 2;
    int fence_cx = fence.x + fence.w / 2;
    int fence_cy = fence.y + fence.h / 2;
    int dx = fence_cx - tent_cx;
    int dy = fence_cy - tent_cy;
    if (std::abs(dx) > std::abs(dy)) {
        return dx > 0 ? GateSide::East : GateSide::West;
    }
    return dy > 0 ? GateSide::South : GateSide::North;
}

// Hand-stamp a single 3x2 tent: walls on the 5 perimeter tiles, floor + door
// on the side facing the fence, glyph override on every wall tile.
void stamp_tent(TileMap& map, const Rect& tent, GateSide door_side,
                uint8_t glyph_id) {
    // Tent interior: single tile at (x+1, y) because tent is 3 wide 2 tall,
    // primary interior is (x+1, y+1)... but there's none for 3x2. Actually a
    // 3x2 rect has 6 tiles: (x,y) (x+1,y) (x+2,y) (x,y+1) (x+1,y+1) (x+2,y+1).
    // Edge tiles = all 6 (every tile is on the perimeter). So there is no
    // interior tile. We place 5 walls + 1 door-floor. The door replaces one
    // perimeter tile with a Floor + Door fixture.
    //
    // Pick the door tile on the door_side at the tent's midpoint:
    int door_x, door_y;
    switch (door_side) {
        case GateSide::North:
            door_x = tent.x + tent.w / 2; // (x+1, y)
            door_y = tent.y;
            break;
        case GateSide::South:
            door_x = tent.x + tent.w / 2; // (x+1, y+1)
            door_y = tent.y + tent.h - 1;
            break;
        case GateSide::East:
            door_x = tent.x + tent.w - 1; // (x+2, y) or (x+2, y+1) — pick lower
            door_y = tent.y + tent.h - 1;
            break;
        case GateSide::West:
            door_x = tent.x;
            door_y = tent.y + tent.h - 1;
            break;
    }

    for (int ty = tent.y; ty < tent.y + tent.h; ++ty) {
        for (int tx = tent.x; tx < tent.x + tent.w; ++tx) {
            if (!in_bounds(tx, ty, map)) continue;
            if (tx == door_x && ty == door_y) {
                map.set(tx, ty, Tile::Floor);
                if (map.fixture_id(tx, ty) >= 0) map.remove_fixture(tx, ty);
                map.add_fixture(tx, ty, make_fixture(FixtureType::Door));
            } else {
                map.set(tx, ty, Tile::Wall);
                if (map.fixture_id(tx, ty) >= 0) map.remove_fixture(tx, ty);
                map.set_glyph_override(tx, ty, glyph_id);
            }
        }
    }
}

// Place a small campfire cluster around (cx, cy): CampStove in the middle,
// Bench tiles flanking it. Skips tiles that already hold structural content.
void stamp_campfire(TileMap& map, int cx, int cy) {
    static constexpr int offsets[][2] = {{0, 0}, {-1, 0}, {1, 0}};
    static const FixtureType types[] = {
        FixtureType::CampStove,
        FixtureType::Bench,
        FixtureType::Bench,
    };
    for (int i = 0; i < 3; ++i) {
        int x = cx + offsets[i][0];
        int y = cy + offsets[i][1];
        if (!in_bounds(x, y, map)) continue;
        Tile t = map.get(x, y);
        if (t == Tile::Wall || t == Tile::StructuralWall ||
            t == Tile::IndoorFloor || t == Tile::Water) continue;
        if (map.fixture_id(x, y) >= 0) {
            // Don't overwrite existing gameplay fixtures.
            continue;
        }
        map.set(x, y, Tile::Floor);
        map.add_fixture(x, y, make_fixture(types[i]));
    }
}

} // anonymous namespace

// ===========================================================================
// OutpostPlanner::plan
// ===========================================================================

SettlementPlan OutpostPlanner::plan(const PlacementResult& placement,
                                    const TerrainChannels& /*channels*/,
                                    const TileMap& /*map*/,
                                    const MapProperties& props,
                                    std::mt19937& rng) const {
    SettlementPlan result;
    result.placement = placement;
    result.size_category = 0;  // outposts are always small

    CivStyle base = select_civ_style(props);
    result.style = make_outpost_style(base);

    const Rect& fp = placement.footprint;
    int center_x = fp.x + fp.w / 2;
    int center_y = fp.y + fp.h / 2;

    // --- 1. Clear the footprint terrain ---
    result.terrain_mods.push_back({TerrainModType::Clear, fp, 0.0f});

    // --- 2. Fenced core rectangle, centered in the footprint ---
    Rect fence{
        center_x - kFenceWidth / 2,
        center_y - kFenceHeight / 2,
        kFenceWidth,
        kFenceHeight
    };

    // --- 3. Gate side + position ---
    GateSide gate_side = pick_gate_side(rng);
    auto [gate_x, gate_y] = gate_position(fence, gate_side);

    // --- 4. Main building, centered, door facing the gate ---
    {
        BuildingSpec spec;
        spec.type = BuildingType::OutpostMain;
        spec.shape = make_main_building_shape(center_x, center_y, gate_side);
        spec.anchor = {center_x, center_y, AnchorType::Center};
        result.buildings.push_back(std::move(spec));
    }

    // --- 5. Perimeter (fence) ---
    {
        PerimeterSpec peri;
        peri.bounds = fence;
        peri.gate_positions.push_back({gate_x, gate_y});
        result.perimeter = std::move(peri);
    }

    // --- 6. Paths ---
    auto& main = result.buildings.front();
    auto& door = main.shape.door_positions.front();

    // Gate -> main building door (L-shaped, 1-wide so PathRouter grid-routes)
    result.paths.push_back({gate_x, gate_y, door.first, door.second, 1});

    // Gate -> outward into the wilderness (short 3-wide stub)
    int out_x = gate_x;
    int out_y = gate_y;
    switch (gate_side) {
        case GateSide::North: out_y = std::max(0, gate_y - kOutwardPathLen); break;
        case GateSide::South: out_y = gate_y + kOutwardPathLen; break;
        case GateSide::East:  out_x = gate_x + kOutwardPathLen; break;
        case GateSide::West:  out_x = std::max(0, gate_x - kOutwardPathLen); break;
    }
    result.paths.push_back({gate_x, gate_y, out_x, out_y, 3});

    return result;
}

// ===========================================================================
// OutpostPlanner::post_stamp
// ===========================================================================

void OutpostPlanner::post_stamp(TileMap& map, const SettlementPlan& plan,
                                Biome biome, std::mt19937& rng) const {
    if (!plan.perimeter.has_value()) return;
    const Rect& fence = plan.perimeter->bounds;
    const Rect& fp = plan.placement.footprint;

    uint8_t glyph_id = fence_material_glyph(biome);

    // --- 1. Fence glyph override ---
    // Walk the fence bounding box; for every Wall tile (placed by PerimeterBuilder)
    // that sits on or near the nominal perimeter, apply the biome glyph override.
    // Organic noise offsets in PerimeterBuilder can nudge walls +/- 1 tile off
    // the exact boundary, so we scan a 3-tile-thick band around each edge.
    auto override_fence_band = [&](int x0, int y0, int x1, int y1) {
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                if (!in_bounds(x, y, map)) continue;
                if (map.get(x, y) == Tile::Wall) {
                    map.set_glyph_override(x, y, glyph_id);
                }
            }
        }
    };

    // North / south bands
    override_fence_band(fence.x - 1, fence.y - 1,
                        fence.x + fence.w, fence.y + 1);
    override_fence_band(fence.x - 1, fence.y + fence.h - 2,
                        fence.x + fence.w, fence.y + fence.h);
    // West / east bands
    override_fence_band(fence.x - 1, fence.y - 1,
                        fence.x + 1, fence.y + fence.h);
    override_fence_band(fence.x + fence.w - 2, fence.y - 1,
                        fence.x + fence.w, fence.y + fence.h);

    // --- 2. Hand-stamp tents in the outer ring ---
    std::uniform_int_distribution<int> count_dist(2, 4);
    int target_tents = count_dist(rng);

    std::vector<Rect> placed_tents;
    placed_tents.reserve(target_tents);

    std::uniform_int_distribution<int> x_dist(fp.x + 1, fp.x + fp.w - kTentWidth - 1);
    std::uniform_int_distribution<int> y_dist(fp.y + 1, fp.y + fp.h - kTentHeight - 1);

    int placed = 0;
    int attempts = 0;
    while (placed < target_tents && attempts < 60) {
        ++attempts;
        int tx = x_dist(rng);
        int ty = y_dist(rng);
        Rect cand{tx, ty, kTentWidth, kTentHeight};

        if (tent_overlap(cand, fence, placed_tents)) continue;

        // Skip sites that would stamp into water, existing walls, or indoor
        // floors (which would mean we're on top of the main building).
        bool bad_terrain = false;
        for (int yy = cand.y; yy < cand.y + cand.h && !bad_terrain; ++yy) {
            for (int xx = cand.x; xx < cand.x + cand.w; ++xx) {
                if (!in_bounds(xx, yy, map)) { bad_terrain = true; break; }
                Tile t = map.get(xx, yy);
                if (t == Tile::Water || t == Tile::IndoorFloor ||
                    t == Tile::StructuralWall) {
                    bad_terrain = true; break;
                }
            }
        }
        if (bad_terrain) continue;

        GateSide facing = tent_facing_side(cand, fence);
        stamp_tent(map, cand, facing, glyph_id);
        placed_tents.push_back(cand);
        ++placed;
    }

    // --- 3. Campfire clusters near the first 1-2 tents ---
    int campfire_count = std::min<int>(
        static_cast<int>(placed_tents.size()),
        1 + static_cast<int>(rng() % 2));
    for (int i = 0; i < campfire_count; ++i) {
        const Rect& tent = placed_tents[i];
        int fence_cx = fence.x + fence.w / 2;
        int fence_cy = fence.y + fence.h / 2;
        int tent_cx = tent.x + tent.w / 2;
        int tent_cy = tent.y + tent.h / 2;

        // Place the campfire 2 tiles further away from the fence than the
        // tent's center, in the outward direction, so it sits in the open.
        int dx = tent_cx - fence_cx;
        int dy = tent_cy - fence_cy;
        int fire_x = tent_cx;
        int fire_y = tent_cy;
        if (std::abs(dx) > std::abs(dy)) {
            fire_x = tent_cx + (dx > 0 ? 2 : -2);
        } else {
            fire_y = tent_cy + (dy > 0 ? 2 : -2);
        }
        stamp_campfire(map, fire_x, fire_y);
    }
}

} // namespace astra
