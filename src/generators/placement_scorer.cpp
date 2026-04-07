#include "astra/placement_scorer.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace astra {

namespace {

// Compute mean and variance of elevation within a footprint region.
struct ElevStats {
    float mean     = 0.0f;
    float variance = 0.0f;
};

ElevStats elevation_stats(const TerrainChannels& ch, int rx, int ry, int rw, int rh) {
    float sum  = 0.0f;
    float sum2 = 0.0f;
    int count  = 0;
    for (int y = ry; y < ry + rh; ++y) {
        for (int x = rx; x < rx + rw; ++x) {
            float e = ch.elev(x, y);
            sum  += e;
            sum2 += e * e;
            ++count;
        }
    }
    if (count == 0) return {};
    float mean = sum / static_cast<float>(count);
    float var  = (sum2 / static_cast<float>(count)) - (mean * mean);
    if (var < 0.0f) var = 0.0f; // numerical safety
    return {mean, var};
}

// Count wall-mask tiles within a footprint.
float wall_ratio(const TerrainChannels& ch, int rx, int ry, int rw, int rh) {
    int walls = 0;
    int total = rw * rh;
    for (int y = ry; y < ry + rh; ++y) {
        for (int x = rx; x < rx + rw; ++x) {
            if (ch.struc(x, y) == StructureMask::Wall) ++walls;
        }
    }
    return (total > 0) ? static_cast<float>(walls) / static_cast<float>(total) : 1.0f;
}

// Count water tiles (from TileMap) within a slightly expanded region around the footprint.
int water_nearby(const TileMap& map, int rx, int ry, int rw, int rh, int search_expand = 6) {
    int count = 0;
    int x0 = std::max(0, rx - search_expand);
    int y0 = std::max(0, ry - search_expand);
    int x1 = std::min(map.width(),  rx + rw + search_expand);
    int y1 = std::min(map.height(), ry + rh + search_expand);
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            if (map.get(x, y) == Tile::Water) ++count;
        }
    }
    return count;
}

// Count water tiles strictly inside the footprint (disqualifying placement on water).
int water_inside(const TileMap& map, int rx, int ry, int rw, int rh) {
    int count = 0;
    for (int y = ry; y < ry + rh; ++y) {
        for (int x = rx; x < rx + rw; ++x) {
            if (map.get(x, y) == Tile::Water) ++count;
        }
    }
    return count;
}

// Find the nearest water-edge tile adjacent to the footprint (on land side).
// Returns {-1,-1} if none found.
std::pair<int,int> find_waterfront_anchor(const TileMap& map, int rx, int ry, int rw, int rh,
                                           int search_expand = 8) {
    int best_x = -1, best_y = -1;
    int best_dist2 = std::numeric_limits<int>::max();
    int cx = rx + rw / 2;
    int cy = ry + rh / 2;

    int x0 = std::max(0, rx - search_expand);
    int y0 = std::max(0, ry - search_expand);
    int x1 = std::min(map.width(),  rx + rw + search_expand);
    int y1 = std::min(map.height(), ry + rh + search_expand);

    static const int dx[] = {-1, 1, 0, 0};
    static const int dy[] = {0, 0, -1, 1};

    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            // We want a land tile adjacent to water
            if (map.get(x, y) == Tile::Water) continue;
            bool adj_water = false;
            for (int d = 0; d < 4; ++d) {
                int nx = x + dx[d];
                int ny = y + dy[d];
                if (nx >= 0 && nx < map.width() && ny >= 0 && ny < map.height()) {
                    if (map.get(nx, ny) == Tile::Water) { adj_water = true; break; }
                }
            }
            if (!adj_water) continue;
            int dist2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
            if (dist2 < best_dist2) {
                best_dist2 = dist2;
                best_x = x;
                best_y = y;
            }
        }
    }
    return {best_x, best_y};
}

// Find highest elevation point in footprint.
std::pair<int,int> find_elevated_point(const TerrainChannels& ch, int rx, int ry, int rw, int rh) {
    float best_e = -1.0f;
    int best_x = rx + rw / 2;
    int best_y = ry + rh / 2;
    for (int y = ry; y < ry + rh; ++y) {
        for (int x = rx; x < rx + rw; ++x) {
            float e = ch.elev(x, y);
            if (e > best_e) {
                best_e = e;
                best_x = x;
                best_y = y;
            }
        }
    }
    return {best_x, best_y};
}

} // anonymous namespace

PlacementResult PlacementScorer::score(const TerrainChannels& channels,
                                        const TileMap& map,
                                        int footprint_w, int footprint_h,
                                        int edge_margin) const {
    PlacementResult result;
    result.valid = false;

    int map_w = channels.width;
    int map_h = channels.height;

    // Ensure footprint fits within margin bounds
    if (footprint_w + 2 * edge_margin > map_w ||
        footprint_h + 2 * edge_margin > map_h) {
        return result;
    }

    float best_score = -std::numeric_limits<float>::max();
    int best_x = -1, best_y = -1;

    int step = 5;
    int x_max = map_w - edge_margin - footprint_w;
    int y_max = map_h - edge_margin - footprint_h;

    // Map center for center bonus
    float center_x = static_cast<float>(map_w) / 2.0f;
    float center_y = static_cast<float>(map_h) / 2.0f;
    float max_dist = std::sqrt(center_x * center_x + center_y * center_y);

    for (int cy = edge_margin; cy <= y_max; cy += step) {
        for (int cx = edge_margin; cx <= x_max; cx += step) {
            // Hard constraint: wall ratio
            float wr = wall_ratio(channels, cx, cy, footprint_w, footprint_h);
            if (wr > 0.15f) continue;

            // Hard constraint: too much water inside footprint
            int wi = water_inside(map, cx, cy, footprint_w, footprint_h);
            float water_inside_ratio = static_cast<float>(wi) /
                                       static_cast<float>(footprint_w * footprint_h);
            if (water_inside_ratio > 0.10f) continue;

            // Flatness (low variance = good)
            ElevStats es = elevation_stats(channels, cx, cy, footprint_w, footprint_h);
            float flatness = 1.0f - std::min(es.variance * 10.0f, 1.0f);

            // Water proximity bonus (nearby but not on)
            int wn = water_nearby(map, cx, cy, footprint_w, footprint_h);
            float water_bonus = std::min(static_cast<float>(wn) / 100.0f, 1.0f);

            // Center bonus — prefer locations closer to map center
            float fx = cx + footprint_w / 2.0f;
            float fy = cy + footprint_h / 2.0f;
            float dist = std::sqrt((fx - center_x) * (fx - center_x) +
                                   (fy - center_y) * (fy - center_y));
            float center_bonus = 1.0f - (dist / max_dist);

            // Wall penalty
            float wall_penalty = wr * 5.0f;

            float s = flatness * 3.0f + water_bonus + center_bonus - wall_penalty;

            if (s > best_score) {
                best_score = s;
                best_x = cx;
                best_y = cy;
            }
        }
    }

    if (best_x < 0) return result;

    // Build result
    result.footprint = Rect{best_x, best_y, footprint_w, footprint_h};
    result.valid = true;

    // --- Anchor discovery ---

    // Center anchor (always present)
    Anchor center_anchor;
    center_anchor.x = best_x + footprint_w / 2;
    center_anchor.y = best_y + footprint_h / 2;
    center_anchor.type = AnchorType::Center;
    result.anchors.push_back(center_anchor);

    // Waterfront anchor — if water edge detected within/near footprint
    auto [wf_x, wf_y] = find_waterfront_anchor(map, best_x, best_y, footprint_w, footprint_h);
    if (wf_x >= 0) {
        Anchor wf;
        wf.x = wf_x;
        wf.y = wf_y;
        wf.type = AnchorType::Waterfront;
        result.anchors.push_back(wf);
    }

    // Elevated anchor — highest point if significantly higher than center
    auto [ep_x, ep_y] = find_elevated_point(channels, best_x, best_y, footprint_w, footprint_h);
    float center_elev = channels.elev(center_anchor.x, center_anchor.y);
    float peak_elev   = channels.elev(ep_x, ep_y);
    if (peak_elev - center_elev > 0.1f) {
        Anchor elev;
        elev.x = ep_x;
        elev.y = ep_y;
        elev.type = AnchorType::Elevated;
        result.anchors.push_back(elev);
    }

    return result;
}

} // namespace astra
