#pragma once

#include "astra/grid_ice.h"
#include "astra/grid_sector.h"
#include "astra/grid_theme.h"
#include "astra/program.h"
#include "astra/renderer.h"

#include <string>

namespace astra {

inline std::string display_name(IceColor c) {
    switch (c) {
        case IceColor::White: return colored("White ICE", grid_theme::white_ice);
        case IceColor::Gray:  return colored("Gray ICE",  grid_theme::gray_ice);
        case IceColor::Black: return colored("Black ICE", grid_theme::black_ice);
    }
    return colored("ICE", Color::White);
}

inline std::string display_name(GridTile t) {
    switch (t) {
        case GridTile::Floor:           return colored("floor",             grid_theme::floor);
        case GridTile::Firewall:        return colored("firewall",          grid_theme::firewall);
        case GridTile::DataNode:        return colored("data node",         grid_theme::data_node);
        case GridTile::ExitNode:        return colored("exit node",         grid_theme::exit_node);
        case GridTile::EncryptedFile:   return colored("encrypted file",    grid_theme::encrypted);
        case GridTile::Wall:            return colored("wall",              Color::DarkGray);
        case GridTile::Connector:       return colored("connector",         grid_theme::connector);
        case GridTile::DeepGridGateway: return colored("deep-grid gateway", grid_theme::deep_grid_gateway);
        case GridTile::WarpAnchor:      return colored("warp anchor",       grid_theme::warp_anchor);
        case GridTile::DeviceAvatar:    return colored("device",            Color::BrightWhite);
        case GridTile::Door:            return colored("door",              grid_theme::door_open);
        case GridTile::Void:            return colored("void",              Color::DarkGray);
    }
    return {};
}

inline Color program_kind_color(ProgramKind k) {
    switch (k) {
        case ProgramKind::Atk: return Color::Red;
        case ProgramKind::Stl: return Color::Cyan;
        case ProgramKind::Utl: return Color::Yellow;
        case ProgramKind::Qh:  return Color::Magenta;
    }
    return Color::White;
}

inline std::string display_name(ProgramId id) {
    const ProgramDef* def = find_program(id);
    if (!def) return {};
    return colored(def->filename, program_kind_color(def->kind));
}

// Convenience overload: color a free-form program filename string (used when
// only the def is in scope, not the id).
inline std::string display_name(const ProgramDef& def) {
    return colored(def.filename, program_kind_color(def.kind));
}

} // namespace astra
