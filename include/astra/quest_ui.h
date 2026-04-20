#pragma once

#include <string>

namespace astra {

struct Quest;

// Render a quest's body text — description, objectives, rewards —
// without any speaker preamble. Shared by the NPC offer dialog and the
// auto-accept quest popup.
std::string format_quest_body(const Quest& q);

} // namespace astra
