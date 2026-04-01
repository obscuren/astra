#pragma once

#include "astra/player.h"
#include "astra/quest.h"
#include "astra/ui.h"

#include <random>
#include <string>

namespace astra {

enum class CharTab : uint8_t {
    Skills,
    Attributes,
    Equipment,
    Tinkering,
    Journal,
    Quests,
    Reputation,
    Ship,
};

static constexpr int char_tab_count = 8;

class CharacterScreen {
public:
    CharacterScreen() = default;

    bool is_open() const;
    void open(Player* player, Renderer* renderer, QuestManager* quests = nullptr,
              bool on_ship = false, CharTab initial_tab = CharTab::Skills);
    void close();
    bool handle_input(int key);
    void draw(int screen_w, int screen_h);

private:
    Player* player_ = nullptr;
    QuestManager* quests_ = nullptr;
    Renderer* renderer_ = nullptr;
    std::mt19937 rng_{std::random_device{}()};
    bool open_ = false;
    CharTab active_tab_ = CharTab::Skills;
    int cursor_ = 0;
    int scroll_ = 0;

    // Equipment tab
    enum class EquipFocus { PaperDoll, Inventory };
    EquipFocus equip_focus_ = EquipFocus::PaperDoll;
    int equip_cursor_ = 0;
    int inv_cursor_ = 0;

    // Skills tab
    std::vector<bool> skill_cat_expanded_;
    int skill_cursor_ = 0;  // index into flattened visible list
    int skill_scroll_ = 0;

    struct SkillVisItem { bool is_cat; int ci; int si; };
    std::vector<SkillVisItem> build_skill_vis() const;

    // Attribute point allocation
    int pending_points_[6] = {};
    bool has_pending() const;
    int total_pending() const;
    void commit_pending();

    // Context menu (reusable PopupMenu)
    PopupMenu context_menu_;
    std::string context_message_;
    int context_msg_timer_ = 0;

    // Look overlay
    bool look_open_ = false;
    const Item* look_item_ = nullptr;

    // Tinkering tab
    enum class TinkerFocus { Workbench, Slots, Synthesizer, Materials };
    TinkerFocus tinker_focus_ = TinkerFocus::Workbench;
    int tinker_slot_cursor_ = 0;     // which enhancement slot (0-2)
    int synth_bp_cursor_ = 0;        // 0 = left blueprint box, 1 = right
    int synth_bp1_ = -1;             // index into player's learned_blueprints
    int synth_bp2_ = -1;
    Item* workbench_item_ = nullptr; // pointer into player inventory or equipment
    int workbench_inv_idx_ = -1;     // index in player inventory, or -1 if from equipment

    void draw_tinkering(DrawContext& ctx);
    void draw_journal(DrawContext& ctx);
    int journal_cursor_ = 0;
    int journal_scroll_ = 0;

    void open_context_menu();
    void execute_context_action(char key);
    void draw_look_overlay(DrawContext& ctx);

    // Drop output — Game reads this after handle_input
    bool has_dropped_item_ = false;
    Item dropped_item_;
    // Ship component install output — Game reads this to update quests
    std::string installed_ship_slot_;
public:
    bool has_dropped_item() const { return has_dropped_item_; }
    Item consume_dropped_item() { has_dropped_item_ = false; return std::move(dropped_item_); }
    std::string consume_installed_ship_slot() {
        std::string s = std::move(installed_ship_slot_);
        installed_ship_slot_.clear();
        return s;
    }
private:

    // Ship tab
    enum class ShipFocus { Equipment, Inventory };
    ShipFocus ship_focus_ = ShipFocus::Equipment;
    int ship_equip_cursor_ = 0;
    int ship_inv_cursor_ = 0;
    bool on_ship_ = false;  // set in open(), controls interactivity

    void draw_attributes(DrawContext& ctx);
    void draw_skills(DrawContext& ctx);
    void draw_equipment(DrawContext& ctx);
    void draw_ship(DrawContext& ctx);
    void draw_stub(DrawContext& ctx, const char* message);
    void draw_reputation(DrawContext& ctx);
    void show_tab_help();

    // Tab help overlay
    PopupMenu tab_help_menu_;
    bool showing_tab_help_ = false;

    void draw_stat_box(DrawContext& ctx, int x, int y,
                       const char* label, int value,
                       bool selected, int modifier = -999,
                       int pending = 0, bool can_allocate = false);
    void draw_section_header(DrawContext& ctx, int y,
                             const char* title, int left_margin = 1);
};

} // namespace astra
