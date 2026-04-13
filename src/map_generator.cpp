#include "astra/map_generator.h"

#include <algorithm>

namespace astra {

// --- Base class orchestration ---

void MapGenerator::generate(TileMap& map, const MapProperties& props, unsigned seed) {
    map_ = &map;
    props_ = &props;
    rooms_.clear();

    map.clear_all();
    map.set_biome(props.biome);

    std::mt19937 rng(seed);

    generate_layout(rng);
    connect_rooms(rng);
    place_features(rng);
    assign_regions(rng);
    generate_backdrop(seed);

    map_ = nullptr;
    props_ = nullptr;
}

// --- Default virtual implementations ---

void MapGenerator::place_features(std::mt19937& /*rng*/) {
    // No-op by default
}

// --- Flavor tables ---

struct FlavorEntry {
    RoomFlavor flavor;
    const char* name;
    const char* enter_message;
};

static constexpr FlavorEntry station_room_flavors[] = {
    {RoomFlavor::EmptyRoom,     "Empty Compartment",
        "An empty compartment. Dust motes drift in the recycled air."},
    {RoomFlavor::Cantina,       "Cantina",
        "The faint smell of synth-brew lingers. A cantina, long since abandoned."},
    {RoomFlavor::StorageBay,    "Storage Bay",
        "Crates and containers line the walls. A storage bay, mostly picked clean."},
    {RoomFlavor::CrewQuarters,  "Crew Quarters",
        "Bunks are bolted to the walls. Someone lived here once."},
    {RoomFlavor::Medbay,        "Medbay",
        "Medical equipment hums on standby. The air smells faintly of antiseptic."},
    {RoomFlavor::Engineering,   "Engineering Bay",
        "Conduits and junction boxes crowd every surface. The station's guts."},
    {RoomFlavor::CommandCenter,  "Command Center",
        "Consoles flicker with residual power. This was the nerve center."},
    {RoomFlavor::CargoHold,     "Cargo Hold",
        "A cavernous hold. Magnetic clamps line the deck, most of them empty."},
    {RoomFlavor::Armory,        "Armory",
        "Weapon racks stand in rows, most stripped bare. A few lockers remain sealed."},
    {RoomFlavor::Observatory,   "Observatory",
        "A viewport dominates the far wall. Jupiter's swirling storms fill the view."},
};

static constexpr FlavorEntry station_corridor_flavors[] = {
    {RoomFlavor::CorridorPlain,       "Corridor",
        "A standard station corridor. Overhead lights cast a sterile glow."},
    {RoomFlavor::CorridorDimLit,      "Dim Corridor",
        "The lights here flicker and buzz. Shadows pool in the corners."},
    {RoomFlavor::CorridorMaintenance, "Maintenance Shaft",
        "Exposed piping and cable runs. A maintenance access corridor."},
    {RoomFlavor::CorridorDamaged,     "Damaged Corridor",
        "Scorch marks streak the walls. Something violent happened here."},
};

static constexpr FlavorEntry derelict_room_flavors[] = {
    {RoomFlavor::DerelictBay,   "Derelict Bay",
        "Twisted metal and shattered hull plating. The station groans."},
    {RoomFlavor::HullBreach,    "Hull Breach",
        "Stars are visible through a gaping tear in the hull. Atmosphere vents into space."},
    {RoomFlavor::StorageBay,    "Ruined Storage Bay",
        "Crates lie scattered and crushed. Whatever was stored here is long gone."},
    {RoomFlavor::EmptyRoom,     "Dark Compartment",
        "An empty compartment. The silence is absolute."},
};

static constexpr FlavorEntry derelict_corridor_flavors[] = {
    {RoomFlavor::CorridorDamaged,     "Wrecked Corridor",
        "Scorch marks and buckled panels. The corridor has seen catastrophic damage."},
    {RoomFlavor::CorridorDimLit,      "Dark Passage",
        "Emergency lighting has failed. Only your lamp cuts through the void."},
};

static constexpr FlavorEntry rocky_room_flavors[] = {
    {RoomFlavor::CavernEmpty,    "Empty Cavern",
        "A rough-hewn cavern. Dripping water echoes off the stone."},
    {RoomFlavor::CavernMushroom, "Fungal Cavern",
        "Bioluminescent fungi cling to the walls, casting a sickly green glow."},
    {RoomFlavor::CavernCrystal,  "Crystal Grotto",
        "Crystalline formations jut from every surface, refracting your light."},
    {RoomFlavor::CavernPool,     "Subterranean Pool",
        "A still pool of dark water fills the center. Something glints beneath."},
    {RoomFlavor::MinedOut,       "Mined-Out Chamber",
        "Pick marks scar the walls. Whatever was here has been extracted."},
    {RoomFlavor::CollapseZone,   "Unstable Cavern",
        "Rubble litters the floor. The ceiling looks ready to give."},
};

static constexpr FlavorEntry rocky_corridor_flavors[] = {
    {RoomFlavor::CorridorPlain,       "Tunnel",
        "A narrow tunnel through the rock. Your footsteps echo ahead."},
    {RoomFlavor::CorridorDimLit,      "Dark Passage",
        "The darkness presses close. You can barely see a few meters ahead."},
    {RoomFlavor::CorridorMaintenance, "Bore Shaft",
        "Machine-cut walls. An old mining bore shaft."},
    {RoomFlavor::CorridorDamaged,     "Collapsed Passage",
        "Fallen rock narrows the path. You squeeze through carefully."},
};

template <std::size_t N>
static const FlavorEntry& pick_flavor(const FlavorEntry (&table)[N], std::mt19937& rng) {
    std::uniform_int_distribution<std::size_t> dist(0, N - 1);
    return table[dist(rng)];
}

void MapGenerator::assign_regions(std::mt19937& rng) {
    bool first_room = true;
    MapType mt = map_->map_type();
    int count = map_->region_count();

    for (int i = 0; i < count; ++i) {
        Region reg = map_->region(i);
        const FlavorEntry* entry = nullptr;

        if (reg.type == RegionType::Room) {
            if (first_room && mt == MapType::SpaceStation) {
                first_room = false;
                reg.flavor = RoomFlavor::EmptyRoom;
                reg.name = "Docking Bay";
                reg.enter_message = "The main docking bay. Shuttle clamps line the deck "
                                    "and the hum of life support fills the air.";
                map_->update_region(i, reg);
                continue;
            }
            first_room = false;

            switch (mt) {
                case MapType::SpaceStation:
                case MapType::Starship:
                    entry = &pick_flavor(station_room_flavors, rng);
                    break;
                case MapType::DerelictStation:
                    entry = &pick_flavor(derelict_room_flavors, rng);
                    break;
                case MapType::Rocky:
                case MapType::Lava:
                case MapType::Asteroid:
                case MapType::Overworld:
                case MapType::DetailMap:
                    entry = &pick_flavor(rocky_room_flavors, rng);
                    break;
                case MapType::Nebula:
                    entry = &pick_flavor(rocky_room_flavors, rng);
                    break;
            }
        } else {
            switch (mt) {
                case MapType::SpaceStation:
                case MapType::Starship:
                    entry = &pick_flavor(station_corridor_flavors, rng);
                    break;
                case MapType::DerelictStation:
                    entry = &pick_flavor(derelict_corridor_flavors, rng);
                    break;
                case MapType::Rocky:
                case MapType::Lava:
                case MapType::Nebula:
                case MapType::Asteroid:
                case MapType::Overworld:
                case MapType::DetailMap:
                    entry = &pick_flavor(rocky_corridor_flavors, rng);
                    break;
            }
        }

        if (entry) {
            reg.flavor = entry->flavor;
            reg.name = entry->name;
            reg.enter_message = entry->enter_message;
            reg.features = default_features(reg.flavor);
            map_->update_region(i, reg);
        }
    }
}

void MapGenerator::generate_backdrop(unsigned seed) {
    if (!props_->has_backdrop) return;

    std::mt19937 rng(seed ^ 0xBACDu);
    std::uniform_int_distribution<int> chance(0, 99);
    std::uniform_int_distribution<int> star_type(0, 9);

    for (int y = 0; y < map_->height(); ++y) {
        for (int x = 0; x < map_->width(); ++x) {
            if (map_->get(x, y) != Tile::Empty) continue;

            int roll = chance(rng);
            if (roll < 3) {
                int st = star_type(rng);
                char c;
                if (st < 6)       c = '.';
                else if (st < 9)  c = '*';
                else              c = '+';
                map_->set_backdrop(x, y, c);
            }
        }
    }
}

// --- Carving utilities ---

void MapGenerator::carve_rect(int x1, int y1, int x2, int y2, int region_id) {
    for (int y = y1; y <= y2; ++y) {
        for (int x = x1; x <= x2; ++x) {
            if (y == y1 || y == y2 || x == x1 || x == x2) {
                map_->set(x, y, Tile::Wall);
            } else {
                map_->set(x, y, Tile::Floor);
            }
            map_->set_region(x, y, region_id);
        }
    }
}

void MapGenerator::carve_corridor_h(int x1, int x2, int y, int region_id) {
    int lo = std::min(x1, x2);
    int hi = std::max(x1, x2);
    for (int x = lo; x <= hi; ++x) {
        map_->set(x, y, Tile::Floor);
        map_->set_region(x, y, region_id);
        if (map_->get(x, y - 1) == Tile::Empty) {
            map_->set(x, y - 1, Tile::Wall);
            map_->set_region(x, y - 1, region_id);
        }
        if (map_->get(x, y + 1) == Tile::Empty) {
            map_->set(x, y + 1, Tile::Wall);
            map_->set_region(x, y + 1, region_id);
        }
    }
}

void MapGenerator::carve_corridor_v(int y1, int y2, int x, int region_id) {
    int lo = std::min(y1, y2);
    int hi = std::max(y1, y2);
    for (int y = lo; y <= hi; ++y) {
        map_->set(x, y, Tile::Floor);
        map_->set_region(x, y, region_id);
        if (map_->get(x - 1, y) == Tile::Empty) {
            map_->set(x - 1, y, Tile::Wall);
            map_->set_region(x - 1, y, region_id);
        }
        if (map_->get(x + 1, y) == Tile::Empty) {
            map_->set(x + 1, y, Tile::Wall);
            map_->set_region(x + 1, y, region_id);
        }
    }
}

bool MapGenerator::in_bounds(int x, int y) const {
    return x >= 0 && x < map_->width() && y >= 0 && y < map_->height();
}

// --- default_properties factory ---

MapProperties default_properties(MapType type) {
    MapProperties p;
    switch (type) {
        case MapType::SpaceStation:
            p.environment = Environment::Station;
            p.climate = Climate::Vacuum;
            p.has_backdrop = true;
            p.room_count_min = 5;
            p.room_count_max = 8;
            p.width = 120;
            p.height = 60;
            break;
        case MapType::DerelictStation:
            p.environment = Environment::Derelict;
            p.climate = Climate::Vacuum;
            p.has_backdrop = true;
            p.room_count_min = 4;
            p.room_count_max = 7;
            p.light_bias = 0;
            p.width = 120;
            p.height = 60;
            break;
        case MapType::Rocky:
            p.environment = Environment::Cave;
            p.climate = Climate::Temperate;
            p.has_backdrop = false;
            p.room_count_min = 4;
            p.room_count_max = 10;
            p.light_bias = 30;
            p.width = 120;
            p.height = 60;
            break;
        case MapType::Lava:
            p.environment = Environment::Cave;
            p.climate = Climate::Volcanic;
            p.has_backdrop = false;
            p.room_count_min = 4;
            p.room_count_max = 8;
            p.light_bias = 40;
            p.width = 120;
            p.height = 60;
            break;
        case MapType::Nebula:
            p.environment = Environment::Surface;
            p.climate = Climate::Irradiated;
            p.has_backdrop = true;
            p.room_count_min = 5;
            p.room_count_max = 10;
            p.width = 120;
            p.height = 60;
            break;
        case MapType::Asteroid:
            p.environment = Environment::Cave;
            p.climate = Climate::Vacuum;
            p.has_backdrop = false;
            p.room_count_min = 5;
            p.room_count_max = 9;
            p.light_bias = 20;
            p.width = 120;
            p.height = 60;
            break;
        case MapType::Starship:
            p.environment = Environment::Station;
            p.climate = Climate::Vacuum;
            p.has_backdrop = true;
            p.room_count_min = 4;
            p.room_count_max = 4;
            p.width = 50;
            p.height = 20;
            break;
        case MapType::Overworld:
            p.environment = Environment::Surface;
            p.climate = Climate::Temperate;
            p.has_backdrop = false;
            p.room_count_min = 0;
            p.room_count_max = 0;
            p.width = 120;
            p.height = 60;
            break;
        case MapType::DetailMap:
            p.environment = Environment::Surface;
            p.climate = Climate::Temperate;
            p.has_backdrop = false;
            p.room_count_min = 0;
            p.room_count_max = 0;
            p.light_bias = 100;
            p.width = 360;
            p.height = 150;
            break;
    }
    return p;
}

// --- Factory ---

// Forward declarations of concrete generators
std::unique_ptr<MapGenerator> make_station_generator();
std::unique_ptr<MapGenerator> make_derelict_station_generator();
std::unique_ptr<MapGenerator> make_derelict_station_generator(const StationContext& ctx);
std::unique_ptr<MapGenerator> make_infested_station_generator(const StationContext& ctx);
std::unique_ptr<MapGenerator> make_open_cave_generator();
std::unique_ptr<MapGenerator> make_tunnel_cave_generator();
std::unique_ptr<MapGenerator> make_hub_station_generator();
std::unique_ptr<MapGenerator> make_hub_station_generator(const StationContext& ctx);
std::unique_ptr<MapGenerator> make_scav_station_generator(const StationContext& ctx);
std::unique_ptr<MapGenerator> make_pirate_station_generator(const StationContext& ctx);
std::unique_ptr<MapGenerator> make_starship_generator();
std::unique_ptr<MapGenerator> make_overworld_generator(const MapProperties& props);
std::unique_ptr<MapGenerator> make_detail_map_generator();
std::unique_ptr<MapGenerator> make_detail_map_generator_v2();

std::unique_ptr<MapGenerator> create_generator(MapType type) {
    switch (type) {
        case MapType::SpaceStation:
            return make_station_generator();
        case MapType::DerelictStation:
            return make_derelict_station_generator();
        case MapType::Rocky:
        case MapType::Lava:
            return make_open_cave_generator();
        case MapType::Asteroid:
            return make_tunnel_cave_generator();
        case MapType::Nebula:
            // Fall back to station for now
            return make_station_generator();
        case MapType::Starship:
            return make_starship_generator();
        case MapType::Overworld: {
            auto props = default_properties(MapType::Overworld);
            return make_overworld_generator(props);
        }
        case MapType::DetailMap:
            return make_detail_map_generator_v2();
    }
    return make_station_generator();
}

std::unique_ptr<MapGenerator> create_generator(MapType type, const MapProperties& props) {
    if (type == MapType::Overworld) {
        return make_overworld_generator(props);
    }
    return create_generator(type);
}

std::unique_ptr<MapGenerator> create_hub_generator() {
    return make_hub_station_generator();
}

std::unique_ptr<MapGenerator> create_derelict_generator() {
    return make_derelict_station_generator();
}

std::unique_ptr<MapGenerator> create_starship_generator() {
    return make_starship_generator();
}

std::unique_ptr<MapGenerator> create_station_generator(const StationContext& ctx) {
    // Dispatch to the appropriate generator based on station type.
    // THA (The Heavens Above, Sol id=1) always uses the hub generator.
    // Non-THA NormalHub stations fall back to make_station_generator() until Task 5
    // provides a dedicated non-THA hub generator.
    switch (ctx.type) {
        case StationType::NormalHub:
            return make_hub_station_generator(ctx);
        case StationType::Scav:
            return make_scav_station_generator(ctx);
        case StationType::Pirate:
            return make_pirate_station_generator(ctx);
        case StationType::Abandoned:
            return make_derelict_station_generator(ctx);
        case StationType::Infested:
            return make_infested_station_generator(ctx);
    }
    return make_station_generator();
}

} // namespace astra
