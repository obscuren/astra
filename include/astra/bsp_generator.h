#pragma once

#include "astra/ruin_types.h"
#include "astra/tilemap.h"

#include <random>

namespace astra {

class BspGenerator {
public:
    void generate(TileMap& map, RuinPlan& plan, std::mt19937& rng) const;

private:
    void subdivide(std::vector<BspNode>& nodes, int node_idx,
                   int max_depth, const std::vector<std::pair<int,int>>& nuclei,
                   float regularity, std::mt19937& rng) const;

    void materialize_walls(TileMap& map, const RuinPlan& plan) const;

    void generate_noise_walls(TileMap& map, const RuinPlan& plan,
                              const std::vector<Rect>& nucleus_zones) const;

    int nucleus_depth(int base_depth, int x, int y,
                      const std::vector<std::pair<int,int>>& nuclei,
                      int radius) const;
};

} // namespace astra
