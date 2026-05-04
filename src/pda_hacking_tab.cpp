#include "astra/pda_screen.h"

#include "astra/consciousness_save.h"
#include "astra/cyberdeck.h"
#include "astra/cyberdeck_mods.h"
#include "astra/device_shell.h"
#include "astra/game.h"
#include "astra/grid_network.h"
#include "astra/hackable.h"
#include "astra/hacking_system.h"
#include "astra/ip.h"
#include "astra/item_defs.h"
#include "astra/item_ids.h"
#include "astra/lan.h"
#include "astra/program.h"
#include "astra/skill_defs.h"
#include "astra/world_manager.h"

#include <cstdio>
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

// Rotating MOTD pool. Tipped slightly cyberpunk on purpose.
const char* next_motd() {
    static const char* lines[] = {
        "  motd: trace the operator, not the keystrokes.",
        "  motd: the network forgets nothing.",
        "  motd: heat is a tax on impatience.",
        "  motd: every console is a confession waiting to happen.",
        "  motd: you don't break ICE, you let ICE break itself.",
        "  motd: an unloaded deck is the safest deck.",
        "  motd: ports closed, hearts open.",
        "  motd: keep your detection low and your aliases high.",
    };
    static int idx = 0;
    const char* s = lines[idx % (sizeof(lines) / sizeof(lines[0]))];
    ++idx;
    return s;
}

} // namespace

void PdaScreen::draw_hacking(UIContext& ctx) {
    // Plan 7 §3a: while a real-world DeviceShell is open, the Hacking tab's
    // content area becomes the device shell. The pda> outer shell stays in
    // memory (history, MOTD greeter latch); on shell exit we revert. The
    // in-Grid path renders into the Tron window via grid_renderer; it is
    // never seen here.
    if (hacking_system_ && hacking_system_->device_shell_open() &&
        hacking_system_->device_shell().via() == ShellVia::RealWorld &&
        game_ && renderer_) {
        hacking_system_->device_shell().render_into(
            renderer_, ctx.bounds(), *game_);
        return;
    }

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

    // Nmap overlay sits on top of the terminal pane.
    if (world_) {
        nmap_widget_.render(ctx, world_->grid_network(),
                            world_->lan_metadata().lan_root);
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
    // Plan 7 §3a: when a real-world DeviceShell is active, the Hacking tab's
    // input goes to the device shell line editor. On `exit` (Esc or cmd_exit)
    // the shell closes and control returns to the pda> shell with its prior
    // state intact.
    if (hacking_system_ && hacking_system_->device_shell_open() &&
        hacking_system_->device_shell().via() == ShellVia::RealWorld &&
        game_) {
        hacking_system_->device_shell().handle_input(key, *game_);
        return;
    }

    if (!has_cat_hacking(*player_)) return;
    // The terminal is hidden when no deck is equipped — swallow keystrokes
    // so they don't accumulate in an invisible input buffer.
    auto* deck_slot = player_->equipment.equipped_cyberdeck();
    if (!deck_slot || !*deck_slot || !(*deck_slot)->deck) return;

    // Nmap overlay swallows input while open. A confirmed jack-in is
    // funnelled into the existing terminal jack request slot so the game
    // input loop picks it up uniformly.
    if (nmap_widget_.is_open() && world_) {
        nmap_widget_.handle_key(world_->grid_network(), key,
                                world_->lan_metadata().lan_root);
        if (uint32_t nid = nmap_widget_.take_jack_in_request(); nid != 0) {
            jack_in_request_node_id_ = nid;
        }
        if (auto br = nmap_widget_.take_breach_request(); br.valid()) {
            pending_breach_request_ = br;
        }
        return;
    }

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
            "help", "man ", "deck info", "ps", "ls",
            "load ", "unload ", "cat ", "echo ", "uname",
            "whoami", "ping ", "nmap ", "nmap -l", "nmap -m", "jack ",
            "lore", "clear", "history"
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
            case 'P': hack_term_run_command("ps"); return;
            case 'I': hack_term_run_command("ls"); return;
            case 'N': hack_term_run_command("nmap -m"); return;
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

    if (v == "help")    return hack_term_cmd_help();
    if (v == "deck") {
        if (args.size() >= 2 && args[1] == "info") return hack_term_cmd_deck_info();
        hack_term_emit("usage: deck info", UITag::TextDim);
        return;
    }
    if (v == "ps")      return hack_term_cmd_ps(args);
    if (v == "ls")      return hack_term_cmd_ls(args);
    if (v == "load")    return hack_term_cmd_load(args);
    if (v == "unload")  return hack_term_cmd_unload(args);
    if (v == "man")     return hack_term_cmd_man(args);
    if (v == "cat")     return hack_term_cmd_cat(args);
    if (v == "echo")    return hack_term_cmd_echo(args);
    if (v == "uname")   return hack_term_cmd_uname(args);
    if (v == "whoami")  return hack_term_cmd_whoami();
    if (v == "ping")    return hack_term_cmd_ping(args);
    if (v == "nmap")    return hack_term_cmd_nmap(args);
    if (v == "jack")    return hack_term_cmd_jack(args);
    if (v == "ssh")     return hack_term_cmd_ssh(args);
    if (v == "lore")    return hack_term_cmd_lore();
    if (v == "clear")   return hack_term_cmd_clear();
    if (v == "history") return hack_term_cmd_history();
    hack_term_emit(v + ": command not found. Try 'help'.", UITag::TextDim);
}

void PdaScreen::hack_term_cmd_help() {
    static const char* lines[] = {
        "Commands:",
        "  help                — this list",
        "  man <cmd>           — full manual page for a command",
        "  deck info           — deck stats",
        "  ps [-a]             — list loaded programs (running in RAM)",
        "  ls [-l]             — list programs in inventory",
        "  load <slot> <id>    — load a program into a slot",
        "  unload <slot>       — unload a slot",
        "  cat <id>            — show a program's full description",
        "  echo <text>         — print text",
        "  uname [-a]          — system identity",
        "  whoami              — operator identity",
        "  ping <ip>           — probe a node (free recon)",
        "  nmap [-l|-m]        — list or map LAN nodes",
        "  jack <ip>           — jack into a node",
        "  ssh [<user>@]<ip>   — open a device shell (default user: root)",
        "  lore                — decrypted archives",
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

void PdaScreen::hack_term_cmd_ps(const std::vector<std::string>& args) {
    auto* ls_deck_slot = player_->equipment.equipped_cyberdeck();
    if (!ls_deck_slot || !*ls_deck_slot || !(*ls_deck_slot)->deck) {
        hack_term_emit("no deck equipped.", UITag::TextDim);
        return;
    }
    auto& d = *(*ls_deck_slot)->deck;

    // -a / aux: extended status header.
    bool extended = false;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "aux" ||
            (args[i].size() >= 2 && args[i][0] == '-' &&
             args[i].find('a') != std::string::npos)) {
            extended = true; break;
        }
    }
    if (extended) {
        hack_term_emit("USER  PID  STAT  RAM   HEAT  PROGRAM", UITag::TextDim);
    }
    int active = 0;
    for (int i = 0; i < d.stats.slots; ++i) {
        if (d.loaded[i].program_def_id == 0) {
            if (extended) {
                hack_term_emit("op    --   ----  --    --    [" + std::to_string(i) + "] (empty)",
                               UITag::TextDim);
            } else {
                hack_term_emit("  [" + std::to_string(i) + "] (empty)");
            }
            continue;
        }
        Item probe = build_by_def_id(d.loaded[i].program_def_id);
        const ProgramDef* def = probe.program ? find_program(probe.program->id) : nullptr;
        if (!def) {
            hack_term_emit("  [" + std::to_string(i) + "] ???");
            continue;
        }
        ++active;
        if (extended) {
            std::string pid = std::string(2 - std::min<size_t>(2, std::to_string(i).size()), '0') +
                              std::to_string(i);
            std::string row = "op    " + pid + "   S     " +
                              std::to_string(def->ram_cost) + "     " +
                              std::to_string(def->heat_cost) + "     " +
                              def->filename + "  " + program_kind_short(def->kind);
            hack_term_emit(row);
        } else {
            std::string row = "  [" + std::to_string(i) + "] " + def->filename + "  " +
                              program_kind_short(def->kind) + "  " +
                              std::to_string(def->ram_cost) + " RAM, " +
                              std::to_string(def->heat_cost) + " Heat";
            hack_term_emit(row);
        }
    }
    if (extended) {
        hack_term_emit("--", UITag::TextDim);
        hack_term_emit(std::to_string(active) + " active / " +
                       std::to_string(d.stats.slots) + " slots // RAM " +
                       std::to_string(d.ram_current) + "/" +
                       std::to_string(d.stats.ram_max), UITag::TextDim);
    }
}

void PdaScreen::hack_term_cmd_ls(const std::vector<std::string>& args) {
    // Collect all program defs from inventory (preserve insertion order).
    std::vector<const ProgramDef*> defs;
    for (const auto& it : player_->inventory.items) {
        if (it.type != ItemType::Program || !it.program) continue;
        const ProgramDef* def = find_program(it.program->id);
        if (def) defs.push_back(def);
    }
    if (defs.empty()) {
        hack_term_emit("  (no programs in inventory)", UITag::TextDim);
        return;
    }

    // Long format if any arg looks like a flag containing 'l' (covers
    // -l, -la, -al, -ahl, etc. — same loose match real ls accepts).
    bool long_fmt = false;
    for (size_t i = 1; i < args.size(); ++i) {
        if (!args[i].empty() && args[i][0] == '-' &&
            args[i].find('l') != std::string::npos) {
            long_fmt = true;
            break;
        }
    }

    if (long_fmt) {
        // -l: one row per file with kind, RAM, and heat/detection cost.
        for (const auto* def : defs) {
            std::string row = "  " + std::string(def->filename) + "  " +
                              program_kind_short(def->kind) + "  " +
                              std::to_string(def->ram_cost) + " RAM";
            if (def->kind == ProgramKind::Qh) {
                row += ", +" + std::to_string(def->detection_cost) + " Det";
            } else {
                row += ", " + std::to_string(def->heat_cost) + " Heat";
            }
            hack_term_emit(row);
        }
        return;
    }

    // Default: column-grid layout (mirrors `ls` in a real shell). Pack as
    // many filenames per row as fit in the panel width. Column width is
    // sized to the longest filename + 2-space gutter.
    size_t col_w = 0;
    for (const auto* def : defs) {
        col_w = std::max(col_w, std::string(def->filename).size());
    }
    col_w += 2;  // gutter

    constexpr size_t kAssumedTermWidth = 70;
    size_t cols = std::max<size_t>(1, kAssumedTermWidth / col_w);

    std::string row;
    size_t in_row = 0;
    for (size_t i = 0; i < defs.size(); ++i) {
        std::string cell = defs[i]->filename;
        if (cell.size() < col_w) cell.append(col_w - cell.size(), ' ');
        row += cell;
        ++in_row;
        if (in_row >= cols || i + 1 == defs.size()) {
            // Strip trailing spaces from the last row before emit.
            while (!row.empty() && row.back() == ' ') row.pop_back();
            hack_term_emit(row);
            row.clear();
            in_row = 0;
        }
    }
}

void PdaScreen::hack_term_cmd_load(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        hack_term_emit("usage: load <slot> <id>", UITag::TextDim);
        return;
    }
    auto* ld_deck_slot = player_->equipment.equipped_cyberdeck();
    if (!ld_deck_slot || !*ld_deck_slot || !(*ld_deck_slot)->deck) {
        hack_term_emit("no deck equipped.", UITag::TextDim);
        return;
    }
    auto& d = *(*ld_deck_slot)->deck;
    int slot = -1;
    try { slot = std::stoi(args[1]); } catch (...) {}
    if (slot < 0 || slot >= d.stats.slots) {
        hack_term_emit("bad slot.", UITag::TextDim);
        return;
    }
    int inv_idx = -1;
    for (size_t i = 0; i < player_->inventory.items.size(); ++i) {
        const auto& it = player_->inventory.items[i];
        if (it.type != ItemType::Program || !it.program) continue;
        const ProgramDef* def = find_program(it.program->id);
        if (def && std::string(def->filename) == args[2]) {
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
                       " already holds " + args[2] + ".", UITag::TextDim);
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
    hack_term_emit("loaded " + args[2] + " into slot " + std::to_string(slot) + ".",
                   UITag::TextDefault);
}

void PdaScreen::hack_term_cmd_unload(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        hack_term_emit("usage: unload <slot>", UITag::TextDim);
        return;
    }
    auto* ul_deck_slot = player_->equipment.equipped_cyberdeck();
    if (!ul_deck_slot || !*ul_deck_slot || !(*ul_deck_slot)->deck) {
        hack_term_emit("no deck equipped.", UITag::TextDim);
        return;
    }
    auto& d = *(*ul_deck_slot)->deck;
    int slot = -1;
    try { slot = std::stoi(args[1]); } catch (...) {}
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

void PdaScreen::hack_term_cmd_man(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        hack_term_emit("usage: man <command>", UITag::TextDim);
        return;
    }
    const std::string& topic = args[1];
    struct Page { const char* cmd; const char* lines[6]; };
    static const Page pages[] = {
        {"help",   {"NAME", "  help — list available commands.", "", "SEE ALSO", "  man <command>", nullptr}},
        {"man",    {"NAME", "  man — show the manual page for a command.", "", "USAGE", "  man <command>", nullptr}},
        {"deck",   {"NAME", "  deck info — print the equipped deck's stats.", "", "ENVIRONMENT", "  Requires an equipped cyberdeck.", nullptr}},
        {"ps",     {"NAME", "  ps — list programs loaded into deck slots.", "", "FLAGS", "  -a, aux : extended ps-style listing", nullptr}},
        {"ls",     {"NAME", "  ls — list programs in inventory.", "", "FLAGS", "  -l : long format (one row per program)", nullptr}},
        {"load",   {"NAME", "  load — load a program from inventory into a deck slot.", "", "USAGE", "  load <slot> <filename>", nullptr}},
        {"unload", {"NAME", "  unload — remove a program from a deck slot.", "", "USAGE", "  unload <slot>", nullptr}},
        {"cat",    {"NAME", "  cat — print a program's full description.", "", "USAGE", "  cat <filename>", nullptr}},
        {"echo",   {"NAME", "  echo — print arguments to the terminal.", "", "USAGE", "  echo <text...>", nullptr}},
        {"uname",  {"NAME", "  uname — print system identification.", "", "FLAGS", "  -a : full identity (deck + version + operator)", nullptr}},
        {"whoami", {"NAME", "  whoami — print the current operator handle.", nullptr, nullptr, nullptr, nullptr}},
        {"ping",   {"NAME", "  ping — probe a node (free recon).", "", "USAGE", "  ping <ip>", nullptr}},
        {"nmap",   {"NAME", "  nmap — list or map nodes on the current LAN.", "", "FLAGS", "  -l/--list : text list   -m/--map : visual widget", nullptr}},
        {"jack",   {"NAME", "  jack — jack into a node.", "", "USAGE", "  jack <ip>", nullptr}},
        {"lore",   {"NAME", "  lore — list decrypted lore archives.", "", "OUTPUT", "  archive ids + origin tick. Use 'cat <archive-id>' to read.", nullptr}},
        {"clear",  {"NAME", "  clear — wipe the scrollback and re-greet.", nullptr, nullptr, nullptr, nullptr}},
        {"history",{"NAME", "  history — replay this session's command history.", nullptr, nullptr, nullptr, nullptr}},
    };
    for (const auto& p : pages) {
        if (topic == p.cmd) {
            for (auto* s : p.lines) {
                if (!s) break;
                hack_term_emit(s);
            }
            return;
        }
    }
    hack_term_emit("No manual entry for '" + topic + "'.", UITag::TextDim);
}

void PdaScreen::hack_term_cmd_cat(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        hack_term_emit("usage: cat <filename-or-archive-id>", UITag::TextDim);
        return;
    }
    const std::string& fname = args[1];

    // Plan 5 Cut 4: try lore_archive first.
    {
        ConsciousnessSave cs;
        read_consciousness(cs);
        for (const auto& a : cs.lore_archive) {
            if (a.archive_id == fname) {
                hack_term_emit(">> archive: " + a.archive_id);
                hack_term_emit(">> origin: galaxy " + std::to_string(a.galaxy_seed_origin) +
                               ", tick " + std::to_string(a.world_tick_origin));
                hack_term_emit("");
                hack_term_emit("(lore body text — Plan 7)", UITag::TextDim);
                hack_term_emit("");
                hack_term_emit(">> end of archive.");
                return;
            }
        }
    }

    // Existing logic (program inventory lookup).
    for (const auto& it : player_->inventory.items) {
        if (it.type != ItemType::Program || !it.program) continue;
        const ProgramDef* def = find_program(it.program->id);
        if (!def || std::string(def->filename) != fname) continue;
        hack_term_emit(std::string(def->filename) + "  // " + def->name);
        hack_term_emit("  kind:      " + std::string(program_kind_name(def->kind)));
        hack_term_emit("  tier:      " + std::to_string(def->tier));
        hack_term_emit("  ram_cost:  " + std::to_string(def->ram_cost));
        if (def->kind == ProgramKind::Qh) {
            hack_term_emit("  detection: +" + std::to_string(def->detection_cost));
            if (!def->target_filter.empty()) {
                std::string targets;
                for (size_t i = 0; i < def->target_filter.size(); ++i) {
                    if (i > 0) targets += ", ";
                    targets += tag_set_describe(def->target_filter[i]);
                }
                hack_term_emit("  targets:   " + targets);
            }
        } else {
            hack_term_emit("  heat_cost: " + std::to_string(def->heat_cost));
        }
        hack_term_emit("");
        hack_term_emit(std::string(def->description), UITag::TextDim);
        return;
    }
    hack_term_emit("cat: " + fname + ": no such file or archive.", UITag::TextDim);
}

void PdaScreen::hack_term_cmd_echo(const std::vector<std::string>& args) {
    std::string out;
    for (size_t i = 1; i < args.size(); ++i) {
        if (i > 1) out += ' ';
        out += args[i];
    }
    hack_term_emit(out);
}

void PdaScreen::hack_term_cmd_uname(const std::vector<std::string>& args) {
    bool full = false;
    for (size_t i = 1; i < args.size(); ++i) {
        if (!args[i].empty() && args[i][0] == '-' &&
            args[i].find('a') != std::string::npos) {
            full = true; break;
        }
    }
    auto* slot = player_->equipment.equipped_cyberdeck();
    std::string deck = (slot && *slot) ? (*slot)->name : "no-deck";
    if (full) {
        hack_term_emit("astra-os 1.0 // " + deck + " // operator: " + player_->name);
    } else {
        hack_term_emit("astra-os");
    }
}

void PdaScreen::hack_term_cmd_whoami() {
    hack_term_emit(player_->name);
}

void PdaScreen::hack_term_cmd_ping(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        hack_term_emit("usage: ping <ip>", UITag::TextDim);
        return;
    }
    auto parsed = parse_ip(args[1]);
    if (!parsed) {
        hack_term_emit("ping: invalid IP '" + args[1] + "'", UITag::TextDim);
        return;
    }
    if (!world_) {
        hack_term_emit("ping: world unavailable.", UITag::TextDim);
        return;
    }
    auto* h = world_->find_hackable_by_ip(*parsed);
    if (!h) {
        hack_term_emit("ping: " + format_ip(*parsed) + ": host unreachable", UITag::TextDim);
        return;
    }

    char line1[160], line2[160], line3[160];
    std::snprintf(line1, sizeof line1, "PING %s (%s):",
                  format_ip(*parsed).c_str(), tag_summary(h->tags));
    int latency = 1 + (static_cast<int>(*parsed) & 7);   // deterministic, cosmetic
    std::snprintf(line2, sizeof line2, "  64 bytes from %s: time=%dms",
                  format_ip(*parsed).c_str(), latency);
    std::snprintf(line3, sizeof line3, "  tier:    %d (%s)",
                  h->security_tier,
                  h->state == HackState::Compromised ? "compromised"
                  : h->state == HackState::Alarmed   ? "alarmed"
                                                    : "clean");
    hack_term_emit(line1);
    hack_term_emit(line2);
    hack_term_emit(line3);

    // Tags line — use tag_set_describe for a readable summary.
    std::string tags_line = "  tags:    ";
    tags_line += tag_set_describe(h->tags);
    hack_term_emit(tags_line);
}

void PdaScreen::hack_term_cmd_nmap(const std::vector<std::string>& args) {
    // No args -> list (the natural default; matches `-l`).
    if (args.size() < 2) return hack_term_cmd_nmap_list();
    if (args[1] == "-h" || args[1] == "--help") {
        hack_term_emit("usage: nmap [-l|--list] [-m|--map] [-h|--help]", UITag::TextDim);
        hack_term_emit("  (no args) list nodes on the current LAN (default)", UITag::TextDim);
        hack_term_emit("  -l   list nodes on the current LAN", UITag::TextDim);
        hack_term_emit("  -m   open the visual map widget", UITag::TextDim);
        return;
    }
    if (args[1] == "-l" || args[1] == "--list") return hack_term_cmd_nmap_list();
    if (args[1] == "-m" || args[1] == "--map")  return hack_term_cmd_nmap_map();
    hack_term_emit("nmap: unknown flag '" + args[1] + "'; try -l, -m, or -h.", UITag::TextDim);
}

void PdaScreen::hack_term_cmd_nmap_list() {
    if (!world_) {
        hack_term_emit("nmap: world unavailable.", UITag::TextDim);
        return;
    }
    const auto& meta = world_->lan_metadata();
    if (meta.nodes_total <= 0 || !meta.lan_root.valid()) {
        hack_term_emit("nmap: no LAN on this map.", UITag::TextDim);
        return;
    }

    char header[160];
    std::snprintf(header, sizeof header,
                  "LAN: %s   (%s/24)   %d nodes, %d cracked",
                  meta.display_name.c_str(),
                  format_ip(meta.subnet_base).c_str(),
                  meta.nodes_total, meta.nodes_cracked);
    hack_term_emit(header);
    hack_term_emit("");
    // Plan 7 §17 A4: surface per-device tier and lock state in a single
    // canonical "tier:N (locked|cracked|unlocked)" column. Manual ssh is
    // strict; players plan attempts off this listing.
    hack_term_emit("  IP            HOST                            EDGE      DEVICE                 OS");

    const auto& net = world_->grid_network();
    for (const auto& e : net.edges()) {
        if (e.from != meta.lan_root) continue;
        const GridNode* n = net.find(e.to);
        if (!n || n->kind != GridNodeKind::Subnet) continue;

        // Edge status: "open" if tier-0; "cracked" if breached;
        // "locked.<tier>" otherwise (tells the player the gateway depth).
        std::string edge_status;
        if (e.gateway_tier == 0)        edge_status = "open";
        else if (e.cracked)             edge_status = "cracked";
        else                            edge_status = "locked." + std::to_string(e.gateway_tier);

        // Subnet labels are stamped to the device IP at registration time.
        // Resolve the Hackable behind the IP so we can render a friendly
        // hostname for the HOST column ("turret-13.tha.lan" etc.) AND the
        // device-tier / device-lock-state column.
        std::string host = n->label;  // fallback: raw IP if lookup misses
        const Hackable* h = nullptr;
        if (auto parsed = parse_ip(n->label)) {
            h = world_->find_hackable_by_ip(*parsed);
            if (h) host = lan_hostname(*h, meta);
        }

        // Plan 7 §17 A4 + §16: per-device tier + lock state and OS slot.
        // Cracked = device.escalated == true (root is mine).
        // Locked  = has Locked tag and not escalated.
        // Unlocked = no Locked tag.
        // AlienTech rows display OS: ??? (unknown).
        std::string device_state;
        std::string os_slot;
        if (h) {
            int dtier = h->security_tier;
            const char* lock_label;
            if (h->escalated)                            lock_label = "cracked";
            else if (has_tag(h->tags, HackTag::Locked))  lock_label = "locked";
            else                                         lock_label = "unlocked";
            char ds[64];
            std::snprintf(ds, sizeof ds, "tier:%d (%s)", dtier, lock_label);
            device_state = ds;
            if (has_tag(h->tags, HackTag::AlienTech)) {
                os_slot = "??? (unknown)";
            } else {
                os_slot = tag_summary(h->tags);
            }
        } else {
            char ds[64];
            std::snprintf(ds, sizeof ds, "tier:%d (?)", n->security_tier);
            device_state = ds;
            os_slot = "?";
        }

        char line[256];
        std::snprintf(line, sizeof line, "  %-13s %-31s %-9s %-22s %s",
                      n->label.c_str(), host.c_str(),
                      edge_status.c_str(),
                      device_state.c_str(),
                      os_slot.c_str());
        hack_term_emit(line);
    }

    if (meta.has_deep_grid_edge) {
        hack_term_emit("  10.x.y.254     [\xe2\x8a\x95 deep-grid]              locked.2  tier:2 (locked)        DeepGridGateway");
    }
}

void PdaScreen::hack_term_cmd_nmap_map() {
    if (!world_) {
        hack_term_emit("nmap: world unavailable.", UITag::TextDim);
        return;
    }
    nmap_widget_.open();
}

void PdaScreen::hack_term_cmd_jack(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        hack_term_emit("usage: jack <ip>", UITag::TextDim);
        return;
    }
    auto parsed = parse_ip(args[1]);
    if (!parsed) {
        hack_term_emit("jack: invalid IP '" + args[1] + "'", UITag::TextDim);
        return;
    }
    if (!world_ || !player_) {
        hack_term_emit("jack: state unavailable.", UITag::TextDim);
        return;
    }
    if (!has_cat_hacking(*player_)) {
        hack_term_emit("jack: requires Cat_Hacking skill.", UITag::TextDim);
        return;
    }
    // Plan 7 §15 / §17 A1: `jack <ip>` is mod-gated. Without a Wireless
    // Jack-In Module installed (v1: present in inventory), the command
    // refuses to dial. The §16 lock-check path is gone — the mod gate is
    // the only error before reachability.
    if (!CyberdeckMods::wireless_jackin_installed(*player_)) {
        hack_term_emit("jack: no wireless jack-in device installed.",
                       UITag::TextDim);
        hack_term_emit("       (requires Wireless Jack-In Module.)",
                       UITag::TextDim);
        return;
    }
    auto* h = world_->find_hackable_by_ip(*parsed);
    if (!h) {
        hack_term_emit("jack: " + format_ip(*parsed) + ": host unreachable", UITag::TextDim);
        return;
    }
    if (h->jack_in_node_id <= 0) {
        hack_term_emit("jack: target has no node id (not yet registered)", UITag::TextDim);
        return;
    }
    jack_in_request_node_id_ = static_cast<uint32_t>(h->jack_in_node_id);
    hack_term_emit(">> uploading consciousness... <<");
}

// Plan 7: `ssh [<user>@]<ip>` — opens a per-device shell.
// Manual ssh strict semantics (spec §4): root@locked-unescalated rejects with
// permission-denied + try-guest hint and DOES NOT open. guest@ always succeeds.
// Spec §16: AlienTech-tagged devices reject with "protocol not understood
// (alien tech)" — no shell opens.
void PdaScreen::hack_term_cmd_ssh(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        hack_term_emit("usage: ssh [<user>@]<ip>", UITag::TextDim);
        return;
    }
    if (!world_ || !player_) {
        hack_term_emit("ssh: state unavailable.", UITag::TextDim);
        return;
    }
    if (!has_cat_hacking(*player_)) {
        hack_term_emit("ssh: requires Cat_Hacking skill.", UITag::TextDim);
        return;
    }

    // Parse `[user@]ip`.
    std::string user = "root";
    std::string ip_str = args[1];
    if (auto at = ip_str.find('@'); at != std::string::npos) {
        user   = ip_str.substr(0, at);
        ip_str = ip_str.substr(at + 1);
    }
    auto parsed = parse_ip(ip_str);
    if (!parsed) {
        hack_term_emit("ssh: invalid IP '" + ip_str + "'", UITag::TextDim);
        return;
    }
    const auto* h = world_->find_hackable_by_ip(*parsed);
    if (!h) {
        hack_term_emit("ssh: " + format_ip(*parsed) + ": host unreachable", UITag::TextDim);
        return;
    }

    // Plan 7 §16 — AlienTech opt-out. Precursor / Conclave / Stellari
    // consoles speak a non-POSIX dialect; ssh just refuses to handshake.
    if (has_tag(h->tags, HackTag::AlienTech)) {
        hack_term_emit("ssh: " + format_ip(*parsed) +
                       ": protocol not understood (alien tech).",
                       UITag::TextDim);
        return;
    }

    bool wants_root = (user == "root");
    bool locked = has_tag(h->tags, HackTag::Locked);
    if (wants_root && locked && !h->escalated) {
        // Strict reject: permission-denied + try-guest hint. No shell opens.
        hack_term_emit("ssh: " + format_ip(*parsed) +
                       ": permission denied (root login disabled).",
                       UITag::TextDim);
        hack_term_emit("      try: ssh guest@" + format_ip(*parsed),
                       UITag::TextDim);
        return;
    }

    // Queue the shell-open request for game_input.cpp to consume.
    ssh_request_ip_   = *parsed;
    ssh_request_root_ = wants_root;
    hack_term_emit("ssh: connecting to " + format_ip(*parsed) + " as " + user + "...",
                   UITag::TextDim);
}

void PdaScreen::hack_term_cmd_lore() {
    ConsciousnessSave cs;
    read_consciousness(cs);

    if (cs.lore_archive.empty()) {
        hack_term_emit("no decrypted archives.", UITag::TextDim);
        return;
    }

    hack_term_emit("decrypted archives:");
    for (const auto& a : cs.lore_archive) {
        char line[160];
        std::snprintf(line, sizeof line, "  %-24s  (origin: tick %d)",
                      a.archive_id.c_str(),
                      static_cast<int>(a.world_tick_origin));
        hack_term_emit(line);
    }
    hack_term_emit("");
    hack_term_emit("use:  cat <archive-id>", UITag::TextDim);
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
    hack_term_emit(next_motd(), UITag::TextDim);
}
void PdaScreen::hack_term_cmd_history() {
    for (size_t i = 0; i < hack_term_history_.size(); ++i) {
        hack_term_emit("  " + std::to_string(i) + "  " + hack_term_history_[i]);
    }
}

} // namespace astra
