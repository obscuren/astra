#include "astra/settlement_types.h"

namespace astra {

CivStyle civ_frontier() {
    CivStyle s;
    s.name = "Frontier";
    s.wall_tile   = Tile::StructuralWall;
    s.floor_tile  = Tile::IndoorFloor;
    s.path_tile   = Tile::Path;
    s.lighting    = FixtureType::Torch;
    s.storage     = FixtureType::Crate;
    s.seating     = FixtureType::Bench;
    s.cooking     = FixtureType::CampStove;
    s.knowledge   = FixtureType::BookCabinet;
    s.display     = FixtureType::Rack;
    s.perimeter_wall = Tile::StructuralWall;
    s.gate        = FixtureType::Gate;
    s.bridge_rail  = FixtureType::BridgeRail;
    s.bridge_floor = FixtureType::BridgeFloor;
    s.decay       = 0.0f;
    return s;
}

CivStyle civ_advanced() {
    CivStyle s;
    s.name = "Advanced";
    s.wall_tile   = Tile::StructuralWall;
    s.floor_tile  = Tile::IndoorFloor;
    s.path_tile   = Tile::IndoorFloor;
    s.lighting    = FixtureType::HoloLight;
    s.storage     = FixtureType::Locker;
    s.seating     = FixtureType::Chair;
    s.cooking     = FixtureType::FoodTerminal;
    s.knowledge   = FixtureType::DataTerminal;
    s.display     = FixtureType::WeaponDisplay;
    s.perimeter_wall = Tile::StructuralWall;
    s.gate        = FixtureType::Gate;
    s.bridge_rail  = FixtureType::BridgeRail;
    s.bridge_floor = FixtureType::BridgeFloor;
    s.decay       = 0.0f;
    return s;
}

CivStyle civ_ruined() {
    CivStyle s;
    s.name = "Ruined";
    s.wall_tile   = Tile::Wall;
    s.floor_tile  = Tile::Floor;
    s.path_tile   = Tile::Floor;
    s.lighting    = FixtureType::Torch;
    s.storage     = FixtureType::Crate;
    s.seating     = FixtureType::Debris;
    s.cooking     = FixtureType::Debris;
    s.knowledge   = FixtureType::Debris;
    s.display     = FixtureType::Debris;
    s.perimeter_wall = Tile::Wall;
    s.gate        = FixtureType::Gate;
    s.bridge_rail  = FixtureType::BridgeRail;
    s.bridge_floor = FixtureType::BridgeFloor;
    s.decay       = 0.35f;
    return s;
}

CivStyle civ_ruin_default() {
    CivStyle s;
    s.name = "RuinDefault";
    s.wall_tile    = Tile::Wall;
    s.floor_tile   = Tile::IndoorFloor;
    s.path_tile    = Tile::Path;
    s.lighting     = FixtureType::Torch;
    s.storage      = FixtureType::Debris;
    s.seating      = FixtureType::Debris;
    s.cooking      = FixtureType::Debris;
    s.knowledge    = FixtureType::Debris;
    s.display      = FixtureType::Debris;
    s.perimeter_wall = Tile::Wall;
    s.gate         = FixtureType::Gate;
    s.bridge_rail  = FixtureType::BridgeRail;
    s.bridge_floor = FixtureType::BridgeFloor;
    s.decay        = 0.5f;
    return s;
}

CivStyle civ_ruin_heavy() {
    CivStyle s = civ_ruin_default();
    s.name = "RuinHeavy";
    s.decay = 0.7f;
    return s;
}

CivStyle select_civ_style(const MapProperties& props) {
    if (props.lore_plague_origin) {
        return civ_ruined();
    }
    if (props.lore_alien_strength > 0.3f) {
        return civ_advanced();
    }
    if (props.lore_tier >= 2) {
        return civ_advanced();
    }
    return civ_frontier();
}

} // namespace astra
