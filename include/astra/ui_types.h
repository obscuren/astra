// include/astra/ui_types.h
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace astra {

// ---------------------------------------------------------------------------
// UITag — semantic intent for UI elements. Renderer resolves to visual style.
// ---------------------------------------------------------------------------

enum class UITag : uint16_t {
    // Layout chrome
    Border, Separator, Title, Footer,

    // Data display
    HealthBar, XpBar, DurabilityBar, ChargeBar,
    HullBar, ShieldBar,

    // Text roles
    TextDefault, TextDim, TextBright,
    TextDanger, TextSuccess, TextWarning, TextAccent,

    // Interactive
    TabActive, TabInactive, NavKey, KeyLabel,
    OptionSelected, OptionNormal, ConversationOption,

    // Game stats
    StatAttack, StatDefense, StatHealth, StatVision, StatSpeed,

    // Time phases
    PhaseDawn, PhaseDay, PhaseDusk, PhaseNight,

    // Rarity
    RarityCommon, RarityUncommon, RarityRare, RarityEpic, RarityLegendary,
};

// ---------------------------------------------------------------------------
// EntityRef — lightweight reference to a game entity for renderer-resolved styling
// ---------------------------------------------------------------------------

struct EntityRef {
    enum class Kind : uint8_t { None, Npc, Item, Quest, Fixture } kind = Kind::None;
    uint16_t id = 0;       // NpcRole, item_def_id, quest_id, FixtureType
    uint8_t  seed = 0;     // race for NPCs, etc.

    bool has_value() const { return kind != Kind::None; }
};

// ---------------------------------------------------------------------------
// Text alignment
// ---------------------------------------------------------------------------

enum class TextAlign : uint8_t { Left, Center, Right };

// ---------------------------------------------------------------------------
// Layout sizing — logical units (terminal: 1 unit = 1 cell, SDL: renderer-defined)
// ---------------------------------------------------------------------------

struct Size {
    enum Kind { Fixed, Fill, Fraction };
    Kind kind;
    float value;
};

inline Size fixed(int units) { return {Size::Fixed, static_cast<float>(units)}; }
inline Size fill() { return {Size::Fill, 0.f}; }
inline Size fraction(float f) { return {Size::Fraction, f}; }

// ---------------------------------------------------------------------------
// UI Component Descriptors
// ---------------------------------------------------------------------------

struct PanelDesc {
    std::string title;
    std::string footer;
    TextAlign footer_align = TextAlign::Center;
    UITag tag = UITag::Border;
};

struct ProgressBarDesc {
    int x = 0;
    int y = 0;
    int width = 10;
    int value = 0;
    int max = 100;
    UITag tag = UITag::HealthBar;
    std::string label;
    EntityRef entity;
};

struct TextDesc {
    int x = 0;
    int y = 0;
    std::string content;
    UITag tag = UITag::TextDefault;
};

struct TextSegment {
    std::string text;
    UITag tag = UITag::TextDefault;
    EntityRef entity;
};

// Helper: build segments for a shortcut key display: [Key] → [ white, Key yellow, ] white
inline std::vector<TextSegment> key_segments(const std::string& key) {
    return {
        {"[", UITag::TextBright},
        {key, UITag::KeyLabel},
        {"]", UITag::TextBright},
    };
}

// Helper: build segments for "key action" pair: [Key] Action
inline std::vector<TextSegment> key_action_segments(const std::string& key, const std::string& action) {
    return {
        {"[", UITag::TextBright},
        {key, UITag::KeyLabel},
        {"] ", UITag::TextBright},
        {action, UITag::TextDim},
    };
}

struct StyledTextDesc {
    int x = 0;
    int y = 0;
    std::vector<TextSegment> segments;
    TextAlign align = TextAlign::Left;
};

struct ListItem {
    std::string label;
    UITag tag = UITag::OptionNormal;
    bool selected = false;
    EntityRef entity;
};

struct ListDesc {
    std::vector<ListItem> items;
    int scroll_offset = 0;
    UITag tag = UITag::OptionNormal;
    UITag selected_tag = UITag::OptionSelected;
};

struct TabBarDesc {
    std::vector<std::string> tabs;
    int active = 0;
    UITag active_tag = UITag::TabActive;
    UITag inactive_tag = UITag::TabInactive;
    TextAlign align = TextAlign::Left;
    bool show_nav = false;
    std::string nav_left_label = "Q";
    std::string nav_right_label = "E";
};

struct WidgetBarEntry {
    std::string name;
    std::string hotkey;  // e.g. "F1"
    bool active = false;
    bool focused = false;
};

struct WidgetBarDesc {
    std::vector<WidgetBarEntry> entries;
};

struct SeparatorDesc {
    UITag tag = UITag::Separator;
    bool vertical = false;  // false = horizontal (─), true = vertical (│)
};

struct LabelValueDesc {
    int x = 0;
    int y = 0;
    std::string label;
    UITag label_tag = UITag::TextDim;
    std::string value;
    UITag value_tag = UITag::TextBright;
    EntityRef entity;
};

} // namespace astra
