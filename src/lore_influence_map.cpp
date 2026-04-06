#include "astra/lore_influence_map.h"

#include <algorithm>
#include <cmath>

namespace astra {

// ── Value noise + fBm (same formulas as overworld_generator) ──

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

// ── Splitmix-style deterministic RNG ──

static unsigned splitmix(unsigned& state) {
    state += 0x9E3779B9u;
    unsigned z = state;
    z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
    z = (z ^ (z >> 13)) * 0xC2B2AE35u;
    return z ^ (z >> 16);
}

static int rng_range(unsigned& state, int lo, int hi) {
    if (lo >= hi) return lo;
    return lo + static_cast<int>(splitmix(state) % static_cast<unsigned>(hi - lo + 1));
}

static float rng_float(unsigned& state) {
    return static_cast<float>(splitmix(state) & 0xFFFFu) / 65535.0f;
}

// ── Quick check: does this MapProperties have any lore features? ──

static bool has_lore_features(const MapProperties& props) {
    return props.lore_beacon
        || props.lore_megastructure
        || props.lore_terraformed
        || props.lore_battle_site
        || props.lore_weapon_test
        || props.lore_scar_count > 0;
}

// ── Generate ──

LoreInfluenceMap generate_lore_influence(
    const MapProperties& props,
    int map_width, int map_height,
    unsigned seed)
{
    if (!has_lore_features(props))
        return {};

    unsigned rng = seed ^ 0x4C0E'1F40u;
    const int total = map_width * map_height;

    LoreInfluenceMap map;
    map.width  = map_width;
    map.height = map_height;
    map.alien_strength.resize(total, 0.0f);
    map.scar_intensity.resize(total, 0.0f);
    map.landmark.resize(total, LandmarkType::None);

    // ── Pass 1: Landmarks ──
    // Place beacon and/or megastructure in suitable interior locations.

    auto place_landmark = [&](LandmarkType type) {
        const int r = world::landmark_zone_radius;
        // Pick a random interior location away from edges
        int margin = r + 4;
        int cx = rng_range(rng, margin, map_width  - margin - 1);
        int cy = rng_range(rng, margin, map_height - margin - 1);

        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                int nx = cx + dx;
                int ny = cy + dy;
                if (nx >= 0 && nx < map_width && ny >= 0 && ny < map_height) {
                    map.landmark[ny * map_width + nx] = type;
                }
            }
        }
    };

    if (props.lore_beacon)
        place_landmark(LandmarkType::Beacon);
    if (props.lore_megastructure)
        place_landmark(LandmarkType::Megastructure);

    // ── Pass 2: Alien biome patches (terraformed) ──
    // 2-5 radial fBm noise origins with ragged edges.

    if (props.lore_terraformed) {
        int num_origins = rng_range(rng, world::terraform_min_origins,
                                         world::terraform_max_origins);
        float target_coverage = world::terraform_min_coverage
            + rng_float(rng) * (world::terraform_max_coverage - world::terraform_min_coverage);

        // Generate origin points
        struct Origin { float x, y, radius; unsigned noise_seed; };
        std::vector<Origin> origins;
        origins.reserve(num_origins);

        for (int i = 0; i < num_origins; ++i) {
            Origin o;
            o.x = static_cast<float>(rng_range(rng, map_width / 6, map_width * 5 / 6));
            o.y = static_cast<float>(rng_range(rng, map_height / 6, map_height * 5 / 6));
            // Radius sized to approximate target coverage spread across origins
            float area_per_origin = target_coverage * map_width * map_height / num_origins;
            o.radius = std::sqrt(area_per_origin / 3.14159f);
            o.noise_seed = splitmix(rng);
            origins.push_back(o);
        }

        // For each cell, compute max contribution from all origins
        for (int y = 0; y < map_height; ++y) {
            for (int x = 0; x < map_width; ++x) {
                float best = 0.0f;
                for (const auto& o : origins) {
                    float dx = static_cast<float>(x) - o.x;
                    float dy = static_cast<float>(y) - o.y;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist > o.radius * 1.5f) continue;

                    // Radial falloff
                    float falloff = 1.0f - std::clamp(dist / o.radius, 0.0f, 1.0f);
                    // fBm noise for ragged edges
                    float noise = fbm(static_cast<float>(x), static_cast<float>(y),
                                      o.noise_seed, 0.08f, 3);
                    // Combine: falloff shapes the patch, noise roughs the edge
                    float strength = falloff * (0.5f + 0.5f * noise);
                    best = std::max(best, strength);
                }
                map.alien_strength[y * map_width + x] = std::clamp(best, 0.0f, 1.0f);
            }
        }
    }

    // ── Pass 3: Scar zones ──
    // Epicenters with radial falloff, additive overlap, clamped to 1.0.

    int scar_count = props.lore_scar_count;
    if (scar_count == 0 && (props.lore_battle_site || props.lore_weapon_test))
        scar_count = props.lore_battle_site ? 3 : 2;

    if (scar_count > 0) {
        for (int i = 0; i < scar_count; ++i) {
            // Epicenter
            int cx = rng_range(rng, world::max_scar_radius,
                               map_width - world::max_scar_radius - 1);
            int cy = rng_range(rng, world::max_scar_radius,
                               map_height - world::max_scar_radius - 1);

            // Radius scales: more scars = smaller individual radius
            float radius_frac = 1.0f - std::clamp(
                static_cast<float>(scar_count - 1) / 8.0f, 0.0f, 0.6f);
            int radius = world::min_scar_radius
                + static_cast<int>(radius_frac * (world::max_scar_radius - world::min_scar_radius));

            unsigned scar_seed = splitmix(rng);

            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    int nx = cx + dx;
                    int ny = cy + dy;
                    if (nx < 0 || nx >= map_width || ny < 0 || ny >= map_height)
                        continue;
                    // Skip landmark zones
                    if (map.landmark[ny * map_width + nx] != LandmarkType::None)
                        continue;

                    float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                    if (dist > static_cast<float>(radius)) continue;

                    float falloff = 1.0f - dist / static_cast<float>(radius);
                    // Add some noise variation
                    float noise = fbm(static_cast<float>(nx), static_cast<float>(ny),
                                      scar_seed, 0.1f, 2);
                    float intensity = falloff * falloff * (0.6f + 0.4f * noise);

                    // Additive overlap
                    float& cell = map.scar_intensity[ny * map_width + nx];
                    cell = std::clamp(cell + intensity, 0.0f, 1.0f);
                }
            }
        }
    }

    return map;
}

}  // namespace astra
