#include "astra/npc_spawner.h"
#include "astra/npc_defs.h"
#include "astra/player.h"
#include "astra/faction.h"

#include <algorithm>

namespace astra {

// ---------------------------------------------------------------------------
// find_walkable_in_bounds — reusable NPC placement within a rect
// ---------------------------------------------------------------------------

bool find_walkable_in_bounds(const TileMap& map, const Rect& bounds,
                              int& out_x, int& out_y,
                              const std::vector<std::pair<int,int>>& occupied,
                              std::mt19937& rng) {
    std::vector<std::pair<int,int>> candidates;
    for (int y = bounds.y; y < bounds.y + bounds.h; ++y) {
        for (int x = bounds.x; x < bounds.x + bounds.w; ++x) {
            if (x < 0 || x >= map.width() || y < 0 || y >= map.height()) continue;
            if (!map.passable(x, y)) continue;
            bool taken = false;
            for (auto& [ox, oy] : occupied) {
                if (ox == x && oy == y) { taken = true; break; }
            }
            if (!taken) candidates.push_back({x, y});
        }
    }
    if (candidates.empty()) return false;
    std::uniform_int_distribution<int> pick(0, static_cast<int>(candidates.size()) - 1);
    auto [px, py] = candidates[pick(rng)];
    out_x = px;
    out_y = py;
    return true;
}

void spawn_hub_npcs(TileMap& map, std::vector<Npc>& npcs,
                    int player_x, int player_y, std::mt19937& rng,
                    const Player* player) {
    int kreth_rep = player ? reputation_for(*player, Faction_KrethMiningGuild) : 0;
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
                place_npc(build_merchant(pick_race(), rng, kreth_rep), rid);
                // Civilians and a drifter hanging around
                place_npc(build_hub_drifter(pick_race(), rng), rid);
                place_npc(build_random_hub_civilian(rng), rid);
                place_npc(build_random_hub_civilian(rng), rid);
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
                // Nova only appears at The Heavens Above (THA)
                if (map.is_tha()) {
                    place_npc(build_nova(), rid);
                }
                place_npc(build_astronomer(Race::Stellari, rng), rid);
                break;
            }
            case RoomFlavor::Engineering: {
                place_npc(build_engineer(Race::Kreth, rng), rid);
                break;
            }
            case RoomFlavor::StorageBay:
            case RoomFlavor::CargoHold: {
                // Occasional drifter or civilian
                std::uniform_int_distribution<int> chance(0, 2);
                if (chance(rng) == 0)
                    place_npc(build_hub_drifter(pick_race(), rng), rid);
                if (chance(rng) == 0)
                    place_npc(build_random_hub_civilian(rng), rid);
                break;
            }
            case RoomFlavor::CrewQuarters: {
                // Residents
                place_npc(build_random_hub_civilian(rng), rid);
                std::uniform_int_distribution<int> chance(0, 1);
                if (chance(rng) == 0)
                    place_npc(build_random_hub_civilian(rng), rid);
                break;
            }
            default:
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// Scav station NPC spawning
// ---------------------------------------------------------------------------

void spawn_scav_npcs(TileMap& map, std::vector<Npc>& npcs,
                     int player_x, int player_y, std::mt19937& rng,
                     const Player* /*player*/) {
    std::vector<std::pair<int,int>> occupied = {{player_x, player_y}};

    auto place_npc = [&](Npc npc, int region_id) {
        if (map.find_open_spot_in_region(region_id, npc.x, npc.y, occupied, &rng)) {
            occupied.push_back({npc.x, npc.y});
            npcs.push_back(std::move(npc));
        }
    };

    static constexpr Race friendly_races[] = {
        Race::Human, Race::Veldrani, Race::Kreth, Race::Sylphari,
    };
    auto pick_race = [&]() -> Race {
        std::uniform_int_distribution<int> dist(0, 3);
        return friendly_races[dist(rng)];
    };

    int region_count = map.region_count();
    bool keeper_placed    = false;
    bool dealer_placed    = false;

    for (int rid = 0; rid < region_count; ++rid) {
        const auto& reg = map.region(rid);
        if (reg.type != RegionType::Room) continue;

        switch (reg.flavor) {
            case RoomFlavor::EmptyRoom: {
                // Docking Bay: Scav Keeper (placed here if no dedicated nook yet)
                if (!keeper_placed) {
                    place_npc(build_scav_keeper(Race::Human, rng), rid);
                    keeper_placed = true;
                }
                break;
            }
            case RoomFlavor::Cantina: {
                // Mess Hall: civilians
                place_npc(build_civilian(pick_race(), rng), rid);
                std::uniform_int_distribution<int> chance(0, 1);
                if (chance(rng) == 0)
                    place_npc(build_civilian(pick_race(), rng), rid);
                break;
            }
            case RoomFlavor::MaintenanceAccess: {
                // Scrap Yard: Junk Dealer
                if (!dealer_placed) {
                    place_npc(build_scav_junk_dealer(pick_race(), rng), rid);
                    dealer_placed = true;
                }
                break;
            }
            case RoomFlavor::CrewQuarters: {
                // Keeper's Nook or Bunk Room: keeper goes in the first one found
                if (!keeper_placed) {
                    place_npc(build_scav_keeper(Race::Human, rng), rid);
                    keeper_placed = true;
                } else {
                    // Bunk Room: occasional civilian
                    std::uniform_int_distribution<int> chance(0, 1);
                    if (chance(rng) == 0)
                        place_npc(build_civilian(pick_race(), rng), rid);
                }
                break;
            }
            default:
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// Pirate station NPC spawning
// ---------------------------------------------------------------------------

void spawn_pirate_npcs(TileMap& map, std::vector<Npc>& npcs,
                       int player_x, int player_y, std::mt19937& rng,
                       const StationContext& ctx,
                       const Player* /*player*/) {
    std::vector<std::pair<int,int>> occupied = {{player_x, player_y}};

    auto place_npc = [&](Npc npc, int region_id) {
        if (map.find_open_spot_in_region(region_id, npc.x, npc.y, occupied, &rng)) {
            occupied.push_back({npc.x, npc.y});
            npcs.push_back(std::move(npc));
        }
    };

    bool captain_placed      = false;
    bool vendor_placed       = false;

    // Grunt count: 3–5, seeded from keeper_seed for determinism.
    std::mt19937 seed_rng(static_cast<uint32_t>(ctx.keeper_seed));
    std::uniform_int_distribution<int> grunt_count_dist(3, 5);
    int grunts_to_place = grunt_count_dist(seed_rng);
    int grunts_placed   = 0;

    int region_count = map.region_count();

    for (int rid = 0; rid < region_count; ++rid) {
        const auto& reg = map.region(rid);
        if (reg.type != RegionType::Room) continue;

        switch (reg.flavor) {
            case RoomFlavor::EmptyRoom: {
                // Docking Bay: pirate grunt on watch
                if (grunts_placed < grunts_to_place) {
                    place_npc(build_pirate_grunt(), rid);
                    ++grunts_placed;
                }
                break;
            }
            case RoomFlavor::Cantina: {
                // Pirate Den: 1–2 grunts lounging around
                for (int i = 0; i < 2 && grunts_placed < grunts_to_place; ++i) {
                    place_npc(build_pirate_grunt(), rid);
                    ++grunts_placed;
                }
                break;
            }
            case RoomFlavor::CrewQuarters: {
                if (reg.name == std::string("Captain's Quarters")) {
                    // Captain's Quarters: pirate captain
                    if (!captain_placed) {
                        place_npc(build_pirate_captain(ctx), rid);
                        captain_placed = true;
                    }
                } else {
                    // Brig: grunt guarding it
                    if (grunts_placed < grunts_to_place) {
                        place_npc(build_pirate_grunt(), rid);
                        ++grunts_placed;
                    }
                }
                break;
            }
            case RoomFlavor::MaintenanceAccess: {
                // Black Market: the Fixer
                if (!vendor_placed) {
                    place_npc(build_black_market_vendor(ctx), rid);
                    vendor_placed = true;
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
    // Search tiles within radius 5 for a walkable floor
    for (int r = 1; r <= 5; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (dx == 0 && dy == 0) continue;
                int nx = fx + dx;
                int ny = fy + dy;
                if (nx < 0 || ny < 0 || nx >= map.width() || ny >= map.height())
                    continue;
                Tile t = map.get(nx, ny);
                // Accept any walkable tile: floor, indoor floor, path, or
                // passable fixtures (furniture tiles set by add_fixture)
                bool walkable = (t == Tile::Floor || t == Tile::IndoorFloor
                              || t == Tile::Path);
                if (!walkable && t == Tile::Fixture) {
                    int fid = map.fixture_id(nx, ny);
                    walkable = (fid >= 0 && map.fixture(fid).passable);
                }
                if (!walkable) continue;
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
    int kreth_rep = player ? reputation_for(*player, Faction_KrethMiningGuild) : 0;
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

    // Bunk #1 → Resident civilian
    auto [bx1, by1] = find_fixture_pos(map, FixtureType::Bunk, 0);
    if (bx1 >= 0)
        place_near(build_random_civilian(rng), bx1, by1);

    // Bunk #2 → 50% chance Resident
    auto [bx2, by2] = find_fixture_pos(map, FixtureType::Bunk, 1);
    if (bx2 >= 0) {
        std::uniform_int_distribution<int> chance(0, 1);
        if (chance(rng) == 0)
            place_near(build_random_civilian(rng), bx2, by2);
    }

    // Plaza wanderers (center of map) — mix of civilians and drifters
    int pcx = map.width() / 2;
    int pcy = map.height() / 2;
    {
        Npc wanderer = build_random_civilian(rng);
        if (find_floor_near(map, pcx, pcy, wanderer.x, wanderer.y, occupied)) {
            occupied.push_back({wanderer.x, wanderer.y});
            npcs.push_back(std::move(wanderer));
        }
    }
    {
        Npc wanderer = build_drifter(pick_friendly_race(rng), rng);
        if (find_floor_near(map, pcx - 2, pcy + 1, wanderer.x, wanderer.y, occupied)) {
            occupied.push_back({wanderer.x, wanderer.y});
            npcs.push_back(std::move(wanderer));
        }
    }
    // 33% chance extra civilian
    {
        std::uniform_int_distribution<int> chance(0, 2);
        if (chance(rng) == 0) {
            Npc wanderer = build_random_civilian(rng);
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
    int kreth_rep = player ? reputation_for(*player, Faction_KrethMiningGuild) : 0;
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

    // Bunk → Guard or civilian
    auto [bx, by] = find_fixture_pos(map, FixtureType::Bunk);
    if (bx >= 0)
        place_near(build_random_civilian(rng), bx, by);

    // Crate → Quartermaster (Storage Shed)
    auto [crx, cry] = find_fixture_pos(map, FixtureType::Crate);
    if (crx >= 0)
        place_near(build_merchant(pick_friendly_race(rng), rng, kreth_rep), crx, cry);

    // Courtyard — civilian + drifter
    int pcx = map.width() / 2;
    int pcy = map.height() / 2;
    {
        Npc patrol = build_random_civilian(rng);
        if (find_floor_near(map, pcx, pcy, patrol.x, patrol.y, occupied)) {
            occupied.push_back({patrol.x, patrol.y});
            npcs.push_back(std::move(patrol));
        }
    }
    {
        Npc patrol = build_drifter(pick_friendly_race(rng), rng);
        if (find_floor_near(map, pcx + 2, pcy, patrol.x, patrol.y, occupied)) {
            occupied.push_back({patrol.x, patrol.y});
            npcs.push_back(std::move(patrol));
        }
    }
}

// ---------------------------------------------------------------------------
// Settlement NPC spawning V2 — civ-style-aware roles and scaling
// ---------------------------------------------------------------------------

void spawn_settlement_npcs_v2(TileMap& map, std::vector<Npc>& npcs,
                               int player_x, int player_y,
                               std::mt19937& rng, const Player* player,
                               int size_category,
                               const std::string& style_name,
                               Biome biome) {
    int kreth_rep = player ? reputation_for(*player, Faction_KrethMiningGuild) : 0;
    std::vector<std::pair<int,int>> occupied = {{player_x, player_y}};
    bool ruined = (style_name == "Ruined");

    // Use poi_bounds from the map for constrained NPC placement
    Rect bounds = map.poi_bounds();
    // Fallback: if no bounds set, use a generous center region
    if (bounds.empty()) {
        bounds = {map.width() / 4, map.height() / 4,
                  map.width() / 2, map.height() / 2};
    }

    auto place_near = [&](Npc npc, int fx, int fy) -> bool {
        if (find_floor_near(map, fx, fy, npc.x, npc.y, occupied)) {
            occupied.push_back({npc.x, npc.y});
            npcs.push_back(std::move(npc));
            return true;
        }
        return false;
    };

    auto place_anywhere = [&](Npc npc) -> bool {
        if (find_walkable_in_bounds(map, bounds, npc.x, npc.y, occupied, rng)) {
            occupied.push_back({npc.x, npc.y});
            npcs.push_back(std::move(npc));
            return true;
        }
        return false;
    };

    int placed = 0;

    // --- Target count ---
    int target_min = 0, target_max = 0;
    switch (size_category) {
        case 0: target_min = 4;  target_max = 6;  break;
        case 1: target_min = 7;  target_max = 10; break;
        default: target_min = 11; target_max = 15; break;
    }
    if (ruined) {
        target_min /= 2;
        target_max /= 2;
    }
    std::uniform_int_distribution<int> target_dist(target_min, target_max);
    int target = target_dist(rng);

    // --- Fixed: Leader near Console ---
    auto [cx, cy] = find_fixture_pos(map, FixtureType::Console);
    if (cx >= 0) {
        Npc leader = ruined ? build_scavenger(pick_friendly_race(rng), rng)
                            : build_commander(pick_friendly_race(rng), rng);
        if (place_near(std::move(leader), cx, cy)) ++placed;
    }

    // --- Fixed: Trader near Table ---
    auto [tx, ty] = find_fixture_pos(map, FixtureType::Table);
    if (tx >= 0) {
        std::uniform_int_distribution<int> trader_pick(0, 2);
        Npc trader;
        Race r = pick_friendly_race(rng);
        switch (trader_pick(rng)) {
            case 0: trader = build_merchant(r, rng, kreth_rep); break;
            case 1: trader = build_food_merchant(r, rng, kreth_rep); break;
            default: trader = build_arms_dealer(r, rng, kreth_rep); break;
        }
        if (place_near(std::move(trader), tx, ty)) ++placed;
    }

    // --- Optional roles ---
    int opt_min = 0, opt_max = 0;
    switch (size_category) {
        case 0: opt_min = 1; opt_max = 2; break;
        case 1: opt_min = 2; opt_max = 4; break;
        default: opt_min = 3; opt_max = 5; break;
    }
    std::uniform_int_distribution<int> opt_dist(opt_min, opt_max);
    int opt_slots = opt_dist(rng);

    bool is_rocky = (biome == Biome::Rocky || biome == Biome::Volcanic || biome == Biome::Sandy);

    // Weight table: {role_builder, weight, fixture_type}
    struct OptionalRole {
        enum Kind { Medic, Engineer, Astronomer, ArmsDealer, FoodMerchant,
                    Drifter, Scavenger, Prospector } kind;
        int weight;
    };

    std::vector<OptionalRole> pool;
    if (style_name == "Frontier") {
        pool = {
            {OptionalRole::Medic, 40},
            {OptionalRole::Engineer, 20},
            {OptionalRole::Astronomer, 10},
            {OptionalRole::ArmsDealer, 30},
            {OptionalRole::FoodMerchant, 50},
            {OptionalRole::Drifter, 30},
        };
        if (is_rocky) pool.push_back({OptionalRole::Prospector, 20});
    } else if (style_name == "Advanced") {
        pool = {
            {OptionalRole::Medic, 60},
            {OptionalRole::Engineer, 60},
            {OptionalRole::Astronomer, 50},
            {OptionalRole::ArmsDealer, 40},
            {OptionalRole::FoodMerchant, 30},
            {OptionalRole::Drifter, 10},
        };
        if (is_rocky) pool.push_back({OptionalRole::Prospector, 10});
    } else { // Ruined
        pool = {
            {OptionalRole::Engineer, 30},
            {OptionalRole::ArmsDealer, 50},
            {OptionalRole::FoodMerchant, 20},
            {OptionalRole::Drifter, 60},
            {OptionalRole::Scavenger, 70},
        };
        if (is_rocky) pool.push_back({OptionalRole::Prospector, 10});
    }

    std::uniform_int_distribution<int> pct(1, 100);
    int opt_placed = 0;
    for (auto& entry : pool) {
        if (opt_placed >= opt_slots) break;
        if (pct(rng) > entry.weight) continue;

        Race race = pick_friendly_race(rng);
        Npc npc;
        int fx = -1, fy = -1;

        switch (entry.kind) {
            case OptionalRole::Medic: {
                npc = build_medic(race, rng);
                auto [hx, hy] = find_fixture_pos(map, FixtureType::HealPod);
                fx = hx; fy = hy;
                break;
            }
            case OptionalRole::Engineer: {
                npc = build_engineer(race, rng);
                auto [ex, ey] = find_fixture_pos(map, FixtureType::Conduit);
                fx = ex; fy = ey;
                break;
            }
            case OptionalRole::Astronomer:
                npc = build_astronomer(race, rng);
                break;
            case OptionalRole::ArmsDealer:
                npc = build_arms_dealer(race, rng, kreth_rep);
                break;
            case OptionalRole::FoodMerchant:
                npc = build_food_merchant(race, rng, kreth_rep);
                break;
            case OptionalRole::Drifter:
                npc = build_drifter(race, rng);
                break;
            case OptionalRole::Scavenger:
                npc = build_scavenger(race, rng);
                break;
            case OptionalRole::Prospector:
                npc = build_prospector(race, rng);
                break;
        }

        bool ok = false;
        if (fx >= 0)
            ok = place_near(std::move(npc), fx, fy);
        else
            ok = place_anywhere(std::move(npc));
        if (ok) { ++placed; ++opt_placed; }
    }

    // --- Fill residents to reach target ---
    // Track which bunks already have a resident (1 NPC per dwelling)
    int next_bunk = 0;

    while (placed < target) {
        std::uniform_int_distribution<int> fill(1, 100);
        int roll = fill(rng);
        Race race = pick_friendly_race(rng);
        Npc npc;

        if (roll <= 60) {
            npc = build_civilian(race, rng);
        } else if (roll <= 80) {
            npc = build_drifter(race, rng);
        } else if (roll <= 90) {
            npc = build_civilian(race, rng);
            npc.role = "Settler";
        } else {
            npc = build_civilian(race, rng);
            npc.role = "Refugee";
        }

        // First few residents go near bunks (1 per dwelling)
        bool ok = false;
        if (next_bunk < 10) {
            auto [bx, by] = find_fixture_pos(map, FixtureType::Bunk, next_bunk);
            if (bx >= 0) {
                Npc attempt = npc;
                ok = place_near(std::move(attempt), bx, by);
                if (ok) ++next_bunk;
            }
        }
        // Remaining residents spawn anywhere walkable in the settlement
        if (!ok) {
            if (!place_anywhere(std::move(npc))) break;
        }
        ++placed;
    }
}

} // namespace astra
