#include "astra/cyberdeck_shell_context.h"

#include "astra/hack_command.h"
#include "astra/item_ids.h"

#include <sstream>

namespace astra {

namespace {

// Returns the prompt prefix for the currently-equipped deck.
const char* prompt_for_deck_(uint16_t deck_def_id) {
    switch (deck_def_id) {
        case ITEM_PIDGIN_MK1:    return "pidgin$ ";
        case ITEM_POLYGLOT_DCK2: return "dck-2> ";
        default:                 return "pda> ";
    }
}

// Rotating MOTD pool. Tipped slightly cyberpunk on purpose.
const char* next_motd_() {
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

std::string CyberdeckShellContext::prompt() const {
    return prompt_for_deck_(deck_def_id_);
}

void CyberdeckShellContext::on_push(ShellOutputSink& sink, Game&) {
    // Bind the sink so commands routed through `submit_command` can emit
    // output without re-fetching it. The per-deck banner is still emitted
    // by PdaScreen::hack_term_greet_for_deck on first display so the visual
    // sequence stays identical to pre-refactor.
    sink_ = &sink;
}

void CyberdeckShellContext::emit(const std::string& line, UITag tag) {
    if (sink_) sink_->shell_emit_line(line, tag);
}

void CyberdeckShellContext::emit_banner_(ShellOutputSink& sink) const {
    switch (deck_def_id_) {
        case ITEM_PIDGIN_MK1:
            sink.shell_emit_line("   ____  _     _       _       ", UITag::TextDim);
            sink.shell_emit_line("  |  _ \\(_) __| | __ _(_)_ __  ", UITag::TextDim);
            sink.shell_emit_line("  | |_) | |/ _` |/ _` | | '_ \\ ", UITag::TextDim);
            sink.shell_emit_line("  |  __/| | (_| | (_| | | | | |", UITag::TextDim);
            sink.shell_emit_line("  |_|   |_|\\__,_|\\__, |_|_| |_|", UITag::TextDim);
            sink.shell_emit_line("                 |___/         ", UITag::TextDim);
            sink.shell_emit_line("  ~ pawn-shop deck, lightly cursed ~", UITag::TextDim);
            sink.shell_emit_line("", UITag::TextDim);
            sink.shell_emit_line("Press 'help' for the basics.", UITag::TextDim);
            break;
        case ITEM_POLYGLOT_DCK2:
            sink.shell_emit_line("     ____   ____ _  __        ___", UITag::TextDim);
            sink.shell_emit_line("    |  _ \\ / ___| |/ /       |__ \\", UITag::TextDim);
            sink.shell_emit_line("    | | | | |   | ' /   ___    / /", UITag::TextDim);
            sink.shell_emit_line("    | |_| | |___| . \\  |___|  / /_", UITag::TextDim);
            sink.shell_emit_line("    |____/ \\____|_|\\_\\       |____|", UITag::TextDim);
            sink.shell_emit_line("    POLYGLOT DCK-2", UITag::TextDim);
            sink.shell_emit_line("", UITag::TextDim);
            sink.shell_emit_line("    CPU 2 / RAM 8 / SLOTS 4", UITag::TextDim);
            sink.shell_emit_line("    thermal envelope ........ nominal", UITag::TextDim);
            sink.shell_emit_line("    operator profile ........ AUTHENTICATED", UITag::TextDim);
            sink.shell_emit_line("", UITag::TextDim);
            sink.shell_emit_line("  Press 'help' for command list.", UITag::TextDim);
            break;
        default:
            sink.shell_emit_line("Cyberdeck online.", UITag::TextDim);
            sink.shell_emit_line("Press 'help' for command list.", UITag::TextDim);
            break;
    }
    sink.shell_emit_line(next_motd_(), UITag::TextDim);
}

void CyberdeckShellContext::submit_command(const std::string& line,
                                           ShellOutputSink& sink,
                                           Game& game) {
    // Refresh the bound sink each submit so callers that rebind (e.g. tests)
    // don't run into stale pointers. Cheap.
    sink_ = &sink;

    // Trim leading/trailing whitespace.
    std::string trimmed = line;
    auto first = trimmed.find_first_not_of(" \t");
    if (first == std::string::npos) return;
    trimmed = trimmed.substr(first);
    while (!trimmed.empty() &&
           (trimmed.back() == ' ' || trimmed.back() == '\t')) {
        trimmed.pop_back();
    }
    if (trimmed.empty()) return;

    push_history(trimmed);

    ParsedArgs args = parse_command_line(trimmed);
    if (args.argv.empty()) return;
    const std::string& name = args.argv[0];

    auto& reg = HackCommandRegistry::get();
    const HackCommand* cmd = reg.find_for(name, *this);
    if (!cmd) {
        emit(name + ": command not found. Try 'help'.", UITag::TextDim);
        return;
    }
    if (cmd->execute) {
        HackCommandResult r = cmd->execute(args, *this, game);
        if (!r.message.empty()) emit(r.message, UITag::TextDim);
    }
}

} // namespace astra
