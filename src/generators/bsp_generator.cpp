#include "astra/bsp_generator.h"
#include "astra/ruin_decay.h"  // CF_RUIN_TINT
#include "astra/noise.h"

#include <algorithm>
#include <cmath>

namespace astra {

namespace {

float dist_to_nearest(int x, int y,
                      const std::vector<std::pair<int,int>>& nuclei) {
    float best = 1e9f;
    for (auto& [nx, ny] : nuclei) {
        float dx = static_cast<float>(x - nx);
        float dy = static_cast<float>(y - ny);
        float d = std::sqrt(dx * dx + dy * dy);
        if (d < best) best = d;
    }
    return best;
}

int base_thick_for_depth(int depth) {
    switch (depth) {
        case 0: return 8;
        case 1: return 6;
        case 2: return 5;
        case 3: return 4;
        case 4: return 3;
        default: return 2;
    }
}

} // anonymous namespace

// --- nucleus_depth ---

int BspGenerator::nucleus_depth(int base_depth, int x, int y,
                                const std::vector<std::pair<int,int>>& nuclei,
                                int radius) const {
    float dist = dist_to_nearest(x, y, nuclei);
    if (dist > static_cast<float>(radius)) return base_depth;

    // Closer to nucleus -> more extra depth (up to +3)
    float t = 1.0f - (dist / static_cast<float>(radius));
    int bonus = static_cast<int>(t * 3.0f);
    bonus = std::clamp(bonus, 1, 3);
    return base_depth + bonus;
}

// --- subdivide ---

void BspGenerator::subdivide(std::vector<BspNode>& nodes, int node_idx,
                              int max_depth,
                              const std::vector<std::pair<int,int>>& nuclei,
                              float regularity, std::mt19937& rng) const {
    auto& node = nodes[node_idx];

    // Compute effective max depth based on proximity to nuclei
    int cx = node.area.x + node.area.w / 2;
    int cy = node.area.y + node.area.h / 2;
    int radius = std::max(node.area.w, node.area.h);
    int effective_max = nucleus_depth(max_depth, cx, cy, nuclei, radius);

    if (node.depth >= effective_max) {
        node.is_leaf = true;
        return;
    }

    // Minimum partition sizes
    int min_side = (node.depth < 2) ? 20 : 8;

    // Alternate split direction more aggressively to avoid monotonous bands.
    // At even depths split one way, odd depths the other, with some randomness.
    bool split_horiz;
    if (node.area.w > node.area.h * 2) {
        split_horiz = false;  // very wide — must split vertically
    } else if (node.area.h > node.area.w * 2) {
        split_horiz = true;   // very tall — must split horizontally
    } else {
        // Alternate based on depth, with 30% chance to override
        split_horiz = (node.depth % 2 == 0);
        if (std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) < 0.3f) {
            split_horiz = !split_horiz;
        }
    }

    int length = split_horiz ? node.area.h : node.area.w;
    if (length < min_side * 2) {
        node.is_leaf = true;
        return;
    }

    // Split position: blend between center and random based on regularity
    float center = static_cast<float>(length) / 2.0f;
    float rand_pos = static_cast<float>(
        std::uniform_int_distribution<int>(min_side, length - min_side)(rng));
    float split_f = regularity * center + (1.0f - regularity) * rand_pos;
    int split_pos = std::clamp(static_cast<int>(split_f), min_side, length - min_side);

    // Create children
    BspNode child_a;
    BspNode child_b;
    child_a.depth = node.depth + 1;
    child_b.depth = node.depth + 1;

    if (split_horiz) {
        child_a.area = {node.area.x, node.area.y,
                        node.area.w, split_pos};
        child_b.area = {node.area.x, node.area.y + split_pos,
                        node.area.w, node.area.h - split_pos};
    } else {
        child_a.area = {node.area.x, node.area.y,
                        split_pos, node.area.h};
        child_b.area = {node.area.x + split_pos, node.area.y,
                        node.area.w - split_pos, node.area.h};
    }

    int idx_a = static_cast<int>(nodes.size());
    nodes.push_back(child_a);
    int idx_b = static_cast<int>(nodes.size());
    nodes.push_back(child_b);

    // Re-fetch node reference after push_back (may have reallocated)
    nodes[node_idx].child_a = idx_a;
    nodes[node_idx].child_b = idx_b;
    nodes[node_idx].is_leaf = false;

    // Mark nucleus proximity on children
    float na_dist = dist_to_nearest(
        nodes[idx_a].area.x + nodes[idx_a].area.w / 2,
        nodes[idx_a].area.y + nodes[idx_a].area.h / 2,
        nuclei);
    float nb_dist = dist_to_nearest(
        nodes[idx_b].area.x + nodes[idx_b].area.w / 2,
        nodes[idx_b].area.y + nodes[idx_b].area.h / 2,
        nuclei);
    float threshold = static_cast<float>(radius) * 0.5f;
    nodes[idx_a].is_nucleus = (na_dist < threshold);
    nodes[idx_b].is_nucleus = (nb_dist < threshold);

    // Recurse
    subdivide(nodes, idx_a, max_depth, nuclei, regularity, rng);
    subdivide(nodes, idx_b, max_depth, nuclei, regularity, rng);
}

// --- materialize_walls ---

void BspGenerator::materialize_walls(TileMap& map,
                                     const RuinPlan& plan) const {
    const auto& nodes = plan.bsp_nodes;
    const auto& fp = plan.footprint;
    unsigned seed = static_cast<unsigned>(fp.x * 73856093u ^ fp.y * 19349663u);

    // Phase 1: draw walls for each non-leaf node
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& node = nodes[i];
        if (node.is_leaf || node.child_a < 0 || node.child_b < 0) continue;

        const auto& ca = nodes[node.child_a];
        const auto& cb = nodes[node.child_b];

        // Determine split direction and boundary position
        bool horiz = (ca.area.x == cb.area.x);  // same x -> horizontal split
        int base_thick = static_cast<int>(
            static_cast<float>(base_thick_for_depth(node.depth))
            * plan.civ.wall_thickness_bias);
        base_thick = std::clamp(base_thick, 1, plan.civ.max_wall_thickness);

        // For shallow walls: draw a partial segment scaled by decay_modifier.
        // decay_modifier 0 = full walls, 1 = 40-80% coverage.
        // Deep walls (nucleus areas) always draw full length.
        float dm = plan.decay_modifier;
        bool partial = (node.depth < 3) && !ca.is_nucleus && !cb.is_nucleus && dm > 0.05f;

        // Determine wall segment seed for consistent partial placement
        unsigned wall_seed = seed + static_cast<unsigned>(i * 7919u);

        if (horiz) {
            int wall_y = ca.area.y + ca.area.h;
            int full_x0 = node.area.x;
            int full_x1 = node.area.x + node.area.w;
            int span = full_x1 - full_x0;

            int x0 = full_x0;
            int x1 = full_x1;
            if (partial && span > 20) {
                // Coverage scales: dm=0 -> 100%, dm=1 -> 40-80%
                float min_cov = 1.0f - dm * 0.6f;  // 1.0 -> 0.4
                float max_cov = 1.0f - dm * 0.2f;  // 1.0 -> 0.8
                float coverage = min_cov + hash_noise(wall_y, 0,
                    wall_seed) * (max_cov - min_cov);
                int seg_len = static_cast<int>(span * coverage);
                int max_offset = span - seg_len;
                int offset = static_cast<int>(hash_noise(0, wall_y,
                    wall_seed + 1u) * max_offset);
                x0 = full_x0 + offset;
                x1 = x0 + seg_len;
            }

            for (int x = x0; x < x1; ++x) {
                float n = fbm(static_cast<float>(x) * 0.08f,
                              static_cast<float>(wall_y) * 0.08f,
                              seed + static_cast<unsigned>(i), 0.2f, 2);
                int thick = std::max(1, static_cast<int>(
                    static_cast<float>(base_thick) * (0.8f + n * 0.4f)));

                int half = thick / 2;
                for (int dy = -half; dy < thick - half; ++dy) {
                    int wy = wall_y + dy;
                    if (wy < 0 || wy >= map.height()) continue;
                    if (x < 0 || x >= map.width()) continue;
                    if (map.get(x, wy) == Tile::Water) continue;
                    map.set(x, wy, Tile::Wall);
                    map.set_custom_flag(x, wy, CF_RUIN_TINT);
                    set_ruin_civ(map, x, wy, plan.civ.civ_index);
                }
            }
        } else {
            int wall_x = ca.area.x + ca.area.w;
            int full_y0 = node.area.y;
            int full_y1 = node.area.y + node.area.h;
            int span = full_y1 - full_y0;

            int y0 = full_y0;
            int y1 = full_y1;
            if (partial && span > 20) {
                float min_cov = 1.0f - dm * 0.6f;
                float max_cov = 1.0f - dm * 0.2f;
                float coverage = min_cov + hash_noise(wall_x, 0,
                    wall_seed) * (max_cov - min_cov);
                int seg_len = static_cast<int>(span * coverage);
                int max_offset = span - seg_len;
                int offset = static_cast<int>(hash_noise(0, wall_x,
                    wall_seed + 1u) * max_offset);
                y0 = full_y0 + offset;
                y1 = y0 + seg_len;
            }

            for (int y = y0; y < y1; ++y) {
                float n = fbm(static_cast<float>(wall_x) * 0.08f,
                              static_cast<float>(y) * 0.08f,
                              seed + static_cast<unsigned>(i), 0.2f, 2);
                int thick = std::max(1, static_cast<int>(
                    static_cast<float>(base_thick) * (0.8f + n * 0.4f)));

                int half = thick / 2;
                for (int dx = -half; dx < thick - half; ++dx) {
                    int wx = wall_x + dx;
                    if (wx < 0 || wx >= map.width()) continue;
                    if (y < 0 || y >= map.height()) continue;
                    if (map.get(wx, y) == Tile::Water) continue;
                    map.set(wx, y, Tile::Wall);
                    map.set_custom_flag(wx, y, CF_RUIN_TINT);
                    set_ruin_civ(map, wx, y, plan.civ.civ_index);
                }
            }
        }
    }

    // Phase 2: carve gaps for navigability
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& node = nodes[i];
        if (node.is_leaf || node.child_a < 0 || node.child_b < 0) continue;

        const auto& ca = nodes[node.child_a];
        const auto& cb = nodes[node.child_b];

        bool horiz = (ca.area.x == cb.area.x);
        int base_thick = static_cast<int>(
            static_cast<float>(base_thick_for_depth(node.depth))
            * plan.civ.wall_thickness_bias);
        base_thick = std::clamp(base_thick, 1, plan.civ.max_wall_thickness);

        // Intentional openings — passageways always exist (structural).
        // Decay modifier scales the WIDTH of openings, not their existence.
        float dm = plan.decay_modifier;
        int gap_width;
        int num_gaps;
        if (node.depth < 2) {
            gap_width = 2 + static_cast<int>(dm * (2.0f + hash_noise(
                static_cast<int>(i), 0, seed) * 3.0f));  // 2-7 scaled by dm
            num_gaps = 1;
        } else if (node.depth < 4) {
            gap_width = 2 + static_cast<int>(dm * 1.0f);  // 2-3
            num_gaps = 1 + static_cast<int>(dm * hash_noise(
                static_cast<int>(i), 1, seed) * 1.5f);  // 1-2
        } else {
            gap_width = 2;  // doorway
            num_gaps = 1;
        }

        // Seed gap positions deterministically
        unsigned gap_seed = seed + static_cast<unsigned>(i * 997u);

        if (horiz) {
            int wall_y = ca.area.y + ca.area.h;
            int x0 = node.area.x;
            int length = node.area.w;

            for (int g = 0; g < num_gaps; ++g) {
                unsigned gh = gap_seed + static_cast<unsigned>(g) * 31u;
                gh = (gh ^ (gh >> 13)) * 1103515245u;
                gh = gh ^ (gh >> 16);
                int gap_start = x0 + static_cast<int>(gh % std::max(1u,
                    static_cast<unsigned>(length - gap_width)));

                for (int dx = 0; dx < gap_width; ++dx) {
                    int gx = gap_start + dx;
                    if (gx < 0 || gx >= map.width()) continue;
                    // Punch through full wall thickness
                    int half = base_thick / 2;
                    for (int dy = -half; dy < base_thick - half; ++dy) {
                        int gy = wall_y + dy;
                        if (gy < 0 || gy >= map.height()) continue;
                        if (map.get(gx, gy) == Tile::Water) continue;
                        map.set(gx, gy, Tile::IndoorFloor);
                    }
                }
            }
        } else {
            int wall_x = ca.area.x + ca.area.w;
            int y0 = node.area.y;
            int length = node.area.h;

            for (int g = 0; g < num_gaps; ++g) {
                unsigned gh = gap_seed + static_cast<unsigned>(g) * 31u;
                gh = (gh ^ (gh >> 13)) * 1103515245u;
                gh = gh ^ (gh >> 16);
                int gap_start = y0 + static_cast<int>(gh % std::max(1u,
                    static_cast<unsigned>(length - gap_width)));

                for (int dy = 0; dy < gap_width; ++dy) {
                    int gy = gap_start + dy;
                    if (gy < 0 || gy >= map.height()) continue;
                    int half = base_thick / 2;
                    for (int dx = -half; dx < base_thick - half; ++dx) {
                        int gx = wall_x + dx;
                        if (gx < 0 || gx >= map.width()) continue;
                        if (map.get(gx, gy) == Tile::Water) continue;
                        map.set(gx, gy, Tile::IndoorFloor);
                    }
                }
            }
        }
    }
}

// --- generate_noise_walls ---
// Outdoor partition walls using ridge noise. Creates organic wall lines
// that curve, branch, and dead-end through the natural terrain.

void BspGenerator::generate_noise_walls(TileMap& map, const RuinPlan& plan,
                                        const std::vector<Rect>& nucleus_zones) const {
    const auto& fp = plan.footprint;
    unsigned seed = static_cast<unsigned>(fp.x * 73856093u ^ fp.y * 19349663u);

    int base_thick = static_cast<int>(
        base_thick_for_depth(0) * plan.civ.wall_thickness_bias);
    base_thick = std::clamp(base_thick, 2, plan.civ.max_wall_thickness);

    // Use two overlapping ridge noise layers at different scales and angles
    // to create a network of intersecting wall lines
    float scale1 = 0.02f + plan.civ.split_regularity * 0.01f;
    float scale2 = 0.015f;
    float threshold = 0.72f;  // higher = fewer, more distinct walls

    for (int y = fp.y; y < fp.y + fp.h; ++y) {
        for (int x = fp.x; x < fp.x + fp.w; ++x) {
            if (x < 0 || x >= map.width() || y < 0 || y >= map.height())
                continue;
            if (map.get(x, y) == Tile::Water) continue;

            // Skip nucleus zones — BSP handles those
            bool in_nucleus = false;
            for (const auto& nz : nucleus_zones) {
                if (nz.contains(x, y)) { in_nucleus = true; break; }
            }
            if (in_nucleus) continue;

            // Layer 1: horizontal-ish walls
            float n1 = ridge_noise(static_cast<float>(x), static_cast<float>(y),
                                   seed, scale1, 4);
            // Layer 2: vertical-ish walls (rotated coordinates)
            float n2 = ridge_noise(static_cast<float>(y) * 0.7f,
                                   static_cast<float>(x) * 0.7f,
                                   seed + 9973u, scale2, 4);

            // Combine: wall where either layer has a ridge
            float wall_val = std::max(n1, n2);

            if (wall_val > threshold) {
                // Thickness modulated by how far above threshold
                float excess = (wall_val - threshold) / (1.0f - threshold);
                int thick = std::max(1, static_cast<int>(base_thick * excess));

                // Place wall at this position (thick walls extend in the
                // direction perpendicular to the dominant ridge)
                map.set(x, y, Tile::Wall);
                map.set_custom_flag(x, y, CF_RUIN_TINT);
                set_ruin_civ(map, x, y, plan.civ.civ_index);
            }
        }
    }

    // Carve passages through the noise walls for navigability.
    // Place 8-15 random corridor cuts across the map.
    std::mt19937 gap_rng(seed + 42u);
    float dm = plan.decay_modifier;
    int num_corridors = 8 + static_cast<int>(dm * 7.0f);
    std::uniform_int_distribution<int> x_dist(fp.x, fp.x + fp.w - 1);
    std::uniform_int_distribution<int> y_dist(fp.y, fp.y + fp.h - 1);

    for (int c = 0; c < num_corridors; ++c) {
        // Random corridor: either horizontal or vertical
        bool horizontal = std::uniform_int_distribution<int>(0, 1)(gap_rng);
        int gap_width = 2 + static_cast<int>(dm * 2.0f);

        if (horizontal) {
            int cy = y_dist(gap_rng);
            int cx_start = x_dist(gap_rng);
            int length = 15 + std::uniform_int_distribution<int>(0, 30)(gap_rng);
            for (int dx = 0; dx < length; ++dx) {
                for (int dy = 0; dy < gap_width; ++dy) {
                    int gx = cx_start + dx;
                    int gy = cy + dy;
                    if (gx < 0 || gx >= map.width() || gy < 0 || gy >= map.height())
                        continue;
                    if (map.get(gx, gy) == Tile::Wall) {
                        map.set(gx, gy, Tile::Floor);
                    }
                }
            }
        } else {
            int cx = x_dist(gap_rng);
            int cy_start = y_dist(gap_rng);
            int length = 10 + std::uniform_int_distribution<int>(0, 20)(gap_rng);
            for (int dy = 0; dy < length; ++dy) {
                for (int dx = 0; dx < gap_width; ++dx) {
                    int gx = cx + dx;
                    int gy = cy_start + dy;
                    if (gx < 0 || gx >= map.width() || gy < 0 || gy >= map.height())
                        continue;
                    if (map.get(gx, gy) == Tile::Wall) {
                        map.set(gx, gy, Tile::Floor);
                    }
                }
            }
        }
    }
}

// --- generate ---

void BspGenerator::generate(TileMap& map, RuinPlan& plan,
                             std::mt19937& rng) const {
    const auto& fp = plan.footprint;

    // 1. Pick 2-4 nucleus points within the central 50% of the footprint
    int num_nuclei = std::uniform_int_distribution<int>(2, 4)(rng);
    std::vector<std::pair<int,int>> nuclei;
    std::vector<Rect> nucleus_zones;
    int cx0 = fp.x + fp.w / 4;
    int cy0 = fp.y + fp.h / 4;
    int cx1 = fp.x + fp.w * 3 / 4;
    int cy1 = fp.y + fp.h * 3 / 4;
    for (int i = 0; i < num_nuclei; ++i) {
        int nx = std::uniform_int_distribution<int>(cx0, std::max(cx0, cx1))(rng);
        int ny = std::uniform_int_distribution<int>(cy0, std::max(cy0, cy1))(rng);
        nuclei.emplace_back(nx, ny);
        // Define a rectangular zone around each nucleus for BSP
        int zw = std::uniform_int_distribution<int>(30, 50)(rng);
        int zh = std::uniform_int_distribution<int>(20, 35)(rng);
        nucleus_zones.push_back({nx - zw / 2, ny - zh / 2, zw, zh});
    }

    // 2. Phase 1: Noise-based outdoor wall network
    generate_noise_walls(map, plan, nucleus_zones);

    // 3. Phase 2: BSP subdivision for each nucleus zone
    plan.bsp_nodes.clear();
    for (size_t i = 0; i < nucleus_zones.size(); ++i) {
        auto& nz = nucleus_zones[i];
        // Clamp to map bounds
        nz.x = std::max(0, nz.x);
        nz.y = std::max(0, nz.y);
        if (nz.x + nz.w > map.width()) nz.w = map.width() - nz.x;
        if (nz.y + nz.h > map.height()) nz.h = map.height() - nz.y;

        // Place IndoorFloor in nucleus zone
        for (int y = nz.y; y < nz.y + nz.h; ++y) {
            for (int x = nz.x; x < nz.x + nz.w; ++x) {
                if (x < 0 || x >= map.width() || y < 0 || y >= map.height())
                    continue;
                if (map.get(x, y) == Tile::Water) continue;
                map.set(x, y, Tile::IndoorFloor);
            }
        }

        // BSP subdivide this zone
        int root_idx = static_cast<int>(plan.bsp_nodes.size());
        BspNode root;
        root.area = nz;
        root.depth = 2;  // start at depth 2 so walls are thinner (interior)
        root.is_nucleus = true;
        plan.bsp_nodes.push_back(root);

        subdivide(plan.bsp_nodes, root_idx, 6, nuclei,
                  plan.civ.split_regularity, rng);
    }

    // 4. Materialize BSP walls (only for nucleus interior walls)
    materialize_walls(map, plan);
}

} // namespace astra
