// src/terminal_ui_theme.cpp
#include "terminal_ui_theme.h"

namespace astra {

UIStyle resolve_ui_tag(UITag tag) {
    switch (tag) {
        // Layout chrome
        case UITag::Border:     return {Color::White, Color::Default};
        case UITag::Separator:  return {Color::DarkGray, Color::Default};
        case UITag::Title:      return {Color::White, Color::Default};
        case UITag::Footer:     return {Color::DarkGray, Color::Default};

        // Data display
        case UITag::HealthBar:     return {Color::Green, Color::Default};
        case UITag::XpBar:         return {Color::Cyan, Color::Default};
        case UITag::DurabilityBar: return {Color::Green, Color::Default};
        case UITag::ChargeBar:     return {Color::Cyan, Color::Default};
        case UITag::HullBar:       return {Color::White, Color::Default};
        case UITag::ShieldBar:     return {Color::Cyan, Color::Default};

        // Text roles
        case UITag::TextDefault: return {Color::Default, Color::Default};
        case UITag::TextDim:     return {Color::DarkGray, Color::Default};
        case UITag::TextBright:  return {Color::White, Color::Default};
        case UITag::TextDanger:  return {Color::Red, Color::Default};
        case UITag::TextSuccess: return {Color::Green, Color::Default};
        case UITag::TextWarning: return {Color::Yellow, Color::Default};
        case UITag::TextAccent:  return {Color::Cyan, Color::Default};

        // Interactive
        case UITag::TabActive:      return {Color::Yellow, Color::Default};
        case UITag::TabInactive:    return {Color::DarkGray, Color::Default};
        case UITag::NavKey:         return {Color::Green, Color::Default};
        case UITag::KeyLabel:       return {Color::Yellow, Color::Default};
        case UITag::OptionSelected:      return {Color::Yellow, Color::Default};
        case UITag::OptionNormal:        return {Color::Cyan, Color::Default};
        case UITag::ConversationOption:  return {Color::White, Color::Default};

        // Stats
        case UITag::StatAttack:  return {Color::Red, Color::Default};
        case UITag::StatDefense: return {Color::Blue, Color::Default};
        case UITag::StatHealth:  return {Color::Green, Color::Default};
        case UITag::StatVision:  return {Color::Cyan, Color::Default};
        case UITag::StatSpeed:   return {Color::Yellow, Color::Default};

        // Rarity
        case UITag::RarityCommon:    return {Color::White, Color::Default};
        case UITag::RarityUncommon:  return {Color::Green, Color::Default};
        case UITag::RarityRare:      return {Color::Blue, Color::Default};
        case UITag::RarityEpic:      return {Color::Magenta, Color::Default};
        case UITag::RarityLegendary: return {static_cast<Color>(208), Color::Default};
    }
    return {Color::Default, Color::Default};
}

UIStyle resolve_ui_tag(UITag tag, int value, int max) {
    if (tag == UITag::HealthBar && max > 0) {
        int pct = (value * 100) / max;
        if (pct <= 25) return {Color::Red, Color::Default};
        if (pct <= 50) return {Color::Yellow, Color::Default};
        return {Color::Green, Color::Default};
    }
    if (tag == UITag::DurabilityBar && max > 0) {
        int pct = (value * 100) / max;
        if (pct <= 25) return {Color::Red, Color::Default};
        if (pct <= 50) return {Color::Yellow, Color::Default};
        return {Color::Green, Color::Default};
    }
    return resolve_ui_tag(tag);
}

} // namespace astra
