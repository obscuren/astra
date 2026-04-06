#include "astra/map_generator.h"
#include "astra/map_properties.h"
#include "astra/civ_aesthetics.h"
#include "astra/world_constants.h"

#include <algorithm>
#include <cmath>
#include <vector>

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
        case Tile::OW_Ruins:       return 0.20f;
        case Tile::OW_CaveEntrance:return 0.25f;
        case Tile::OW_CrashedShip: return 0.15f;
        case Tile::OW_Outpost:     return 0.18f;
        case Tile::OW_Settlement:  return 0.05f;
        case Tile::OW_Beacon:      return 0.15f;
        case Tile::OW_Megastructure: return 0.20f;
        case Tile::OW_AlienTerrain: return 0.12f;
        case Tile::OW_ScorchedEarth: return 0.20f;
        case Tile::OW_GlassedCrater: return 0.60f;
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

// --- Shared edge seeding ---

// Compute a deterministic seed for the shared edge between two zones.
// Both zones sharing this edge produce the same seed for matching terrain.
// edge_dir: 0=North, 1=South, 2=West, 3=East
static unsigned shared_edge_seed(unsigned base_seed, int ow_x, int ow_y,
                                  int zone_x, int zone_y, int edge_dir) {
    int edge_ow_x = ow_x, edge_ow_y = ow_y;
    int edge_zx = zone_x, edge_zy = zone_y;

    // Normalize: for a north edge, use the zone above's south edge coords
    // For a west edge, use the zone left's east edge coords
    // This ensures both sides compute the same seed
    if (edge_dir == 0) { // North
        edge_zy = zone_y - 1;
        if (edge_zy < 0) { edge_ow_y -= 1; edge_zy = 2; }
    } else if (edge_dir == 2) { // West
        edge_zx = zone_x - 1;
        if (edge_zx < 0) { edge_ow_x -= 1; edge_zx = 2; }
    }
    // South and East are already the "canonical" position

    return base_seed
        ^ (static_cast<unsigned>(edge_ow_x + 1000) * 7919u)
        ^ (static_cast<unsigned>(edge_ow_y + 1000) * 6271u)
        ^ (static_cast<unsigned>(edge_zx) * 3571u)
        ^ (static_cast<unsigned>(edge_zy) * 4517u)
        ^ (static_cast<unsigned>(edge_dir & 1) * 8831u);
}

struct EdgeStrip {
    std::vector<float> wall_density;
    std::vector<float> water_density;
};

// Generate deterministic terrain strip for a shared edge
static EdgeStrip generate_edge_strip(unsigned seed, int length,
                                     float base_wall, float base_water) {
    EdgeStrip strip;
    strip.wall_density.resize(length);
    strip.water_density.resize(length);
    for (int i = 0; i < length; ++i) {
        float n = fbm(static_cast<float>(i), 0.0f, seed, 0.1f, 3);
        strip.wall_density[i]  = base_wall  * (0.5f + n);
        strip.water_density[i] = base_water * (0.3f + 0.7f * n);
    }
    return strip;
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

    // Check if ANY neighbor differs from center (for overworld tile boundary blending)
    bool has_different_neighbor =
        (nn != Tile::Empty && nn != terrain) ||
        (ns != Tile::Empty && ns != terrain) ||
        (ne != Tile::Empty && ne != terrain) ||
        (nw != Tile::Empty && nw != terrain);

    // Compute center tile's densities
    float center_wall = terrain_wall_density(terrain);
    float center_water = terrain_water_density(terrain);

    // Shared edge strips for zone connectivity
    unsigned edge_base_seed = seed ^ 0xED6Eu;
    int zx = props_->zone_x;
    int zy = props_->zone_y;
    int owx = props_->overworld_x;
    int owy = props_->overworld_y;

    auto north_strip = generate_edge_strip(
        shared_edge_seed(edge_base_seed, owx, owy, zx, zy, 0), w, center_wall, center_water);
    auto south_strip = generate_edge_strip(
        shared_edge_seed(edge_base_seed, owx, owy, zx, zy, 1), w, center_wall, center_water);
    auto west_strip = generate_edge_strip(
        shared_edge_seed(edge_base_seed, owx, owy, zx, zy, 2), h, center_wall, center_water);
    auto east_strip = generate_edge_strip(
        shared_edge_seed(edge_base_seed, owx, owy, zx, zy, 3), h, center_wall, center_water);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // Base noise value (shared across all terrain decisions)
            float n = fbm(static_cast<float>(x), static_cast<float>(y), seed, 0.08f);

            // Start with center terrain's densities
            float wall_threshold = center_wall;
            float water_threshold = center_water;

            // Always compute edge weights for shared edge blending
            EdgeWeights ew = compute_edge_weights(x, y, w, h, seed);

            auto blend = [](float base, float target, float weight) {
                return base + (target - base) * weight;
            };

            // Shared edge blending (always applies for zone-to-zone continuity)
            if (ew.north > 0.0f) {
                wall_threshold  = blend(wall_threshold,  north_strip.wall_density[x],  ew.north);
                water_threshold = blend(water_threshold, north_strip.water_density[x], ew.north);
            }
            if (ew.south > 0.0f) {
                wall_threshold  = blend(wall_threshold,  south_strip.wall_density[x],  ew.south);
                water_threshold = blend(water_threshold, south_strip.water_density[x], ew.south);
            }
            if (ew.west > 0.0f) {
                wall_threshold  = blend(wall_threshold,  west_strip.wall_density[y],  ew.west);
                water_threshold = blend(water_threshold, west_strip.water_density[y], ew.west);
            }
            if (ew.east > 0.0f) {
                wall_threshold  = blend(wall_threshold,  east_strip.wall_density[y],  ew.east);
                water_threshold = blend(water_threshold, east_strip.water_density[y], ew.east);
            }

            // Overworld tile boundary blending (on top of shared edge, only for edge zones)
            if (has_different_neighbor) {
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

            // Scar influence: more walls, less water in scarred areas
            float scar = props_->lore_scar_intensity;
            if (scar > world::scar_light_threshold) {
                wall_threshold += scar * 0.3f;
                water_threshold *= (1.0f - scar * 0.5f);
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

// --- Biome scatter: decorative features for terrain variety ---

// Small decorative stamp shapes placed randomly on floor tiles.
struct ScatterStamp {
    enum Shape { Single, Cluster3, Ring3x3, Line3, Block2x2 };
    Shape shape;
    FixtureType fixture;
    bool passable;
};

static void place_stamp(TileMap* map, int sx, int sy, ScatterStamp::Shape shape,
                         FixtureData fd, bool allow_indoor = false) {
    auto try_place = [&](int x, int y) {
        if (x < 1 || x >= map->width() - 1 || y < 1 || y >= map->height() - 1) return;
        Tile t = map->get(x, y);
        if (t != Tile::Floor && !(allow_indoor && t == Tile::IndoorFloor)) return;
        if (map->fixture_id(x, y) >= 0) return;
        map->add_fixture(x, y, fd);
    };

    switch (shape) {
        case ScatterStamp::Single:
            try_place(sx, sy);
            break;
        case ScatterStamp::Cluster3:
            try_place(sx, sy);
            try_place(sx + 1, sy);
            try_place(sx, sy + 1);
            break;
        case ScatterStamp::Ring3x3:
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if (dx != 0 || dy != 0)
                        try_place(sx + dx, sy + dy);
            break;
        case ScatterStamp::Line3:
            try_place(sx - 1, sy);
            try_place(sx, sy);
            try_place(sx + 1, sy);
            break;
        case ScatterStamp::Block2x2:
            try_place(sx, sy);
            try_place(sx + 1, sy);
            try_place(sx, sy + 1);
            try_place(sx + 1, sy + 1);
            break;
    }
}

// Semantic obstacle helpers — renderer resolves glyph/color from biome context
static FixtureData natural_obstacle() {
    FixtureData fd;
    fd.type = FixtureType::NaturalObstacle;
    fd.passable = false;
    fd.interactable = false;
    return fd;
}

static FixtureData settlement_prop() {
    FixtureData fd;
    fd.type = FixtureType::SettlementProp;
    fd.passable = false;
    fd.interactable = false;
    return fd;
}

// Each biome defines a palette of scatter features with relative weights.
struct ScatterEntry {
    int weight;
    ScatterStamp::Shape shape;
    FixtureData fixture;
};

static void scatter_biome_features(TileMap* map, std::mt19937& rng, Biome biome) {
    int w = map->width();
    int h = map->height();
    int area = w * h;

    std::vector<ScatterEntry> palette;
    int attempts = 0;

    switch (biome) {
        case Biome::Grassland:
            palette = {
                {2, ScatterStamp::Single,   natural_obstacle()},  // boulder
                {1, ScatterStamp::Block2x2, natural_obstacle()},  // boulder cluster
            };
            attempts = area / 100;
            break;
        case Biome::Forest:
            palette = {
                {2, ScatterStamp::Single,   natural_obstacle()},  // tree stump
                {1, ScatterStamp::Block2x2, natural_obstacle()},  // dense thicket
            };
            attempts = area / 120;
            break;
        case Biome::Jungle:
            palette = {
                {3, ScatterStamp::Single,   natural_obstacle()},  // thick trunk
                {2, ScatterStamp::Block2x2, natural_obstacle()},  // root mass
            };
            attempts = area / 70;
            break;
        case Biome::Sandy:
            palette = {
                {1, ScatterStamp::Single,   natural_obstacle()},  // large rock
            };
            attempts = area / 200;
            break;
        case Biome::Ice:
            palette = {
                {1, ScatterStamp::Block2x2, natural_obstacle()},  // frozen boulder
            };
            attempts = area / 200;
            break;
        case Biome::Fungal:
            palette = {
                {3, ScatterStamp::Single,   natural_obstacle()},  // large mushroom
            };
            attempts = area / 120;
            break;
        case Biome::Rocky:
            palette = {
                {2, ScatterStamp::Cluster3, natural_obstacle()},  // rock pile
                {1, ScatterStamp::Block2x2, natural_obstacle()},  // boulder
            };
            attempts = area / 100;
            break;
        case Biome::Volcanic:
            palette = {
                {1, ScatterStamp::Line3,    natural_obstacle()},  // lava tube
            };
            attempts = area / 180;
            break;
        case Biome::Aquatic:
            // all entries were passable — no obstacles
            attempts = 0;
            break;
        case Biome::Crystal:
            palette = {
                {3, ScatterStamp::Single,   natural_obstacle()},  // crystal formation
            };
            attempts = area / 120;
            break;
        case Biome::Corroded:
            palette = {
                {1, ScatterStamp::Block2x2, natural_obstacle()},  // collapsed structure
            };
            attempts = area / 150;
            break;
        case Biome::AlienCrystalline:
            palette = {
                {3, ScatterStamp::Single,   natural_obstacle()},
                {1, ScatterStamp::Block2x2, natural_obstacle()},
                {1, ScatterStamp::Cluster3, natural_obstacle()},
            };
            attempts = area / 80;
            break;
        case Biome::AlienOrganic:
            palette = {
                {3, ScatterStamp::Single,   natural_obstacle()},
                {2, ScatterStamp::Block2x2, natural_obstacle()},
            };
            attempts = area / 70;
            break;
        case Biome::AlienGeometric:
            palette = {
                {2, ScatterStamp::Single,   natural_obstacle()},
                {1, ScatterStamp::Line3,    natural_obstacle()},
            };
            attempts = area / 100;
            break;
        case Biome::AlienVoid:
            palette = {
                {2, ScatterStamp::Single,   natural_obstacle()},
                {1, ScatterStamp::Cluster3, natural_obstacle()},
            };
            attempts = area / 100;
            break;
        case Biome::AlienLight:
            palette = {
                {3, ScatterStamp::Single,   natural_obstacle()},
                {1, ScatterStamp::Block2x2, natural_obstacle()},
            };
            attempts = area / 90;
            break;
        case Biome::ScarredGlassed:
            palette = {
                {2, ScatterStamp::Single,   natural_obstacle()},
                {2, ScatterStamp::Block2x2, natural_obstacle()},
                {1, ScatterStamp::Cluster3, natural_obstacle()},
            };
            attempts = area / 70;
            break;
        case Biome::ScarredScorched:
            palette = {
                {3, ScatterStamp::Single,   natural_obstacle()},
                {1, ScatterStamp::Block2x2, natural_obstacle()},
            };
            attempts = area / 90;
            break;
        default:
            return;
    }

    if (palette.empty()) return;

    int total_weight = 0;
    for (const auto& e : palette) total_weight += e.weight;

    std::uniform_int_distribution<int> xdist(2, w - 3);
    std::uniform_int_distribution<int> ydist(2, h - 3);
    std::uniform_int_distribution<int> wdist(0, total_weight - 1);

    for (int i = 0; i < attempts; ++i) {
        int x = xdist(rng);
        int y = ydist(rng);

        if (map->get(x, y) != Tile::Floor) continue;
        if (map->fixture_id(x, y) >= 0) continue;

        int roll = wdist(rng);
        int cumulative = 0;
        const ScatterEntry* chosen = &palette[0];
        for (const auto& e : palette) {
            cumulative += e.weight;
            if (roll < cumulative) { chosen = &e; break; }
        }

        place_stamp(map, x, y, chosen->shape, chosen->fixture);
    }
}

// --- Reusable settlement/civilization decoration pass ---
// Scatters sci-fi props on IndoorFloor (paths/plazas) and adjacent Floor tiles.
// Biome-aware: grassy biomes get plants, others get tech/salvage props.
static void scatter_settlement_props(TileMap* map, std::mt19937& rng, Biome biome) {
    int w = map->width();
    int h = map->height();

    // Collect candidate tiles (IndoorFloor paths/plazas and nearby outdoor Floor)
    // We place props on these tiles only.

    // --- Prop palette ---
    // Universal sci-fi props (placed on IndoorFloor — paths/plazas)
    std::vector<ScatterEntry> path_palette = {
        {3, ScatterStamp::Single,   settlement_prop()},  // antenna
        {2, ScatterStamp::Single,   settlement_prop()},  // water well
        {2, ScatterStamp::Single,   settlement_prop()},  // bench
        {1, ScatterStamp::Single,   settlement_prop()},  // lamp post
        {1, ScatterStamp::Single,   settlement_prop()},  // gear/machinery
    };

    // Biome-specific outdoor props (placed on Floor tiles near settlement)
    std::vector<ScatterEntry> outdoor_palette;
    switch (biome) {
        case Biome::Grassland:
        case Biome::Forest:
        case Biome::Jungle:
            outdoor_palette = {
                {1, ScatterStamp::Single,   natural_obstacle()},  // potted plant
            };
            break;
        case Biome::Sandy:
        case Biome::Volcanic:
            outdoor_palette = {
                {2, ScatterStamp::Single,   natural_obstacle()},  // rock
            };
            break;
        case Biome::Ice:
            // all entries were passable — handled by floor scatter
            break;
        case Biome::Corroded:
        case Biome::Crystal:
            outdoor_palette = {
                {2, ScatterStamp::Single,   natural_obstacle()},  // junk pile
            };
            break;
        default:
            // all entries were passable — handled by floor scatter
            break;
    }

    // Scatter path props on IndoorFloor tiles
    {
        int total_w = 0;
        for (const auto& e : path_palette) total_w += e.weight;
        std::uniform_int_distribution<int> xd(2, w - 3);
        std::uniform_int_distribution<int> yd(2, h - 3);
        std::uniform_int_distribution<int> wd(0, total_w - 1);
        int attempts = 30;
        for (int i = 0; i < attempts; ++i) {
            int x = xd(rng), y = yd(rng);
            if (map->get(x, y) != Tile::IndoorFloor) continue;
            if (map->fixture_id(x, y) >= 0) continue;
            int roll = wd(rng);
            int cum = 0;
            const ScatterEntry* chosen = &path_palette[0];
            for (const auto& e : path_palette) {
                cum += e.weight;
                if (roll < cum) { chosen = &e; break; }
            }
            place_stamp(map, x, y, chosen->shape, chosen->fixture, true);
        }
    }

    // Scatter outdoor props on Floor tiles near buildings
    if (!outdoor_palette.empty()) {
        int total_w = 0;
        for (const auto& e : outdoor_palette) total_w += e.weight;
        std::uniform_int_distribution<int> xd(2, w - 3);
        std::uniform_int_distribution<int> yd(2, h - 3);
        std::uniform_int_distribution<int> wd(0, total_w - 1);
        int attempts = 40;
        for (int i = 0; i < attempts; ++i) {
            int x = xd(rng), y = yd(rng);
            if (map->get(x, y) != Tile::Floor) continue;
            if (map->fixture_id(x, y) >= 0) continue;
            int roll = wd(rng);
            int cum = 0;
            const ScatterEntry* chosen = &outdoor_palette[0];
            for (const auto& e : outdoor_palette) {
                cum += e.weight;
                if (roll < cum) { chosen = &e; break; }
            }
            place_stamp(map, x, y, chosen->shape, chosen->fixture);
        }
    }
}

void DetailMapGenerator::place_features(std::mt19937& rng) {
    // Select scatter biome — lore overrides natural biome
    Biome scatter_biome = props_->biome;
    if (props_->lore_scar_intensity > world::scar_medium_threshold) {
        scatter_biome = props_->lore_scar_intensity > world::scar_heavy_threshold
            ? Biome::ScarredGlassed : Biome::ScarredScorched;
    } else if (props_->lore_alien_strength > world::alien_strength_threshold) {
        scatter_biome = alien_biome_for_architecture(props_->lore_alien_architecture);
    }
    scatter_biome_features(map_, rng, scatter_biome);

    if (!props_->detail_has_poi) return;

    int w = map_->width();
    int h = map_->height();
    int cx = w / 2;
    int cy = h / 2;

    switch (props_->detail_poi_type) {
        case Tile::OW_CaveEntrance: {
            std::uniform_real_distribution<float> prob(0.0f, 1.0f);
            std::uniform_real_distribution<float> noise_dist(-1.5f, 1.5f);

            // --- Rocky outcrop (~16x12) using distance-from-center with noise ---
            int rx = 8, ry = 6; // half-extents
            for (int dy = -ry; dy <= ry; ++dy) {
                for (int dx = -rx; dx <= rx; ++dx) {
                    int px = cx + dx, py = cy + dy;
                    if (!in_bounds(px, py)) continue;
                    float dist = std::sqrt(static_cast<float>(dx * dx) / (rx * rx)
                                         + static_cast<float>(dy * dy) / (ry * ry));
                    float noise = noise_dist(rng);
                    float threshold = 0.85f;
                    if (dist + noise * 0.25f < threshold) {
                        // Edge weathering: ~80% wall at edges, 100% deeper inside
                        if (dist > 0.6f && prob(rng) > 0.80f) continue;
                        map_->set(px, py, Tile::Wall);
                    }
                }
            }

            // --- Cave mouth approach from south ---
            // 3-wide corridor from south edge toward center, narrowing to 1-wide
            for (int dy = ry; dy >= 1; --dy) {
                int half_width = (dy > ry / 2) ? 1 : 0; // wide at south, narrow near center
                if (dy > ry - 2) half_width = 1; // ensure 3-wide at south edge
                for (int dx = -half_width; dx <= half_width; ++dx) {
                    int px = cx + dx, py = cy + dy;
                    if (in_bounds(px, py))
                        map_->set(px, py, Tile::Floor);
                }
            }

            // --- Scattered boulders (6-10 small wall clusters) ---
            std::uniform_int_distribution<int> boulder_count(6, 10);
            std::uniform_int_distribution<int> boulder_ox(-12, 12);
            std::uniform_int_distribution<int> boulder_oy(-9, 9);
            int num_boulders = boulder_count(rng);
            for (int i = 0; i < num_boulders; ++i) {
                int bx = cx + boulder_ox(rng);
                int by = cy + boulder_oy(rng);
                // Skip if inside the main outcrop
                float bdist = std::sqrt(static_cast<float>((bx - cx) * (bx - cx)) / (rx * rx)
                                       + static_cast<float>((by - cy) * (by - cy)) / (ry * ry));
                if (bdist < 0.9f) continue;
                // 1x1 or 2x2 boulder
                int bsize = (prob(rng) < 0.5f) ? 1 : 2;
                for (int bdy = 0; bdy < bsize; ++bdy)
                    for (int bdx = 0; bdx < bsize; ++bdx)
                        if (in_bounds(bx + bdx, by + bdy) && prob(rng) < 0.5f)
                            map_->set(bx + bdx, by + bdy, Tile::Wall);
            }

            // --- Portal at center ---
            map_->set(cx, cy, Tile::Portal);

            // --- Debris decorations near entrance ---
            std::uniform_int_distribution<int> debris_count(3, 5);
            std::uniform_int_distribution<int> debris_ox(-3, 3);
            std::uniform_int_distribution<int> debris_oy(1, ry + 2);
            int num_debris = debris_count(rng);
            for (int i = 0; i < num_debris; ++i) {
                int dx = cx + debris_ox(rng);
                int dy = cy + debris_oy(rng);
                if (in_bounds(dx, dy) && map_->get(dx, dy) == Tile::Floor) {
                    map_->set(dx, dy, Tile::Fixture);
                    map_->add_fixture(dx, dy, make_fixture(FixtureType::Debris));
                }
            }
            break;
        }
        case Tile::OW_Settlement: {
            // --- Randomly generated frontier settlement ---
            // Plaza at center, buildings on shuffled cardinal sides,
            // 3-wide paths connecting everything, solid walls, sparse windows.

            // Building shape constants
            // 0=rectangle, 1=L-shape, 2=T-shape, 3=two-room, 4=U-shape
            constexpr int SHAPE_RECT = 0;
            constexpr int SHAPE_L    = 1;
            constexpr int SHAPE_T    = 2;
            constexpr int SHAPE_DUAL = 3;
            constexpr int SHAPE_U    = 4;

            // Helper: draw a shaped building using a bitmap approach.
            // door_side: 0=north, 1=south, 2=west, 3=east
            // material: 0=metal, 1=concrete, 2=wood, 3=salvage
            // shape: building shape index
            auto draw_building = [&](int bx, int by, int bw, int bh, int door_side,
                                     std::pair<int,int>& door_out, uint8_t material,
                                     int shape) {
                // Clamp shape to rectangle if building is too small
                if ((shape == SHAPE_L || shape == SHAPE_U) && (bw < 10 || bh < 6))
                    shape = SHAPE_RECT;
                if (shape == SHAPE_T && (bw < 12 || bh < 7))
                    shape = SHAPE_RECT;
                if (shape == SHAPE_DUAL && (bw < 14 || bh < 7))
                    shape = SHAPE_RECT;

                // Build bitmap: which local cells are part of the building
                std::vector<bool> bmap(bw * bh, false);
                auto bset = [&](int lx, int ly) {
                    if (lx >= 0 && lx < bw && ly >= 0 && ly < bh)
                        bmap[ly * bw + lx] = true;
                };
                auto bget = [&](int lx, int ly) -> bool {
                    if (lx < 0 || lx >= bw || ly < 0 || ly >= bh) return false;
                    return bmap[ly * bw + lx];
                };
                auto fill_rect = [&](int rx, int ry, int rw, int rh) {
                    for (int y = ry; y < ry + rh; ++y)
                        for (int x = rx; x < rx + rw; ++x)
                            bset(x, y);
                };

                switch (shape) {
                    case SHAPE_RECT:
                        fill_rect(0, 0, bw, bh);
                        break;
                    case SHAPE_L:
                        // Top-wide + bottom-left
                        fill_rect(0, 0, bw, bh * 2 / 3 + 1);
                        fill_rect(0, 0, bw / 2 + 1, bh);
                        break;
                    case SHAPE_T:
                        // Top-wide + bottom-center stem
                        fill_rect(0, 0, bw, bh / 2 + 1);
                        fill_rect(bw / 3, 0, bw - 2 * (bw / 3), bh);
                        break;
                    case SHAPE_DUAL:
                        // Left room + right room + connecting corridor
                        { int room_w = bw / 3 + 1;
                          int corr_h = std::max(3, bh / 3);
                          int corr_y = bh / 2 - corr_h / 2;
                          fill_rect(0, 0, room_w, bh);
                          fill_rect(bw - room_w, 0, room_w, bh);
                          fill_rect(room_w - 1, corr_y, bw - 2 * room_w + 2, corr_h);
                        }
                        break;
                    case SHAPE_U:
                        // Left wing + right wing + bottom connector
                        { int wing_w = bw / 3 + 1;
                          fill_rect(0, 0, wing_w, bh);
                          fill_rect(bw - wing_w, 0, wing_w, bh);
                          fill_rect(0, bh * 2 / 3, bw, bh - bh * 2 / 3);
                        }
                        break;
                }

                // Render: outer edge = wall, interior = floor
                for (int ly = 0; ly < bh; ++ly) {
                    for (int lx = 0; lx < bw; ++lx) {
                        if (!bget(lx, ly)) continue;
                        int px = bx + lx, py = by + ly;
                        if (!in_bounds(px, py)) continue;
                        bool outer = !bget(lx - 1, ly) || !bget(lx + 1, ly) ||
                                     !bget(lx, ly - 1) || !bget(lx, ly + 1);
                        map_->set(px, py, outer ? Tile::StructuralWall : Tile::IndoorFloor);
                        if (outer)
                            map_->set_glyph_override(px, py, material);
                    }
                }

                // Door: find an outer wall tile on the requested side, near center
                int dx = bx + bw / 2, dy = by + bh / 2;
                switch (door_side) {
                    case 0: // north: scan down from top at center x
                        for (int ly = 0; ly < bh; ++ly)
                            if (bget(bw / 2, ly)) { dy = by + ly; break; }
                        dx = bx + bw / 2;
                        break;
                    case 1: // south: scan up from bottom at center x
                        for (int ly = bh - 1; ly >= 0; --ly)
                            if (bget(bw / 2, ly)) { dy = by + ly; break; }
                        dx = bx + bw / 2;
                        break;
                    case 2: // west: scan right from left at center y
                        for (int lx = 0; lx < bw; ++lx)
                            if (bget(lx, bh / 2)) { dx = bx + lx; break; }
                        dy = by + bh / 2;
                        break;
                    case 3: // east: scan left from right at center y
                        for (int lx = bw - 1; lx >= 0; --lx)
                            if (bget(lx, bh / 2)) { dx = bx + lx; break; }
                        dy = by + bh / 2;
                        break;
                }
                if (in_bounds(dx, dy)) {
                    map_->set(dx, dy, Tile::Fixture);
                    map_->add_fixture(dx, dy, make_fixture(FixtureType::Door));
                }
                door_out = {dx, dy};

                // Windows: scan outer walls, place every ~4 tiles, skip door/corners
                int win_count = 0;
                for (int ly = 0; ly < bh; ++ly) {
                    for (int lx = 0; lx < bw; ++lx) {
                        if (!bget(lx, ly)) continue;
                        bool outer = !bget(lx - 1, ly) || !bget(lx + 1, ly) ||
                                     !bget(lx, ly - 1) || !bget(lx, ly + 1);
                        if (!outer) continue;
                        int px = bx + lx, py = by + ly;
                        if (px == dx && py == dy) continue; // skip door
                        // Skip corners (two or more adjacent sides missing)
                        int missing = (!bget(lx-1,ly)?1:0) + (!bget(lx+1,ly)?1:0) +
                                      (!bget(lx,ly-1)?1:0) + (!bget(lx,ly+1)?1:0);
                        if (missing >= 2) continue;
                        // Place a window every ~4 wall tiles
                        win_count++;
                        if (win_count % 4 == 2 && in_bounds(px, py)) {
                            map_->set(px, py, Tile::Fixture);
                            map_->add_fixture(px, py, make_fixture(FixtureType::Window));
                        }
                    }
                }
            };

            // Helper: carve a 3-wide paved path (IndoorFloor) as a straight
            // axis-aligned segment from (x1,y1) to (x2,y2).
            auto carve_straight = [&](int x1, int y1, int x2, int y2) {
                if (x1 == x2) { // vertical
                    int lo = std::min(y1, y2), hi = std::max(y1, y2);
                    for (int y = lo; y <= hi; ++y)
                        for (int d = -1; d <= 1; ++d)
                            if (in_bounds(x1 + d, y))
                                map_->set(x1 + d, y, Tile::IndoorFloor);
                } else { // horizontal
                    int lo = std::min(x1, x2), hi = std::max(x1, x2);
                    for (int x = lo; x <= hi; ++x)
                        for (int d = -1; d <= 1; ++d)
                            if (in_bounds(x, y1 + d))
                                map_->set(x, y1 + d, Tile::IndoorFloor);
                }
            };

            // --- Randomize layout ---
            std::uniform_int_distribution<int> plaza_w_dist(10, 14);
            std::uniform_int_distribution<int> plaza_h_dist(6, 10);
            int plaza_w = plaza_w_dist(rng);
            int plaza_h = plaza_h_dist(rng);
            int plaza_x = cx - plaza_w / 2;
            int plaza_y = cy - plaza_h / 2;

            // Shuffle cardinal directions: 0=north, 1=south, 2=west, 3=east
            int dirs[4] = {0, 1, 2, 3};
            for (int i = 3; i > 0; --i) {
                std::uniform_int_distribution<int> swap_dist(0, i);
                std::swap(dirs[i], dirs[swap_dist(rng)]);
            }
            // dirs[0] = main hall, dirs[1] = market, dirs[2..3] = dwellings

            // Buildings sit 8 tiles from plaza edge (room for 3-wide path + breathing room)
            int gap = 8;

            // Compute building origin + door side from cardinal direction.
            // offset shifts the building along the wall for side-by-side placement.
            auto place_on_side = [&](int side, int bw, int bh, int offset = 0)
                -> std::tuple<int, int, int> {
                switch (side) {
                    case 0: return {cx - bw / 2 + offset, plaza_y - gap - bh, 1};
                    case 1: return {cx - bw / 2 + offset, plaza_y + plaza_h + gap, 0};
                    case 2: return {plaza_x - gap - bw, cy - bh / 2 + offset, 3};
                    case 3: return {plaza_x + plaza_w + gap, cy - bh / 2 + offset, 2};
                    default: return {cx, cy, 0};
                }
            };

            // Plaza-edge midpoint on a given side (path endpoint from plaza)
            auto plaza_edge_mid = [&](int side) -> std::pair<int,int> {
                switch (side) {
                    case 0: return {cx, plaza_y};          // north edge
                    case 1: return {cx, plaza_y + plaza_h - 1}; // south edge
                    case 2: return {plaza_x, cy};          // west edge
                    case 3: return {plaza_x + plaza_w - 1, cy}; // east edge
                    default: return {cx, cy};
                }
            };

            // ============================================================
            // PHASE 1: Compute all building positions (consume RNG state)
            // ============================================================

            // Building materials: 0=metal, 1=concrete, 2=wood, 3=salvage
            std::uniform_int_distribution<int> mat_dist(0, 3);
            // Building shapes: randomly assigned (clamped to rect if too small)
            std::uniform_int_distribution<int> shape_dist(SHAPE_RECT, SHAPE_U);

            // Main hall (metal or concrete — sturdy, any shape)
            std::uniform_int_distribution<int> hall_w_dist(14, 18);
            std::uniform_int_distribution<int> hall_h_dist(7, 9);
            int hall_w = hall_w_dist(rng), hall_h = hall_h_dist(rng);
            auto [hall_x, hall_y, hall_door] = place_on_side(dirs[0], hall_w, hall_h);
            uint8_t hall_mat = static_cast<uint8_t>(mat_dist(rng) % 2);
            int hall_shape = shape_dist(rng);

            // Market (any material, any shape)
            std::uniform_int_distribution<int> mkt_w_dist(10, 14);
            std::uniform_int_distribution<int> mkt_h_dist(6, 8);
            int mkt_w = mkt_w_dist(rng), mkt_h = mkt_h_dist(rng);
            auto [mkt_x, mkt_y, mkt_door] = place_on_side(dirs[1], mkt_w, mkt_h);
            uint8_t mkt_mat = static_cast<uint8_t>(mat_dist(rng));
            int mkt_shape = shape_dist(rng);

            // Market stalls count
            std::uniform_int_distribution<int> stall_count_dist(2, 3);
            int num_stalls = stall_count_dist(rng);

            // Dwellings
            struct DwellInfo { int x, y, w, h, door_side, side; uint8_t mat; int shape; };
            std::vector<DwellInfo> dwellings;
            std::uniform_int_distribution<int> dwell_count_dist(2, 4);
            int num_dwellings = dwell_count_dist(rng);
            std::uniform_int_distribution<int> dw_dist(7, 9);
            std::uniform_int_distribution<int> dh_dist(5, 7);
            for (int i = 0; i < num_dwellings; ++i) {
                int side = dirs[2 + (i % 2)];
                int dw = dw_dist(rng), dh = dh_dist(rng);
                // Spread dwellings apart: offset along the wall with generous spacing
                int perp_offset = (i / 2) * (dw + 4) - ((i > 1) ? (dw + 4) / 2 : 0);
                auto [bx, by, ds] = place_on_side(side, dw, dh, perp_offset);
                uint8_t dmat = static_cast<uint8_t>(mat_dist(rng));
                int dshape = shape_dist(rng);
                dwellings.push_back({bx, by, dw, dh, ds, side, dmat, dshape});
            }

            // Stools
            std::uniform_int_distribution<int> stool_count_dist(3, 5);
            int num_stools = stool_count_dist(rng);
            struct StoolPos { int x, y; };
            std::vector<StoolPos> stools;
            for (int i = 0; i < num_stools; ++i) {
                std::uniform_int_distribution<int> sx_dist(plaza_x + 2, plaza_x + plaza_w - 3);
                std::uniform_int_distribution<int> sy_dist(plaza_y + 2, plaza_y + plaza_h - 3);
                stools.push_back({sx_dist(rng), sy_dist(rng)});
            }

            // ============================================================
            // PHASE 2: Draw plaza, roads, and paths (floor layer)
            // ============================================================

            // Plaza (paved)
            for (int y = plaza_y; y < plaza_y + plaza_h; ++y)
                for (int x = plaza_x; x < plaza_x + plaza_w; ++x)
                    if (in_bounds(x, y))
                        map_->set(x, y, Tile::IndoorFloor);

            // Entry roads (3-wide paved, 14 tiles outward from each plaza edge)
            int road_len = 14;
            for (int y = plaza_y - 1; y >= std::max(1, plaza_y - road_len); --y)
                for (int d = -1; d <= 1; ++d)
                    if (in_bounds(cx + d, y))
                        map_->set(cx + d, y, Tile::IndoorFloor);
            for (int y = plaza_y + plaza_h; y < std::min(map_->height() - 1, plaza_y + plaza_h + road_len); ++y)
                for (int d = -1; d <= 1; ++d)
                    if (in_bounds(cx + d, y))
                        map_->set(cx + d, y, Tile::IndoorFloor);
            for (int x = plaza_x - 1; x >= std::max(1, plaza_x - road_len); --x)
                for (int d = -1; d <= 1; ++d)
                    if (in_bounds(x, cy + d))
                        map_->set(x, cy + d, Tile::IndoorFloor);
            for (int x = plaza_x + plaza_w; x < std::min(map_->width() - 1, plaza_x + plaza_w + road_len); ++x)
                for (int d = -1; d <= 1; ++d)
                    if (in_bounds(x, cy + d))
                        map_->set(x, cy + d, Tile::IndoorFloor);

            // Paths from plaza edge to every building door.
            // Main hall door
            {
                auto [ex, ey] = plaza_edge_mid(dirs[0]);
                // Compute hall door pos
                int hdx = 0, hdy = 0;
                switch (hall_door) {
                    case 0: hdx = hall_x + hall_w / 2; hdy = hall_y;            break;
                    case 1: hdx = hall_x + hall_w / 2; hdy = hall_y + hall_h - 1; break;
                    case 2: hdx = hall_x;             hdy = hall_y + hall_h / 2; break;
                    case 3: hdx = hall_x + hall_w - 1; hdy = hall_y + hall_h / 2; break;
                }
                bool vert = (dirs[0] <= 1);
                if (vert) {
                    carve_straight(ex, ey, ex, hdy);
                    carve_straight(ex, hdy, hdx, hdy);
                } else {
                    carve_straight(ex, ey, hdx, ey);
                    carve_straight(hdx, ey, hdx, hdy);
                }
            }
            // Market door
            {
                auto [ex, ey] = plaza_edge_mid(dirs[1]);
                int mdx = 0, mdy = 0;
                switch (mkt_door) {
                    case 0: mdx = mkt_x + mkt_w / 2; mdy = mkt_y;            break;
                    case 1: mdx = mkt_x + mkt_w / 2; mdy = mkt_y + mkt_h - 1; break;
                    case 2: mdx = mkt_x;             mdy = mkt_y + mkt_h / 2; break;
                    case 3: mdx = mkt_x + mkt_w - 1; mdy = mkt_y + mkt_h / 2; break;
                }
                bool vert = (dirs[1] <= 1);
                if (vert) {
                    carve_straight(ex, ey, ex, mdy);
                    carve_straight(ex, mdy, mdx, mdy);
                } else {
                    carve_straight(ex, ey, mdx, ey);
                    carve_straight(mdx, ey, mdx, mdy);
                }
            }
            // Dwelling doors
            for (auto& dw : dwellings) {
                // Compute door position (same logic as draw_building)
                int door_x = 0, door_y = 0;
                switch (dw.door_side) {
                    case 0: door_x = dw.x + dw.w / 2; door_y = dw.y;          break;
                    case 1: door_x = dw.x + dw.w / 2; door_y = dw.y + dw.h - 1; break;
                    case 2: door_x = dw.x;            door_y = dw.y + dw.h / 2; break;
                    case 3: door_x = dw.x + dw.w - 1; door_y = dw.y + dw.h / 2; break;
                }
                auto [ex, ey] = plaza_edge_mid(dw.side);
                bool vert = (dw.side <= 1);
                if (vert) {
                    carve_straight(ex, ey, ex, door_y);
                    carve_straight(ex, door_y, door_x, door_y);
                } else {
                    carve_straight(ex, ey, door_x, ey);
                    carve_straight(door_x, ey, door_x, door_y);
                }
            }

            // ============================================================
            // PHASE 3: Draw all buildings (solid walls stamp over paths)
            // ============================================================

            // Main hall
            std::pair<int,int> hall_door_pos;
            draw_building(hall_x, hall_y, hall_w, hall_h, hall_door, hall_door_pos,
                          hall_mat, hall_shape);

            // Market
            std::pair<int,int> mkt_door_pos;
            draw_building(mkt_x, mkt_y, mkt_w, mkt_h, mkt_door, mkt_door_pos,
                          mkt_mat, mkt_shape);

            // Dwellings
            for (auto& dw : dwellings) {
                std::pair<int,int> ignore;
                draw_building(dw.x, dw.y, dw.w, dw.h, dw.door_side, ignore,
                              dw.mat, dw.shape);
            }

            // ============================================================
            // PHASE 4: Place fixtures (after buildings are solid)
            // ============================================================

            // Plaza stools
            for (auto& s : stools) {
                if (in_bounds(s.x, s.y) && map_->get(s.x, s.y) == Tile::Floor) {
                    map_->set(s.x, s.y, Tile::Fixture);
                    map_->add_fixture(s.x, s.y, make_fixture(FixtureType::Stool));
                }
            }

            // Main hall fixtures
            if (in_bounds(hall_x + 2, hall_y + 2)) {
                map_->set(hall_x + 2, hall_y + 2, Tile::Fixture);
                map_->add_fixture(hall_x + 2, hall_y + 2, make_fixture(FixtureType::Console));
            }
            if (in_bounds(hall_x + hall_w - 3, hall_y + hall_h - 2)) {
                map_->set(hall_x + hall_w - 3, hall_y + hall_h - 2, Tile::Fixture);
                map_->add_fixture(hall_x + hall_w - 3, hall_y + hall_h - 2, make_fixture(FixtureType::Rack));
            }
            if (in_bounds(hall_x + hall_w - 3, hall_y + 2))
                map_->set(hall_x + hall_w - 3, hall_y + 2, Tile::Portal);

            // Market fixtures
            if (in_bounds(mkt_x + 2, mkt_y + 1)) {
                map_->set(mkt_x + 2, mkt_y + 1, Tile::Fixture);
                map_->add_fixture(mkt_x + 2, mkt_y + 1, make_fixture(FixtureType::Crate));
            }
            if (in_bounds(mkt_x + 4, mkt_y + 1)) {
                map_->set(mkt_x + 4, mkt_y + 1, Tile::Fixture);
                map_->add_fixture(mkt_x + 4, mkt_y + 1, make_fixture(FixtureType::Crate));
            }
            if (in_bounds(mkt_x + 3, mkt_y + mkt_h - 2)) {
                map_->set(mkt_x + 3, mkt_y + mkt_h - 2, Tile::Fixture);
                map_->add_fixture(mkt_x + 3, mkt_y + mkt_h - 2, make_fixture(FixtureType::Table));
            }

            // Market stalls (open-air, along path to market)
            {
                int mkt_side = dirs[1];
                for (int i = 0; i < num_stalls; ++i) {
                    int sx = 0, sy = 0, sx2 = 0, sy2 = 0;
                    int off = (i - 1) * 3;
                    switch (mkt_side) {
                        case 0: sx = cx + off + 2; sy = plaza_y - 1;
                                sx2 = sx + 1; sy2 = sy; break;
                        case 1: sx = cx + off + 2; sy = plaza_y + plaza_h;
                                sx2 = sx + 1; sy2 = sy; break;
                        case 2: sx = plaza_x - 1; sy = cy + off + 2;
                                sx2 = sx; sy2 = sy + 1; break;
                        case 3: sx = plaza_x + plaza_w; sy = cy + off + 2;
                                sx2 = sx; sy2 = sy + 1; break;
                    }
                    if (in_bounds(sx, sy) && map_->get(sx, sy) == Tile::Floor) {
                        map_->set(sx, sy, Tile::Fixture);
                        map_->add_fixture(sx, sy, make_fixture(FixtureType::Table));
                    }
                    if (in_bounds(sx2, sy2) && map_->get(sx2, sy2) == Tile::Floor) {
                        map_->set(sx2, sy2, Tile::Fixture);
                        map_->add_fixture(sx2, sy2, make_fixture(FixtureType::Crate));
                    }
                }
            }

            // Dwelling fixtures
            for (auto& dw : dwellings) {
                if (in_bounds(dw.x + 2, dw.y + 1)) {
                    map_->set(dw.x + 2, dw.y + 1, Tile::Fixture);
                    map_->add_fixture(dw.x + 2, dw.y + 1, make_fixture(FixtureType::Bunk));
                }
            }

            // Torches — plaza corners and near building entrances
            auto place_torch = [&](int tx, int ty) {
                if (!in_bounds(tx, ty)) return;
                Tile t = map_->get(tx, ty);
                if (t == Tile::Floor || t == Tile::IndoorFloor) {
                    map_->set(tx, ty, Tile::Fixture);
                    map_->add_fixture(tx, ty, make_fixture(FixtureType::Torch));
                }
            };
            // Plaza corners
            place_torch(plaza_x - 1, plaza_y - 1);
            place_torch(plaza_x + plaza_w, plaza_y - 1);
            place_torch(plaza_x - 1, plaza_y + plaza_h);
            place_torch(plaza_x + plaza_w, plaza_y + plaza_h);
            // Place torches in front of building entrances (outside the door)
            // For each door, find which direction faces outside (Floor, not
            // StructuralWall or IndoorFloor inside the building), then place
            // a torch 2 tiles out on each side of the entrance.
            for (int sy = 0; sy < h; ++sy) {
                for (int sx = 0; sx < w; ++sx) {
                    if (map_->get(sx, sy) != Tile::Fixture) continue;
                    int fid = map_->fixture_id(sx, sy);
                    if (fid < 0) continue;
                    if (map_->fixture(fid).type != FixtureType::Door) continue;
                    // Check each cardinal direction for the outside
                    static const int ddx[] = {0, 0, -1, 1};
                    static const int ddy[] = {-1, 1, 0, 0};
                    for (int d = 0; d < 4; ++d) {
                        int nx = sx + ddx[d], ny = sy + ddy[d];
                        if (!in_bounds(nx, ny)) continue;
                        Tile nt = map_->get(nx, ny);
                        // Outside = not a wall (path or open ground)
                        if (nt == Tile::StructuralWall || nt == Tile::Wall) continue;
                        // Place torches flanking the entrance, perpendicular to exit direction
                        int px = ddx[d] == 0 ? 1 : 0;  // perpendicular x
                        int py = ddy[d] == 0 ? 1 : 0;  // perpendicular y
                        // 2 tiles out from door, offset left and right
                        int fx = sx + ddx[d] * 2;
                        int fy = sy + ddy[d] * 2;
                        place_torch(fx + px, fy + py);
                        place_torch(fx - px, fy - py);
                        break; // only one outside direction per door
                    }
                }
            }

            // ============================================================
            // PHASE 5: Scatter settlement props (reusable decoration pass)
            // ============================================================
            scatter_settlement_props(map_, rng, props_->biome);

            break;
        }
        case Tile::OW_Ruins: {
            // --- Large ruins complex ---
            // Central complex: 3-5 rooms of varying sizes within a ~20x16 area
            std::uniform_int_distribution<int> room_count_dist(3, 5);
            std::uniform_real_distribution<float> prob(0.0f, 1.0f);
            int num_rooms = room_count_dist(rng);

            struct RuinRoom { int x, y, w, h; };
            std::vector<RuinRoom> rooms;

            // Place rooms within the central complex area (~20x16 centered)
            int complex_x = cx - 10;
            int complex_y = cy - 8;
            int complex_w = 20;
            int complex_h = 16;

            // First room always at center
            rooms.push_back({cx - 3, cy - 2, 7, 5});

            std::uniform_int_distribution<int> rw_dist(4, 8);
            std::uniform_int_distribution<int> rh_dist(4, 6);

            for (int i = 1; i < num_rooms; ++i) {
                int rw = rw_dist(rng);
                int rh = rh_dist(rng);
                std::uniform_int_distribution<int> rx_dist(complex_x, complex_x + complex_w - rw);
                std::uniform_int_distribution<int> ry_dist(complex_y, complex_y + complex_h - rh);
                int rx = rx_dist(rng);
                int ry = ry_dist(rng);
                rooms.push_back({rx, ry, rw, rh});
            }

            // Draw room walls with ~70% probability (crumbling effect)
            for (const auto& room : rooms) {
                for (int y = room.y; y < room.y + room.h; ++y) {
                    for (int x = room.x; x < room.x + room.w; ++x) {
                        if (!in_bounds(x, y)) continue;
                        bool edge = (y == room.y || y == room.y + room.h - 1 ||
                                     x == room.x || x == room.x + room.w - 1);
                        if (edge) {
                            if (prob(rng) < 0.70f)
                                map_->set(x, y, Tile::Wall);
                        } else {
                            map_->set(x, y, Tile::Floor);
                        }
                    }
                }
                // Doorway: carve opening on a random wall
                std::uniform_int_distribution<int> side_dist(0, 3);
                int side = side_dist(rng);
                switch (side) {
                    case 0: // north wall
                        if (in_bounds(room.x + room.w / 2, room.y))
                            map_->set(room.x + room.w / 2, room.y, Tile::Floor);
                        break;
                    case 1: // south wall
                        if (in_bounds(room.x + room.w / 2, room.y + room.h - 1))
                            map_->set(room.x + room.w / 2, room.y + room.h - 1, Tile::Floor);
                        break;
                    case 2: // west wall
                        if (in_bounds(room.x, room.y + room.h / 2))
                            map_->set(room.x, room.y + room.h / 2, Tile::Floor);
                        break;
                    case 3: // east wall
                        if (in_bounds(room.x + room.w - 1, room.y + room.h / 2))
                            map_->set(room.x + room.w - 1, room.y + room.h / 2, Tile::Floor);
                        break;
                }
            }

            // Outer perimeter wall (~24x20) with ~40% probability (collapsed sections)
            int perim_x = cx - 12;
            int perim_y = cy - 10;
            int perim_w = 24;
            int perim_h = 20;
            for (int x = perim_x; x < perim_x + perim_w; ++x) {
                if (in_bounds(x, perim_y) && prob(rng) < 0.40f)
                    map_->set(x, perim_y, Tile::Wall);
                if (in_bounds(x, perim_y + perim_h - 1) && prob(rng) < 0.40f)
                    map_->set(x, perim_y + perim_h - 1, Tile::Wall);
            }
            for (int y = perim_y; y < perim_y + perim_h; ++y) {
                if (in_bounds(perim_x, y) && prob(rng) < 0.40f)
                    map_->set(perim_x, y, Tile::Wall);
                if (in_bounds(perim_x + perim_w - 1, y) && prob(rng) < 0.40f)
                    map_->set(perim_x + perim_w - 1, y, Tile::Wall);
            }

            // Extending corridors: 2-3 corridors radiating outward
            std::uniform_int_distribution<int> corr_count_dist(2, 3);
            std::uniform_int_distribution<int> corr_len_dist(8, 15);
            std::uniform_int_distribution<int> corr_dir_dist(0, 3);
            int num_corridors = corr_count_dist(rng);

            for (int i = 0; i < num_corridors; ++i) {
                int dir = corr_dir_dist(rng);
                int len = corr_len_dist(rng);
                int sx = cx, sy = cy;
                int ddx = 0, ddy = 0;
                switch (dir) {
                    case 0: ddy = -1; sy = perim_y; break; // north
                    case 1: ddy =  1; sy = perim_y + perim_h - 1; break; // south
                    case 2: ddx = -1; sx = perim_x; break; // west
                    case 3: ddx =  1; sx = perim_x + perim_w - 1; break; // east
                }
                for (int step = 0; step < len; ++step) {
                    int px = sx + ddx * step;
                    int py = sy + ddy * step;
                    for (int perp = -1; perp <= 1; ++perp) {
                        int wx = px + (ddx == 0 ? perp : 0);
                        int wy = py + (ddy == 0 ? perp : 0);
                        if (!in_bounds(wx, wy)) continue;
                        bool wall_edge = (perp == -1 || perp == 1);
                        if (wall_edge && prob(rng) < 0.50f)
                            map_->set(wx, wy, Tile::Wall);
                        else if (perp == 0)
                            map_->set(wx, wy, Tile::Floor);
                    }
                }
            }

            // Scattered wall fragments: 4-8 small clusters near center
            std::uniform_int_distribution<int> frag_count_dist(4, 8);
            std::uniform_int_distribution<int> frag_size_dist(2, 3);
            std::uniform_int_distribution<int> frag_offset_dist(-15, 15);
            int num_frags = frag_count_dist(rng);
            for (int i = 0; i < num_frags; ++i) {
                int fx = cx + frag_offset_dist(rng);
                int fy = cy + frag_offset_dist(rng);
                int fs = frag_size_dist(rng);
                for (int y = fy; y < fy + fs; ++y)
                    for (int x = fx; x < fx + fs; ++x)
                        if (in_bounds(x, y) && prob(rng) < 0.50f)
                            map_->set(x, y, Tile::Wall);
            }

            // Fixtures: debris in rooms, console in first room, crate in second
            {
                // Console in central room
                int console_x = rooms[0].x + rooms[0].w / 2;
                int console_y = rooms[0].y + rooms[0].h / 2;
                if (in_bounds(console_x, console_y)) {
                    map_->set(console_x, console_y, Tile::Fixture);
                    map_->add_fixture(console_x, console_y, make_fixture(FixtureType::Console));
                }
                // Crate in another room if available
                if (rooms.size() > 1) {
                    int crate_x = rooms[1].x + rooms[1].w / 2;
                    int crate_y = rooms[1].y + rooms[1].h / 2;
                    if (in_bounds(crate_x, crate_y)) {
                        map_->set(crate_x, crate_y, Tile::Fixture);
                        map_->add_fixture(crate_x, crate_y, make_fixture(FixtureType::Crate));
                    }
                }
                // Debris in remaining rooms handled by floor scatter
            }

            // Portal in central room for dungeon access
            {
                int portal_x = rooms[0].x + rooms[0].w / 2 + 1;
                int portal_y = rooms[0].y + rooms[0].h / 2;
                if (in_bounds(portal_x, portal_y)) {
                    map_->set(portal_x, portal_y, Tile::Portal);
                }
            }
            break;
        }
        case Tile::OW_CrashedShip: {
            std::uniform_real_distribution<float> prob(0.0f, 1.0f);

            // Fuselage dimensions: ~20x6, nose tapers, stern is wider
            // Ship oriented east-west, nose at east (+x), stern at west (-x)
            int ship_len = 20;
            int ship_half = ship_len / 2; // 10
            int body_half_h = 3; // half-height of widest section

            // Slight diagonal offset for crash angle
            std::uniform_int_distribution<int> skid_dist(-1, 1);
            int skid_y = skid_dist(rng);

            // --- Impact gouge (skid mark behind stern) ---
            std::uniform_int_distribution<int> gouge_len_dist(8, 12);
            int gouge_len = gouge_len_dist(rng);
            for (int gx = 1; gx <= gouge_len; ++gx) {
                int gxp = cx - ship_half - gx;
                int gyp = cy + skid_y;
                for (int perp = -1; perp <= 1; ++perp) {
                    if (!in_bounds(gxp, gyp + perp)) continue;
                    if (perp == 0) {
                        map_->set(gxp, gyp + perp, Tile::IndoorFloor); // scorched skid
                    } else if (prob(rng) < 0.40f) {
                        map_->set(gxp, gyp + perp, Tile::Wall); // churned rubble
                    }
                }
            }

            // --- Main fuselage hull and interior ---
            // For each x along the ship, compute half-height
            auto hull_half_h = [&](int dx) -> int {
                // dx is relative to cx, ranges from -ship_half to +ship_half
                if (dx > ship_half - 4) {
                    // Nose taper: narrow down over last 4 tiles
                    int taper = dx - (ship_half - 4); // 1..4
                    return std::max(1, body_half_h - taper);
                }
                if (dx < -ship_half + 2) {
                    // Slight stern taper
                    return body_half_h - 1;
                }
                return body_half_h;
            };

            // Draw hull walls and carve interior
            for (int dx = -ship_half; dx <= ship_half; ++dx) {
                int hh = hull_half_h(dx);
                int y_off = (dx * skid_y) / ship_half; // slight diagonal
                for (int dy = -hh; dy <= hh; ++dy) {
                    int px = cx + dx;
                    int py = cy + dy + y_off;
                    if (!in_bounds(px, py)) continue;
                    bool edge = (dy == -hh || dy == hh ||
                                 dx == -ship_half || dx == ship_half);
                    if (edge) {
                        // Hull wall with ~75% probability (breached plating)
                        if (prob(rng) < 0.75f) {
                            map_->set(px, py, Tile::StructuralWall);
                            map_->set_glyph_override(px, py, 0); // metal
                        } else {
                            map_->set(px, py, Tile::IndoorFloor); // breach
                        }
                    } else {
                        map_->set(px, py, Tile::IndoorFloor);
                    }
                }
            }

            // --- Interior bulkheads (partial structural walls separating rooms) ---
            // Bulkhead 1: between engine bay and mid-section (at dx ~ -4)
            int bulk1_x = cx - 4;
            for (int dy = -(body_half_h - 1); dy <= (body_half_h - 1); ++dy) {
                if (!in_bounds(bulk1_x, cy + dy)) continue;
                if (std::abs(dy) != 0 && prob(rng) < 0.60f) { // gap at center
                    map_->set(bulk1_x, cy + dy, Tile::StructuralWall);
                    map_->set_glyph_override(bulk1_x, cy + dy, 0); // metal
                }
            }
            // Bulkhead 2: between mid-section and cockpit (at dx ~ +4)
            int bulk2_x = cx + 4;
            for (int dy = -(body_half_h - 1); dy <= (body_half_h - 1); ++dy) {
                if (!in_bounds(bulk2_x, cy + dy)) continue;
                if (std::abs(dy) != 0 && prob(rng) < 0.60f) {
                    map_->set(bulk2_x, cy + dy, Tile::StructuralWall);
                    map_->set_glyph_override(bulk2_x, cy + dy, 0);
                }
            }

            // --- Breach points: 2-3 holes punched in hull ---
            std::uniform_int_distribution<int> breach_count_dist(2, 3);
            std::uniform_int_distribution<int> breach_x_dist(-ship_half + 2, ship_half - 3);
            int num_breaches = breach_count_dist(rng);
            for (int b = 0; b < num_breaches; ++b) {
                int bx = cx + breach_x_dist(rng);
                int hh = hull_half_h(bx - cx);
                // Pick top or bottom hull edge
                int by = cy + (prob(rng) < 0.5f ? -hh : hh);
                int y_off = ((bx - cx) * skid_y) / ship_half;
                by += y_off;
                // Clear 2-3 tiles along hull
                std::uniform_int_distribution<int> breach_len_dist(2, 3);
                int blen = breach_len_dist(rng);
                for (int i = 0; i < blen; ++i) {
                    if (in_bounds(bx + i, by))
                        map_->set(bx + i, by, Tile::IndoorFloor);
                }
            }

            // --- Debris field (~30x20 around the ship) ---
            // Small wall fragments
            std::uniform_int_distribution<int> frag_count_dist(8, 15);
            std::uniform_int_distribution<int> frag_x_dist(-15, 15);
            std::uniform_int_distribution<int> frag_y_dist(-10, 10);
            std::uniform_int_distribution<int> frag_size_dist(1, 2);
            int num_frags = frag_count_dist(rng);
            for (int i = 0; i < num_frags; ++i) {
                int fx = cx + frag_x_dist(rng);
                int fy = cy + frag_y_dist(rng);
                int fs = frag_size_dist(rng);
                for (int y = fy; y < fy + fs; ++y)
                    for (int x = fx; x < fx + fs; ++x)
                        if (in_bounds(x, y) && prob(rng) < 0.60f)
                            map_->set(x, y, Tile::Wall);
            }

            // Debris near ship handled by floor scatter

            // --- Fixtures inside the ship ---
            // Console in cockpit (front section, dx ~ +6)
            {
                int fx = cx + 6, fy = cy;
                if (in_bounds(fx, fy) && map_->get(fx, fy) == Tile::IndoorFloor) {
                    map_->set(fx, fy, Tile::Fixture);
                    map_->add_fixture(fx, fy, make_fixture(FixtureType::Console));
                }
            }

            // Crates in mid-section (1-2)
            {
                std::uniform_int_distribution<int> crate_count_dist(1, 2);
                std::uniform_int_distribution<int> crate_x_dist(-3, 3);
                std::uniform_int_distribution<int> crate_y_dist(-1, 1);
                int num_crates = crate_count_dist(rng);
                for (int i = 0; i < num_crates; ++i) {
                    int fx = cx + crate_x_dist(rng);
                    int fy = cy + crate_y_dist(rng);
                    if (in_bounds(fx, fy) && map_->get(fx, fy) == Tile::IndoorFloor &&
                        map_->fixture_id(fx, fy) < 0) {
                        map_->set(fx, fy, Tile::Fixture);
                        map_->add_fixture(fx, fy, make_fixture(FixtureType::Crate));
                    }
                }
            }

            // Conduit in engine bay (rear section, dx ~ -7)
            {
                int fx = cx - 7, fy = cy;
                if (in_bounds(fx, fy) && map_->get(fx, fy) == Tile::IndoorFloor) {
                    map_->set(fx, fy, Tile::Fixture);
                    map_->add_fixture(fx, fy, make_fixture(FixtureType::Conduit));
                }
            }

            // Portal in mid-section for dungeon access
            {
                int px = cx + 1, py = cy;
                if (in_bounds(px, py)) {
                    map_->set(px, py, Tile::Portal);
                }
            }
            break;
        }
        case Tile::OW_Outpost: {
            // --- Military/frontier outpost compound ---
            // Makeshift perimeter wall (weathered), main building with door,
            // storage shed, corner guard towers, optional outer watchtowers,
            // paved courtyard paths, and proper IndoorFloor inside buildings.
            std::uniform_real_distribution<float> prob(0.0f, 1.0f);

            // Wider defensive perimeter (~30x22)
            int perim_x = cx - 15;
            int perim_y = cy - 11;
            int perim_w = 30;
            int perim_h = 22;

            // Building coordinates (declared early for path layout)
            int mb_x = cx - 6, mb_y = cy - 4;
            int mb_w = 12, mb_h = 9;
            int shed_x = cx + 7, shed_y = cy - 5;
            int shed_w = 7, shed_h = 5;

            // Optional outer watchtowers (1-2, outside perimeter)
            std::uniform_int_distribution<int> tower_count_dist(1, 2);
            int num_outer_towers = tower_count_dist(rng);
            struct OuterTower { int x, y; };
            OuterTower outer_towers[2];
            // Place outside the perimeter at random positions
            std::uniform_int_distribution<int> tower_side_dist(0, 3);
            for (int i = 0; i < num_outer_towers; ++i) {
                int side = tower_side_dist(rng);
                switch (side) {
                    case 0: // north
                        outer_towers[i] = {cx - 4 + i * 8, perim_y - 6};
                        break;
                    case 1: // south
                        outer_towers[i] = {cx - 4 + i * 8, perim_y + perim_h + 2};
                        break;
                    case 2: // west
                        outer_towers[i] = {perim_x - 6, cy - 3 + i * 6};
                        break;
                    case 3: // east
                        outer_towers[i] = {perim_x + perim_w + 2, cy - 3 + i * 6};
                        break;
                }
            }

            // ============================================================
            // PHASE 1: Courtyard floor and paths (drawn first)
            // ============================================================

            // Paved courtyard inside perimeter
            for (int y = perim_y + 1; y < perim_y + perim_h - 1; ++y)
                for (int x = perim_x + 1; x < perim_x + perim_w - 1; ++x)
                    if (in_bounds(x, y))
                        map_->set(x, y, Tile::IndoorFloor);

            // 3-wide paths from gates extending outward
            int road_len = 10;
            // North road
            for (int y = perim_y - 1; y >= std::max(1, perim_y - road_len); --y)
                for (int d = -1; d <= 1; ++d)
                    if (in_bounds(cx + d, y))
                        map_->set(cx + d, y, Tile::IndoorFloor);
            // South road
            for (int y = perim_y + perim_h; y < std::min(map_->height() - 1, perim_y + perim_h + road_len); ++y)
                for (int d = -1; d <= 1; ++d)
                    if (in_bounds(cx + d, y))
                        map_->set(cx + d, y, Tile::IndoorFloor);

            // Paths to outer watchtowers
            for (int i = 0; i < num_outer_towers; ++i) {
                auto& ot = outer_towers[i];
                // L-shaped path from nearest gate
                int gx = cx, gy = (ot.y < cy) ? perim_y : perim_y + perim_h - 1;
                // Vertical segment
                int lo_y = std::min(gy, ot.y + 2), hi_y = std::max(gy, ot.y + 2);
                for (int y = lo_y; y <= hi_y; ++y)
                    for (int d = -1; d <= 1; ++d)
                        if (in_bounds(gx + d, y))
                            map_->set(gx + d, y, Tile::IndoorFloor);
                // Horizontal segment
                int lo_x = std::min(gx, ot.x + 2), hi_x = std::max(gx, ot.x + 2);
                for (int x = lo_x; x <= hi_x; ++x)
                    for (int d = -1; d <= 1; ++d)
                        if (in_bounds(x, ot.y + 2 + d))
                            map_->set(x, ot.y + 2 + d, Tile::IndoorFloor);
            }

            // ============================================================
            // PHASE 2: Perimeter wall (makeshift, ~70% coverage)
            // ============================================================

            for (int x = perim_x; x < perim_x + perim_w; ++x) {
                if (in_bounds(x, perim_y) && prob(rng) < 0.70f)
                    map_->set(x, perim_y, Tile::Wall);
                if (in_bounds(x, perim_y + perim_h - 1) && prob(rng) < 0.70f)
                    map_->set(x, perim_y + perim_h - 1, Tile::Wall);
            }
            for (int y = perim_y; y < perim_y + perim_h; ++y) {
                if (in_bounds(perim_x, y) && prob(rng) < 0.70f)
                    map_->set(perim_x, y, Tile::Wall);
                if (in_bounds(perim_x + perim_w - 1, y) && prob(rng) < 0.70f)
                    map_->set(perim_x + perim_w - 1, y, Tile::Wall);
            }

            // North and south gate gaps (5-wide for main entrance)
            for (int dx = -2; dx <= 2; ++dx) {
                int gx = cx + dx;
                if (in_bounds(gx, perim_y))
                    map_->set(gx, perim_y, Tile::IndoorFloor);
                if (in_bounds(gx, perim_y + perim_h - 1))
                    map_->set(gx, perim_y + perim_h - 1, Tile::IndoorFloor);
            }

            // ============================================================
            // PHASE 3: Buildings (solid walls stamp over courtyard)
            // ============================================================

            // Corner guard towers (4x4 StructuralWall blocks, metal)
            int tower_positions[][2] = {
                {perim_x - 1, perim_y - 1},
                {perim_x + perim_w - 3, perim_y - 1},
                {perim_x - 1, perim_y + perim_h - 3},
                {perim_x + perim_w - 3, perim_y + perim_h - 3}
            };
            for (const auto& tp : tower_positions) {
                int tx = tp[0], ty = tp[1];
                for (int y = ty; y < ty + 4; ++y)
                    for (int x = tx; x < tx + 4; ++x)
                        if (in_bounds(x, y)) {
                            bool edge = (y == ty || y == ty + 3 ||
                                         x == tx || x == tx + 3);
                            map_->set(x, y, edge ? Tile::StructuralWall : Tile::IndoorFloor);
                            if (edge) map_->set_glyph_override(x, y, 0); // metal
                        }
            }

            // Main building (StructuralWall, salvage material — makeshift)
            for (int y = mb_y; y < mb_y + mb_h; ++y) {
                for (int x = mb_x; x < mb_x + mb_w; ++x) {
                    if (!in_bounds(x, y)) continue;
                    bool edge = (y == mb_y || y == mb_y + mb_h - 1 ||
                                 x == mb_x || x == mb_x + mb_w - 1);
                    map_->set(x, y, edge ? Tile::StructuralWall : Tile::IndoorFloor);
                    if (edge) map_->set_glyph_override(x, y, 3); // salvage
                }
            }
            // South door
            if (in_bounds(cx, mb_y + mb_h - 1)) {
                map_->set(cx, mb_y + mb_h - 1, Tile::Fixture);
                map_->add_fixture(cx, mb_y + mb_h - 1, make_fixture(FixtureType::Door));
            }
            // Internal dividing wall (splits east/west halves)
            for (int y = mb_y + 1; y < mb_y + mb_h - 1; ++y) {
                if (in_bounds(cx, y) && y != cy) { // gap at center for passage
                    map_->set(cx, y, Tile::StructuralWall);
                    map_->set_glyph_override(cx, y, 3);
                }
            }
            // Windows on main building
            if (in_bounds(mb_x, cy)) {
                map_->set(mb_x, cy, Tile::Fixture);
                map_->add_fixture(mb_x, cy, make_fixture(FixtureType::Window));
            }
            if (in_bounds(mb_x + mb_w - 1, cy)) {
                map_->set(mb_x + mb_w - 1, cy, Tile::Fixture);
                map_->add_fixture(mb_x + mb_w - 1, cy, make_fixture(FixtureType::Window));
            }

            // Storage shed (StructuralWall, wood material)
            for (int y = shed_y; y < shed_y + shed_h; ++y) {
                for (int x = shed_x; x < shed_x + shed_w; ++x) {
                    if (!in_bounds(x, y)) continue;
                    bool edge = (y == shed_y || y == shed_y + shed_h - 1 ||
                                 x == shed_x || x == shed_x + shed_w - 1);
                    map_->set(x, y, edge ? Tile::StructuralWall : Tile::IndoorFloor);
                    if (edge) map_->set_glyph_override(x, y, 2); // wood
                }
            }
            // Shed door on west wall
            if (in_bounds(shed_x, shed_y + shed_h / 2)) {
                map_->set(shed_x, shed_y + shed_h / 2, Tile::Fixture);
                map_->add_fixture(shed_x, shed_y + shed_h / 2, make_fixture(FixtureType::Door));
            }

            // Outer watchtowers (5x5, concrete)
            for (int i = 0; i < num_outer_towers; ++i) {
                auto& ot = outer_towers[i];
                for (int y = ot.y; y < ot.y + 5; ++y)
                    for (int x = ot.x; x < ot.x + 5; ++x)
                        if (in_bounds(x, y)) {
                            bool edge = (y == ot.y || y == ot.y + 4 ||
                                         x == ot.x || x == ot.x + 4);
                            map_->set(x, y, edge ? Tile::StructuralWall : Tile::IndoorFloor);
                            if (edge) map_->set_glyph_override(x, y, 1); // concrete
                        }
                // Door on side facing the compound
                int door_x = ot.x + 2, door_y = ot.y + 4;
                if (ot.y > cy) door_y = ot.y; // door on north if south of compound
                if (in_bounds(door_x, door_y)) {
                    map_->set(door_x, door_y, Tile::Fixture);
                    map_->add_fixture(door_x, door_y, make_fixture(FixtureType::Door));
                }
            }

            // ============================================================
            // PHASE 4: Fixtures
            // ============================================================

            // Command room fixtures (west half of main building)
            if (in_bounds(cx - 3, cy - 1)) {
                map_->set(cx - 3, cy - 1, Tile::Fixture);
                map_->add_fixture(cx - 3, cy - 1, make_fixture(FixtureType::Console));
            }
            // Portal in command room
            if (in_bounds(cx - 2, cy + 1))
                map_->set(cx - 2, cy + 1, Tile::Portal);
            // Barracks fixture (east half)
            if (in_bounds(cx + 3, cy - 1)) {
                map_->set(cx + 3, cy - 1, Tile::Fixture);
                map_->add_fixture(cx + 3, cy - 1, make_fixture(FixtureType::Bunk));
            }
            // Shed fixtures
            if (in_bounds(shed_x + 1, shed_y + 1)) {
                map_->set(shed_x + 1, shed_y + 1, Tile::Fixture);
                map_->add_fixture(shed_x + 1, shed_y + 1, make_fixture(FixtureType::Crate));
            }
            if (in_bounds(shed_x + 3, shed_y + 1)) {
                map_->set(shed_x + 3, shed_y + 1, Tile::Fixture);
                map_->add_fixture(shed_x + 3, shed_y + 1, make_fixture(FixtureType::Crate));
            }
            if (in_bounds(shed_x + 5, shed_y + 2)) {
                map_->set(shed_x + 5, shed_y + 2, Tile::Fixture);
                map_->add_fixture(shed_x + 5, shed_y + 2, make_fixture(FixtureType::Rack));
            }

            // Settlement props pass (reusable)
            scatter_settlement_props(map_, rng, props_->biome);

            break;
        }
        case Tile::OW_Landing: {
            // ============================================================
            // Landing Pad with Embedded Starship
            // ============================================================
            // Ship rooms use Wall/Floor (matching starship_generator exactly).
            // Pad surface uses IndoorFloor. No hull bounding box.

            // Ship room definitions (same as starship_generator)
            struct ShipRoom { int x, y, w, h; };
            static constexpr ShipRoom ship_rooms[] = {
                { 2,  6, 8,  6},   // Cockpit (0)
                {12,  5, 12, 8},   // Command Center (1)
                {26,  6, 10, 6},   // Mess Hall (2)
                {38,  5, 10, 8},   // Quarters (3)
            };
            static constexpr int ship_room_count = 4;

            // Offset: center the 50x20 ship grid on (cx, cy)
            int ox = cx - 25;
            int oy = cy - 10;

            // ---- PHASE 1: Landing pad surface ----
            int pad_x = cx - 26, pad_y = cy - 11;
            int pad_w = 52, pad_h = 22;
            for (int y = pad_y; y < pad_y + pad_h; ++y)
                for (int x = pad_x; x < pad_x + pad_w; ++x)
                    if (in_bounds(x, y))
                        map_->set(x, y, Tile::IndoorFloor);

            // Pad perimeter markings (StructuralWall, concrete)
            for (int x = pad_x; x < pad_x + pad_w; ++x) {
                if (in_bounds(x, pad_y)) {
                    map_->set(x, pad_y, Tile::StructuralWall);
                    map_->set_glyph_override(x, pad_y, 1);
                }
                if (in_bounds(x, pad_y + pad_h - 1)) {
                    map_->set(x, pad_y + pad_h - 1, Tile::StructuralWall);
                    map_->set_glyph_override(x, pad_y + pad_h - 1, 1);
                }
            }
            for (int y = pad_y; y < pad_y + pad_h; ++y) {
                if (in_bounds(pad_x, y)) {
                    map_->set(pad_x, y, Tile::StructuralWall);
                    map_->set_glyph_override(pad_x, y, 1);
                }
                if (in_bounds(pad_x + pad_w - 1, y)) {
                    map_->set(pad_x + pad_w - 1, y, Tile::StructuralWall);
                    map_->set_glyph_override(pad_x + pad_w - 1, y, 1);
                }
            }

            // 3-wide entry road south from pad + open pad perimeter for exit
            for (int d = -1; d <= 1; ++d) {
                if (in_bounds(cx + d, pad_y + pad_h - 1))
                    map_->set(cx + d, pad_y + pad_h - 1, Tile::IndoorFloor);
            }
            for (int y = pad_y + pad_h; y < std::min(h - 1, pad_y + pad_h + 10); ++y)
                for (int d = -1; d <= 1; ++d)
                    if (in_bounds(cx + d, y))
                        map_->set(cx + d, y, Tile::IndoorFloor);

            // ---- PHASE 2: Ship rooms (Wall/Floor, matching starship_generator) ----
            struct RoomInfo { int x1, y1, x2, y2; };
            RoomInfo room_rects[ship_room_count];

            for (int i = 0; i < ship_room_count; ++i) {
                const auto& sr = ship_rooms[i];
                int rx1 = ox + sr.x;
                int ry1 = oy + sr.y;
                int rx2 = rx1 + sr.w - 1;
                int ry2 = ry1 + sr.h - 1;
                room_rects[i] = {rx1, ry1, rx2, ry2};

                for (int y = ry1; y <= ry2; ++y) {
                    for (int x = rx1; x <= rx2; ++x) {
                        if (!in_bounds(x, y)) continue;
                        bool edge = (y == ry1 || y == ry2 || x == rx1 || x == rx2);
                        map_->set(x, y, edge ? Tile::StructuralWall : Tile::IndoorFloor);
                        if (edge) map_->set_glyph_override(x, y, 0); // metal
                    }
                }
            }

            // ---- PHASE 3: Corridors connecting rooms ----
            for (int i = 0; i < ship_room_count - 1; ++i) {
                int corr_y = (room_rects[i].y1 + room_rects[i].y2) / 2;
                int x1 = room_rects[i].x2;
                int x2 = room_rects[i + 1].x1;
                for (int x = x1; x <= x2; ++x) {
                    if (in_bounds(x, corr_y))
                        map_->set(x, corr_y, Tile::IndoorFloor);
                    // Wall borders only on Empty tiles
                    if (in_bounds(x, corr_y - 1) &&
                        map_->get(x, corr_y - 1) == Tile::Empty) {
                        map_->set(x, corr_y - 1, Tile::StructuralWall);
                        map_->set_glyph_override(x, corr_y - 1, 0);
                    }
                    if (in_bounds(x, corr_y + 1) &&
                        map_->get(x, corr_y + 1) == Tile::Empty) {
                        map_->set(x, corr_y + 1, Tile::StructuralWall);
                        map_->set_glyph_override(x, corr_y + 1, 0);
                    }
                }
            }

            // ---- PHASE 4: Airlock door on south wall of command center ----
            {
                auto& r = room_rects[1]; // Command Center
                int airlock_x = (r.x1 + r.x2) / 2;
                int airlock_y = r.y2; // south wall
                if (in_bounds(airlock_x, airlock_y)) {
                    map_->set(airlock_x, airlock_y, Tile::Fixture);
                    map_->add_fixture(airlock_x, airlock_y, make_fixture(FixtureType::Door));
                }
            }

            // ---- PHASE 5: Ship room fixtures ----
            auto place_fix = [&](int fx, int fy, FixtureType type) {
                if (in_bounds(fx, fy) && map_->get(fx, fy) == Tile::IndoorFloor) {
                    map_->set(fx, fy, Tile::Fixture);
                    map_->add_fixture(fx, fy, make_fixture(type));
                }
            };

            // Cockpit (room 0): Viewports along north wall, Consoles below
            {
                auto& r = room_rects[0];
                for (int x = r.x1 + 1; x <= r.x2 - 1; ++x)
                    place_fix(x, r.y1 + 1, FixtureType::Viewport);
                for (int x = r.x1 + 1; x <= r.x2 - 1; x += 2)
                    place_fix(x, r.y1 + 2, FixtureType::Console);
            }

            // Command Center (room 1): StarChart center, Consoles below
            {
                auto& r = room_rects[1];
                int rcx = (r.x1 + 1 + r.x2 - 1) / 2;
                int rcy = (r.y1 + 1 + r.y2 - 1) / 2;
                place_fix(rcx, rcy, FixtureType::StarChart);
                for (int x = rcx - 1; x <= rcx + 1; ++x)
                    place_fix(x, rcy + 1, FixtureType::Console);
            }

            // Mess Hall (room 2): Table center, Stools, FoodTerminal
            {
                auto& r = room_rects[2];
                int rcx = (r.x1 + 1 + r.x2 - 1) / 2;
                int rcy = (r.y1 + 1 + r.y2 - 1) / 2;
                place_fix(rcx, rcy, FixtureType::Table);
                place_fix(rcx - 1, rcy, FixtureType::Stool);
                place_fix(rcx + 1, rcy, FixtureType::Stool);
                place_fix(rcx, r.y1 + 1, FixtureType::FoodTerminal);
            }

            // Quarters (room 3): Bunks along walls, RestPod at end
            {
                auto& r = room_rects[3];
                for (int y = r.y1 + 1; y <= r.y2 - 1; y += 2) {
                    place_fix(r.x1 + 1, y, FixtureType::Bunk);
                    place_fix(r.x2 - 1, y, FixtureType::Bunk);
                }
                int rcx = (r.x1 + 1 + r.x2 - 1) / 2;
                place_fix(rcx, r.y2 - 1, FixtureType::RestPod);
            }

            // ---- PHASE 6: Ship room regions ----
            {
                struct ShipRoomInfo {
                    RoomFlavor flavor;
                    const char* name;
                    const char* enter_message;
                };
                static const ShipRoomInfo room_info[] = {
                    {RoomFlavor::ShipCockpit, "Cockpit",
                        "The cockpit. Stars drift beyond the viewport, navigation consoles glow softly."},
                    {RoomFlavor::ShipCommandCenter, "Command Center",
                        "The command center. A star chart terminal dominates the room."},
                    {RoomFlavor::ShipMessHall, "Mess Hall",
                        "The mess hall. A small table and food terminal — comforts of home."},
                    {RoomFlavor::ShipQuarters, "Sleeping Quarters",
                        "The sleeping quarters. Bunks line the walls. A rest pod hums at the far end."},
                };

                for (int i = 0; i < ship_room_count; ++i) {
                    Region reg;
                    reg.type = RegionType::Room;
                    reg.lit = true;
                    reg.flavor = room_info[i].flavor;
                    reg.name = room_info[i].name;
                    reg.enter_message = room_info[i].enter_message;
                    reg.features = default_features(room_info[i].flavor);
                    int rid = map_->add_region(reg);

                    auto& r = room_rects[i];
                    for (int y = r.y1; y <= r.y2; ++y)
                        for (int x = r.x1; x <= r.x2; ++x)
                            if (in_bounds(x, y))
                                map_->set_region(x, y, rid);
                }

                // Corridor region
                Region creg;
                creg.type = RegionType::Corridor;
                creg.lit = true;
                creg.flavor = RoomFlavor::CorridorPlain;
                creg.name = "Ship Corridor";
                creg.enter_message = "A narrow corridor connecting the ship's compartments.";
                int crid = map_->add_region(creg);

                // Assign corridor tiles (Floor tiles not yet assigned to a room)
                for (int i = 0; i < ship_room_count - 1; ++i) {
                    int corr_y = (room_rects[i].y1 + room_rects[i].y2) / 2;
                    int x1 = room_rects[i].x2;
                    int x2 = room_rects[i + 1].x1;
                    for (int x = x1; x <= x2; ++x) {
                        for (int dy = -1; dy <= 1; ++dy) {
                            int cy2 = corr_y + dy;
                            if (in_bounds(x, cy2) && map_->region_id(x, cy2) < 0)
                                map_->set_region(x, cy2, crid);
                        }
                    }
                }
            }

            // ---- PHASE 7: Control tower building ----
            {
                auto place_fix_indoor = [&](int fx, int fy, FixtureType type) {
                    if (in_bounds(fx, fy) && map_->get(fx, fy) == Tile::IndoorFloor) {
                        map_->set(fx, fy, Tile::Fixture);
                        map_->add_fixture(fx, fy, make_fixture(type));
                    }
                };

                int tw_x = pad_x + 2, tw_y = pad_y + pad_h + 1;
                int tw_w = 6, tw_h = 5;
                for (int y = tw_y; y < tw_y + tw_h; ++y) {
                    for (int x = tw_x; x < tw_x + tw_w; ++x) {
                        if (!in_bounds(x, y)) continue;
                        bool edge = (y == tw_y || y == tw_y + tw_h - 1 ||
                                     x == tw_x || x == tw_x + tw_w - 1);
                        map_->set(x, y, edge ? Tile::StructuralWall : Tile::IndoorFloor);
                        if (edge) map_->set_glyph_override(x, y, 1);
                    }
                }
                if (in_bounds(tw_x + tw_w / 2, tw_y)) {
                    map_->set(tw_x + tw_w / 2, tw_y, Tile::Fixture);
                    map_->add_fixture(tw_x + tw_w / 2, tw_y, make_fixture(FixtureType::Door));
                }
                place_fix_indoor(tw_x + 2, tw_y + 2, FixtureType::Console);
                place_fix_indoor(tw_x + 3, tw_y + 2, FixtureType::Console);

                // Fuel/supply area
                int supply_x = pad_x + pad_w - 8;
                int supply_y = pad_y + pad_h + 2;
                place_fix_indoor(supply_x, supply_y, FixtureType::Crate);
                place_fix_indoor(supply_x + 2, supply_y, FixtureType::Crate);
                place_fix_indoor(supply_x + 4, supply_y, FixtureType::Rack);
            }

            break;
        }
        case Tile::OW_Beacon: {
            // Central spire: ring of walls with portal at center
            int spire_r = 4;
            for (int dy = -spire_r; dy <= spire_r; ++dy) {
                for (int dx = -spire_r; dx <= spire_r; ++dx) {
                    int px = cx + dx, py = cy + dy;
                    if (px < 0 || px >= w || py < 0 || py >= h) continue;
                    float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                    if (dist > static_cast<float>(spire_r) - 1.5f && dist < static_cast<float>(spire_r) - 0.5f)
                        map_->set(px, py, Tile::Wall);
                    else if (dist < static_cast<float>(spire_r) - 1.5f)
                        map_->set(px, py, Tile::Floor);
                }
            }
            map_->set(cx, cy, Tile::Portal);

            // Satellite pylons at cardinal directions
            for (auto [ddx, ddy] : std::initializer_list<std::pair<int,int>>{{0,-7},{0,7},{-7,0},{7,0}}) {
                int px = cx + ddx, py = cy + ddy;
                if (px >= 0 && px < w && py >= 0 && py < h)
                    map_->set(px, py, Tile::Wall);
                if (px+1 >= 0 && px+1 < w && py >= 0 && py < h)
                    map_->set(px+1, py, Tile::Wall);
            }
            break;
        }
        case Tile::OW_Megastructure: {
            // Large rectangular foundation
            int fw = 10, fh = 8;
            for (int dy = -fh/2; dy <= fh/2; ++dy) {
                for (int dx = -fw/2; dx <= fw/2; ++dx) {
                    int px = cx + dx, py = cy + dy;
                    if (px < 0 || px >= w || py < 0 || py >= h) continue;
                    if (std::abs(dx) >= fw/2 - 1 || std::abs(dy) >= fh/2 - 1)
                        map_->set(px, py, Tile::Wall);
                    else
                        map_->set(px, py, Tile::Floor);
                }
            }
            auto console_fd = make_fixture(FixtureType::Console);
            map_->add_fixture(cx - 2, cy, console_fd);
            map_->add_fixture(cx + 2, cy, console_fd);
            map_->set(cx, cy - fh/2, Tile::Floor);  // north door
            map_->set(cx, cy + fh/2, Tile::Floor);  // south door
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
            if (map_->region_id(x, y) < 0) // preserve ship room regions
                map_->set_region(x, y, rid);
}

// --- Factory ---

std::unique_ptr<MapGenerator> make_detail_map_generator() {
    return std::make_unique<DetailMapGenerator>();
}

} // namespace astra
