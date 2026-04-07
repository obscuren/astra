#include "astra/biome_profile.h"
#include "astra/noise.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace astra {

// ---------------------------------------------------------------------------
// Strategy 1: moisture_none — no-op, grid stays zero
// ---------------------------------------------------------------------------
void moisture_none(float* /*grid*/, int /*w*/, int /*h*/,
                   std::mt19937& /*rng*/, const float* /*elevation*/,
                   const BiomeProfile& /*prof*/) {}

// ---------------------------------------------------------------------------
// Strategy 2: moisture_pools — organic blob-shaped pools in low areas
// ---------------------------------------------------------------------------
void moisture_pools(float* grid, int w, int h,
                    std::mt19937& rng, const float* elevation,
                    const BiomeProfile& prof) {
    // Number of pool seed points
    int num_pools = 6 + static_cast<int>(rng() % 9); // 6-14

    struct Pool {
        float cx, cy;
        float radius;
        unsigned seed;
    };
    std::vector<Pool> pools;
    pools.reserve(num_pools);

    for (int i = 0; i < num_pools; ++i) {
        // Pick best of 5 random positions (lowest elevation)
        float best_x = 0, best_y = 0;
        float best_elev = 2.0f;
        for (int s = 0; s < 5; ++s) {
            int sx = static_cast<int>(rng() % static_cast<unsigned>(w));
            int sy = static_cast<int>(rng() % static_cast<unsigned>(h));
            float e = elevation[sy * w + sx];
            if (e < best_elev) {
                best_elev = e;
                best_x = static_cast<float>(sx);
                best_y = static_cast<float>(sy);
            }
        }
        float radius = 4.0f + static_cast<float>(rng() % 1300) / 100.0f; // 4-16
        unsigned pool_seed = static_cast<unsigned>(rng());
        pools.push_back({best_x, best_y, radius, pool_seed});
    }

    // For each cell, compute max contribution from all pools
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float max_val = 0.0f;
            for (auto& p : pools) {
                float dx = static_cast<float>(x) - p.cx;
                float dy = static_cast<float>(y) - p.cy;
                float dist = std::sqrt(dx * dx + dy * dy);
                float outer = p.radius * 1.8f;
                if (dist > outer) continue;

                float d = dist / p.radius; // normalized distance
                float falloff = std::max(0.0f, 1.0f - d);
                float noise_val = fbm(static_cast<float>(x),
                                      static_cast<float>(y),
                                      p.seed, prof.moisture_frequency, 3);
                float shaped = falloff + (noise_val - 0.5f) * 0.6f;
                shaped = std::clamp(shaped, 0.0f, 1.0f);
                max_val = std::max(max_val, shaped);
            }

            // Elevation gate: fade to zero above flood_level
            float elev = elevation[y * w + x];
            float gate = std::max(0.0f, 1.0f - (elev - prof.flood_level) * 5.0f);
            max_val *= gate;

            grid[y * w + x] = std::clamp(max_val, 0.0f, 1.0f);
        }
    }
}

// ---------------------------------------------------------------------------
// Strategy 3: moisture_river — single sinuous river band
// ---------------------------------------------------------------------------
void moisture_river(float* grid, int w, int h,
                    std::mt19937& rng, const float* /*elevation*/,
                    const BiomeProfile& prof) {
    bool horizontal = (rng() % 10) < 7;

    unsigned seed1 = static_cast<unsigned>(rng());
    unsigned seed2 = static_cast<unsigned>(rng());

    auto rand_float = [&](float lo, float hi) {
        return lo + static_cast<float>(rng() % 10000) / 10000.0f * (hi - lo);
    };

    if (horizontal) {
        float base_y = rand_float(h * 0.3f, h * 0.7f);
        float amplitude_a = rand_float(h * 0.08f, h * 0.15f);
        float freq_a = rand_float(0.015f, 0.03f);
        float amplitude_b = amplitude_a * 0.4f;
        float freq_b = freq_a * 2.3f;
        float phase_b = rand_float(0.0f, 6.28f);
        float base_width = rand_float(3.0f, 6.0f);

        for (int x = 0; x < w; ++x) {
            float fx = static_cast<float>(x);
            float center = base_y
                         + amplitude_a * std::sin(fx * freq_a)
                         + amplitude_b * std::sin(fx * freq_b + phase_b);
            center += (fbm(fx, base_y, seed1, prof.moisture_frequency, 2) - 0.5f) * 6.0f;

            float width = base_width + fbm(fx, 0.0f, seed2, 0.02f, 2) * 3.0f;
            float half_w = width * 0.5f;

            for (int y = 0; y < h; ++y) {
                float dist = std::abs(static_cast<float>(y) - center);
                float val = 0.0f;
                if (dist <= half_w) {
                    val = 1.0f;
                } else if (dist <= half_w + 2.0f) {
                    val = 1.0f - (dist - half_w) / 2.0f;
                }
                grid[y * w + x] = std::max(grid[y * w + x], val);
            }
        }
    } else {
        // Vertical river
        float base_x = rand_float(w * 0.3f, w * 0.7f);
        float amplitude_a = rand_float(w * 0.08f, w * 0.15f);
        float freq_a = rand_float(0.015f, 0.03f);
        float amplitude_b = amplitude_a * 0.4f;
        float freq_b = freq_a * 2.3f;
        float phase_b = rand_float(0.0f, 6.28f);
        float base_width = rand_float(3.0f, 6.0f);

        for (int y = 0; y < h; ++y) {
            float fy = static_cast<float>(y);
            float center = base_x
                         + amplitude_a * std::sin(fy * freq_a)
                         + amplitude_b * std::sin(fy * freq_b + phase_b);
            center += (fbm(base_x, fy, seed1, prof.moisture_frequency, 2) - 0.5f) * 6.0f;

            float width = base_width + fbm(0.0f, fy, seed2, 0.02f, 2) * 3.0f;
            float half_w = width * 0.5f;

            for (int x = 0; x < w; ++x) {
                float dist = std::abs(static_cast<float>(x) - center);
                float val = 0.0f;
                if (dist <= half_w) {
                    val = 1.0f;
                } else if (dist <= half_w + 2.0f) {
                    val = 1.0f - (dist - half_w) / 2.0f;
                }
                grid[y * w + x] = std::max(grid[y * w + x], val);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Strategy 4: moisture_coastline — large water body with irregular shore
// ---------------------------------------------------------------------------
void moisture_coastline(float* grid, int w, int h,
                        std::mt19937& rng, const float* /*elevation*/,
                        const BiomeProfile& prof) {
    int edge = static_cast<int>(rng() % 4); // 0=north,1=south,2=west,3=east
    unsigned seed1 = static_cast<unsigned>(rng());
    unsigned seed2 = static_cast<unsigned>(rng());

    auto rand_float = [&](float lo, float hi) {
        return lo + static_cast<float>(rng() % 10000) / 10000.0f * (hi - lo);
    };

    // Base depth: 35-50% of perpendicular dimension
    bool vertical_edge = (edge >= 2); // west or east
    int perp = vertical_edge ? w : h;
    float base_depth = rand_float(perp * 0.35f, perp * 0.50f);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float shore, cell_pos;

            if (edge == 0) { // north — water at top
                float fx = static_cast<float>(x);
                shore = base_depth
                      + fbm(fx, 0.0f, seed1, prof.moisture_frequency, 4) * (h * 0.12f)
                      + (ridge_noise(fx, 0.0f, seed2, 0.008f, 2) - 0.5f) * (h * 0.08f);
                cell_pos = static_cast<float>(y);
            } else if (edge == 1) { // south — water at bottom
                float fx = static_cast<float>(x);
                shore = base_depth
                      + fbm(fx, 0.0f, seed1, prof.moisture_frequency, 4) * (h * 0.12f)
                      + (ridge_noise(fx, 0.0f, seed2, 0.008f, 2) - 0.5f) * (h * 0.08f);
                cell_pos = static_cast<float>(h - 1 - y);
            } else if (edge == 2) { // west — water at left
                float fy = static_cast<float>(y);
                shore = base_depth
                      + fbm(0.0f, fy, seed1, prof.moisture_frequency, 4) * (w * 0.12f)
                      + (ridge_noise(0.0f, fy, seed2, 0.008f, 2) - 0.5f) * (w * 0.08f);
                cell_pos = static_cast<float>(x);
            } else { // east — water at right
                float fy = static_cast<float>(y);
                shore = base_depth
                      + fbm(0.0f, fy, seed1, prof.moisture_frequency, 4) * (w * 0.12f)
                      + (ridge_noise(0.0f, fy, seed2, 0.008f, 2) - 0.5f) * (w * 0.08f);
                cell_pos = static_cast<float>((vertical_edge ? w : h) - 1 - x);
            }

            float val;
            if (cell_pos < shore) {
                val = 1.0f; // water side
            } else if (cell_pos < shore + 4.0f) {
                val = 1.0f - (cell_pos - shore) / 4.0f; // gradient
            } else {
                val = 0.0f;
            }
            grid[y * w + x] = val;
        }
    }

    // Add 3-5 small tidal pools on land side, within 20 tiles of shore
    int num_tidal = 3 + static_cast<int>(rng() % 3);
    for (int i = 0; i < num_tidal; ++i) {
        unsigned pool_seed = static_cast<unsigned>(rng());
        float radius = rand_float(2.0f, 5.0f);

        // Find a position on land near shore
        // Try random positions until we find one with low-ish moisture (land side)
        float px = 0, py = 0;
        for (int attempt = 0; attempt < 20; ++attempt) {
            int tx = static_cast<int>(rng() % static_cast<unsigned>(w));
            int ty = static_cast<int>(rng() % static_cast<unsigned>(h));
            float m = grid[ty * w + tx];
            if (m < 0.1f && m >= 0.0f) {
                // Check proximity to shore (within 20 tiles of a wet cell)
                bool near_shore = false;
                for (int dy = -20; dy <= 20 && !near_shore; dy += 4) {
                    for (int dx = -20; dx <= 20 && !near_shore; dx += 4) {
                        int nx = tx + dx, ny = ty + dy;
                        if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                            if (grid[ny * w + nx] > 0.5f) near_shore = true;
                        }
                    }
                }
                if (near_shore) {
                    px = static_cast<float>(tx);
                    py = static_cast<float>(ty);
                    break;
                }
            }
        }

        // Stamp pool blob
        int r_ext = static_cast<int>(radius * 1.8f) + 1;
        int cx = static_cast<int>(px), cy = static_cast<int>(py);
        for (int dy = -r_ext; dy <= r_ext; ++dy) {
            for (int dx = -r_ext; dx <= r_ext; ++dx) {
                int nx = cx + dx, ny = cy + dy;
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                if (dist > radius * 1.8f) continue;
                float d = dist / radius;
                float falloff = std::max(0.0f, 1.0f - d);
                float nv = fbm(static_cast<float>(nx), static_cast<float>(ny),
                               pool_seed, prof.moisture_frequency, 3);
                float shaped = falloff + (nv - 0.5f) * 0.4f;
                shaped = std::clamp(shaped, 0.0f, 1.0f);
                grid[ny * w + nx] = std::max(grid[ny * w + nx], shaped);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Strategy 5: moisture_channels — branching channels (lava rivers, etc.)
// ---------------------------------------------------------------------------
void moisture_channels(float* grid, int w, int h,
                       std::mt19937& rng, const float* elevation,
                       const BiomeProfile& prof) {
    struct ChannelPoint {
        int x, y;
        int width; // half-width
    };
    std::vector<ChannelPoint> all_points;

    auto rand_float = [&](float lo, float hi) {
        return lo + static_cast<float>(rng() % 10000) / 10000.0f * (hi - lo);
    };

    int num_sources = 1 + static_cast<int>(rng() % 2); // 1-2

    for (int src = 0; src < num_sources; ++src) {
        unsigned seed_walk = static_cast<unsigned>(rng());

        // Start on a random edge
        int start_edge = static_cast<int>(rng() % 4);
        float cx, cy;
        int primary_dx = 0, primary_dy = 0;

        if (start_edge == 0) { // north
            cx = rand_float(0.0f, static_cast<float>(w));
            cy = 0.0f;
            primary_dy = 1;
        } else if (start_edge == 1) { // south
            cx = rand_float(0.0f, static_cast<float>(w));
            cy = static_cast<float>(h - 1);
            primary_dy = -1;
        } else if (start_edge == 2) { // west
            cx = 0.0f;
            cy = rand_float(0.0f, static_cast<float>(h));
            primary_dx = 1;
        } else { // east
            cx = static_cast<float>(w - 1);
            cy = rand_float(0.0f, static_cast<float>(h));
            primary_dx = -1;
        }

        // Random walk generating the primary channel
        auto walk_channel = [&](float sx, float sy, int pdx, int pdy,
                                int half_w, int max_steps, unsigned wseed,
                                bool can_branch) {
            float px = sx, py = sy;
            int steps_since_branch = 0;
            int next_branch = 20 + static_cast<int>(rng() % 21); // 20-40

            for (int step = 0; step < max_steps; ++step) {
                int ix = static_cast<int>(px);
                int iy = static_cast<int>(py);
                if (ix < 0 || ix >= w || iy < 0 || iy >= h) break;

                all_points.push_back({ix, iy, half_w});

                // Advance in primary direction
                float advance = 1.0f + static_cast<float>(rng() % 2);
                float npx = px + pdx * advance;
                float npy = py + pdy * advance;

                // Perpendicular offset from noise
                float noise_off = (fbm(static_cast<float>(step), 0.0f,
                                       wseed, 0.05f, 2) - 0.5f) * 3.0f;

                // Elevation bias: prefer going downhill
                int look_dist = 3;
                if (pdx != 0) {
                    // Moving horizontally: bias y based on elevation difference
                    int above_y = std::max(0, iy - look_dist);
                    int below_y = std::min(h - 1, iy + look_dist);
                    float e_above = elevation[above_y * w + ix];
                    float e_below = elevation[below_y * w + ix];
                    noise_off += (e_above - e_below) * 2.0f; // bias downhill
                } else {
                    // Moving vertically: bias x
                    int left_x = std::max(0, ix - look_dist);
                    int right_x = std::min(w - 1, ix + look_dist);
                    float e_left = elevation[iy * w + left_x];
                    float e_right = elevation[iy * w + right_x];
                    noise_off += (e_left - e_right) * 2.0f;
                }

                if (pdx != 0) {
                    npy += noise_off;
                } else {
                    npx += noise_off;
                }

                px = npx;
                py = npy;

                // Branching
                steps_since_branch++;
                if (can_branch && steps_since_branch >= next_branch) {
                    steps_since_branch = 0;
                    next_branch = 20 + static_cast<int>(rng() % 21);

                    // Branch direction: angled off primary
                    float angle_deg = rand_float(30.0f, 60.0f);
                    if (rng() % 2) angle_deg = -angle_deg;
                    float angle_rad = angle_deg * 3.14159f / 180.0f;

                    float bdx, bdy;
                    if (pdx != 0) {
                        bdx = static_cast<float>(pdx) * std::cos(angle_rad);
                        bdy = std::sin(angle_rad);
                    } else {
                        bdx = std::sin(angle_rad);
                        bdy = static_cast<float>(pdy) * std::cos(angle_rad);
                    }

                    // Normalize
                    float blen = std::sqrt(bdx * bdx + bdy * bdy);
                    if (blen > 0.0f) { bdx /= blen; bdy /= blen; }

                    int branch_len = 15 + static_cast<int>(rng() % 26); // 15-40
                    int branch_hw = std::max(0, half_w - 1); // narrower
                    unsigned bseed = static_cast<unsigned>(rng());

                    // Walk the branch
                    float bx = px, by = py;
                    for (int bs = 0; bs < branch_len; ++bs) {
                        int bix = static_cast<int>(bx);
                        int biy = static_cast<int>(by);
                        if (bix < 0 || bix >= w || biy < 0 || biy >= h) break;

                        all_points.push_back({bix, biy, branch_hw});

                        float bn = (fbm(static_cast<float>(bs), 0.0f,
                                        bseed, 0.05f, 2) - 0.5f) * 1.5f;
                        bx += bdx * 1.5f + (-bdy) * bn * 0.5f;
                        by += bdy * 1.5f + bdx * bn * 0.5f;

                        // Sub-branch with 30% chance every 15-25 steps
                        if (bs > 0 && bs % (15 + static_cast<int>(rng() % 11)) == 0
                            && (rng() % 100) < 30) {
                            float sa = rand_float(30.0f, 60.0f) * 3.14159f / 180.0f;
                            if (rng() % 2) sa = -sa;
                            float sdx = bdx * std::cos(sa) - bdy * std::sin(sa);
                            float sdy = bdx * std::sin(sa) + bdy * std::cos(sa);
                            float sbx = bx, sby = by;
                            int sub_len = 8 + static_cast<int>(rng() % 15);
                            unsigned sseed = static_cast<unsigned>(rng());
                            for (int ss = 0; ss < sub_len; ++ss) {
                                int six = static_cast<int>(sbx);
                                int siy = static_cast<int>(sby);
                                if (six < 0 || six >= w || siy < 0 || siy >= h) break;
                                all_points.push_back({six, siy, 0}); // 1-tile wide
                                float sn = (fbm(static_cast<float>(ss), 0.0f,
                                                sseed, 0.07f, 2) - 0.5f) * 1.0f;
                                sbx += sdx * 1.5f + (-sdy) * sn * 0.3f;
                                sby += sdy * 1.5f + sdx * sn * 0.3f;
                            }
                        }
                    }
                }
            }
        };

        int max_steps = std::max(w, h);
        walk_channel(cx, cy, primary_dx, primary_dy, 1, max_steps, seed_walk, true);
    }

    // Stamp channel points onto grid
    for (auto& pt : all_points) {
        int hw = pt.width;
        for (int dy = -hw - 1; dy <= hw + 1; ++dy) {
            for (int dx = -hw - 1; dx <= hw + 1; ++dx) {
                int nx = pt.x + dx, ny = pt.y + dy;
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                int adx = std::abs(dx), ady = std::abs(dy);
                int manhattan = adx + ady;
                float val;
                if (adx <= hw && ady <= hw) {
                    val = 1.0f; // core
                } else if (manhattan <= hw + 2) {
                    // Soft edge
                    float edge_dist = static_cast<float>(manhattan - hw);
                    val = 0.8f - edge_dist * 0.15f;
                    // Add noise perturbation for organic feel
                    float nv = fbm(static_cast<float>(nx), static_cast<float>(ny),
                                   42u, prof.moisture_frequency * 2.0f, 2);
                    val += (nv - 0.5f) * 0.2f;
                    val = std::clamp(val, 0.0f, 0.8f);
                } else {
                    continue;
                }
                grid[ny * w + nx] = std::max(grid[ny * w + nx], val);
            }
        }
    }
}

} // namespace astra
