#include "astra/poi_placement.h"

#include "astra/celestial_body.h"
#include "astra/map_properties.h"

#include <algorithm>
#include <random>

namespace astra {

namespace {

// Settlement/outpost spacing (same as legacy).
constexpr int kDefaultSpacing = 8;
// Tighter spacing for ships, ruins, caves near their own kind.
constexpr int kCloseSpacing = 6;

PoiTerrainRequirements reqs_for_settlement() {
    PoiTerrainRequirements r;
    r.needs_flat = true;
    r.min_spacing = kDefaultSpacing;
    return r;
}

PoiTerrainRequirements reqs_for_outpost() {
    PoiTerrainRequirements r;
    r.needs_flat = true;
    r.min_spacing = kDefaultSpacing;
    return r;
}

PoiTerrainRequirements reqs_for_cave(CaveVariant v) {
    PoiTerrainRequirements r;
    r.min_spacing = kCloseSpacing;
    if (v == CaveVariant::NaturalCave || v == CaveVariant::AbandonedMine)
        r.needs_cliff = true;
    // Excavation: no strict cliff requirement.
    return r;
}

PoiTerrainRequirements reqs_for_ship() {
    PoiTerrainRequirements r;
    r.needs_flat = true;
    r.min_spacing = kCloseSpacing;
    return r;
}

PoiTerrainRequirements reqs_for_ruin() {
    PoiTerrainRequirements r;
    r.min_spacing = kCloseSpacing;
    return r;
}

} // namespace

std::vector<PoiRequest> expand_budget_to_requests(const PoiBudget& budget,
                                                   const MapProperties& props,
                                                   std::mt19937& /*rng*/) {
    std::vector<PoiRequest> out;

    // Settlements
    for (int i = 0; i < budget.settlements; ++i) {
        PoiRequest r;
        r.poi_tile = Tile::OW_Settlement;
        r.reqs = reqs_for_settlement();
        r.priority = (i == 0 && props.body_type == BodyType::Terrestrial)
                         ? PoiPriority::Required
                         : PoiPriority::Normal;
        out.push_back(std::move(r));
    }

    // Outposts
    for (int i = 0; i < budget.outposts; ++i) {
        PoiRequest r;
        r.poi_tile = Tile::OW_Outpost;
        r.reqs = reqs_for_outpost();
        r.priority = PoiPriority::Normal;
        out.push_back(std::move(r));
    }

    // Caves — natural
    for (int i = 0; i < budget.caves.natural; ++i) {
        PoiRequest r;
        r.poi_tile = Tile::OW_CaveEntrance;
        r.cave_variant = CaveVariant::NaturalCave;
        r.reqs = reqs_for_cave(CaveVariant::NaturalCave);
        r.priority = PoiPriority::Normal;
        out.push_back(std::move(r));
    }
    // Caves — mine
    for (int i = 0; i < budget.caves.mine; ++i) {
        PoiRequest r;
        r.poi_tile = Tile::OW_CaveEntrance;
        r.cave_variant = CaveVariant::AbandonedMine;
        r.reqs = reqs_for_cave(CaveVariant::AbandonedMine);
        r.priority = (props.lore_tier >= 2) ? PoiPriority::Required
                                             : PoiPriority::Normal;
        out.push_back(std::move(r));
    }
    // Caves — excavation
    for (int i = 0; i < budget.caves.excavation; ++i) {
        PoiRequest r;
        r.poi_tile = Tile::OW_CaveEntrance;
        r.cave_variant = CaveVariant::AncientExcavation;
        r.reqs = reqs_for_cave(CaveVariant::AncientExcavation);
        r.priority = (props.lore_tier >= 3) ? PoiPriority::Required
                                             : PoiPriority::Normal;
        out.push_back(std::move(r));
    }

    // Ruins
    int half_hidden = std::max(0, budget.hidden_ruin_count() / 2);
    int hidden_placed = 0;
    for (const auto& ruin : budget.ruins) {
        PoiRequest r;
        r.poi_tile = Tile::OW_Ruins;
        r.reqs = reqs_for_ruin();
        r.ruin_civ = ruin.civ;
        r.ruin_formation = ruin.formation;
        r.ruin_hidden = ruin.hidden;
        if (ruin.hidden && props.lore_tier >= 3 && hidden_placed < half_hidden) {
            r.priority = PoiPriority::Required;
            ++hidden_placed;
        } else {
            r.priority = PoiPriority::Normal;
        }
        out.push_back(std::move(r));
    }

    // Ships
    for (size_t i = 0; i < budget.ships.size(); ++i) {
        PoiRequest r;
        r.poi_tile = Tile::OW_CrashedShip;
        r.ship_class = budget.ships[i].klass;
        r.reqs = reqs_for_ship();
        r.priority = (i == 0 && props.lore_battle_site) ? PoiPriority::Required
                                                         : PoiPriority::Normal;
        out.push_back(std::move(r));
    }

    return out;
}

namespace {

struct TerrainCell {
    Biome biome = Biome::Station;
    bool cliff_adjacent = false;
    AnchorDirection cliff_dir = AnchorDirection::None;
    bool water_adjacent = false;
    AnchorDirection water_dir = AnchorDirection::None;
    bool flat = false;
    bool walkable = false;
};

struct TerrainCache {
    int w = 0;
    int h = 0;
    std::vector<TerrainCell> cells;
    const TerrainCell& at(int x, int y) const { return cells[y * w + x]; }
    TerrainCell& at(int x, int y) { return cells[y * w + x]; }
};

bool tile_is_walkable_base(Tile t) {
    switch (t) {
        case Tile::OW_Plains:
        case Tile::OW_Forest:
        case Tile::OW_Desert:
        case Tile::OW_Swamp:
        case Tile::OW_Barren:
        case Tile::OW_Fungal:
        case Tile::OW_IceField:
        case Tile::OW_AlienTerrain:
        case Tile::OW_ScorchedEarth:
            return true;
        default:
            return false;
    }
}

TerrainCache build_terrain_cache(const TileMap& map) {
    TerrainCache c;
    c.w = map.width();
    c.h = map.height();
    c.cells.resize(static_cast<size_t>(c.w) * static_cast<size_t>(c.h));

    for (int y = 0; y < c.h; ++y) {
        for (int x = 0; x < c.w; ++x) {
            TerrainCell cell;
            Tile t = map.get(x, y);
            cell.walkable = tile_is_walkable_base(t);
            cell.flat = cell.walkable;

            static const int dx4[] = { 0, 0, -1, 1};
            static const int dy4[] = {-1, 1,  0, 0};
            static const AnchorDirection dir4[] = {
                AnchorDirection::North, AnchorDirection::South,
                AnchorDirection::West,  AnchorDirection::East,
            };
            for (int d = 0; d < 4; ++d) {
                int nx = x + dx4[d], ny = y + dy4[d];
                if (nx < 0 || nx >= c.w || ny < 0 || ny >= c.h) continue;
                Tile nt = map.get(nx, ny);
                if (nt == Tile::OW_Mountains || nt == Tile::OW_Crater) {
                    if (!cell.cliff_adjacent) {
                        cell.cliff_adjacent = true;
                        cell.cliff_dir = dir4[d];
                    }
                }
                if (nt == Tile::OW_Lake || nt == Tile::OW_River) {
                    if (!cell.water_adjacent) {
                        cell.water_adjacent = true;
                        cell.water_dir = dir4[d];
                    }
                }
            }
            c.at(x, y) = cell;
        }
    }
    return c;
}

bool candidate_meets_reqs(const TerrainCell& cell, const PoiTerrainRequirements& r) {
    if (!cell.walkable) return false;
    if (r.needs_cliff && !cell.cliff_adjacent) return false;
    if (r.needs_water_adjacent && !cell.water_adjacent) return false;
    if (r.needs_flat && !cell.flat) return false;
    return true;
}

int manhattan(int ax, int ay, int bx, int by) {
    int dx = ax - bx; if (dx < 0) dx = -dx;
    int dy = ay - by; if (dy < 0) dy = -dy;
    return dx + dy;
}

int score_candidate(int x, int y, int map_w, int map_h) {
    return manhattan(x, y, map_w / 2, map_h / 2);
}

PoiAnchorHint make_hint(const PoiRequest& req, const TerrainCell& cell) {
    PoiAnchorHint h;
    h.valid = true;
    if (cell.cliff_adjacent && req.reqs.needs_cliff) {
        h.reason = AnchorReason::CliffAdjacent;
        h.direction = cell.cliff_dir;
    } else if (cell.water_adjacent && req.reqs.needs_water_adjacent) {
        h.reason = AnchorReason::WaterAdjacent;
        h.direction = cell.water_dir;
    } else if (req.reqs.needs_flat) {
        h.reason = AnchorReason::Flat;
    } else {
        h.reason = AnchorReason::Open;
    }
    h.cave_variant = req.cave_variant;
    h.ship_class = req.ship_class;
    h.ruin_civ = req.ruin_civ;
    h.ruin_formation = req.ruin_formation;
    return h;
}

} // namespace

void run_poi_placement(TileMap& overworld, const MapProperties& props,
                       std::mt19937& rng) {
    const PoiBudget& budget = overworld.poi_budget();
    auto requests = expand_budget_to_requests(budget, props, rng);

    // Partition by priority: Required first, then Normal, then Opportunistic.
    std::stable_sort(requests.begin(), requests.end(),
                     [](const PoiRequest& a, const PoiRequest& b) {
        return static_cast<int>(a.priority) < static_cast<int>(b.priority);
    });

    TerrainCache cache = build_terrain_cache(overworld);
    int w = cache.w;
    int h = cache.h;

    struct Placed { int x, y, spacing; };
    std::vector<Placed> placed;

    auto too_close = [&](int px, int py, int spacing) {
        for (const auto& p : placed) {
            int s = std::max(p.spacing, spacing);
            if (manhattan(px, py, p.x, p.y) < s) return true;
        }
        return false;
    };

    // Candidate order is shuffled once so spatially-different seeds get
    // different POI layouts.
    std::vector<int> candidate_order(static_cast<size_t>(w) * static_cast<size_t>(h));
    for (int i = 0; i < w * h; ++i) candidate_order[i] = i;
    std::shuffle(candidate_order.begin(), candidate_order.end(), rng);

    for (const auto& req : requests) {
        int best_idx = -1;
        int best_score = 0;
        for (int idx : candidate_order) {
            int x = idx % w;
            int y = idx / w;
            if (x < 2 || x >= w - 2 || y < 2 || y >= h - 2) continue;
            const TerrainCell& cell = cache.at(x, y);
            if (!candidate_meets_reqs(cell, req.reqs)) continue;
            if (too_close(x, y, req.reqs.min_spacing)) continue;
            int s = score_candidate(x, y, w, h);
            if (best_idx < 0 || s < best_score) {
                best_idx = idx;
                best_score = s;
            }
        }
        if (best_idx < 0) continue; // failed — silent for Normal/Opportunistic

        int px = best_idx % w;
        int py = best_idx / w;
        const TerrainCell& cell = cache.at(px, py);
        PoiAnchorHint hint = make_hint(req, cell);

        if (req.poi_tile == Tile::OW_Ruins && req.ruin_hidden) {
            HiddenPoi hp;
            hp.x = px;
            hp.y = py;
            hp.underlying_tile = overworld.get(px, py);
            hp.real_tile = Tile::OW_Ruins;
            hp.ruin_civ = req.ruin_civ;
            hp.ruin_formation = req.ruin_formation;
            overworld.hidden_pois_mut().push_back(hp);
        } else {
            overworld.set(px, py, req.poi_tile);
        }
        overworld.set_anchor_hint(px, py, hint);
        placed.push_back({px, py, req.reqs.min_spacing});
    }
}

} // namespace astra
