#pragma once
#include "astra/station_type.h"
#include <cstdint>
#include <string>

namespace astra {

enum class KeeperArchetype : uint8_t {
    GruffVeteran,
    ChattyBureaucrat,
    NervousNewcomer,
    RetiredSpacer,
    CorporateStiff,
    EccentricLoner,
};

std::string pick_keeper_name(uint64_t keeper_seed);
std::string pick_scav_keeper_name(uint64_t keeper_seed);
std::string pick_pirate_captain_name(uint64_t keeper_seed);
KeeperArchetype pick_keeper_archetype(uint64_t keeper_seed);
const char* archetype_voice(KeeperArchetype);
std::string keeper_specialty_hook(const StationContext& ctx);

}  // namespace astra
