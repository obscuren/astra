#pragma once

// Plan 7 — polymorphic shell-context base class.
//
// Both the cyberdeck shell (pda> ...) and the per-device ssh shell
// (TUR-OS:root# ...) are subclasses of ShellContext. A ShellStack owns the
// active stack (cyberdeck pushed at PDA-open, device pushed on ssh, popped on
// exit). Visually nothing changes — this is a pure refactor that promotes the
// implicit "device shell open?" boolean to an explicit context-stack lookup.
//
// Per-context history. Single unified Command type (see hack_command.h) with
// scope filtering.

#include <string>
#include <vector>

namespace astra {

class CyberdeckShellContext;
class DeviceShell;          // Renamed → DeviceShellContext in step 3.
class Game;
class ShellOutputSink;

class ShellContext {
public:
    virtual ~ShellContext() = default;

    // Hackable jack_in_node_id, deck item_def_id, or -1 for "ambient".
    virtual int owner_id() const = 0;

    // Prompt rendered at the bottom of the unified PDA Hacking scroll
    // (e.g. "pda> ", "TUR-OS:root# ").
    virtual std::string prompt() const = 0;

    // Lifecycle hooks. Called by ShellStack at push/pop and when this context
    // becomes the active top after popping a child (for re-greet, transcript
    // continuity, etc.).
    virtual void on_push  (ShellOutputSink&, Game&) {}
    virtual void on_pop   (ShellOutputSink&, Game&) {}
    virtual void on_resume(ShellOutputSink&, Game&) {}

    // Submit a typed line. Implementation looks up the Command in the
    // registry, filters by scope/tag/tier, and executes.
    virtual void submit_command(const std::string& line,
                                ShellOutputSink&, Game&) = 0;

    // Long-channel hooks. Default no-op so cyberdeck doesn't carry boilerplate.
    virtual void tick_world(Game&) {}
    virtual bool channel_active() const { return false; }
    virtual void abort_channel(Game&, const char* reason) { (void)reason; }

    // Typed accessors. Each subclass overrides its own.
    virtual CyberdeckShellContext* as_cyberdeck() { return nullptr; }
    virtual DeviceShell*           as_device()    { return nullptr; }

    // Per-context command history. Up/down arrow walks THIS, not a global
    // buffer. Cyberdeck history persists across PDA close/reopen (its context
    // sticks); device-session history dies with the popped context.
    std::vector<std::string>& history() { return history_; }
    const std::vector<std::string>& history() const { return history_; }
    void push_history(const std::string& cmd) {
        history_.push_back(cmd);
        if (history_.size() > 64) history_.erase(history_.begin());
    }

protected:
    std::vector<std::string> history_;
};

} // namespace astra
