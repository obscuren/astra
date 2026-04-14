#pragma once

#include "astra/player.h"
#include "astra/quest.h"
#include "astra/ui.h"

#include <random>
#include <string>

namespace astra {

class WorldManager;

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
              bool on_ship = false, CharTab initial_tab = CharTab::Skills,
              bool can_board_ship = false,
              const WorldManager* world = nullptr);
    bool consume_board_ship_request() {
        bool r = board_ship_requested_;
        board_ship_requested_ = false;
        return r;
    }
    void close();
    bool handle_input(int key);
    void draw(int screen_w, int screen_h);

private:
    Player* player_ = nullptr;
    QuestManager* quests_ = nullptr;
    Renderer* renderer_ = nullptr;
    const WorldManager* world_ = nullptr;
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

    // Context menu (reusable MenuState)
    MenuState context_menu_;
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

    void draw_tinkering(UIContext& ctx);
    void draw_journal(UIContext& ctx);
    int journal_cursor_ = 0;
    int journal_scroll_ = 0;

    // Quests tab
    struct QuestVisItem {
        enum class Kind : uint8_t { Category, ArcHeader, Quest } kind;
        enum class QState : uint8_t { Active, Available, Locked, Completed } qstate = QState::Active;
        int cat_idx = -1;
        std::string arc_id;
        std::string quest_id;
    };
    std::vector<bool> quest_cat_expanded_;
    int quest_cursor_ = 0;
    int quest_scroll_ = 0;
    enum class QuestFocus : uint8_t { Left, Right };
    QuestFocus quest_focus_ = QuestFocus::Left;
    int quest_reward_cursor_ = 0;
    void draw_quests(UIContext& ctx);
    std::vector<QuestVisItem> build_quest_vis() const;

    void open_context_menu();
    void execute_context_action(char key);
    void draw_look_overlay(UIContext& ctx);
    void draw_context_menu(int screen_w, int screen_h);

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
    enum class ShipFocus { Actions, Equipment, Inventory };
    ShipFocus ship_focus_ = ShipFocus::Equipment;
    int ship_equip_cursor_ = 0;
    int ship_inv_cursor_ = 0;
    bool on_ship_ = false;  // set in open(), controls interactivity
    bool can_board_ship_ = false;  // set in open(), enables the Board action
    int ship_action_cursor_ = 0;   // 0 = Board Ship (only action for now)
    bool board_ship_requested_ = false;

    void draw_attributes(UIContext& ctx);
    void draw_skills(UIContext& ctx);
    void draw_equipment(UIContext& ctx);
    void draw_ship(UIContext& ctx);
    void draw_stub(UIContext& ctx, const char* message);
    void draw_reputation(UIContext& ctx);
    void draw_tab_help(int screen_w, int screen_h);
    void show_tab_help();

    // Tab help overlay
    MenuState tab_help_menu_;
    bool showing_tab_help_ = false;

    void draw_stat_box(UIContext& ctx, int x, int y,
                       const char* label, int value,
                       bool selected, int modifier = -999,
                       int pending = 0, bool can_allocate = false);
    void draw_section_header(UIContext& ctx, int y,
                             const char* title, int left_margin = 1);
};

} // namespace astra
