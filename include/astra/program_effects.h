#pragma once

#include "astra/program.h"

namespace astra {

class Game;
struct Hackable;

// TODO(task-9): Replace this no-op stub with the real dispatch table.
// Task 7 needs the symbol to link; Task 9 implements the three effects
// (reboot_optics, friendly_fire, data_leech).
inline void apply_program_effect(ProgramId, Game&, Hackable&, int, int) {}

} // namespace astra
