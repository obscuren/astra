// Plan 7 — `echo` cyberdeck command. Concatenates argv[1..] with single
// spaces and prints. Mirrors a real shell's echo(1).

#include "astra/cyberdeck_shell_context.h"
#include "astra/hack_command.h"
#include "astra/shell_context.h"

#include <string>

namespace astra {

namespace {

HackCommandResult exec_echo(const ParsedArgs& args, ShellContext& ctx, Game&) {
    auto* deck = ctx.as_cyberdeck();
    if (!deck) return {};
    std::string out;
    for (size_t i = 1; i < args.argv.size(); ++i) {
        if (i > 1) out += ' ';
        out += args.argv[i];
    }
    deck->emit(out);
    return {};
}

const HackCommand k_echo{
    "echo",
    "echo <text...>",
    "print arguments",
    CommandScope::Cyberdeck,
    HackTag::None, false, 0, 0, 0, false,
    &exec_echo,
};

struct AutoRegister {
    AutoRegister() { register_hack_command(&k_echo); }
};
const AutoRegister k_auto;

} // namespace

void register_echo_command_anchor() { (void)&k_auto; }

} // namespace astra
