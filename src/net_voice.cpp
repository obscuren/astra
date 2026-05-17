#include "astra/net_voice.h"

namespace astra::net_voice {

std::string cmd(const std::string& body) {
    return "> " + body;
}

std::string sys(const std::string& body) {
    return ">> " + body;
}

}  // namespace astra::net_voice
