#include "astra/game.h"
#include "astra/tile_props.h"

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


} // namespace astra
