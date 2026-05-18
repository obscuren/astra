#pragma once

// Combat-arena grammar — dev-only test bench (`:jack combat`).
// A JACK hub with four pipes of distinct drawn lengths radiating to
// White / Gray-pack / Black / VAULT(breakwall) stations. See
// .claude/specs/netspace-combat-arena-spec.md.
//
//          [ GRAY ]                north  : mid  pipe -> seg 4
//             |
//  [VAULT]══[ JACK ]══[ WHITE ]    west/east : wall/short
//             |
//          [ BLACK ]               south : long pipe -> seg 6

#include "astra/netspace.h"

namespace astra {

Netspace gen_combat_netspace(const TargetDescriptor& desc);

}  // namespace astra
