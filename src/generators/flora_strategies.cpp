#include "astra/biome_profile.h"
#include "astra/noise.h"

namespace astra {

// ---------------------------------------------------------------------------
// Helper
// ---------------------------------------------------------------------------

static void place_flora(TileMap& map, int x, int y, FixtureType type) {
    if (x < 0 || x >= map.width() || y < 0 || y >= map.height()) return;
    if (map.get(x, y) != Tile::Floor) return;
    if (map.fixture_id(x, y) >= 0) return;
    FixtureData fd;
    fd.type = type;
    fd.passable = true;
    fd.interactable = false;
    fd.blocks_vision = false;
    map.add_fixture(x, y, fd);
}

// ---------------------------------------------------------------------------
// Grassland — flower patches (sparse), grass sweeps, sparse herbs
// ---------------------------------------------------------------------------

void flora_grassland(TileMap& map, int w, int h, std::mt19937& rng,
                     const float* /*elevation*/, const float* /*moisture*/,
                     const BiomeProfile& /*prof*/) {
    unsigned seed1 = rng();
    unsigned seed2 = rng();

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // Flowers: small sparse patches — noise must be high AND random roll
            float fn = fbm(static_cast<float>(x), static_cast<float>(y), seed1, 0.12f);
            if (fn > 0.68f && rng() % 100 < 40) {
                place_flora(map, x, y, FixtureType::FloraFlower);
                continue;
            }
            // Tall grass: broader sweeps but sparser
            float gn = fbm(static_cast<float>(x), static_cast<float>(y), seed2, 0.04f);
            if (gn > 0.58f && rng() % 100 < 25) {
                place_flora(map, x, y, FixtureType::FloraGrass);
                continue;
            }
            // Herbs: very sparse random
            if (rng() % 100 < 1) {
                place_flora(map, x, y, FixtureType::FloraHerb);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Forest — mushrooms near trees (sparser), fern patches, sparse grass
// ---------------------------------------------------------------------------

void flora_forest(TileMap& map, int w, int h, std::mt19937& rng,
                  const float* /*elevation*/, const float* /*moisture*/,
                  const BiomeProfile& /*prof*/) {
    unsigned seed1 = rng();

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (map.get(x, y) != Tile::Floor) continue;
            if (map.fixture_id(x, y) >= 0) continue;

            // Mushroom: near trees, 12% chance (was 30% — too dense)
            bool near_tree = false;
            for (int dy2 = -2; dy2 <= 2 && !near_tree; ++dy2) {
                for (int dx2 = -2; dx2 <= 2 && !near_tree; ++dx2) {
                    int nx = x + dx2, ny = y + dy2;
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                    int fid = map.fixture_id(nx, ny);
                    if (fid >= 0 && map.fixture(fid).type == FixtureType::NaturalObstacle)
                        near_tree = true;
                }
            }
            if (near_tree && rng() % 100 < 12) {
                place_flora(map, x, y, FixtureType::FloraMushroom);
                continue;
            }

            // Herbs: noise patches but sparser
            float hn = fbm(static_cast<float>(x), static_cast<float>(y), seed1, 0.06f);
            if (hn > 0.65f && rng() % 100 < 30) {
                place_flora(map, x, y, FixtureType::FloraHerb);
                continue;
            }

            if (rng() % 100 < 2) {
                place_flora(map, x, y, FixtureType::FloraGrass);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Jungle — moderate flowers, grass, sparse herbs
// ---------------------------------------------------------------------------

void flora_jungle(TileMap& map, int w, int h, std::mt19937& rng,
                  const float* /*elevation*/, const float* /*moisture*/,
                  const BiomeProfile& /*prof*/) {
    unsigned seed1 = rng();

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float fn = fbm(static_cast<float>(x), static_cast<float>(y), seed1, 0.09f);
            if (fn > 0.62f && rng() % 100 < 35) {
                place_flora(map, x, y, FixtureType::FloraFlower);
                continue;
            }
            if (rng() % 100 < 5) {
                place_flora(map, x, y, FixtureType::FloraGrass);
                continue;
            }
            if (rng() % 100 < 1) {
                place_flora(map, x, y, FixtureType::FloraHerb);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Rocky — ore veins, sparse lichen
// ---------------------------------------------------------------------------

void flora_rocky(TileMap& map, int w, int h, std::mt19937& rng,
                 const float* /*elevation*/, const float* /*moisture*/,
                 const BiomeProfile& /*prof*/) {
    unsigned seed1 = rng();

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (fbm(static_cast<float>(x), static_cast<float>(y), seed1, 0.1f) > 0.65f) {
                place_flora(map, x, y, FixtureType::MineralOre);
                continue;
            }
            if (rng() % 100 < 3) {
                place_flora(map, x, y, FixtureType::FloraLichen);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Volcanic — ore at high elevation, rare scrap
// ---------------------------------------------------------------------------

void flora_volcanic(TileMap& map, int w, int h, std::mt19937& rng,
                    const float* elevation, const float* /*moisture*/,
                    const BiomeProfile& /*prof*/) {
    unsigned seed1 = rng();

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (fbm(static_cast<float>(x), static_cast<float>(y), seed1, 0.08f) > 0.6f
                && elevation[y * w + x] > 0.5f) {
                place_flora(map, x, y, FixtureType::MineralOre);
                continue;
            }
            if (rng() % 100 < 1) {
                place_flora(map, x, y, FixtureType::ScrapComponent);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Fungal — moderate mushroom patches with variety, sparse herbs
// ---------------------------------------------------------------------------

void flora_fungal(TileMap& map, int w, int h, std::mt19937& rng,
                  const float* /*elevation*/, const float* /*moisture*/,
                  const BiomeProfile& /*prof*/) {
    unsigned seed1 = rng();
    unsigned seed2 = rng();

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // Mushroom patches — higher threshold, random gating for variety
            float fn = fbm(static_cast<float>(x), static_cast<float>(y), seed1, 0.05f);
            if (fn > 0.6f && rng() % 100 < 35) {
                place_flora(map, x, y, FixtureType::FloraMushroom);
                continue;
            }
            // Second layer with different noise for more varied distribution
            float fn2 = fbm(static_cast<float>(x), static_cast<float>(y), seed2, 0.1f);
            if (fn2 > 0.7f && rng() % 100 < 20) {
                place_flora(map, x, y, FixtureType::FloraMushroom);
                continue;
            }
            // Sparse herbs between patches
            if (rng() % 100 < 2) {
                place_flora(map, x, y, FixtureType::FloraHerb);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Ice — crystal veins, sparse lichen
// ---------------------------------------------------------------------------

void flora_ice(TileMap& map, int w, int h, std::mt19937& rng,
               const float* /*elevation*/, const float* /*moisture*/,
               const BiomeProfile& /*prof*/) {
    // Sparse lichen patches across the tundra. No crystals — this is ice, not a gemstone field.
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (rng() % 100 < 3) {
                place_flora(map, x, y, FixtureType::FloraLichen);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Marsh — moisture-driven grass, herbs, sparse flowers
// ---------------------------------------------------------------------------

void flora_marsh(TileMap& map, int w, int h, std::mt19937& rng,
                 const float* /*elevation*/, const float* moisture,
                 const BiomeProfile& /*prof*/) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (moisture[y * w + x] > 0.3f && rng() % 100 < 10) {
                place_flora(map, x, y, FixtureType::FloraGrass);
                continue;
            }
            if (rng() % 100 < 3) {
                place_flora(map, x, y, FixtureType::FloraHerb);
                continue;
            }
            if (rng() % 100 < 1) {
                place_flora(map, x, y, FixtureType::FloraFlower);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Crystal — crystal patches
// ---------------------------------------------------------------------------

void flora_crystal(TileMap& map, int w, int h, std::mt19937& rng,
                   const float* /*elevation*/, const float* /*moisture*/,
                   const BiomeProfile& /*prof*/) {
    unsigned seed1 = rng();

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (fbm(static_cast<float>(x), static_cast<float>(y), seed1, 0.07f) > 0.55f
                && rng() % 100 < 40) {
                place_flora(map, x, y, FixtureType::MineralCrystal);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Corroded — scrap spread out (lower frequency), rare ore
// ---------------------------------------------------------------------------

void flora_corroded(TileMap& map, int w, int h, std::mt19937& rng,
                    const float* /*elevation*/, const float* /*moisture*/,
                    const BiomeProfile& /*prof*/) {
    unsigned seed1 = rng();

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // Lower frequency for bigger spread, random gate to avoid density
            float fn = fbm(static_cast<float>(x), static_cast<float>(y), seed1, 0.04f);
            if (fn > 0.55f && rng() % 100 < 25) {
                place_flora(map, x, y, FixtureType::ScrapComponent);
                continue;
            }
            if (rng() % 100 < 2) {
                place_flora(map, x, y, FixtureType::MineralOre);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Sandy — very sparse grass, rare ore
// ---------------------------------------------------------------------------

void flora_sandy(TileMap& map, int w, int h, std::mt19937& rng,
                 const float* /*elevation*/, const float* /*moisture*/,
                 const BiomeProfile& /*prof*/) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (rng() % 100 < 1) {
                place_flora(map, x, y, FixtureType::FloraGrass);
                continue;
            }
            if (rng() % 100 < 2) {
                place_flora(map, x, y, FixtureType::MineralOre);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Scarred — scrap spread, rare ore
// ---------------------------------------------------------------------------

void flora_scarred(TileMap& map, int w, int h, std::mt19937& rng,
                   const float* /*elevation*/, const float* /*moisture*/,
                   const BiomeProfile& /*prof*/) {
    unsigned seed1 = rng();

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float fn = fbm(static_cast<float>(x), static_cast<float>(y), seed1, 0.05f);
            if (fn > 0.55f && rng() % 100 < 30) {
                place_flora(map, x, y, FixtureType::ScrapComponent);
                continue;
            }
            if (rng() % 100 < 1) {
                place_flora(map, x, y, FixtureType::MineralOre);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Aquatic — moisture-driven grass, rare crystal
// ---------------------------------------------------------------------------

void flora_aquatic(TileMap& map, int w, int h, std::mt19937& rng,
                   const float* /*elevation*/, const float* moisture,
                   const BiomeProfile& /*prof*/) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (moisture[y * w + x] > 0.2f && rng() % 100 < 5) {
                place_flora(map, x, y, FixtureType::FloraGrass);
                continue;
            }
            if (rng() % 100 < 1) {
                place_flora(map, x, y, FixtureType::MineralCrystal);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Alien — crystals + mushrooms, moderate density
// ---------------------------------------------------------------------------

void flora_alien(TileMap& map, int w, int h, std::mt19937& rng,
                 const float* /*elevation*/, const float* /*moisture*/,
                 const BiomeProfile& /*prof*/) {
    unsigned seed1 = rng();
    unsigned seed2 = rng();

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float fn = fbm(static_cast<float>(x), static_cast<float>(y), seed1, 0.06f);
            if (fn > 0.58f && rng() % 100 < 30) {
                place_flora(map, x, y, FixtureType::MineralCrystal);
                continue;
            }
            float fn2 = fbm(static_cast<float>(x), static_cast<float>(y), seed2, 0.08f);
            if (fn2 > 0.6f && rng() % 100 < 25) {
                place_flora(map, x, y, FixtureType::FloraMushroom);
            }
        }
    }
}

} // namespace astra
