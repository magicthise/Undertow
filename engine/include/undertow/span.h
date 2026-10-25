#pragma once

#include <iosfwd>
#include <optional>
#include <vector>

#include "undertow/rational.h"

namespace undertow {

// A stretch of time from begin up to, but not including, end. A zero-width
// span (begin == end) stands for a single instant.
struct TimeSpan {
    Rational begin;
    Rational end;

    TimeSpan() = default;
    TimeSpan(Rational begin, Rational end) : begin(begin), end(end) {}

    bool isZeroWidth() const { return begin == end; }
    Rational duration() const { return end - begin; }

    // Splits the span at cycle boundaries. A zero-width span comes back as is.
    std::vector<TimeSpan> cycles() const;

    // The overlap with another span, or nothing. Because spans exclude their
    // end, spans that only touch end-to-start don't overlap, and a zero-width
    // overlap only counts when the instant lies inside both spans.
    std::optional<TimeSpan> intersection(const TimeSpan& other) const;

    template <class F>
    TimeSpan withTime(F f) const {
        return TimeSpan(f(begin), f(end));
    }
};

bool operator==(const TimeSpan& a, const TimeSpan& b);
bool operator!=(const TimeSpan& a, const TimeSpan& b);
std::ostream& operator<<(std::ostream& out, const TimeSpan& span);

// The whole cycle containing t.
inline TimeSpan wholeCycle(const Rational& t) { return TimeSpan(sam(t), nextSam(t)); }

}  // namespace undertow
