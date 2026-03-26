#pragma once

#include "astra/renderer.h"
#include "astra/ui.h"

namespace astra {

// Forward declarations — MapRenderer only needs read access
class WorldManager;
class CombatSystem;
class InputManager;
struct Player;

struct MapRenderContext {
    Renderer* renderer;
    Rect map_rect;
    const WorldManager& world;
    const Player& player;
    const CombatSystem& combat;
    InputManager& input; // non-const: caches look cell
    int camera_x;
    int camera_y;
};

void render_map(const MapRenderContext& ctx);

} // namespace astra
