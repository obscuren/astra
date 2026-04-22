#include "astra/dungeon/dungeon_style.h"

#include <cassert>
#include <string>
#include <unordered_map>

namespace astra::dungeon {

namespace {

// Initialized at static-init time; read-only afterwards.
const DungeonStyle kSimpleRoomsAndCorridors = [] {
    DungeonStyle s;
    s.id                   = StyleId::SimpleRoomsAndCorridors;
    s.debug_name           = "simple_rooms";
    s.backdrop_material    = "rock";
    s.layout               = LayoutKind::BSPRooms;
    s.stairs_strategy      = StairsStrategy::EntryExitRooms;
    s.allowed_overlays     = { OverlayKind::BattleScarred, OverlayKind::Infested };
    s.decoration_pack      = "ruin_debris";
    s.connectivity_required = true;
    return s;
}();

} // namespace

const DungeonStyle& style_config(StyleId id) {
    switch (id) {
    case StyleId::SimpleRoomsAndCorridors: return kSimpleRoomsAndCorridors;

    // Placeholders — follow-up slices register real configs.
    case StyleId::PrecursorRuin:
    case StyleId::OpenCave:
    case StyleId::TunnelCave:
    case StyleId::DerelictStation:
        assert(!"style not yet registered");
        return kSimpleRoomsAndCorridors;
    }
    assert(!"unknown StyleId");
    return kSimpleRoomsAndCorridors;
}

bool parse_style_id(const std::string& debug_name, StyleId& out) {
    static const std::unordered_map<std::string, StyleId> kByName = {
        { "simple_rooms", StyleId::SimpleRoomsAndCorridors },
        // Future: { "precursor_ruin", StyleId::PrecursorRuin }, etc.
    };
    auto it = kByName.find(debug_name);
    if (it == kByName.end()) return false;
    out = it->second;
    return true;
}

} // namespace astra::dungeon
