#pragma once

#include <algorithm>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "undertow/pattern.h"

namespace undertow::testing {

// Renders a pattern's events as compact text, sorted by time, for readable
// comparisons: "0-1/2:a 1/2-1:b". A fragment of a longer event shows the
// whole event in brackets, "1/2-1(0-1):a"; an event with no whole starts
// with '~'.
inline std::string show(const Pattern<std::string>& pat, const Rational& begin, const Rational& end) {
    auto events = pat.query(begin, end);
    std::sort(events.begin(), events.end(), [](const Event<std::string>& a, const Event<std::string>& b) {
        return std::tie(a.part.begin, a.part.end, a.value) < std::tie(b.part.begin, b.part.end, b.value);
    });
    std::ostringstream out;
    for (size_t i = 0; i < events.size(); ++i) {
        const auto& e = events[i];
        if (i > 0) out << ' ';
        if (!e.whole) out << '~';
        out << e.part.begin << '-' << e.part.end;
        if (e.whole && *e.whole != e.part) out << '(' << e.whole->begin << '-' << e.whole->end << ')';
        out << ':' << e.value;
    }
    return out.str();
}

}  // namespace undertow::testing
