#include "astra/game.h"
#include "astra/debug_spawn.h"
#include "astra/lore_types.h"
#include "astra/tinkering.h"
#include "astra/item_defs.h"
#include "astra/map_generator.h"
#include "astra/map_properties.h"
#include "astra/npc_defs.h"
#include "astra/npc_spawner.h"
#include "astra/star_chart.h"

namespace astra {


void Game::save_current_location() {
    animations_.clear();
    LocationKey key;
    if (world_.map().location_name() == "Maintenance Tunnels") {
        key = WorldManager::maintenance_key;
    } else if (world_.navigation().on_ship) {
        key = WorldManager::ship_key;
    } else if (world_.navigation().at_station) {
        key = LocationKey{world_.navigation().current_system_id, -1, -1, true, -1, -1, 0, -1, -1};
    } else if (world_.on_overworld()) {
        key = LocationKey{world_.navigation().current_system_id, world_.navigation().current_body_index,
               world_.navigation().current_moon_index, false, -1, -1, 0, -1, -1};
    } else if (world_.on_detail_map()) {
        key = LocationKey{world_.navigation().current_system_id, world_.navigation().current_body_index,
               world_.navigation().current_moon_index, false, world_.overworld_x(), world_.overworld_y(), 0,
               world_.zone_x(), world_.zone_y()};
    } else {
        // Dungeon (depth=1)
        key = LocationKey{world_.navigation().current_system_id, world_.navigation().current_body_index,
               world_.navigation().current_moon_index, false, world_.overworld_x(), world_.overworld_y(), 1,
               world_.zone_x(), world_.zone_y()};
    }
    LocationState& state = world_.location_cache()[key];
    state.map = std::move(world_.map());
    state.visibility = std::move(world_.visibility());
    state.npcs = std::move(world_.npcs());
    state.ground_items = std::move(world_.ground_items());
    state.player_x = player_.x;
    state.player_y = player_.y;
}

void Game::restore_location(const LocationKey& key) {
    auto it = world_.location_cache().find(key);
    if (it == world_.location_cache().end()) return;
    LocationState& state = it->second;
    world_.map() = std::move(state.map);
    world_.visibility() = std::move(state.visibility);
    world_.npcs() = std::move(state.npcs);
    world_.ground_items() = std::move(state.ground_items);

    // Always restore cached position — return to where we left
    player_.x = state.player_x;
    player_.y = state.player_y;

    world_.location_cache().erase(it);
}

void Game::enter_ship() {
    save_current_location();
    world_.navigation().on_ship = true;
    world_.set_surface_mode(SurfaceMode::Dungeon);

    if (world_.location_cache().count(WorldManager::ship_key)) {
        restore_location(WorldManager::ship_key);
    } else {
        // Generate the ship for the first time
        unsigned ship_seed = world_.seed() ^ 0x5B1Bu;
        auto props = default_properties(MapType::Starship);
        world_.map() = TileMap(props.width, props.height, MapType::Starship);
        auto gen = create_starship_generator();
        gen->generate(world_.map(), props, ship_seed);
        world_.map().set_location_name("Your Starship");

        world_.npcs().clear();
        world_.ground_items().clear();
        // Spawn in region 0 (cockpit)
        if (!world_.map().find_open_spot_in_region(0, player_.x, player_.y, {})) {
            world_.map().find_open_spot(player_.x, player_.y);
        }

        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    world_.visibility().reveal_all();
    world_.current_region() = -1;
    compute_camera();
    check_region_change();
    log("You board your starship.");
}

void Game::exit_ship_to_station() {
    save_current_location();
    world_.navigation().on_ship = false;

    // Restore station from cache
    LocationKey station_key = {world_.navigation().current_system_id,
                               -1, -1, true, -1, -1, 0, -1, -1};
    if (world_.location_cache().count(station_key)) {
        restore_location(station_key);
    }
    world_.set_surface_mode(SurfaceMode::Dungeon);
    recompute_fov();
    world_.current_region() = -1;
    compute_camera();
    check_region_change();
    log("You disembark and return to the station.");
}

void Game::enter_maintenance_tunnels() {
    save_current_location();
    world_.set_surface_mode(SurfaceMode::Dungeon);

    if (world_.location_cache().count(WorldManager::maintenance_key)) {
        restore_location(WorldManager::maintenance_key);
    } else {
        // Generate a small derelict-style dungeon
        unsigned tunnel_seed = world_.seed() ^ 0xD33Du;
        auto props = default_properties(MapType::SpaceStation);
        props.width = 50;
        props.height = 40;
        props.room_count_min = 4;
        props.room_count_max = 6;
        props.difficulty = 1;
        world_.map() = TileMap(props.width, props.height, MapType::SpaceStation);
        auto gen = create_derelict_generator();
        gen->generate(world_.map(), props, tunnel_seed);
        world_.map().set_location_name("Maintenance Tunnels");

        world_.npcs().clear();
        world_.ground_items().clear();

        // Spawn player at region 0
        if (!world_.map().find_open_spot_in_region(0, player_.x, player_.y, {})) {
            world_.map().find_open_spot(player_.x, player_.y);
        }

        // Place exit stairs near player spawn
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0) continue;
                int sx = player_.x + dx, sy = player_.y + dy;
                if (sx >= 0 && sy >= 0 && sx < world_.map().width() && sy < world_.map().height()
                    && world_.map().get(sx, sy) == Tile::Floor
                    && world_.map().fixture_ids()[sy * world_.map().width() + sx] < 0) {
                    world_.map().add_fixture(sx, sy, make_fixture(FixtureType::StairsUp));
                    goto tunnels_stairs_placed;
                }
            }
        }
        tunnels_stairs_placed:

        // Spawn Young Xytomorphs
        std::mt19937 npc_rng(tunnel_seed ^ 0xA1u);
        std::vector<std::pair<int,int>> occupied = {{player_.x, player_.y}};
        int enemy_count = std::uniform_int_distribution<int>(3, 5)(npc_rng);
        for (int i = 0; i < enemy_count; ++i) {
            Npc xeno = build_xytomorph(npc_rng);
            xeno.name = "Young Xytomorph";
            xeno.hp = 6; xeno.max_hp = 6;
            xeno.base_damage = 1;
            xeno.base_xp = 10;
            int rx = 0, ry = 0;
            // Try to place in a room other than region 0
            bool placed = false;
            for (int r = 1; r < world_.map().region_count() && !placed; ++r) {
                if (world_.map().find_open_spot_in_region(r, rx, ry, occupied)) {
                    xeno.x = rx; xeno.y = ry;
                    occupied.push_back({rx, ry});
                    world_.npcs().push_back(std::move(xeno));
                    placed = true;
                }
            }
            if (!placed) {
                if (world_.map().find_open_spot_other_room(
                        player_.x, player_.y, rx, ry, occupied, &npc_rng)) {
                    xeno.x = rx; xeno.y = ry;
                    occupied.push_back({rx, ry});
                    world_.npcs().push_back(std::move(xeno));
                }
            }
        }

        // Place Engine Coil as a ground item in a deeper room
        {
            Item engine = build_engine_coil_mk1();
            int ix = 0, iy = 0;
            int last_region = world_.map().region_count() - 1;
            if (last_region > 0 &&
                world_.map().find_open_spot_in_region(last_region, ix, iy, occupied)) {
                world_.ground_items().push_back({ix, iy, std::move(engine)});
            } else if (world_.map().find_open_spot_other_room(
                           player_.x, player_.y, ix, iy, occupied, &npc_rng)) {
                world_.ground_items().push_back({ix, iy, std::move(engine)});
            }
        }

        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    recompute_fov();
    world_.current_region() = -1;
    compute_camera();
    check_region_change();
    log("You descend into the maintenance tunnels.");
}

void Game::exit_maintenance_tunnels() {
    save_current_location();

    // Restore the hub station from cache
    LocationKey hub_key = {world_.navigation().current_system_id,
                           -1, -1, true, -1, -1, 0, -1, -1};
    if (world_.location_cache().count(hub_key)) {
        restore_location(hub_key);
    }
    world_.set_surface_mode(SurfaceMode::Dungeon);
    recompute_fov();
    world_.current_region() = -1;
    compute_camera();
    check_region_change();
    log("You climb back up to the station.");
}

// Enrich a dungeon/ruin entry message with lore context if the system has annotations.
static std::string lore_entry_message(const std::string& base_msg,
                                       const NavigationData& nav,
                                       const WorldLore& lore) {
    // Find current system
    for (const auto& sys : nav.systems) {
        if (sys.id == nav.current_system_id && sys.lore.lore_tier > 0) {
            const auto& ann = sys.lore;
            if (!ann.primary_civ_name.empty()) {
                std::string enriched = base_msg;
                // Remove trailing period if present
                if (!enriched.empty() && enriched.back() == '.')
                    enriched.pop_back();
                enriched += " — ruins of " + ann.primary_civ_name + " origin";

                // Find the civilization's epoch for dating
                for (const auto& civ : lore.civilizations) {
                    if (civ.short_name == ann.primary_civ_name) {
                        char buf[64];
                        std::snprintf(buf, sizeof(buf), ", dating to %.1f billion years ago.",
                                      civ.epoch_start_bya);
                        enriched += buf;
                        return enriched;
                    }
                }
                enriched += ".";
                return enriched;
            }
            break;
        }
    }
    return base_msg;
}

void Game::enter_overworld_tile() {
    Tile tile = world_.map().get(player_.x, player_.y);
    world_.overworld_x() = player_.x;
    world_.overworld_y() = player_.y;

    // Determine detail map type + biome from overworld tile
    MapType detail_type = MapType::Rocky;
    Biome detail_biome = detail_biome_for_terrain(tile, world_.map().biome());

    const char* enter_msg = "You explore the area.";

    switch (tile) {
        case Tile::OW_CaveEntrance:
            detail_type = MapType::Rocky;
            enter_msg = "You descend into the cavern.";
            break;
        case Tile::OW_Ruins:
            detail_type = MapType::DerelictStation;
            detail_biome = Biome::Corroded;
            enter_msg = "You enter the ancient ruins.";
            break;
        case Tile::OW_Settlement:
            detail_type = MapType::SpaceStation;
            detail_biome = Biome::Station;
            enter_msg = "You enter the small settlement.";
            break;
        case Tile::OW_CrashedShip:
            detail_type = MapType::DerelictStation;
            detail_biome = Biome::Station;
            enter_msg = "You climb into the wrecked hull.";
            break;
        case Tile::OW_Outpost:
            detail_type = MapType::SpaceStation;
            detail_biome = Biome::Station;
            enter_msg = "You enter the abandoned outpost.";
            break;
        case Tile::OW_Plains:
        case Tile::OW_Desert:
            detail_type = MapType::Rocky;
            enter_msg = "You survey the terrain closely.";
            break;
        case Tile::OW_IceField:
            detail_type = MapType::Rocky;
            enter_msg = "You traverse the frozen landscape.";
            break;
        case Tile::OW_LavaFlow:
            detail_type = MapType::Lava;
            enter_msg = "You carefully navigate the volcanic terrain.";
            break;
        case Tile::OW_Fungal:
            detail_type = MapType::Rocky;
            enter_msg = "You push through the alien growth.";
            break;
        case Tile::OW_Crater:
            detail_type = MapType::Asteroid;
            enter_msg = "You descend into the impact crater.";
            break;
        case Tile::OW_Forest:
            detail_type = MapType::Rocky;
            enter_msg = "You push into the dense forest.";
            break;
        case Tile::OW_River:
            detail_type = MapType::Rocky;
            enter_msg = "You ford the rushing river.";
            break;
        case Tile::OW_Swamp:
            detail_type = MapType::Rocky;
            enter_msg = "You wade into the murky swamp.";
            break;
        default:
            return; // OW_Landing, OW_Mountains, OW_Lake — can't enter
    }

    // Remember the body name for the detail map
    std::string body_name = world_.map().location_name();

    // Detail map LocationKey uses overworld coords (depth=1 for legacy dungeon entry)
    LocationKey detail_key = {world_.navigation().current_system_id,
                              world_.navigation().current_body_index,
                              world_.navigation().current_moon_index,
                              false, world_.overworld_x(), world_.overworld_y(), 1,
                              world_.zone_x(), world_.zone_y()};

    // Save overworld before entering detail
    save_current_location();
    world_.set_surface_mode(SurfaceMode::Dungeon);

    if (world_.location_cache().count(detail_key)) {
        restore_location(detail_key);
    } else {
        // Generate detail map
        unsigned detail_seed = world_.seed()
            ^ (world_.navigation().current_system_id * 7919u)
            ^ (static_cast<unsigned>(world_.navigation().current_body_index) * 6271u)
            ^ (static_cast<unsigned>(world_.navigation().current_moon_index + 1) * 3571u)
            ^ (static_cast<unsigned>(world_.overworld_x()) * 1013u)
            ^ (static_cast<unsigned>(world_.overworld_y()) * 2039u);

        auto props = default_properties(detail_type);
        props.biome = detail_biome;
        world_.map() = TileMap(props.width, props.height, detail_type);

        // Smaller maps for minor POIs
        if (tile == Tile::OW_CrashedShip || tile == Tile::OW_Outpost) {
            props.room_count_min = 3;
            props.room_count_max = 5;
        }

        auto gen = create_generator(detail_type);
        gen->generate(world_.map(), props, detail_seed);
        world_.map().set_location_name(body_name);

        world_.npcs().clear();
        world_.ground_items().clear();
        world_.map().find_open_spot(player_.x, player_.y);

        // Spawn NPCs
        std::mt19937 npc_rng(detail_seed ^ 0xD3ADu);
        std::vector<std::pair<int,int>> occupied = {{player_.x, player_.y}};
        debug_spawn(world_.map(), world_.npcs(), player_.x, player_.y, occupied, npc_rng);

        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    world_.current_region() = -1;
    recompute_fov();
    compute_camera();
    check_region_change();
    // Archaeology skill: show civilization origin in entry message
    if (player_has_skill(player_, SkillId::Cat_Archaeology))
        log(lore_entry_message(enter_msg, world_.navigation(), world_.lore()));
    else
        log(enter_msg);
}

void Game::exit_to_overworld() {
    // Save detail map
    save_current_location();

    // Restore overworld
    LocationKey ow_key = {world_.navigation().current_system_id,
                          world_.navigation().current_body_index,
                          world_.navigation().current_moon_index,
                          false, -1, -1, 0, -1, -1};

    if (world_.location_cache().count(ow_key)) {
        restore_location(ow_key);
    }

    player_.x = world_.overworld_x();
    player_.y = world_.overworld_y();
    world_.set_surface_mode(SurfaceMode::Overworld);
    world_.visibility().reveal_all();
    animations_.spawn_fixture_anims(world_.map(), world_.visibility());
    world_.current_region() = -1;
    compute_camera();
    log("You return to the surface.");
}

MapProperties Game::build_detail_props(int ow_x, int ow_y) {
    auto props = default_properties(MapType::DetailMap);

    // Read overworld from cache
    LocationKey ow_key = {world_.navigation().current_system_id,
                          world_.navigation().current_body_index,
                          world_.navigation().current_moon_index,
                          false, -1, -1, 0, -1, -1};

    const TileMap* ow_map = nullptr;
    if (world_.on_overworld()) {
        ow_map = &world_.map();
    } else {
        auto it = world_.location_cache().find(ow_key);
        if (it != world_.location_cache().end())
            ow_map = &it->second.map;
    }
    if (!ow_map) return props;

    props.detail_terrain = ow_map->get(ow_x, ow_y);
    props.biome = detail_biome_for_terrain(props.detail_terrain, ow_map->biome());

    // Sample overworld neighbors — only at outer edges of the 3x3 zone grid.
    // Interior zone edges border the same overworld tile, so no terrain blending.
    int zx = world_.zone_x();
    int zy = world_.zone_y();
    if (zy == 0 && ow_y > 0)
        props.detail_neighbor_n = ow_map->get(ow_x, ow_y - 1);
    if (zy == zones_per_tile - 1 && ow_y < ow_map->height() - 1)
        props.detail_neighbor_s = ow_map->get(ow_x, ow_y + 1);
    if (zx == 0 && ow_x > 0)
        props.detail_neighbor_w = ow_map->get(ow_x - 1, ow_y);
    if (zx == zones_per_tile - 1 && ow_x < ow_map->width() - 1)
        props.detail_neighbor_e = ow_map->get(ow_x + 1, ow_y);

    // Check for POI
    Tile t = props.detail_terrain;
    if (t == Tile::OW_CaveEntrance || t == Tile::OW_Settlement ||
        t == Tile::OW_Ruins || t == Tile::OW_CrashedShip ||
        t == Tile::OW_Outpost || t == Tile::OW_Landing) {
        props.detail_has_poi = true;
        props.detail_poi_type = t;
    }

    return props;
}

void Game::enter_detail_map() {
    world_.overworld_x() = player_.x;
    world_.overworld_y() = player_.y;

    // Enter at center zone of the 3x3 grid
    world_.zone_x() = 1;
    world_.zone_y() = 1;

    auto props = build_detail_props(world_.overworld_x(), world_.overworld_y());
    std::string body_name = world_.map().location_name();

    LocationKey detail_key = {world_.navigation().current_system_id,
                              world_.navigation().current_body_index,
                              world_.navigation().current_moon_index,
                              false, world_.overworld_x(), world_.overworld_y(), 0,
                              world_.zone_x(), world_.zone_y()};

    save_current_location();
    world_.set_surface_mode(SurfaceMode::DetailMap);

    if (world_.location_cache().count(detail_key)) {
        restore_location(detail_key);
    } else {
        unsigned detail_seed = world_.seed()
            ^ (world_.navigation().current_system_id * 7919u)
            ^ (static_cast<unsigned>(world_.navigation().current_body_index) * 6271u)
            ^ (static_cast<unsigned>(world_.navigation().current_moon_index + 1) * 3571u)
            ^ (static_cast<unsigned>(world_.overworld_x()) * 1013u)
            ^ (static_cast<unsigned>(world_.overworld_y()) * 2039u)
            ^ (static_cast<unsigned>(world_.zone_x()) * 4517u)
            ^ (static_cast<unsigned>(world_.zone_y()) * 5381u);

        world_.map() = TileMap(props.width, props.height, MapType::DetailMap);
        auto gen = create_generator(MapType::DetailMap);
        gen->generate(world_.map(), props, detail_seed);
        world_.map().set_biome(props.biome);
        world_.map().set_location_name(body_name);

        world_.npcs().clear();
        world_.ground_items().clear();

        // Place player: spawn in cockpit for landing pad, center otherwise
        if (props.detail_poi_type == Tile::OW_Landing) {
            // Find the cockpit region (ShipCockpit flavor)
            bool found_cockpit = false;
            for (int i = 0; i < world_.map().region_count(); ++i) {
                if (world_.map().region(i).flavor == RoomFlavor::ShipCockpit) {
                    if (world_.map().find_open_spot_in_region(i, player_.x, player_.y, {})) {
                        found_cockpit = true;
                    }
                    break;
                }
            }
            if (!found_cockpit) {
                player_.x = world_.map().width() / 2;
                player_.y = world_.map().height() / 2;
                if (!world_.map().passable(player_.x, player_.y))
                    world_.map().find_open_spot(player_.x, player_.y);
            }
        } else {
            player_.x = world_.map().width() / 2;
            player_.y = world_.map().height() / 2;
            if (!world_.map().passable(player_.x, player_.y))
                world_.map().find_open_spot(player_.x, player_.y);
        }

        // Spawn NPCs in settlements and outposts (after player placement)
        std::mt19937 npc_rng(detail_seed ^ 0xC1A5u);
        if (props.detail_poi_type == Tile::OW_Settlement) {
            spawn_settlement_npcs(world_.map(), world_.npcs(), player_.x, player_.y, npc_rng, &player_);
        } else if (props.detail_poi_type == Tile::OW_Outpost) {
            spawn_outpost_npcs(world_.map(), world_.npcs(), player_.x, player_.y, npc_rng, &player_);
        }

        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    world_.current_region() = -1;
    recompute_fov();
    compute_camera();
    check_region_change();

    // Terrain-specific entry message
    const char* msg = "You explore the area.";
    switch (props.detail_terrain) {
        case Tile::OW_CaveEntrance: msg = "A cave entrance looms before you."; break;
        case Tile::OW_Settlement:   msg = "You enter the settlement area."; break;
        case Tile::OW_Ruins:        msg = "Ancient ruins surround you."; break;
        case Tile::OW_CrashedShip:  msg = "Wreckage of a starship lies scattered."; break;
        case Tile::OW_Landing:      msg = "You stand in your ship's cockpit. The landing pad stretches outside."; break;
        case Tile::OW_Outpost:      msg = "You approach the outpost."; break;
        case Tile::OW_Forest:       msg = "Dense forest surrounds you."; break;
        case Tile::OW_Desert:       msg = "Sand stretches in every direction."; break;
        case Tile::OW_IceField:     msg = "Ice formations glitter around you."; break;
        case Tile::OW_LavaFlow:     msg = "Heat radiates from the volcanic ground."; break;
        case Tile::OW_Swamp:        msg = "You wade into murky terrain."; break;
        case Tile::OW_Crater:       msg = "You descend into the impact crater."; break;
        case Tile::OW_Fungal:       msg = "Alien growths crowd around you."; break;
        case Tile::OW_Plains:       msg = "Open terrain stretches before you."; break;
        default: break;
    }
    log(msg);
}

void Game::exit_detail_to_overworld() {
    save_current_location();

    LocationKey ow_key = {world_.navigation().current_system_id,
                          world_.navigation().current_body_index,
                          world_.navigation().current_moon_index,
                          false, -1, -1, 0, -1, -1};

    if (world_.location_cache().count(ow_key)) {
        restore_location(ow_key);
    }

    player_.x = world_.overworld_x();
    player_.y = world_.overworld_y();
    world_.zone_x() = 1;
    world_.zone_y() = 1;
    world_.set_surface_mode(SurfaceMode::Overworld);
    world_.visibility().reveal_all();
    animations_.spawn_fixture_anims(world_.map(), world_.visibility());
    world_.current_region() = -1;
    compute_camera();
    log("You return to the surface view.");
}

void Game::enter_dungeon_from_detail() {
    // Save detail map (depth=0)
    save_current_location();

    // Determine dungeon type from overworld tile
    LocationKey ow_key = {world_.navigation().current_system_id,
                          world_.navigation().current_body_index,
                          world_.navigation().current_moon_index,
                          false, -1, -1, 0, -1, -1};

    Tile ow_tile = Tile::OW_CaveEntrance;
    Biome ow_biome = world_.map().biome();
    auto it = world_.location_cache().find(ow_key);
    if (it != world_.location_cache().end()) {
        ow_tile = it->second.map.get(world_.overworld_x(), world_.overworld_y());
        ow_biome = it->second.map.biome();
    }

    // Map overworld tile to dungeon type + biome
    MapType detail_type = MapType::Rocky;
    Biome detail_biome = detail_biome_for_terrain(ow_tile, ow_biome);
    const char* enter_msg = "You descend deeper.";

    switch (ow_tile) {
        case Tile::OW_CaveEntrance:
            detail_type = MapType::Rocky;
            enter_msg = "You descend into the cavern.";
            break;
        case Tile::OW_Ruins:
            detail_type = MapType::DerelictStation;
            detail_biome = Biome::Corroded;
            enter_msg = "You enter the ancient ruins.";
            break;
        case Tile::OW_Settlement:
            detail_type = MapType::SpaceStation;
            detail_biome = Biome::Station;
            enter_msg = "You enter the settlement interior.";
            break;
        case Tile::OW_CrashedShip:
            detail_type = MapType::DerelictStation;
            detail_biome = Biome::Station;
            enter_msg = "You climb into the wrecked hull.";
            break;
        case Tile::OW_Outpost:
            detail_type = MapType::SpaceStation;
            detail_biome = Biome::Station;
            enter_msg = "You enter the outpost interior.";
            break;
        default:
            detail_type = MapType::Rocky;
            enter_msg = "You descend underground.";
            break;
    }

    std::string body_name = world_.map().location_name();

    LocationKey dungeon_key = {world_.navigation().current_system_id,
                               world_.navigation().current_body_index,
                               world_.navigation().current_moon_index,
                               false, world_.overworld_x(), world_.overworld_y(), 1,
                               world_.zone_x(), world_.zone_y()};

    world_.set_surface_mode(SurfaceMode::Dungeon);

    if (world_.location_cache().count(dungeon_key)) {
        restore_location(dungeon_key);
    } else {
        unsigned detail_seed = world_.seed()
            ^ (world_.navigation().current_system_id * 7919u)
            ^ (static_cast<unsigned>(world_.navigation().current_body_index) * 6271u)
            ^ (static_cast<unsigned>(world_.navigation().current_moon_index + 1) * 3571u)
            ^ (static_cast<unsigned>(world_.overworld_x()) * 1013u)
            ^ (static_cast<unsigned>(world_.overworld_y()) * 2039u)
            ^ 0xDE3Du;

        auto props = default_properties(detail_type);
        props.biome = detail_biome;
        world_.map() = TileMap(props.width, props.height, detail_type);
        auto gen = create_generator(detail_type);
        gen->generate(world_.map(), props, detail_seed);
        world_.map().set_location_name(body_name);

        world_.npcs().clear();
        world_.ground_items().clear();
        world_.map().find_open_spot(player_.x, player_.y);

        // Place exit stairs near player spawn
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0) continue;
                int sx = player_.x + dx, sy = player_.y + dy;
                if (sx >= 0 && sy >= 0 && sx < world_.map().width() && sy < world_.map().height()
                    && world_.map().get(sx, sy) == Tile::Floor
                    && world_.map().fixture_ids()[sy * world_.map().width() + sx] < 0) {
                    world_.map().add_fixture(sx, sy, make_fixture(FixtureType::StairsUp));
                    goto stairs_placed;
                }
            }
        }
        stairs_placed:

        std::mt19937 npc_rng(detail_seed ^ 0xD3ADu);
        std::vector<std::pair<int,int>> occupied = {{player_.x, player_.y}};

        // Check for quest-specific spawns
        auto qit = world_.quest_locations().find(dungeon_key);
        if (qit == world_.quest_locations().end()) {
            // Also check body-level key
            LocationKey body_key = {world_.navigation().current_system_id,
                                    world_.navigation().current_body_index,
                                    world_.navigation().current_moon_index,
                                    false, -1, -1, 0, -1, -1};
            qit = world_.quest_locations().find(body_key);
        }

        if (qit != world_.quest_locations().end()) {
            const auto& meta = qit->second;
            // Spawn quest-specific NPCs
            for (const auto& role : meta.npc_roles) {
                Npc npc = create_npc_by_role(role, npc_rng);
                if (world_.map().find_open_spot_other_room(
                        player_.x, player_.y, npc.x, npc.y, occupied, &npc_rng)) {
                    occupied.push_back({npc.x, npc.y});
                    world_.npcs().push_back(std::move(npc));
                }
            }
            // Place quest items on the ground
            for (const auto& item_name : meta.quest_items) {
                int ix = 0, iy = 0;
                if (world_.map().find_open_spot_other_room(
                        player_.x, player_.y, ix, iy, occupied, &npc_rng)) {
                    Item quest_item;
                    quest_item.name = item_name;
                    quest_item.type = ItemType::QuestItem;
                    quest_item.description = "A quest item: " + item_name;
                    world_.ground_items().push_back({ix, iy, quest_item});
                    occupied.push_back({ix, iy});
                }
            }
        } else {
            debug_spawn(world_.map(), world_.npcs(), player_.x, player_.y, occupied, npc_rng);
        }

        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    // Quest location entry tracking — notify with the map's location name
    {
        LocationKey dkey = {world_.navigation().current_system_id,
                            world_.navigation().current_body_index,
                            world_.navigation().current_moon_index,
                            false, world_.overworld_x(), world_.overworld_y(), 1,
                            world_.zone_x(), world_.zone_y()};
        LocationKey bkey = {world_.navigation().current_system_id,
                            world_.navigation().current_body_index,
                            world_.navigation().current_moon_index,
                            false, -1, -1, 0, -1, -1};
        if (world_.quest_locations().count(dkey) ||
            world_.quest_locations().count(bkey)) {
            quest_manager_.on_location_entered(world_.map().location_name());
        }
    }

    world_.current_region() = -1;
    recompute_fov();
    compute_camera();
    check_region_change();
    if (player_has_skill(player_, SkillId::Cat_Archaeology))
        log(lore_entry_message(enter_msg, world_.navigation(), world_.lore()));
    else
        log(enter_msg);
}

void Game::exit_dungeon_to_detail() {
    // Save dungeon (depth=1)
    save_current_location();

    // Restore detail map (depth=0)
    LocationKey detail_key = {world_.navigation().current_system_id,
                              world_.navigation().current_body_index,
                              world_.navigation().current_moon_index,
                              false, world_.overworld_x(), world_.overworld_y(), 0,
                              world_.zone_x(), world_.zone_y()};

    world_.set_surface_mode(SurfaceMode::DetailMap);

    if (world_.location_cache().count(detail_key)) {
        restore_location(detail_key);
    } else {
        // Detail map was never cached — generate it
        auto props = build_detail_props(world_.overworld_x(), world_.overworld_y());
        unsigned detail_seed = world_.seed()
            ^ (world_.navigation().current_system_id * 7919u)
            ^ (static_cast<unsigned>(world_.navigation().current_body_index) * 6271u)
            ^ (static_cast<unsigned>(world_.navigation().current_moon_index + 1) * 3571u)
            ^ (static_cast<unsigned>(world_.overworld_x()) * 1013u)
            ^ (static_cast<unsigned>(world_.overworld_y()) * 2039u);

        world_.map() = TileMap(props.width, props.height, MapType::DetailMap);
        auto gen = create_generator(MapType::DetailMap);
        gen->generate(world_.map(), props, detail_seed);
        world_.map().set_biome(props.biome);

        world_.npcs().clear();
        world_.ground_items().clear();
        world_.map().find_open_spot(player_.x, player_.y);
        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    world_.current_region() = -1;
    recompute_fov();
    compute_camera();
    check_region_change();
    log("You return to the surface.");
}

void Game::transition_detail_edge(int dx, int dy) {
    int new_zx = world_.zone_x() + dx;
    int new_zy = world_.zone_y() + dy;

    // Check if we're still within the 3x3 zone grid
    bool crossing_overworld = (new_zx < 0 || new_zx >= zones_per_tile ||
                               new_zy < 0 || new_zy >= zones_per_tile);

    int new_ow_x = world_.overworld_x();
    int new_ow_y = world_.overworld_y();
    int time_cost = 5; // intra-tile zone transition

    if (crossing_overworld) {
        // Wrap zone and shift overworld tile
        int ow_dx = 0, ow_dy = 0;
        if (new_zx < 0)             { ow_dx = -1; new_zx = zones_per_tile - 1; }
        if (new_zx >= zones_per_tile) { ow_dx = 1;  new_zx = 0; }
        if (new_zy < 0)             { ow_dy = -1; new_zy = zones_per_tile - 1; }
        if (new_zy >= zones_per_tile) { ow_dy = 1;  new_zy = 0; }

        new_ow_x += ow_dx;
        new_ow_y += ow_dy;

        // Check overworld bounds and passability
        LocationKey ow_key = {world_.navigation().current_system_id,
                              world_.navigation().current_body_index,
                              world_.navigation().current_moon_index,
                              false, -1, -1, 0, -1, -1};

        auto ow_it = world_.location_cache().find(ow_key);
        if (ow_it == world_.location_cache().end()) {
            log("You can't go that way.");
            return;
        }

        const auto& ow_map = ow_it->second.map;
        if (new_ow_x < 0 || new_ow_x >= ow_map.width() ||
            new_ow_y < 0 || new_ow_y >= ow_map.height()) {
            log("You've reached the edge of this region.");
            return;
        }

        Tile dest_tile = ow_map.get(new_ow_x, new_ow_y);
        if (dest_tile == Tile::OW_Mountains || dest_tile == Tile::OW_Lake) {
            log("Impassable terrain blocks your path.");
            return;
        }

        // Update overworld position on cached map
        ow_it->second.player_x = new_ow_x;
        ow_it->second.player_y = new_ow_y;

        time_cost = 15;
        // Terrain lore halves cross-tile travel
        Tile dest_ow = ow_map.get(new_ow_x, new_ow_y);
        SkillId lore = terrain_lore_for(dest_ow);
        if (static_cast<uint32_t>(lore) != 0 && player_has_skill(player_, lore))
            time_cost /= 2;
    }

    // Save current zone
    save_current_location();

    // Update position
    world_.overworld_x() = new_ow_x;
    world_.overworld_y() = new_ow_y;
    world_.zone_x() = new_zx;
    world_.zone_y() = new_zy;

    // Generate or restore the target zone
    auto props = build_detail_props(new_ow_x, new_ow_y);

    LocationKey new_detail_key = {world_.navigation().current_system_id,
                                  world_.navigation().current_body_index,
                                  world_.navigation().current_moon_index,
                                  false, new_ow_x, new_ow_y, 0, new_zx, new_zy};

    if (world_.location_cache().count(new_detail_key)) {
        restore_location(new_detail_key);
    } else {
        unsigned detail_seed = world_.seed()
            ^ (world_.navigation().current_system_id * 7919u)
            ^ (static_cast<unsigned>(world_.navigation().current_body_index) * 6271u)
            ^ (static_cast<unsigned>(world_.navigation().current_moon_index + 1) * 3571u)
            ^ (static_cast<unsigned>(new_ow_x) * 1013u)
            ^ (static_cast<unsigned>(new_ow_y) * 2039u)
            ^ (static_cast<unsigned>(new_zx) * 4517u)
            ^ (static_cast<unsigned>(new_zy) * 5381u);

        world_.map() = TileMap(props.width, props.height, MapType::DetailMap);
        auto gen = create_generator(MapType::DetailMap);
        gen->generate(world_.map(), props, detail_seed);
        world_.map().set_biome(props.biome);

        world_.npcs().clear();
        world_.ground_items().clear();
        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    // Place player at opposite edge, preserving position on the other axis
    int prev_x = player_.x;
    int prev_y = player_.y;

    if (dx == -1) player_.x = world_.map().width() - 2;
    else if (dx == 1) player_.x = 1;
    else player_.x = std::clamp(prev_x, 1, world_.map().width() - 2);

    if (dy == -1) player_.y = world_.map().height() - 2;
    else if (dy == 1) player_.y = 1;
    else player_.y = std::clamp(prev_y, 1, world_.map().height() - 2);

    // Ensure we're on a passable tile
    if (!world_.map().passable(player_.x, player_.y)) {
        world_.map().find_open_spot(player_.x, player_.y);
    }

    world_.current_region() = -1;
    recompute_fov();
    compute_camera();
    check_region_change();
    advance_world(time_cost);
}

void Game::travel_to_destination(const ChartAction& action) {
    if (action.system_index < 0 ||
        action.system_index >= static_cast<int>(world_.navigation().systems.size()))
        return;

    auto& target_sys = world_.navigation().systems[action.system_index];
    generate_system_bodies(target_sys);

    // Determine destination key and metadata
    LocationKey dest_key;
    MapType dest_type = MapType::SpaceStation;
    Biome dest_biome = Biome::Station;
    std::string location_name;

    switch (action.type) {
        case ChartActionType::WarpToSystem: {
            // Warp puts you on your ship in the new system
            save_current_location();
            world_.navigation().current_system_id = target_sys.id;
            discover_nearby(world_.navigation(), target_sys.id, 20.0f);
            world_.navigation().on_ship = true;
            world_.navigation().at_station = false;
            world_.navigation().current_body_index = -1;
            world_.navigation().current_moon_index = -1;

            if (world_.location_cache().count(WorldManager::ship_key)) {
                restore_location(WorldManager::ship_key);
            } else {
                // Generate ship for the first time
                unsigned ship_seed = world_.seed() ^ 0x5B1Bu;
                auto props = default_properties(MapType::Starship);
                world_.map() = TileMap(props.width, props.height, MapType::Starship);
                auto gen = create_starship_generator();
                gen->generate(world_.map(), props, ship_seed);
                world_.map().set_location_name("Your Starship");
                world_.npcs().clear();
                world_.ground_items().clear();
                if (!world_.map().find_open_spot_in_region(0, player_.x, player_.y, {})) {
                    world_.map().find_open_spot(player_.x, player_.y);
                }
                world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
            }

            world_.visibility().reveal_all();
            world_.current_region() = -1;
            recompute_fov();
            compute_camera();
            check_region_change();
            log("Warp drive engaged...");
            log("You arrive at " + colored(target_sys.name, Color::Yellow)
                + ". Open the star chart to navigate.");
            return;
        }
        case ChartActionType::TravelToStation: {
            dest_key = LocationKey{target_sys.id, -1, -1, true, -1, -1, 0, -1, -1};
            dest_type = target_sys.station.derelict ? MapType::DerelictStation
                                                    : MapType::SpaceStation;
            dest_biome = Biome::Station;
            location_name = target_sys.station.name;
            break;
        }
        case ChartActionType::TravelToBody: {
            if (action.body_index < 0 ||
                action.body_index >= static_cast<int>(target_sys.bodies.size()))
                return;

            dest_key = LocationKey{target_sys.id, action.body_index, action.moon_index, false, -1, -1, 0, -1, -1};
            const auto& body = target_sys.bodies[action.body_index];

            // Map body type to map type
            switch (body.type) {
                case BodyType::Rocky:
                case BodyType::Terrestrial:
                    dest_type = MapType::Rocky;
                    break;
                case BodyType::DwarfPlanet:
                case BodyType::AsteroidBelt:
                    dest_type = MapType::Asteroid;
                    break;
                default:
                    dest_type = MapType::Rocky;
                    break;
            }

            // Determine biome from body or moon properties
            unsigned biome_seed = world_.seed() ^ (target_sys.id * 997u)
                                ^ (static_cast<unsigned>(action.body_index) * 6271u);

            if (action.moon_index >= 0) {
                // Generate independent moon body
                unsigned moon_seed = world_.seed() ^ (target_sys.id * 7919u)
                                   ^ (static_cast<unsigned>(action.body_index) * 6271u);
                auto moon = generate_moon_body(body, action.moon_index, moon_seed);
                biome_seed ^= (static_cast<unsigned>(action.moon_index) * 4219u);
                dest_biome = determine_biome(moon.type, moon.atmosphere, moon.temperature, biome_seed);
                location_name = moon.name;
            } else {
                dest_biome = determine_biome(body.type, body.atmosphere, body.temperature, biome_seed);
                location_name = body.name;
            }
            break;
        }
        default:
            return;
    }

    // Save current location before leaving
    save_current_location();

    // Update navigation state
    switch (action.type) {
        case ChartActionType::TravelToStation:
            world_.navigation().on_ship = false;
            world_.navigation().at_station = true;
            world_.navigation().current_body_index = -1;
            world_.navigation().current_moon_index = -1;
            world_.day_clock().set_body_day_length(200); // station standard day
            break;
        case ChartActionType::TravelToBody: {
            world_.navigation().on_ship = false;
            world_.navigation().at_station = false;
            world_.navigation().current_body_index = action.body_index;
            world_.navigation().current_moon_index = action.moon_index;
            const auto& body = target_sys.bodies[action.body_index];
            if (action.moon_index >= 0) {
                unsigned moon_seed = world_.seed() ^ (target_sys.id * 7919u)
                                   ^ (static_cast<unsigned>(action.body_index) * 6271u);
                auto moon = generate_moon_body(body, action.moon_index, moon_seed);
                world_.day_clock().set_body_day_length(moon.day_length);
            } else {
                world_.day_clock().set_body_day_length(body.day_length);
            }
            break;
        }
        default:
            break;
    }

    // Body destinations go to an overworld instead of directly to a cave
    if (action.type == ChartActionType::TravelToBody) {
        const auto& body = target_sys.bodies[action.body_index];

        // Check cache for overworld
        if (world_.location_cache().count(dest_key)) {
            restore_location(dest_key);
        } else {
            // Generate overworld from body or moon properties
            auto props = default_properties(MapType::Overworld);
            props.biome = dest_biome;

            if (action.moon_index >= 0) {
                unsigned moon_seed = world_.seed() ^ (target_sys.id * 7919u)
                                   ^ (static_cast<unsigned>(action.body_index) * 6271u);
                auto moon = generate_moon_body(body, action.moon_index, moon_seed);
                props.body_type = moon.type;
                props.body_atmosphere = moon.atmosphere;
                props.body_temperature = moon.temperature;
                props.body_has_dungeon = moon.has_dungeon;
                props.body_danger_level = moon.danger_level;
            } else {
                props.body_type = body.type;
                props.body_atmosphere = body.atmosphere;
                props.body_temperature = body.temperature;
                props.body_has_dungeon = body.has_dungeon;
                props.body_danger_level = body.danger_level;
            }

            // Apply lore annotations to overworld properties
            const auto& la = target_sys.lore;
            props.lore_tier = la.lore_tier;
            props.lore_battle_site = la.battle_site;
            props.lore_weapon_test = la.weapon_test_site;
            props.lore_megastructure = la.has_megastructure;
            props.lore_beacon = la.beacon;
            props.lore_terraformed = la.terraformed;
            props.lore_plague_origin = la.plague_origin;
            props.lore_scar_count = la.scar_count;
            props.lore_civ_architecture = la.primary_civ_architecture;
            props.lore_primary_civ_index = la.primary_civ_index;

            unsigned ow_seed = world_.seed() ^ (target_sys.id * 7919u)
                             ^ (static_cast<unsigned>(action.body_index) * 6271u)
                             ^ (static_cast<unsigned>(action.moon_index + 1) * 3571u);

            world_.map() = TileMap(props.width, props.height, MapType::Overworld);
            auto gen = create_generator(MapType::Overworld);
            gen->generate(world_.map(), props, ow_seed);
            world_.map().set_biome(dest_biome);
            world_.map().set_location_name(location_name);

            world_.npcs().clear();
            world_.ground_items().clear();

            // Find landing tile for player spawn
            bool found_landing = false;
            for (int y = 0; y < world_.map().height() && !found_landing; ++y) {
                for (int x = 0; x < world_.map().width() && !found_landing; ++x) {
                    if (world_.map().get(x, y) == Tile::OW_Landing) {
                        player_.x = x;
                        player_.y = y;
                        found_landing = true;
                    }
                }
            }
            if (!found_landing) {
                player_.x = world_.map().width() / 2;
                player_.y = world_.map().height() / 2;
            }

            world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
        }

        world_.set_surface_mode(SurfaceMode::Overworld);
        world_.overworld_x() = 0;
        world_.overworld_y() = 0;
        world_.visibility().reveal_all();
        animations_.spawn_fixture_anims(world_.map(), world_.visibility());
        world_.current_region() = -1;
        compute_camera();
        log("You land on " + colored(location_name, Color::Cyan)
            + ". The surface stretches before you.");

        // Notify quest system of arrival at this body
        quest_manager_.on_location_entered(location_name);
        return;
    }

    // Station destinations
    if (world_.location_cache().count(dest_key)) {
        restore_location(dest_key);
    } else {
        // Generate fresh map
        unsigned travel_seed = world_.rng()();
        auto props = default_properties(dest_type);
        props.biome = dest_biome;
        world_.map() = TileMap(props.width, props.height, dest_type);
        auto gen = create_generator(dest_type);
        gen->generate(world_.map(), props, travel_seed);
        world_.map().set_location_name(location_name);

        world_.npcs().clear();
        world_.ground_items().clear();
        world_.map().find_open_spot(player_.x, player_.y);

        std::mt19937 npc_rng(travel_seed ^ 0xD3ADu);
        std::vector<std::pair<int,int>> occupied = {{player_.x, player_.y}};
        debug_spawn(world_.map(), world_.npcs(), player_.x, player_.y, occupied, npc_rng);

        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    // Place ShipTerminal at stations so the player can re-board
    {
        bool has_terminal = false;
        for (int i = 0; i < world_.map().fixture_count(); ++i) {
            if (world_.map().fixture(i).type == FixtureType::ShipTerminal) {
                has_terminal = true;
                break;
            }
        }
        if (!has_terminal) {
            std::vector<std::pair<int,int>> occupied = {{player_.x, player_.y}};
            for (const auto& npc : world_.npcs()) {
                occupied.push_back({npc.x, npc.y});
            }
            int tx, ty;
            if (world_.map().find_open_spot_near(player_.x, player_.y, tx, ty, occupied, &world_.rng())) {
                world_.map().add_fixture(tx, ty, make_fixture(FixtureType::ShipTerminal));
            }
        }
    }

    world_.set_surface_mode(SurfaceMode::Dungeon);
    world_.current_region() = -1;
    recompute_fov();
    compute_camera();
    check_region_change();
    log("You dock at " + colored(location_name, Color::Cyan) + ".");
}


void Game::compute_camera() {
    // Center camera on look cursor if in look mode, otherwise on player
    int focus_x = input_.looking() ? input_.look_x() : player_.x;
    int focus_y = input_.looking() ? input_.look_y() : player_.y;
    camera_x_ = focus_x - map_rect_.w / 2;
    camera_y_ = focus_y - map_rect_.h / 2;

    if (camera_x_ < 0) camera_x_ = 0;
    if (camera_y_ < 0) camera_y_ = 0;
    if (camera_x_ + map_rect_.w > world_.map().width()) camera_x_ = world_.map().width() - map_rect_.w;
    if (camera_y_ + map_rect_.h > world_.map().height()) camera_y_ = world_.map().height() - map_rect_.h;
    if (camera_x_ < 0) camera_x_ = 0;
    if (camera_y_ < 0) camera_y_ = 0;
}

void Game::recompute_fov() {
    if (world_.navigation().on_ship) {
        world_.visibility().reveal_all();
        return;
    }

    // Determine effective view radius based on context
    int radius = player_.view_radius + player_.equipment.total_modifiers().view_radius;
    bool is_indoor = world_.map().map_type() == MapType::SpaceStation
                  || world_.map().map_type() == MapType::DerelictStation
                  || world_.map().map_type() == MapType::Starship;
    bool is_dungeon = !world_.on_overworld() && !world_.on_detail_map()
                      && world_.map().map_type() != MapType::DetailMap
                      && !is_indoor;
    if (is_indoor) {
        // Stations and ships: always fully lit, use base view_radius
    } else if (is_dungeon) {
        // Underground: always use light_radius (no sunlight)
        radius = player_.light_radius;
    } else if (!world_.on_overworld()) {
        // Surface detail maps: time of day affects view range
        int max_radius = std::max(world_.map().width(), world_.map().height());
        radius = world_.day_clock().effective_view_radius(max_radius, player_.light_radius);
    }

    compute_fov(world_.map(), world_.visibility(), player_.x, player_.y, radius);

    // Light source pass: visible light-emitting fixtures extend FOV
    {
        std::vector<LightSource> lights;
        for (int y = 0; y < world_.map().height(); ++y) {
            for (int x = 0; x < world_.map().width(); ++x) {
                if (world_.visibility().get(x, y) != Visibility::Visible) continue;
                if (world_.map().get(x, y) != Tile::Fixture) continue;
                int fid = world_.map().fixture_id(x, y);
                if (fid < 0) continue;
                int lr = world_.map().fixture(fid).light_radius;
                if (lr > 0) lights.push_back({x, y, lr});
            }
        }
        if (!lights.empty()) {
            compute_fov_lit(world_.map(), world_.visibility(),
                            player_.x, player_.y, lights);
        }
    }

    // Detail maps: shadowcast for lighting, but entire map stays revealed
    if (world_.on_detail_map() || world_.map().map_type() == MapType::DetailMap) {
        world_.visibility().explore_all();
        animations_.spawn_fixture_anims(world_.map(), world_.visibility());
        return;
    }

    std::vector<bool> reveal(world_.map().region_count(), false);
    for (int y = 0; y < world_.map().height(); ++y) {
        for (int x = 0; x < world_.map().width(); ++x) {
            if (world_.visibility().get(x, y) == Visibility::Visible) {
                int rid = world_.map().region_id(x, y);
                if (rid >= 0 && world_.map().region(rid).lit) {
                    reveal[rid] = true;
                }
            }
        }
    }

    for (int y = 0; y < world_.map().height(); ++y) {
        for (int x = 0; x < world_.map().width(); ++x) {
            int rid = world_.map().region_id(x, y);
            if (rid >= 0 && reveal[rid]) {
                world_.visibility().set_visible(x, y);
            }
        }
    }

    // Add fixture animations for newly visible tiles (preserves existing ones)
    animations_.spawn_fixture_anims(world_.map(), world_.visibility());
}

void Game::advance_world(int cost) {
    // Grant energy to all NPCs on the current map
    for (auto& npc : world_.npcs()) {
        npc.energy += cost * npc.quickness / 100;
    }

    // Process NPC turns until no NPC can act
    bool acted = true;
    while (acted) {
        acted = false;
        for (auto& npc : world_.npcs()) {
            while (npc.energy >= energy_threshold) {
                npc.energy -= energy_threshold;
                combat_.process_npc_turn(npc, *this);
                acted = true;
            }
        }
    }

    combat_.remove_dead_npcs(*this);
    check_player_death();
    ++world_.world_tick();
    world_.day_clock().advance(1);

    // Tick and expire effects
    tick_effects(player_.effects, player_.hp, player_.effective_max_hp());
    expire_effects(player_.effects);
    for (auto& npc : world_.npcs()) {
        if (npc.alive()) {
            tick_effects(npc.effects, npc.hp, npc.max_hp);
            expire_effects(npc.effects);
        }
    }

    // Water damage
    if (player_.hp > 0 && world_.map().get(player_.x, player_.y) == Tile::Water) {
        int water_dmg = apply_damage_effects(player_.effects, 1);
        player_.hp -= water_dmg;
        if (player_.hp < 0) player_.hp = 0;
        switch (world_.map().biome()) {
            case Biome::Fungal:
                log("Spores sting as you wade through the pool. (-1 HP)");
                break;
            case Biome::Crystal:
                log("Sharp mineral deposits cut at your legs. (-1 HP)");
                break;
            case Biome::Corroded:
                log("The toxic sludge burns! (-1 HP)");
                break;
            case Biome::Aquatic:
                log("The underground current pulls at you. (-1 HP)");
                break;
            default:
                log("The dark water chills you to the bone. (-1 HP)");
                break;
        }
        if (player_.hp <= 0) {
            death_message_ = "Drowned";
        }
        check_player_death();
    }

    // Passive health regeneration
    if (player_.hp > 0 && player_.hp < player_.max_hp) {
        int interval = regen_interval(player_.hunger);
        if (interval > 0) {
            ++player_.regen_counter;
            if (player_.regen_counter >= interval) {
                player_.regen_counter = 0;
                ++player_.hp;
                log("You feel a little better.");
            }
        }
    }
}


// ─────────────────────────────────────────────────────────────────
// Lost mechanic
// ─────────────────────────────────────────────────────────────────

int Game::get_lost_chance(Tile terrain) const {
    int base = 15;
    // Wayfinding category unlock gives small flat reduction
    if (player_has_skill(player_, SkillId::Cat_Wayfinding)) base -= 2;
    // Terrain lore halves chance for matching terrain
    SkillId lore = terrain_lore_for(terrain);
    if (static_cast<uint32_t>(lore) != 0 && player_has_skill(player_, lore))
        base /= 2;
    return std::max(base, 1);
}

int Game::regain_chance() const {
    int grace = 30;
    int ramp_divisor = 3;
    int cap = 25;
    if (player_has_skill(player_, SkillId::CompassSense)) {
        grace = 15;
        ramp_divisor = 2;
        cap = 40;
    }
    if (lost_moves_ < grace) return 0;
    int chance = (lost_moves_ - grace) / ramp_divisor;
    return std::min(chance, cap);
}

void Game::check_get_lost() {
    if (dev_mode_) return;  // dev never gets lost
    if (lost_) return;      // already lost

    Tile terrain = world_.map().get(player_.x, player_.y);
    int chance = get_lost_chance(terrain);

    std::uniform_int_distribution<int> dist(1, 100);
    if (dist(world_.rng()) > chance) return; // not lost

    // Mark as lost and show popup — stay on overworld until dismissed
    lost_ = true;
    lost_pending_ = true;
    lost_moves_ = 0;

    lost_popup_.reset();
    lost_popup_.title = "Lost!";
    lost_popup_.body =
        "The terrain all looks the same. You've lost your bearings "
        "and can't find your way back to the surface view.\n\n"
        "Keep moving to regain your sense of direction.";
    lost_popup_.add_option('f', "Press on");
    lost_popup_.footer = "[Space] Continue";
    lost_popup_.selection = 0;
    lost_popup_.open = true;
}

void Game::enter_lost_detail() {
    lost_pending_ = false;

    world_.overworld_x() = player_.x;
    world_.overworld_y() = player_.y;

    // Random zone within the 3x3 grid
    std::uniform_int_distribution<int> zone_dist(0, zones_per_tile - 1);
    world_.zone_x() = zone_dist(world_.rng());
    world_.zone_y() = zone_dist(world_.rng());

    auto props = build_detail_props(world_.overworld_x(), world_.overworld_y());
    std::string body_name = world_.map().location_name();

    LocationKey detail_key = {world_.navigation().current_system_id,
                              world_.navigation().current_body_index,
                              world_.navigation().current_moon_index,
                              false, world_.overworld_x(), world_.overworld_y(), 0,
                              world_.zone_x(), world_.zone_y()};

    save_current_location();
    world_.set_surface_mode(SurfaceMode::DetailMap);

    if (world_.location_cache().count(detail_key)) {
        restore_location(detail_key);
    } else {
        unsigned detail_seed = world_.seed()
            ^ (world_.navigation().current_system_id * 7919u)
            ^ (static_cast<unsigned>(world_.navigation().current_body_index) * 6271u)
            ^ (static_cast<unsigned>(world_.navigation().current_moon_index + 1) * 3571u)
            ^ (static_cast<unsigned>(world_.overworld_x()) * 1013u)
            ^ (static_cast<unsigned>(world_.overworld_y()) * 2039u)
            ^ (static_cast<unsigned>(world_.zone_x()) * 4517u)
            ^ (static_cast<unsigned>(world_.zone_y()) * 5381u);

        world_.map() = TileMap(props.width, props.height, MapType::DetailMap);
        auto gen = create_generator(MapType::DetailMap);
        gen->generate(world_.map(), props, detail_seed);
        world_.map().set_biome(props.biome);
        world_.map().set_location_name(body_name);

        world_.npcs().clear();
        world_.ground_items().clear();
        world_.visibility() = VisibilityMap(world_.map().width(), world_.map().height());
    }

    // Random position within the zone
    player_.x = world_.map().width() / 2;
    player_.y = world_.map().height() / 2;
    if (!world_.map().passable(player_.x, player_.y))
        world_.map().find_open_spot(player_.x, player_.y);

    world_.current_region() = -1;
    recompute_fov();
    compute_camera();
}

void Game::check_regain_bearings() {
    if (!lost_) return;
    if (!world_.on_detail_map()) return;

    ++lost_moves_;

    int chance = regain_chance();
    std::uniform_int_distribution<int> dist(1, 100);
    if (dist(world_.rng()) > chance) return;

    // Regained bearings
    lost_ = false;
    lost_moves_ = 0;

    lost_popup_.reset();
    lost_popup_.title = "Bearings Regained";
    lost_popup_.body =
        "You recognize a landmark and regain your sense of direction. "
        "You can now return to the surface view.";
    lost_popup_.add_option('f', "Continue");
    lost_popup_.footer = "[Space] Continue";
    lost_popup_.selection = 0;
    lost_popup_.open = true;
}

} // namespace astra
