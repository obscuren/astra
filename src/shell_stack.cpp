#include "astra/shell_stack.h"

namespace astra {

void ShellStack::push(std::unique_ptr<ShellContext> ctx,
                      ShellOutputSink& sink, Game& game) {
    if (!ctx) return;
    ShellContext* raw = ctx.get();
    stack_.push_back(std::move(ctx));
    raw->on_push(sink, game);
}

void ShellStack::pop(ShellOutputSink& sink, Game& game) {
    if (stack_.empty()) return;
    auto top = std::move(stack_.back());
    stack_.pop_back();
    top->on_pop(sink, game);
    if (!stack_.empty()) {
        stack_.back()->on_resume(sink, game);
    }
}

void ShellStack::clear(ShellOutputSink& sink, Game& game) {
    while (!stack_.empty()) {
        auto top = std::move(stack_.back());
        stack_.pop_back();
        top->on_pop(sink, game);
    }
}

} // namespace astra
