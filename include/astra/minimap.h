#pragma once

#include "astra/renderer.h"
#include "astra/tilemap.h"
#include "astra/visibility_map.h"
#include "astra/npc.h"
#include "astra/ui.h"

#include <vector>

namespace astra {

// Feature flags toggled by the Wayfinding skill tree.
struct MinimapFlags {
    bool scouts_eye = false;    // Scout's Eye: show all NPCs (hostile + friendly)
    bool cartographer = false;  // Cartographer: show items + POIs
};

class Minimap {
public:
    Minimap() = default;

    void draw(UIContext& ctx,
              const TileMap& map,
              const VisibilityMap& vis,
              int player_x, int player_y,
              const std::vector<Npc>& npcs,
              const MinimapFlags& flags = {});

private:
    static Color tile_color(Tile t, MapType map_type);
    static Color dim_color(Color c);
    static bool is_exit_tile(Tile t, const TileMap& map, int x, int y);
};

} // namespace astra
