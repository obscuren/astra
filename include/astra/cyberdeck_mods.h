#pragma once

#include "astra/player.h"

namespace astra {

// Plan 7 §15 — Cyberdeck mod system (gate only in v1).
//
// The cyberdeck has installable mods that unlock new capabilities. v1 ships
// only the gate for one mod category — `WirelessJackIn` — used to gate
// `pda> jack <ip>` from the cyberdeck shell. The mod system itself (slots,
// install UI, tinkerer NPCs, mod balance) is deferred to Plan 11+.
//
// For v1: a mod is "installed" when its corresponding item is present in
// the player's inventory. No install ritual. When Plan 11 lands the proper
// mod system this rule is replaced with a real per-cyberdeck slot.
struct CyberdeckMods {
    // True if any item with item_def_id == ITEM_AEROJACK or ITEM_UNTETHER
    // sits in the player's inventory.
    static bool wireless_jackin_installed(const Player& player);
};

} // namespace astra
