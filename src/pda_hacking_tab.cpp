#include "astra/pda_screen.h"

#include "astra/cyberdeck.h"
#include "astra/item_defs.h"
#include "astra/item_ids.h"
#include "astra/program.h"
#include "astra/skill_defs.h"

#include <sstream>

namespace astra {

namespace {

bool has_cat_hacking(const Player& p) {
    return player_has_skill(p, SkillId::Cat_Hacking);
}

std::vector<std::string> tokenize_(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) out.push_back(std::move(tok));
    return out;
}

// Returns the prompt prefix for the currently-equipped deck.
// Matches the greeter logo so the terminal feels like the deck's shell.
const char* prompt_for_deck(uint16_t deck_def_id) {
    switch (deck_def_id) {
        case ITEM_PIDGIN_MK1:    return "pidgin$ ";
        case ITEM_POLYGLOT_DCK2: return "dck-2> ";
        default:                 return "pda> ";
    }
}

} // namespace

void PdaScreen::draw_hacking(UIContext& ctx) {
    if (!has_cat_hacking(*player_)) {
        // Locked splash. The .qh programs in the deck stay visible because
        // the H-key flow doesn't require Cat_Hacking — only jacking in does.
        int cy = ctx.height() / 2 - 4;
        // Title is 22 visible columns wide. Use ASCII chevrons to keep
        // centering trivial — the Unicode guillemets render as 2 cols
        // each in our terminal which makes byte-based centering wrong.
        const char* title = "-- HACKING (LOCKED) --";
        const int title_cols = 22;
        ctx.text({.x = ctx.width() / 2 - title_cols / 2, .y = cy,
                  .content = title,
                  .tag = UITag::TextDim});
        ctx.text({.x = 4, .y = cy + 2,
                  .content = "Unlock 'Hacking' in the Skills tab to access the deck terminal.",
                  .tag = UITag::TextDim});
        ctx.text({.x = 4, .y = cy + 4,
                  .content = "Quickhacks (.qh) work without this skill — see below.",
                  .tag = UITag::TextDim});

        ctx.text({.x = 4, .y = cy + 6,
                  .content = "Loaded quickhacks:",
                  .tag = UITag::TextDefault});
        int row = cy + 7;
        auto* ht_deck_slot = player_->equipment.equipped_cyberdeck();
        if (ht_deck_slot && *ht_deck_slot && (*ht_deck_slot)->deck) {
            auto& deck = *(*ht_deck_slot)->deck;
            int found = 0;
            for (int i = 0; i < deck.stats.slots; ++i) {
                if (deck.loaded[i].program_def_id == 0) continue;
                Item probe = build_by_def_id(deck.loaded[i].program_def_id);
                if (!probe.program) continue;
                const ProgramDef* def = find_program(probe.program->id);
                if (!def || def->kind != ProgramKind::Qh) continue;
                std::string line = std::string("  [") + char('a' + i) + "] " + def->filename;
                ctx.text({.x = 6, .y = row + found,
                          .content = line, .tag = UITag::TextDefault});
                ++found;
            }
            if (found == 0) {
                ctx.text({.x = 6, .y = row, .content = "  (no .qh loaded)",
                          .tag = UITag::TextDim});
            }
        } else {
            ctx.text({.x = 6, .y = row, .content = "  (no cyberdeck equipped)",
                      .tag = UITag::TextDim});
        }
        return;
    }

    // ── Unlocked: render the terminal subwindow ──
    auto* active_deck_slot = player_->equipment.equipped_cyberdeck();
    bool has_deck = active_deck_slot && *active_deck_slot && (*active_deck_slot)->deck;

    if (!has_deck) {
        // Reset greeter latch so re-equipping any deck shows its greeter.
        hack_term_greeted_deck_def_id_ = 0;
        // No deck equipped — suppress the terminal entirely and explain
        // what's needed. The skill is unlocked but useless without hardware.
        int cy = ctx.height() / 2 - 2;
        const char* title = "-- NO CYBERDECK EQUIPPED --";
        const int title_cols = 27;
        ctx.text({.x = ctx.width() / 2 - title_cols / 2, .y = cy,
                  .content = title, .tag = UITag::TextDim});
        ctx.text({.x = 4, .y = cy + 2,
                  .content = "Equip a cyberdeck in a Utility slot to use the deck terminal.",
                  .tag = UITag::TextDim});
        ctx.text({.x = 4, .y = cy + 3,
                  .content = "Decks drop from BlackMarket and MerchantArms (see PDA → Equipment).",
                  .tag = UITag::TextDim});
        return;
    }

    // Per-deck greeter — fires once per equipped deck (or after `clear`).
    {
        uint16_t cur_id = (*active_deck_slot)->item_def_id;
        if (hack_term_greeted_deck_def_id_ != cur_id) {
            hack_term_greet_for_deck(cur_id);
            hack_term_greeted_deck_def_id_ = cur_id;
        }
    }

    auto& d = *(*active_deck_slot)->deck;
    std::string header = "RAM " + std::to_string(d.ram_current) + "/" +
                         std::to_string(d.stats.ram_max) +
                         "  CPU " + std::to_string(d.stats.cpu) +
                         "  SLOTS " + std::to_string(d.stats.slots) +
                         "  STEALTH +" + std::to_string(d.stats.stealth) +
                         "  COOLING " + std::to_string(d.stats.cooling_rate) + "/turn";
    ctx.text({.x = 2, .y = 1, .content = header, .tag = UITag::TextDefault});
    std::string deck_name = "[ " + (*active_deck_slot)->name + " ]";
    ctx.text({.x = ctx.width() - static_cast<int>(deck_name.size()) - 2, .y = 1,
              .content = deck_name, .tag = UITag::TextDim});

    // Terminal flow: history lines start at top; the live prompt is the
    // last line. When the combined output overflows the visible area,
    // oldest lines scroll off (or are pushed up by hack_term_scroll_).
    int top = 3;
    int bottom = ctx.height() - 2;
    int visible = bottom - top + 1;
    int hist_count = static_cast<int>(hack_term_lines_.size());
    int total = hist_count + 1;                 // +1 for live prompt

    // Clamp scroll: we can scroll up at most (total - visible) lines.
    int max_scroll = std::max(0, total - visible);
    if (hack_term_scroll_ > max_scroll) hack_term_scroll_ = max_scroll;
    if (hack_term_scroll_ < 0) hack_term_scroll_ = 0;
    int skip = std::max(0, total - visible - hack_term_scroll_);

    int row = top;
    for (int i = skip; i < hist_count && row <= bottom; ++i, ++row) {
        ctx.text({.x = 2, .y = row,
                  .content = hack_term_lines_[i].text,
                  .tag = hack_term_lines_[i].tag});
    }
    // Render the prompt only if it's still in view (not scrolled past).
    if (hack_term_scroll_ == 0 && row <= bottom) {
        // Cursor sits at hack_term_input_cursor_; render an underscore at
        // that position so the user can see where insertions/backspaces land.
        int cur = hack_term_input_cursor_;
        if (cur < 0) cur = 0;
        if (cur > static_cast<int>(hack_term_input_.size())) cur = static_cast<int>(hack_term_input_.size());
        std::string prefix = prompt_for_deck((*active_deck_slot)->item_def_id);
        std::string prompt = prefix + hack_term_input_.substr(0, cur) + "_" +
                             hack_term_input_.substr(cur);
        ctx.text({.x = 2, .y = row, .content = prompt, .tag = UITag::TextDefault});
    }
}

void PdaScreen::hack_term_emit(const std::string& line, UITag tag) {
    hack_term_lines_.push_back({line, tag});
    if (hack_term_lines_.size() > 200) {
        hack_term_lines_.erase(hack_term_lines_.begin(),
                               hack_term_lines_.begin() + 50);
    }
    // Any new output snaps the view back to the bottom — matches xterm/bash.
    hack_term_scroll_ = 0;
}

void PdaScreen::handle_hacking_key(int key) {
    if (!has_cat_hacking(*player_)) return;
    // The terminal is hidden when no deck is equipped — swallow keystrokes
    // so they don't accumulate in an invisible input buffer.
    auto* deck_slot = player_->equipment.equipped_cyberdeck();
    if (!deck_slot || !*deck_slot || !(*deck_slot)->deck) return;

    auto clamp_cursor = [&]() {
        if (hack_term_input_cursor_ < 0) hack_term_input_cursor_ = 0;
        int n = static_cast<int>(hack_term_input_.size());
        if (hack_term_input_cursor_ > n) hack_term_input_cursor_ = n;
    };

    if (key == '\n' || key == '\r') {
        if (!hack_term_input_.empty()) {
            const char* prefix = prompt_for_deck((*deck_slot)->item_def_id);
            hack_term_emit(prefix + hack_term_input_, UITag::TextDefault);
            hack_term_history_.push_back(hack_term_input_);
            if (hack_term_history_.size() > 50)
                hack_term_history_.erase(hack_term_history_.begin());
            hack_term_run_command(hack_term_input_);
            hack_term_input_.clear();
            hack_term_input_cursor_ = 0;
            hack_term_history_cursor_ = -1;
        }
        return;
    }
    if (key == '\b' || key == 127) {
        // Backspace: delete the char to the LEFT of the cursor.
        clamp_cursor();
        if (hack_term_input_cursor_ > 0) {
            hack_term_input_.erase(hack_term_input_cursor_ - 1, 1);
            --hack_term_input_cursor_;
        }
        return;
    }
    if (key == KEY_DELETE) {
        // Delete: remove the char AT the cursor.
        clamp_cursor();
        if (hack_term_input_cursor_ < static_cast<int>(hack_term_input_.size())) {
            hack_term_input_.erase(hack_term_input_cursor_, 1);
        }
        return;
    }
    if (key == KEY_LEFT) {
        if (hack_term_input_cursor_ > 0) --hack_term_input_cursor_;
        return;
    }
    if (key == KEY_RIGHT) {
        if (hack_term_input_cursor_ < static_cast<int>(hack_term_input_.size()))
            ++hack_term_input_cursor_;
        return;
    }
    if (key == KEY_PAGE_UP) {
        // Scroll one page (visible area minus a line of overlap) up.
        // Magic 12 ≈ half the typical terminal pane; the draw clamp
        // re-computes the upper bound so over-scroll is harmless.
        hack_term_scroll_ += 12;
        return;
    }
    if (key == KEY_PAGE_DOWN) {
        hack_term_scroll_ -= 12;
        if (hack_term_scroll_ < 0) hack_term_scroll_ = 0;
        return;
    }
    if (key == KEY_UP) {
        if (hack_term_history_.empty()) return;
        if (hack_term_history_cursor_ == -1)
            hack_term_history_cursor_ = static_cast<int>(hack_term_history_.size()) - 1;
        else if (hack_term_history_cursor_ > 0)
            --hack_term_history_cursor_;
        hack_term_input_ = hack_term_history_[hack_term_history_cursor_];
        hack_term_input_cursor_ = static_cast<int>(hack_term_input_.size());
        return;
    }
    if (key == KEY_DOWN) {
        if (hack_term_history_cursor_ == -1) return;
        if (hack_term_history_cursor_ < static_cast<int>(hack_term_history_.size()) - 1) {
            ++hack_term_history_cursor_;
            hack_term_input_ = hack_term_history_[hack_term_history_cursor_];
        } else {
            hack_term_history_cursor_ = -1;
            hack_term_input_.clear();
        }
        hack_term_input_cursor_ = static_cast<int>(hack_term_input_.size());
        return;
    }
    if (key == '\t') {
        static const char* cmds[] = {
            "help", "deck info", "programs ls", "programs load",
            "programs unload", "netmap", "jack -t", "lore",
            "clear", "history"
        };
        for (const char* c : cmds) {
            if (std::string(c).rfind(hack_term_input_, 0) == 0) {
                hack_term_input_ = c;
                hack_term_input_ += ' ';
                hack_term_input_cursor_ = static_cast<int>(hack_term_input_.size());
                return;
            }
        }
        return;
    }
    // Single-key shortcuts (menu fallbacks) — only when input buffer is empty.
    if (hack_term_input_.empty()) {
        switch (key) {
            case '?': hack_term_run_command("help"); return;
            case 'P': hack_term_run_command("programs ls"); return;
            case 'N': hack_term_run_command("netmap"); return;
            case 'L': hack_term_run_command("lore"); return;
        }
    }
    if (key >= ' ' && key < 127) {
        // Insert at cursor — supports edit-in-place.
        clamp_cursor();
        if (hack_term_input_.size() < 64) {
            hack_term_input_.insert(hack_term_input_cursor_, 1, static_cast<char>(key));
            ++hack_term_input_cursor_;
        }
    }
}

void PdaScreen::hack_term_run_command(const std::string& line) {
    auto args = tokenize_(line);
    if (args.empty()) return;
    const std::string& v = args[0];

    if (v == "help") return hack_term_cmd_help();
    if (v == "deck") {
        if (args.size() >= 2 && args[1] == "info") return hack_term_cmd_deck_info();
        hack_term_emit("usage: deck info", UITag::TextDim);
        return;
    }
    if (v == "programs") {
        if (args.size() >= 2 && args[1] == "ls") return hack_term_cmd_programs_ls();
        if (args.size() >= 2 && args[1] == "load")
            return hack_term_cmd_programs_load(args);
        if (args.size() >= 2 && args[1] == "unload")
            return hack_term_cmd_programs_unload(args);
        hack_term_emit("usage: programs <ls|load|unload>", UITag::TextDim);
        return;
    }
    if (v == "netmap")  return hack_term_cmd_netmap();
    if (v == "jack")    return hack_term_cmd_jack(args);
    if (v == "lore")    return hack_term_cmd_lore();
    if (v == "clear")   return hack_term_cmd_clear();
    if (v == "history") return hack_term_cmd_history();
    hack_term_emit("?: unknown command. Try 'help'.", UITag::TextDim);
}

void PdaScreen::hack_term_cmd_help() {
    static const char* lines[] = {
        "Commands:",
        "  help                       — this list",
        "  deck info                  — deck stats",
        "  programs ls                — list loaded programs",
        "  programs load <slot> <id>  — load program from inventory",
        "  programs unload <slot>     — unload a slot",
        "  netmap                     — known networks (stub in Plan 2)",
        "  jack -t <node>             — jack in (Grid coming in Plan 3)",
        "  lore                       — decrypted archives",
        "  clear / history",
    };
    for (auto* s : lines) hack_term_emit(s, UITag::TextDim);
}

void PdaScreen::hack_term_cmd_deck_info() {
    auto* di_deck_slot = player_->equipment.equipped_cyberdeck();
    if (!di_deck_slot || !*di_deck_slot || !(*di_deck_slot)->deck) {
        hack_term_emit("no deck equipped.", UITag::TextDim);
        return;
    }
    auto& d = *(*di_deck_slot)->deck;
    hack_term_emit("Deck: " + (*di_deck_slot)->name);
    hack_term_emit("  RAM " + std::to_string(d.ram_current) + "/" + std::to_string(d.stats.ram_max));
    hack_term_emit("  CPU " + std::to_string(d.stats.cpu));
    hack_term_emit("  SLOTS " + std::to_string(d.stats.slots));
    hack_term_emit("  STEALTH +" + std::to_string(d.stats.stealth));
    hack_term_emit("  COOLING " + std::to_string(d.stats.cooling_rate) + "/turn");
    hack_term_emit("  HEAT_CAP " + std::to_string(d.stats.heat_cap));
}

void PdaScreen::hack_term_cmd_programs_ls() {
    auto* ls_deck_slot = player_->equipment.equipped_cyberdeck();
    if (!ls_deck_slot || !*ls_deck_slot || !(*ls_deck_slot)->deck) {
        hack_term_emit("no deck equipped.", UITag::TextDim);
        return;
    }
    auto& d = *(*ls_deck_slot)->deck;
    for (int i = 0; i < d.stats.slots; ++i) {
        if (d.loaded[i].program_def_id == 0) {
            hack_term_emit("  [" + std::to_string(i) + "] (empty)");
            continue;
        }
        Item probe = build_by_def_id(d.loaded[i].program_def_id);
        if (!probe.program) {
            hack_term_emit("  [" + std::to_string(i) + "] ???");
            continue;
        }
        const ProgramDef* def = find_program(probe.program->id);
        if (!def) {
            hack_term_emit("  [" + std::to_string(i) + "] ???");
            continue;
        }
        std::string row = "  [" + std::to_string(i) + "] " + def->filename + "  " +
                          program_kind_short(def->kind) + "  " +
                          std::to_string(def->ram_cost) + " RAM, " +
                          std::to_string(def->heat_cost) + " Heat";
        hack_term_emit(row);
    }
}

void PdaScreen::hack_term_cmd_programs_load(const std::vector<std::string>& args) {
    if (args.size() < 4) {
        hack_term_emit("usage: programs load <slot> <id>", UITag::TextDim);
        return;
    }
    auto* ld_deck_slot = player_->equipment.equipped_cyberdeck();
    if (!ld_deck_slot || !*ld_deck_slot || !(*ld_deck_slot)->deck) {
        hack_term_emit("no deck equipped.", UITag::TextDim);
        return;
    }
    auto& d = *(*ld_deck_slot)->deck;
    int slot = -1;
    try { slot = std::stoi(args[2]); } catch (...) {}
    if (slot < 0 || slot >= d.stats.slots) {
        hack_term_emit("bad slot.", UITag::TextDim);
        return;
    }
    int inv_idx = -1;
    for (size_t i = 0; i < player_->inventory.items.size(); ++i) {
        const auto& it = player_->inventory.items[i];
        if (it.type != ItemType::Program || !it.program) continue;
        const ProgramDef* def = find_program(it.program->id);
        if (def && std::string(def->filename) == args[3]) {
            inv_idx = static_cast<int>(i);
            break;
        }
    }
    if (inv_idx < 0) {
        hack_term_emit("no such program in inventory.", UITag::TextDim);
        return;
    }
    // No-op when the slot already holds the requested program. Without
    // this guard, the unload-then-load path duplicates the program.
    uint16_t inv_def_id = player_->inventory.items[inv_idx].item_def_id;
    if (d.loaded[slot].program_def_id == inv_def_id) {
        hack_term_emit("slot " + std::to_string(slot) +
                       " already holds " + args[3] + ".", UITag::TextDim);
        return;
    }
    // Unload current occupant back to inventory.
    if (d.loaded[slot].program_def_id != 0) {
        Item old = build_by_def_id(d.loaded[slot].program_def_id);
        player_->inventory.items.push_back(std::move(old));
        d.loaded[slot].program_def_id = 0;
    }
    d.loaded[slot].program_def_id = inv_def_id;
    player_->inventory.items.erase(player_->inventory.items.begin() + inv_idx);
    hack_term_emit("loaded " + args[3] + " into slot " + std::to_string(slot) + ".",
                   UITag::TextDefault);
}

void PdaScreen::hack_term_cmd_programs_unload(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        hack_term_emit("usage: programs unload <slot>", UITag::TextDim);
        return;
    }
    auto* ul_deck_slot = player_->equipment.equipped_cyberdeck();
    if (!ul_deck_slot || !*ul_deck_slot || !(*ul_deck_slot)->deck) {
        hack_term_emit("no deck equipped.", UITag::TextDim);
        return;
    }
    auto& d = *(*ul_deck_slot)->deck;
    int slot = -1;
    try { slot = std::stoi(args[2]); } catch (...) {}
    if (slot < 0 || slot >= d.stats.slots) {
        hack_term_emit("bad slot.", UITag::TextDim);
        return;
    }
    if (d.loaded[slot].program_def_id == 0) {
        hack_term_emit("slot already empty.", UITag::TextDim);
        return;
    }
    Item old = build_by_def_id(d.loaded[slot].program_def_id);
    player_->inventory.items.push_back(std::move(old));
    d.loaded[slot].program_def_id = 0;
    hack_term_emit("unloaded slot " + std::to_string(slot) + ".", UITag::TextDefault);
}

void PdaScreen::hack_term_cmd_netmap() {
    hack_term_emit("netmap: no networks discovered yet (Plan 3 will populate).",
                   UITag::TextDim);
}
void PdaScreen::hack_term_cmd_jack(const std::vector<std::string>&) {
    hack_term_emit("The Grid is not yet implemented (Plan 3).", UITag::TextDim);
}
void PdaScreen::hack_term_cmd_lore() {
    hack_term_emit("no decrypted archives (Plan 3+).", UITag::TextDim);
}
void PdaScreen::hack_term_cmd_clear() {
    hack_term_lines_.clear();
    auto* slot = player_->equipment.equipped_cyberdeck();
    if (slot && *slot && (*slot)->deck) {
        hack_term_greet_for_deck((*slot)->item_def_id);
        hack_term_greeted_deck_def_id_ = (*slot)->item_def_id;
    }
}

void PdaScreen::hack_term_greet_for_deck(uint16_t deck_def_id) {
    switch (deck_def_id) {
        case ITEM_PIDGIN_MK1:
            hack_term_emit("   ____  _     _       _       ", UITag::TextDim);
            hack_term_emit("  |  _ \\(_) __| | __ _(_)_ __  ", UITag::TextDim);
            hack_term_emit("  | |_) | |/ _` |/ _` | | '_ \\ ", UITag::TextDim);
            hack_term_emit("  |  __/| | (_| | (_| | | | | |", UITag::TextDim);
            hack_term_emit("  |_|   |_|\\__,_|\\__, |_|_| |_|", UITag::TextDim);
            hack_term_emit("                 |___/         ", UITag::TextDim);
            hack_term_emit("  ~ pawn-shop deck, lightly cursed ~", UITag::TextDim);
            hack_term_emit("", UITag::TextDim);
            hack_term_emit("Press 'help' for the basics.", UITag::TextDim);
            break;
        case ITEM_POLYGLOT_DCK2:
            hack_term_emit("     ____   ____ _  __        ___", UITag::TextDim);
            hack_term_emit("    |  _ \\ / ___| |/ /       |__ \\", UITag::TextDim);
            hack_term_emit("    | | | | |   | ' /   ___    / /", UITag::TextDim);
            hack_term_emit("    | |_| | |___| . \\  |___|  / /_", UITag::TextDim);
            hack_term_emit("    |____/ \\____|_|\\_\\       |____|", UITag::TextDim);
            hack_term_emit("    POLYGLOT DCK-2", UITag::TextDim);
            hack_term_emit("", UITag::TextDim);
            hack_term_emit("    CPU 2 / RAM 8 / SLOTS 4", UITag::TextDim);
            hack_term_emit("    thermal envelope ........ nominal", UITag::TextDim);
            hack_term_emit("    operator profile ........ AUTHENTICATED", UITag::TextDim);
            hack_term_emit("", UITag::TextDim);
            hack_term_emit("  Press 'help' for command list.", UITag::TextDim);
            break;
        default:
            hack_term_emit("Cyberdeck online.", UITag::TextDim);
            hack_term_emit("Press 'help' for command list.", UITag::TextDim);
            break;
    }
}
void PdaScreen::hack_term_cmd_history() {
    for (size_t i = 0; i < hack_term_history_.size(); ++i) {
        hack_term_emit("  " + std::to_string(i) + "  " + hack_term_history_[i]);
    }
}

} // namespace astra
