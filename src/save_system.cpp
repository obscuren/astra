#include "astra/save_system.h"
#include "astra/game.h"
#include "astra/star_chart.h"

namespace astra {

static SaveData build_save_data(Game& game, bool dead) {
    SaveData data;
    auto& world = game.world();
    auto& player = game.player();

    data.seed = world.seed();
    data.world_tick = world.world_tick();
    data.dead = dead;
    data.player = player;
    data.current_map_id = 0;
    data.current_region = world.current_region();
    data.death_message = game.death_message();

    if (!dead) {
        data.active_tab = game.active_tab();
        data.panel_visible = game.panel_visible();
        data.messages = game.messages();
        data.stash = world.stash();
        data.navigation = world.navigation();
        data.surface_mode = static_cast<uint8_t>(world.surface_mode());
        data.overworld_x = world.overworld_x();
        data.overworld_y = world.overworld_y();
        data.local_tick = world.day_clock().local_tick;
        data.local_ticks_per_day = world.day_clock().local_ticks_per_day;
    }

    MapState ms;
    ms.map_id = 0;
    ms.tilemap = world.map();
    ms.visibility = world.visibility();
    ms.npcs = world.npcs();
    if (!dead) ms.ground_items = world.ground_items();
    data.maps.push_back(std::move(ms));

    return data;
}

void SaveSystem::save(Game& game) {
    auto data = build_save_data(game, false);
    write_save("save_" + std::to_string(game.world().seed()), data);
}

void SaveSystem::save_death(Game& game) {
    auto data = build_save_data(game, true);
    write_save("save_" + std::to_string(game.world().seed()), data);
}

bool SaveSystem::load(const std::string& filename, Game& game) {
    SaveData data;
    if (!read_save(filename, data)) return false;
    if (data.dead) return false;
    if (data.maps.empty()) return false;

    auto& world = game.world();
    auto& player = game.player();

    game.set_dev_mode(false);
    world.seed() = data.seed;
    world.rng().seed(world.seed());
    world.world_tick() = data.world_tick;
    player = data.player;
    game.set_death_message(data.death_message);
    world.current_region() = data.current_region;
    game.set_active_tab(data.active_tab);
    game.set_panel_visible(data.panel_visible);
    game.messages() = data.messages;
    world.stash() = data.stash;

    // Restore first map
    const auto& ms = data.maps[0];
    world.map() = ms.tilemap;
    world.visibility() = ms.visibility;
    world.npcs() = ms.npcs;
    world.ground_items() = ms.ground_items;

    // Restore navigation data (or bootstrap for old saves)
    if (!data.navigation.systems.empty()) {
        world.navigation() = data.navigation;
    } else {
        world.navigation() = generate_galaxy(world.seed());
    }
    game.rebuild_star_chart_viewer();

    // Restore overworld state
    world.set_surface_mode(static_cast<SurfaceMode>(data.surface_mode));
    world.overworld_x() = data.overworld_x;
    world.overworld_y() = data.overworld_y;

    // Restore day clock
    world.day_clock().local_tick = data.local_tick;
    world.day_clock().local_ticks_per_day = data.local_ticks_per_day;

    game.reset_interaction_state();
    game.post_load();

    return true;
}

} // namespace astra
