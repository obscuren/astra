#pragma once

#include "astra/player.h"
#include "astra/ui.h"

#include <string>
#include <vector>

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
    void open(Player* player, Renderer* renderer);
    void close();
    bool handle_input(int key);
    void draw(int screen_w, int screen_h);

private:
    Player* player_ = nullptr;
    Renderer* renderer_ = nullptr;
    bool open_ = false;
    CharTab active_tab_ = CharTab::Attributes;
    int cursor_ = 0;
    int scroll_ = 0;

    // Equipment tab
    enum class EquipFocus { PaperDoll, Inventory };
    EquipFocus equip_focus_ = EquipFocus::PaperDoll;
    int equip_cursor_ = 0;
    int inv_cursor_ = 0;

    // Context menu
    bool context_open_ = false;
    int context_selection_ = 0;
    struct ContextOption { char key; std::string label; };
    std::vector<ContextOption> context_options_;
    std::string context_message_;
    int context_msg_timer_ = 0;

    // Look overlay
    bool look_open_ = false;
    const Item* look_item_ = nullptr;

    void open_context_menu();
    void handle_context_key(int key);
    void execute_context_action(char key);
    void draw_context_menu(DrawContext& ctx);
    void draw_look_overlay(DrawContext& ctx);

    void draw_tab_bar(DrawContext& ctx);
    void draw_attributes(DrawContext& ctx);
    void draw_skills(DrawContext& ctx);
    void draw_equipment(DrawContext& ctx);
    void draw_stub(DrawContext& ctx, const char* message);
    void draw_reputation(DrawContext& ctx);

    void draw_stat_box(DrawContext& ctx, int x, int y,
                       const char* label, int value,
                       bool selected, int modifier = -999);
    void draw_section_header(DrawContext& ctx, int y,
                             const char* title, int left_margin = 1);
};

} // namespace astra
