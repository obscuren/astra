#include "astra/grid_room_templates.h"

namespace astra {

// Tag priority: AlienTech > Weaponized > DataStore > HasOptics > Mobile > PowerNode > Electronic
RoomTemplateKind choose_template_for_tags(HackTagMask tags) {
    if (has_tag(tags, HackTag::AlienTech))   return RoomTemplateKind::PrecursorShrine;
    if (has_tag(tags, HackTag::Weaponized))  return RoomTemplateKind::Weaponized;
    if (has_tag(tags, HackTag::DataStore))   return RoomTemplateKind::DataVault;
    if (has_tag(tags, HackTag::HasOptics))   return RoomTemplateKind::Surveillance;
    if (has_tag(tags, HackTag::Mobile))      return RoomTemplateKind::CrewQuarters;
    if (has_tag(tags, HackTag::PowerNode))   return RoomTemplateKind::PowerNode;
    if (has_tag(tags, HackTag::Electronic))  return RoomTemplateKind::Generic;
    return RoomTemplateKind::Generic;
}

RoomTemplateSize template_size_constraints(RoomTemplateKind kind) {
    // Widths are ~2× heights to compensate for terminal cells being taller
    // than wide (typical monospace cell is 8×16px → 2:1 height:width ratio).
    switch (kind) {
        case RoomTemplateKind::Weaponized:      return {12, 6, 16, 8};
        case RoomTemplateKind::Surveillance:    return { 9, 4, 12, 6};
        case RoomTemplateKind::DataVault:       return {11, 4, 14, 6};
        case RoomTemplateKind::PowerNode:       return { 7, 3,  9, 4};
        case RoomTemplateKind::CrewQuarters:    return { 9, 3, 12, 5};
        case RoomTemplateKind::PrecursorShrine: return {12, 6, 16, 8};
        case RoomTemplateKind::Generic:
        default:                                return { 6, 3,  8, 4};
    }
}

RoomTemplateSeedRule template_seed_rule(RoomTemplateKind kind, int tier) {
    RoomTemplateSeedRule rule;
    switch (kind) {
        case RoomTemplateKind::Weaponized:
            rule.n_white_ice = 1;
            rule.n_gray_ice  = (tier >= 3) ? 1 : 0;
            break;
        case RoomTemplateKind::Surveillance:
            rule.n_white_ice  = 1;
            rule.n_data_nodes = 1;
            break;
        case RoomTemplateKind::DataVault:
            rule.n_white_ice         = 1;
            rule.n_gray_ice          = (tier >= 3) ? 1 : 0;
            rule.n_data_nodes        = 2 + tier;
            rule.n_encrypted_files   = 1 + (tier >= 2 ? 1 : 0);
            rule.lock_incoming_doors = (tier >= 2);
            break;
        case RoomTemplateKind::PowerNode:
            // Empty.
            break;
        case RoomTemplateKind::CrewQuarters:
            // Empty (NPC dialog comes in Plan 9).
            break;
        case RoomTemplateKind::PrecursorShrine:
            rule.n_white_ice         = 1;
            rule.n_black_ice         = (tier >= 3) ? 1 : 0;
            rule.n_data_nodes        = 1;
            rule.lock_incoming_doors = true;
            break;
        case RoomTemplateKind::Generic:
        default:
            // Empty.
            break;
    }
    return rule;
}

} // namespace astra
