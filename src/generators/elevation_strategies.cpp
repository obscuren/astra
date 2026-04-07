#include "astra/biome_profile.h"
#include "astra/noise.h"

#include <cmath>

namespace astra {

void elevation_gentle(float* grid, int w, int h,
                      std::mt19937& rng, const BiomeProfile& prof) {
    unsigned seed = rng();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float val = fbm(static_cast<float>(x), static_cast<float>(y),
                            seed, prof.elevation_frequency, prof.elevation_octaves);
            grid[y * w + x] = val * 0.7f;
        }
    }
}

void elevation_rugged(float* grid, int w, int h,
                      std::mt19937& rng, const BiomeProfile& prof) {
    unsigned seed  = rng();
    unsigned seed2 = rng();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float fx = static_cast<float>(x);
            float fy = static_cast<float>(y);
            float val    = fbm(fx, fy, seed,  prof.elevation_frequency, prof.elevation_octaves);
            float detail = fbm(fx, fy, seed2, prof.elevation_frequency * 2.0f, 2);
            val = val * 0.7f + detail * 0.3f;
            val = std::pow(val, 0.8f);
            grid[y * w + x] = val;
        }
    }
}

void elevation_flat(float* grid, int w, int h,
                    std::mt19937& rng, const BiomeProfile& prof) {
    unsigned seed = rng();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float val = fbm(static_cast<float>(x), static_cast<float>(y),
                            seed, prof.elevation_frequency, prof.elevation_octaves);
            grid[y * w + x] = val * 0.15f;
        }
    }
}

void elevation_ridgeline(float* grid, int w, int h,
                         std::mt19937& rng, const BiomeProfile& prof) {
    unsigned seed = rng();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float val = ridge_noise(static_cast<float>(x), static_cast<float>(y),
                                    seed, prof.elevation_frequency, prof.elevation_octaves);
            grid[y * w + x] = val;
        }
    }
}

} // namespace astra
