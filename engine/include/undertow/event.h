#pragma once

#include <optional>

#include "undertow/span.h"

namespace undertow {

// Something happening in a pattern. When a query cuts across an event, the
// result holds just the queried fragment (part) along with the event's full
// extent (whole), so players can tell a note's start from its continuation.
template <class T>
struct Event {
    // The full extent of the event; empty for continuously varying values.
    std::optional<TimeSpan> whole;
    // The fragment of the event inside the queried span.
    TimeSpan part;
    T value;

    // True when this fragment contains the event's start, i.e. it should trigger.
    bool hasOnset() const { return whole && whole->begin == part.begin; }
    const TimeSpan& wholeOrPart() const { return whole ? *whole : part; }
};

}  // namespace undertow
