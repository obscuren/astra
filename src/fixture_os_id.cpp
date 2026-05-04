#include "astra/fixture_os_id.h"

namespace astra {

const FixtureOsId& os_id_for(FixtureType f) {
    // Static tables, lifetime = process. Returned by const-ref.
    static const FixtureOsId k_door{"DOOR-OS",     "v2.0", "root", "DOOR",     "$"};
    static const FixtureOsId k_camera{"CAM-OS",    "v1.4", "root", "CAM",      "$"};
    static const FixtureOsId k_console{"CONS-OS",  "v3.1", "root", "CONS",     "$"};
    static const FixtureOsId k_terminal{"TERM-OS", "v3.1", "root", "TERM",     "$"};
    static const FixtureOsId k_vending{"VENDOTRON","v1.4", "root", "VEND",     "$"};
    static const FixtureOsId k_locker{"LOCK-OS",   "v1.0", "root", "LOCKER",   "$"};
    static const FixtureOsId k_power{"PWR-OS",     "v0.9", "root", "PWR",      "$"};
    static const FixtureOsId k_default{"DEV-OS",   "v1.0", "root", "device",   "$"};

    switch (f) {
        case FixtureType::Door:
        case FixtureType::Gate:
            return k_door;
        case FixtureType::HealPod:
            return k_terminal;
        case FixtureType::FoodTerminal:
        case FixtureType::WeaponDisplay:
            return k_vending;
        case FixtureType::SupplyLocker:
        case FixtureType::Locker:
            return k_locker;
        case FixtureType::Console:
        case FixtureType::CommandTerminal:
        case FixtureType::DataTerminal:
        case FixtureType::ShipTerminal:
        case FixtureType::StarChart:
        case FixtureType::StarChartL:
        case FixtureType::StarChartR:
            return k_console;
        case FixtureType::Conduit:
        case FixtureType::Lamp:
        case FixtureType::HoloLight:
            return k_power;
        default:
            return k_default;
    }
    // For Phase B we'll explicitly add CAM-OS for Camera fixtures, TUR-OS
    // for turret-equivalents, etc. The Camera entry is reserved here so the
    // table is visibly complete — it's used by NPC implants below.
    (void)k_camera;
}

const FixtureOsId& os_id_for_implant() {
    // NPC implants currently apply Electronic | Mobile | Weaponized (turret-y).
    static const FixtureOsId k_turret{"TUR-OS", "v2.7", "root", "TUR", "$"};
    return k_turret;
}

} // namespace astra
