#include "astra/dungeon_level_generator.h"

#include "astra/map_generator.h"
#include "astra/map_properties.h"
#include "astra/tilemap.h"

#include <random>
#include <utility>
#include <vector>

namespace astra {

namespace {

// Scan the fixture_ids grid for the first tile occupied by a fixture of
// the given type. Returns {-1,-1} when none is found.
std::pair<int, int> find_fixture_xy(const TileMap& m, FixtureType type) {
    const auto& ids = m.fixture_ids();
    const int w = m.width();
    const int h = m.height();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int fid = ids[y * w + x];
            if (fid < 0) continue;
            if (m.fixture(fid).type == type) {
                return {x, y};
            }
        }
    }
    return {-1, -1};
}

// Collect every passable tile in the given region. If no region matches or
// none of its tiles are passable, returns empty.
std::vector<std::pair<int, int>> collect_region_open(const TileMap& m, int rid) {
    std::vector<std::pair<int, int>> out;
    if (rid < 0 || rid >= m.region_count()) return out;
    const int w = m.width();
    const int h = m.height();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (m.region_id(x, y) != rid) continue;
            if (!m.passable(x, y)) continue;
            if (m.fixture_ids()[y * w + x] >= 0) continue;
            out.emplace_back(x, y);
        }
    }
    return out;
}

// Pick the geometric centroid of a region's passable cells; falls back to
// the first passable cell.
bool region_centroid(const TileMap& m, int rid, int& out_x, int& out_y) {
    auto cells = collect_region_open(m, rid);
    if (cells.empty()) return false;
    long sx = 0, sy = 0;
    for (const auto& p : cells) {
        sx += p.first;
        sy += p.second;
    }
    int cx = static_cast<int>(sx / static_cast<long>(cells.size()));
    int cy = static_cast<int>(sy / static_cast<long>(cells.size()));
    // Use centroid if it is actually a member of the region; otherwise
    // fall back to the first collected cell.
    if (m.region_id(cx, cy) == rid && m.passable(cx, cy) &&
        m.fixture_ids()[cy * m.width() + cx] < 0) {
        out_x = cx;
        out_y = cy;
        return true;
    }
    out_x = cells.front().first;
    out_y = cells.front().second;
    return true;
}

void place_planned_fixtures(TileMap& map,
                            const DungeonLevelSpec& spec,
                            std::mt19937& rng,
                            int entry_rid,
                            int exit_rid) {
    for (const auto& pf : spec.fixtures) {
        int fx = -1, fy = -1;

        if (pf.placement_hint == "back_chamber") {
            // Walk deepest rooms first, skipping entry/exit rooms when possible.
            for (int r = map.region_count() - 1; r >= 0 && fx < 0; --r) {
                if (map.region(r).type != RegionType::Room) continue;
                if (r == entry_rid || r == exit_rid) continue;
                auto cells = collect_region_open(map, r);
                if (cells.empty()) continue;
                std::uniform_int_distribution<size_t> d(0, cells.size() - 1);
                auto p = cells[d(rng)];
                fx = p.first;
                fy = p.second;
            }
        } else if (pf.placement_hint == "center") {
            // Find the region nearest the map center.
            int cx = map.width() / 2;
            int cy = map.height() / 2;
            int center_rid = map.region_id(cx, cy);
            if (center_rid >= 0) {
                int rx = 0, ry = 0;
                if (map.find_open_spot_in_region(center_rid, rx, ry, {}, &rng)) {
                    fx = rx;
                    fy = ry;
                }
            }
        }

        // Fallback: any passable, fixture-free tile.
        if (fx < 0) {
            std::vector<std::pair<int, int>> open;
            const auto& ids = map.fixture_ids();
            const int w = map.width();
            const int h = map.height();
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    if (!map.passable(x, y)) continue;
                    if (ids[y * w + x] >= 0) continue;
                    open.emplace_back(x, y);
                }
            }
            if (open.empty()) continue;
            std::uniform_int_distribution<size_t> d(0, open.size() - 1);
            auto p = open[d(rng)];
            fx = p.first;
            fy = p.second;
        }

        FixtureData fd;
        fd.type = FixtureType::QuestFixture;
        fd.interactable = true;
        fd.cooldown = -1;           // one-shot by default
        fd.quest_fixture_id = pf.quest_fixture_id;
        map.add_fixture(fx, fy, fd);
    }
}

} // namespace

uint32_t dungeon_level_seed(uint32_t world_seed, const LocationKey& k) {
    uint32_t s = world_seed;
    s ^= static_cast<uint32_t>(std::get<0>(k)) * 73856093u;
    s ^= static_cast<uint32_t>(std::get<1>(k) + 1) * 19349663u;
    s ^= static_cast<uint32_t>(std::get<2>(k) + 1) * 83492791u;
    s ^= static_cast<uint32_t>(std::get<4>(k) + 1) * 2654435761u;
    s ^= static_cast<uint32_t>(std::get<5>(k) + 1) * 40503u;
    s ^= static_cast<uint32_t>(std::get<6>(k)) * 6271u;   // depth
    return s;
}

std::pair<int, int> find_stairs_up(const TileMap& m) {
    return find_fixture_xy(m, FixtureType::StairsUp);
}

std::pair<int, int> find_stairs_down(const TileMap& m) {
    return find_fixture_xy(m, FixtureType::DungeonHatch);
}

void generate_dungeon_level(TileMap& map,
                            const DungeonRecipe& recipe,
                            int depth,
                            uint32_t seed,
                            std::pair<int, int> entered_from) {
    if (depth < 1 || depth > static_cast<int>(recipe.levels.size())) return;
    const auto& spec = recipe.levels[depth - 1];

    // The concrete ruin-style dungeon map type used throughout the codebase
    // for Precursor / ancient-civ interiors is DerelictStation. See
    // Game::enter_dungeon_from_detail for the reference dispatch.
    //
    // NOTE: MapProperties does NOT carry civ_name / decay_level fields —
    // those are consumed by RuinGenerator::generate() directly, which is
    // not reachable through the create_generator() factory. For this
    // entry point we rely on biome (Corroded) to theme the level; a
    // future pass should route spec.civ_name / spec.decay_level through
    // a dedicated factory overload.
    const MapType dtype = MapType::DerelictStation;
    MapProperties props = default_properties(dtype);
    props.biome = Biome::Corroded;
    props.difficulty = std::max(1, spec.enemy_tier);

    // Recreate the underlying grid at the requested size. Matches the
    // pattern in game_world.cpp (create TileMap, then run generator).
    map = TileMap(props.width, props.height, dtype);

    auto gen = create_generator(dtype);
    gen->generate(map, props, seed);

    // ---- StairsUp ----
    // Preferred: the tile the player descended from, if still passable
    // and unoccupied by a fixture.
    int ux = -1, uy = -1;
    const auto& ids = map.fixture_ids();
    auto open_at = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < map.width() && y < map.height() &&
               map.passable(x, y) &&
               ids[y * map.width() + x] < 0;
    };

    if (entered_from.first >= 0 && entered_from.second >= 0 &&
        open_at(entered_from.first, entered_from.second)) {
        ux = entered_from.first;
        uy = entered_from.second;
    } else {
        // Fallback: first real room region.
        for (int r = 0; r < map.region_count() && ux < 0; ++r) {
            if (map.region(r).type != RegionType::Room) continue;
            int rx = 0, ry = 0;
            std::mt19937 pick_rng(seed ^ 0xD00Du);
            if (map.find_open_spot_in_region(r, rx, ry, {}, &pick_rng)) {
                ux = rx;
                uy = ry;
            }
        }
    }
    if (ux < 0) {
        // Last-ditch: map-wide centroid search.
        int rx = 0, ry = 0;
        map.find_open_spot(rx, ry);
        ux = rx;
        uy = ry;
    }
    {
        FixtureData f;
        f.type = FixtureType::StairsUp;
        f.interactable = true;
        f.cooldown = 0;
        map.add_fixture(ux, uy, f);
    }

    int entry_rid = map.region_id(ux, uy);

    // ---- StairsDown ----
    // Suppressed on boss levels. Otherwise place in the last room region
    // that is distinct from the entry region.
    int exit_rid = -1;
    if (!spec.is_boss_level) {
        for (int r = map.region_count() - 1; r >= 0 && exit_rid < 0; --r) {
            if (map.region(r).type != RegionType::Room) continue;
            if (r == entry_rid) continue;
            int rx = 0, ry = 0;
            std::mt19937 pick_rng(seed ^ 0xB007u);
            std::vector<std::pair<int, int>> exclude{{ux, uy}};
            if (map.find_open_spot_in_region(r, rx, ry, exclude, &pick_rng)) {
                FixtureData f;
                f.type = FixtureType::DungeonHatch;
                f.interactable = true;
                f.cooldown = 0;
                map.add_fixture(rx, ry, f);
                exit_rid = r;
            }
        }
        // If we couldn't find a distinct room (very small map), fall back
        // to any open tile that isn't the stairs-up cell.
        if (exit_rid < 0) {
            int cx = 0, cy = 0;
            if (map.find_open_spot_other_room(ux, uy, cx, cy)) {
                FixtureData f;
                f.type = FixtureType::DungeonHatch;
                f.interactable = true;
                f.cooldown = 0;
                map.add_fixture(cx, cy, f);
                exit_rid = map.region_id(cx, cy);
            }
        }
    }

    // ---- Planned fixtures ----
    std::mt19937 fix_rng(seed ^ 0xA5A5u);
    place_planned_fixtures(map, spec, fix_rng, entry_rid, exit_rid);

    // NPC spawning is caller-owned (see task header).
    (void)recipe; // recipe already consumed via `spec`
}

} // namespace astra
