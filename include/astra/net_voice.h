#pragma once
#include <string>
namespace astra::net_voice {
// Runner command-line voice. cmd() = the deck echoing the runner's
// intent (single-chevron "> " prefix the log pane styles as command
// voice). sys() = a terse system note (">> " prefix). The canonical
// register is the storyboard phrasing in /tmp/ui-mock-2 — terse,
// lowercase, declarative. Keep new lines in that voice.
std::string cmd(const std::string& body);
std::string sys(const std::string& body);
}  // namespace astra::net_voice
