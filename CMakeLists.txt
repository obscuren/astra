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
    src/fov.cpp
    src/npc.cpp
    src/npcs/station_keeper.cpp
    src/npcs/merchant.cpp
    src/npcs/drifter.cpp
    src/npcs/xytomorph.cpp
    src/npcs/nova.cpp
    src/npcs/hub_npcs.cpp
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
    src/item_defs.cpp
    src/character.cpp
    src/shop.cpp
    src/trade_window.cpp
    src/character_screen.cpp
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

if(SDL)
    target_compile_definitions(${ASTRA_BIN} PRIVATE ASTRA_HAS_SDL=1)
    target_include_directories(${ASTRA_BIN} PRIVATE ${SDL3_INCLUDE_DIRS} ${SDL3_TTF_INCLUDE_DIRS})
    target_link_directories(${ASTRA_BIN} PRIVATE ${SDL3_LIBRARY_DIRS} ${SDL3_TTF_LIBRARY_DIRS})
    target_link_libraries(${ASTRA_BIN} PRIVATE ${SDL3_LIBRARIES} ${SDL3_TTF_LIBRARIES})
endif()
