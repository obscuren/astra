#include "astra/event_bus.h"

#include <algorithm>

namespace astra {

EventKind event_kind_of(const Event& ev) {
    return std::visit([](auto&& payload) -> EventKind {
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, SystemEnteredEvent>)
            return EventKind::SystemEntered;
        else if constexpr (std::is_same_v<T, BodyEnteredEvent>)
            return EventKind::BodyEntered;
        else if constexpr (std::is_same_v<T, QuestStageCompletedEvent>)
            return EventKind::QuestStageCompleted;
    }, ev);
}

HandlerId EventBus::subscribe(EventKind kind, EventHandler handler) {
    HandlerId id = next_id_++;
    subs_.push_back({id, kind, std::move(handler)});
    return id;
}

void EventBus::unsubscribe(HandlerId id) {
    subs_.erase(std::remove_if(subs_.begin(), subs_.end(),
                               [id](const Subscription& s) { return s.id == id; }),
                subs_.end());
}

void EventBus::emit(Game& game, const Event& ev) {
    EventKind kind = event_kind_of(ev);
    // Copy list to allow handlers to subscribe/unsubscribe during emit.
    auto snapshot = subs_;
    for (auto& s : snapshot) {
        if (s.kind == kind) s.handler(game, ev);
    }
}

void EventBus::clear() {
    subs_.clear();
    next_id_ = 1;
}

} // namespace astra
