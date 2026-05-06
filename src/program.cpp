#include "astra/program.h"
#include "astra/grid_ice.h"
#include "astra/grid_session.h"
#include "astra/hackable.h"

namespace astra {

const char* program_kind_name(ProgramKind k) {
    switch (k) {
        case ProgramKind::Atk: return "Attack";
        case ProgramKind::Stl: return "Stealth";
        case ProgramKind::Utl: return "Utility";
        case ProgramKind::Qh:  return "Quickhack";
    }
    return "?";
}

const char* program_kind_short(ProgramKind k) {
    switch (k) {
        case ProgramKind::Atk: return "ATK";
        case ProgramKind::Stl: return "STL";
        case ProgramKind::Utl: return "UTL";
        case ProgramKind::Qh:  return "QH";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// Plan 6 valid_target predicates — checked on Telegraph confirm.
// ---------------------------------------------------------------------------

namespace {

bool icebreaker_valid_target(const GridSession& s, int x, int y) {
    for (const auto& ice : s.ice) {
        if (ice.hp > 0 && ice.x == x && ice.y == y) return true;
    }
    return false;
}

bool breach_valid_target(const GridSession& s, int x, int y) {
    if (!s.sector.in_bounds(x, y)) return false;
    GridTile t = s.sector.at(x, y);
    // breach.exe targets locked Doors (the bridge tile between rooms),
    // and the deep-grid gateway. Firewall is retained for hand-authored
    // sectors that still stamp impassable walls.
    if (t == GridTile::Door) return s.sector.is_locked_door(x, y);
    return t == GridTile::Firewall ||
           t == GridTile::DeepGridGateway;
}

bool decrypt_valid_target(const GridSession& s, int x, int y) {
    if (!s.sector.in_bounds(x, y)) return false;
    return s.sector.at(x, y) == GridTile::EncryptedFile;
}

bool pulse_hammer_valid_target(const GridSession& s, int x, int y) {
    if (!s.sector.in_bounds(x, y)) return false;
    return s.sector.passable(x, y);
}

// daemon_hijack and icebreaker share the "any live ICE" predicate.

TelegraphSpec burst_at(int range) {
    TelegraphSpec s;
    s.shape = TelegraphShape::Burst;
    s.range = range;
    s.width = 0;
    s.stop_at_wall = false;
    s.require_walkable_dest = false;
    return s;
}

TelegraphSpec burst_aoe(int range, int width) {
    TelegraphSpec s;
    s.shape = TelegraphShape::Burst;
    s.range = range;
    s.width = width;
    s.stop_at_wall = false;
    s.require_walkable_dest = true;
    return s;
}

} // namespace

const std::vector<ProgramDef>& program_registry() {
    using H = HackTag;
    using TM = TargetingMode;
    static const std::vector<ProgramDef> regs = {
        // --- ATK / STL / UTL — fired in-Grid; Plan 6 wires real targeting ---
        { ProgramId::IcebreakerLite, ProgramKind::Atk, 1, 2, 2, "Icebreaker Lite", "icebreaker_lite.exe",
          "Light cracker for white ICE. Tile-targeted within 4 cells.", 0, {},
          TM::Tile, burst_at(4), &icebreaker_valid_target },
        { ProgramId::GhostTrace,     ProgramKind::Stl, 1, 3, 0, "Ghost Trace",     "ghost_trace.exe",
          "Sheds 3 Trace and cloaks you from white ICE for 3 turns.", 0, {},
          TM::Self, {}, nullptr },
        { ProgramId::Cooldown,       ProgramKind::Stl, 1, 2, 0, "Cooldown",        "cooldown.exe",
          "Drops the equipped deck's Heat by 4.", 0, {},
          TM::Self, {}, nullptr },
        { ProgramId::Breach,         ProgramKind::Utl, 1, 3, 3, "Breach",          "breach.exe",
          "Burns one firewall tile or cracks one gateway under your cursor.", 0, {},
          TM::Tile, burst_at(1), &breach_valid_target },
        { ProgramId::Decrypt,        ProgramKind::Utl, 1, 2, 1, "Decrypt",         "decrypt.exe",
          "Reads one encrypted file under your cursor.", 0, {},
          TM::Tile, burst_at(1), &decrypt_valid_target },
        { ProgramId::PulseHammer,    ProgramKind::Atk, 3, 4, 5, "Pulse Hammer",    "pulse_hammer.exe",
          "AoE 1d6 dmg to all ICE in a 3×3 around the target tile.", 0, {},
          TM::Tile, burst_aoe(4, 1), &pulse_hammer_valid_target },
        { ProgramId::DaemonHijack,   ProgramKind::Utl, 3, 5, 4, "Daemon Hijack",   "daemon_hijack.exe",
          "Take control of one ICE for 3 turns.", 0, {},
          TM::Tile, burst_at(4), &icebreaker_valid_target },

        // --- QH — out-of-Grid Plan 2 layer; targeting handled by world dialog. ---
        { ProgramId::RebootOptics,   ProgramKind::Qh, 1, 1, 0, "Reboot Optics",    "reboot_optics.qh",
          "Soft-reboots a camera or turret's optics. Blinded for 4 turns.",
          1, { static_cast<TagSet>(H::HasOptics) },
          TM::Self, {}, nullptr },
        { ProgramId::FriendlyFire,   ProgramKind::Qh, 2, 3, 0, "Friendly Fire",    "friendly_fire.qh",
          "Re-targets a mobile weapon platform onto its allies for 2 turns.",
          3, { H::Weaponized | H::Mobile },
          TM::Self, {}, nullptr },
        { ProgramId::DataLeech,      ProgramKind::Qh, 1, 2, 0, "Data Leech",       "data_leech.qh",
          "Drains a packet of operational data from a hackable.",
          2, { static_cast<TagSet>(H::DataStore) },
          TM::Self, {}, nullptr },

        // --- Spec 1 §5.2 — Mark-interaction Sigils (E2) ---
        { ProgramId::Echo,   ProgramKind::Utl, 1, 1, 1,  "Echo",   "echo.exe",
          "Refreshes or propagates a Mark on the target tile.", 0, {},
          TM::Tile, burst_at(4), nullptr },
        { ProgramId::Lull,   ProgramKind::Utl, 1, 2, 2,  "Lull",   "lull.exe",
          "Suppresses a target's reaction — delays ICE activation.", 0, {},
          TM::Tile, burst_at(4), nullptr },
        { ProgramId::Veil,   ProgramKind::Utl, 1, 2, 3,  "Veil",   "veil.exe",
          "Cloaks a Marked tile, hiding it from ICE detection routines.", 0, {},
          TM::Tile, burst_at(4), nullptr },
        { ProgramId::Falter, ProgramKind::Utl, 2, 4, 5,  "Falter", "falter.exe",
          "Disrupts a target's logic loop, causing it to skip its next action.", 0, {},
          TM::Tile, burst_at(4), nullptr },
        { ProgramId::Shroud, ProgramKind::Utl, 2, 5, 6,  "Shroud", "shroud.exe",
          "Wraps a Marked zone in noise, reducing trace accumulation.", 0, {},
          TM::Tile, burst_at(4), nullptr },
        { ProgramId::Wither, ProgramKind::Atk, 2, 3, 4,  "Wither", "wither.exe",
          "Degrades a Marked target's defences over successive turns.", 0, {},
          TM::Tile, burst_at(4), nullptr },
        { ProgramId::Snuff,  ProgramKind::Atk, 3, 8, 10, "Snuff",  "snuff.exe",
          "Instantly kills a Marked target at close range.", 0, {},
          TM::Tile, burst_at(1), nullptr, true },
        { ProgramId::Fester, ProgramKind::Atk, 3, 5, 6,  "Fester", "fester.exe",
          "Infects an adjacent Marked target with a damage-over-time payload.", 0, {},
          TM::Tile, burst_at(1), nullptr, true },
        { ProgramId::Lance,  ProgramKind::Atk, 3, 7, 8,  "Lance",  "lance.exe",
          "Drives a focused data spike through an adjacent Marked target.", 0, {},
          TM::Tile, burst_at(1), nullptr, true },
    };
    return regs;
}

const ProgramDef* find_program(ProgramId id) {
    for (const auto& p : program_registry())
        if (p.id == id) return &p;
    return nullptr;
}

} // namespace astra
