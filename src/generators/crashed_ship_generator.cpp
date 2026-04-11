#include "astra/crashed_ship_generator.h"

#include "astra/crashed_ship_types.h"
#include "astra/noise.h"
#include "astra/placement_scorer.h"
#include "astra/poi_placement.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace astra {

namespace {

// --- Helpers ---

bool in_bounds(int x, int y, const TileMap& map) {
    return x >= 0 && x < map.width() && y >= 0 && y < map.height();
}

std::pair<int, int> rotate(int dx, int dy, ShipOrientation o) {
    switch (o) {
        case ShipOrientation::East:  return { dx,  dy};
        case ShipOrientation::West:  return {-dx,  dy};
        case ShipOrientation::South: return {-dy,  dx};
        case ShipOrientation::North: return { dy, -dx};
    }
    return {dx, dy};
}

// --- Per-class specs (ship-local, east-facing frame) ---

const ShipClassSpec& spec_escape_pod() {
    static const ShipClassSpec s{
        /*class_id*/ ShipClass::EscapePod,
        /*name*/ "EscapePod",
        /*hull_len*/ 10,
        /*body_half_h*/ 3,
        /*nose_taper_len*/ 0,   // flat blunt nose
        /*stern_taper*/ 0,
        /*hull_coverage*/ 0.75f,
        /*rooms*/ {{-5, 4}},
        /*bulkhead_dx*/ {},
        /*nacelle_len*/ 1,      // small 1-tile engine stubs
        /*wing_span*/ 0,        // no wings (too small)
        /*wing_width*/ 0,
        /*skid_min*/ 14, /*skid_max*/ 18,
        /*debris_radius*/ 8,
        /*debris_min*/ 3, /*debris_max*/ 5,
        /*breach_min*/ 2, /*breach_max*/ 2,
        /*fixtures_by_room*/ {
            {FixtureType::Console, FixtureType::Bunk, FixtureType::Crate},
        },
    };
    return s;
}

const ShipClassSpec& spec_freighter() {
    static const ShipClassSpec s{
        /*class_id*/ ShipClass::Freighter,
        /*name*/ "Freighter",
        /*hull_len*/ 24,
        /*body_half_h*/ 4,
        /*nose_taper_len*/ 1,   // slightly rounded nose
        /*stern_taper*/ 0,
        /*hull_coverage*/ 0.75f,
        /*rooms*/ {
            {-12, -5},  // engine bay
            { -3,  3},  // cargo hold
            {  5, 11},  // cockpit
        },
        /*bulkhead_dx*/ {-4, 4},
        /*nacelle_len*/ 2,      // 2-tile engine nacelles
        /*wing_span*/ 1,        // 1-tile wing stubs
        /*wing_width*/ 2,       // 2 tiles wide along dx
        /*skid_min*/ 22, /*skid_max*/ 28,
        /*debris_radius*/ 14,
        /*debris_min*/ 8, /*debris_max*/ 15,
        /*breach_min*/ 3, /*breach_max*/ 4,
        /*fixtures_by_room*/ {
            {FixtureType::Conduit, FixtureType::Rack},
            {FixtureType::Crate, FixtureType::Crate, FixtureType::Crate,
             FixtureType::Crate, FixtureType::Crate},
            {FixtureType::Console},
        },
    };
    return s;
}

const ShipClassSpec& spec_corvette() {
    static const ShipClassSpec s{
        /*class_id*/ ShipClass::Corvette,
        /*name*/ "Corvette",
        /*hull_len*/ 36,
        /*body_half_h*/ 5,
        /*nose_taper_len*/ 1,   // barely tapered nose, blunt corvette
        /*stern_taper*/ 0,
        /*hull_coverage*/ 0.75f,
        /*rooms*/ {
            {-18, -12},  // engine bay
            {-10,  -5},  // cargo hold
            { -3,   1},  // mess
            {  3,   9},  // quarters
            { 11,  17},  // cockpit
        },
        /*bulkhead_dx*/ {-11, -4, 2, 10},
        /*nacelle_len*/ 2,      // 2-tile engine nacelles
        /*wing_span*/ 2,        // 2-tile wing extensions
        /*wing_width*/ 3,       // 3 tiles wide along dx
        /*skid_min*/ 32, /*skid_max*/ 40,
        /*debris_radius*/ 20,
        /*debris_min*/ 12, /*debris_max*/ 20,
        /*breach_min*/ 4, /*breach_max*/ 5,
        /*fixtures_by_room*/ {
            {FixtureType::Conduit, FixtureType::Rack},
            {FixtureType::Crate, FixtureType::Crate, FixtureType::Crate,
             FixtureType::Crate, FixtureType::Crate},
            {FixtureType::Table, FixtureType::Bench, FixtureType::Bench},
            {FixtureType::Bunk, FixtureType::Bunk, FixtureType::Bunk},
            {FixtureType::Console, FixtureType::Console},
        },
    };
    return s;
}

const ShipClassSpec& spec_for(ShipClass c) {
    switch (c) {
        case ShipClass::EscapePod: return spec_escape_pod();
        case ShipClass::Freighter: return spec_freighter();
        case ShipClass::Corvette:  return spec_corvette();
    }
    return spec_freighter();
}

// --- Class selection ---

ShipClass pick_ship_class(int lore_tier, std::mt19937& rng) {
    int r = static_cast<int>(rng() % 100);
    if (lore_tier <= 0) {
        if (r < 70) return ShipClass::EscapePod;
        if (r < 95) return ShipClass::Freighter;
        return ShipClass::Corvette;
    }
    if (lore_tier == 1) {
        if (r < 30) return ShipClass::EscapePod;
        if (r < 85) return ShipClass::Freighter;
        return ShipClass::Corvette;
    }
    if (r < 10) return ShipClass::EscapePod;
    if (r < 50) return ShipClass::Freighter;
    return ShipClass::Corvette;
}

ShipOrientation pick_orientation(std::mt19937& rng) {
    switch (rng() % 4) {
        case 0: return ShipOrientation::East;
        case 1: return ShipOrientation::West;
        case 2: return ShipOrientation::South;
        default: return ShipOrientation::North;
    }
}

// --- Hull geometry ---

int hull_half_h(const ShipClassSpec& spec, int dx) {
    int half = spec.hull_len / 2;
    int body = spec.body_half_h;

    // Nose taper (positive dx end)
    int nose_start = half - 1 - spec.nose_taper_len;  // last "full" dx
    if (dx > nose_start) {
        int shrink = dx - nose_start;
        return std::max(1, body - shrink);
    }
    // Stern taper (negative dx end): narrows over the last 2 tiles by stern_taper
    int stern_start = -half + 1;  // first "full" dx after stern
    if (spec.stern_taper > 0 && dx < stern_start) {
        return std::max(1, body - spec.stern_taper);
    }
    return body;
}

// Is (dx, dy) inside the hull bounding shape (ship-local)?
bool inside_hull(const ShipClassSpec& spec, int dx, int dy) {
    int half = spec.hull_len / 2;
    if (dx < -half || dx > half - 1) return false;
    int hh = hull_half_h(spec, dx);
    return dy >= -hh && dy <= hh;
}

// Is (dx, dy) on the hull edge (walls go here)?
bool on_hull_edge(const ShipClassSpec& spec, int dx, int dy) {
    if (!inside_hull(spec, dx, dy)) return false;
    int half = spec.hull_len / 2;
    int hh = hull_half_h(spec, dx);
    bool axial_edge = (dx == -half || dx == half - 1);
    bool lateral_edge = (dy == -hh || dy == hh);
    return axial_edge || lateral_edge;
}

// --- Footprint size helper (pre-rotation: axis length × perp width) ---

struct FootprintSize { int axis, perp; };

FootprintSize footprint_for(const ShipClassSpec& spec) {
    int axis = spec.hull_len + spec.skid_max;
    int perp = (spec.body_half_h * 2 + 1) + 12;  // 6 tile margin each side
    return {axis, perp};
}

// Rotate footprint size to world dimensions based on orientation.
std::pair<int, int> rotate_footprint(FootprintSize fs, ShipOrientation o) {
    switch (o) {
        case ShipOrientation::East:
        case ShipOrientation::West:
            return {fs.axis, fs.perp};
        case ShipOrientation::South:
        case ShipOrientation::North:
            return {fs.perp, fs.axis};
    }
    return {fs.axis, fs.perp};
}

// Compute the hull center (world coords) inside the placement footprint so
// that the skid fits fully behind the stern. The ship-local stern sits at
// dx = -hull_len/2 and the skid extends another skid_max tiles in that
// direction, so we place the hull center offset from the footprint edge
// opposite the stern direction.
std::pair<int, int> compute_center(const Rect& foot,
                                    const ShipClassSpec& spec,
                                    ShipOrientation o) {
    int half = spec.hull_len / 2;
    int axis_offset = spec.skid_max + half;  // distance from the stern-side footprint edge
    int perp_center = 0;  // 0 = perp-center relative to footprint center

    switch (o) {
        case ShipOrientation::East:
            // Stern points west → stern at foot.x, center at foot.x + axis_offset
            return {foot.x + axis_offset, foot.y + foot.h / 2 + perp_center};
        case ShipOrientation::West:
            // Stern points east → stern at foot.x + foot.w - 1, center at foot.x + foot.w - 1 - axis_offset
            return {foot.x + foot.w - 1 - axis_offset, foot.y + foot.h / 2 + perp_center};
        case ShipOrientation::South:
            // Stern points north → stern at foot.y, center at foot.y + axis_offset
            return {foot.x + foot.w / 2 + perp_center, foot.y + axis_offset};
        case ShipOrientation::North:
            // Stern points south → stern at foot.y + foot.h - 1, center at foot.y + foot.h - 1 - axis_offset
            return {foot.x + foot.w / 2 + perp_center, foot.y + foot.h - 1 - axis_offset};
    }
    return {foot.x + foot.w / 2, foot.y + foot.h / 2};
}

// --- Stamping helpers ---

void clear_fixture_and_set(TileMap& map, int x, int y, Tile t) {
    if (!in_bounds(x, y, map)) return;
    if (map.fixture_id(x, y) >= 0) map.remove_fixture(x, y);
    map.set(x, y, t);
}

// --- Stamp skid mark ---

void stamp_skid(TileMap& map, int center_x, int center_y,
                const ShipClassSpec& spec, ShipOrientation o,
                std::mt19937& rng) {
    std::uniform_int_distribution<int> len_dist(spec.skid_min, spec.skid_max);
    int skid_length = len_dist(rng);

    int half = spec.hull_len / 2;
    unsigned noise_seed = rng();
    std::uniform_real_distribution<float> prob(0.0f, 1.0f);

    // Walk ship-local dx from -half - skid_length up to -half - 1 (stern).
    // step index i goes 0..skid_length-1 for noise continuity.
    for (int i = 0; i < skid_length; ++i) {
        int dx = -half - skid_length + i;

        float n = fbm(static_cast<float>(i) * 0.15f, 0.0f, noise_seed, 1.0f, 2);
        int perp_offset = static_cast<int>(std::round((n - 0.5f) * 4.0f));  // ~-2..+2

        for (int dy_local = -1; dy_local <= 1; ++dy_local) {
            int dy = dy_local + perp_offset;
            auto [rx, ry] = rotate(dx, dy, o);
            int wx = center_x + rx;
            int wy = center_y + ry;
            if (!in_bounds(wx, wy, map)) continue;

            // Clear any scatter in this tile regardless of whether we stamp.
            if (map.fixture_id(wx, wy) >= 0) map.remove_fixture(wx, wy);

            // Center row always scorched; side rows with 80% probability
            // (gives the skid visible gaps).
            bool stamp_tile = (dy_local == 0) || (prob(rng) < 0.80f);
            if (!stamp_tile) continue;

            Tile current = map.get(wx, wy);
            if (current == Tile::Water) continue;
            map.set(wx, wy, Tile::IndoorFloor);
        }

        // Flanking rubble fragments outside the 3-wide band (30% chance).
        for (int side : {-2, 2}) {
            int dy = side + perp_offset;
            auto [rx, ry] = rotate(dx, dy, o);
            int wx = center_x + rx;
            int wy = center_y + ry;
            if (!in_bounds(wx, wy, map)) continue;
            if (prob(rng) < 0.30f) {
                if (map.fixture_id(wx, wy) >= 0) map.remove_fixture(wx, wy);
                Tile current = map.get(wx, wy);
                if (current != Tile::Water) {
                    map.set(wx, wy, Tile::Wall);
                }
            }
        }
    }
}

// --- Stamp hull ---

void stamp_hull(TileMap& map, int center_x, int center_y,
                const ShipClassSpec& spec, ShipOrientation o,
                std::mt19937& rng) {
    std::uniform_real_distribution<float> prob(0.0f, 1.0f);
    int half = spec.hull_len / 2;

    for (int dx = -half; dx <= half - 1; ++dx) {
        int hh = hull_half_h(spec, dx);
        for (int dy = -hh; dy <= hh; ++dy) {
            auto [rx, ry] = rotate(dx, dy, o);
            int wx = center_x + rx;
            int wy = center_y + ry;
            if (!in_bounds(wx, wy, map)) continue;

            // Clear whatever scatter was on this tile.
            if (map.fixture_id(wx, wy) >= 0) map.remove_fixture(wx, wy);

            if (on_hull_edge(spec, dx, dy)) {
                if (prob(rng) < spec.hull_coverage) {
                    map.set(wx, wy, Tile::StructuralWall);
                    map.set_glyph_override(wx, wy, 0);  // metal
                } else {
                    map.set(wx, wy, Tile::IndoorFloor);  // breached plating
                }
            } else {
                map.set(wx, wy, Tile::IndoorFloor);
            }
        }
    }
}

// --- Stamp bulkheads ---

void stamp_bulkheads(TileMap& map, int center_x, int center_y,
                     const ShipClassSpec& spec, ShipOrientation o,
                     std::mt19937& rng) {
    std::uniform_real_distribution<float> prob(0.0f, 1.0f);

    for (int bulk_dx : spec.bulkhead_dx) {
        int hh = hull_half_h(spec, bulk_dx);
        for (int dy = -(hh - 1); dy <= (hh - 1); ++dy) {
            if (dy == 0) continue;  // center gap for passage
            if (prob(rng) >= 0.60f) continue;  // partial coverage

            auto [rx, ry] = rotate(bulk_dx, dy, o);
            int wx = center_x + rx;
            int wy = center_y + ry;
            if (!in_bounds(wx, wy, map)) continue;
            if (map.fixture_id(wx, wy) >= 0) map.remove_fixture(wx, wy);
            map.set(wx, wy, Tile::StructuralWall);
            map.set_glyph_override(wx, wy, 0);
        }
    }
}

// --- Stamp breaches ---

void stamp_breaches(TileMap& map, int center_x, int center_y,
                    const ShipClassSpec& spec, ShipOrientation o,
                    std::mt19937& rng) {
    int half = spec.hull_len / 2;
    std::uniform_int_distribution<int> count_dist(spec.breach_min, spec.breach_max);
    std::uniform_int_distribution<int> dx_dist(-half + 2, half - 3);
    std::uniform_int_distribution<int> len_dist(2, 3);
    std::uniform_real_distribution<float> prob(0.0f, 1.0f);

    int num = count_dist(rng);
    for (int b = 0; b < num; ++b) {
        int dx_start = dx_dist(rng);
        int hh = hull_half_h(spec, dx_start);
        int dy = (prob(rng) < 0.5f) ? -hh : hh;
        int blen = len_dist(rng);

        for (int i = 0; i < blen; ++i) {
            int dx = dx_start + i;
            // Recompute hh in case the breach spans a taper.
            int hh_i = hull_half_h(spec, dx);
            int actual_dy = (dy > 0) ? hh_i : -hh_i;
            auto [rx, ry] = rotate(dx, actual_dy, o);
            int wx = center_x + rx;
            int wy = center_y + ry;
            if (!in_bounds(wx, wy, map)) continue;
            map.set(wx, wy, Tile::IndoorFloor);
        }
    }
}

// --- Stamp engine nacelles (behind the stern, top/bottom flanks) ---

void stamp_nacelles(TileMap& map, int center_x, int center_y,
                    const ShipClassSpec& spec, ShipOrientation o,
                    std::mt19937& rng) {
    if (spec.nacelle_len <= 0) return;
    int half = spec.hull_len / 2;
    std::uniform_real_distribution<float> prob(0.0f, 1.0f);

    // Nacelle top at dy = body_half_h, bottom at dy = -body_half_h.
    // Extends behind the stern: dx in [-half - nacelle_len, -half - 1].
    for (int flank : {-1, 1}) {
        int dy = flank * spec.body_half_h;
        for (int i = 0; i < spec.nacelle_len; ++i) {
            int dx = -half - 1 - i;
            auto [rx, ry] = rotate(dx, dy, o);
            int wx = center_x + rx;
            int wy = center_y + ry;
            if (!in_bounds(wx, wy, map)) continue;

            // Overwrite whatever's there (skid rubble, scatter) — nacelle
            // is intentional structure.
            if (map.fixture_id(wx, wy) >= 0) map.remove_fixture(wx, wy);
            // 80% coverage so nacelles look beat-up but still read as engines.
            if (prob(rng) < 0.80f) {
                map.set(wx, wy, Tile::StructuralWall);
                map.set_glyph_override(wx, wy, 0);  // metal
            } else {
                map.set(wx, wy, Tile::IndoorFloor);
            }
        }
    }
}

// --- Stamp mid-section wings (perpendicular extensions from hull flanks) ---

void stamp_wings(TileMap& map, int center_x, int center_y,
                 const ShipClassSpec& spec, ShipOrientation o,
                 std::mt19937& rng) {
    if (spec.wing_span <= 0 || spec.wing_width <= 0) return;
    std::uniform_real_distribution<float> prob(0.0f, 1.0f);

    // Wings span a few tiles along dx at the middle (dx = 0), extending
    // wing_span tiles perpendicular to the hull on the top and bottom edges.
    int wing_dx_start = -(spec.wing_width / 2);
    int wing_dx_end = wing_dx_start + spec.wing_width - 1;

    for (int flank : {-1, 1}) {
        // The wing's inner edge sits just outside the hull's body_half_h.
        int inner_dy = flank * (spec.body_half_h + 1);
        int outer_dy = flank * (spec.body_half_h + spec.wing_span);
        int dy_lo = std::min(inner_dy, outer_dy);
        int dy_hi = std::max(inner_dy, outer_dy);

        for (int dy = dy_lo; dy <= dy_hi; ++dy) {
            for (int dx = wing_dx_start; dx <= wing_dx_end; ++dx) {
                auto [rx, ry] = rotate(dx, dy, o);
                int wx = center_x + rx;
                int wy = center_y + ry;
                if (!in_bounds(wx, wy, map)) continue;

                if (map.fixture_id(wx, wy) >= 0) map.remove_fixture(wx, wy);
                // 85% coverage, gaps read as battle-damage.
                if (prob(rng) < 0.85f) {
                    map.set(wx, wy, Tile::StructuralWall);
                    map.set_glyph_override(wx, wy, 0);  // metal
                }
            }
        }
    }
}

// --- Stamp debris field ---

void stamp_debris(TileMap& map, int center_x, int center_y,
                  const ShipClassSpec& spec, ShipOrientation o,
                  std::mt19937& rng) {
    std::uniform_int_distribution<int> count_dist(spec.debris_min, spec.debris_max);
    std::uniform_int_distribution<int> off_dist(-spec.debris_radius, spec.debris_radius);
    std::uniform_int_distribution<int> size_dist(1, 2);
    std::uniform_real_distribution<float> prob(0.0f, 1.0f);

    int num = count_dist(rng);
    int half = spec.hull_len / 2;

    for (int i = 0; i < num; ++i) {
        // Pick a ship-local offset outside the hull.
        int local_dx = 0, local_dy = 0;
        int attempts = 0;
        bool ok = false;
        while (attempts < 10) {
            ++attempts;
            local_dx = off_dist(rng);
            local_dy = off_dist(rng);
            // Skip if inside the hull bounding box.
            if (local_dx >= -half && local_dx <= half - 1 &&
                std::abs(local_dy) <= spec.body_half_h) continue;
            // Skip if on the skid band (behind the stern, dy within 2 of axis).
            if (local_dx < -half && std::abs(local_dy) <= 2) continue;
            ok = true;
            break;
        }
        if (!ok) continue;

        int fs = size_dist(rng);
        for (int dy = local_dy; dy < local_dy + fs; ++dy) {
            for (int dx = local_dx; dx < local_dx + fs; ++dx) {
                if (prob(rng) >= 0.60f) continue;
                auto [rx, ry] = rotate(dx, dy, o);
                int wx = center_x + rx;
                int wy = center_y + ry;
                if (!in_bounds(wx, wy, map)) continue;
                Tile current = map.get(wx, wy);
                if (current == Tile::StructuralWall ||
                    current == Tile::IndoorFloor ||
                    current == Tile::Water) continue;
                if (map.fixture_id(wx, wy) >= 0) map.remove_fixture(wx, wy);
                map.set(wx, wy, Tile::Wall);
            }
        }
    }
}

// --- Place fixtures ---

void place_fixtures(TileMap& map, int center_x, int center_y,
                    const ShipClassSpec& spec, ShipOrientation o,
                    std::mt19937& rng) {
    for (size_t room_idx = 0; room_idx < spec.rooms.size(); ++room_idx) {
        const auto& room = spec.rooms[room_idx];
        const auto& fixtures = spec.fixtures_by_room[room_idx];

        // Gather interior floor tiles within this room's dx range.
        std::vector<std::pair<int,int>> candidates;
        for (int dx = room.dx_min; dx <= room.dx_max; ++dx) {
            int hh = hull_half_h(spec, dx);
            for (int dy = -(hh - 1); dy <= (hh - 1); ++dy) {
                auto [rx, ry] = rotate(dx, dy, o);
                int wx = center_x + rx;
                int wy = center_y + ry;
                if (!in_bounds(wx, wy, map)) continue;
                if (map.get(wx, wy) != Tile::IndoorFloor) continue;
                if (map.fixture_id(wx, wy) >= 0) continue;
                candidates.push_back({wx, wy});
            }
        }

        if (candidates.empty()) continue;

        // Shuffle candidates for random placement.
        std::shuffle(candidates.begin(), candidates.end(), rng);

        size_t placed = 0;
        for (FixtureType ftype : fixtures) {
            if (placed >= candidates.size()) break;
            // Some fixture counts are ranges in the spec (e.g. 3-5 crates).
            // We encode max counts in the vector, so just place up to
            // however many candidates exist.
            auto [wx, wy] = candidates[placed++];
            map.set(wx, wy, Tile::Fixture);
            map.add_fixture(wx, wy, make_fixture(ftype));
        }
    }
}

// --- Place dungeon portal (~20%) ---

void maybe_place_portal(TileMap& map, int center_x, int center_y,
                        const ShipClassSpec& spec, ShipOrientation o,
                        std::mt19937& rng) {
    if (rng() % 5 != 0) return;  // 20% chance
    if (spec.rooms.empty()) return;

    const auto& mid_room = spec.rooms[spec.rooms.size() / 2];
    // Find any interior floor tile in the mid room that isn't already a fixture.
    std::vector<std::pair<int,int>> candidates;
    for (int dx = mid_room.dx_min; dx <= mid_room.dx_max; ++dx) {
        int hh = hull_half_h(spec, dx);
        for (int dy = -(hh - 1); dy <= (hh - 1); ++dy) {
            auto [rx, ry] = rotate(dx, dy, o);
            int wx = center_x + rx;
            int wy = center_y + ry;
            if (!in_bounds(wx, wy, map)) continue;
            if (map.get(wx, wy) != Tile::IndoorFloor) continue;
            if (map.fixture_id(wx, wy) >= 0) continue;
            candidates.push_back({wx, wy});
        }
    }
    if (candidates.empty()) return;
    auto [wx, wy] = candidates[rng() % candidates.size()];
    map.set(wx, wy, Tile::Portal);
}

} // anonymous namespace

// ===========================================================================
// CrashedShipGenerator::generate
// ===========================================================================

Rect CrashedShipGenerator::generate(TileMap& map,
                                     const TerrainChannels& channels,
                                     const MapProperties& props,
                                     std::mt19937& rng) const {
    // 1. Skip Aquatic biome entirely.
    if (props.biome == Biome::Aquatic) return {};

    // 2. Pick ship class (anchor hint > dev override > lore-weighted).
    ShipClass klass;
    if (props.detail_poi_anchor.valid) {
        klass = props.detail_poi_anchor.ship_class;
    } else if (props.detail_crashed_ship_class == "pod") {
        klass = ShipClass::EscapePod;
    } else if (props.detail_crashed_ship_class == "freighter") {
        klass = ShipClass::Freighter;
    } else if (props.detail_crashed_ship_class == "corvette") {
        klass = ShipClass::Corvette;
    } else {
        klass = pick_ship_class(props.lore_tier, rng);
    }
    const ShipClassSpec& spec = spec_for(klass);

    // 3. Pick orientation.
    ShipOrientation orient = pick_orientation(rng);

    // 4. Compute footprint size for the placement scorer.
    FootprintSize fs = footprint_for(spec);
    auto [foot_w, foot_h] = rotate_footprint(fs, orient);

    // 5. Score placement.
    PlacementScorer scorer;
    auto placement = scorer.score(channels, map, foot_w, foot_h);
    if (!placement.valid) return {};

    // 6. Compute hull center within the footprint.
    auto [center_x, center_y] = compute_center(placement.footprint, spec, orient);

    // 7. Stamp skid mark first so the hull can overwrite the stern end.
    stamp_skid(map, center_x, center_y, spec, orient, rng);

    // 8. Stamp hull.
    stamp_hull(map, center_x, center_y, spec, orient, rng);

    // 9. Stamp bulkheads.
    stamp_bulkheads(map, center_x, center_y, spec, orient, rng);

    // 10. Stamp breaches.
    stamp_breaches(map, center_x, center_y, spec, orient, rng);

    // 10b. Stamp engine nacelles behind the stern.
    stamp_nacelles(map, center_x, center_y, spec, orient, rng);

    // 10c. Stamp mid-section wings.
    stamp_wings(map, center_x, center_y, spec, orient, rng);

    // 11. Stamp debris field.
    stamp_debris(map, center_x, center_y, spec, orient, rng);

    // 12. Place fixtures per room.
    place_fixtures(map, center_x, center_y, spec, orient, rng);

    // 13. Maybe place dungeon portal (~20%).
    maybe_place_portal(map, center_x, center_y, spec, orient, rng);

    return placement.footprint;
}

} // namespace astra
