#include "undertow/span.h"

#include <algorithm>
#include <ostream>

namespace undertow {

std::vector<TimeSpan> TimeSpan::cycles() const {
    if (isZeroWidth()) return {*this};
    std::vector<TimeSpan> pieces;
    Rational from = begin;
    while (from < end) {
        const Rational to = std::min(nextSam(from), end);
        pieces.emplace_back(from, to);
        from = to;
    }
    return pieces;
}

std::optional<TimeSpan> TimeSpan::intersection(const TimeSpan& other) const {
    const Rational from = std::max(begin, other.begin);
    const Rational to = std::min(end, other.end);
    if (from > to) return std::nullopt;
    if (from == to) {
        // An instant at the (excluded) end of a non-empty span isn't inside it.
        if (!isZeroWidth() && from == end) return std::nullopt;
        if (!other.isZeroWidth() && from == other.end) return std::nullopt;
    }
    return TimeSpan(from, to);
}

bool operator==(const TimeSpan& a, const TimeSpan& b) { return a.begin == b.begin && a.end == b.end; }
bool operator!=(const TimeSpan& a, const TimeSpan& b) { return !(a == b); }

std::ostream& operator<<(std::ostream& out, const TimeSpan& span) { return out << span.begin << "-" << span.end; }

}  // namespace undertow
