#pragma once

#include "astra/device_shell.h"
#include "astra/grid_nmap_widget.h"
#include "astra/player.h"
#include "astra/quest.h"
#include "astra/recipe.h"
#include "astra/rect.h"
#include "astra/ui.h"

#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <unordered_set>

namespace astra {

class WorldManager;
class HackingSystem;
class Game;

enum class PdaTab : uint8_t {
    Skills,
    Attributes,
    Equipment,
    Tinkering,
    Hacking,
    Cooking,
    Journal,
    Quests,
    Reputation,
    Ship,
};

static constexpr int pda_tab_count = 10;

class PdaScreen : public ShellOutputSink {
public:
    PdaScreen() = default;

    bool is_open() const;
    // initial_tab = nullopt -> open on the last-used tab (persists across
    // open/close). Callers that want to force a specific tab (ARIA, etc.)
    // pass it explicitly.
    void open(Player* player, Renderer* renderer, QuestManager* quests = nullptr,
              bool on_ship = false,
              std::optional<PdaTab> initial_tab = std::nullopt,
              bool can_board_ship = false,
              const WorldManager* world = nullptr,
              Game* game = nullptr,
              HackingSystem* hacking_system = nullptr);
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
    // Plan 7: pointers used to render/route the device shell into the
    // Hacking tab when a real-world shell is active. Both can be null when
    // the PDA is opened from a context that doesn't need a device shell
    // (tests, intro, etc.); the Hacking tab gracefully falls back to the
    // pda> shell rendering and input in that case.
    Game* game_ = nullptr;
    HackingSystem* hacking_system_ = nullptr;
    std::mt19937 rng_{std::random_device{}()};
    bool open_ = false;
    PdaTab active_tab_ = PdaTab::Skills;
    int cursor_ = 0;
    int scroll_ = 0;

    // Equipment tab
    enum class EquipFocus { PaperDoll, Inventory };
    EquipFocus equip_focus_ = EquipFocus::PaperDoll;
    int equip_cursor_ = 0;
    int inv_cursor_ = 0;
    // View toggle: Equipment paper-doll vs Implant paper-doll (Tab key)
    enum class EquipmentTabView { Equipment, Implants };
    EquipmentTabView equipment_tab_view_ = EquipmentTabView::Equipment;

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
    enum class TinkerFocus { Workbench, Slots, Synthesizer, Materials, Catalog, Refinement, Schematics };
    TinkerFocus tinker_focus_ = TinkerFocus::Workbench;
    // Last non-catalog focus — restored when Tab returns from catalog.
    TinkerFocus tinker_prev_left_focus_ = TinkerFocus::Workbench;
    int tinker_slot_cursor_ = 0;     // which enhancement slot (0-2)
    int synth_bp_cursor_ = 0;        // 0 = left blueprint box, 1 = right
    int synth_bp1_ = -1;             // index into player's learned_blueprints
    int synth_bp2_ = -1;
    Item* workbench_item_ = nullptr; // pointer into player inventory or equipment
    int workbench_inv_idx_ = -1;     // index in player inventory, or -1 if from equipment

    // Blueprint Catalog (right pane): cursor indexes blueprints; collapsed names hide recipes.
    enum class CatalogTab : uint8_t { Blueprints, Schematics };
    CatalogTab catalog_tab_ = CatalogTab::Blueprints;
    int catalog_cursor_ = 0;
    int catalog_scroll_ = 0;
    std::unordered_set<std::string> catalog_collapsed_;

    void draw_tinkering(UIContext& ctx);

    // Cooking tab
    enum class CookingFocus : uint8_t { Slots, Cookbook };
    CookingFocus cooking_focus_ = CookingFocus::Slots;
    int cooking_slot_cursor_ = 0;       // 0..2 (which pot slot)
    int cooking_cookbook_cursor_ = 0;   // index into known recipe list
    std::unordered_set<uint16_t> cooking_collapsed_recipes_;  // recipe ids that are currently collapsed
    int cooking_cookbook_scroll_ = 0;
    // Ingredient picker modal (opened from a pot slot).
    bool cooking_picker_active_ = false;
    int  cooking_picker_cursor_ = 0;    // index into filtered ingredient list
    int  cooking_picker_slot_ = 0;      // which pot slot (0..2) we're filling
    // Quantity prompt (follows the picker once an ingredient is chosen).
    bool cooking_qty_prompt_active_ = false;
    bool cooking_qty_prompt_edited_ = false;  // first digit replaces prefill
    int  cooking_qty_prompt_value_ = 1;
    uint16_t cooking_qty_prompt_item_def_id_ = 0;

    void handle_cooking_key(int key);
    void handle_cooking_picker_key(int key);
    void handle_cooking_qty_prompt_key(int key);
    void cooking_open_picker_for_slot(int slot_idx);
    void cooking_picker_confirm();
    void cooking_commit_qty_prompt();
    void cooking_clear_slot(int idx);
    void cooking_toggle_recipe(uint16_t recipe_id);
    void cooking_attempt_cook();

    void draw_cooking(UIContext& ctx);

public:
    // ── ShellOutputSink overrides (Plan 7 unified terminal) ──
    // The Hacking tab terminal IS the cyberdeck shell. While a DeviceShell
    // session is active, all session output (ritual, command output, channel
    // ticks, logout) appends to the same scrollback. Prompt + dispatch morph
    // by `hacking_system_->device_shell_open()`; on exit the prompt reverts
    // to `pda> ` and the entire ssh transcript stays in scrollback.
    void shell_emit_line(const std::string& text, UITag tag) override;
    void shell_clear_scroll() override;
    void shell_set_progress_line(const std::string& text, UITag tag) override;
    void shell_commit_progress_line() override;
    int  shell_progress_cells_hint() const override;
    // Render the unified Hacking-tab terminal into an arbitrary rect. Used
    // by the in-Grid Tron-window (grid_renderer.cpp) so the same scroll +
    // prompt swap into the playfield rect when the avatar is jacked into a
    // device. HUD chrome (Trace/Heat panes, log pane, program bar) stays
    // visible because it lives outside this rect.
    void draw_hacking_into(Renderer* renderer, Rect bounds);

    // Public entry-point for the in-Grid Tron-window: when the Grid input
    // handler sees that a Grid-via DeviceShell is active, it forwards keys
    // here so the unified terminal handles them just like the PDA does.
    void hack_term_handle_key_for_grid(int key) { handle_hacking_key(key); }

    // Plan 7 §4: (hack) Shell Access doorway autotypes a smart-formed
    // `ssh <user>@<ip>` into the existing pda> input buffer and submits it.
    // The connection ritual + session open just like a manual `ssh` typed
    // by the player, keeping the unified scroll the single source of truth.
    void hack_term_autotype_and_submit(const std::string& line);

    // Plan 7 — accessors used by the cyberdeck commands now living in
    // src/hack_commands/cmd_*.cpp. Each command receives a ShellContext& and
    // a Game&; from there it reaches the PdaScreen via game.pda_screen() to
    // queue request fields, open the nmap widget, etc. Visible behaviour is
    // unchanged — these are 1:1 setters for the previously private state.
    void hack_term_set_jack_in_request(uint32_t node_id) { jack_in_request_node_id_ = node_id; }
    void hack_term_set_ssh_request(uint32_t ip, bool as_root) {
        ssh_request_ip_   = ip;
        ssh_request_root_ = as_root;
    }
    void hack_term_open_nmap_widget() { nmap_widget_.open(); }
    GridNmapWidget& hack_term_nmap_widget() { return nmap_widget_; }
    // Re-greet on `clear` — re-emits the per-deck banner + MOTD into scroll.
    void hack_term_re_greet() {
        auto* slot = player_ ? player_->equipment.equipped_cyberdeck() : nullptr;
        if (slot && *slot && (*slot)->deck) {
            hack_term_greet_for_deck((*slot)->item_def_id);
            hack_term_greeted_deck_def_id_ = (*slot)->item_def_id;
        }
    }
    // Wipe the visible scroll (for cyberdeck `clear`). Does NOT re-greet.
    void hack_term_clear_lines() {
        hack_term_lines_.clear();
        hack_term_scroll_ = 0;
    }
    // Close the PDA from inside a cyberdeck command (`exit`).
    void hack_term_close_pda() { close(); }

private:
    // Hacking tab — terminal subwindow
    struct HackTermLine {
        std::string text;
        UITag tag = UITag::TextDim;
    };
    std::vector<HackTermLine> hack_term_lines_;
    std::string               hack_term_input_;
    int                       hack_term_input_cursor_ = 0;   // byte index into hack_term_input_
    // Up/down history index into the active ShellContext's history vector.
    // -1 = "below the bottom" / fresh buffer.
    int                       hack_term_history_cursor_ = -1;
    int                       hack_term_scroll_ = 0;         // lines scrolled up from bottom (0 = latest)
    uint16_t                  hack_term_greeted_deck_def_id_ = 0;

    // ── Plan 7 unified terminal ──
    // Transient progress-bar slot — overwritten in place each world tick by
    // active long-channels. Empty string = no transient line shown.
    std::string hack_term_progress_text_;
    UITag       hack_term_progress_tag_ = UITag::TextWarning;
    bool        hack_term_progress_set_ = false;
    // Cached on each Hacking-tab render so shell_progress_cells_hint() can
    // size the bar to ~50% of the visible width even when called from
    // device_shell::tick_world (outside the render path).
    int         hack_term_content_width_ = 0;

    void hack_term_emit(const std::string& line, UITag tag = UITag::TextDefault);
    void hack_term_greet_for_deck(uint16_t deck_def_id);
    void handle_hacking_key(int key);

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
    // arc_id -> collapsed? (absent = expanded by default)
    std::unordered_set<std::string> quest_arcs_collapsed_;
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
    // Use-item output — Game reads this after handle_input and calls use_item(idx)
    int use_item_request_idx_ = -1;
    // Recharge-item output — Game reads this and opens cell picker for the item
    int recharge_request_idx_ = -1;
    // Recharge-equipped output: -1=none, 0=weapon, 1=shield
    int recharge_equipped_request_ = -1;
    // Ship component install output — Game reads this to update quests
    std::string installed_ship_slot_;
    // Jack-in request — terminal `jack -t <node>`. 0 = none.
    uint32_t jack_in_request_node_id_ = 0;
    // Plan 7 Phase A: ssh request from `pda> ssh [<user>@]<ip>`. The game
    // input loop pops this, locates the mutable Hackable, and opens the
    // device shell at the requested tier (root or guest).
    // 0 = none.
    uint32_t ssh_request_ip_ = 0;
    bool     ssh_request_root_ = true;
    // Nmap overlay — shown above the Hacking terminal pane.
    GridNmapWidget nmap_widget_;
    // Pending nmap-side breach (Plan 5 Task 39). The widget never mutates
    // GridNetwork directly; the host (game_input) consumes this slot, charges
    // the deck for breach.exe, and flips the matching edge's `cracked = true`.
    // {0, 0} = none.
    GridNmapBreachRequest pending_breach_request_ = {};
    // Skill side-effect request — set when a skill with a side effect is learned.
    // Consumed by game_input.cpp which calls apply_skill_side_effects(game, id).
    // 0 = none (SkillId 0 is not a real skill).
    uint32_t pending_skill_side_effect_id_ = 0;
public:
    bool has_dropped_item() const { return has_dropped_item_; }
    Item consume_dropped_item() { has_dropped_item_ = false; return std::move(dropped_item_); }
    bool has_use_item_request() const { return use_item_request_idx_ >= 0; }
    int consume_use_item_request() { int i = use_item_request_idx_; use_item_request_idx_ = -1; return i; }
    int recharge_request_idx() const { return recharge_request_idx_; }
    void clear_recharge_request() { recharge_request_idx_ = -1; }
    int recharge_equipped_request() const { return recharge_equipped_request_; }
    void clear_recharge_equipped_request() { recharge_equipped_request_ = -1; }
    std::string consume_installed_ship_slot() {
        std::string s = std::move(installed_ship_slot_);
        installed_ship_slot_.clear();
        return s;
    }
    uint32_t consume_jack_in_request() {
        uint32_t v = jack_in_request_node_id_;
        jack_in_request_node_id_ = 0;
        return v;
    }
    // Returns 0 if no ssh pending. `as_root` is set to whether the player
    // typed `ssh root@` (true) or `ssh guest@` (false). Manual semantics.
    uint32_t consume_ssh_request(bool& as_root) {
        uint32_t v = ssh_request_ip_;
        as_root   = ssh_request_root_;
        ssh_request_ip_  = 0;
        ssh_request_root_ = true;
        return v;
    }
    GridNmapBreachRequest consume_breach_request() {
        GridNmapBreachRequest r = pending_breach_request_;
        pending_breach_request_ = {};
        return r;
    }
    uint32_t consume_skill_side_effect_request() {
        uint32_t v = pending_skill_side_effect_id_;
        pending_skill_side_effect_id_ = 0;
        return v;
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
    void draw_equipment_paperdoll(UIContext& ctx, int dy);
    void draw_implant_paperdoll(UIContext& ctx, int dy);
    void draw_combined_bonuses(UIContext& ctx, int y);
    void draw_equipment_inventory(UIContext& ctx, int half);
    void draw_ship(UIContext& ctx);
    void draw_stub(UIContext& ctx, const char* message);
    void draw_reputation(UIContext& ctx);
    void draw_hacking(UIContext& ctx);
    void draw_tab_help(int screen_w, int screen_h);
    void show_tab_help();

    // Tab help overlay
    MenuState tab_help_menu_;
    bool showing_tab_help_ = false;

    void draw_stat_box(UIContext& ctx, int x, int y,
                       const char* label, int value,
                       bool selected, int modifier = -999,
                       int pending = 0, bool can_allocate = false);
    // right_edge = -1 falls back to ctx.width()/2 (left panel default).
    void draw_section_header(UIContext& ctx, int y,
                             const char* title, int left_margin = 1,
                             int right_edge = -1);
};

} // namespace astra
