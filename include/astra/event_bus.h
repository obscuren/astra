#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace astra {

class Game; // forward declare

// ─── Event payloads ───────────────────────────────────────────────
// Keep these tiny. Add new event types by adding a new struct + a new
// variant arm. Producers emit, scenarios subscribe.

struct SystemEnteredEvent {
    uint32_t system_id = 0;
    uint32_t previous_system_id = 0;  // 0 on first entry
};

struct BodyEnteredEvent {
    uint32_t system_id = 0;
    int body_index = -1;
    bool is_station = false;
};

struct QuestStageCompletedEvent {
    std::string quest_id;
};

using Event = std::variant<
    SystemEnteredEvent,
    BodyEnteredEvent,
    QuestStageCompletedEvent
>;

// ─── Bus ──────────────────────────────────────────────────────────

using HandlerId = uint64_t;

enum class EventKind : uint32_t {
    SystemEntered = 0,
    BodyEntered,
    QuestStageCompleted,
    _Count
};

EventKind event_kind_of(const Event& ev);

using EventHandler = std::function<void(Game&, const Event&)>;

class EventBus {
public:
    HandlerId subscribe(EventKind kind, EventHandler handler);
    void unsubscribe(HandlerId id);

    // Calls every handler subscribed to the event's kind in registration
    // order. Exceptions escape — handlers must not throw on hot paths.
    void emit(Game& game, const Event& ev);

    // Removes all subscriptions. Called on world reset / new game.
    void clear();

private:
    struct Subscription {
        HandlerId id;
        EventKind kind;
        EventHandler handler;
    };
    std::vector<Subscription> subs_;
    HandlerId next_id_ = 1;
};

} // namespace astra
