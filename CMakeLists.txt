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
    src/ui_layout.cpp
    src/ui_components.cpp
    src/fov.cpp
    src/npc.cpp
    src/npcs/station_keeper.cpp
    src/npcs/merchant.cpp
    src/npcs/drifter.cpp
    src/npcs/xytomorph.cpp
    src/npcs/nova.cpp
    src/npcs/hub_npcs.cpp
    src/npcs/civilian.cpp
    src/npc_spawner.cpp
    src/debug_spawn.cpp
    src/tile_props.cpp
    src/tilemap.cpp
    src/map_generator.cpp
    src/generators/station_generator.cpp
    src/generators/open_cave_generator.cpp
    src/generators/tunnel_cave_generator.cpp
    src/generators/hub_station_generator.cpp
    src/generators/derelict_station_generator.cpp
    src/generators/starship_generator.cpp
    src/generators/overworld_generator.cpp
    src/generators/detail_map_generator.cpp
    src/overworld_stamps.cpp
    src/visibility_map.cpp
    src/save_file.cpp
    src/celestial_body.cpp
    src/star_chart.cpp
    src/star_chart_viewer.cpp
    src/item.cpp
    src/animation.cpp
    src/map_editor.cpp
    src/minimap.cpp
    src/name_generator.cpp
    src/lore_generator.cpp
    src/lore_viewer.cpp
    src/narrative_templates.cpp
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
    src/help_screen.cpp
    src/input_manager.cpp
    src/dialog_manager.cpp
    src/ability.cpp
    src/quest.cpp
    src/quests/missing_hauler.cpp
    src/quests/getting_airborne.cpp
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
