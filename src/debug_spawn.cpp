#include "astra/debug_spawn.h"

namespace astra {

void debug_spawn(TileMap& map,
                 std::vector<Npc>& npcs,
                 int player_x, int player_y,
                 std::vector<std::pair<int,int>>& occupied,
                 std::mt19937& rng) {
    // Young Xytomorph — weaker variant in the starting room
    {
        Npc young = create_npc(NpcRole::Xytomorph, Race::Xytomorph, rng);
        young.role = "Young Xytomorph";
        young.hp = 5;
        young.max_hp = 5;
        young.base_damage = 1;
        young.base_xp = 15;
        young.quickness = 100;
        if (map.find_open_spot_near(player_x, player_y,
                                    young.x, young.y, occupied, &rng)) {
            occupied.push_back({young.x, young.y});
            npcs.push_back(std::move(young));
        }
    }

    // Full Xytomorph — in a different room
    {
        Npc xyto = create_npc(NpcRole::Xytomorph, Race::Xytomorph, rng);
        if (map.find_open_spot_other_room(player_x, player_y,
                                          xyto.x, xyto.y, occupied, &rng)) {
            occupied.push_back({xyto.x, xyto.y});
            npcs.push_back(std::move(xyto));
        }
    }
}

} // namespace astra
