#pragma once

#include "astra/interaction.h"
#include "astra/npc.h"
#include "astra/ui.h"
#include "astra/ui_types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

class Game; // forward declare
struct Quest;

class DialogManager {
public:
    DialogManager() = default;

    bool is_open() const { return open_; }
    void close();

    // NPC interaction
    void open_npc_dialog(Npc& npc, Game& game);
    void advance_dialog(int selected, Game& game);

    // Fixture interaction. Builds the per-FixtureType native dialog (food
    // terminal, ship terminal, ARIA, ...) and — when the fixture is electrical
    // and capability gates are met — appends `(hack) Jack In`, `(hack) Sync
    // Soul`, and `(qh) ...` options into the SAME dialog so the player never
    // hops through a separate hackable menu.
    //
    // `interact_fixture_use_only` is the implementation; `interact_fixture` is
    // a thin shim retained for callers that built their flow around the
    // legacy two-layer model. Both end up at the same place now.
    void interact_fixture(int fixture_id, Game& game);
    void interact_fixture_use_only(int fixture_id, Game& game);

    // Tutorial choice dialog
    void show_tutorial_choice(Game& game);
    void show_tutorial_followup();

    // Announce an auto-accepted quest. Shows the quest info panel with a
    // single [a] Accept option. The quest is already in the active pool
    // when this is called; Accept just dismisses.
    void show_auto_accept(Game& game, const Quest& quest);

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
    std::vector<UITag> option_tags_;      // per-option line tag (default OptionNormal;
                                          // hacking-related options use TextSuccess/green)
    int selected_ = 0;                   // cursor position
    float max_width_frac_ = 0.45f;       // panel width as fraction of screen
    std::string footer_;
    EntityRef entity_;                    // NPC/fixture identity — renderer resolves glyph+color

    // Plan 5 single-dialog refactor: when a dialog is opened by interacting with
    // an electrical fixture, we stash its id here so `(hack) Jack In`,
    // `Sync Soul`, and `(qh) ...` options can resolve their target without a
    // separate menu layer. -1 = dialog isn't tied to a fixture.
    int dialog_fixture_id_ = -1;

    // Plan 5 single-dialog refactor: per-option "kind" marker for the
    // hacking-related options injected into fixture dialogs. Most options use
    // Normal (the existing dispatch by dialog_node_/interact_options_ wins).
    // Hacking-related options get HackingJackIn/HackingSyncSoul/HackingRunQh
    // and are short-circuited at the top of advance_dialog. Indexed parallel
    // to options_; size == options_.size() at all times.
    enum class OptionKind : uint8_t {
        Normal,
        HackingJackIn,
        HackingSyncSoul,
        HackingRunQh,
        HackingShellAccess,   // Plan 7: opens per-device shell (real-world doorway)
    };
    std::vector<OptionKind> option_kinds_;

    // For options whose kind is HackingRunQh, the deck-slot index (0..slots-1)
    // that locates the program. -1 for other kinds. Indexed parallel to options_.
    std::vector<int> dialog_option_qh_slot_;

    // Helper: reset dialog content for a new screen
    void reset_content(const std::string& title, float width_frac = 0.45f);
    void add_option(char key, const std::string& label);
    void add_option(char key, const std::string& label, UITag tag);

    // Plan 5 single-dialog refactor: append hacking-related options to a
    // freshly-built fixture dialog, gated on equipped deck + skill + tags.
    // Each helper is a no-op when its precondition is unmet. Called at the END
    // of each FixtureType case in interact_fixture_use_only that opens a
    // dialog (and at the start of synthesised Use+JackIn dialogs). The
    // dialog_fixture_id_ must be set to `fid` before calling.
    void append_qh_options(int fid, Game& game);
    void append_jack_in_option(int fid, Game& game);
    void append_sync_soul_option(int fid, Game& game);
    // Plan 7: appends `(hack) Shell Access` on any Hackable with the
    // Electronic tag (and not AlienTech) when the player has Cat_Hacking.
    // Hotkey 's' is taken by Sync Soul on AlienTech devices, so we use 'h'.
    void append_shell_access_option(int fid, Game& game);

    // Word-wrap body text respecting COLOR_BEGIN/COLOR_END markers
    static std::vector<std::string> word_wrap(const std::string& text, int width);

    Npc* interacting_npc_ = nullptr;
    const std::vector<DialogNode>* dialog_tree_ = nullptr;
    int dialog_node_ = -1;

    enum class InteractOption : uint8_t {
        Talk, Shop, Quest, QuestTurnIn,
        StoryQuestOffer, StoryQuestAccept, StoryQuestDecline,
        AutoAcceptAck,
        // Nova Stage 1 "Static in the Dark" turn-in flow:
        //   NovaHookEntry opens Nova's monologue + 3 response choices.
        //   NovaHookCare / Skeptic / Action show her reaction + "I'll go."
        //   NovaHookConfirm completes the quest on "I'll go.".
        NovaHookEntry, NovaHookCare, NovaHookSkeptic, NovaHookAction,
        NovaHookConfirm,
        Farewell,
    };
    std::vector<InteractOption> interact_options_;
    std::vector<std::string> pending_story_offers_;  // ids parallel to StoryQuestOffer interact_options entries
    std::string detail_offer_quest_id_;              // non-empty while showing quest offer detail view

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
