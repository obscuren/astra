#include "astra/grid_regional_generator.h"

#include <algorithm>
#include <random>
#include <vector>

namespace astra::grid_regional_generator {

namespace {

struct Rect {
    int x, y, w, h;
    int cx() const { return x + w / 2; }
    int cy() const { return y + h / 2; }
};

// Tunables picked to keep room geometry pleasant at 40×24.
constexpr int kSectorW       = 40;
constexpr int kSectorH       = 24;
constexpr int kMinRoomEdge   = 6;   // smallest split that still has a 4-cell carve
constexpr int kMinCarveEdge  = 4;   // wall, two interior cells, wall

// Split rect into two children along the longer axis. Returns false if the
// rect is too small to split further.
bool split(const Rect& src, std::mt19937& rng, Rect& a, Rect& b) {
    bool can_h = src.w >= kMinRoomEdge * 2;
    bool can_v = src.h >= kMinRoomEdge * 2;
    if (!can_h && !can_v) return false;

    bool vertical = can_v && (!can_h || src.h * 11 > src.w * 10
                              || (rng() & 1u));
    if (vertical) {
        std::uniform_int_distribution<int> d(kMinRoomEdge, src.h - kMinRoomEdge);
        int cut = d(rng);
        a = {src.x, src.y,           src.w, cut};
        b = {src.x, src.y + cut,     src.w, src.h - cut};
    } else {
        std::uniform_int_distribution<int> d(kMinRoomEdge, src.w - kMinRoomEdge);
        int cut = d(rng);
        a = {src.x,           src.y, cut,           src.h};
        b = {src.x + cut,     src.y, src.w - cut,   src.h};
    }
    return true;
}

void carve_room(GridSector& s, const Rect& r) {
    // Outer wall stays Firewall; carve the 1-tile-inset interior to Floor.
    for (int y = r.y + 1; y < r.y + r.h - 1; ++y) {
        for (int x = r.x + 1; x < r.x + r.w - 1; ++x) {
            s.set(x, y, GridTile::Floor);
        }
    }
}

// Punch a single 1-tile floor doorway through whichever wall separates a from b.
void connect(GridSector& s, const Rect& a, const Rect& b, std::mt19937& rng) {
    // Determine shared edge.
    bool horizontal = (a.x + a.w == b.x) || (b.x + b.w == a.x);
    if (horizontal) {
        int wall_x = (a.x + a.w == b.x) ? a.x + a.w - 1 : a.x;
        int y_lo   = std::max(a.y, b.y) + 1;
        int y_hi   = std::min(a.y + a.h, b.y + b.h) - 2;
        if (y_hi < y_lo) return;
        std::uniform_int_distribution<int> d(y_lo, y_hi);
        int y = d(rng);
        s.set(wall_x, y, GridTile::Floor);
    } else {
        int wall_y = (a.y + a.h == b.y) ? a.y + a.h - 1 : a.y;
        int x_lo   = std::max(a.x, b.x) + 1;
        int x_hi   = std::min(a.x + a.w, b.x + b.w) - 2;
        if (x_hi < x_lo) return;
        std::uniform_int_distribution<int> d(x_lo, x_hi);
        int x = d(rng);
        s.set(x, wall_y, GridTile::Floor);
    }
}

// Recursive BSP. We track parent-child pairs so we can connect siblings.
void subdivide(const Rect& root, int target_leaves, std::mt19937& rng,
               std::vector<Rect>& leaves,
               std::vector<std::pair<Rect, Rect>>& sibling_pairs) {
    if (target_leaves <= 1) {
        leaves.push_back(root);
        return;
    }
    Rect a, b;
    if (!split(root, rng, a, b)) {
        leaves.push_back(root);
        return;
    }
    sibling_pairs.emplace_back(a, b);
    int left_target  = target_leaves / 2;
    int right_target = target_leaves - left_target;
    subdivide(a, left_target,  rng, leaves, sibling_pairs);
    subdivide(b, right_target, rng, leaves, sibling_pairs);
}

bool place_random_floor(GridSector& s, GridTile t, std::mt19937& rng) {
    std::uniform_int_distribution<int> dx(1, s.w - 2);
    std::uniform_int_distribution<int> dy(1, s.h - 2);
    for (int tries = 0; tries < 60; ++tries) {
        int x = dx(rng), y = dy(rng);
        if (s.at(x, y) == GridTile::Floor) {
            s.set(x, y, t);
            return true;
        }
    }
    return false;
}

} // namespace

GridSector generate(uint32_t seed, int security_tier,
                    int min_rooms, int max_rooms) {
    std::mt19937 rng(seed);

    GridSector s;
    s.w = kSectorW;
    s.h = kSectorH;
    s.tiles.assign(static_cast<size_t>(s.w * s.h), GridTile::Firewall);

    std::uniform_int_distribution<int> rd(min_rooms, max_rooms);
    int target = rd(rng);

    std::vector<Rect> leaves;
    std::vector<std::pair<Rect, Rect>> pairs;
    Rect root{0, 0, s.w, s.h};
    subdivide(root, target, rng, leaves, pairs);

    for (const auto& r : leaves) carve_room(s, r);
    for (const auto& [a, b] : pairs) connect(s, a, b, rng);

    // Spawn at first leaf's centre; make sure the tile is floor.
    if (!leaves.empty()) {
        s.spawn_x = leaves.front().cx();
        s.spawn_y = leaves.front().cy();
        if (s.at(s.spawn_x, s.spawn_y) != GridTile::Floor) {
            s.set(s.spawn_x, s.spawn_y, GridTile::Floor);
        }
    } else {
        s.spawn_x = 1;
        s.spawn_y = 1;
    }

    // ExitNode in the leaf farthest from spawn so jack-out isn't on top of you.
    if (!leaves.empty()) {
        const Rect* far = &leaves.front();
        int best = -1;
        for (const auto& r : leaves) {
            int dx = r.cx() - s.spawn_x;
            int dy = r.cy() - s.spawn_y;
            int d2 = dx * dx + dy * dy;
            if (d2 > best) { best = d2; far = &r; }
        }
        s.set(far->cx(), far->cy(), GridTile::ExitNode);
    }

    // Decorate.
    int n_enc = 1 + static_cast<int>(rng() % 4);
    for (int i = 0; i < n_enc; ++i) place_random_floor(s, GridTile::EncryptedFile, rng);
    int n_data = static_cast<int>(rng() % 3);
    for (int i = 0; i < n_data; ++i) place_random_floor(s, GridTile::DataNode, rng);
    if ((rng() & 1u) == 0) place_random_floor(s, GridTile::Gateway, rng);

    if (security_tier >= 3) {
        place_random_floor(s, GridTile::EncryptedFile, rng);
    }

    return s;
}

} // namespace astra::grid_regional_generator
