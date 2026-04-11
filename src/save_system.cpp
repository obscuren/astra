#include "astra/save_system.h"
#include "astra/game.h"
#include "astra/quest.h"
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
        data.active_widgets = game.active_widgets();
        data.focused_widget = static_cast<uint8_t>(game.focused_widget());
        data.panel_visible = game.panel_visible();
        data.messages = game.messages();
        data.stash = world.stash();
        data.navigation = world.navigation();
        data.surface_mode = static_cast<uint8_t>(world.surface_mode());
        data.overworld_x = world.overworld_x();
        data.overworld_y = world.overworld_y();
        data.zone_x = world.zone_x();
        data.zone_y = world.zone_y();
        data.lost = game.lost();
        data.lost_moves = game.lost_moves();
        data.local_tick = world.day_clock().local_tick;
        data.local_ticks_per_day = world.day_clock().local_ticks_per_day;

        // Overworld return position (set when the player boards their ship
        // from a planet overworld).
        const auto& ret = world.overworld_return();
        data.overworld_return_valid = ret.valid;
        data.overworld_return_x = ret.x;
        data.overworld_return_y = ret.y;
        data.overworld_return_body_key = ret.body_key;

        // Quest state
        data.active_quests = game.quests().active_quests();
        data.completed_quests = game.quests().completed_quests();
        data.quest_locations = world.quest_locations();
        data.lore = world.lore();
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
    game.set_active_widgets(data.active_widgets);
    game.set_focused_widget(data.focused_widget);
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
    // Restore quest state (before star chart rebuild so markers appear)
    game.quests().restore(std::move(data.active_quests),
                          std::move(data.completed_quests));
    world.quest_locations() = std::move(data.quest_locations);
    world.lore() = std::move(data.lore);
    apply_lore_to_galaxy(world.navigation(), world.lore());

    game.rebuild_star_chart_viewer();

    // Restore overworld state
    world.set_surface_mode(static_cast<SurfaceMode>(data.surface_mode));
    world.overworld_x() = data.overworld_x;
    world.overworld_y() = data.overworld_y;
    world.zone_x() = data.zone_x;
    world.zone_y() = data.zone_y;
    game.set_lost(data.lost, data.lost_moves);

    // Restore day clock
    world.day_clock().local_tick = data.local_tick;
    world.day_clock().local_ticks_per_day = data.local_ticks_per_day;

    // Restore overworld return position
    auto& ret = world.overworld_return();
    ret.valid = data.overworld_return_valid;
    ret.x = data.overworld_return_x;
    ret.y = data.overworld_return_y;
    ret.body_key = data.overworld_return_body_key;

    game.reset_interaction_state();
    game.post_load();

    return true;
}

} // namespace astra
