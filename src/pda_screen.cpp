#include "astra/pda_screen.h"
#include "astra/aura.h"
#include "astra/character.h"
#include "astra/device_shell.h"
#include "astra/hacking_system.h"
#include "astra/recipe.h"
#include "astra/display_name.h"
#include "astra/effect.h"
#include "astra/faction.h"
#include "astra/skill_defs.h"
#include "astra/skill_grant.h"
#include "astra/journal.h"
#include "astra/quest.h"
#include "astra/tinkering.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <string>

namespace astra {

static const char* tab_names[] = {
    "Skills", "Attributes", "Inventory & Equipment", "Tinkering", "Hacking",
    "Cooking", "Journal", "Quests", "Reputation", "Ship",
};

bool PdaScreen::is_open() const { return open_; }

void PdaScreen::open(Player* player, Renderer* renderer, QuestManager* quests,
                           bool on_ship, std::optional<PdaTab> initial_tab,
                           bool can_board_ship,
                           const WorldManager* world,
                           Game* game,
                           HackingSystem* hacking_system) {
    player_ = player;
    renderer_ = renderer;
    quests_ = quests;
    on_ship_ = on_ship;
    can_board_ship_ = can_board_ship;
    world_ = world;
    game_ = game;
    hacking_system_ = hacking_system;
    open_ = true;

    // Plan 7 unified terminal: bind the DeviceShell's output sink to us so
    // ritual, command output, and channel ticks all flow into the PDA's
    // single Hacking-tab scroll. Safe to bind even when no shell is open;
    // the sink is consulted only while a session is active.
    if (hacking_system_) {
        hacking_system_->device_shell().bind_sink(this);
    }
    if (initial_tab) active_tab_ = *initial_tab;
    // else: keep active_tab_ from last close
    cursor_ = 0;
    scroll_ = 0;
    board_ship_requested_ = false;
    // Default Ship-tab focus: if Board Ship is available, start on the action
    // row so keyboard users land on it immediately. Otherwise focus equipment.
    ship_focus_ = can_board_ship_ ? ShipFocus::Actions : ShipFocus::Equipment;
    ship_action_cursor_ = 0;
    for (int i = 0; i < 6; ++i) pending_points_[i] = 0;

    // Initialize skill category expand state: only learned categories start expanded
    const auto& catalog = skill_catalog();
    skill_cat_expanded_.assign(catalog.size(), false);
    for (size_t ci = 0; ci < catalog.size(); ++ci) {
        for (auto sid : player_->learned_skills) {
            if (sid == catalog[ci].unlock_id) {
                skill_cat_expanded_[ci] = true;
                break;
            }
        }
    }

    // Show tab help overlay if player hasn't seen this tab yet (skip for DevCommander)
    showing_tab_help_ = false;
    show_tab_help();
}

void PdaScreen::close() { open_ = false; }

bool PdaScreen::handle_input(int key) {
    if (!open_) return false;

    // Tab help popup — intercepts input when showing
    if (showing_tab_help_ && tab_help_menu_.open) {
        MenuResult r = tab_help_menu_.handle_input(key);
        if (r == MenuResult::Selected || r == MenuResult::Closed) {
            tab_help_menu_.reset();
            showing_tab_help_ = false;
            player_->tab_help_seen |= (1 << static_cast<int>(active_tab_));
        }
        return true;
    }

    // ESC: close overlays first, then the screen itself
    if (key == 27) {
        if (look_open_) {
            look_open_ = false;
            look_item_ = nullptr;
            return true;
        }
        if (context_menu_.open) {
            context_menu_.reset();
            return true;
        }
        if (active_tab_ == PdaTab::Cooking && cooking_qty_prompt_active_) {
            cooking_qty_prompt_active_ = false;
            cooking_qty_prompt_edited_ = false;
            return true;
        }
        if (active_tab_ == PdaTab::Cooking && cooking_picker_active_) {
            cooking_picker_active_ = false;
            return true;
        }
        if (active_tab_ == PdaTab::Hacking && nmap_widget_.is_open()) {
            nmap_widget_.close();
            return true;
        }
        // Plan 7 unified terminal: if a real-world DeviceShell session is
        // active, ESC routes to the Hacking tab key handler which closes the
        // session (yanks the cable). The PDA itself stays open so the user
        // can keep using it.
        if (active_tab_ == PdaTab::Hacking &&
            hacking_system_ && hacking_system_->device_shell_open() &&
            hacking_system_->device_shell().via() == ShellVia::RealWorld) {
            handle_hacking_key(key);
            return true;
        }
        close();
        return true;
    }

    // Tab switching with [ and ] — bracket pair reads as prev/next page
    // and never collides with text input on tabs that accept typed commands.
    bool tab_switch_blocked = false;
    if (active_tab_ == PdaTab::Cooking &&
        (cooking_picker_active_ || cooking_qty_prompt_active_)) {
        tab_switch_blocked = true;
    }
    // Plan 7 §3a: brackets are valid characters in shell commands; while the
    // device shell is active, route everything (including [ and ]) to it.
    if (active_tab_ == PdaTab::Hacking &&
        hacking_system_ && hacking_system_->device_shell_open() &&
        hacking_system_->device_shell().via() == ShellVia::RealWorld) {
        tab_switch_blocked = true;
    }
    if (key == '[' && !tab_switch_blocked) {
        int t = static_cast<int>(active_tab_);
        t = (t - 1 + pda_tab_count) % pda_tab_count;
        active_tab_ = static_cast<PdaTab>(t);
        cursor_ = 0;
        scroll_ = 0;
        show_tab_help();
        return true;
    }
    if (key == ']' && !tab_switch_blocked) {
        int t = static_cast<int>(active_tab_);
        t = (t + 1) % pda_tab_count;
        active_tab_ = static_cast<PdaTab>(t);
        cursor_ = 0;
        scroll_ = 0;
        show_tab_help();
        return true;
    }

    // Look overlay — any key closes
    if (look_open_) {
        look_open_ = false;
        look_item_ = nullptr;
        return true;
    }

    // Context menu intercepts input when open
    if (context_menu_.open) {
        MenuResult mr = context_menu_.handle_input(key);
        if (mr == MenuResult::Selected) {
            if (active_tab_ == PdaTab::Tinkering) {
                // Tinkering item/material picker result
                int sel = context_menu_.selection;
                if (tinker_focus_ == TinkerFocus::Workbench && !workbench_item_) {
                    // Find the sel-th workbench-eligible item in inventory.
                    // Filter must match the one used to populate the picker (search "Place Item").
                    int count = 0;
                    for (int i = 0; i < static_cast<int>(player_->inventory.items.size()); ++i) {
                        const auto& it = player_->inventory.items[i];
                        if (it.slot.has_value() || it.max_durability > 0 || it.enhancement_slots > 0) {
                            if (count == sel) {
                                workbench_inv_idx_ = i;
                                workbench_item_ = &player_->inventory.items[i];
                                // Init enhancement slots if needed
                                if (workbench_item_->enhancement_slots == 0 && workbench_item_->slot.has_value())
                                    init_enhancement_slots(*workbench_item_);
                                context_message_ = "Placed " + workbench_item_->name + " on workbench.";
                                context_msg_timer_ = 2;
                                break;
                            }
                            count++;
                        }
                    }
                } else if (tinker_focus_ == TinkerFocus::Synthesizer) {
                    // Blueprint picker result
                    if (sel >= 0 && sel < static_cast<int>(player_->learned_blueprints.size())) {
                        if (synth_bp_cursor_ == 0) synth_bp1_ = sel;
                        else synth_bp2_ = sel;
                    }
                } else if (tinker_focus_ == TinkerFocus::Slots && workbench_item_) {
                    // Find the sel-th crafting material that has a slot effect
                    int count = 0;
                    for (int i = 0; i < static_cast<int>(player_->inventory.items.size()); ++i) {
                        const auto& it = player_->inventory.items[i];
                        if (it.type == ItemType::CraftingMaterial && get_material_effect(it.id)) {
                            if (count == sel) {
                                auto result = enhance_item(*workbench_item_, tinker_slot_cursor_, it.id, *player_);
                                context_message_ = result.message;
                                context_msg_timer_ = 3;
                                break;
                            }
                            count++;
                        }
                    }
                } else if (tinker_focus_ == TinkerFocus::Refinement) {
                    const auto& recipes = refinement_recipes();
                    if (sel >= 0 && sel < static_cast<int>(recipes.size())) {
                        auto result = refine_item(recipes[sel], *player_);
                        context_message_ = result.message;
                        context_msg_timer_ = 3;
                    }
                } else if (tinker_focus_ == TinkerFocus::Schematics) {
                    if (sel >= 0 && sel < static_cast<int>(player_->learned_schematics.size())) {
                        auto sid = player_->learned_schematics[sel].schematic_id;
                        auto result = craft_schematic(sid, *player_);
                        context_message_ = result.message;
                        context_msg_timer_ = 3;
                    }
                }
            } else {
                execute_context_action(context_menu_.selected_key());
            }
        }
        return true;
    }

    if (context_msg_timer_ > 0) --context_msg_timer_;

    // Tab-specific input
    if (active_tab_ == PdaTab::Attributes) {
        // 2D grid navigation for attribute boxes
        // Row 0: STR(0) AGI(1) TOU(2) INT(3) WIL(4) LUK(5)   — 6 primary
        // Row 1: QCK(6) SPD(7) DEF(8) DDG(9)                  — 4 secondary
        // Row 2: ACD(10) ELC(11) CLD(12) HET(13)              — 4 resistances
        static constexpr int row_start[] = {0, 6, 10};
        static constexpr int row_size[]  = {6, 4, 4};
        static constexpr int num_rows = 3;

        auto cur_row = [&]() -> int {
            if (cursor_ < 6) return 0;
            if (cursor_ < 10) return 1;
            return 2;
        };
        auto col_in_row = [&]() -> int {
            return cursor_ - row_start[cur_row()];
        };

        if (key == KEY_LEFT) {
            int r = cur_row();
            int col = col_in_row();
            cursor_ = row_start[r] + (col > 0 ? col - 1 : row_size[r] - 1);
        }
        if (key == KEY_RIGHT) {
            int r = cur_row();
            int col = col_in_row();
            cursor_ = row_start[r] + (col < row_size[r] - 1 ? col + 1 : 0);
        }
        if (key == KEY_UP) {
            int r = (cur_row() + num_rows - 1) % num_rows;
            int col = std::min(col_in_row(), row_size[r] - 1);
            cursor_ = row_start[r] + col;
        }
        if (key == KEY_DOWN) {
            int r = (cur_row() + 1) % num_rows;
            int col = std::min(col_in_row(), row_size[r] - 1);
            cursor_ = row_start[r] + col;
        }

        // +/- to allocate/deallocate points on primary attributes (cursor 0-5)
        if (cursor_ < 6) {
            int remaining = player_->attribute_points - total_pending();
            if ((key == '+' || key == '=') && remaining > 0) {
                pending_points_[cursor_]++;
            }
            if (key == '-' && pending_points_[cursor_] > 0) {
                pending_points_[cursor_]--;
            }
            // Space commits pending points
            if (key == ' ' && has_pending()) {
                commit_pending();
            }
        }
    } else if (active_tab_ == PdaTab::Equipment) {
        if (key == '\t') {
            // Tab toggles ONLY the paper-doll view; the inventory pane
            // stays put. Reset paper-doll cursor on switch; preserve
            // whichever pane focus was active.
            equipment_tab_view_ = (equipment_tab_view_ == EquipmentTabView::Equipment)
                ? EquipmentTabView::Implants : EquipmentTabView::Equipment;
            equip_cursor_ = 0;
            return true;
        }
        if (equipment_tab_view_ == EquipmentTabView::Implants) {
            // Implant view: same shape as Equipment view, just a smaller
            // paper-doll. Up/Down between implant slots, Right crosses to
            // the inventory pane, Left crosses back, Space opens context.
            if (equip_focus_ == EquipFocus::PaperDoll) {
                if (key == KEY_UP && equip_cursor_ > 0) --equip_cursor_;
                if (key == KEY_DOWN && equip_cursor_ < Player::IMPLANT_SLOTS - 1)
                    ++equip_cursor_;
                if (key == KEY_RIGHT) {
                    equip_focus_ = EquipFocus::Inventory;
                    inv_cursor_  = 0;
                }
            } else {
                int count = static_cast<int>(player_->inventory.items.size());
                if (key == KEY_UP   && inv_cursor_ > 0) --inv_cursor_;
                if (key == KEY_DOWN && inv_cursor_ < count - 1) ++inv_cursor_;
                if (key == KEY_LEFT) {
                    equip_focus_ = EquipFocus::PaperDoll;
                }
            }
            if (key == ' ') {
                open_context_menu();
                return true;
            }
            return false;
        }
        if (equip_focus_ == EquipFocus::PaperDoll) {
            // 2D grid navigation for paper doll slots
            // Layout:  row0: Face(0)
            //          row1: Head(1)
            //          row2: LHand(2) LArm(3) Body(4) RArm(5) RHand(6)
            //          row3: Shield(11) Back(7)  Util1(12) Util2(13)
            //          row4: Feet(8)
            //          row5: Thrown(9) Missile(10)
            //
            // Navigation tables: -1 = no movement
            static constexpr int nav_up[]    = {-1, 0,  1,  1,  1,  1,  1,  4,  7,  8, 12,  4,  5,  6};
            static constexpr int nav_down[]  = { 1, 4,  7,  7,  7,  7,  7,  8,  9, -1, -1,  8, 10, -1};
            static constexpr int nav_left[]  = {-1,-1, -1,  2,  3,  4,  5, 11, -1, -1,  9, -1,  7, 12};
            static constexpr int nav_right[] = {-1,-1,  3,  4,  5,  6, -1, 12, -1, 10, -1,  7, 13, -1};

            int next = -1;
            if (key == KEY_UP)    next = nav_up[equip_cursor_];
            if (key == KEY_DOWN)  next = nav_down[equip_cursor_];
            if (key == KEY_LEFT)  next = nav_left[equip_cursor_];
            if (key == KEY_RIGHT) next = nav_right[equip_cursor_];
            if (next >= 0) {
                equip_cursor_ = next;
            } else if (key == KEY_RIGHT) {
                // Rightmost paper-doll column — cross to inventory pane.
                equip_focus_ = EquipFocus::Inventory;
                inv_cursor_ = 0;
            }
        } else {
            int count = static_cast<int>(player_->inventory.items.size());
            if (key == KEY_UP && inv_cursor_ > 0) --inv_cursor_;
            if (key == KEY_DOWN && inv_cursor_ < count - 1) ++inv_cursor_;
            if (key == KEY_LEFT) {
                // Left edge of inventory — cross back to paper-doll pane.
                equip_focus_ = EquipFocus::PaperDoll;
            }
        }
        if (key == ' ') {
            open_context_menu();
            return true;
        }
    } else if (active_tab_ == PdaTab::Ship) {
        if (key == '\t') {
            // Cycle focus: Actions -> Equipment -> Inventory -> Actions...
            // Skip Actions if Board Ship isn't available.
            if (ship_focus_ == ShipFocus::Actions)
                ship_focus_ = ShipFocus::Equipment;
            else if (ship_focus_ == ShipFocus::Equipment)
                ship_focus_ = ShipFocus::Inventory;
            else
                ship_focus_ = can_board_ship_ ? ShipFocus::Actions : ShipFocus::Equipment;
            return true;
        }
        if (ship_focus_ == ShipFocus::Actions) {
            // Only one action for now. Down-arrow drops into Equipment.
            if (key == KEY_DOWN) {
                ship_focus_ = ShipFocus::Equipment;
            }
            if (key == '\r' || key == '\n' || key == ' ') {
                if (can_board_ship_) {
                    board_ship_requested_ = true;
                    close();
                    return true;
                } else {
                    context_message_ = "Board Ship — must be on a planet.";
                    context_msg_timer_ = 2;
                }
            }
        } else if (ship_focus_ == ShipFocus::Equipment) {
            if (key == KEY_UP) {
                if (ship_equip_cursor_ > 0) {
                    --ship_equip_cursor_;
                } else if (can_board_ship_) {
                    ship_focus_ = ShipFocus::Actions;
                }
            }
            if (key == KEY_DOWN && ship_equip_cursor_ < ship_slot_count - 1) ++ship_equip_cursor_;
            if (key == ' ' && on_ship_) {
                open_context_menu();
                return true;
            }
        } else {
            int count = static_cast<int>(player_->ship.cargo.size());
            if (key == KEY_UP && ship_inv_cursor_ > 0) --ship_inv_cursor_;
            if (key == KEY_DOWN && ship_inv_cursor_ < count - 1) ++ship_inv_cursor_;
            if (key == ' ' && on_ship_) {
                open_context_menu();
                return true;
            }
        }
    } else if (active_tab_ == PdaTab::Skills) {

        auto has_skill = [&](SkillId id) {
            for (auto sid : player_->learned_skills)
                if (sid == id) return true;
            return false;
        };

        auto vis = build_skill_vis();

        int max_c = static_cast<int>(vis.size()) - 1;
        if (key == KEY_UP && skill_cursor_ > 0) --skill_cursor_;
        if (key == KEY_DOWN && skill_cursor_ < max_c) ++skill_cursor_;
        if (skill_cursor_ > max_c) skill_cursor_ = max_c;

        if (skill_cursor_ >= 0 && skill_cursor_ < static_cast<int>(vis.size())) {
            const auto& v = vis[skill_cursor_];

            // Space: toggle expand/collapse (categories only)
            if (key == ' ' && v.is_cat) {
                skill_cat_expanded_[v.ci] = !skill_cat_expanded_[v.ci];
            }

            // l: learn (category unlock or skill)
            if (key == 'l') {
                if (v.is_cat) {
                    const auto& cat = skill_catalog()[v.ci];
                    if (!has_skill(cat.unlock_id) && player_->skill_points >= cat.sp_cost) {
                        player_->skill_points -= cat.sp_cost;
                        grant_skill(*player_, cat.unlock_id);
                        skill_cat_expanded_[v.ci] = true;
                        context_message_ = "Unlocked " + cat.name + "!";
                        context_msg_timer_ = 3;
                    }
                } else {
                    const auto& cat = skill_catalog()[v.ci];
                    const auto& sk = cat.skills[v.si];
                    if (!has_skill(cat.unlock_id)) {} // locked category
                    else if (has_skill(sk.id)) {} // already learned
                    else if (player_->skill_points < sk.sp_cost) {} // can't afford
                    else {
                        bool meets_req = true;
                        if (sk.attribute_req > 0 && sk.attribute_name) {
                            const auto& a = player_->attributes;
                            std::string attr(sk.attribute_name);
                            int val = 0;
                            if (attr == "Agility") val = a.agility;
                            else if (attr == "Strength") val = a.strength;
                            else if (attr == "Toughness") val = a.toughness;
                            else if (attr == "Intelligence") val = a.intelligence;
                            else if (attr == "Willpower") val = a.willpower;
                            else if (attr == "Luck") val = a.luck;
                            if (val < sk.attribute_req) meets_req = false;
                        }
                        // Cross-category prerequisite: Advanced Fire Making requires Camp Making.
                        if (sk.id == SkillId::AdvancedFireMaking &&
                            !player_has_skill(*player_, SkillId::CampMaking)) {
                            meets_req = false;
                        }
                        if (meets_req) {
                            player_->skill_points -= sk.sp_cost;
                            grant_skill(*player_, sk.id);
                            if (sk.id == SkillId::Haggle) {
                                add_effect(player_->effects, make_haggle_ge());
                            }
                            if (sk.id == SkillId::ThickSkin) {
                                add_effect(player_->effects, make_thick_skin_ge());
                            }
                            // Skills with one-time side effects: signal game_input
                            // to call apply_skill_side_effects(game, id).
                            if (sk.id == SkillId::ConsciousnessAnchor) {
                                pending_skill_side_effect_id_ =
                                    static_cast<uint32_t>(sk.id);
                            }
                            // Learning a skill itself may add skill-sourced auras.
                            rebuild_auras_from_sources(*player_);
                            context_message_ = "Learned " + sk.name + "!";
                            context_msg_timer_ = 3;
                        }
                    }
                }
            }
        }
    } else if (active_tab_ == PdaTab::Tinkering) {
        // Tab: swap focus between left-cluster (workbench/slots/synth/materials) and right catalog.
        if (key == '\t') {
            if (tinker_focus_ == TinkerFocus::Catalog) {
                tinker_focus_ = tinker_prev_left_focus_;
            } else {
                tinker_prev_left_focus_ = tinker_focus_;
                tinker_focus_ = TinkerFocus::Catalog;
            }
            return true;
        }

        // Catalog focus: navigate unlocked recipes + toggle expand/collapse by result name.
        if (tinker_focus_ == TinkerFocus::Catalog) {
            // Tab switch: left/right toggles between Blueprints and Schematics.
            if (key == KEY_LEFT || key == KEY_RIGHT) {
                catalog_tab_ = (catalog_tab_ == CatalogTab::Blueprints)
                                ? CatalogTab::Schematics
                                : CatalogTab::Blueprints;
                catalog_cursor_ = 0;
                catalog_scroll_ = 0;
                return true;
            }
            // Build the active list based on the current tab.
            int rcount = 0;
            std::vector<const SynthesisRecipe*> known;
            if (catalog_tab_ == CatalogTab::Blueprints) {
                auto player_knows_k = [&](const char* bp_name) {
                    for (const auto& bp : player_->learned_blueprints)
                        if (bp.name == bp_name) return true;
                    return false;
                };
                for (const auto& r : synthesis_recipes()) {
                    if (player_knows_k(r.blueprint_1) || player_knows_k(r.blueprint_2))
                        known.push_back(&r);
                }
                rcount = static_cast<int>(known.size());
            } else {
                rcount = static_cast<int>(player_->learned_schematics.size());
            }

            if (key == KEY_UP && catalog_cursor_ > 0) --catalog_cursor_;
            if (key == KEY_DOWN && catalog_cursor_ < rcount - 1) ++catalog_cursor_;
            if (key == ' ' && rcount > 0
                && catalog_cursor_ >= 0 && catalog_cursor_ < rcount) {
                std::string name;
                if (catalog_tab_ == CatalogTab::Blueprints) {
                    name = known[catalog_cursor_]->result_name;
                } else {
                    const auto& ls = player_->learned_schematics[catalog_cursor_];
                    const SchematicRecipe* rec = find_schematic_recipe(ls.schematic_id);
                    name = rec ? rec->output_name : ls.name;
                }
                auto it = catalog_collapsed_.find(name);
                if (it == catalog_collapsed_.end()) catalog_collapsed_.insert(name);
                else catalog_collapsed_.erase(it);
            }
            // [C] crafts the highlighted schematic directly (Schematics tab only).
            if (key == 'C' && catalog_tab_ == CatalogTab::Schematics
                && catalog_cursor_ >= 0 && catalog_cursor_ < rcount
                && player_has_skill(*player_, SkillId::Cat_Tinkering)) {
                auto sid = player_->learned_schematics[catalog_cursor_].schematic_id;
                auto result = craft_schematic(sid, *player_);
                context_message_ = result.message;
                context_msg_timer_ = 4;
            }
            return true;
        }

        // Navigation between workbench, slots, materials
        if (key == KEY_UP) {
            if (tinker_focus_ == TinkerFocus::Materials) tinker_focus_ = TinkerFocus::Synthesizer;
            else if (tinker_focus_ == TinkerFocus::Synthesizer) tinker_focus_ = TinkerFocus::Slots;
            else if (tinker_focus_ == TinkerFocus::Slots) tinker_focus_ = TinkerFocus::Workbench;
        }
        if (key == KEY_DOWN) {
            if (tinker_focus_ == TinkerFocus::Workbench) tinker_focus_ = TinkerFocus::Slots;
            else if (tinker_focus_ == TinkerFocus::Slots) tinker_focus_ = TinkerFocus::Synthesizer;
            else if (tinker_focus_ == TinkerFocus::Synthesizer) tinker_focus_ = TinkerFocus::Materials;
        }
        if (tinker_focus_ == TinkerFocus::Slots) {
            if (key == KEY_LEFT && tinker_slot_cursor_ > 0) --tinker_slot_cursor_;
            if (key == KEY_RIGHT && tinker_slot_cursor_ < 2) ++tinker_slot_cursor_;
        }

        // Space: place/remove item on workbench, or slot material
        if (key == ' ') {
            if (tinker_focus_ == TinkerFocus::Workbench) {
                if (workbench_item_) {
                    // Remove item from workbench
                    workbench_item_ = nullptr;
                    workbench_inv_idx_ = -1;
                    context_message_ = "Item removed from workbench.";
                    context_msg_timer_ = 2;
                } else {
                    // Open item picker (rich labels via display_name)
                    context_menu_.reset();
                    context_menu_.title = "Place Item";
                    for (int i = 0; i < static_cast<int>(player_->inventory.items.size()); ++i) {
                        const auto& it = player_->inventory.items[i];
                        if (it.slot.has_value() || it.max_durability > 0 || it.enhancement_slots > 0) {
                            char key_ch = (i < 26) ? ('a' + i) : ('1' + i - 26);
                            context_menu_.add_option(key_ch, display_name(it));
                        }
                    }
                    context_menu_.selection = 0;
                    context_menu_.open = true;
                }
            } else if (tinker_focus_ == TinkerFocus::Slots && workbench_item_) {
                int si = tinker_slot_cursor_;
                if (si < workbench_item_->enhancement_slots) {
                    // Ensure vector is sized
                    while (static_cast<int>(workbench_item_->enhancements.size()) <= si)
                        workbench_item_->enhancements.push_back({});
                    if (!workbench_item_->enhancements[si].filled) {
                        // Open material picker (rich labels via display_name)
                        context_menu_.reset();
                        context_menu_.title = "Select Material";
                        for (int i = 0; i < static_cast<int>(player_->inventory.items.size()); ++i) {
                            const auto& it = player_->inventory.items[i];
                            if (it.type == ItemType::CraftingMaterial && get_material_effect(it.id)) {
                                char key_ch = (i < 26) ? ('a' + i) : ('1' + i - 26);
                                context_menu_.add_option(key_ch, display_name(it));
                            }
                        }
                        context_menu_.selection = 0;
                        context_menu_.open = true;
                    }
                }
            }
        }

        // Synthesizer input
        if (tinker_focus_ == TinkerFocus::Synthesizer) {
            if (key == KEY_LEFT) synth_bp_cursor_ = 0;
            if (key == KEY_RIGHT) synth_bp_cursor_ = 1;
            if (key == ' ' && !player_->learned_blueprints.empty()) {
                context_menu_.reset();
                context_menu_.title = "Select Blueprint";
                for (int i = 0; i < static_cast<int>(player_->learned_blueprints.size()); ++i) {
                    char key_ch = (i < 26) ? ('a' + i) : ('1' + i - 26);
                    context_menu_.add_option(key_ch, player_->learned_blueprints[i].name);
                }
                context_menu_.selection = 0;
                context_menu_.open = true;
            }
            if (key == 'y' && synth_bp1_ >= 0 && synth_bp2_ >= 0 &&
                player_has_skill(*player_, SkillId::Synthesize)) {
                const auto& bp1 = player_->learned_blueprints[synth_bp1_].name;
                const auto& bp2 = player_->learned_blueprints[synth_bp2_].name;
                auto result = synthesize_item(bp1, bp2, *player_, rng_);
                context_message_ = result.message;
                context_msg_timer_ = 4;
                if (result.success) {
                    synth_bp1_ = -1;
                    synth_bp2_ = -1;
                }
            }
        }

        // Refinement picker (workbench-independent)
        if (key == 'R' && player_has_skill(*player_, SkillId::BasicRepair)) {
            tinker_focus_ = TinkerFocus::Refinement;
            context_menu_.reset();
            context_menu_.title = "Refine Material";
            const auto& recipes = refinement_recipes();
            for (int i = 0; i < static_cast<int>(recipes.size()); ++i) {
                std::string label = recipes[i].name + std::string(" — ");
                bool first = true;
                for (const auto& req : recipes[i].inputs) {
                    if (!first) label += " + ";
                    first = false;
                    const MaterialDef* def = find_material(req.material_id);
                    label += std::to_string(req.count) + "x " + (def ? def->name : "?");
                }
                char key_ch = (i < 9) ? ('1' + i) : ('a' + i - 9);
                context_menu_.add_option(key_ch, label);
            }
            context_menu_.selection = 0;
            context_menu_.open = true;
        }

        // Schematic crafting picker (workbench-independent)
        if (key == 'C' && player_has_skill(*player_, SkillId::Cat_Tinkering)) {
            if (player_->learned_schematics.empty()) {
                context_message_ = "No schematics learned. Find one and read it.";
                context_msg_timer_ = 3;
            } else {
                tinker_focus_ = TinkerFocus::Schematics;
                context_menu_.reset();
                context_menu_.title = "Craft from Schematic";
                for (int i = 0; i < static_cast<int>(player_->learned_schematics.size()); ++i) {
                    const auto& ls = player_->learned_schematics[i];
                    const SchematicRecipe* r = find_schematic_recipe(ls.schematic_id);
                    std::string label = ls.name;
                    if (r) {
                        label += " — ";
                        bool first = true;
                        for (const auto& req : r->material_costs) {
                            if (!first) label += " + ";
                            first = false;
                            const MaterialDef* def = find_material(req.material_id);
                            label += std::to_string(req.count) + "x " + (def ? def->name : "?");
                        }
                    }
                    char key_ch = (i < 9) ? ('1' + i) : ('a' + i - 9);
                    context_menu_.add_option(key_ch, label);
                }
                context_menu_.selection = 0;
                context_menu_.open = true;
            }
        }

        // Action hotkeys
        if (workbench_item_) {
            if (key == 'r') {
                auto result = repair_item(*workbench_item_, *player_);
                context_message_ = result.message;
                context_msg_timer_ = 3;
            }
            if (key == 'a') {
                size_t bp_count_before = player_->learned_blueprints.size();
                std::string item_name = workbench_item_->name;
                auto result = analyze_item(*workbench_item_, *player_, rng_);
                context_message_ = result.message;
                context_msg_timer_ = 4;
                // If a new blueprint was learned, create journal entry
                if (player_->learned_blueprints.size() > bp_count_before) {
                    const auto& bp = player_->learned_blueprints.back();
                    // TODO: get world_tick and phase from game — for now use 0
                    player_->journal.push_back(make_blueprint_journal_entry(
                        bp.name, bp.description, item_name, 0, "Unknown"));
                }
                if (result.consumed) {
                    // Item destroyed during analysis — remove from inventory
                    if (workbench_inv_idx_ >= 0 && workbench_inv_idx_ < static_cast<int>(player_->inventory.items.size())) {
                        player_->inventory.items.erase(player_->inventory.items.begin() + workbench_inv_idx_);
                    }
                    workbench_item_ = nullptr;
                    workbench_inv_idx_ = -1;
                }
            }
            if (key == 's') {
                auto result = salvage_item(*workbench_item_, *player_, rng_);
                context_message_ = result.message;
                context_msg_timer_ = 3;
                if (result.consumed) {
                    if (workbench_inv_idx_ >= 0 && workbench_inv_idx_ < static_cast<int>(player_->inventory.items.size())) {
                        player_->inventory.items.erase(player_->inventory.items.begin() + workbench_inv_idx_);
                    }
                    workbench_item_ = nullptr;
                    workbench_inv_idx_ = -1;
                }
            }
            if (key == 'f') {
                // Assemble (finalize) — commit pending enhancements
                auto result = commit_enhancements(*workbench_item_);
                context_message_ = result.message;
                context_msg_timer_ = 3;
                if (result.success) {
                    // Clear workbench after successful assembly
                    workbench_item_ = nullptr;
                    workbench_inv_idx_ = -1;
                }
            }
            if (key == 'x' && tinker_focus_ == TinkerFocus::Slots) {
                // Clear slot — undo pending enhancement
                auto result = clear_enhancement_slot(*workbench_item_, tinker_slot_cursor_, *player_);
                context_message_ = result.message;
                context_msg_timer_ = 3;
            }
        }
    } else if (active_tab_ == PdaTab::Journal) {
        int count = static_cast<int>(player_->journal.size());
        if (count > 0) {
            // List is rendered newest-first, so Up = higher index, Down = lower index
            if (key == KEY_UP) journal_cursor_ = (journal_cursor_ + 1) % count;
            if (key == KEY_DOWN) journal_cursor_ = (journal_cursor_ - 1 + count) % count;
        }
    } else if (active_tab_ == PdaTab::Reputation) {
        int count = static_cast<int>(player_->reputation.size());
        if (count > 0) {
            if (key == KEY_UP && cursor_ > 0) --cursor_;
            if (key == KEY_DOWN && cursor_ < count - 1) ++cursor_;
        }
    } else if (active_tab_ == PdaTab::Cooking) {
        if (!player_has_skill(*player_, SkillId::Cat_Cooking)) return true;
        if (cooking_qty_prompt_active_) {
            handle_cooking_qty_prompt_key(key);
        } else if (cooking_picker_active_) {
            handle_cooking_picker_key(key);
        } else {
            handle_cooking_key(key);
        }
        return true;
    } else if (active_tab_ == PdaTab::Quests) {
        if (quest_cat_expanded_.size() < 4) quest_cat_expanded_.assign(4, true);
        auto vis = build_quest_vis();
        int max_c = static_cast<int>(vis.size()) - 1;

        // Resolve selected quest's reward items (for right-pane focus)
        const Quest* sel_q = nullptr;
        if (quest_cursor_ >= 0 && quest_cursor_ < static_cast<int>(vis.size())) {
            const auto& v = vis[quest_cursor_];
            if (v.kind == QuestVisItem::Kind::Quest) {
                switch (v.qstate) {
                    case QuestVisItem::QState::Active:
                        sel_q = quests_->find_active(v.quest_id); break;
                    case QuestVisItem::QState::Available:
                        for (const auto& q : quests_->available_quests())
                            if (q.id == v.quest_id) { sel_q = &q; break; }
                        break;
                    case QuestVisItem::QState::Completed:
                        for (const auto& q : quests_->completed_quests())
                            if (q.id == v.quest_id) { sel_q = &q; break; }
                        break;
                    case QuestVisItem::QState::Locked:
                        break;
                }
            }
        }
        int reward_item_count = sel_q ? static_cast<int>(sel_q->reward.items.size()) : 0;
        bool right_allowed = reward_item_count > 0 &&
                             sel_q &&
                             quest_cursor_ >= 0 && quest_cursor_ < static_cast<int>(vis.size()) &&
                             (vis[quest_cursor_].qstate == QuestVisItem::QState::Active ||
                              vis[quest_cursor_].qstate == QuestVisItem::QState::Available);

        if (!right_allowed) quest_focus_ = QuestFocus::Left;

        if (key == '\t') {
            if (right_allowed)
                quest_focus_ = (quest_focus_ == QuestFocus::Left)
                                   ? QuestFocus::Right : QuestFocus::Left;
            if (quest_focus_ == QuestFocus::Right) quest_reward_cursor_ = 0;
        } else if (quest_focus_ == QuestFocus::Right) {
            if (reward_item_count == 0) {
                quest_focus_ = QuestFocus::Left;
            } else {
                if (quest_reward_cursor_ >= reward_item_count)
                    quest_reward_cursor_ = reward_item_count - 1;
                if (key == KEY_UP && quest_reward_cursor_ > 0) --quest_reward_cursor_;
                if (key == KEY_DOWN && quest_reward_cursor_ < reward_item_count - 1)
                    ++quest_reward_cursor_;
                if (key == ' ') {
                    look_item_ = &sel_q->reward.items[quest_reward_cursor_];
                    look_open_ = true;
                }
            }
        } else {
            if (key == KEY_UP && quest_cursor_ > 0) --quest_cursor_;
            if (key == KEY_DOWN && quest_cursor_ < max_c) ++quest_cursor_;
            if (quest_cursor_ > max_c) quest_cursor_ = std::max(0, max_c);

            if (quest_cursor_ >= 0 && quest_cursor_ < static_cast<int>(vis.size())) {
                const auto& v = vis[quest_cursor_];
                if (key == ' ' && v.kind == QuestVisItem::Kind::Category) {
                    quest_cat_expanded_[v.cat_idx] = !quest_cat_expanded_[v.cat_idx];
                } else if (key == ' ' && v.kind == QuestVisItem::Kind::ArcHeader) {
                    if (quest_arcs_collapsed_.count(v.arc_id))
                        quest_arcs_collapsed_.erase(v.arc_id);
                    else
                        quest_arcs_collapsed_.insert(v.arc_id);
                }
            }
        }
    } else if (active_tab_ == PdaTab::Hacking) {
        handle_hacking_key(key);
        return true;
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────
// Context menu
// ─────────────────────────────────────────────────────────────────

void PdaScreen::open_context_menu() {
    context_menu_.reset(); // reset

    if (active_tab_ == PdaTab::Ship) {
        if (ship_focus_ == ShipFocus::Equipment) {
            auto slot = static_cast<ShipSlot>(ship_equip_cursor_);
            const auto& item = player_->ship.slot_ref(slot);
            if (!item) return;
            context_menu_.add_option('l', "look");
            context_menu_.add_option('r', "uninstall");
        } else {
            auto& cargo = player_->ship.cargo;
            if (ship_inv_cursor_ < 0 || ship_inv_cursor_ >= static_cast<int>(cargo.size()))
                return;
            context_menu_.add_option('l', "look");
            if (cargo[ship_inv_cursor_].ship_slot.has_value())
                context_menu_.add_option('e', "install");
        }
        context_menu_.selection = 0;
        context_menu_.open = true;
        return;
    }

    if (equip_focus_ == EquipFocus::PaperDoll) {
        // Implant paper-doll uses player_->implants[], not equipment slots.
        if (active_tab_ == PdaTab::Equipment &&
            equipment_tab_view_ == EquipmentTabView::Implants) {
            if (equip_cursor_ < 0 ||
                equip_cursor_ >= static_cast<int>(player_->implants.size())) return;
            const auto& implant = player_->implants[equip_cursor_];
            if (!implant) return;
            context_menu_.add_option('l', "look");
            context_menu_.add_option('r', "remove");
            context_menu_.selection = 0;
            context_menu_.open = true;
            return;
        }
        auto slot = static_cast<EquipSlot>(equip_cursor_);
        const auto& item = player_->equipment.slot_ref(slot);
        if (!item) return;
        context_menu_.add_option('l', "look");
        context_menu_.add_option('r', "remove");
        if (item->energy) {
            context_menu_.add_option('c', "recharge");
        }
        if (item->ranged) {
            context_menu_.add_option('u', "unload");
        }
        for (const auto& enh : item->enhancements) {
            if (enh.committed && enh.solar_panel) {
                context_menu_.add_option('g', enh.solar_panel->active
                                              ? "disable solar panel"
                                              : "enable solar panel");
                break;
            }
        }
    } else {
        if (player_->inventory.items.empty()) return;
        if (inv_cursor_ < 0 || inv_cursor_ >= static_cast<int>(player_->inventory.items.size())) return;
        const auto& item = player_->inventory.items[inv_cursor_];
        context_menu_.add_option('l', "look");
        if (item.type == ItemType::Implant) {
            context_menu_.add_option('e', "install implant");
        } else if (item.slot.has_value()) {
            if (item.type == ItemType::MeleeWeapon) {
                // Melee weapons can go in either hand
                context_menu_.add_option('e', "equip right hand");
                context_menu_.add_option('q', "equip left hand");
            } else {
                context_menu_.add_option('e', "equip");
            }
        }
        if (item.energy) {
            context_menu_.add_option('r', "recharge");
        }
        if (item.ranged) {
            context_menu_.add_option('u', "unload");
        }
        // Solar Panel toggle if any committed slot has one
        for (const auto& enh : item.enhancements) {
            if (enh.committed && enh.solar_panel) {
                context_menu_.add_option('g', enh.solar_panel->active
                                              ? "disable solar panel"
                                              : "enable solar panel");
                break;
            }
        }
        if (!item.energy && item.usable) {
            const char* verb = "use";
            if (item.type == ItemType::Food)     verb = "eat";
            else if (item.type == ItemType::Cookbook) verb = "read";
            context_menu_.add_option('u', verb);
        }
        context_menu_.add_option('d', "drop");
    }

    context_menu_.selection = 0;
    context_menu_.open = true;
}

void PdaScreen::execute_context_action(char key) {
    if (active_tab_ == PdaTab::Ship) {
        if (ship_focus_ == ShipFocus::Equipment) {
            auto slot = static_cast<ShipSlot>(ship_equip_cursor_);
            auto& equipped = player_->ship.slot_ref(slot);
            if (!equipped) return;
            if (key == 'l') {
                look_item_ = &(*equipped);
                look_open_ = true;
            } else if (key == 'r') {
                context_message_ = "Uninstalled " + equipped->label() + ".";
                context_msg_timer_ = 3;
                player_->ship.cargo.push_back(std::move(*equipped));
                equipped.reset();
            }
        } else {
            auto& cargo = player_->ship.cargo;
            if (ship_inv_cursor_ < 0 || ship_inv_cursor_ >= static_cast<int>(cargo.size()))
                return;
            auto& item = cargo[ship_inv_cursor_];
            if (key == 'l') {
                look_item_ = &item;
                look_open_ = true;
            } else if (key == 'e' && item.ship_slot.has_value()) {
                ShipSlot target = *item.ship_slot;
                auto& sl = player_->ship.slot_ref(target);
                Item to_install = std::move(item);
                cargo.erase(cargo.begin() + ship_inv_cursor_);
                if (sl) cargo.push_back(std::move(*sl));
                sl = std::move(to_install);
                context_message_ = "Installed " + sl->label() + ".";
                context_msg_timer_ = 3;
                installed_ship_slot_ = ship_slot_name(target);
                if (ship_inv_cursor_ >= static_cast<int>(cargo.size()) && ship_inv_cursor_ > 0)
                    --ship_inv_cursor_;
            }
        }
        return;
    }
    if (equip_focus_ == EquipFocus::PaperDoll) {
        // Implant paper-doll routes to player_->implants[] instead.
        if (active_tab_ == PdaTab::Equipment &&
            equipment_tab_view_ == EquipmentTabView::Implants) {
            if (equip_cursor_ < 0 ||
                equip_cursor_ >= static_cast<int>(player_->implants.size())) return;
            auto& implant = player_->implants[equip_cursor_];
            if (!implant) return;
            if (key == 'l') {
                look_item_ = &(*implant);
                look_open_ = true;
            } else if (key == 'r') {
                if (!player_->inventory.can_add(*implant)) {
                    context_message_ = "Inventory too heavy.";
                    context_msg_timer_ = 3;
                    return;
                }
                context_message_ = "Removed " + implant->label() + ".";
                context_msg_timer_ = 3;
                player_->inventory.items.push_back(std::move(*implant));
                implant.reset();
            }
            return;
        }
        auto slot = static_cast<EquipSlot>(equip_cursor_);
        auto& equipped = player_->equipment.slot_ref(slot);
        if (!equipped) return;

        if (key == 'l') {
            look_item_ = &(*equipped);
            look_open_ = true;
        } else if (key == 'r') {
            if (!player_->inventory.can_add(*equipped)) {
                context_message_ = "Inventory too heavy.";
                context_msg_timer_ = 3;
                return;
            }
            context_message_ = "Removed " + equipped->label() + ".";
            context_msg_timer_ = 3;
            // Clear shield affinity on unequip
            if (slot == EquipSlot::Shield) {
                player_->shield_affinity = {};
            }
            player_->inventory.items.push_back(std::move(*equipped));
            equipped.reset();
        } else if (key == 'c') {
            // Recharge equipped item — route to weapon or shield picker
            if (slot == EquipSlot::Shield) {
                recharge_equipped_request_ = 1;  // shield
            } else if (slot == EquipSlot::Missile) {
                recharge_equipped_request_ = 0;  // weapon
            }
        } else if (key == 'g') {
            bool handled = false;
            for (auto& enh : equipped->enhancements) {
                if (enh.committed && enh.solar_panel) {
                    enh.solar_panel->active = !enh.solar_panel->active;
                    context_message_ = std::string("Solar Panel ") +
                                       (enh.solar_panel->active ? "enabled." : "disabled.");
                    context_msg_timer_ = 3;
                    handled = true;
                    break;
                }
            }
            if (!handled && equipped->toggleable) {
                if (item_has_active_module(*equipped)) {
                    context_message_ = "Auto mode (" + active_module_name(*equipped) +
                                       ") — manual toggle disabled.";
                    context_msg_timer_ = 3;
                } else {
                    equipped->active = !equipped->active;
                    context_message_ = equipped->label() + (equipped->active ? " on." : " off.");
                    context_msg_timer_ = 3;
                }
            }
        } else if (key == 'u') {
            if (equipped->ranged && equipped->energy && equipped->energy->current > 0) {
                context_message_ = "Unloaded " + std::to_string(equipped->energy->current) + " charge.";
                equipped->energy->current = 0;
            } else {
                context_message_ = "Nothing to unload.";
            }
            context_msg_timer_ = 3;
        }
    } else {
        auto& items = player_->inventory.items;
        if (inv_cursor_ < 0 || inv_cursor_ >= static_cast<int>(items.size())) return;

        if (key == 'l') {
            look_item_ = &items[inv_cursor_];
            look_open_ = true;
        } else if (key == 'e' || key == 'q') {
            auto& item = items[inv_cursor_];
            if (item.type == ItemType::Implant) {
                // Implants go into player_.implants[], not EquipSlot slots.
                bool installed = false;
                for (size_t i = 0; i < player_->implants.size(); ++i) {
                    if (!player_->implants[i]) {
                        Item to_install = std::move(item);
                        items.erase(items.begin() + inv_cursor_);
                        context_message_ = "Installed " + to_install.name +
                                           " in implant slot " + std::to_string(i + 1) + ".";
                        context_msg_timer_ = 3;
                        player_->implants[i] = std::move(to_install);
                        installed = true;
                        break;
                    }
                }
                if (!installed) {
                    context_message_ = "All implant slots are occupied.";
                    context_msg_timer_ = 3;
                }
                if (inv_cursor_ >= static_cast<int>(items.size()) && inv_cursor_ > 0)
                    --inv_cursor_;
            } else if (item.slot) {
                EquipSlot target_slot = *item.slot;
                // 'q' = left hand for melee weapons
                if (key == 'q' && item.type == ItemType::MeleeWeapon) {
                    target_slot = EquipSlot::LeftHand;
                }
                Item to_equip = std::move(item);
                items.erase(items.begin() + inv_cursor_);

                if (to_equip.type == ItemType::Cyberdeck) {
                    // Cyberdeck-specific routing: prefer swapping an already-equipped
                    // deck; otherwise fill the first empty utility slot; otherwise
                    // displace utility1.
                    auto& eq = player_->equipment;
                    if (eq.utility1 && eq.utility1->type == ItemType::Cyberdeck) {
                        items.push_back(std::move(*eq.utility1));
                        eq.utility1 = std::move(to_equip);
                        context_message_ = "Equipped " + eq.utility1->label() + ".";
                    } else if (eq.utility2 && eq.utility2->type == ItemType::Cyberdeck) {
                        items.push_back(std::move(*eq.utility2));
                        eq.utility2 = std::move(to_equip);
                        context_message_ = "Equipped " + eq.utility2->label() + ".";
                    } else if (!eq.utility1) {
                        eq.utility1 = std::move(to_equip);
                        context_message_ = "Equipped " + eq.utility1->label() + ".";
                    } else if (!eq.utility2) {
                        eq.utility2 = std::move(to_equip);
                        context_message_ = "Equipped " + eq.utility2->label() + ".";
                    } else {
                        items.push_back(std::move(*eq.utility1));
                        eq.utility1 = std::move(to_equip);
                        context_message_ = "Equipped " + eq.utility1->label() + ".";
                    }
                } else {
                    auto& sl = player_->equipment.slot_ref(target_slot);
                    // Clear shield affinity when swapping out old shield
                    if (target_slot == EquipSlot::Shield && sl) {
                        player_->shield_affinity = {};
                    }
                    if (sl) items.push_back(std::move(*sl));
                    sl = std::move(to_equip);
                    // Sync shield affinity from newly equipped shield
                    if (target_slot == EquipSlot::Shield) {
                        player_->shield_affinity = sl->type_affinity;
                    }
                    context_message_ = "Equipped " + sl->label() + ".";
                }
                context_msg_timer_ = 3;
                if (inv_cursor_ >= static_cast<int>(items.size()) && inv_cursor_ > 0)
                    --inv_cursor_;
            }
        } else if (key == 'r') {
            recharge_request_idx_ = inv_cursor_;
        } else if (key == 'g') {
            auto& item = items[inv_cursor_];
            bool handled = false;
            for (auto& enh : item.enhancements) {
                if (enh.committed && enh.solar_panel) {
                    enh.solar_panel->active = !enh.solar_panel->active;
                    context_message_ = std::string("Solar Panel ") +
                                       (enh.solar_panel->active ? "enabled." : "disabled.");
                    context_msg_timer_ = 3;
                    handled = true;
                    break;
                }
            }
            if (!handled && item.toggleable) {
                if (item_has_active_module(item)) {
                    context_message_ = "Auto mode (" + active_module_name(item) +
                                       ") — manual toggle disabled.";
                    context_msg_timer_ = 3;
                } else {
                    item.active = !item.active;
                    context_message_ = item.label() + (item.active ? " on." : " off.");
                    context_msg_timer_ = 3;
                }
            }
        } else if (key == 'u') {
            auto& item = items[inv_cursor_];
            if (item.ranged) {
                if (item.energy && item.energy->current > 0) {
                    context_message_ = "Unloaded " + std::to_string(item.energy->current) + " charge.";
                    item.energy->current = 0;
                } else {
                    context_message_ = "Nothing to unload.";
                }
                context_msg_timer_ = 3;
            } else if (item.usable) {
                // Hand off to Game::use_item via the consume-request channel.
                use_item_request_idx_ = inv_cursor_;
            }
        } else if (key == 'd') {
            auto& item = items[inv_cursor_];
            context_message_ = "Dropped " + item.label() + ".";
            context_msg_timer_ = 3;
            dropped_item_ = std::move(item);
            has_dropped_item_ = true;
            items.erase(items.begin() + inv_cursor_);
            if (inv_cursor_ >= static_cast<int>(items.size()) && inv_cursor_ > 0)
                --inv_cursor_;
        }
    }
}


void PdaScreen::draw_context_menu(int screen_w, int screen_h) {
    if (!context_menu_.open) return;

    const auto& opts = context_menu_.options;
    int sel = context_menu_.selection;

    // Get the item being acted on for entity header
    const Item* ctx_item = nullptr;
    if (player_ && active_tab_ == PdaTab::Equipment) {
        if (equip_focus_ == EquipFocus::Inventory &&
            inv_cursor_ >= 0 && inv_cursor_ < static_cast<int>(player_->inventory.items.size())) {
            ctx_item = &player_->inventory.items[inv_cursor_];
        } else if (equip_focus_ == EquipFocus::PaperDoll) {
            auto slot = static_cast<EquipSlot>(equip_cursor_);
            const auto& equipped = player_->equipment.slot_ref(slot);
            if (equipped) ctx_item = &(*equipped);
        }
    } else if (player_ && active_tab_ == PdaTab::Ship) {
        if (ship_focus_ == ShipFocus::Inventory &&
            ship_inv_cursor_ >= 0 && ship_inv_cursor_ < static_cast<int>(player_->ship.cargo.size())) {
            ctx_item = &player_->ship.cargo[ship_inv_cursor_];
        } else if (ship_focus_ == ShipFocus::Equipment) {
            auto slot = static_cast<ShipSlot>(ship_equip_cursor_);
            const auto& installed = player_->ship.slot_ref(slot);
            if (installed) ctx_item = &(*installed);
        }
    }

    // Compute dimensions — wider with padding (labels may carry COLOR markers).
    int max_label = 0;
    for (const auto& o : opts) {
        int len = UIContext::rich_visible_length(o.label) + 6; // "  [x] label  "
        if (len > max_label) max_label = len;
    }
    int win_w = std::max(max_label + 6, 30);

    // Height: header + blank + options with spacing + blank + chrome
    int content_h = 0;
    if (ctx_item) {
        content_h += 3; // glyph + name + separator
    } else if (!context_menu_.title.empty()) {
        content_h += 2; // title + separator
    }
    content_h += 1; // blank before options
    content_h += static_cast<int>(opts.size()) * 2 - 1; // options with blank lines between
    content_h += 1; // blank after options
    int chrome_h = 2 + 1; // borders + footer
    int win_h = content_h + chrome_h;

    int mx = (screen_w - win_w) / 2;
    int my = (screen_h - win_h) / 2;

    UIContext full(renderer_, Rect{mx, my, win_w, win_h});
    auto pc = full.panel({.footer = "[Esc] Cancel"});

    int cw = pc.width();
    int y = 0;

    // Header: entity header for items, title for other menus
    if (ctx_item) {
        EntityRef entity{EntityRef::Kind::Item, ctx_item->item_def_id};
        int glyph_x = cw / 2;
        pc.styled_text({.x = glyph_x, .y = y, .segments = {
            {"?", UITag::TextDefault, entity},
        }});
        y++;

        int name_x = (cw - static_cast<int>(ctx_item->name.size())) / 2;
        if (name_x < 1) name_x = 1;
        pc.text({.x = name_x, .y = y, .content = ctx_item->name, .tag = rarity_tag(ctx_item->rarity)});
        y++;

        pc.sub(Rect{0, y, cw, 1}).separator({});
        y++;
    } else {
        // Title header for non-item menus (tinkering: Place Item, Select Material, etc.)
        const auto& title = context_menu_.title;
        if (!title.empty()) {
            int tx = (cw - static_cast<int>(title.size())) / 2;
            if (tx < 1) tx = 1;
            pc.text({.x = tx, .y = y, .content = title, .tag = UITag::TextBright});
            y++;

            pc.sub(Rect{0, y, cw, 1}).separator({});
            y++;
        }
    }

    y++; // blank before options

    // Options with conversation-style spacing.
    // Labels may carry COLOR markers (display_name output) — render with text_rich.
    for (int i = 0; i < static_cast<int>(opts.size()); ++i) {
        bool is_sel = (i == sel);
        std::string prefix = is_sel ? "> " : "  ";
        UITag prefix_tag = is_sel ? UITag::OptionSelected : UITag::OptionNormal;
        std::string head = prefix + "[" + opts[i].key + "] ";
        pc.text({.x = 2, .y = y, .content = head, .tag = prefix_tag});
        pc.text_rich(2 + static_cast<int>(head.size()), y, opts[i].label);
        y += 2; // blank line between options
    }
}

void PdaScreen::draw_look_overlay(UIContext& ctx) {
    if (!look_open_ || !look_item_) return;
    const auto& item = *look_item_;

    int win_w = 44;
    int content_h = 20;
    int chrome_h = 2 + 1; // borders + footer
    int win_h = content_h + chrome_h + 3; // +3 for entity header
    if (win_h > ctx.height() - 4) win_h = ctx.height() - 4;

    int mx = (ctx.width() - win_w) / 2;
    int my = (ctx.height() - win_h) / 2;

    UIContext full(renderer_, Rect{ctx.bounds().x + mx, ctx.bounds().y + my, win_w, win_h});
    auto panel_content = full.panel({.footer = "[any key] Close"});

    int cw = panel_content.width();
    int y = 0;

    // Entity header: glyph centered
    EntityRef entity{EntityRef::Kind::Item, item.item_def_id};
    int glyph_x = cw / 2;
    panel_content.styled_text({.x = glyph_x, .y = y, .segments = {
        {"?", UITag::TextDefault, entity},
    }});
    y++;

    // Item name centered — display_name() wraps glyph + name + slots + dice + energy + stack.
    // Skip the leading "<glyph> " prefix here since we already drew the glyph above.
    std::string rich = display_name(item);
    // Strip the leading glyph + space (the glyph is shown in the centered header).
    // display_name() format starts with: COLOR_BEGIN <c> <glyph utf8> COLOR_END " "
    auto strip_prefix = [](std::string s) -> std::string {
        size_t i = 0;
        if (i < s.size() && s[i] == COLOR_BEGIN) {
            i += 2;
            while (i < s.size() && s[i] != COLOR_END) ++i;
            if (i < s.size()) ++i;
        }
        if (i < s.size() && s[i] == ' ') ++i;
        return s.substr(i);
    };
    std::string body = strip_prefix(rich);
    int name_x = (cw - panel_content.rich_visible_length(body)) / 2;
    if (name_x < 1) name_x = 1;
    panel_content.text_rich(name_x, y, body);
    y++;

    // Separator between header and content
    panel_content.sub(Rect{0, y, cw, 1}).separator({});
    y++;

    // Item info in remaining space — with left/right padding
    int pad = 2;
    auto info_area = panel_content.sub(Rect{pad, y, cw - pad * 2, panel_content.height() - y});
    draw_item_info(info_area, item);
}

void PdaScreen::draw(int screen_w, int screen_h) {
    if (!open_ || !renderer_) return;

    // Compute footer text based on active tab
    std::string footer_text;
    if (active_tab_ == PdaTab::Tinkering) {
        // Catalog is tabbed: [C] Craft only makes sense in the Schematics tab.
        bool in_schem_tab = (tinker_focus_ == TinkerFocus::Catalog
                             && catalog_tab_ == CatalogTab::Schematics);
        bool craft_visible = (tinker_focus_ != TinkerFocus::Catalog) || in_schem_tab;
        footer_text = "[ESC] Close  [\xe2\x86\x91\xe2\x86\x93] Nav  [Tab] Catalog  [Space] Select  [r] Repair  [a] Analyze  [s] Salvage  [f] Assemble  [x] Clear  [y] Synth  [R] Refine";
        if (craft_visible) footer_text += "  [C] Craft";
        if (tinker_focus_ == TinkerFocus::Catalog) {
            footer_text += "  [\xe2\x86\x90\xe2\x86\x92] Tab";
        }
    } else if (active_tab_ == PdaTab::Skills) {
        footer_text = "[ESC] Close  [\xe2\x86\x91\xe2\x86\x93] Navigate  [Space] Expand  [l] Learn";
    } else if (active_tab_ == PdaTab::Cooking) {
        footer_text = "[ESC] Close  [Tab] Focus  [\xe2\x86\x90\xe2\x86\x92] Slot  [Space] Add  [x] Clear  [c] Cook";
    } else if (active_tab_ == PdaTab::Equipment) {
        footer_text = "[ESC] Close  [\xe2\x86\x91\xe2\x86\x93] Navigate  [Space] Interact";
    } else if (has_pending()) {
        footer_text = "[ESC] Close  [\xe2\x86\x91\xe2\x86\x93] Navigate  [-/+] Adjust  [Space] Commit";
    } else {
        footer_text = "[ESC] Close  [\xe2\x86\x91\xe2\x86\x93] Navigate";
    }

    // Outer panel via semantic UI
    int pad_x = 2;
    int pad_y = 2;
    int win_w = screen_w - pad_x * 2;
    int win_h = screen_h - pad_y * 2;
    UIContext outer(renderer_, Rect{pad_x, pad_y, win_w, win_h});
    auto ctx = outer.panel({.title = "PDA", .footer = footer_text});

    // Tab bar + separator + content via semantic layout
    std::vector<std::string> tabs(std::begin(tab_names), std::end(tab_names));
    auto layout = ctx.rows({fixed(1), fixed(1), fill()});
    layout[0].tab_bar({
        .tabs = tabs,
        .active = static_cast<int>(active_tab_),
        .align = TextAlign::Center,
        .show_nav = true,
    });
    layout[1].separator({});
    auto& tab_area = layout[2];

    // Full-width area for section headers that span full width
    UIContext full = tab_area;
    // Padded content area for tab content
    int pad = 3;
    UIContext content = tab_area.sub(Rect{pad, 0, tab_area.width() - pad * 2, tab_area.height()});

    switch (active_tab_) {
        case PdaTab::Attributes: draw_attributes(content); break;
        case PdaTab::Skills:     draw_skills(full); break;
        case PdaTab::Equipment:  draw_equipment(content); break;
        case PdaTab::Reputation: draw_reputation(content); break;
        case PdaTab::Tinkering:  draw_tinkering(full); break;
        case PdaTab::Journal:    draw_journal(content); break;
        case PdaTab::Quests:    draw_quests(content); break;
        case PdaTab::Ship:       draw_ship(content); break;
        case PdaTab::Cooking:    draw_cooking(full); break;
        case PdaTab::Hacking:    draw_hacking(content); break;
    }

    // Draw vertical divider only for tabs that use a split layout
    bool needs_divider = (active_tab_ == PdaTab::Attributes
                       || active_tab_ == PdaTab::Skills
                       || active_tab_ == PdaTab::Equipment
                       || active_tab_ == PdaTab::Ship
                       || active_tab_ == PdaTab::Quests
                       || (active_tab_ == PdaTab::Tinkering && player_has_skill(*player_, SkillId::Cat_Tinkering))
                       || (active_tab_ == PdaTab::Cooking && player_has_skill(*player_, SkillId::Cat_Cooking))
                       || (active_tab_ == PdaTab::Journal && !player_->journal.empty()));
    if (needs_divider) {
        int divider_x = content.width() / 2;
        // The ┬ on the separator row needs to align with the │ in the content area.
        // content is offset by pad from tab_area, so add pad for layout[1] coordinates.
        int sep_divider_x = divider_x + pad;
        int last = content.height() - 1;
        layout[1].put(sep_divider_x, 0, BoxDraw::TT, Color::DarkGray);  // ┬ connects to tab separator
        for (int vy = 0; vy < last; ++vy) {
            content.put(divider_x, vy, BoxDraw::V, Color::DarkGray);
        }
        content.put(divider_x, last, BoxDraw::BT, Color::DarkGray); // ┴ at bottom
    }

    // Context menu overlay (equipment tab)
    draw_context_menu(screen_w, screen_h);

    // Look overlay
    draw_look_overlay(content);

    // Status message at bottom of content
    if (context_msg_timer_ > 0 && !context_message_.empty()) {
        int msg_x = content.width() / 2 - static_cast<int>(context_message_.size()) / 2;
        content.text({.x = msg_x, .y = content.height() - 1,
                      .content = context_message_, .tag = UITag::TextSuccess});
    }

    // Tab help overlay (shown once per tab for non-dev players)
    if (showing_tab_help_) {
        draw_tab_help(screen_w, screen_h);
    }
}

// (Tab bar drawing now handled by semantic UIContext::tab_bar in draw())

// ─────────────────────────────────────────────────────────────────
// Stat box drawing helper
// ─────────────────────────────────────────────────────────────────

void PdaScreen::draw_stat_box(UIContext& ctx, int x, int y,
                                     const char* label, int value,
                                     bool selected, int modifier,
                                     int pending, bool can_allocate) {
    // Box is 7 wide. Height: 3 (label + value) or 4 (+ modifier row)
    bool has_mod = (modifier != -999);
    int h = has_mod ? 4 : 3;
    Color border_color = selected ? Color::Yellow : Color::DarkGray;

    // Top border
    ctx.put(x, y, BoxDraw::TL, border_color);
    for (int i = 1; i < 6; ++i) ctx.put(x + i, y, BoxDraw::H, border_color);
    ctx.put(x + 6, y, BoxDraw::TR, border_color);

    // Label row
    ctx.put(x, y + 1, BoxDraw::V, border_color);
    std::string lbl(label);
    int pad = static_cast<int>(5 - lbl.size()) / 2;
    ctx.text({.x = x + 1 + pad, .y = y + 1, .content = lbl,
              .tag = selected ? UITag::TextWarning : UITag::TextAccent});
    ctx.put(x + 6, y + 1, BoxDraw::V, border_color);

    // Value row — green if has pending points
    ctx.put(x, y + 2, BoxDraw::V, border_color);
    std::string val = std::to_string(value);
    int vpad = static_cast<int>(5 - val.size()) / 2;
    UITag val_tag = (pending > 0) ? UITag::TextSuccess : UITag::TextBright;
    ctx.text({.x = x + 1 + vpad, .y = y + 2, .content = val, .tag = val_tag});
    ctx.put(x + 6, y + 2, BoxDraw::V, border_color);

    // Modifier row (primary attributes only)
    if (has_mod) {
        ctx.put(x, y + 3, BoxDraw::V, border_color);
        std::string mod_str;
        UITag mod_tag;
        if (modifier > 0) {
            mod_str = "[+" + std::to_string(modifier) + "]";
            mod_tag = UITag::TextSuccess;
        } else if (modifier < 0) {
            mod_str = "[" + std::to_string(modifier) + "]";
            mod_tag = UITag::TextDanger;
        } else {
            mod_str = "[ 0]";
            mod_tag = UITag::TextDim;
        }
        int mpad = static_cast<int>(5 - mod_str.size()) / 2;
        ctx.text({.x = x + 1 + mpad, .y = y + 3, .content = mod_str, .tag = mod_tag});
        ctx.put(x + 6, y + 3, BoxDraw::V, border_color);
    }

    // Bottom border — show -/+ hint when allocatable
    int bot = y + h;
    ctx.put(x, bot, BoxDraw::BL, border_color);
    bool show_hint = (pending > 0) || (selected && can_allocate);
    if (show_hint) {
        ctx.put(x + 1, bot, BoxDraw::H, border_color);
        ctx.styled_text({.x = x + 2, .y = bot, .segments = {
            {"-", UITag::KeyLabel}, {"/", UITag::TextDim}, {"+", UITag::KeyLabel}
        }});
        ctx.put(x + 5, bot, BoxDraw::H, border_color);
    } else {
        for (int i = 1; i < 6; ++i) ctx.put(x + i, bot, BoxDraw::H, border_color);
    }
    ctx.put(x + 6, bot, BoxDraw::BR, border_color);
}

void PdaScreen::draw_section_header(UIContext& ctx, int y,
                                           const char* title, int left_margin,
                                           int right_edge) {
    // ──┤ TITLE ├──────── (stops before right_edge)
    if (right_edge < 0) right_edge = ctx.width() / 2;
    // Leading ──
    ctx.put(left_margin, y, BoxDraw::H, Color::DarkGray);
    ctx.put(left_margin + 1, y, BoxDraw::H, Color::DarkGray);
    // ┤
    ctx.put(left_margin + 2, y, BoxDraw::RT, Color::DarkGray);
    // space + TITLE + space — title may contain UTF-8 multi-byte glyphs
    // so use visible cell count, not byte count.
    ctx.put(left_margin + 3, y, ' ');
    ctx.text({.x = left_margin + 4, .y = y, .content = title, .tag = UITag::TextBright});
    int title_cells = UIContext::rich_visible_length(title);
    int after_title = left_margin + 4 + title_cells;
    ctx.put(after_title, y, ' ');
    // ├
    ctx.put(after_title + 1, y, BoxDraw::LT, Color::DarkGray);
    // Trailing ──
    for (int x = after_title + 2; x < right_edge; ++x) {
        ctx.put(x, y, BoxDraw::H, Color::DarkGray);
    }
}

// ─────────────────────────────────────────────────────────────────
// Stub tab
// ─────────────────────────────────────────────────────────────────

void PdaScreen::draw_stub(UIContext& ctx, const char* message) {
    ctx.text({.x = ctx.width() / 2 - static_cast<int>(std::string(message).size()) / 2,
              .y = ctx.height() / 2, .content = message, .tag = UITag::TextDim});
}

// ─────────────────────────────────────────────────────────────────
// Tab help — shown once per tab for new players (block-layout popup)
// ─────────────────────────────────────────────────────────────────

static const char* tab_help_body(PdaTab tab) {
    switch (tab) {
        case PdaTab::Skills:
            return "Your learned skills and available skill trees.\n\n"
                   "Skills are organized into categories. Expand a "
                   "category to see individual skills. Spend skill "
                   "points to learn new abilities.\n\n"
                   "[Space] Expand/collapse category\n"
                   "[l] Learn a skill (costs SP)";
        case PdaTab::Attributes:
            return "Your primary attributes define your character.\n\n"
                   "Attributes affect combat, health, and more. "
                   "When you level up you gain attribute points "
                   "that can be spent here.\n\n"
                   "[-/+] Adjust allocation\n"
                   "[Space] Commit changes";
        case PdaTab::Equipment:
            return "Manage your personal gear and inventory.\n\n"
                   "Left side shows your equipped items. Right "
                   "side shows your inventory (backpack).\n\n"
                   "[Tab] Switch between equipped/inventory\n"
                   "[e] Equip an item from inventory\n"
                   "[r] Remove equipped item\n"
                   "[d] Drop item from inventory\n"
                   "[l] Look at item details";
        case PdaTab::Tinkering:
            return "Analyze, repair, and enhance your gear.\n\n"
                   "Place an item on the workbench to work on it. "
                   "Items can be repaired, analyzed for blueprints, "
                   "salvaged for materials, or enhanced with mods.\n\n"
                   "[r] Repair item\n"
                   "[a] Analyze for blueprints\n"
                   "[s] Salvage for materials\n"
                   "[f] Assemble from blueprints";
        case PdaTab::Journal:
            return "Your personal log of discoveries and events.\n\n"
                   "Entries are added as you explore the galaxy. "
                   "Check here for lore, encounter notes, and "
                   "important story moments.";
        case PdaTab::Quests:
            return "Track your active and completed quests.\n\n"
                   "Active quests show their objectives and your "
                   "progress toward completing them. Completed "
                   "quests are listed below.\n\n"
                   "Rewards are shown for each active quest.";
        case PdaTab::Reputation:
            return "Your standing with the galaxy's factions.\n\n"
                   "Reputation affects prices, dialog options, "
                   "and quest availability. Help a faction to "
                   "improve your standing. Hostile actions will "
                   "lower it.\n\n"
                   "Tiers: Hated < Disliked < Neutral < Liked < Trusted";
        case PdaTab::Ship:
            return "Your starship's components and diagnostics.\n\n"
                   "Install and manage ship components here. "
                   "Critical systems must be online before you "
                   "can travel between star systems.\n\n"
                   "[Tab] Switch components/cargo\n"
                   "[Space] Install or uninstall a component\n\n"
                   "Board your ship to manage equipment.";
        case PdaTab::Cooking:
            return "The kitchen. Combine ingredients in the cooking pot "
                   "and craft dishes at a campfire, stove, or kitchen.\n\n"
                   "[Tab] switch between pot slots, ingredients, cookbook\n"
                   "[Enter] on an ingredient to add it to a slot\n"
                   "[x] on a slot to clear it\n"
                   "[c] cook the slotted ingredients\n\n"
                   "You must be near a cooking fire to cook.";
        case PdaTab::Hacking:
            return "Cyberdeck terminal.\n\n"
                   "Type commands at the prompt. 'help' lists all commands. "
                   "Tab to autocomplete. Up/Down walk history. Left/Right "
                   "edit in place. PgUp/PgDn scroll the buffer.\n\n"
                   "nmap [-l|-m]      list or map LAN nodes\n"
                   "jack -t <node>    jack into a network node (Cat_Hacking)\n\n"
                   "[H in world] Quickhack a hackable target\n"
                   "[?] help / [P] ps / [I] ls / [N] nmap / [L] lore";
    }
    return "";
}

static const char* tab_help_title(PdaTab tab) {
    switch (tab) {
        case PdaTab::Skills:     return "Skills";
        case PdaTab::Attributes: return "Attributes";
        case PdaTab::Equipment:  return "Inventory & Equipment";
        case PdaTab::Tinkering:  return "Tinkering";
        case PdaTab::Journal:    return "Journal";
        case PdaTab::Quests:     return "Quests";
        case PdaTab::Reputation: return "Reputation";
        case PdaTab::Ship:       return "Ship";
        case PdaTab::Cooking:    return "Cooking";
        case PdaTab::Hacking:    return "Hacking";
    }
    return "";
}

void PdaScreen::draw_tab_help(int screen_w, int screen_h) {
    if (!tab_help_menu_.open) return;

    int win_w = static_cast<int>(screen_w * 0.45f);
    if (win_w < 30) win_w = 30;

    // Word-wrap body
    int inner_w = win_w - 4;
    std::vector<std::string> body_lines;
    if (!tab_help_menu_.body.empty()) {
        std::string line;
        int vis_len = 0;
        for (char ch : tab_help_menu_.body) {
            if (ch == '\n') {
                body_lines.push_back(line);
                line.clear();
                vis_len = 0;
                continue;
            }
            line += ch;
            ++vis_len;
            if (vis_len >= inner_w) {
                auto sp = line.rfind(' ');
                if (sp != std::string::npos && sp > 0) {
                    body_lines.push_back(line.substr(0, sp));
                    line = line.substr(sp + 1);
                    vis_len = static_cast<int>(line.size());
                } else {
                    body_lines.push_back(line);
                    line.clear();
                    vis_len = 0;
                }
            }
        }
        if (!line.empty()) body_lines.push_back(line);
    }

    int option_count = static_cast<int>(tab_help_menu_.options.size());
    int body_h = tab_help_menu_.body.empty() ? 0 : static_cast<int>(body_lines.size()) + 2;
    int content_h = body_h + 1 + option_count * 2 - 1 + 1;
    int chrome_h = 2 + 2 + (tab_help_menu_.footer.empty() ? 0 : 1);
    int win_h = content_h + chrome_h;

    int wx = (screen_w - win_w) / 2;
    int wy = (screen_h - win_h) / 2;

    UIContext full(renderer_, Rect{wx, wy, win_w, win_h});
    auto ctx = full.panel({.title = tab_help_menu_.title, .footer = tab_help_menu_.footer});

    int y = 0;
    for (const auto& bl : body_lines) {
        ctx.text(1, y, bl, Color::Cyan);
        y++;
    }
    if (!body_lines.empty()) y++;

    std::vector<ListItem> items;
    int sel = tab_help_menu_.selection;
    for (int i = 0; i < option_count; ++i) {
        std::string label = "[" + std::string(1, tab_help_menu_.options[i].key) + "] " + tab_help_menu_.options[i].label;
        items.push_back({label, UITag::OptionNormal, i == sel});
    }
    int list_h = ctx.height() - y;
    if (list_h > 0) {
        auto list_area = ctx.sub(Rect{0, y, ctx.width(), list_h});
        list_area.list({.items = items, .tag = UITag::ConversationOption, .selected_tag = UITag::OptionSelected});
    }
}

void PdaScreen::show_tab_help() {
    int tab_bit = 1 << static_cast<int>(active_tab_);
    if (player_->player_class == PlayerClass::DevCommander) return;
    if (player_->tab_help_seen & tab_bit) return;

    tab_help_menu_.reset();
    tab_help_menu_.title = tab_help_title(active_tab_);
    tab_help_menu_.body = tab_help_body(active_tab_);
    tab_help_menu_.add_option('f', "Got it");
    tab_help_menu_.footer = "[Space] Dismiss";
    tab_help_menu_.selection = 0;
    tab_help_menu_.open = true;
    showing_tab_help_ = true;
}

} // namespace astra
