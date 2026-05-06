#pragma once

#include <cstdint>
#include <vector>
#include "astra/hackable.h"
#include "astra/telegraph.h"

namespace astra {

// Forward declaration to keep program.h independent of grid_session.h.
struct GridSession;

enum class TargetingMode : uint8_t {
    Self,   // fires immediately, no cursor
    Tile,   // opens Telegraph, on_confirm validates valid_target predicate
};

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

    // Spec 1 §5.2 — Mark-interaction Sigils (E2)
    Echo           = 300,
    Lull           = 301,
    Veil           = 302,
    Falter         = 303,
    Shroud         = 304,
    Wither         = 305,
    Snuff          = 306,
    Fester         = 307,
    Lance          = 308,
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
    std::vector<TagSet> target_filter;             // QH only: which tag sets it can target (AND within, OR across)

    // Plan 6: in-Grid targeting metadata. For Self programs, telegraph_spec
    // and valid_target are unused. For Tile programs, telegraph_spec drives
    // the cursor + preview; valid_target is checked on confirm and a
    // [ERR] line is logged on miss.
    TargetingMode       targeting   = TargetingMode::Self;
    TelegraphSpec       telegraph_spec;
    bool (*valid_target)(const GridSession&, int x, int y) = nullptr;
    bool                requires_adjacency = false;  // E1: if true, Sigil must be adjacent to target tile
};

const std::vector<ProgramDef>& program_registry();
const ProgramDef* find_program(ProgramId id);

// Per-Item payload — populated only when Item::type == ItemType::Program.
struct ProgramData {
    ProgramId id = ProgramId::IcebreakerLite;
};

} // namespace astra
