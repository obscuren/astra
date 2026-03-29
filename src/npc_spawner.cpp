#include "astra/npc_spawner.h"
#include "astra/npc_defs.h"
#include "astra/player.h"

namespace astra {

void spawn_hub_npcs(TileMap& map, std::vector<Npc>& npcs,
                    int player_x, int player_y, std::mt19937& rng,
                    const Player* player) {
    int kreth_rep = player ? reputation_for(*player, "Kreth Mining Guild") : 0;
    std::vector<std::pair<int,int>> occupied = {{player_x, player_y}};

    auto place_npc = [&](Npc npc, int region_id) {
        if (map.find_open_spot_in_region(region_id, npc.x, npc.y, occupied, &rng)) {
            occupied.push_back({npc.x, npc.y});
            npcs.push_back(std::move(npc));
        }
    };

    // Races for NPC variety
    static constexpr Race friendly_races[] = {
        Race::Human, Race::Veldrani, Race::Kreth, Race::Sylphari, Race::Stellari
    };
    auto pick_race = [&]() -> Race {
        std::uniform_int_distribution<int> dist(0, 3); // exclude Stellari for most
        return friendly_races[dist(rng)];
    };

    // Walk through all regions and spawn NPCs based on flavor
    int region_count = map.region_count();
    for (int rid = 0; rid < region_count; ++rid) {
        const auto& reg = map.region(rid);
        if (reg.type != RegionType::Room) continue;

        switch (reg.flavor) {
            case RoomFlavor::EmptyRoom: {
                // Docking Bay: Station Keeper
                place_npc(build_station_keeper(Race::Human, rng), rid);
                break;
            }
            case RoomFlavor::Cantina: {
                place_npc(build_food_merchant(pick_race(), rng, kreth_rep), rid);
                // Also a drifter hanging around
                place_npc(build_drifter(Race::Sylphari, rng), rid);
                break;
            }
            case RoomFlavor::Medbay: {
                place_npc(build_medic(pick_race(), rng), rid);
                break;
            }
            case RoomFlavor::CommandCenter: {
                place_npc(build_commander(Race::Human, rng), rid);
                break;
            }
            case RoomFlavor::Armory: {
                place_npc(build_arms_dealer(Race::Kreth, rng, kreth_rep), rid);
                break;
            }
            case RoomFlavor::Observatory: {
                // Nova's room — she's always here
                place_npc(build_nova(), rid);
                place_npc(build_astronomer(Race::Stellari, rng), rid);
                break;
            }
            case RoomFlavor::Engineering: {
                place_npc(build_engineer(Race::Kreth, rng), rid);
                break;
            }
            case RoomFlavor::StorageBay:
            case RoomFlavor::CargoHold: {
                // Occasional drifter
                std::uniform_int_distribution<int> chance(0, 1);
                if (chance(rng) == 0) {
                    place_npc(build_drifter(pick_race(), rng), rid);
                }
                break;
            }
            case RoomFlavor::CrewQuarters: {
                // Occasional sleeping drifter
                std::uniform_int_distribution<int> chance(0, 2);
                if (chance(rng) == 0) {
                    place_npc(build_drifter(pick_race(), rng), rid);
                }
                break;
            }
            default:
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// Settlement / Outpost NPC spawning (fixture-scanning approach)
// ---------------------------------------------------------------------------

static bool find_floor_near(const TileMap& map, int fx, int fy,
                            int& out_x, int& out_y,
                            const std::vector<std::pair<int,int>>& occupied) {
    // Search tiles within radius 2 for a walkable indoor/outdoor floor
    for (int r = 1; r <= 2; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (dx == 0 && dy == 0) continue;
                int nx = fx + dx;
                int ny = fy + dy;
                if (nx < 0 || ny < 0 || nx >= map.width() || ny >= map.height())
                    continue;
                Tile t = map.get(nx, ny);
                if (t != Tile::IndoorFloor && t != Tile::Floor) continue;
                // Not occupied
                bool taken = false;
                for (auto& p : occupied) {
                    if (p.first == nx && p.second == ny) { taken = true; break; }
                }
                if (!taken) {
                    out_x = nx;
                    out_y = ny;
                    return true;
                }
            }
        }
    }
    return false;
}

// Scan the fixture grid for the first fixture of the given type.
// Returns (x, y) position or (-1, -1) if not found.
// `skip` lets us skip the first N matches to find subsequent fixtures of the same type.
static std::pair<int,int> find_fixture_pos(const TileMap& map, FixtureType type, int skip = 0) {
    const auto& ids = map.fixture_ids();
    const auto& fixtures = map.fixtures_vec();
    int w = map.width();
    int skipped = 0;
    for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
        if (ids[i] < 0) continue;
        if (fixtures[ids[i]].type == type) {
            if (skipped < skip) { ++skipped; continue; }
            return {i % w, i / w};
        }
    }
    return {-1, -1};
}

static Race pick_friendly_race(std::mt19937& rng) {
    static constexpr Race races[] = {
        Race::Human, Race::Veldrani, Race::Kreth, Race::Sylphari
    };
    std::uniform_int_distribution<int> dist(0, 3);
    return races[dist(rng)];
}

void spawn_settlement_npcs(TileMap& map, std::vector<Npc>& npcs,
                           int player_x, int player_y, std::mt19937& rng,
                           const Player* player) {
    int kreth_rep = player ? reputation_for(*player, "Kreth Mining Guild") : 0;
    std::vector<std::pair<int,int>> occupied = {{player_x, player_y}};

    auto place_near = [&](Npc npc, int fx, int fy) {
        if (find_floor_near(map, fx, fy, npc.x, npc.y, occupied)) {
            occupied.push_back({npc.x, npc.y});
            npcs.push_back(std::move(npc));
        }
    };

    // Console → Settlement Leader (Main Hall)
    auto [cx, cy] = find_fixture_pos(map, FixtureType::Console);
    if (cx >= 0)
        place_near(build_commander(pick_friendly_race(rng), rng), cx, cy);

    // Table → Merchant (Market)
    auto [tx, ty] = find_fixture_pos(map, FixtureType::Table);
    if (tx >= 0)
        place_near(build_merchant(pick_friendly_race(rng), rng, kreth_rep), tx, ty);

    // Crate → Arms Dealer or Food Merchant (50/50)
    auto [crx, cry] = find_fixture_pos(map, FixtureType::Crate);
    if (crx >= 0) {
        std::uniform_int_distribution<int> coin(0, 1);
        if (coin(rng) == 0)
            place_near(build_arms_dealer(pick_friendly_race(rng), rng, kreth_rep), crx, cry);
        else
            place_near(build_food_merchant(pick_friendly_race(rng), rng, kreth_rep), crx, cry);
    }

    // Bunk #1 → Resident
    auto [bx1, by1] = find_fixture_pos(map, FixtureType::Bunk, 0);
    if (bx1 >= 0)
        place_near(build_drifter(pick_friendly_race(rng), rng), bx1, by1);

    // Bunk #2 → 50% chance Resident
    auto [bx2, by2] = find_fixture_pos(map, FixtureType::Bunk, 1);
    if (bx2 >= 0) {
        std::uniform_int_distribution<int> chance(0, 1);
        if (chance(rng) == 0)
            place_near(build_drifter(pick_friendly_race(rng), rng), bx2, by2);
    }

    // Plaza wanderers (center of map)
    int pcx = map.width() / 2;
    int pcy = map.height() / 2;
    {
        Npc wanderer = build_drifter(pick_friendly_race(rng), rng);
        if (find_floor_near(map, pcx, pcy, wanderer.x, wanderer.y, occupied)) {
            occupied.push_back({wanderer.x, wanderer.y});
            npcs.push_back(std::move(wanderer));
        }
    }
    // 33% chance second plaza wanderer
    {
        std::uniform_int_distribution<int> chance(0, 2);
        if (chance(rng) == 0) {
            Npc wanderer = build_drifter(pick_friendly_race(rng), rng);
            if (find_floor_near(map, pcx + 2, pcy + 1, wanderer.x, wanderer.y, occupied)) {
                occupied.push_back({wanderer.x, wanderer.y});
                npcs.push_back(std::move(wanderer));
            }
        }
    }
}

void spawn_outpost_npcs(TileMap& map, std::vector<Npc>& npcs,
                        int player_x, int player_y, std::mt19937& rng,
                        const Player* player) {
    int kreth_rep = player ? reputation_for(*player, "Kreth Mining Guild") : 0;
    std::vector<std::pair<int,int>> occupied = {{player_x, player_y}};

    auto place_near = [&](Npc npc, int fx, int fy) {
        if (find_floor_near(map, fx, fy, npc.x, npc.y, occupied)) {
            occupied.push_back({npc.x, npc.y});
            npcs.push_back(std::move(npc));
        }
    };

    // Console → Outpost Commander (Main Building)
    auto [cx, cy] = find_fixture_pos(map, FixtureType::Console);
    if (cx >= 0)
        place_near(build_commander(pick_friendly_race(rng), rng), cx, cy);

    // Bunk → Guard (Main Building)
    auto [bx, by] = find_fixture_pos(map, FixtureType::Bunk);
    if (bx >= 0)
        place_near(build_drifter(pick_friendly_race(rng), rng), bx, by);

    // Crate → Quartermaster (Storage Shed)
    auto [crx, cry] = find_fixture_pos(map, FixtureType::Crate);
    if (crx >= 0)
        place_near(build_merchant(pick_friendly_race(rng), rng, kreth_rep), crx, cry);

    // Courtyard patrol
    int pcx = map.width() / 2;
    int pcy = map.height() / 2;
    {
        Npc patrol = build_drifter(pick_friendly_race(rng), rng);
        if (find_floor_near(map, pcx, pcy, patrol.x, patrol.y, occupied)) {
            occupied.push_back({patrol.x, patrol.y});
            npcs.push_back(std::move(patrol));
        }
    }
}

} // namespace astra
