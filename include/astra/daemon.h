#pragma once
// Phase 5 S7c.1: typed daemon kinds layered on top of IceColor archetype.
//
// DaemonKind specifies a daemon's thematic + statistical identity (name,
// glyph, color, base HP, cast stats, render style). The kind drives
// COSMETIC + STATISTICAL specialization; the underlying behavior path
// is still selected by IceColor (Gray = caster, White = patrol, Black =
// walker). No new behavior code paths are introduced by S7c.1 -- only
// data variation.
//
// New daemon kinds get added here per grammar slice. S7c.1 ships
// Watchdog (legacy default, equivalent to the S6 hardcoded Gray) +
// Lock + Bolt (door grammar). Other grammars (atm/camera/corpse/
// elevator/turret/vending) add their daemons in future slices.

#include "astra/net_ice.h"          // IceColor + DaemonKind enum
#include "astra/renderer.h"         // Color
#include <cstdint>

namespace astra {

enum class DaemonRenderStyle : std::uint8_t {
    Glyph,           // single-cell glyph at (ice.x, ice.y)
    RoomFill,        // density gradient across the room interior top row
};

struct DaemonDef {
    const char*       name;             // "WATCHDOG" / "LOCK" / "BOLT"
    const char*       cast_prog_name;   // "GRAY.exe" / "LOCK.fw" / "BOLT.T9"
    const char*       glyph;            // single-glyph display; used only for render_style==Glyph
    Color             color;
    IceColor          archetype;
    int               base_hp;
    int               windup_beats;     // pre-cast windup (Gray archetype only)
    int               cast_damage;
    int               cast_radius;
    bool              is_boss;          // reserved; not read in S7c.1
    DaemonRenderStyle render_style;
};

// Static table lookup. Never returns null. Out-of-range kind falls back
// to Watchdog so any future enum additions in flight don't crash.
const DaemonDef& daemon_def(DaemonKind k);

}  // namespace astra
