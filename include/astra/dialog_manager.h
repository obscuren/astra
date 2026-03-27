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

    enum class InteractOption : uint8_t { Talk, Shop, Quest, Farewell };
    std::vector<InteractOption> interact_options_;

    // Repair bench state (dialog_node_ == -10)
    int repair_bench_item_ = -1; // inventory index of item on bench, -1 = empty
};

} // namespace astra
