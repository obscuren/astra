#include "astra/pda_screen.h"

#include "astra/cyberdeck.h"
#include "astra/device_shell.h"
#include "astra/game.h"
#include "astra/grid_network.h"
#include "astra/hacking_system.h"
#include "astra/item_defs.h"
#include "astra/item_ids.h"
#include "astra/lan.h"
#include "astra/program.h"
#include "astra/skill_defs.h"
#include "astra/world_manager.h"

namespace astra {

namespace {

bool has_cat_hacking(const Player& p) {
    return player_has_skill(p, SkillId::Cat_Hacking);
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
    //
    // Plan 7 unified terminal: in-flight ritual reveal-bytes count as one
    // (partial) line at the bottom of scroll. The transient progress line
    // (long-channel `[#---] N%`) sits one row above the prompt — overwritten
    // each tick — so we don't pollute scroll with each tick.
    int top = 3;
    int bottom = ctx.height() - 2;
    int visible = bottom - top + 1;
    int hist_count = static_cast<int>(hack_term_lines_.size());

    // Cache content width for shell_progress_cells_hint (called outside
    // render from device_shell::tick_world / start_channel).
    hack_term_content_width_ = ctx.width();

    // Optional rows: transient progress, prompt. (Ritual lines are committed
    // straight to scroll — no partial-reveal row needed.)
    bool show_progress = hack_term_progress_set_ && !hack_term_progress_text_.empty();
    int extra_rows = (show_progress ? 1 : 0) + 1;  // +1 for prompt
    int total = hist_count + extra_rows;

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
    // Transient progress line.
    if (hack_term_scroll_ == 0 && show_progress && row <= bottom) {
        ctx.text({.x = 2, .y = row,
                  .content = hack_term_progress_text_,
                  .tag = hack_term_progress_tag_});
        ++row;
    }
    // Render the prompt only if it's still in view (not scrolled past).
    if (hack_term_scroll_ == 0 && row <= bottom) {
        // Cursor sits at hack_term_input_cursor_; render an underscore at
        // that position so the user can see where insertions/backspaces land.
        int cur = hack_term_input_cursor_;
        if (cur < 0) cur = 0;
        if (cur > static_cast<int>(hack_term_input_.size())) cur = static_cast<int>(hack_term_input_.size());
        // Plan 7 unified prompt: morphs to the device shell prompt while a
        // session is active.
        DeviceShell* dev = hacking_system_ ? hacking_system_->device_shell() : nullptr;
        std::string prefix;
        if (dev) {
            prefix = dev->prompt();
        } else {
            prefix = prompt_for_deck((*active_deck_slot)->item_def_id);
        }
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
    // Plan 7 unified terminal: the ssh session is a state transition of the
    // pda> terminal. While a DeviceShell session is active the same line
    // editor is used; only the prompt rendering and submission dispatch
    // morph. ESC routes to the shell so it closes (yanks cable in real-world
    // / returns to spatial sector view in-Grid). The PDA itself stays open.
    DeviceShell* active_dev = hacking_system_ ? hacking_system_->device_shell() : nullptr;
    bool session_active = active_dev != nullptr;

    if (session_active && key == 27 && game_) {
        // ESC outside ritual = exit / yank. DeviceShell::on_pop emits the
        // logout pair and reverts the prompt. If a long channel is active,
        // ESC instead aborts that channel (and keeps the session open).
        if (active_dev->channel_active()) {
            active_dev->abort_channel(*game_, "user");
        } else {
            hacking_system_->close_device_shell(*game_);
        }
        return;
    }
    // While a long channel is in flight we still allow ESC (handled above)
    // and Enter (no-op so the player can't submit a stale line). Movement
    // keys / typing are swallowed so the player just watches the bar.
    if (session_active && active_dev->channel_active()) {
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
            // Echo the prompt + input into the SAME scrollback. Whether we're
            // at pda> or inside an ssh session, output flows into one buffer.
            std::string echo_prefix;
            if (session_active) {
                echo_prefix = active_dev->prompt();
            } else {
                echo_prefix = prompt_for_deck((*deck_slot)->item_def_id);
            }
            hack_term_emit(echo_prefix + hack_term_input_, UITag::TextDefault);
            // Per-context history is pushed by the context's submit_command
            // (CyberdeckShellContext::submit_command and
            // DeviceShell::submit_command both call push_history).
            std::string line = hack_term_input_;
            hack_term_input_.clear();
            hack_term_input_cursor_ = 0;
            hack_term_history_cursor_ = -1;
            // Route through the active ShellContext (cyberdeck or device).
            // The cyberdeck context is pushed on PDA open with a deck — by
            // the time we get here (deck check above passed) it is on the
            // stack. Defensive: drop the line silently if not.
            ShellContext* active_ctx =
                hacking_system_ ? hacking_system_->shell_stack().active() : nullptr;
            if (active_ctx && game_) {
                active_ctx->submit_command(line, *this, *game_);
            }
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
    if (key == KEY_UP || key == KEY_DOWN) {
        // Per-context history. Walks the active ShellContext's vector
        // (cyberdeck below, device on top). With no context, do nothing —
        // by this point a deck is required, so a cyberdeck context exists.
        ShellContext* active_ctx =
            hacking_system_ ? hacking_system_->shell_stack().active() : nullptr;
        if (!active_ctx) return;
        const std::vector<std::string>& hist = active_ctx->history();
        int n = static_cast<int>(hist.size());
        if (key == KEY_UP) {
            if (n == 0) return;
            if (hack_term_history_cursor_ == -1)
                hack_term_history_cursor_ = n - 1;
            else if (hack_term_history_cursor_ > 0)
                --hack_term_history_cursor_;
            hack_term_input_ = hist[hack_term_history_cursor_];
        } else {
            if (hack_term_history_cursor_ == -1) return;
            if (hack_term_history_cursor_ < n - 1) {
                ++hack_term_history_cursor_;
                hack_term_input_ = hist[hack_term_history_cursor_];
            } else {
                hack_term_history_cursor_ = -1;
                hack_term_input_.clear();
            }
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
    // Single-key shortcuts (menu fallbacks) — only when input buffer is empty
    // AND no session is active (these are pda>-only menu fallbacks; inside an
    // ssh session 'P', 'I', 'N', 'L' are valid characters in commands).
    if (!session_active && hack_term_input_.empty()) {
        const char* synth = nullptr;
        switch (key) {
            case '?': synth = "help"; break;
            case 'P': synth = "ps"; break;
            case 'I': synth = "ls"; break;
            case 'N': synth = "nmap -m"; break;
            case 'L': synth = "lore"; break;
            default: break;
        }
        if (synth) {
            ShellContext* active_ctx =
                hacking_system_ ? hacking_system_->shell_stack().active() : nullptr;
            if (active_ctx && game_) {
                active_ctx->submit_command(synth, *this, *game_);
            }
            return;
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

// ── ShellOutputSink overrides (Plan 7 unified terminal) ──

void PdaScreen::shell_emit_line(const std::string& text, UITag tag) {
    hack_term_emit(text, tag);
}

void PdaScreen::shell_clear_scroll() {
    // Wipe the scroll. Don't re-greet the deck — a session-side `clear`
    // shouldn't print the cyberdeck banner. The cyberdeck-side `clear`
    // (universal cmd in cmd_universals.cpp + hack_term_re_greet on
    // PdaScreen) is responsible for re-greeting.
    hack_term_lines_.clear();
    hack_term_scroll_ = 0;
}

void PdaScreen::shell_set_progress_line(const std::string& text, UITag tag) {
    hack_term_progress_text_ = text;
    hack_term_progress_tag_ = tag;
    hack_term_progress_set_ = !text.empty();
    // Snap to bottom so the bar stays visible while it ticks.
    if (hack_term_progress_set_) hack_term_scroll_ = 0;
}

int PdaScreen::shell_progress_cells_hint() const {
    // 50% of the Hacking-tab content width, minus 7 cells for "[" + "] " +
    // "100%" overhead. Falls back to 10 cells before the first render.
    int w = hack_term_content_width_;
    if (w <= 0) return 10;
    int cells = (w / 2) - 7;
    if (cells < 4) cells = 4;
    return cells;
}

void PdaScreen::shell_commit_progress_line() {
    if (hack_term_progress_set_ && !hack_term_progress_text_.empty()) {
        hack_term_emit(hack_term_progress_text_, hack_term_progress_tag_);
    }
    hack_term_progress_text_.clear();
    hack_term_progress_set_ = false;
}

void PdaScreen::hack_term_autotype_and_submit(const std::string& line) {
    if (line.empty()) return;
    // Echo + dispatch through the active ShellContext so the line lives in
    // ctx history, the prompt printed in scroll matches what the player
    // would see if they typed it themselves, and any tag/permission rejects
    // fall through the same code path. Falls back to the legacy dispatch
    // if no context is on the stack (defensive — the cyberdeck context is
    // pushed on PDA open with a deck).
    auto* deck_slot = player_ ? player_->equipment.equipped_cyberdeck() : nullptr;
    if (!deck_slot || !*deck_slot || !(*deck_slot)->deck) return;
    const char* prefix = prompt_for_deck((*deck_slot)->item_def_id);
    hack_term_emit(prefix + line, UITag::TextDefault);
    ShellContext* active_ctx =
        hacking_system_ ? hacking_system_->shell_stack().active() : nullptr;
    if (active_ctx && game_) {
        active_ctx->submit_command(line, *this, *game_);
    }
}

void PdaScreen::draw_hacking_into(Renderer* renderer, Rect bounds) {
    if (!renderer || bounds.w <= 0 || bounds.h <= 0) return;
    UIContext ctx(renderer, bounds);
    // Paint a clean background so the Tron playfield doesn't bleed through.
    for (int j = 0; j < ctx.height(); ++j) {
        for (int i = 0; i < ctx.width(); ++i) {
            ctx.put(i, j, ' ', Color::White, Color::Black);
        }
    }
    draw_hacking(ctx);
}

} // namespace astra
