cmake_minimum_required(VERSION 3.20)
project(astra VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SDL "Build with SDL3 renderer support" OFF)
option(DEV "Build with developer mode enabled" OFF)

set(ASTRA_SOURCES
    src/main.cpp
    src/game.cpp
    src/ui.cpp
    src/display_name.cpp
    src/ui_layout.cpp
    src/ui_components.cpp
    src/fov.cpp
    src/npc.cpp
    src/npcs/archon_remnant.cpp
    src/npcs/archon_sentinel.cpp
    src/npcs/station_keeper.cpp
    src/npcs/merchant.cpp
    src/npcs/drifter.cpp
    src/npcs/xytomorph.cpp
    src/npcs/nova.cpp
    src/npcs/hub_npcs.cpp
    src/npcs/civilian.cpp
    src/npcs/scavenger.cpp
    src/npcs/prospector.cpp
    src/npcs/keeper_personas.cpp
    src/npcs/scav_keeper.cpp
    src/npcs/scav_merchant.cpp
    src/npcs/pirate_captain.cpp
    src/npcs/pirate_grunt.cpp
    src/npcs/black_market_vendor.cpp
    src/npcs/void_reaver.cpp
    src/npc_spawner.cpp
    src/debug_spawn.cpp
    src/tile_props.cpp
    src/tilemap.cpp
    src/map_generator.cpp
    src/generators/station_generator.cpp
    src/generators/open_cave_generator.cpp
    src/generators/tunnel_cave_generator.cpp
    src/generators/hub_station_generator.cpp
    src/generators/scav_station_generator.cpp
    src/generators/pirate_station_generator.cpp
    src/generators/derelict_station_generator.cpp
    src/generators/infested_station_generator.cpp
    src/generators/starship_generator.cpp
    src/generators/overworld_generator_base.cpp
    src/generators/default_overworld_generator.cpp
    src/generators/temperate_overworld_generator.cpp
    src/generators/cold_rocky_overworld_generator.cpp
    src/generators/scarred_overworld_generator.cpp
    src/generators/detail_map_generator.cpp
    src/generators/biome_profiles.cpp
    src/generators/elevation_strategies.cpp
    src/generators/moisture_strategies.cpp
    src/generators/structure_strategies.cpp
    src/generators/flora_strategies.cpp
    src/generators/terrain_compositor.cpp
    src/generators/edge_strip.cpp
    src/generators/detail_map_generator_v2.cpp
    src/generators/civ_styles.cpp
    src/generators/furniture_palettes.cpp
    src/generators/placement_scorer.cpp
    src/generators/settlement_planner.cpp
    src/generators/outpost_planner.cpp
    src/generators/crashed_ship_generator.cpp
    src/generators/cave_entrance_generator.cpp
    src/generators/building_generator.cpp
    src/generators/path_router.cpp
    src/generators/perimeter_builder.cpp
    src/generators/ruin_decay.cpp
    src/generators/exterior_decorator.cpp
    src/generators/poi_phase.cpp
    src/generators/poi_placement.cpp
    src/generators/ruin_civ_configs.cpp
    src/generators/bsp_generator.cpp
    src/generators/room_identifier.cpp
    src/generators/ruin_stamps.cpp
    src/generators/ruin_generator.cpp
    src/overworld_stamps.cpp
    src/poi_budget.cpp
    src/playback_viewer.cpp
    src/visibility_map.cpp
    src/save_file.cpp
    src/celestial_body.cpp
    src/body_presets.cpp
    src/star_chart.cpp
    src/star_chart_viewer.cpp
    src/item.cpp
    src/animation.cpp
    src/map_editor.cpp
    src/minimap.cpp
    src/name_generator.cpp
    src/lore_generator.cpp
    src/lore_influence_map.cpp
    src/lore_viewer.cpp
    src/narrative_templates.cpp
    src/galaxy_sim.cpp
    src/item_defs.cpp
    src/item_gen.cpp
    src/journal.cpp
    src/tinkering.cpp
    src/skill_defs.cpp
    src/character.cpp
    src/shop.cpp
    src/trade_window.cpp
    src/character_screen.cpp
    src/character_creation.cpp
    src/effect.cpp
    src/faction.cpp
    src/ship.cpp
    src/station_type.cpp
    src/station_roll.cpp
    src/help_screen.cpp
    src/input_manager.cpp
    src/dialog_manager.cpp
    src/ability.cpp
    src/quest.cpp
    src/quest_fixture.cpp
    src/quest_graph.cpp
    src/quest_validator.cpp
    src/quests/missing_hauler.cpp
    src/quests/getting_airborne.cpp
    src/quests/hauler_b.cpp
    src/quests/hauler_c.cpp
    src/quests/hauler_d.cpp
    src/quests/stellar_signal_beacon.cpp
    src/quests/stellar_signal_echoes.cpp
    src/quests/stellar_signal_hook.cpp
    src/game_combat.cpp
    src/game_input.cpp
    src/game_interaction.cpp
    src/game_world.cpp
    src/game_rendering.cpp
    src/map_renderer.cpp
    src/terminal_theme.cpp
    src/terminal_ui_theme.cpp
    src/terminal_renderer_ui.cpp
    src/terminal_renderer_galaxy.cpp
    src/repair_bench.cpp
    src/save_system.cpp
    src/dev_console.cpp
    src/boot_sequence.cpp
    src/dice.cpp
)

if(WIN32)
    list(APPEND ASTRA_SOURCES src/terminal_renderer_win.cpp)
else()
    list(APPEND ASTRA_SOURCES src/terminal_renderer.cpp)
endif()

if(SDL)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL3 REQUIRED sdl3)
    pkg_check_modules(SDL3_TTF REQUIRED sdl3-ttf)

    list(APPEND ASTRA_SOURCES src/sdl_renderer.cpp)
endif()

# ---------------------------------------------------------------------------
# Determine artifact name based on build configuration
# ---------------------------------------------------------------------------
set(ASTRA_BIN "astra")
if(DEV AND SDL)
    set(ASTRA_BIN "astra-sdl-dev")
elseif(DEV)
    set(ASTRA_BIN "astra-dev")
elseif(SDL)
    set(ASTRA_BIN "astra-sdl")
endif()

add_executable(${ASTRA_BIN} ${ASTRA_SOURCES})
target_include_directories(${ASTRA_BIN} PRIVATE include)

if(DEV)
    target_compile_definitions(${ASTRA_BIN} PRIVATE ASTRA_DEV_MODE=1)
endif()

# Version string: release builds pass -DASTRA_RELEASE_VERSION=v0.4.0 from CI
if(ASTRA_RELEASE_VERSION)
    target_compile_definitions(${ASTRA_BIN} PRIVATE ASTRA_VERSION="${ASTRA_RELEASE_VERSION}")
elseif(DEV)
    target_compile_definitions(${ASTRA_BIN} PRIVATE ASTRA_VERSION="dev build")
else()
    target_compile_definitions(${ASTRA_BIN} PRIVATE ASTRA_VERSION="")
endif()

# Static link on MinGW so the exe is self-contained (no DLL dependencies)
if(MINGW)
    target_link_options(${ASTRA_BIN} PRIVATE -static -static-libgcc -static-libstdc++)
endif()

if(SDL)
    target_compile_definitions(${ASTRA_BIN} PRIVATE ASTRA_HAS_SDL=1)
    target_include_directories(${ASTRA_BIN} PRIVATE ${SDL3_INCLUDE_DIRS} ${SDL3_TTF_INCLUDE_DIRS})
    target_link_directories(${ASTRA_BIN} PRIVATE ${SDL3_LIBRARY_DIRS} ${SDL3_TTF_LIBRARY_DIRS})
    target_link_libraries(${ASTRA_BIN} PRIVATE ${SDL3_LIBRARIES} ${SDL3_TTF_LIBRARIES})
endif()
