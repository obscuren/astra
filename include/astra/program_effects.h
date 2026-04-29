#pragma once

#include "astra/program.h"

namespace astra {

class Game;
struct Hackable;

// Apply a quickhack effect to a target. Pre-conditions are validated
// upstream by HackingSystem::execute_quickhack (program is .qh, target
// passes filter, RAM debited, Detection bumped). This function only
// performs the world mutation.
void apply_program_effect(ProgramId id, Game& game, Hackable& target,
                          int target_x, int target_y);

} // namespace astra
