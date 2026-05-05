#pragma once

#include "astra/grid_ice.h"
#include "astra/grid_network.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace astra {

// FixtureType is defined in tilemap.h. Forward-declare here to keep grid_sector.h
// independent of the (large) tilemap.h header — the field below is a by-value
// enum, fine with a forward declaration as long as users include tilemap.h.
enum class FixtureType : uint8_t;

enum class GridTile : uint8_t {
    Floor,            // .
    Firewall,         // # (impassable, breachable)
    DataNode,         // $
    ExitNode,         // X (jack-out)
    EncryptedFile,    // ?
    Wall,             // outside-of-sector (v1 deep-grid structural ring)
    Connector,           // Plan 5 Cut 2: visible bus-trace wiring (DarkGray ═║...)
    DeepGridGateway,     // Plan 5 Cut 2: connected-LAN ⊕ portal to deep-Grid (BrightCyan)
    WarpAnchor,          // Plan 5 Cut 3: Atlas warp tile (BrightWhite ◉)
    DeviceAvatar,        // Plan 5 Cut 2.6: wall-mounted device representation in subnet sector
    Door,                // Plan 8: bridge tile between rooms (open or locked variant)
    Void,                // Plan 8: out-of-room background for v2 sectors; renders as nothing
};

struct GridSector {
    int                   w = 0;
    int                   h = 0;
    std::vector<GridTile> tiles;       // size w*h, row-major
    int                   spawn_x = 0;
    int                   spawn_y = 0;
    GridNodeId            source_node;  // which network node this sector belongs to

    // Plan 5 Cut 2.6: source FixtureType for subnet sectors. Drives the
    // wall-mounted device-avatar glyph (camera ▤, door ║, healpod ⊞, ...).
    // Default to value 0 — subnet generators stamp the actual type.
    FixtureType source_fixture_type = static_cast<FixtureType>(0);

    // Plan 5 Cut 2 (PairHash still used for locked_doors, avatar_fixture_type).
    struct PairHash {
        size_t operator()(const std::pair<int,int>& p) const noexcept {
            return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
        }
    };

    // Plan 8: per-subnet spawn point. When the player jacks into a specific
    // subnet's GridNodeId, they spawn at this (x,y) inside that subnet's
    // room. Lookup falls back to (spawn_x, spawn_y) — the lobby — if missing.
    struct GridNodeIdHash {
        size_t operator()(GridNodeId id) const noexcept {
            return std::hash<uint32_t>()(id.value);
        }
    };
    std::unordered_map<GridNodeId, std::pair<int,int>, GridNodeIdHash> per_node_spawn;

    // Plan 8: which Door tiles are currently locked. Cracked doors are removed.
    // A tile at (x,y) is in this set iff GridTile::Door + locked.
    std::unordered_set<std::pair<int,int>, PairHash> locked_doors;

    // Plan 8 Cut 4: per-tile source FixtureType for DeviceAvatar glyph lookup.
    // Renderer falls back to source_fixture_type if a tile isn't in this map.
    std::unordered_map<std::pair<int,int>, FixtureType, PairHash> avatar_fixture_type;

    // Plan 8 Cut 5: per-room ICE seed records. Generator populates from room
    // templates; hacking session spawns GridIce instances at start.
    struct IceSeedRecord {
        int      x;
        int      y;
        IceColor color;
        int      hp;
        int      min_trace = 0;  // Plan 8: trace % at which this ICE appears (0 = immediately)
    };
    std::vector<IceSeedRecord> ice_seeds;

    // Plan 8 Cut 7: Single deep-grid gateway destination. Replaces the
    // gateway_target map (which carried per-tile Subnet destinations under
    // v1 — that idiom is gone). At most one ⊕ tile per sector; this field
    // holds its target. Empty (default-constructed) means no ⊕ stamped.
    GridNodeId deep_grid_destination;

    // Plan 8 Cut 8: per-zone bounding box for HUD overlay rendering.
    // Populated by generate_lan_sector_v2 after rooms are placed; consumed
    // by the zone overlay render layer.
    struct ZoneBox {
        int         x, y, w, h;   // sector-coord bounding box (union of zone's rooms)
        int         tier;          // 1, 2, or 3
        std::string banner;        // pulled from LanZone.name; "ZONE" if empty
    };
    std::vector<ZoneBox> zone_boxes;

    // Plan 8: per-room rect for renderer header lookup. The HUD shows the
    // device hostname for the room the player is currently inside; on a
    // bridge / corridor / outside-room cell, only the LAN is shown.
    // Lobby is included so the header reads "LOBBY" inside the central hub.
    struct SubnetRoom {
        int        x, y, w, h;     // sector-coord bounding box
        GridNodeId subnet;          // empty for lobby
        bool       is_lobby = false;
    };
    std::vector<SubnetRoom> subnet_rooms;

    // Returns the SubnetRoom containing (x, y), or nullptr if the cell is
    // outside any room (corridor, bridge, void).
    const SubnetRoom* room_at(int x, int y) const;

    GridTile at(int x, int y) const;
    void     set(int x, int y, GridTile t);
    bool     in_bounds(int x, int y) const;
    bool     passable(int x, int y) const; // floor + walked-through gateway/exit tiles
    bool     is_locked_door(int x, int y) const;
    void     unlock_door(int x, int y);    // removes (x,y) from locked_doors
};

// Procedural generators. Same seed -> same layout (stable revisits).
GridSector gen_regional_sector(uint32_t seed, int security_tier);
// Hand-authored -- see grid_anchor_layout.cpp.
GridSector make_consciousness_anchor_sector();
GridSector make_player_deep_grid_base();

} // namespace astra
