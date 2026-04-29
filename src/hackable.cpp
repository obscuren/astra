#include "astra/hackable.h"

// Task 4 will replace this temporary forward-declared ProgramId block
// with a real `#include "astra/program.h"`. The placeholder lets Task 2
// commit cleanly before Task 4 lands.
namespace astra {
enum class ProgramId : uint16_t {
    IcebreakerLite = 1,
    GhostTrace     = 2,
    Cooldown       = 3,
    Breach         = 4,
    Decrypt        = 5,
    RebootOptics   = 100,
    FriendlyFire   = 101,
    DataLeech      = 102,
};
}

namespace astra {

const char* device_kind_name(DeviceKind k) {
    switch (k) {
        case DeviceKind::Turret:           return "Turret";
        case DeviceKind::Camera:           return "Camera";
        case DeviceKind::Door:             return "Door";
        case DeviceKind::PowerConduit:     return "Power Conduit";
        case DeviceKind::PrecursorConsole: return "Precursor Console";
    }
    return "?";
}

Hackable make_hackable(DeviceKind kind, int tier) {
    Hackable h;
    h.device_kind = kind;
    h.security_tier = tier;
    switch (kind) {
        case DeviceKind::Turret:
            h.available_qh = { ProgramId::RebootOptics, ProgramId::FriendlyFire };
            break;
        case DeviceKind::Camera:
            h.available_qh = { ProgramId::RebootOptics };
            break;
        case DeviceKind::Door:
            h.available_qh = { /* future: bypass_lock — Plan 3 */ };
            break;
        case DeviceKind::PowerConduit:
            h.available_qh = { /* future: blackout — Plan 3 */ };
            break;
        case DeviceKind::PrecursorConsole:
            h.available_qh = {}; // jack-in only
            break;
    }
    return h;
}

} // namespace astra
