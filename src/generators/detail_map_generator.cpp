#include "astra/map_generator.h"
#include "astra/map_properties.h"

#include <algorithm>
#include <cmath>

namespace astra {

class DetailMapGenerator : public MapGenerator {
protected:
    void generate_layout(std::mt19937& rng) override;
    void connect_rooms(std::mt19937& /*rng*/) override {}
    void place_features(std::mt19937& rng) override;
    void assign_regions(std::mt19937& /*rng*/) override;
    void generate_backdrop(unsigned /*seed*/) override {}
};

// --- Noise ---

static float hash_noise(int x, int y, unsigned seed) {
    unsigned h = static_cast<unsigned>(x) * 374761393u
               + static_cast<unsigned>(y) * 668265263u
               + seed * 1274126177u;
    h = (h ^ (h >> 13)) * 1103515245u;
    h = h ^ (h >> 16);
    return static_cast<float>(h & 0xFFFFu) / 65535.0f;
}

static float smooth_noise(float fx, float fy, unsigned seed) {
    int ix = static_cast<int>(std::floor(fx));
    int iy = static_cast<int>(std::floor(fy));
    float dx = fx - ix;
    float dy = fy - iy;
    float sx = dx * dx * (3.0f - 2.0f * dx);
    float sy = dy * dy * (3.0f - 2.0f * dy);
    float n00 = hash_noise(ix, iy, seed);
    float n10 = hash_noise(ix + 1, iy, seed);
    float n01 = hash_noise(ix, iy + 1, seed);
    float n11 = hash_noise(ix + 1, iy + 1, seed);
    float top = n00 + sx * (n10 - n00);
    float bot = n01 + sx * (n11 - n01);
    return top + sy * (bot - top);
}

static float fbm(float x, float y, unsigned seed, float scale, int octaves = 4) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float total_amp = 0.0f;
    float freq = scale;
    for (int i = 0; i < octaves; ++i) {
        value += amplitude * smooth_noise(x * freq, y * freq, seed + i * 31u);
        total_amp += amplitude;
        amplitude *= 0.5f;
        freq *= 2.0f;
    }
    return value / total_amp;
}

// --- Terrain classification ---

static bool is_water_terrain(Tile t) {
    return t == Tile::OW_River || t == Tile::OW_Lake || t == Tile::OW_Swamp
        || t == Tile::OW_LavaFlow;
}

// What wall density (0..1) does this overworld terrain map to in detail view?
// Higher = more walls.  0 = no walls at all (pure open/water).
static float terrain_wall_density(Tile t) {
    switch (t) {
        case Tile::OW_Mountains:   return 0.80f;
        case Tile::OW_Forest:      return 0.50f;
        case Tile::OW_Fungal:      return 0.48f;
        case Tile::OW_Crater:      return 0.25f;
        case Tile::OW_IceField:    return 0.12f;
        case Tile::OW_Plains:      return 0.08f;
        case Tile::OW_Desert:      return 0.06f;
        case Tile::OW_Swamp:       return 0.15f;
        default:                   return 0.10f;
    }
}

// What water density (0..1) does this overworld terrain produce?
static float terrain_water_density(Tile t) {
    switch (t) {
        case Tile::OW_Lake:       return 0.65f;
        case Tile::OW_River:      return 0.40f;
        case Tile::OW_Swamp:      return 0.35f;
        case Tile::OW_LavaFlow:   return 0.50f;
        default:                  return 0.0f;
    }
}

// --- Edge blending ---

// For a cell at (x,y), compute how much influence each edge neighbor has.
// Returns a blend weight 0..1 for each direction.
struct EdgeWeights {
    float north = 0.0f;
    float south = 0.0f;
    float east  = 0.0f;
    float west  = 0.0f;
};

static EdgeWeights compute_edge_weights(int x, int y, int w, int h, unsigned seed) {
    EdgeWeights ew;

    // How far into the map does neighbor terrain bleed?
    // ~20% of dimension: visible ridge/water at edges without dominating center.
    float blend_zone_y = h * 0.20f;
    float blend_zone_x = w * 0.20f;

    // North: distance from top edge
    {
        float dist = static_cast<float>(y);
        // Noise varies the boundary depth per-column
        float boundary_noise = fbm(static_cast<float>(x), 0.0f, seed + 101u, 0.04f, 3);
        float depth = blend_zone_y * (0.6f + 0.8f * boundary_noise);
        if (dist < depth) {
            float t = 1.0f - (dist / depth);
            // Steep quartic falloff — strong only right at the edge
            ew.north = t * t;
        }
    }
    // South
    {
        float dist = static_cast<float>(h - 1 - y);
        float boundary_noise = fbm(static_cast<float>(x), 0.0f, seed + 201u, 0.04f, 3);
        float depth = blend_zone_y * (0.6f + 0.8f * boundary_noise);
        if (dist < depth) {
            float t = 1.0f - (dist / depth);
            ew.south = t * t;
        }
    }
    // West
    {
        float dist = static_cast<float>(x);
        float boundary_noise = fbm(0.0f, static_cast<float>(y), seed + 301u, 0.04f, 3);
        float depth = blend_zone_x * (0.6f + 0.8f * boundary_noise);
        if (dist < depth) {
            float t = 1.0f - (dist / depth);
            ew.west = t * t;
        }
    }
    // East
    {
        float dist = static_cast<float>(w - 1 - x);
        float boundary_noise = fbm(0.0f, static_cast<float>(y), seed + 401u, 0.04f, 3);
        float depth = blend_zone_x * (0.6f + 0.8f * boundary_noise);
        if (dist < depth) {
            float t = 1.0f - (dist / depth);
            ew.east = t * t;
        }
    }

    return ew;
}

// --- Layout generation ---

void DetailMapGenerator::generate_layout(std::mt19937& rng) {
    int w = map_->width();
    int h = map_->height();

    Tile terrain = props_->detail_terrain;
    unsigned seed = static_cast<unsigned>(rng());

    Tile nn = props_->detail_neighbor_n;
    Tile ns = props_->detail_neighbor_s;
    Tile ne = props_->detail_neighbor_e;
    Tile nw = props_->detail_neighbor_w;

    // Check if ANY neighbor differs from center (optimization)
    bool has_different_neighbor =
        (nn != Tile::Empty && nn != terrain) ||
        (ns != Tile::Empty && ns != terrain) ||
        (ne != Tile::Empty && ne != terrain) ||
        (nw != Tile::Empty && nw != terrain);

    // Compute center tile's densities
    float center_wall = terrain_wall_density(terrain);
    float center_water = terrain_water_density(terrain);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // Base noise value (shared across all terrain decisions)
            float n = fbm(static_cast<float>(x), static_cast<float>(y), seed, 0.08f);

            // Start with center terrain's densities
            float wall_threshold = center_wall;
            float water_threshold = center_water;

            // Blend toward neighbor densities near edges
            if (has_different_neighbor) {
                EdgeWeights ew = compute_edge_weights(x, y, w, h, seed);

                // Each edge weight blends the threshold toward that neighbor's density
                auto blend = [](float base, float target, float weight) {
                    return base + (target - base) * weight;
                };

                if (nn != Tile::Empty && ew.north > 0.0f) {
                    wall_threshold  = blend(wall_threshold,  terrain_wall_density(nn),  ew.north);
                    water_threshold = blend(water_threshold, terrain_water_density(nn), ew.north);
                }
                if (ns != Tile::Empty && ew.south > 0.0f) {
                    wall_threshold  = blend(wall_threshold,  terrain_wall_density(ns),  ew.south);
                    water_threshold = blend(water_threshold, terrain_water_density(ns), ew.south);
                }
                if (nw != Tile::Empty && ew.west > 0.0f) {
                    wall_threshold  = blend(wall_threshold,  terrain_wall_density(nw),  ew.west);
                    water_threshold = blend(water_threshold, terrain_water_density(nw), ew.west);
                }
                if (ne != Tile::Empty && ew.east > 0.0f) {
                    wall_threshold  = blend(wall_threshold,  terrain_wall_density(ne),  ew.east);
                    water_threshold = blend(water_threshold, terrain_water_density(ne), ew.east);
                }
            }

            // Classify this cell using the blended thresholds
            Tile t = Tile::Floor;

            // Special cases that need unique shapes (river, crater)
            bool special = false;

            if (terrain == Tile::OW_River) {
                // River: sinusoidal water band through center
                float center_line = h * 0.5f;
                float wander = fbm(static_cast<float>(x), 0.0f, seed + 50u, 0.04f, 3);
                center_line += (wander - 0.5f) * h * 0.3f;
                float band = 5.0f + 3.0f * fbm(static_cast<float>(x), 10.0f, seed + 51u, 0.06f, 2);
                float dist = std::abs(static_cast<float>(y) - center_line);
                if (dist < band) { t = Tile::Water; special = true; }
            }
            if (terrain == Tile::OW_Crater && !special) {
                float cx = w * 0.5f;
                float cy = h * 0.5f;
                float ddx = (x - cx) / (w * 0.45f);
                float ddy = (y - cy) / (h * 0.45f);
                float r = std::sqrt(ddx * ddx + ddy * ddy);
                float rim_noise = fbm(static_cast<float>(x), static_cast<float>(y),
                                     seed + 60u, 0.1f, 2) * 0.15f;
                if (r > (0.80f + rim_noise)) { t = Tile::Wall; special = true; }
            }

            if (!special) {
                // Water first (takes priority over walls for lakes/swamps/lava)
                if (water_threshold > 0.0f && n < water_threshold) {
                    t = Tile::Water;
                }
                // Walls: use (1 - n) so high noise = wall, threshold controls density
                else if (wall_threshold > 0.0f && n > (1.0f - wall_threshold)) {
                    t = Tile::Wall;
                }
            }

            map_->set(x, y, t);
        }
    }

    // --- Moisture pass: near-water edges create small runoff pools/streams ---
    // This happens AFTER base generation so it adds wet patches to floor tiles
    bool any_water_neighbor = is_water_terrain(nn) || is_water_terrain(ns)
                           || is_water_terrain(ne) || is_water_terrain(nw)
                           || is_water_terrain(terrain);
    if (any_water_neighbor) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (map_->get(x, y) != Tile::Floor) continue;

                // Count nearby water tiles (radius 3) to find "wet zones"
                int water_count = 0;
                for (int dy = -3; dy <= 3; ++dy) {
                    for (int dx = -3; dx <= 3; ++dx) {
                        int nx = x + dx, ny = y + dy;
                        if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                            if (map_->get(nx, ny) == Tile::Water) ++water_count;
                        }
                    }
                }

                if (water_count > 3) {
                    // Fine-scale noise for pool shapes
                    float pool = fbm(static_cast<float>(x) * 2.5f,
                                     static_cast<float>(y) * 2.5f,
                                     seed + 800u, 0.18f, 2);
                    float wetness = static_cast<float>(water_count) / 20.0f;
                    if (pool > (1.0f - wetness * 0.6f)) {
                        map_->set(x, y, Tile::Water);
                    }
                }
            }
        }
    }

    // --- Ensure walkable paths ---
    // Carve winding paths connecting all four edges through center.
    // Clears both Wall and Water so the player can always traverse.
    unsigned path_seed = seed + 900u;

    // Horizontal path
    for (int x = 0; x < w; ++x) {
        float wander = fbm(static_cast<float>(x), 0.0f, path_seed, 0.04f, 3);
        int py = static_cast<int>(h * 0.5f + (wander - 0.5f) * h * 0.25f);
        py = std::clamp(py, 2, h - 3);
        for (int dy = -1; dy <= 1; ++dy) {
            int yy = py + dy;
            if (yy >= 0 && yy < h) {
                Tile cur = map_->get(x, yy);
                if (cur == Tile::Wall || cur == Tile::Water)
                    map_->set(x, yy, Tile::Floor);
            }
        }
    }
    // Vertical path
    for (int y = 0; y < h; ++y) {
        float wander = fbm(0.0f, static_cast<float>(y), path_seed + 1u, 0.04f, 3);
        int px = static_cast<int>(w * 0.5f + (wander - 0.5f) * w * 0.25f);
        px = std::clamp(px, 2, w - 3);
        for (int dx = -1; dx <= 1; ++dx) {
            int xx = px + dx;
            if (xx >= 0 && xx < w) {
                Tile cur = map_->get(xx, y);
                if (cur == Tile::Wall || cur == Tile::Water)
                    map_->set(xx, y, Tile::Floor);
            }
        }
    }
}

// --- POI placement ---

void DetailMapGenerator::place_features(std::mt19937& rng) {
    if (!props_->detail_has_poi) return;

    int w = map_->width();
    int h = map_->height();
    int cx = w / 2;
    int cy = h / 2;

    switch (props_->detail_poi_type) {
        case Tile::OW_CaveEntrance: {
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if (in_bounds(cx + dx, cy + dy))
                        map_->set(cx + dx, cy + dy, Tile::Wall);
            map_->set(cx, cy, Tile::Portal);
            break;
        }
        case Tile::OW_Settlement: {
            int bx = cx - 3, by = cy - 2;
            for (int y = by; y <= by + 4; ++y)
                for (int x = bx; x <= bx + 6; ++x)
                    if (in_bounds(x, y))
                        map_->set(x, y, (y == by || y == by + 4 || x == bx || x == bx + 6)
                                  ? Tile::Wall : Tile::Floor);
            map_->set(cx, by + 4, Tile::Floor);
            if (in_bounds(cx - 1, cy)) {
                map_->set(cx - 1, cy, Tile::Fixture);
                map_->add_fixture(cx - 1, cy, make_fixture(FixtureType::Table));
            }
            if (in_bounds(cx + 1, cy)) {
                map_->set(cx + 1, cy, Tile::Fixture);
                map_->add_fixture(cx + 1, cy, make_fixture(FixtureType::Console));
            }
            break;
        }
        case Tile::OW_Ruins: {
            std::uniform_int_distribution<int> gap(0, 2);
            for (int i = 0; i < 3; ++i) {
                int rx = cx - 5 + i * 4;
                int ry = cy - 2;
                for (int y = ry; y <= ry + 3; ++y)
                    for (int x = rx; x <= rx + 3; ++x)
                        if (in_bounds(x, y))
                            map_->set(x, y, (gap(rng) == 0) ? Tile::Floor : Tile::Wall);
            }
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -3; dx <= 3; ++dx)
                    if (in_bounds(cx + dx, cy + dy))
                        map_->set(cx + dx, cy + dy, Tile::Floor);
            break;
        }
        case Tile::OW_CrashedShip: {
            for (int dx = -4; dx <= 4; ++dx) {
                int half_h = 2 - std::abs(dx) / 2;
                for (int dy = -half_h; dy <= half_h; ++dy)
                    if (in_bounds(cx + dx, cy + dy))
                        map_->set(cx + dx, cy + dy, Tile::Wall);
            }
            for (int dx = -2; dx <= 2; ++dx)
                for (int dy = -1; dy <= 1; ++dy)
                    if (in_bounds(cx + dx, cy + dy))
                        map_->set(cx + dx, cy + dy, Tile::Floor);
            if (in_bounds(cx, cy)) {
                map_->set(cx, cy, Tile::Fixture);
                map_->add_fixture(cx, cy, make_fixture(FixtureType::Debris));
            }
            break;
        }
        case Tile::OW_Outpost: {
            int bx = cx - 2, by = cy - 2;
            for (int y = by; y <= by + 4; ++y)
                for (int x = bx; x <= bx + 4; ++x)
                    if (in_bounds(x, y))
                        map_->set(x, y, (y == by || y == by + 4 || x == bx || x == bx + 4)
                                  ? Tile::Wall : Tile::Floor);
            map_->set(cx, by + 4, Tile::Floor);
            break;
        }
        case Tile::OW_Landing: {
            for (int dy = -2; dy <= 2; ++dy)
                for (int dx = -3; dx <= 3; ++dx)
                    if (in_bounds(cx + dx, cy + dy))
                        map_->set(cx + dx, cy + dy, Tile::Floor);
            if (in_bounds(cx, cy)) {
                map_->set(cx, cy, Tile::Fixture);
                map_->add_fixture(cx, cy, make_fixture(FixtureType::ShipTerminal));
            }
            break;
        }
        default:
            break;
    }
}

// --- Region assignment ---

void DetailMapGenerator::assign_regions(std::mt19937& /*rng*/) {
    Region reg;
    reg.type = RegionType::Room;
    reg.lit = true;
    reg.name = "Surface";
    reg.enter_message = "";
    int rid = map_->add_region(reg);

    int w = map_->width();
    int h = map_->height();
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            map_->set_region(x, y, rid);
}

// --- Factory ---

std::unique_ptr<MapGenerator> make_detail_map_generator() {
    return std::make_unique<DetailMapGenerator>();
}

} // namespace astra
