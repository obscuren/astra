#include "astra/biome_profile.h"
#include "astra/noise.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace astra {

// ---------------------------------------------------------------------------
// Strategy 1: structure_none — no-op, grid stays at StructureMask::None
// ---------------------------------------------------------------------------
void structure_none(StructureMask* /*grid*/, int /*w*/, int /*h*/,
                    std::mt19937& /*rng*/,
                    const float* /*elevation*/, const float* /*moisture*/,
                    const BiomeProfile& /*prof*/) {
    // Intentionally empty — defer everything to elevation + moisture.
}

// ---------------------------------------------------------------------------
// Strategy 2: structure_cliffs — diagonal cliff bands across the map
// ---------------------------------------------------------------------------
void structure_cliffs(StructureMask* grid, int w, int h,
                      std::mt19937& rng,
                      const float* /*elevation*/, const float* /*moisture*/,
                      const BiomeProfile& prof) {
    float intensity = prof.structure_intensity;
    int num_cliffs = 1 + static_cast<int>(intensity * 2.0f);
    num_cliffs = std::clamp(num_cliffs, 1, 3);

    std::uniform_int_distribution<int> dist_x(0, w - 1);
    std::uniform_int_distribution<int> dist_y(0, h - 1);
    std::uniform_int_distribution<int> dist_width(2, 4);
    std::uniform_int_distribution<int> dist_edge(0, 3); // top, right, bottom, left
    std::uniform_int_distribution<int> dist_jitter(-1, 1);

    for (int c = 0; c < num_cliffs; ++c) {
        int cliff_width = dist_width(rng);
        int half_w = cliff_width / 2;

        // Pick start edge and end on opposite or adjacent edge.
        int start_edge = dist_edge(rng);
        int end_edge = (start_edge + 2) % 4; // opposite edge

        // Compute start and end points on edges.
        // Edges: 0=top, 1=right, 2=bottom, 3=left
        int x1, y1, x2, y2;
        auto point_on_edge = [&](int edge, int& px, int& py) {
            switch (edge) {
                case 0: px = dist_x(rng); py = 0;     break;
                case 1: px = w - 1;       py = dist_y(rng); break;
                case 2: px = dist_x(rng); py = h - 1; break;
                case 3: px = 0;           py = dist_y(rng); break;
            }
        };
        point_on_edge(start_edge, x1, y1);
        point_on_edge(end_edge, x2, y2);

        // Walk from start to end using Bresenham-like stepping.
        int dx = std::abs(x2 - x1);
        int dy = std::abs(y2 - y1);
        int sx = (x1 < x2) ? 1 : -1;
        int sy = (y1 < y2) ? 1 : -1;
        int err = dx - dy;

        int cx = x1, cy = y1;
        int jitter_offset = 0;
        int step_count = 0;

        while (true) {
            // Apply jitter every 10-20 steps for irregular edges.
            if (step_count % 15 == 0) {
                jitter_offset += dist_jitter(rng);
                jitter_offset = std::clamp(jitter_offset, -2, 2);
            }

            // Determine perpendicular direction and stamp wall bar.
            // If line is more horizontal, perpendicular is vertical (and vice versa).
            bool mostly_horizontal = (dx >= dy);
            for (int offset = -half_w; offset <= half_w; ++offset) {
                int wx, wy;
                if (mostly_horizontal) {
                    wx = cx;
                    wy = cy + offset + jitter_offset;
                } else {
                    wx = cx + offset + jitter_offset;
                    wy = cy;
                }
                if (wx >= 0 && wx < w && wy >= 0 && wy < h) {
                    grid[wy * w + wx] = StructureMask::Wall;
                }
            }

            if (cx == x2 && cy == y2) break;

            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; cx += sx; }
            if (e2 <  dx) { err += dx; cy += sy; }
            ++step_count;
        }
    }
}

// ---------------------------------------------------------------------------
// Strategy 3: structure_islands — floor platforms in wet areas
// ---------------------------------------------------------------------------
void structure_islands(StructureMask* grid, int w, int h,
                       std::mt19937& rng,
                       const float* /*elevation*/, const float* moisture,
                       const BiomeProfile& prof) {
    float intensity = prof.structure_intensity;
    int num_islands = 6 + static_cast<int>(intensity * 6.0f);
    num_islands = std::clamp(num_islands, 6, 12);

    std::uniform_int_distribution<int> dist_x(0, w - 1);
    std::uniform_int_distribution<int> dist_y(0, h - 1);
    std::uniform_int_distribution<int> dist_w(8, 25);
    std::uniform_int_distribution<int> dist_h(5, 12);

    int placed = 0;
    int attempts = 0;
    const int max_attempts = num_islands * 20;

    while (placed < num_islands && attempts < max_attempts) {
        ++attempts;
        int px = dist_x(rng);
        int py = dist_y(rng);

        // Only place platforms where moisture indicates water.
        if (moisture[py * w + px] <= 0.5f) continue;

        int pw = dist_w(rng);
        int ph = dist_h(rng);

        // Clamp to map bounds.
        int x0 = std::max(0, px - pw / 2);
        int y0 = std::max(0, py - ph / 2);
        int x1 = std::min(w - 1, px + pw / 2);
        int y1 = std::min(h - 1, py + ph / 2);

        // Verify the center region is actually wet.
        bool wet_enough = true;
        int wet_count = 0, total_count = 0;
        for (int iy = y0; iy <= y1; ++iy) {
            for (int ix = x0; ix <= x1; ++ix) {
                ++total_count;
                if (moisture[iy * w + ix] > 0.5f) ++wet_count;
            }
        }
        if (total_count == 0 || wet_count < total_count / 2) continue;

        // Stamp floor with irregular edges — skip corners and random edge cells
        for (int iy = y0; iy <= y1; ++iy) {
            for (int ix = x0; ix <= x1; ++ix) {
                if (moisture[iy * w + ix] <= 0.5f) continue;

                // Cut corners: skip cells near rectangle corners
                int dx_edge = std::min(ix - x0, x1 - ix);
                int dy_edge = std::min(iy - y0, y1 - iy);
                int corner_dist = dx_edge + dy_edge;
                int corner_cut = 2 + static_cast<int>(rng() % 2); // 2-3 tile cut
                if (corner_dist < corner_cut) continue;

                // Random edge nibbles: skip ~30% of outermost cells
                if ((dx_edge == 0 || dy_edge == 0) && rng() % 100 < 30) continue;

                grid[iy * w + ix] = StructureMask::Floor;
            }
        }
        ++placed;
    }
}

// ---------------------------------------------------------------------------
// Strategy 4: structure_formations — angular wall clusters
// ---------------------------------------------------------------------------
void structure_formations(StructureMask* grid, int w, int h,
                          std::mt19937& rng,
                          const float* /*elevation*/, const float* /*moisture*/,
                          const BiomeProfile& prof) {
    float intensity = prof.structure_intensity;
    int num_formations = 4 + static_cast<int>(intensity * 4.0f);
    num_formations = std::clamp(num_formations, 4, 8);

    std::uniform_int_distribution<int> dist_x(0, w - 1);
    std::uniform_int_distribution<int> dist_y(0, h - 1);
    std::uniform_int_distribution<int> dist_rects(1, 3);
    std::uniform_int_distribution<int> dist_rw(2, 8);
    std::uniform_int_distribution<int> dist_rh(2, 6);
    std::uniform_int_distribution<int> dist_offset(-4, 4);

    struct Center { int x, y; };
    std::vector<Center> centers;
    centers.reserve(num_formations);

    int placed = 0;
    int attempts = 0;
    const int max_attempts = num_formations * 30;

    while (placed < num_formations && attempts < max_attempts) {
        ++attempts;
        int cx = dist_x(rng);
        int cy = dist_y(rng);

        // Enforce minimum spacing of 30 tiles between centers.
        bool too_close = false;
        for (auto& c : centers) {
            int dx = std::abs(cx - c.x);
            int dy = std::abs(cy - c.y);
            if (dx < 30 && dy < 30) { too_close = true; break; }
        }
        if (too_close) continue;

        centers.push_back({cx, cy});

        // Stamp 1-3 overlapping rectangles.
        int num_rects = dist_rects(rng);
        for (int r = 0; r < num_rects; ++r) {
            int rw = dist_rw(rng);
            int rh = dist_rh(rng);
            int ox = (r == 0) ? 0 : dist_offset(rng);
            int oy = (r == 0) ? 0 : dist_offset(rng);

            int x0 = std::max(0, cx + ox - rw / 2);
            int y0 = std::max(0, cy + oy - rh / 2);
            int x1 = std::min(w - 1, cx + ox + rw / 2);
            int y1 = std::min(h - 1, cy + oy + rh / 2);

            for (int iy = y0; iy <= y1; ++iy) {
                for (int ix = x0; ix <= x1; ++ix) {
                    grid[iy * w + ix] = StructureMask::Wall;
                }
            }
        }
        ++placed;
    }
}

// ---------------------------------------------------------------------------
// Strategy 5: structure_craters — circular crater rims with open interiors
// ---------------------------------------------------------------------------
void structure_craters(StructureMask* grid, int w, int h,
                       std::mt19937& rng,
                       const float* /*elevation*/, const float* /*moisture*/,
                       const BiomeProfile& prof) {
    float intensity = prof.structure_intensity;
    int num_craters = 2 + static_cast<int>(intensity * 2.0f);
    num_craters = std::clamp(num_craters, 2, 4);

    std::uniform_int_distribution<int> dist_x(20, w - 21);
    std::uniform_int_distribution<int> dist_y(20, h - 21);
    std::uniform_int_distribution<int> dist_radius(8, 18);
    std::uniform_int_distribution<int> dist_rim(2, 4);

    struct Center { int x, y; };
    std::vector<Center> centers;
    centers.reserve(num_craters);

    int placed = 0;
    int attempts = 0;
    const int max_attempts = num_craters * 20;

    while (placed < num_craters && attempts < max_attempts) {
        ++attempts;
        int cx = dist_x(rng);
        int cy = dist_y(rng);

        // Ensure craters don't overlap too much — min spacing of 40 tiles.
        bool too_close = false;
        for (auto& c : centers) {
            int dx = std::abs(cx - c.x);
            int dy = std::abs(cy - c.y);
            if (dx < 40 && dy < 40) { too_close = true; break; }
        }
        if (too_close) continue;

        centers.push_back({cx, cy});

        int radius = dist_radius(rng);
        int rim_width = dist_rim(rng);

        // Stamp the crater using Euclidean distance (int-truncated for tile look)
        int scan = radius + 2;
        for (int dy = -scan; dy <= scan; ++dy) {
            for (int dx = -scan; dx <= scan; ++dx) {
                int px = cx + dx;
                int py = cy + dy;
                if (px < 0 || px >= w || py < 0 || py >= h) continue;

                int d2 = dx * dx + dy * dy;
                int inner2 = (radius - rim_width) * (radius - rim_width);
                int outer2 = radius * radius;
                if (d2 >= inner2 && d2 <= outer2) {
                    grid[py * w + px] = StructureMask::Wall; // rim
                } else if (d2 < inner2) {
                    grid[py * w + px] = StructureMask::Water; // lava-filled interior
                }
            }
        }
        ++placed;
    }
}

// ---------------------------------------------------------------------------
// Strategy 6: structure_mountains — neighbor-driven mountain variants
// ---------------------------------------------------------------------------
void structure_mountains(StructureMask* grid, int w, int h,
                         std::mt19937& rng,
                         const float* elevation, const float* /*moisture*/,
                         const BiomeProfile& prof) {
    int neighbors = prof.mountain_neighbor_count;

    // --- Cellular automata mountain generation ---
    // All variants use cave-automata: random fill → smoothing passes.
    // Neighbor count controls initial wall density (more neighbors = more open).
    //
    // Passes (0-1):  55% initial fill → dense mountains with winding valleys
    // Gradient (2):  45% initial fill → moderate mountain coverage
    // Plateau (3-4): 35% initial fill → open highland with scattered massifs

    int fill_pct;
    int smooth_passes;
    if (neighbors <= 1) {
        fill_pct = 55;
        smooth_passes = 5;
    } else if (neighbors == 2) {
        fill_pct = 45;
        smooth_passes = 5;
    } else {
        fill_pct = 35;
        smooth_passes = 4;
    }

    // Step 1: Random fill
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // Map edges are always wall (keeps terrain contained)
            if (x == 0 || x == w - 1 || y == 0 || y == h - 1) {
                grid[y * w + x] = StructureMask::Wall;
            } else {
                grid[y * w + x] = (static_cast<int>(rng() % 100) < fill_pct)
                    ? StructureMask::Wall : StructureMask::None;
            }
        }
    }

    // Step 2: Cellular automata smoothing
    // Rule: cell becomes wall if 5+ of its 8 neighbors are wall (B5678/S45678 variant)
    std::vector<StructureMask> buf(w * h);
    for (int pass = 0; pass < smooth_passes; ++pass) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (x == 0 || x == w - 1 || y == 0 || y == h - 1) {
                    buf[y * w + x] = StructureMask::Wall;
                    continue;
                }
                // Count wall neighbors (8-connected)
                int walls = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        if (grid[(y + dy) * w + (x + dx)] == StructureMask::Wall)
                            ++walls;
                    }
                }
                // Birth at 5+, survive at 4+
                if (grid[y * w + x] == StructureMask::Wall) {
                    buf[y * w + x] = (walls >= 4) ? StructureMask::Wall : StructureMask::None;
                } else {
                    buf[y * w + x] = (walls >= 5) ? StructureMask::Wall : StructureMask::None;
                }
            }
        }
        std::copy(buf.begin(), buf.end(), grid);
    }

    // Step 3: Open the edges so terrain doesn't feel boxed in
    // Clear the border walls we forced (let the compositor/bleed handle edges)
    for (int x = 0; x < w; ++x) {
        grid[x] = StructureMask::None;             // top row
        grid[(h - 1) * w + x] = StructureMask::None; // bottom row
    }
    for (int y = 0; y < h; ++y) {
        grid[y * w] = StructureMask::None;             // left col
        grid[y * w + (w - 1)] = StructureMask::None;   // right col
    }
}

} // namespace astra
