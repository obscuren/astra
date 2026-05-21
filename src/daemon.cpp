#include "astra/daemon.h"

#include "astra/net_theme.h"

namespace astra {

namespace {

// Static table indexed by DaemonKind enum value. Order MUST match the
// enum. New entries get appended to BOTH this table and the enum in
// daemon.h.
const DaemonDef kDaemonTable[] = {
    // Watchdog -- legacy default, equivalent to the S6 hardcoded Gray.
    // Empty glyph leaves the renderer on the IceColor-archetype fallback
    // path (preserves White ▼ / Gray ◇ / Black ▲ glyphs + colors for
    // any pre-S7c.1 ICE that defaults kind=Watchdog).
    {
        /* name              */ "WATCHDOG",
        /* cast_prog_name    */ "GRAY.exe",
        /* glyph             */ "",
        /* color             */ net_theme::gray_ice,
        /* archetype         */ IceColor::Gray,
        /* base_hp           */ 2,
        /* windup_beats      */ 4,        // == kIceGrayWindupBeats
        /* cast_damage       */ 1,        // == kIceGrayCastDamage
        /* cast_radius       */ 0,        // == kIceGrayCastRadius
        /* is_boss           */ false,
        /* render_style      */ DaemonRenderStyle::Glyph,
    },
    // Lock -- door grammar's room-fill defender.
    {
        /* name              */ "LOCK",
        /* cast_prog_name    */ "LOCK.fw",
        /* glyph             */ "",        // unused for RoomFill
        /* color             */ net_theme::gray_ice,   // DarkGray (encryption-class wall)
        /* archetype         */ IceColor::Gray,
        /* base_hp           */ 4,
        /* windup_beats      */ 5,
        /* cast_damage       */ 1,
        /* cast_radius       */ 0,
        /* is_boss           */ false,
        /* render_style      */ DaemonRenderStyle::RoomFill,
    },
    // Bolt -- door grammar's micro-boss.
    {
        /* name              */ "BOLT",
        /* cast_prog_name    */ "BOLT.T9",
        /* glyph             */ "\xe2\x96\xa3",  // ▣
        /* color             */ Color::Yellow,
        /* archetype         */ IceColor::Gray,
        /* base_hp           */ 12,
        /* windup_beats      */ 6,
        /* cast_damage       */ 2,
        /* cast_radius       */ 0,
        /* is_boss           */ true,
        /* render_style      */ DaemonRenderStyle::Glyph,
    },
};

constexpr std::size_t kDaemonTableSize =
    sizeof(kDaemonTable) / sizeof(kDaemonTable[0]);

}  // namespace

const DaemonDef& daemon_def(DaemonKind k) {
    const auto i = static_cast<std::size_t>(k);
    if (i >= kDaemonTableSize) return kDaemonTable[0];   // fallback: Watchdog
    return kDaemonTable[i];
}

}  // namespace astra
