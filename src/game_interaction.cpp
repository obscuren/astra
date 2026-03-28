#include "astra/game.h"
#include "astra/tile_props.h"

#include <deque>
#include <vector>

namespace astra {

void Game::try_move(int dx, int dy) {
    int nx = player_.x + dx;
    int ny = player_.y + dy;

    // Detail map: edge transitions
    if (world_.on_detail_map()) {
        if (nx < 0 || nx >= world_.map().width() || ny < 0 || ny >= world_.map().height()) {
            int ddx = (nx < 0) ? -1 : (nx >= world_.map().width()) ? 1 : 0;
            int ddy = (ny < 0) ? -1 : (ny >= world_.map().height()) ? 1 : 0;
            transition_detail_edge(ddx, ddy);
            return;
        }
        // Fall through to standard dungeon movement below
    }

    // Overworld: simplified movement
    if (world_.on_overworld()) {
        if (nx < 0 || nx >= world_.map().width() || ny < 0 || ny >= world_.map().height()) return;
        if (!world_.map().passable(nx, ny)) {
            log("Impassable terrain.");
            return;
        }
        Tile prev_tile = world_.map().get(player_.x, player_.y);
        player_.x = nx;
        player_.y = ny;
        // Walk-over messages for POI tiles (suppress when moving within same tile type)
        Tile stepped = world_.map().get(nx, ny);
        if (stepped != prev_tile) {
            switch (stepped) {
                case Tile::OW_Settlement:   log("A settlement. Press > to enter."); break;
                case Tile::OW_CaveEntrance: log("A cave entrance. Press > to descend."); break;
                case Tile::OW_Ruins:        log("Ancient ruins. Press > to explore."); break;
                case Tile::OW_CrashedShip:  log("Wreckage of a starship. Press > to investigate."); break;
                case Tile::OW_Outpost:      log("An outpost. Press > to enter."); break;
                case Tile::OW_Landing:      log("Your starship. Press 'e' to board."); break;
                default: break;
            }
        }
        compute_camera();
        advance_world(15);
        return;
    }

    if (!world_.map().passable(nx, ny)) {
        auto msg = random_bump_message(world_.map().get(nx, ny), world_.map().map_type(), world_.rng());
        if (!msg.empty()) {
            log(std::string(msg));
        }
        return;
    }

    // Check NPC collision
    for (auto& npc : world_.npcs()) {
        if (npc.alive() && npc.x == nx && npc.y == ny) {
            if (npc.disposition == Disposition::Hostile) {
                combat_.attack_npc(npc, *this);
                advance_world(ActionCost::move);
                return;
            }
            // Swap positions with friendly/neutral NPC
            npc.return_x = npc.x;
            npc.return_y = npc.y;
            npc.x = player_.x;
            npc.y = player_.y;
            player_.x = nx;
            player_.y = ny;
            recompute_fov();
            compute_camera();
            advance_world(ActionCost::move);
            return;
        }
    }

    player_.x = nx;
    player_.y = ny;

    // Portal tile: return to detail map / overworld if in a dungeon on a body
    if (world_.map().get(nx, ny) == Tile::Portal &&
        world_.surface_mode() == SurfaceMode::Dungeon && !world_.navigation().at_station && !world_.navigation().on_ship) {
        exit_dungeon_to_detail();
        return;
    }

    recompute_fov();
    compute_camera();
    check_region_change();
    advance_world(ActionCost::move);
}

void Game::check_region_change() {
    int rid = world_.map().region_id(player_.x, player_.y);
    if (rid == world_.current_region() || rid < 0) return;

    world_.current_region() = rid;
    const auto& reg = world_.map().region(rid);
    if (!reg.enter_message.empty()) {
        log(reg.enter_message);
    }

    // Feature hints for hub rooms
    if (has_feature(reg.features, RoomFeature::Healing)) {
        log("Healing pods hum softly, ready for use. [e to interact]");
    }
    if (has_feature(reg.features, RoomFeature::FoodShop)) {
        log("A food terminal glows nearby. [e to interact]");
    }
    if (has_feature(reg.features, RoomFeature::Rest)) {
        log("A rest pod glows at the far end. [e to interact]");
    }
    if (has_feature(reg.features, RoomFeature::Repair)) {
        log("A repair bench sits against the wall. [e to interact]");
    }
}

void Game::try_interact(int dx, int dy) {
    int tx = player_.x + dx;
    int ty = player_.y + dy;

    // Find NPC at target tile
    Npc* target = nullptr;
    for (auto& npc : world_.npcs()) {
        if (npc.x == tx && npc.y == ty) {
            target = &npc;
            break;
        }
    }

    if (!target) {
        Tile t = world_.map().get(tx, ty);
        if (t == Tile::Fixture) {
            int fid = world_.map().fixture_id(tx, ty);
            if (fid >= 0 && world_.map().fixture(fid).interactable) {
                dialog_.interact_fixture(fid, *this);
                advance_world(ActionCost::interact);
                return;
            }
            // Non-interactable fixture
            log("Nothing useful here.");
            return;
        }
        if (t == Tile::Wall || t == Tile::StructuralWall) {
            log("You run your hand along the cold bulkhead. Nothing of interest.");
        } else if (t == Tile::Empty) {
            log("You stare into the void. It stares back.");
        } else {
            log("Nothing to interact with there.");
        }
        return;
    }

    if (target->disposition == Disposition::Hostile) {
        log(target->display_name() + " snarls at you.");
        advance_world(ActionCost::interact);
        return;
    }

    if (target->interactions.empty()) {
        log(target->display_name() + " has nothing to say.");
        return;
    }

    log("You approach " + target->display_name() + ".");
    dialog_.open_npc_dialog(*target, *this);
    advance_world(ActionCost::interact);
}

bool Game::is_interactable(int tx, int ty) const {
    // Check for NPC
    for (const auto& npc : world_.npcs()) {
        if (npc.x == tx && npc.y == ty && npc.disposition != Disposition::Hostile) return true;
    }
    // Check for interactable fixture (including doors)
    Tile t = world_.map().get(tx, ty);
    if (t == Tile::Fixture) {
        int fid = world_.map().fixture_id(tx, ty);
        if (fid >= 0 && world_.map().fixture(fid).interactable) return true;
    }
    // Check for ground items at player's own tile
    if (tx == player_.x && ty == player_.y) {
        for (const auto& gi : world_.ground_items()) {
            if (gi.x == tx && gi.y == ty) return true;
        }
        // Stairs/portals under player
        if (t == Tile::Portal) return true;
    }
    return false;
}

int Game::count_adjacent_interactables() const {
    static const int dx[] = {0, -1, 1, 0, 0};
    static const int dy[] = {0, 0, 0, -1, 1};
    int count = 0;
    for (int i = 0; i < 5; ++i) {
        if (is_interactable(player_.x + dx[i], player_.y + dy[i])) ++count;
    }
    return count;
}

void Game::use_action() {
    // Scan all 4 adjacent tiles + player tile for interactables
    struct Target { int x, y; };
    std::vector<Target> targets;

    static const int dx[] = {0, -1, 1, 0, 0};
    static const int dy[] = {0, 0, 0, -1, 1};
    for (int i = 0; i < 5; ++i) {
        int tx = player_.x + dx[i];
        int ty = player_.y + dy[i];
        if (is_interactable(tx, ty)) targets.push_back({tx, ty});
    }

    if (targets.empty()) {
        log("Nothing to interact with nearby.");
        return;
    }

    if (targets.size() == 1) {
        use_at(targets[0].x, targets[0].y);
        return;
    }

    // Multiple targets — prompt for direction
    awaiting_interact_ = true;
    log("Use -- choose a direction.");
}

void Game::use_at(int tx, int ty) {
    // Ground items at player position
    if (tx == player_.x && ty == player_.y) {
        // Check for ground items first
        for (size_t i = 0; i < world_.ground_items().size(); ++i) {
            if (world_.ground_items()[i].x == tx && world_.ground_items()[i].y == ty) {
                pickup_ground_item();
                return;
            }
        }
        // Portal / stairs
        Tile t = world_.map().get(tx, ty);
        if (t == Tile::Portal) {
            if (world_.on_detail_map()) {
                enter_dungeon_from_detail();
            } else if (world_.surface_mode() == SurfaceMode::Dungeon) {
                exit_dungeon_to_detail();
            }
            return;
        }
    }

    // Adjacent tile — delegate to try_interact logic
    int dx = tx - player_.x;
    int dy = ty - player_.y;
    try_interact(dx, dy);
}


// ── Auto-walk / Auto-explore ────────────────────────────────────────

static const int dx4[] = {0, 0, -1, 1};
static const int dy4[] = {-1, 1, 0, 0};

bool Game::auto_walk_should_stop() const {
    // Hostile NPC visible
    for (const auto& npc : world_.npcs()) {
        if (!npc.alive() || npc.disposition != Disposition::Hostile) continue;
        if (world_.visibility().get(npc.x, npc.y) == Visibility::Visible) return true;
    }
    // Item on ground at player position
    for (const auto& gi : world_.ground_items()) {
        if (gi.x == player_.x && gi.y == player_.y) return true;
    }
    // Player took damage
    if (player_.hp < auto_walk_hp_) return true;
    // Door adjacent
    for (int i = 0; i < 4; ++i) {
        int nx = player_.x + dx4[i], ny = player_.y + dy4[i];
        if (world_.map().get(nx, ny) == Tile::Fixture) {
            int fid = world_.map().fixture_id(nx, ny);
            if (fid >= 0 && world_.map().fixture(fid).type == FixtureType::Door) return true;
        }
    }
    return false;
}

void Game::auto_step() {
    if (auto_walk_should_stop()) {
        auto_walking_ = false;
        auto_exploring_ = false;
        return;
    }

    if (auto_walking_) {
        // Straight-line walk
        int nx = player_.x + auto_walk_dx_;
        int ny = player_.y + auto_walk_dy_;
        if (!world_.map().passable(nx, ny)) {
            auto_walking_ = false;
            return;
        }
        // Check for intersection: only in corridors (<=2 open neighbors)
        // In open rooms, skip this check to avoid immediately stopping
        int open_here = 0;
        int open_next = 0;
        for (int i = 0; i < 4; ++i) {
            if (world_.map().passable(player_.x + dx4[i], player_.y + dy4[i])) ++open_here;
            if (world_.map().passable(nx + dx4[i], ny + dy4[i])) ++open_next;
        }
        // If we're in a corridor and the next tile is an intersection, stop
        if (open_here <= 2 && open_next > 2) {
            auto_walking_ = false;
            return;
        }
        // Check for hostile NPC at destination
        for (const auto& npc : world_.npcs()) {
            if (npc.alive() && npc.x == nx && npc.y == ny) {
                auto_walking_ = false;
                return;
            }
        }
        try_move(auto_walk_dx_, auto_walk_dy_);
        auto_walk_hp_ = player_.hp; // update for damage detection
    }
    else if (auto_exploring_) {
        auto [dx, dy] = bfs_explore_step();
        if (dx == 0 && dy == 0) {
            auto_exploring_ = false;
            log("Nothing left to explore nearby.");
            return;
        }
        try_move(dx, dy);
        auto_walk_hp_ = player_.hp;
    }
}

std::pair<int,int> Game::bfs_explore_step() const {
    int w = world_.map().width();
    int h = world_.map().height();
    int px = player_.x, py = player_.y;

    // BFS to find nearest tile adjacent to an unexplored tile
    std::vector<std::vector<int>> dist(h, std::vector<int>(w, -1));
    std::vector<std::vector<std::pair<int,int>>> parent(h, std::vector<std::pair<int,int>>(w, {-1,-1}));

    std::deque<std::pair<int,int>> queue;
    dist[py][px] = 0;
    queue.push_back({px, py});

    static const int dx4[] = {0, 0, -1, 1};
    static const int dy4[] = {-1, 1, 0, 0};

    int goal_x = -1, goal_y = -1;

    while (!queue.empty()) {
        auto [cx, cy] = queue.front();
        queue.pop_front();

        // Check if this tile is adjacent to an unexplored tile
        if (cx != px || cy != py) { // not the starting tile
            for (int i = 0; i < 4; ++i) {
                int nx = cx + dx4[i], ny = cy + dy4[i];
                if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                    if (world_.visibility().get(nx, ny) == Visibility::Unexplored) {
                        goal_x = cx;
                        goal_y = cy;
                        goto found;
                    }
                }
            }
        }

        // Expand neighbors
        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx4[i], ny = cy + dy4[i];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            if (dist[ny][nx] >= 0) continue; // already visited
            if (!world_.map().passable(nx, ny)) continue;
            // Don't walk through NPCs
            bool blocked = false;
            for (const auto& npc : world_.npcs()) {
                if (npc.alive() && npc.x == nx && npc.y == ny) { blocked = true; break; }
            }
            if (blocked) continue;
            dist[ny][nx] = dist[cy][cx] + 1;
            parent[ny][nx] = {cx, cy};
            queue.push_back({nx, ny});
        }
    }
    return {0, 0}; // nothing to explore

found:
    // Trace back to find the first step from player
    int tx = goal_x, ty = goal_y;
    while (parent[ty][tx].first != px || parent[ty][tx].second != py) {
        auto [ppx, ppy] = parent[ty][tx];
        tx = ppx;
        ty = ppy;
    }
    return {tx - px, ty - py};
}

} // namespace astra
