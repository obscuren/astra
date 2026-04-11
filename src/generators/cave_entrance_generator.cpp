#include "astra/cave_entrance_generator.h"

#include "astra/cave_entrance_types.h"
#include "astra/placement_scorer.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace astra {

namespace {

// --- Helpers ---

bool in_bounds(int x, int y, const TileMap& map) {
    return x >= 0 && x < map.width() && y >= 0 && y < map.height();
}

// --- Variant specs ---

const CaveVariantSpec& spec_natural_cave() {
    static const CaveVariantSpec s{
        /*variant*/ CaveVariant::NaturalCave,
        /*name*/ "NaturalCave",
        /*foot_w*/ 16, /*foot_h*/ 12,
        /*requires_cliff*/ true,
        /*debris_min*/ 3, /*debris_max*/ 5,
        /*fixtures*/ {},
    };
    return s;
}

const CaveVariantSpec& spec_abandoned_mine() {
    static const CaveVariantSpec s{
        /*variant*/ CaveVariant::AbandonedMine,
        /*name*/ "AbandonedMine",
        /*foot_w*/ 22, /*foot_h*/ 16,
        /*requires_cliff*/ true,
        /*debris_min*/ 3, /*debris_max*/ 5,
        /*fixtures*/ {FixtureType::Crate, FixtureType::Crate, FixtureType::Conduit},
    };
    return s;
}

const CaveVariantSpec& spec_ancient_excavation() {
    static const CaveVariantSpec s{
        /*variant*/ CaveVariant::AncientExcavation,
        /*name*/ "AncientExcavation",
        /*foot_w*/ 30, /*foot_h*/ 22,
        /*requires_cliff*/ false,
        /*debris_min*/ 2, /*debris_max*/ 4,
        /*fixtures*/ {FixtureType::Console, FixtureType::BookCabinet},
    };
    return s;
}

const CaveVariantSpec& spec_for(CaveVariant v) {
    switch (v) {
        case CaveVariant::NaturalCave:       return spec_natural_cave();
        case CaveVariant::AbandonedMine:     return spec_abandoned_mine();
        case CaveVariant::AncientExcavation: return spec_ancient_excavation();
        case CaveVariant::None: break;
    }
    return spec_natural_cave();
}

// --- Variant selection ---

CaveVariant pick_cave_variant(int lore_tier, Biome b, std::mt19937& rng) {
    bool cliff_biome = (b == Biome::Mountains || b == Biome::Rocky ||
                        b == Biome::Volcanic);
    int r = static_cast<int>(rng() % 100);

    if (!cliff_biome) {
        if (lore_tier < 1) return CaveVariant::None;
        return CaveVariant::AncientExcavation;
    }

    if (lore_tier <= 0) {
        if (r < 80) return CaveVariant::NaturalCave;
        if (r < 95) return CaveVariant::AbandonedMine;
        return CaveVariant::AncientExcavation;
    }
    if (lore_tier == 1) {
        if (r < 50) return CaveVariant::NaturalCave;
        if (r < 85) return CaveVariant::AbandonedMine;
        return CaveVariant::AncientExcavation;
    }
    if (r < 20) return CaveVariant::NaturalCave;
    if (r < 50) return CaveVariant::AbandonedMine;
    return CaveVariant::AncientExcavation;
}

// --- Cliff edge detection (map-wide) ---
//
// Scans the entire map for "cliff edge" tiles — a Wall tile that's part
// of a wall cluster (2+ wall neighbors) AND has an adjacent Floor tile.
// Collects all candidates, then filters by whether the surrounding area
// has enough open terrain to fit the variant's footprint.

bool has_open_area_around(const TileMap& map, int cx, int cy,
                          int half_w, int half_h) {
    int open = 0;
    int total = 0;
    for (int y = cy - half_h; y <= cy + half_h; ++y) {
        for (int x = cx - half_w; x <= cx + half_w; ++x) {
            if (!in_bounds(x, y, map)) continue;
            ++total;
            Tile t = map.get(x, y);
            if (t == Tile::Floor || t == Tile::IndoorFloor) ++open;
        }
    }
    if (total == 0) return false;
    // Require at least 40% of the surrounding area to be open terrain so
    // there's room for the cave footprint without stamping into solid rock.
    return (float)open / (float)total > 0.40f;
}

std::optional<CliffHit> find_cliff_edge_global(const TileMap& map,
                                               int foot_w, int foot_h,
                                               std::mt19937& rng) {
    static constexpr int dxs[] = { 0,  0, -1, 1};
    static constexpr int dys[] = {-1,  1,  0, 0};

    std::vector<CliffHit> candidates;
    int w = map.width();
    int h = map.height();
    int margin = std::max(foot_w, foot_h);

    for (int y = margin; y < h - margin; ++y) {
        for (int x = margin; x < w - margin; ++x) {
            if (map.get(x, y) != Tile::Wall) continue;

            // Must be part of a cliff cluster (2+ wall neighbors).
            int wall_neighbors = 0;
            for (int d = 0; d < 4; ++d) {
                int nx = x + dxs[d], ny = y + dys[d];
                if (in_bounds(nx, ny, map) && map.get(nx, ny) == Tile::Wall)
                    ++wall_neighbors;
            }
            if (wall_neighbors < 2) continue;

            // Must have at least one Floor neighbor.
            int floor_dir = -1;
            for (int d = 0; d < 4; ++d) {
                int nx = x + dxs[d], ny = y + dys[d];
                if (!in_bounds(nx, ny, map)) continue;
                if (map.get(nx, ny) == Tile::Floor) { floor_dir = d; break; }
            }
            if (floor_dir < 0) continue;

            // Must have enough open terrain around it for the footprint.
            // Use half the max dimension as the check radius.
            int half = std::max(foot_w, foot_h) / 2;
            if (!has_open_area_around(map, x, y, half, half)) continue;

            CliffHit hit;
            hit.wall_x = x;
            hit.wall_y = y;
            hit.floor_x = x + dxs[floor_dir];
            hit.floor_y = y + dys[floor_dir];
            if (dys[floor_dir] < 0)      hit.mouth_facing = CaveFacing::North;
            else if (dys[floor_dir] > 0) hit.mouth_facing = CaveFacing::South;
            else if (dxs[floor_dir] < 0) hit.mouth_facing = CaveFacing::West;
            else                         hit.mouth_facing = CaveFacing::East;
            candidates.push_back(hit);
        }
    }

    if (candidates.empty()) return std::nullopt;

    // Return a random candidate so regenerations place the cave differently.
    size_t idx = rng() % candidates.size();
    return candidates[idx];
}

// Unit vector in the direction of the mouth facing (outward from cliff).
std::pair<int, int> facing_vector(CaveFacing f) {
    switch (f) {
        case CaveFacing::North: return { 0, -1};
        case CaveFacing::South: return { 0,  1};
        case CaveFacing::East:  return { 1,  0};
        case CaveFacing::West:  return {-1,  0};
    }
    return {0, 1};
}

// --- Stamp helpers ---

void clear_fixture_and_set(TileMap& map, int x, int y, Tile t) {
    if (!in_bounds(x, y, map)) return;
    if (map.fixture_id(x, y) >= 0) map.remove_fixture(x, y);
    map.set(x, y, t);
}

void place_debris_around(TileMap& map, int cx, int cy, int radius,
                          int min_count, int max_count, std::mt19937& rng) {
    std::uniform_int_distribution<int> count_dist(min_count, max_count);
    std::uniform_int_distribution<int> off_dist(-radius, radius);
    int num = count_dist(rng);
    for (int i = 0; i < num; ++i) {
        int x = cx + off_dist(rng);
        int y = cy + off_dist(rng);
        if (!in_bounds(x, y, map)) continue;
        if (map.get(x, y) != Tile::Floor) continue;
        if (map.fixture_id(x, y) >= 0) continue;
        map.set(x, y, Tile::Fixture);
        map.add_fixture(x, y, make_fixture(FixtureType::Debris));
    }
}

// Place extra fixtures from a list on random floor tiles within a rect.
void place_fixtures_in_rect(TileMap& map, const Rect& r,
                             const std::vector<FixtureType>& fixtures,
                             std::mt19937& rng) {
    std::vector<std::pair<int,int>> candidates;
    for (int y = r.y; y < r.y + r.h; ++y) {
        for (int x = r.x; x < r.x + r.w; ++x) {
            if (!in_bounds(x, y, map)) continue;
            if (map.get(x, y) != Tile::Floor &&
                map.get(x, y) != Tile::IndoorFloor) continue;
            if (map.fixture_id(x, y) >= 0) continue;
            candidates.push_back({x, y});
        }
    }
    if (candidates.empty()) return;
    std::shuffle(candidates.begin(), candidates.end(), rng);
    size_t placed = 0;
    for (FixtureType ft : fixtures) {
        if (placed >= candidates.size()) break;
        auto [x, y] = candidates[placed++];
        map.set(x, y, Tile::Fixture);
        map.add_fixture(x, y, make_fixture(ft));
    }
}

// --- Natural cave stamp ---
//
// Carves a 3-wide cave mouth directly INTO the cliff starting at the
// cliff hit tile, extending several tiles deeper into the rock. The
// portal sits at the back of the carved cave. The player walks up to
// the cliff, sees the dark mouth, enters it, and lands on the portal.

void stamp_natural_cave(TileMap& map, const Rect& foot, const CliffHit& hit,
                        std::mt19937& rng) {
    std::uniform_real_distribution<float> prob(0.0f, 1.0f);

    auto [fvx, fvy] = facing_vector(hit.mouth_facing);  // outward from cliff
    int pvx = -fvy;  // perpendicular
    int pvy = fvx;

    // Carve a 3-wide × 4-deep cave mouth INTO the cliff.
    // i=0 : cliff face (wall hit)
    // i=-1..-3 : deeper into the cliff
    constexpr int kDepth = 4;
    for (int i = 0; i < kDepth; ++i) {
        int base_x = hit.wall_x - fvx * i;
        int base_y = hit.wall_y - fvy * i;
        for (int w = -1; w <= 1; ++w) {
            int wx = base_x + pvx * w;
            int wy = base_y + pvy * w;
            clear_fixture_and_set(map, wx, wy, Tile::Floor);
        }
    }

    // Portal at the deepest point of the cave (back wall).
    int portal_x = hit.wall_x - fvx * (kDepth - 1);
    int portal_y = hit.wall_y - fvy * (kDepth - 1);
    clear_fixture_and_set(map, portal_x, portal_y, Tile::Portal);

    // Also carve a small 3-wide clearing just outside the mouth so the
    // player has room to walk up to the entrance without tripping over
    // scatter or existing boulders.
    for (int i = 1; i <= 2; ++i) {
        int base_x = hit.wall_x + fvx * i;
        int base_y = hit.wall_y + fvy * i;
        for (int w = -1; w <= 1; ++w) {
            int wx = base_x + pvx * w;
            int wy = base_y + pvy * w;
            if (!in_bounds(wx, wy, map)) continue;
            // Don't overwrite water or other important tiles; just clear scatter.
            if (map.fixture_id(wx, wy) >= 0) map.remove_fixture(wx, wy);
            if (map.get(wx, wy) == Tile::Wall) {
                map.set(wx, wy, Tile::Floor);
            }
        }
    }

    // Debris just outside the mouth (floor side).
    place_debris_around(map, hit.floor_x + fvx, hit.floor_y + fvy, 3, 3, 5, rng);

    // Scattered boulders around the footprint for flavor, avoiding the
    // cave mouth area.
    std::uniform_int_distribution<int> boulder_count(6, 10);
    int num_boulders = boulder_count(rng);
    std::uniform_int_distribution<int> ox(foot.x + 1, foot.x + foot.w - 2);
    std::uniform_int_distribution<int> oy(foot.y + 1, foot.y + foot.h - 2);
    for (int i = 0; i < num_boulders; ++i) {
        int bx = ox(rng);
        int by = oy(rng);
        if (!in_bounds(bx, by, map)) continue;
        if (map.get(bx, by) != Tile::Floor) continue;
        if (map.fixture_id(bx, by) >= 0) continue;
        // Keep the cave mouth clearing free.
        if (std::abs(bx - hit.wall_x) <= 3 && std::abs(by - hit.wall_y) <= 3)
            continue;
        map.set(bx, by, Tile::Wall);
    }
}

// --- Abandoned mine stamp ---
//
// Stamps a squared-off pit immediately adjacent to the cliff. The pit's
// cliff-facing edge backs into the rock, and its outward-facing edge
// has a clear 3-wide opening so the player can walk in. The portal
// sits at the back of the pit (against the cliff). Mine cart rails
// extend outward from the opening.

void stamp_abandoned_mine(TileMap& map, const Rect& foot, const CliffHit& hit,
                          std::mt19937& rng) {
    auto [fvx, fvy] = facing_vector(hit.mouth_facing);  // outward from cliff
    int pvx = -fvy;  // perpendicular
    int pvy = fvx;

    // Pit is 7 wide (parallel to cliff) × 5 deep (perpendicular to cliff).
    // The pit's back edge sits at the cliff face (wall hit) and extends
    // outward into the open terrain.
    constexpr int kPitParallel = 7;     // parallel to cliff
    constexpr int kPitPerpendicular = 5; // perpendicular to cliff

    // Center of pit: offset outward from the cliff hit by pit_perp/2.
    int pit_cx = hit.wall_x + fvx * (kPitPerpendicular / 2);
    int pit_cy = hit.wall_y + fvy * (kPitPerpendicular / 2);

    // Stamp pit tiles. For each local (parallel, perpendicular) offset,
    // rotate through pv/fv.
    auto pit_tile = [&](int par, int perp, Tile t, int glyph_id = -1) {
        int tx = pit_cx + pvx * par + fvx * perp;
        int ty = pit_cy + pvy * par + fvy * perp;
        if (!in_bounds(tx, ty, map)) return;
        if (map.fixture_id(tx, ty) >= 0) map.remove_fixture(tx, ty);
        map.set(tx, ty, t);
        if (glyph_id >= 0 && t == Tile::StructuralWall) {
            map.set_glyph_override(tx, ty, static_cast<uint8_t>(glyph_id));
        }
    };

    int par_min = -(kPitParallel / 2);
    int par_max = kPitParallel / 2;
    int perp_min = -(kPitPerpendicular / 2);
    int perp_max = kPitPerpendicular / 2;

    // Back edge of the pit (perp_min) sits near the cliff.
    // Front edge (perp_max) faces the open terrain.
    for (int par = par_min; par <= par_max; ++par) {
        for (int perp = perp_min; perp <= perp_max; ++perp) {
            bool back_edge = (perp == perp_min);
            bool front_edge = (perp == perp_max);
            bool side_edge = (par == par_min || par == par_max);

            if (back_edge || side_edge) {
                pit_tile(par, perp, Tile::StructuralWall, 0);  // metal
            } else if (front_edge) {
                // Front edge: leave a 3-tile opening in the center (par == -1, 0, 1).
                if (par >= -1 && par <= 1) {
                    pit_tile(par, perp, Tile::Floor);
                } else {
                    pit_tile(par, perp, Tile::StructuralWall, 0);
                }
            } else {
                pit_tile(par, perp, Tile::Floor);
            }
        }
    }

    // Wooden support beams: swap glyph to wood on the side edges near the front.
    pit_tile(par_min, perp_max - 1, Tile::StructuralWall, 2);
    pit_tile(par_max, perp_max - 1, Tile::StructuralWall, 2);
    pit_tile(par_min, 0, Tile::StructuralWall, 2);
    pit_tile(par_max, 0, Tile::StructuralWall, 2);

    // Portal at the back of the pit (par=0, perp=perp_min+1 so it's an
    // interior tile next to the back wall, not ON the back wall).
    {
        int bx = pit_cx + pvx * 0 + fvx * (perp_min + 1);
        int by = pit_cy + pvy * 0 + fvy * (perp_min + 1);
        clear_fixture_and_set(map, bx, by, Tile::Portal);
    }

    // Short mine cart rails leading outward from the pit's front opening.
    // Start at the gap (par=0, perp=perp_max) and extend outward.
    int rail_len = 6;
    for (int i = 1; i <= rail_len; ++i) {
        int rx = pit_cx + fvx * (perp_max + i);
        int ry = pit_cy + fvy * (perp_max + i);
        if (!in_bounds(rx, ry, map)) continue;
        if (map.fixture_id(rx, ry) >= 0) map.remove_fixture(rx, ry);
        map.set(rx, ry, Tile::Floor);
        if (i % 2 == 0) {
            map.set(rx, ry, Tile::Fixture);
            map.add_fixture(rx, ry, make_fixture(FixtureType::Conduit));
        }
    }

    // Clear a small approach pad (3 wide) in front of the rails so the
    // player has room to walk up to the pit from the open side.
    for (int i = 1; i <= 2; ++i) {
        int base_x = pit_cx + fvx * (perp_max + i);
        int base_y = pit_cy + fvy * (perp_max + i);
        for (int w = -1; w <= 1; ++w) {
            int wx = base_x + pvx * w;
            int wy = base_y + pvy * w;
            if (!in_bounds(wx, wy, map)) continue;
            if (map.fixture_id(wx, wy) >= 0 && w != 0) {
                map.remove_fixture(wx, wy);
            }
            if (map.get(wx, wy) == Tile::Wall) {
                map.set(wx, wy, Tile::Floor);
            }
        }
    }

    // Place spec fixtures (Crate, Crate, Conduit) inside the pit.
    // Build a small rect over the pit's interior in world coords.
    int pit_min_x = pit_cx - std::max(std::abs(pvx * par_min), std::abs(fvx * perp_min)) - 1;
    int pit_min_y = pit_cy - std::max(std::abs(pvy * par_min), std::abs(fvy * perp_min)) - 1;
    place_fixtures_in_rect(map,
        Rect{pit_min_x, pit_min_y, kPitParallel + 2, kPitPerpendicular + 2},
        spec_abandoned_mine().fixtures, rng);

    // Debris near the entrance (front opening, outward side).
    {
        int dbx = pit_cx + fvx * (perp_max + 1);
        int dby = pit_cy + fvy * (perp_max + 1);
        place_debris_around(map, dbx, dby, 3, 3, 5, rng);
    }
}

// --- Ancient excavation stamp ---

void stamp_ancient_excavation(TileMap& map, const Rect& foot,
                               std::mt19937& rng) {
    std::uniform_real_distribution<float> prob(0.0f, 1.0f);

    int cx = foot.x + foot.w / 2;
    int cy = foot.y + foot.h / 2;

    // Stone plaza: 14x10 outline centered.
    int plaza_w = 14;
    int plaza_h = 10;
    int plaza_x = cx - plaza_w / 2;
    int plaza_y = cy - plaza_h / 2;

    for (int y = plaza_y; y < plaza_y + plaza_h; ++y) {
        for (int x = plaza_x; x < plaza_x + plaza_w; ++x) {
            if (!in_bounds(x, y, map)) continue;
            if (map.fixture_id(x, y) >= 0) map.remove_fixture(x, y);
            bool edge = (x == plaza_x || x == plaza_x + plaza_w - 1 ||
                         y == plaza_y || y == plaza_y + plaza_h - 1);
            if (edge) {
                // 70% wall coverage for weathered look
                if (prob(rng) < 0.70f) {
                    map.set(x, y, Tile::StructuralWall);
                    map.set_glyph_override(x, y, 1);  // stone
                } else {
                    map.set(x, y, Tile::IndoorFloor);
                }
            } else {
                map.set(x, y, Tile::IndoorFloor);
            }
        }
    }

    // Four corner pillar fragments: 2x2 StructuralWall clusters at each corner.
    const int corner_positions[4][2] = {
        {plaza_x - 1, plaza_y - 1},
        {plaza_x + plaza_w - 1, plaza_y - 1},
        {plaza_x - 1, plaza_y + plaza_h - 1},
        {plaza_x + plaza_w - 1, plaza_y + plaza_h - 1},
    };
    for (const auto& cp : corner_positions) {
        for (int dy = 0; dy < 2; ++dy) {
            for (int dx = 0; dx < 2; ++dx) {
                int px = cp[0] + dx, py = cp[1] + dy;
                if (!in_bounds(px, py, map)) continue;
                if (prob(rng) < 0.70f) {
                    if (map.fixture_id(px, py) >= 0) map.remove_fixture(px, py);
                    map.set(px, py, Tile::StructuralWall);
                    map.set_glyph_override(px, py, 1);  // stone
                }
            }
        }
    }

    // Central pit: 5x3 cutout where stone steps descend.
    int pit_w = 5;
    int pit_h = 3;
    int pit_x = cx - pit_w / 2;
    int pit_y = cy - pit_h / 2;
    for (int y = pit_y; y < pit_y + pit_h; ++y) {
        for (int x = pit_x; x < pit_x + pit_w; ++x) {
            if (!in_bounds(x, y, map)) continue;
            if (map.fixture_id(x, y) >= 0) map.remove_fixture(x, y);
            map.set(x, y, Tile::IndoorFloor);
        }
    }

    // Portal at the center of the pit.
    clear_fixture_and_set(map, cx, cy, Tile::Portal);

    // Place spec fixtures (Console + BookCabinet) inside the plaza.
    place_fixtures_in_rect(map, Rect{plaza_x + 1, plaza_y + 1,
                                      plaza_w - 2, plaza_h - 2},
                           spec_ancient_excavation().fixtures, rng);

    // Debris inside the plaza.
    place_debris_around(map, cx, cy, 5, 2, 4, rng);

    // Boulders scattered outside the plaza.
    std::uniform_int_distribution<int> boulder_count(6, 10);
    int num_boulders = boulder_count(rng);
    std::uniform_int_distribution<int> bo_x(foot.x + 1, foot.x + foot.w - 2);
    std::uniform_int_distribution<int> bo_y(foot.y + 1, foot.y + foot.h - 2);
    for (int i = 0; i < num_boulders; ++i) {
        int bx = bo_x(rng);
        int by = bo_y(rng);
        // Skip if inside the plaza bounds.
        if (bx >= plaza_x - 2 && bx < plaza_x + plaza_w + 2 &&
            by >= plaza_y - 2 && by < plaza_y + plaza_h + 2) continue;
        if (!in_bounds(bx, by, map)) continue;
        if (map.get(bx, by) != Tile::Floor) continue;
        if (map.fixture_id(bx, by) >= 0) map.remove_fixture(bx, by);
        map.set(bx, by, Tile::Wall);
    }
}

} // anonymous namespace

// ===========================================================================
// CaveEntranceGenerator::generate
// ===========================================================================

Rect CaveEntranceGenerator::generate(TileMap& map,
                                      const TerrainChannels& channels,
                                      const MapProperties& props,
                                      std::mt19937& rng) const {
    // 1. Pick variant.
    CaveVariant variant;
    if (props.detail_cave_variant == "natural") {
        variant = CaveVariant::NaturalCave;
    } else if (props.detail_cave_variant == "mine") {
        variant = CaveVariant::AbandonedMine;
    } else if (props.detail_cave_variant == "excavation") {
        variant = CaveVariant::AncientExcavation;
    } else {
        variant = pick_cave_variant(props.lore_tier, props.biome, rng);
    }
    if (variant == CaveVariant::None) return {};

    const CaveVariantSpec& spec = spec_for(variant);

    if (spec.requires_cliff) {
        // Cliff-embedded variants: scan the entire map for cliff edges,
        // not a PlacementScorer footprint (PlacementScorer rejects walls
        // and would never find a cliff-adjacent spot).
        auto cliff = find_cliff_edge_global(map, spec.foot_w, spec.foot_h, rng);
        if (!cliff.has_value()) return {};

        // Derive a footprint rect around the cliff hit for the return value.
        int cx = cliff->wall_x;
        int cy = cliff->wall_y;
        Rect foot{
            std::max(0, cx - spec.foot_w / 2),
            std::max(0, cy - spec.foot_h / 2),
            spec.foot_w,
            spec.foot_h
        };
        if (foot.x + foot.w > map.width())  foot.w = map.width()  - foot.x;
        if (foot.y + foot.h > map.height()) foot.h = map.height() - foot.y;

        switch (variant) {
            case CaveVariant::NaturalCave:
                stamp_natural_cave(map, foot, *cliff, rng);
                break;
            case CaveVariant::AbandonedMine:
                stamp_abandoned_mine(map, foot, *cliff, rng);
                break;
            default:
                return {};
        }
        return foot;
    }

    // Non-cliff variants (ancient excavation): use PlacementScorer as normal.
    PlacementScorer scorer;
    auto placement = scorer.score(channels, map, spec.foot_w, spec.foot_h);
    if (!placement.valid) return {};

    stamp_ancient_excavation(map, placement.footprint, rng);
    return placement.footprint;
}

} // namespace astra
