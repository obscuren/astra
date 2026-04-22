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

const std::vector<RequiredFixture> kPrecursorRuinRequiredFixtures = {
    // L3 vault — plinth first so FlankPair entries resolve against it.
    { FixtureKind::Plinth,          PlacementSlot::SanctumCenter, {1,1}, depth_mask_bit(3) },
    { FixtureKind::ResonancePillar, PlacementSlot::FlankPair,     {2,2}, depth_mask_bit(3) },
    { FixtureKind::Brazier,         PlacementSlot::FlankPair,     {2,2}, depth_mask_bit(3) },

    // L2 chapels (1–2 altars per chapel).
    { FixtureKind::Altar,           PlacementSlot::ChapelCenter,  {1,2}, depth_mask_bit(2) },
    { FixtureKind::Brazier,         PlacementSlot::FlankPair,     {2,2}, depth_mask_bit(2) },

    // Structural pillars in nave + vault.
    { FixtureKind::Pillar,          PlacementSlot::EachRoomOnce,  {0,2},
      depth_mask_bit(2) | depth_mask_bit(3) },

    // Inscriptions, all depths.
    { FixtureKind::Inscription,     PlacementSlot::WallAttached,  {1,2},
      depth_mask_all(3) },
};

const DungeonStyle kPrecursorRuin = [] {
    DungeonStyle s;
    s.id                    = StyleId::PrecursorRuin;
    s.debug_name            = "precursor_ruin";
    s.backdrop_material     = "rock";
    s.layout                = LayoutKind::PrecursorVault;
    s.stairs_strategy       = StairsStrategy::EntryExitRooms;
    s.allowed_overlays      = { OverlayKind::BattleScarred, OverlayKind::Infested };
    s.decoration_pack       = "precursor_vault";
    s.connectivity_required = true;
    s.required_fixtures     = kPrecursorRuinRequiredFixtures;
    return s;
}();

} // namespace

const DungeonStyle& style_config(StyleId id) {
    switch (id) {
    case StyleId::SimpleRoomsAndCorridors: return kSimpleRoomsAndCorridors;
    case StyleId::PrecursorRuin:           return kPrecursorRuin;

    // Placeholders — follow-up slices register real configs.
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
        { "simple_rooms",   StyleId::SimpleRoomsAndCorridors },
        { "precursor_ruin", StyleId::PrecursorRuin },
        // Future: remaining styles registered as slices land.
    };
    auto it = kByName.find(debug_name);
    if (it == kByName.end()) return false;
    out = it->second;
    return true;
}

} // namespace astra::dungeon
