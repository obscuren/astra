#include "astra/npc_spawner.h"
#include "astra/npc_defs.h"

namespace astra {

void spawn_hub_npcs(TileMap& map, std::vector<Npc>& npcs,
                    int player_x, int player_y, std::mt19937& rng) {
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
                place_npc(build_food_merchant(pick_race(), rng), rid);
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
                place_npc(build_arms_dealer(Race::Kreth, rng), rid);
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

} // namespace astra
