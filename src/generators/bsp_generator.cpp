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
        base_thick = std::max(base_thick, 1);

        // For shallow walls: only draw a partial segment (40-80% of span).
        // This prevents every BSP split from forming a closed rectangle.
        // Deep walls (nucleus areas) draw full length to form actual rooms.
        bool partial = (node.depth < 3) && !ca.is_nucleus && !cb.is_nucleus;

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
                // Draw 40-80% of the span, offset randomly
                float coverage = 0.4f + hash_noise(wall_y, 0,
                    wall_seed) * 0.4f;
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
                float coverage = 0.4f + hash_noise(wall_x, 0,
                    wall_seed) * 0.4f;
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
        base_thick = std::max(base_thick, 1);

        // Intentional openings — former doorways and hallways.
        // Shallow (thick) walls get wide openings like collapsed hallways.
        // Deep (thin) walls get narrower doorway-sized openings.
        int gap_width;
        int num_gaps;
        if (node.depth < 2) {
            gap_width = 4 + static_cast<int>(hash_noise(
                static_cast<int>(i), 0, seed) * 3.0f);  // 4-6 wide
            num_gaps = 1;
        } else if (node.depth < 4) {
            gap_width = 3;  // corridor-sized
            num_gaps = 1 + static_cast<int>(hash_noise(
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

// --- generate ---

void BspGenerator::generate(TileMap& map, RuinPlan& plan,
                             std::mt19937& rng) const {
    const auto& fp = plan.footprint;

    // 1. Pick 2-4 nucleus points within the central 50% of the footprint
    int num_nuclei = std::uniform_int_distribution<int>(2, 4)(rng);
    std::vector<std::pair<int,int>> nuclei;
    int cx0 = fp.x + fp.w / 4;
    int cy0 = fp.y + fp.h / 4;
    int cx1 = fp.x + fp.w * 3 / 4;
    int cy1 = fp.y + fp.h * 3 / 4;
    for (int i = 0; i < num_nuclei; ++i) {
        int nx = std::uniform_int_distribution<int>(cx0, std::max(cx0, cx1))(rng);
        int ny = std::uniform_int_distribution<int>(cy0, std::max(cy0, cy1))(rng);
        nuclei.emplace_back(nx, ny);
    }

    // 2. Init BSP tree with root node
    plan.bsp_nodes.clear();
    BspNode root;
    root.area = fp;
    root.depth = 0;
    plan.bsp_nodes.push_back(root);

    // 3. Subdivide recursively (base max depth = 5)
    subdivide(plan.bsp_nodes, 0, 5, nuclei, plan.civ.split_regularity, rng);

    // 4. Place IndoorFloor only in nucleus leaf areas (deep interior rooms).
    //    Shallow leaves keep their natural terrain — the ruin walls cut through it.
    for (const auto& node : plan.bsp_nodes) {
        if (!node.is_leaf || !node.is_nucleus) continue;
        for (int y = node.area.y; y < node.area.y + node.area.h; ++y) {
            for (int x = node.area.x; x < node.area.x + node.area.w; ++x) {
                if (x < 0 || x >= map.width() || y < 0 || y >= map.height())
                    continue;
                if (map.get(x, y) == Tile::Water) continue;
                map.set(x, y, Tile::IndoorFloor);
            }
        }
    }

    // 5. Materialize walls
    materialize_walls(map, plan);
}

} // namespace astra
