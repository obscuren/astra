#include "astra/biome_profile.h"
#include "astra/noise.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace astra {

// ---------------------------------------------------------------------------
// Helpers for grid-aligned water shapes
// ---------------------------------------------------------------------------

// Fill an axis-aligned rectangle with moisture
static void stamp_rect(float* grid, int w, int h,
                        int x1, int y1, int x2, int y2, float val) {
    x1 = std::max(0, x1); y1 = std::max(0, y1);
    x2 = std::min(w - 1, x2); y2 = std::min(h - 1, y2);
    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
            grid[y * w + x] = std::max(grid[y * w + x], val);
}

// Fill a horizontal corridor (1 tile high, variable width)
static void stamp_h_corridor(float* grid, int w, int h,
                              int x1, int x2, int y, int thickness, float val) {
    if (x1 > x2) std::swap(x1, x2);
    int half = thickness / 2;
    stamp_rect(grid, w, h, x1, y - half, x2, y + half, val);
}

// Fill a vertical corridor
static void stamp_v_corridor(float* grid, int w, int h,
                              int x, int y1, int y2, int thickness, float val) {
    if (y1 > y2) std::swap(y1, y2);
    int half = thickness / 2;
    stamp_rect(grid, w, h, x - half, y1, x + half, y2, val);
}

// ---------------------------------------------------------------------------
// Strategy 1: moisture_none — no-op
// ---------------------------------------------------------------------------
void moisture_none(float* /*grid*/, int /*w*/, int /*h*/,
                   std::mt19937& /*rng*/, const float* /*elevation*/,
                   const BiomeProfile& /*prof*/) {}

// ---------------------------------------------------------------------------
// Strategy 2: moisture_pools — rectangular pools in low elevation
// Grid-aligned rectangular water bodies, not noise blobs.
// ---------------------------------------------------------------------------
void moisture_pools(float* grid, int w, int h,
                    std::mt19937& rng, const float* elevation,
                    const BiomeProfile& prof) {
    int num_pools = 4 + static_cast<int>(rng() % 5); // 4-8

    for (int i = 0; i < num_pools; ++i) {
        // Pick position biased toward low elevation
        int best_x = 0, best_y = 0;
        float best_elev = 2.0f;
        for (int s = 0; s < 5; ++s) {
            int sx = static_cast<int>(rng() % static_cast<unsigned>(w));
            int sy = static_cast<int>(rng() % static_cast<unsigned>(h));
            float e = elevation[sy * w + sx];
            if (e < best_elev) {
                best_elev = e;
                best_x = sx;
                best_y = sy;
            }
        }

        // Skip if elevation too high
        if (best_elev > prof.flood_level) continue;

        // Random rectangular pool dimensions
        int pw = 4 + static_cast<int>(rng() % 12); // 4-15 wide
        int ph = 3 + static_cast<int>(rng() % 8);  // 3-10 tall

        // Stamp the rectangle
        int x1 = best_x - pw / 2;
        int y1 = best_y - ph / 2;
        stamp_rect(grid, w, h, x1, y1, x1 + pw, y1 + ph, 1.0f);

        // Occasionally add a notch or extension for irregular shape
        if (rng() % 3 == 0) {
            int ext_w = 2 + static_cast<int>(rng() % (pw / 2 + 1));
            int ext_h = 2 + static_cast<int>(rng() % 3);
            int side = static_cast<int>(rng() % 4);
            switch (side) {
                case 0: stamp_rect(grid, w, h, x1, y1 - ext_h, x1 + ext_w, y1, 1.0f); break;
                case 1: stamp_rect(grid, w, h, x1, y1 + ph, x1 + ext_w, y1 + ph + ext_h, 1.0f); break;
                case 2: stamp_rect(grid, w, h, x1 - ext_w, y1, x1, y1 + ext_h, 1.0f); break;
                case 3: stamp_rect(grid, w, h, x1 + pw, y1, x1 + pw + ext_w, y1 + ext_h, 1.0f); break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Strategy 3: moisture_river — blocky zigzag river with axis-aligned segments
// The river walks across the map making right-angle turns.
// ---------------------------------------------------------------------------
void moisture_river(float* grid, int w, int h,
                    std::mt19937& rng, const float* /*elevation*/,
                    const BiomeProfile& /*prof*/) {
    bool horizontal = (rng() % 10) < 7;
    int thickness = 2 + static_cast<int>(rng() % 3); // 2-4 tiles wide

    if (horizontal) {
        // River flows left to right with vertical jogs
        int y = h / 4 + static_cast<int>(rng() % (h / 2)); // start y
        int x = 0;
        int segment_min = 15, segment_max = 50;

        while (x < w) {
            // Horizontal segment
            int seg_len = segment_min + static_cast<int>(rng() % (segment_max - segment_min));
            int x_end = std::min(x + seg_len, w - 1);
            stamp_h_corridor(grid, w, h, x, x_end, y, thickness, 1.0f);
            x = x_end;

            if (x >= w - 1) break;

            // Vertical jog (right-angle turn)
            int jog = 3 + static_cast<int>(rng() % 10); // 3-12 tiles
            if (rng() % 2) jog = -jog;
            int new_y = std::clamp(y + jog, thickness, h - 1 - thickness);
            stamp_v_corridor(grid, w, h, x, y, new_y, thickness, 1.0f);
            y = new_y;
        }
    } else {
        // Vertical river flows top to bottom with horizontal jogs
        int x = w / 4 + static_cast<int>(rng() % (w / 2));
        int y = 0;
        int segment_min = 8, segment_max = 25;

        while (y < h) {
            int seg_len = segment_min + static_cast<int>(rng() % (segment_max - segment_min));
            int y_end = std::min(y + seg_len, h - 1);
            stamp_v_corridor(grid, w, h, x, y, y_end, thickness, 1.0f);
            y = y_end;

            if (y >= h - 1) break;

            int jog = 5 + static_cast<int>(rng() % 20);
            if (rng() % 2) jog = -jog;
            int new_x = std::clamp(x + jog, thickness, w - 1 - thickness);
            stamp_h_corridor(grid, w, h, x, new_x, y, thickness, 1.0f);
            x = new_x;
        }
    }
}

// ---------------------------------------------------------------------------
// Strategy 4: moisture_coastline — stepped shoreline (staircase edges)
// ---------------------------------------------------------------------------
void moisture_coastline(float* grid, int w, int h,
                        std::mt19937& rng, const float* /*elevation*/,
                        const BiomeProfile& /*prof*/) {
    int edge = static_cast<int>(rng() % 4); // 0=N, 1=S, 2=W, 3=E
    unsigned seed1 = static_cast<unsigned>(rng());

    // Base depth: 35-50% of perpendicular dimension
    bool vert = (edge >= 2);
    int perp = vert ? w : h;
    int base_depth = static_cast<int>(perp * 0.35f)
                   + static_cast<int>(rng() % static_cast<unsigned>(perp * 0.15f));

    // Generate stepped shoreline: divide the parallel axis into segments
    // Each segment has a constant depth (creating staircase pattern)
    int parallel = vert ? h : w;
    int step_width = 8 + static_cast<int>(rng() % 12); // 8-19 tiles per step

    std::vector<int> shore_depths;
    int current_depth = base_depth;
    for (int pos = 0; pos < parallel; pos += step_width) {
        shore_depths.push_back(current_depth);
        // Jog depth by a few tiles for next step
        int jog = static_cast<int>(rng() % 7) - 3; // -3 to +3
        current_depth = std::clamp(current_depth + jog, perp / 5, perp * 3 / 5);
        step_width = 8 + static_cast<int>(rng() % 12);
    }

    // Fill water based on edge direction and stepped shore
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int par_pos = vert ? y : x;
            int perp_pos;
            switch (edge) {
                case 0: perp_pos = y; break;             // north: depth from top
                case 1: perp_pos = (h - 1) - y; break;   // south: depth from bottom
                case 2: perp_pos = x; break;              // west: depth from left
                default: perp_pos = (w - 1) - x; break;  // east: depth from right
            }

            int seg_idx = par_pos / std::max(1, static_cast<int>(shore_depths.size() > 0
                ? (parallel / static_cast<int>(shore_depths.size())) : 1));
            seg_idx = std::clamp(seg_idx, 0, static_cast<int>(shore_depths.size()) - 1);
            int depth = shore_depths[seg_idx];

            if (perp_pos < depth) {
                grid[y * w + x] = 1.0f;
            }
        }
    }

    // Add a few rectangular tidal pools near the shore on land side
    int num_tidal = 2 + static_cast<int>(rng() % 3);
    for (int i = 0; i < num_tidal; ++i) {
        // Find land cell near shore
        for (int attempt = 0; attempt < 20; ++attempt) {
            int tx = static_cast<int>(rng() % static_cast<unsigned>(w));
            int ty = static_cast<int>(rng() % static_cast<unsigned>(h));
            if (grid[ty * w + tx] > 0.5f) continue; // already water
            // Check if near water
            bool near_water = false;
            for (int dy = -8; dy <= 8 && !near_water; dy += 4)
                for (int dx = -8; dx <= 8 && !near_water; dx += 4) {
                    int nx = tx + dx, ny = ty + dy;
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h && grid[ny * w + nx] > 0.5f)
                        near_water = true;
                }
            if (near_water) {
                int pw = 3 + static_cast<int>(rng() % 5);
                int ph = 2 + static_cast<int>(rng() % 4);
                stamp_rect(grid, w, h, tx, ty, tx + pw, ty + ph, 1.0f);
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Strategy 5: moisture_channels — grid-aligned branching channels
// Axis-aligned paths with right-angle branches. For lava rivers.
// ---------------------------------------------------------------------------
void moisture_channels(float* grid, int w, int h,
                       std::mt19937& rng, const float* /*elevation*/,
                       const BiomeProfile& /*prof*/) {
    int num_sources = 1 + static_cast<int>(rng() % 2); // 1-2
    int thickness = 1 + static_cast<int>(rng() % 2);   // 1-2 tiles

    for (int src = 0; src < num_sources; ++src) {
        // Start on a random edge
        int start_edge = static_cast<int>(rng() % 4);
        int cx, cy;
        bool moving_h; // true = currently moving horizontally

        if (start_edge == 0) { cy = 0; cx = static_cast<int>(rng() % w); moving_h = false; }
        else if (start_edge == 1) { cy = h - 1; cx = static_cast<int>(rng() % w); moving_h = false; }
        else if (start_edge == 2) { cx = 0; cy = static_cast<int>(rng() % h); moving_h = true; }
        else { cx = w - 1; cy = static_cast<int>(rng() % h); moving_h = true; }

        // Walk with alternating H/V segments and right-angle turns
        int segments = 8 + static_cast<int>(rng() % 8); // 8-15 segments
        for (int seg = 0; seg < segments; ++seg) {
            int seg_len = 10 + static_cast<int>(rng() % 30); // 10-39 tiles

            if (moving_h) {
                int dir = (rng() % 2) ? 1 : -1;
                int nx = std::clamp(cx + dir * seg_len, 0, w - 1);
                stamp_h_corridor(grid, w, h, cx, nx, cy, thickness, 1.0f);
                cx = nx;
            } else {
                int dir = (rng() % 2) ? 1 : -1;
                int ny = std::clamp(cy + dir * seg_len, 0, h - 1);
                stamp_v_corridor(grid, w, h, cx, cy, ny, thickness, 1.0f);
                cy = ny;
            }

            // Right-angle turn
            moving_h = !moving_h;

            // Occasionally spawn a branch
            if (rng() % 3 == 0) {
                int branch_len = 8 + static_cast<int>(rng() % 15);
                if (moving_h) {
                    // Branch goes horizontal from current position
                    int dir = (rng() % 2) ? 1 : -1;
                    int bx = std::clamp(cx + dir * branch_len, 0, w - 1);
                    stamp_h_corridor(grid, w, h, cx, bx, cy, std::max(1, thickness - 1), 1.0f);
                } else {
                    int dir = (rng() % 2) ? 1 : -1;
                    int by = std::clamp(cy + dir * branch_len, 0, h - 1);
                    stamp_v_corridor(grid, w, h, cx, cy, by, std::max(1, thickness - 1), 1.0f);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Strategy 6: moisture_marsh — low-frequency noise threshold
// Generate a dedicated low-frequency heightmap. Threshold it so the low
// basins become water (~45% coverage). The noise naturally produces large
// connected pools with irregular edges and channels at saddle points.
// ---------------------------------------------------------------------------
void moisture_marsh(float* grid, int w, int h,
                    std::mt19937& rng, const float* elevation,
                    const BiomeProfile& /*prof*/) {
    int size = w * h;
    unsigned seed1 = static_cast<unsigned>(rng());
    unsigned seed2 = static_cast<unsigned>(rng());

    // --- Step 1: Generate dedicated water heightmap ---
    // Low frequency (0.008) + 2 octaves = big smooth basins (~125 tile features)
    // Detail noise at higher frequency adds ragged edges
    std::vector<float> water_height(size);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float fx = static_cast<float>(x);
            float fy = static_cast<float>(y);

            // Primary: basins (~55 tile features)
            float large = fbm(fx, fy, seed1, 0.018f, 2);
            // Detail: edge irregularity
            float detail = fbm(fx, fy, seed2, 0.04f, 1);

            float val = large * 0.75f + detail * 0.25f;

            // Slight bias toward low-elevation areas
            val -= elevation[y * w + x] * 0.3f;

            water_height[y * w + x] = val;
        }
    }

    // --- Step 2: Find threshold for ~45% water coverage ---
    std::vector<float> sorted(water_height);
    size_t target_idx = static_cast<size_t>(0.45f * static_cast<float>(size));
    std::nth_element(sorted.begin(), sorted.begin() + target_idx, sorted.end());
    float threshold = sorted[target_idx];

    // --- Step 3: Apply threshold — below = water, above = land ---
    for (int i = 0; i < size; ++i) {
        grid[i] = (water_height[i] <= threshold) ? 1.0f : 0.0f;
    }
}

} // namespace astra
