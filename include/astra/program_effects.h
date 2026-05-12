#pragma once

#include "astra/program.h"

#include <string>

namespace astra {

class Game;
struct NetSession;
struct Hackable;

// Apply a quickhack effect to a target. Pre-conditions are validated
// upstream by HackingSystem::execute_quickhack (program is .qh, target
// passes filter, RAM debited, Detection bumped). This function only
// performs the world mutation.
void apply_program_effect(ProgramId id, Game& game, Hackable& target,
                          int target_x, int target_y);

// Grid-side program dispatch context.
struct NetProgramContext {
    Game&        game;
    NetSession& session;
    int          target_x;     // -1 if N/A
    int          target_y;
};

// Apply a Grid-.exe effect. Pre-conditions (RAM debit, heat cost) are
// handled upstream by the program picker. Returns a log message string.
std::string apply_program_in_grid(ProgramId id, NetProgramContext ctx);

} // namespace astra
