#pragma once
// Phase 5 S7c.2: shared daemon-seeding helpers, promoted from
// gen_door_netspace.cpp's local namespace so the six remaining grammars
// (ATM / CAMERA / CORPSE / ELEVATOR / TURRET / VENDING) can reuse the
// same construction. Inline so each TU compiles its own copy without
// adding a new .cpp.

#include "astra/daemon.h"
#include "astra/net_ice.h"
#include "astra/net_pipe_path.h"        // room_index_at
#include "astra/net_room.h"             // NetRoom
#include "astra/netspace_layout.h"      // NetspaceBuilder

namespace astra {

// Construct an Ice for the given DaemonKind, position it at the room's
// interior top-center, apply tier-scaled overrides on top of the def
// baseline, set home_room_idx, and push it into b.ns.initial_ice.
// hp_override is REQUIRED (writes both hp and hp_max). The override
// fields go to zero to mean "use def baseline" downstream in
// ice_cast_tick.
inline void seed_daemon(NetspaceBuilder& b, const NetRoom& room,
                        DaemonKind kind, int hp_override,
                        int windup_override, int cast_dmg_override) {
    Ice ic;
    ic.x = room.x + room.w / 2;
    ic.y = room.y + 1;
    ic.kind = kind;
    const DaemonDef& def = daemon_def(kind);
    ic.color = def.archetype;
    ic.hp     = hp_override;
    ic.hp_max = hp_override;
    ic.windup_override      = windup_override;
    ic.cast_damage_override = cast_dmg_override;
    ic.home_room_idx = room_index_at(b.ns, ic.x, ic.y);
    b.ns.initial_ice.push_back(ic);
}

// Sibling helper for single-tile daemon placement (no room lookup).
// ELEVATOR's SCRTY.fw uses this -- the SECURITY breakwall tile lives
// in the spine pipe gap, not inside a NetRoom. home_room_idx falls
// back to -1 if (x,y) isn't inside any room.
inline void seed_daemon_at(NetspaceBuilder& b, int x, int y,
                           DaemonKind kind, int hp_override,
                           int windup_override, int cast_dmg_override) {
    Ice ic;
    ic.x = x;
    ic.y = y;
    ic.kind = kind;
    const DaemonDef& def = daemon_def(kind);
    ic.color = def.archetype;
    ic.hp     = hp_override;
    ic.hp_max = hp_override;
    ic.windup_override      = windup_override;
    ic.cast_damage_override = cast_dmg_override;
    ic.home_room_idx = room_index_at(b.ns, x, y);   // -1 if no room
    b.ns.initial_ice.push_back(ic);
}

// Seed a daemon at an arbitrary cell INSIDE a specific room (for
// grammars where the interior top-center isn't where the daemon belongs
// — e.g., ARCHIVE.K9 wants the room's middle row, not its top row).
// `room` parameter is semantic-only (caller asserts (x,y) is inside it);
// home_room_idx is computed from room_index_at(b.ns, x, y).
inline void seed_daemon_in_room_at(NetspaceBuilder& b, const NetRoom& room,
                                   int x, int y, DaemonKind kind,
                                   int hp_override, int windup_override,
                                   int cast_dmg_override) {
    (void)room;   // semantic: caller asserts (x,y) is inside `room`
    Ice ic;
    ic.x = x;
    ic.y = y;
    ic.kind = kind;
    const DaemonDef& def = daemon_def(kind);
    ic.color = def.archetype;
    ic.hp     = hp_override;
    ic.hp_max = hp_override;
    ic.windup_override      = windup_override;
    ic.cast_damage_override = cast_dmg_override;
    ic.home_room_idx = room_index_at(b.ns, x, y);
    b.ns.initial_ice.push_back(ic);
}

}  // namespace astra
