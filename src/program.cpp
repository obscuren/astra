#include "astra/program.h"
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

const std::vector<ProgramDef>& program_registry() {
    using K = ProgramKind;
    using D = DeviceKind;
    static const std::vector<ProgramDef> regs = {
        // ATK / STL / UTL — placeholders for Plan 3 (no effects in Plan 2)
        { ProgramId::IcebreakerLite, K::Atk, 1, 2, 2, "Icebreaker Lite", "icebreaker_lite.exe",
          "Light cracker for white ICE. (Used in the Grid — Plan 3.)", 0, {} },
        { ProgramId::GhostTrace,     K::Stl, 1, 3, 0, "Ghost Trace",     "ghost_trace.exe",
          "Sheds Trace and hides you from white ICE briefly. (Plan 3.)", 0, {} },
        { ProgramId::Cooldown,       K::Stl, 1, 2, 0, "Cooldown",        "cooldown.exe",
          "Drops Heat by 4. (Plan 3.)", 0, {} },
        { ProgramId::Breach,         K::Utl, 1, 3, 3, "Breach",          "breach.exe",
          "Burns one firewall tile or one gateway lock level. (Plan 3.)", 0, {} },
        { ProgramId::Decrypt,        K::Utl, 1, 2, 1, "Decrypt",         "decrypt.exe",
          "Reads one encrypted file. (Plan 3.)", 0, {} },
        { ProgramId::PulseHammer,    K::Atk, 3, 4, 5, "Pulse Hammer",    "pulse_hammer.exe",
          "AoE 1d6 dmg to all ICE adjacent to target tile.", 0, {} },
        { ProgramId::DaemonHijack,   K::Utl, 3, 5, 4, "Daemon Hijack",   "daemon_hijack.exe",
          "Take control of one ICE for 3 turns.", 0, {} },

        // QH — the active Plan 2 layer.
        { ProgramId::RebootOptics,   K::Qh, 1, 1, 0, "Reboot Optics",    "reboot_optics.qh",
          "Soft-reboots a camera or turret's optics. Blinded for 4 turns.",
          1, { D::Camera, D::Turret } },
        { ProgramId::FriendlyFire,   K::Qh, 2, 3, 0, "Friendly Fire",    "friendly_fire.qh",
          "Re-targets a turret onto its allies for 2 turns.",
          3, { D::Turret } },
        { ProgramId::DataLeech,      K::Qh, 1, 2, 0, "Data Leech",       "data_leech.qh",
          "Drains a packet of operational data from a hackable.",
          2, { D::Camera, D::PowerConduit, D::PrecursorConsole } },
    };
    return regs;
}

const ProgramDef* find_program(ProgramId id) {
    for (const auto& p : program_registry())
        if (p.id == id) return &p;
    return nullptr;
}

} // namespace astra
