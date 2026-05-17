#pragma once

// Turret netspace grammar — see docs/design/netspace.md § "Turret — hostile from frame one".
// Heavy ▓-walled arena with a JACK→AMMO→FRIEND vertical spine. Gray ICE is
// placed within kIceVisionRange of the jack-in tile so combat starts on turn 1.
// Two terminal nodes: TurretDisarm (AMMO) and TurretFlip (FRIEND).

#include "astra/netspace.h"

namespace astra {

Netspace gen_turret_netspace(const TargetDescriptor& desc);

} // namespace astra
