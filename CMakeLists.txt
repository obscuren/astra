cmake_minimum_required(VERSION 3.20)
project(crawler VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SDL "Build with SDL3 renderer support" OFF)

set(CRAWLER_SOURCES
    src/main.cpp
    src/game.cpp
    src/terminal_renderer.cpp
)

if(SDL)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL3 REQUIRED sdl3)
    pkg_check_modules(SDL3_TTF REQUIRED sdl3-ttf)

    list(APPEND CRAWLER_SOURCES src/sdl_renderer.cpp)
endif()

add_executable(crawler ${CRAWLER_SOURCES})
target_include_directories(crawler PRIVATE include)

if(SDL)
    target_compile_definitions(crawler PRIVATE CRAWLER_HAS_SDL=1)
    target_include_directories(crawler PRIVATE ${SDL3_INCLUDE_DIRS} ${SDL3_TTF_INCLUDE_DIRS})
    target_link_directories(crawler PRIVATE ${SDL3_LIBRARY_DIRS} ${SDL3_TTF_LIBRARY_DIRS})
    target_link_libraries(crawler PRIVATE ${SDL3_LIBRARIES} ${SDL3_TTF_LIBRARIES})
endif()
