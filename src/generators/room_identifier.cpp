#include "astra/room_identifier.h"

#include <algorithm>
#include <queue>
#include <vector>

namespace astra {

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static bool is_walkable(Tile t) {
    return t == Tile::Floor || t == Tile::IndoorFloor || t == Tile::Path;
}

// ---------------------------------------------------------------------------
// identify — BFS flood-fill to discover rooms within the ruin footprint
// ---------------------------------------------------------------------------

void RoomIdentifier::identify(const TileMap& map, RuinPlan& plan,
                              std::mt19937& rng) const {
    const Rect& fp = plan.footprint;
    const int fw = fp.w;
    const int fh = fp.h;

    // visited grid local to the footprint
    std::vector<bool> visited(fw * fh, false);

    auto idx = [&](int lx, int ly) { return ly * fw + lx; };

    // Step 1: mark all non-walkable tiles as visited
    for (int ly = 0; ly < fh; ++ly) {
        for (int lx = 0; lx < fw; ++lx) {
            int mx = fp.x + lx;
            int my = fp.y + ly;
            if (!is_walkable(map.get(mx, my))) {
                visited[idx(lx, ly)] = true;
            }
        }
    }

    // 4-directional offsets
    static constexpr int dx[] = {0, 0, -1, 1};
    static constexpr int dy[] = {-1, 1, 0, 0};

    // Step 2: BFS from each unvisited tile
    for (int ly = 0; ly < fh; ++ly) {
        for (int lx = 0; lx < fw; ++lx) {
            if (visited[idx(lx, ly)]) continue;

            // flood-fill one connected region
            RuinRoom room;
            std::queue<std::pair<int,int>> q;
            q.push({lx, ly});
            visited[idx(lx, ly)] = true;

            int min_x = lx, max_x = lx;
            int min_y = ly, max_y = ly;

            while (!q.empty()) {
                auto [cx, cy] = q.front();
                q.pop();

                room.floor_tiles.push_back({fp.x + cx, fp.y + cy});

                min_x = std::min(min_x, cx);
                max_x = std::max(max_x, cx);
                min_y = std::min(min_y, cy);
                max_y = std::max(max_y, cy);

                for (int d = 0; d < 4; ++d) {
                    int nx = cx + dx[d];
                    int ny = cy + dy[d];
                    if (nx < 0 || nx >= fw || ny < 0 || ny >= fh) continue;
                    if (visited[idx(nx, ny)]) continue;
                    visited[idx(nx, ny)] = true;
                    q.push({nx, ny});
                }
            }

            // Step 3: skip small rooms (< 64 floor tiles)
            if (static_cast<int>(room.floor_tiles.size()) < 64) continue;

            // Step 4: compute bounding rect (map-space)
            room.bounds = {fp.x + min_x, fp.y + min_y,
                           max_x - min_x + 1, max_y - min_y + 1};

            // Check nucleus overlap: room center vs BSP nucleus leaves
            int center_x = room.bounds.x + room.bounds.w / 2;
            int center_y = room.bounds.y + room.bounds.h / 2;

            for (const auto& node : plan.bsp_nodes) {
                if (node.is_nucleus && node.area.contains(center_x, center_y)) {
                    room.is_nucleus = true;
                    break;
                }
            }

            // Assign theme
            room.theme = theme_for_geometry(room, plan.civ, rng);

            plan.rooms.push_back(std::move(room));
        }
    }
}

// ---------------------------------------------------------------------------
// theme_for_geometry — pick a BuildingType based on room shape & civ prefs
// ---------------------------------------------------------------------------

BuildingType RoomIdentifier::theme_for_geometry(const RuinRoom& room,
                                                const CivConfig& civ,
                                                std::mt19937& rng) const {
    // Nucleus rooms get civ-preferred types when available
    if (room.is_nucleus && !civ.preferred_rooms.empty()) {
        std::uniform_int_distribution<int> dist(
            0, static_cast<int>(civ.preferred_rooms.size()) - 1);
        return civ.preferred_rooms[dist(rng)];
    }

    // Organic assignment by geometry
    int area = static_cast<int>(room.floor_tiles.size());
    float w = static_cast<float>(room.bounds.w);
    float h = static_cast<float>(room.bounds.h);
    float aspect = (h > 0.0f) ? w / h : 1.0f;

    if (area > 300)                          return BuildingType::GreatHall;
    if (area < 80)                           return BuildingType::Vault;
    if (aspect > 2.5f || aspect < 0.4f)      return BuildingType::Archive;
    if (area > 150)                          return BuildingType::Temple;
    return BuildingType::Observatory;
}

// ---------------------------------------------------------------------------
// tag_connected_components — safety-net flood-fill for untagged layouts
// ---------------------------------------------------------------------------

void tag_connected_components(TileMap& map, RegionType default_type) {
    const int w = map.width();
    const int h = map.height();

    // visited grid — track cells already enqueued to avoid double-visiting
    std::vector<bool> visited(w * h, false);

    // Pre-mark non-passable and already-tagged cells as visited
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!map.passable(x, y) || map.region_id(x, y) >= 0) {
                visited[y * w + x] = true;
            }
        }
    }

    // 4-directional offsets
    static constexpr int dx[] = {0, 0, -1, 1};
    static constexpr int dy[] = {-1, 1, 0, 0};

    // BFS from each unvisited passable, untagged cell
    for (int sy = 0; sy < h; ++sy) {
        for (int sx = 0; sx < w; ++sx) {
            if (visited[sy * w + sx]) continue;

            // Flood-fill one connected component
            std::vector<std::pair<int,int>> component;
            std::queue<std::pair<int,int>> q;
            q.push({sx, sy});
            visited[sy * w + sx] = true;

            while (!q.empty()) {
                auto [cx, cy] = q.front();
                q.pop();

                component.push_back({cx, cy});

                for (int d = 0; d < 4; ++d) {
                    int nx = cx + dx[d];
                    int ny = cy + dy[d];
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                    if (visited[ny * w + nx]) continue;
                    visited[ny * w + nx] = true;
                    q.push({nx, ny});
                }
            }

            // Register the component as a new region
            Region reg;
            reg.type = default_type;
            int rid = map.add_region(std::move(reg));

            for (auto [cx, cy] : component) {
                map.set_region(cx, cy, rid);
            }
        }
    }
}

} // namespace astra
