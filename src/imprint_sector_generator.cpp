#include "astra/imprint_sector_generator.h"

#include <algorithm>
#include <random>

namespace astra {

GridSector gen_imprint_sector(const ImprintGenInput& in) {
    std::mt19937 rng(in.seed != 0 ? in.seed : 1u);

    // Interior size: 4..8 in each dimension.
    // npc_threat_tier (1..3) biases toward larger sectors at higher threat.
    int tier_bias  = std::clamp(in.npc_threat_tier - 1, 0, 2);  // 0, 1, or 2
    int interior_w = 4 + tier_bias + static_cast<int>(rng() % (5 - tier_bias));  // 4..8
    int interior_h = 4 + tier_bias + static_cast<int>(rng() % (5 - tier_bias));  // 4..8

    int w = interior_w + 2;  // +2 for Firewall border
    int h = interior_h + 2;

    GridSector sec;
    sec.w = w;
    sec.h = h;
    sec.tiles.assign(static_cast<size_t>(w) * h, GridTile::Firewall);

    // Carve interior floor.
    for (int y = 1; y <= interior_h; ++y) {
        for (int x = 1; x <= interior_w; ++x) {
            sec.tiles[static_cast<size_t>(y) * w + x] = GridTile::Floor;
        }
    }

    // Reward count distribution: ~60% empty, ~30% 1 reward, ~10% 2 rewards.
    int roll     = static_cast<int>(rng() % 100);
    int n_rewards = (roll < 60) ? 0 : (roll < 90) ? 1 : 2;

    // Place reward tiles, avoiding cells already occupied.
    int placed   = 0;
    int attempts = 0;
    while (placed < n_rewards && attempts < 64) {
        int rx = 1 + static_cast<int>(rng() % interior_w);
        int ry = 1 + static_cast<int>(rng() % interior_h);
        if (sec.at(rx, ry) != GridTile::Floor) { ++attempts; continue; }

        // Reward type weights (sum 100):
        //   DataNode      65%  — lore fragments, credit stashes, Sigil drops (reuse glyph for now)
        //   EncryptedFile 35%  — schematics / intel (rarer)
        // Spec 2 will introduce dedicated Cache / Schematic / Sigil tile types.
        int t = static_cast<int>(rng() % 100);
        GridTile reward = (t < 65) ? GridTile::DataNode : GridTile::EncryptedFile;
        sec.set(rx, ry, reward);
        ++placed;
        ++attempts;
    }

    // Spawn at center of interior.
    sec.spawn_x = w / 2;
    sec.spawn_y = h / 2;

    return sec;
}

}  // namespace astra
