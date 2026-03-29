#pragma once

#include "astra/interaction.h"
#include "astra/npc.h"
#include "astra/ui.h"

#include <string>
#include <vector>

namespace astra {

class Game; // forward declare

class DialogManager {
public:
    DialogManager() = default;

    bool is_open() const { return npc_dialog_.is_open(); }
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
    const std::string& body() const { return npc_dialog_body_; }

private:
    PopupMenu npc_dialog_;
    std::string npc_dialog_body_;
    Npc* interacting_npc_ = nullptr;
    const std::vector<DialogNode>* dialog_tree_ = nullptr;
    int dialog_node_ = -1;

    enum class InteractOption : uint8_t { Talk, Shop, Quest, QuestTurnIn, Farewell };
    std::vector<InteractOption> interact_options_;

    // ARIA command terminal output flags
    bool aria_open_ship_tab_ = false;
    bool aria_open_star_chart_ = false;
    bool aria_tutorial_followup_ = false;
public:
    bool consume_aria_ship_tab() { bool v = aria_open_ship_tab_; aria_open_ship_tab_ = false; return v; }
    bool consume_aria_star_chart() { bool v = aria_open_star_chart_; aria_open_star_chart_ = false; return v; }
    bool consume_aria_tutorial_followup() { bool v = aria_tutorial_followup_; aria_tutorial_followup_ = false; return v; }
};

} // namespace astra
