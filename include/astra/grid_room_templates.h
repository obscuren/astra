#pragma once

#include "astra/hackable.h"   // HackTagMask, HackTag

#include <cstdint>

namespace astra {

enum class RoomTemplateKind : uint8_t {
    Weaponized,
    Surveillance,
    DataVault,
    PowerNode,
    CrewQuarters,
    PrecursorShrine,
    Generic
};

struct RoomTemplateSize { int min_w, min_h, max_w, max_h; };

struct RoomTemplateSeedRule {
    int n_white_ice = 0;
    int n_gray_ice  = 0;
    int n_black_ice = 0;
    int n_data_nodes = 0;
    int n_encrypted_files = 0;
    bool lock_incoming_doors = false;
};

RoomTemplateKind     choose_template_for_tags(HackTagMask tags);
RoomTemplateSize     template_size_constraints(RoomTemplateKind kind);
RoomTemplateSeedRule template_seed_rule(RoomTemplateKind kind, int tier);

} // namespace astra
