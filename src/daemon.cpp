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
    // ── S7c.2: ATM ──────────────────────────────────────────────────────
    // VaultFw — VAULT breakwall (RoomFill, mirrors LOCK shape).
    {
        /* name              */ "VAULT",
        /* cast_prog_name    */ "VAULT.fw",
        /* glyph             */ "",        // unused for RoomFill
        /* color             */ net_theme::gray_ice,
        /* archetype         */ IceColor::Gray,
        /* base_hp           */ 4,
        /* windup_beats      */ 5,
        /* cast_damage       */ 1,
        /* cast_radius       */ 0,
        /* is_boss           */ false,
        /* render_style      */ DaemonRenderStyle::RoomFill,
    },
    // TellrK9 — vault enforcer (Glyph ◈ Yellow, boss-class).
    {
        /* name              */ "TELLR",
        /* cast_prog_name    */ "TELLR.K9",
        /* glyph             */ "\xe2\x97\x88",   // ◈
        /* color             */ Color::Yellow,
        /* archetype         */ IceColor::Gray,
        /* base_hp           */ 12,
        /* windup_beats      */ 6,
        /* cast_damage       */ 2,
        /* cast_radius       */ 0,
        /* is_boss           */ true,
        /* render_style      */ DaemonRenderStyle::Glyph,
    },
    // FraudExe — FRAUD trigger spawn (archetype-glyph fallback).
    {
        /* name              */ "FRAUD",
        /* cast_prog_name    */ "FRAUD.exe",
        /* glyph             */ "",        // fallback: archetype ◇
        /* color             */ net_theme::gray_ice,
        /* archetype         */ IceColor::Gray,
        /* base_hp           */ 2,
        /* windup_beats      */ 4,
        /* cast_damage       */ 1,
        /* cast_radius       */ 0,
        /* is_boss           */ false,
        /* render_style      */ DaemonRenderStyle::Glyph,
    },
    // PktDat — PACKETS trigger swarm (· glyph, BrightYellow, cheap).
    {
        /* name              */ "PKT",
        /* cast_prog_name    */ "PKT.dat",
        /* glyph             */ "\xc2\xb7",       // ·
        /* color             */ Color::BrightYellow,
        /* archetype         */ IceColor::Gray,
        /* base_hp           */ 1,
        /* windup_beats      */ 3,
        /* cast_damage       */ 1,
        /* cast_radius       */ 0,
        /* is_boss           */ false,
        /* render_style      */ DaemonRenderStyle::Glyph,
    },
    // ── S7c.2: CAMERA ───────────────────────────────────────────────────
    // LensCam — per-lens scanner (◎ bullseye, Cyan, very weak).
    {
        /* name              */ "LENS",
        /* cast_prog_name    */ "LENS.cam",
        /* glyph             */ "\xe2\x97\x8e",   // ◎
        /* color             */ Color::Cyan,
        /* archetype         */ IceColor::Gray,
        /* base_hp           */ 1,
        /* windup_beats      */ 4,
        /* cast_damage       */ 1,
        /* cast_radius       */ 0,
        /* is_boss           */ false,
        /* render_style      */ DaemonRenderStyle::Glyph,
    },
    // ArchiveK9 — archive enforcer (▤ filing cabinet, Yellow, boss).
    {
        /* name              */ "ARCHIVE",
        /* cast_prog_name    */ "ARCHIVE.K9",
        /* glyph             */ "\xe2\x96\xa4",   // ▤
        /* color             */ Color::Yellow,
        /* archetype         */ IceColor::Gray,
        /* base_hp           */ 10,
        /* windup_beats      */ 5,
        /* cast_damage       */ 2,
        /* cast_radius       */ 0,
        /* is_boss           */ true,
        /* render_style      */ DaemonRenderStyle::Glyph,
    },
    // ── S7c.2: CORPSE ───────────────────────────────────────────────────
    // MemryKex — MEMORY corruption (RoomFill over the existing ▓ blob).
    {
        /* name              */ "MEMRY",
        /* cast_prog_name    */ "MEMRY.kex",
        /* glyph             */ "",        // unused for RoomFill
        /* color             */ net_theme::wall_heavy,   // BrightMagenta
        /* archetype         */ IceColor::Gray,
        /* base_hp           */ 6,
        /* windup_beats      */ 5,
        /* cast_damage       */ 1,
        /* cast_radius       */ 0,
        /* is_boss           */ false,
        /* render_style      */ DaemonRenderStyle::RoomFill,
    },
    // ── S7c.2: ELEVATOR ─────────────────────────────────────────────────
    // FloorK9 — per-floor patrol (archetype-glyph fallback, baseline stats).
    {
        /* name              */ "FLOOR",
        /* cast_prog_name    */ "FLOOR.K9",
        /* glyph             */ "",        // fallback: archetype ◇
        /* color             */ net_theme::gray_ice,
        /* archetype         */ IceColor::Gray,
        /* base_hp           */ 2,
        /* windup_beats      */ 4,
        /* cast_damage       */ 1,
        /* cast_radius       */ 0,
        /* is_boss           */ false,
        /* render_style      */ DaemonRenderStyle::Glyph,
    },
    // ScrtyFw — SECURITY enforcer on the floor ABOVE the spine gate.
    // The daemon IS the gate: its death flips the gap tile passable
    // via the gate_tile_x/y hook in tick_grid. (S7d: migrated to
    // Glyph render style; single-cell density render retired.)
    {
        /* name              */ "SCRTY",
        /* cast_prog_name    */ "SCRTY.fw",
        /* glyph             */ "\xe2\x96\x92",   // ▒ — medium-density block, reads as "encrypted firewall"
        /* color             */ Color::Yellow,
        /* archetype         */ IceColor::Gray,
        /* base_hp           */ 5,
        /* windup_beats      */ 5,
        /* cast_damage       */ 1,
        /* cast_radius       */ 0,
        /* is_boss           */ false,
        /* render_style      */ DaemonRenderStyle::Glyph,
    },
    // HouseK9 — penthouse enforcer (⌂ house glyph, Yellow, boss).
    {
        /* name              */ "HOUSE",
        /* cast_prog_name    */ "HOUSE.K9",
        /* glyph             */ "\xe2\x8c\x82",   // ⌂
        /* color             */ Color::Yellow,
        /* archetype         */ IceColor::Gray,
        /* base_hp           */ 14,
        /* windup_beats      */ 6,
        /* cast_damage       */ 2,
        /* cast_radius       */ 0,
        /* is_boss           */ true,
        /* render_style      */ DaemonRenderStyle::Glyph,
    },
    // ── S7c.2: TURRET ───────────────────────────────────────────────────
    // TrrtDat — corridor mook (◆ solid diamond, Red, fast windup).
    {
        /* name              */ "TRRT",
        /* cast_prog_name    */ "TRRT.dat",
        /* glyph             */ "\xe2\x97\x86",   // ◆
        /* color             */ Color::Red,
        /* archetype         */ IceColor::Gray,
        /* base_hp           */ 2,
        /* windup_beats      */ 3,       // FAST — TURRET reads twitchier
        /* cast_damage       */ 1,
        /* cast_radius       */ 0,
        /* is_boss           */ false,
        /* render_style      */ DaemonRenderStyle::Glyph,
    },
    // ── S7c.2: VENDING ──────────────────────────────────────────────────
    // LolBin — ultra-rare easter egg (Φ phi, BrightYellow, 0 damage).
    {
        /* name              */ "LOL",
        /* cast_prog_name    */ "LOL.bin",
        /* glyph             */ "\xce\xa6",       // Φ
        /* color             */ Color::BrightYellow,
        /* archetype         */ IceColor::Gray,
        /* base_hp           */ 1,
        /* windup_beats      */ 8,
        /* cast_damage       */ 0,
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
