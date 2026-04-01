#pragma once

#include "astra/interaction.h"
#include "astra/npc.h"
#include "astra/ui.h"
#include "astra/ui_types.h"

#include <string>
#include <vector>

namespace astra {

class Game; // forward declare

class DialogManager {
public:
    DialogManager() = default;

    bool is_open() const { return open_; }
    void close();

    // NPC interaction
    void open_npc_dialog(Npc& npc, Game& game);
    void advance_dialog(int selected, Game& game);

    // Fixture interaction
    void interact_fixture(int fixture_id, Game& game);

    // Tutorial choice dialog
    void show_tutorial_choice(Game& game);
    void show_tutorial_followup();

    // Input — returns true if consumed. Handles Tab (trade), l (look), etc.
    bool handle_input(int key, Game& game);

    // Rendering
    void draw(Renderer* renderer, int screen_w, int screen_h);

    // State queries
    Npc* interacting_npc() const { return interacting_npc_; }
    const std::string& body() const { return body_; }

private:
    // Dialog state — persistent across conversation steps, no destroy/recreate
    bool open_ = false;
    std::string title_;
    std::string body_;                    // may contain COLOR_BEGIN/COLOR_END markers
    std::vector<std::string> options_;    // option labels
    std::vector<char> hotkeys_;           // per-option hotkeys
    int selected_ = 0;                   // cursor position
    float max_width_frac_ = 0.45f;       // panel width as fraction of screen
    std::string footer_;
    EntityRef entity_;                    // NPC/fixture identity — renderer resolves glyph+color

    // Helper: reset dialog content for a new screen
    void reset_content(const std::string& title, float width_frac = 0.45f);
    void add_option(char key, const std::string& label);

    // Word-wrap body text respecting COLOR_BEGIN/COLOR_END markers
    static std::vector<std::string> word_wrap(const std::string& text, int width);

    Npc* interacting_npc_ = nullptr;
    const std::vector<DialogNode>* dialog_tree_ = nullptr;
    int dialog_node_ = -1;

    enum class InteractOption : uint8_t { Talk, Shop, Quest, QuestTurnIn, Farewell };
    std::vector<InteractOption> interact_options_;

    // ARIA command terminal output flags
    bool aria_open_ship_tab_ = false;
    bool aria_open_star_chart_ = false;
    bool aria_tutorial_followup_ = false;
    bool aria_disembark_ = false;
    bool aria_open_datapad_ = false;
public:
    bool consume_aria_ship_tab() { bool v = aria_open_ship_tab_; aria_open_ship_tab_ = false; return v; }
    bool consume_aria_star_chart() { bool v = aria_open_star_chart_; aria_open_star_chart_ = false; return v; }
    bool consume_aria_tutorial_followup() { bool v = aria_tutorial_followup_; aria_tutorial_followup_ = false; return v; }
    bool consume_aria_disembark() { bool v = aria_disembark_; aria_disembark_ = false; return v; }
    bool consume_aria_open_datapad() { bool v = aria_open_datapad_; aria_open_datapad_ = false; return v; }
};

} // namespace astra
