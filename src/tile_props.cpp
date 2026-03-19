#include "astra/tile_props.h"

namespace astra {

// --- Bump messages per map type ---

// Space station walls (bulkhead)
static constexpr std::string_view station_wall_bumps[] = {
    "You press against the cold bulkhead. It doesn't budge.",
    "The metal wall hums faintly under your hand.",
    "Your shoulder meets reinforced plating. Solid.",
    "You tap the wall. A hollow echo rings back.",
    "The bulkhead is sealed tight. No way through.",
    "Condensation beads on the wall where you touch it.",
    "A faint vibration runs through the hull plating.",
    "You lean against the wall. Somewhere behind it, machinery whirs.",
};

// Space station floors (if you somehow bump a floor — shouldn't happen, but just in case)
static constexpr std::string_view station_floor_bumps[] = {
    "The deck plating is scuffed but solid.",
};

// Rocky dungeon walls
static constexpr std::string_view rocky_wall_bumps[] = {
    "You scrape your hand against rough stone.",
    "The rock face is cold and unyielding.",
    "Dust crumbles from the wall where you push.",
    "Your fingers find no handholds in the stone.",
    "A thin vein of mineral glints in the rock.",
    "The tunnel wall is slick with moisture.",
    "You press against the rock. It's been here longer than you.",
    "Loose gravel shifts underfoot as you lean into the wall.",
};

// Lava dungeon walls (obsidianite)
static constexpr std::string_view lava_wall_bumps[] = {
    "The obsidian surface radiates heat.",
    "You pull your hand back — the rock is scorching.",
    "Glowing cracks web through the dark stone.",
    "The wall pulses with a deep, volcanic warmth.",
    "Ash flakes off at your touch.",
    "The air shimmers near the wall from the heat.",
};

// Nebula walls (crystallized gas)
static constexpr std::string_view nebula_wall_bumps[] = {
    "The crystallized gas chimes softly at your touch.",
    "Your hand passes through a faint luminous haze before meeting solid crystal.",
    "The structure hums with trapped energy.",
    "Prismatic light fractures through the translucent wall.",
    "A low resonance vibrates through the crystal lattice.",
    "The nebula wall feels cool and impossibly smooth.",
};

// Empty space (walking into the void)
static constexpr std::string_view void_bumps[] = {
    "You stare into the void between the stars.",
    "Nothing but vacuum out there.",
    "The emptiness stretches on forever.",
    "Your suit sensors warn: hard vacuum ahead.",
};

// --- Lookup ---

TileProps tile_props(Tile tile, MapType map_type) {
    if (tile == Tile::Empty) {
        return {Material::None, void_bumps};
    }

    if (tile == Tile::Wall) {
        switch (map_type) {
            case MapType::SpaceStation:
            case MapType::DerelictStation:
            case MapType::Starship:
                return {Material::Bulkhead, station_wall_bumps};
            case MapType::Rocky:
            case MapType::Asteroid:
                return {Material::Rock, rocky_wall_bumps};
            case MapType::Lava:
                return {Material::Obsidianite, lava_wall_bumps};
            case MapType::Nebula:
                return {Material::Nebula, nebula_wall_bumps};
        }
    }

    if (tile == Tile::Floor) {
        switch (map_type) {
            case MapType::SpaceStation:
                return {Material::Plating, station_floor_bumps};
            default:
                return {Material::Rock, {}};
        }
    }

    if (tile == Tile::Water || tile == Tile::Ice) {
        return {Material::None, {}};
    }

    return {Material::None, {}};
}

std::string_view random_bump_message(Tile tile, MapType map_type, std::mt19937& rng) {
    auto props = tile_props(tile, map_type);
    if (props.bump_messages.empty()) return {};
    std::uniform_int_distribution<std::size_t> dist(0, props.bump_messages.size() - 1);
    return props.bump_messages[dist(rng)];
}

} // namespace astra
