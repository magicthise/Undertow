#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "undertow/event.h"
#include "undertow/span.h"

namespace undertow {

template <class T>
using Events = std::vector<Event<T>>;

// A pattern is a function from a span of time to the events active in it.
// Every other operation (sequencing, changing speed, combining) wraps that
// function in another one. Copies share the function, so they're cheap.
template <class T>
class Pattern {
public:
    using Value = T;
    using Query = std::function<Events<T>(const TimeSpan&)>;

    // The empty pattern.
    Pattern() : query_(std::make_shared<const Query>([](const TimeSpan&) { return Events<T>{}; })) {}
    explicit Pattern(Query query) : query_(std::make_shared<const Query>(std::move(query))) {}

    Events<T> query(const TimeSpan& span) const { return (*query_)(span); }
    Events<T> query(const Rational& begin, const Rational& end) const { return query(TimeSpan(begin, end)); }

    // The value of a constant pattern made by pure(), or null. Functions with
    // patterned arguments use it to apply a plain value directly, as Tidal
    // does; the general path would split events at every cycle boundary.
    const T* pureValue() const { return pureValue_.get(); }
    Pattern withPureValue(T value) const {
        Pattern copy = *this;
        copy.pureValue_ = std::make_shared<const T>(std::move(value));
        return copy;
    }

private:
    std::shared_ptr<const Query> query_;
    std::shared_ptr<const T> pureValue_;
};

// ---------------------------------------------------------------------------
// Building blocks

template <class T>
Pattern<T> silence() {
    return Pattern<T>();
}

// The value once per cycle, filling the cycle.
template <class T>
Pattern<T> pure(T value) {
    return Pattern<T>([value](const TimeSpan& span) {
               Events<T> events;
               for (const auto& part : span.cycles()) events.push_back(Event<T>{wholeCycle(part.begin), part, value});
               return events;
           })
        .withPureValue(value);
}

// Applies f to every value, keeping the timing.
template <class T, class F>
auto fmap(const Pattern<T>& pat, F f) {
    using U = std::decay_t<std::invoke_result_t<const F&, const T&>>;
    if (const T* value = pat.pureValue()) return pure<U>(f(*value));
    return Pattern<U>([pat, f](const TimeSpan& span) {
        Events<U> out;
        auto events = pat.query(span);
        out.reserve(events.size());
        for (auto& e : events) out.push_back(Event<U>{e.whole, e.part, f(e.value)});
        return out;
    });
}

// Queries one cycle at a time, for operations that work cycle by cycle.
template <class T>
Pattern<T> splitQueries(const Pattern<T>& pat) {
    return Pattern<T>([pat](const TimeSpan& span) {
        Events<T> out;
        for (const auto& cycle : span.cycles()) {
            auto events = pat.query(cycle);
            out.insert(out.end(), std::make_move_iterator(events.begin()), std::make_move_iterator(events.end()));
        }
        return out;
    });
}

template <class T, class F>
Pattern<T> withQueryTime(const Pattern<T>& pat, F f) {
    return Pattern<T>([pat, f](const TimeSpan& span) { return pat.query(span.withTime(f)); });
}

template <class T, class F>
Pattern<T> withEventTime(const Pattern<T>& pat, F f) {
    return Pattern<T>([pat, f](const TimeSpan& span) {
        auto events = pat.query(span);
        for (auto& e : events) {
            e.part = e.part.withTime(f);
            if (e.whole) e.whole = e.whole->withTime(f);
        }
        return events;
    });
}

// ---------------------------------------------------------------------------
// Time

// Plays each cycle backwards.
template <class T>
Pattern<T> rev(const Pattern<T>& pat) {
    return splitQueries(Pattern<T>([pat](const TimeSpan& span) {
        // Mirror the cycle around its midpoint: query the mirrored span, then
        // mirror the results back.
        const Rational cycle = sam(span.begin);
        const Rational mirror = cycle + cycle + 1;
        auto reflect = [&mirror](const TimeSpan& s) { return TimeSpan(mirror - s.end, mirror - s.begin); };
        auto events = pat.query(reflect(span));
        for (auto& e : events) {
            e.part = reflect(e.part);
            if (e.whole) e.whole = reflect(*e.whole);
        }
        return events;
    }));
}

// Speeds the pattern up by `factor`; a negative factor also reverses it.
template <class T>
Pattern<T> fast(const Rational& factor, const Pattern<T>& pat) {
    if (factor == 0) return silence<T>();
    if (factor < 0) return rev(fast(-factor, pat));
    auto sped = withQueryTime(pat, [factor](const Rational& t) { return t * factor; });
    return withEventTime(sped, [factor](const Rational& t) { return t / factor; });
}

template <class T>
Pattern<T> slow(const Rational& factor, const Pattern<T>& pat) {
    if (factor == 0) return silence<T>();
    return fast(Rational(1) / factor, pat);
}

// Shifts the pattern earlier in time by `offset` cycles (Tidal's <~).
template <class T>
Pattern<T> early(const Rational& offset, const Pattern<T>& pat) {
    auto shifted = withQueryTime(pat, [offset](const Rational& t) { return t + offset; });
    return withEventTime(shifted, [offset](const Rational& t) { return t - offset; });
}

// Shifts the pattern later in time by `offset` cycles (Tidal's ~>).
template <class T>
Pattern<T> late(const Rational& offset, const Pattern<T>& pat) {
    return early(-offset, pat);
}

// ---------------------------------------------------------------------------
// Combining patterns

// Plays all the patterns at once.
template <class T>
Pattern<T> stack(std::vector<Pattern<T>> pats) {
    auto shared = std::make_shared<const std::vector<Pattern<T>>>(std::move(pats));
    return Pattern<T>([shared](const TimeSpan& span) {
        Events<T> out;
        for (const auto& pat : *shared) {
            auto events = pat.query(span);
            out.insert(out.end(), std::make_move_iterator(events.begin()), std::make_move_iterator(events.end()));
        }
        return out;
    });
}

// Plays one pattern per cycle, in turn. Each pattern carries on from where it
// left off, so with two patterns the second plays its own cycle 0, 1, 2... on
// cycles 1, 3, 5...
template <class T>
Pattern<T> slowcat(std::vector<Pattern<T>> pats) {
    if (pats.empty()) return silence<T>();
    if (pats.size() == 1) return pats.front();
    auto shared = std::make_shared<const std::vector<Pattern<T>>>(std::move(pats));
    return Pattern<T>([shared](const TimeSpan& span) {
        const auto& pats = *shared;
        const auto count = static_cast<int64_t>(pats.size());
        Events<T> out;
        for (const auto& part : span.cycles()) {
            const int64_t cycle = part.begin.floor();
            const int64_t index = floorMod(cycle, count);
            // Shift so the chosen pattern sees how many times it has played.
            const Rational offset = cycle - floorDiv(cycle - index, count);
            for (auto& e : pats[static_cast<size_t>(index)].query(part.begin - offset, part.end - offset)) {
                e.part = TimeSpan(e.part.begin + offset, e.part.end + offset);
                if (e.whole) e.whole = TimeSpan(e.whole->begin + offset, e.whole->end + offset);
                out.push_back(std::move(e));
            }
        }
        return out;
    });
}

template <class T>
Pattern<T> cat(std::vector<Pattern<T>> pats) {
    return slowcat(std::move(pats));
}

// Squeezes all the patterns into one cycle, one after another.
template <class T>
Pattern<T> fastcat(std::vector<Pattern<T>> pats) {
    const auto count = static_cast<int64_t>(pats.size());
    if (count == 0) return silence<T>();
    return fast(Rational(count), slowcat(std::move(pats)));
}

// Squeezes each cycle into the window [begin, end) of the same cycle, with
// silence either side. Both ends must lie within 0 to 1.
template <class T>
Pattern<T> compress(const Rational& begin, const Rational& end, const Pattern<T>& pat) {
    if (begin >= end || begin < 0 || end > 1) return silence<T>();
    const Rational width = end - begin;
    return splitQueries(Pattern<T>([begin, end, width, pat](const TimeSpan& span) {
        const Rational cycle = sam(span.begin);
        const auto window = span.intersection(TimeSpan(cycle + begin, cycle + end));
        if (!window) return Events<T>{};
        auto inward = [&](const Rational& t) { return cycle + (t - cycle - begin) / width; };
        auto outward = [&](const Rational& t) { return cycle + begin + (t - cycle) * width; };
        auto events = pat.query(window->withTime(inward));
        for (auto& e : events) {
            e.part = e.part.withTime(outward);
            if (e.whole) e.whole = e.whole->withTime(outward);
        }
        return events;
    }));
}

// Like fastcat, but each pattern gets a share of the cycle proportional to its weight.
template <class T>
Pattern<T> timecat(const std::vector<std::pair<Rational, Pattern<T>>>& weighted) {
    Rational total = 0;
    for (const auto& entry : weighted) total += entry.first;
    if (total <= 0) return silence<T>();
    std::vector<Pattern<T>> parts;
    Rational position = 0;
    for (const auto& [weight, pat] : weighted) {
        if (weight <= 0) continue;
        parts.push_back(compress(position / total, (position + weight) / total, pat));
        position += weight;
    }
    return stack(std::move(parts));
}

// ---------------------------------------------------------------------------
// Patterns of patterns

// Flattens a pattern of patterns. While each outer event is active, the inner
// pattern it holds plays, keeping the inner pattern's own timing.
template <class T>
Pattern<T> innerJoin(const Pattern<Pattern<T>>& outer) {
    return Pattern<T>([outer](const TimeSpan& span) {
        Events<T> out;
        for (const auto& o : outer.query(span)) {
            for (auto& inner : o.value.query(o.part)) {
                if (auto part = inner.part.intersection(span))
                    out.push_back(Event<T>{inner.whole, *part, std::move(inner.value)});
            }
        }
        return out;
    });
}

// Lets a parameter be a pattern: while each value of `arg` is active, the
// result plays the pattern `build` makes from that value.
template <class A, class F>
auto patternify(const Pattern<A>& arg, F build) {
    if (const A* value = arg.pureValue()) return build(*value);
    return innerJoin(fmap(arg, build));
}

template <class T>
Pattern<T> fast(const Pattern<Rational>& factor, const Pattern<T>& pat) {
    return patternify(factor, [pat](const Rational& f) { return fast(f, pat); });
}

template <class T>
Pattern<T> slow(const Pattern<Rational>& factor, const Pattern<T>& pat) {
    return patternify(factor, [pat](const Rational& f) { return slow(f, pat); });
}

template <class T>
Pattern<T> early(const Pattern<Rational>& offset, const Pattern<T>& pat) {
    return patternify(offset, [pat](const Rational& o) { return early(o, pat); });
}

template <class T>
Pattern<T> late(const Pattern<Rational>& offset, const Pattern<T>& pat) {
    return patternify(offset, [pat](const Rational& o) { return late(o, pat); });
}

// ---------------------------------------------------------------------------
// Conditional

// Uses transform(pat) on cycles where test(cycle number) is true.
template <class T, class Test, class Transform>
Pattern<T> whenCycle(Test test, Transform transform, const Pattern<T>& pat) {
    const Pattern<T> transformed = transform(pat);
    return splitQueries(Pattern<T>([test, transformed, pat](const TimeSpan& span) {
        return test(span.begin.floor()) ? transformed.query(span) : pat.query(span);
    }));
}

// Applies transform every n-th cycle, starting with cycle 0.
template <class T, class Transform>
Pattern<T> every(int64_t n, Transform transform, const Pattern<T>& pat) {
    if (n == 0) return pat;
    return whenCycle([n](int64_t cycle) { return floorMod(cycle, n) == 0; }, transform, pat);
}

template <class T, class Transform>
Pattern<T> every(const Pattern<int64_t>& n, Transform transform, const Pattern<T>& pat) {
    return patternify(n, [transform, pat](int64_t value) { return every(value, transform, pat); });
}

// ---------------------------------------------------------------------------
// Combining values from two patterns

// Rhythm from the left pattern; each left event is combined with whatever the
// right pattern holds while it plays.
template <class A, class B, class F>
auto appLeft(const Pattern<A>& left, const Pattern<B>& right, F combine) {
    using R = std::decay_t<std::invoke_result_t<const F&, const A&, const B&>>;
    if (left.pureValue() && right.pureValue()) return pure<R>(combine(*left.pureValue(), *right.pureValue()));
    return Pattern<R>([left, right, combine](const TimeSpan& span) {
        Events<R> out;
        for (const auto& l : left.query(span)) {
            for (const auto& r : right.query(l.wholeOrPart())) {
                if (auto part = l.part.intersection(r.part))
                    out.push_back(Event<R>{l.whole, *part, combine(l.value, r.value)});
            }
        }
        return out;
    });
}

// Rhythm from the right pattern.
template <class A, class B, class F>
auto appRight(const Pattern<A>& left, const Pattern<B>& right, F combine) {
    using R = std::decay_t<std::invoke_result_t<const F&, const A&, const B&>>;
    if (left.pureValue() && right.pureValue()) return pure<R>(combine(*left.pureValue(), *right.pureValue()));
    return Pattern<R>([left, right, combine](const TimeSpan& span) {
        Events<R> out;
        for (const auto& r : right.query(span)) {
            for (const auto& l : left.query(r.wholeOrPart())) {
                if (auto part = r.part.intersection(l.part))
                    out.push_back(Event<R>{r.whole, *part, combine(l.value, r.value)});
            }
        }
        return out;
    });
}

// Rhythm from both: events happen wherever events from the two overlap.
template <class A, class B, class F>
auto appBoth(const Pattern<A>& left, const Pattern<B>& right, F combine) {
    using R = std::decay_t<std::invoke_result_t<const F&, const A&, const B&>>;
    if (left.pureValue() && right.pureValue()) return pure<R>(combine(*left.pureValue(), *right.pureValue()));
    return Pattern<R>([left, right, combine](const TimeSpan& span) {
        Events<R> out;
        const auto rightEvents = right.query(span);
        for (const auto& l : left.query(span)) {
            for (const auto& r : rightEvents) {
                auto part = l.part.intersection(r.part);
                if (!part) continue;
                std::optional<TimeSpan> whole;
                if (l.whole && r.whole)
                    whole = TimeSpan(std::max(l.whole->begin, r.whole->begin), std::min(l.whole->end, r.whole->end));
                out.push_back(Event<R>{whole, *part, combine(l.value, r.value)});
            }
        }
        return out;
    });
}

// ---------------------------------------------------------------------------
// Rhythm

// Spreads `pulses` hits as evenly as possible over `steps` steps (Bjorklund's
// algorithm). Asking for more pulses than steps gives that many hits.
std::vector<bool> bjorklund(int64_t pulses, int64_t steps);

// Plays the pattern on the hits of a Euclidean rhythm, rotated left by
// `rotation` steps. Negative pulses play on the gaps instead.
template <class T>
Pattern<T> euclid(int64_t pulses, int64_t steps, int64_t rotation, const Pattern<T>& pat) {
    if (steps <= 0) return silence<T>();
    const bool onGaps = pulses < 0;
    std::vector<Pattern<T>> cells;
    for (bool hit : bjorklund(onGaps ? -pulses : pulses, steps)) cells.push_back(hit != onGaps ? pat : silence<T>());
    auto rhythm = fastcat(std::move(cells));
    return rotation == 0 ? rhythm : early(Rational(rotation, steps), rhythm);
}

// Euclid with patterned arguments; a new rhythm starts wherever any argument changes.
template <class T>
Pattern<T> euclid(const Pattern<int64_t>& pulses, const Pattern<int64_t>& steps, const Pattern<int64_t>& rotation,
                  const Pattern<T>& pat) {
    struct Args {
        int64_t pulses;
        int64_t steps;
        int64_t rotation;
    };
    auto pulsesAndSteps = appBoth(pulses, steps, [](int64_t p, int64_t s) { return std::make_pair(p, s); });
    auto args = appBoth(pulsesAndSteps, rotation, [](const std::pair<int64_t, int64_t>& ps, int64_t r) {
        return Args{ps.first, ps.second, r};
    });
    return patternify(args, [pat](const Args& a) { return euclid(a.pulses, a.steps, a.rotation, pat); });
}

}  // namespace undertow
