#pragma once

#include "astra/map_properties.h"
#include "astra/tilemap.h"

#include <memory>
#include <random>
#include <vector>

namespace astra {

class MapGenerator {
public:
    virtual ~MapGenerator() = default;

    // Main entry point — calls phases in order.
    void generate(TileMap& map, const MapProperties& props, unsigned seed);

protected:
    // --- Override these in subclasses ---
    virtual void generate_layout(std::mt19937& rng) = 0;
    virtual void connect_rooms(std::mt19937& rng) = 0;
    virtual void place_features(std::mt19937& rng);   // default: no-op
    virtual void assign_regions(std::mt19937& rng);    // default: basic flavor assignment
    virtual void generate_backdrop(unsigned seed);     // default: starfield if has_backdrop

    // --- Carving utilities ---
    void carve_rect(int x1, int y1, int x2, int y2, int region_id);
    void carve_corridor_h(int x1, int x2, int y, int region_id);
    void carve_corridor_v(int y1, int y2, int x, int region_id);
    bool in_bounds(int x, int y) const;

    // Access during generation
    TileMap* map_ = nullptr;
    const MapProperties* props_ = nullptr;

public:
    struct RoomRect { int x1, y1, x2, y2; };

protected:
    std::vector<RoomRect> rooms_;
};

// Factory
std::unique_ptr<MapGenerator> create_generator(MapType type);
std::unique_ptr<MapGenerator> create_hub_generator();
std::unique_ptr<MapGenerator> create_derelict_generator();

} // namespace astra
