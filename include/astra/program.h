#pragma once

#include <cstdint>
#include <vector>

namespace astra {

enum class DeviceKind : uint8_t;

enum class ProgramKind : uint8_t {
    Atk,   // .exe — Grid combat (Plan 3)
    Stl,   // .exe — Stealth/cooldown (Plan 3)
    Utl,   // .exe — Utility (Plan 3)
    Qh,    // .qh — real-world quickhack (Plan 2 active)
};

const char* program_kind_name(ProgramKind k);
const char* program_kind_short(ProgramKind k);  // "ATK", "STL", "UTL", "QH"

// Stable per-program id. Used by effect dispatch and save format.
// Numeric values MUST match the temporary placeholder used in
// src/hackable.cpp before this task lands.
enum class ProgramId : uint16_t {
    IcebreakerLite = 1,
    GhostTrace     = 2,
    Cooldown       = 3,
    Breach         = 4,
    Decrypt        = 5,
    PulseHammer    = 200,    // Plan 4 — T3 ATK AoE
    DaemonHijack   = 201,    // Plan 4 — T3 UTL ICE charm
    RebootOptics   = 100,
    FriendlyFire   = 101,
    DataLeech      = 102,
};

struct ProgramDef {
    ProgramId           id;
    ProgramKind         kind;
    int                 tier        = 1;
    int                 ram_cost    = 1;
    int                 heat_cost   = 0;
    const char*         name        = "";          // canonical display name
    const char*         filename    = "";          // "icebreaker_lite.exe" / "reboot_optics.qh"
    const char*         description = "";
    int                 detection_cost = 1;        // QH only: amount added to zone Detection
    std::vector<DeviceKind> target_filter;         // QH only: which device_kinds it can target
};

const std::vector<ProgramDef>& program_registry();
const ProgramDef* find_program(ProgramId id);

// Per-Item payload — populated only when Item::type == ItemType::Program.
struct ProgramData {
    ProgramId id = ProgramId::IcebreakerLite;
};

} // namespace astra
