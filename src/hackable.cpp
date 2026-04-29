#include "astra/hackable.h"
#include "astra/program.h"

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
