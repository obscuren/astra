#pragma once

// Plan 7 — ShellStack owns a stack of ShellContexts. Pushed in order:
//   - CyberdeckShellContext — first PDA open with deck equipped, persists
//     until deck is unequipped or the stack is cleared (save/load reset).
//   - DeviceShellContext — pushed on successful ssh; popped on `exit` or Esc.
//
// Owned by HackingSystem. Replaces the implicit `device_shell_` member.

#include "astra/shell_context.h"

#include <memory>
#include <vector>

namespace astra {

class Game;
class ShellOutputSink;

class ShellStack {
public:
    // Push ctx on top, then call ctx.on_push(sink, game). The stack takes
    // ownership.
    void push(std::unique_ptr<ShellContext> ctx, ShellOutputSink& sink, Game& game);

    // Pop the top context: calls on_pop, then on_resume on the new top
    // (if any). No-op when empty.
    void pop(ShellOutputSink& sink, Game& game);

    // Top of the stack, or nullptr when empty.
    ShellContext* active() {
        return stack_.empty() ? nullptr : stack_.back().get();
    }
    const ShellContext* active() const {
        return stack_.empty() ? nullptr : stack_.back().get();
    }

    bool empty() const { return stack_.empty(); }
    std::size_t size() const { return stack_.size(); }

    // Walk the stack from bottom to top. Used for save/load reset and
    // contexts that need to find a sibling (e.g. cyberdeck below a device).
    ShellContext* at(std::size_t i) {
        return i < stack_.size() ? stack_[i].get() : nullptr;
    }

    // Save/load reset. Pops every frame top-down, calling on_pop so each
    // context emits its closure line; then clears.
    void clear(ShellOutputSink& sink, Game& game);

    // Hard reset — destroys every context without firing on_pop. Use only on
    // new_game/load_save where a clean slate is the goal and there's no
    // sink to emit through.
    void force_clear() { stack_.clear(); }

private:
    std::vector<std::unique_ptr<ShellContext>> stack_;
};

} // namespace astra
