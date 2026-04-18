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
        data.locked_quests = game.quests().locked_quests();
        data.available_quests = game.quests().available_quests();
        data.active_quests = game.quests().active_quests();
        data.completed_quests = game.quests().completed_quests();
        data.quest_locations = world.quest_locations();
        data.pending_quest_cleanup = world.pending_quest_cleanup();
        data.stellar_signal_echo_ids = world.stellar_signal_echo_ids();
        data.stellar_signal_beacon_id = world.stellar_signal_beacon_id();
        data.world_flags = world.world_flags();
        data.ambushed_systems = world.ambushed_systems();
        data.lore = world.lore();
    }

    // maps[0]: the active map the player is currently on.
    MapState ms;
    ms.map_id = 0;
    ms.tilemap = world.map();
    ms.visibility = world.visibility();
    ms.npcs = world.npcs();
    if (!dead) ms.ground_items = world.ground_items();

    // v23: copy POI budget, hidden POIs, anchor hints from the overworld map.
    ms.poi_budget = world.map().poi_budget();
    ms.hidden_pois = world.map().hidden_pois();
    for (const auto& [k, h] : world.map().anchor_hints()) {
        ms.anchor_hints.push_back({k, h});
    }

    data.maps.push_back(std::move(ms));

    // v24: persist the location cache (previously visited maps).
    if (!dead) {
        for (const auto& [key, state] : world.location_cache()) {
            MapState cached;
            cached.map_id = 0;
            cached.tilemap = state.map;
            cached.visibility = state.visibility;
            cached.npcs = state.npcs;
            cached.ground_items = state.ground_items;
            cached.player_x = state.player_x;
            cached.player_y = state.player_y;

            // Copy POI data from the cached tilemap.
            cached.poi_budget = state.map.poi_budget();
            cached.hidden_pois = state.map.hidden_pois();
            for (const auto& [ak, ah] : state.map.anchor_hints()) {
                cached.anchor_hints.push_back({ak, ah});
            }

            // Store the LocationKey so we can rebuild the cache on load.
            auto& [sys, body, moon, is_sta, ox, oy, depth] = key;
            cached.loc_system_id = sys;
            cached.loc_body_index = body;
            cached.loc_moon_index = moon;
            cached.loc_is_station = is_sta;
            cached.loc_ow_x = ox;
            cached.loc_ow_y = oy;
            cached.loc_depth = depth;

            data.maps.push_back(std::move(cached));
        }
    }

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

    world.map().set_poi_budget(ms.poi_budget);
    world.map().hidden_pois_mut() = ms.hidden_pois;
    for (const auto& [k, h] : ms.anchor_hints) {
        int x = static_cast<int>(k % static_cast<uint64_t>(world.map().width()));
        int y = static_cast<int>(k / static_cast<uint64_t>(world.map().width()));
        world.map().set_anchor_hint(x, y, h);
    }

    // v24: restore location cache from maps[1+].
    for (size_t i = 1; i < data.maps.size(); ++i) {
        auto& cm = data.maps[i];
        LocationKey key{cm.loc_system_id, cm.loc_body_index,
                        cm.loc_moon_index, cm.loc_is_station,
                        cm.loc_ow_x, cm.loc_ow_y, cm.loc_depth};

        // Restore POI data onto the tilemap before caching.
        cm.tilemap.set_poi_budget(cm.poi_budget);
        cm.tilemap.hidden_pois_mut() = std::move(cm.hidden_pois);
        for (const auto& [ak, ah] : cm.anchor_hints) {
            int ax = static_cast<int>(ak % static_cast<uint64_t>(cm.tilemap.width()));
            int ay = static_cast<int>(ak / static_cast<uint64_t>(cm.tilemap.width()));
            cm.tilemap.set_anchor_hint(ax, ay, ah);
        }

        LocationState& state = world.location_cache()[key];
        state.map = std::move(cm.tilemap);
        state.visibility = std::move(cm.visibility);
        state.npcs = std::move(cm.npcs);
        state.ground_items = std::move(cm.ground_items);
        state.player_x = cm.player_x;
        state.player_y = cm.player_y;
    }

    // Legacy v22 reconstruction — if the map has no budget but is an overworld,
    // scan placed POI tiles to produce a best-effort budget.
    if (world.map().map_type() == MapType::Overworld &&
        world.map().poi_budget().settlements == 0 &&
        world.map().poi_budget().outposts == 0 &&
        world.map().poi_budget().ruins.empty() &&
        world.map().poi_budget().ships.empty() &&
        world.map().poi_budget().total_caves() == 0 &&
        world.map().poi_budget().beacons == 0 &&
        world.map().poi_budget().megastructures == 0) {
        world.map().set_poi_budget(reconstruct_poi_budget_from_map(world.map()));
    }

    // Restore navigation data (or bootstrap for old saves)
    if (!data.navigation.systems.empty()) {
        world.navigation() = data.navigation;
    } else {
        world.navigation() = generate_galaxy(world.seed());
    }
    // Restore quest state (before star chart rebuild so markers appear)
    game.quests().restore(std::move(data.locked_quests),
                          std::move(data.available_quests),
                          std::move(data.active_quests),
                          std::move(data.completed_quests));
    game.quests().reconcile_with_catalog(game);
    world.quest_locations() = std::move(data.quest_locations);
    world.pending_quest_cleanup() = std::move(data.pending_quest_cleanup);
    world.stellar_signal_echo_ids() = data.stellar_signal_echo_ids;
    world.stellar_signal_beacon_id() = data.stellar_signal_beacon_id;
    world.world_flags() = std::move(data.world_flags);
    world.ambushed_systems() = std::move(data.ambushed_systems);
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
