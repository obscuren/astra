#pragma once

#include "astra/tilemap.h"

#include <cstdint>
#include <random>
#include <span>
#include <string_view>

namespace astra {

enum class Material : uint8_t {
    None,
    Bulkhead,       // space station metal walls
    Plating,        // space station floors
    Rock,           // asteroid / rocky dungeon
    Obsidianite,    // volcanic / lava dungeon
    Nebula,         // crystallized gas structures
};

struct TileProps {
    Material material = Material::None;
    std::span<const std::string_view> bump_messages;
};

// Look up tile properties based on tile type and map context.
TileProps tile_props(Tile tile, MapType map_type);

// Pick a random bump message for a tile. Returns empty if none.
std::string_view random_bump_message(Tile tile, MapType map_type, std::mt19937& rng);

} // namespace astra
