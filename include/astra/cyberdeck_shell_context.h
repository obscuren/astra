#pragma once

// Plan 7 — CyberdeckShellContext.
//
// Cyberdeck-shell flavour (`pidgin$ `, `dck-2> `, `pda> `) lives here as a
// ShellContext subclass. Pushed onto the ShellStack on first PDA-open with a
// deck equipped; persists across PDA close/reopen and across save/load when
// the deck is still equipped. The owner_id is the deck's `item_def_id`.
//
// `on_push` greets per-deck (banner + MOTD), matching the existing
// `hack_term_greet_for_deck()` UX. `submit_command` dispatches through the
// HackCommandRegistry filtered by Scope::Cyberdeck or Scope::Universal.

#include "astra/device_shell.h"     // for ShellOutputSink
#include "astra/shell_context.h"

#include <cstdint>
#include <string>

namespace astra {

class CyberdeckShellContext : public ShellContext {
public:
    explicit CyberdeckShellContext(uint16_t deck_def_id)
        : deck_def_id_(deck_def_id) {}

    // ── ShellContext overrides ──
    int owner_id() const override { return static_cast<int>(deck_def_id_); }
    std::string prompt() const override;
    void on_push(ShellOutputSink& sink, Game& game) override;
    void submit_command(const std::string& line,
                        ShellOutputSink& sink, Game& game) override;
    CyberdeckShellContext* as_cyberdeck() override { return this; }

    uint16_t deck_def_id() const { return deck_def_id_; }
    void set_deck_def_id(uint16_t v) { deck_def_id_ = v; }

    // Bound output sink. Set when this context is pushed onto the stack;
    // commands can route output via emit() instead of capturing the sink
    // pointer themselves. Mirrors DeviceShell::sink().
    void bind_sink(ShellOutputSink* s) { sink_ = s; }
    ShellOutputSink* sink() { return sink_; }
    void emit(const std::string& line, UITag tag = UITag::TextDefault);

private:
    uint16_t deck_def_id_ = 0;
    ShellOutputSink* sink_ = nullptr;

    // Per-deck banner + MOTD lines emitted on push (and after `clear` for
    // some decks). Phase B keeps the full menu in pda_hacking_tab.cpp; this
    // class is the public face of the cyberdeck shell that flows through the
    // ShellStack.
    void emit_banner_(ShellOutputSink& sink) const;
};

} // namespace astra
