// Checks Undertow against real Tidal. conformance/cases.txt lists the cases;
// tools/tidal-oracle/generate.py runs them through Tidal and writes
// conformance/tidal-expected.txt, which is committed so this test needs no
// Haskell.
//
// Events must match exactly, with one allowance: for a fragment that doesn't
// contain its event's start, only the fragment's time and value are
// compared. Tidal sometimes gives such fragments a whole event that doesn't
// even contain them (seen with compressed patterns, e.g. "a/2 b"), and these
// fragments never trigger anything, so the reported whole can't change what
// plays.

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "undertow/mini.h"
#include "undertow/pattern.h"

using namespace undertow;

namespace {

using P = Pattern<std::string>;

struct Case {
    std::string id;
    int64_t cycles = 0;
    std::string kind;
    std::string source;
};

const std::string kDirectory = UNDERTOW_CONFORMANCE_DIR;

std::vector<Case> readCases() {
    std::ifstream file(kDirectory + "/cases.txt");
    std::vector<Case> cases;
    std::string line;
    while (std::getline(file, line)) {
        const auto first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#') continue;
        std::istringstream fields(line);
        Case c;
        fields >> c.id >> c.cycles >> c.kind;
        std::getline(fields >> std::ws, c.source);
        cases.push_back(c);
    }
    return cases;
}

// Blanks the whole of a fragment that doesn't contain its event's start (see
// the top of the file). Lines are "wholeBegin wholeEnd partBegin partEnd
// value", with "-" for a missing whole.
std::string normalize(const std::string& line) {
    std::istringstream fields(line);
    std::string wholeBegin, wholeEnd, partBegin;
    fields >> wholeBegin >> wholeEnd >> partBegin;
    if (wholeBegin == "-" || wholeBegin == partBegin) return line;
    return "~ ~" + line.substr(wholeBegin.size() + 1 + wholeEnd.size());
}

std::vector<std::string> normalized(std::vector<std::string> lines) {
    for (auto& line : lines) line = normalize(line);
    std::sort(lines.begin(), lines.end());
    return lines;
}

// Tidal's events for each case.
std::map<std::string, std::vector<std::string>> readExpected() {
    std::ifstream file(kDirectory + "/tidal-expected.txt");
    std::map<std::string, std::vector<std::string>> expected;
    std::string line;
    std::vector<std::string>* current = nullptr;
    while (std::getline(file, line)) {
        if (line.rfind("case ", 0) == 0) {
            current = &expected[line.substr(5)];
        } else if (line == "end") {
            current = nullptr;
        } else if (current && !line.empty() && line[0] != '#') {
            current->push_back(line);
        }
    }
    for (auto& entry : expected) entry.second = normalized(entry.second);
    return expected;
}

// Undertow's events, in the line format tidal-oracle prints.
std::vector<std::string> describe(const P& pat, int64_t cycles) {
    std::vector<std::string> lines;
    for (const auto& e : pat.query(0, cycles)) {
        std::ostringstream line;
        if (e.whole) {
            line << e.whole->begin << ' ' << e.whole->end;
        } else {
            line << "- -";
        }
        line << ' ' << e.part.begin << ' ' << e.part.end << ' ' << e.value;
        lines.push_back(line.str());
    }
    return normalized(lines);
}

// A double as the exact fraction it holds, matching Haskell's toRational.
std::string exactFraction(double value) {
    int exponent = 0;
    const double mantissa = std::frexp(value, &exponent);
    auto numerator = static_cast<int64_t>(std::ldexp(mantissa, 53));
    exponent -= 53;
    while (exponent < 0 && numerator % 2 == 0) {
        numerator /= 2;
        ++exponent;
    }
    if (exponent >= 0) return Rational(numerator * (int64_t(1) << exponent)).toString();
    return Rational(numerator, int64_t(1) << -exponent).toString();
}

P m(const std::string& source) { return parseMini(source); }

Pattern<Rational> numbers(const std::string& source) {
    return fmap(parseMini(source), [](const std::string& word) { return Rational::parse(word); });
}

template <class T>
Pattern<T> constant(T value) {
    return pure(value);
}

// Undertow versions of the "expr" cases. Tidal's numeric arguments are
// patterns even when written as plain numbers, so these use the patterned
// forms too. Once the interpreter exists, the Tidal code will be read directly.
const std::map<std::string, std::function<P()>>& expressionCases() {
    auto twice = [](const P& p) { return fast(2, p); };
    static const std::map<std::string, std::function<P()>> cases = {
        {"expr/rev", [] { return rev(m("a b c")); }},
        {"expr/rev-alternate", [] { return rev(m("a <b c>")); }},
        {"expr/fast", [] { return fast(constant<Rational>(2), m("a b")); }},
        {"expr/fast-negative", [] { return fast(constant<Rational>(-2), m("a b c")); }},
        {"expr/fast-pattern", [] { return fast(numbers("1 2"), m("a b c")); }},
        {"expr/slow", [] { return slow(constant<Rational>(3), m("a b")); }},
        {"expr/early", [] { return early(constant(Rational(1, 4)), m("a b")); }},
        {"expr/late", [] { return late(constant(Rational(1, 4)), m("a b")); }},
        {"expr/every", [twice] { return every(constant<int64_t>(3), twice, m("a b")); }},
        {"expr/every-zero", [twice] { return every(constant<int64_t>(0), twice, m("a b")); }},
        {"expr/cat", [] { return cat<std::string>({m("a b"), m("c")}); }},
        {"expr/fastcat", [] { return fastcat<std::string>({m("a"), m("b c")}); }},
        {"expr/stack", [] { return stack<std::string>({m("a b"), m("c")}); }},
        {"expr/timecat", [] { return timecat<std::string>({{1, m("a")}, {2, m("b c")}}); }},
        {"expr/compress", [] { return compress(Rational(1, 4), Rational(3, 4), m("a b")); }},
        {"expr/euclid",
         [] { return euclid(constant<int64_t>(3), constant<int64_t>(8), constant<int64_t>(0), m("a")); }},
        {"expr/euclid-off",
         [] { return euclid(constant<int64_t>(3), constant<int64_t>(8), constant<int64_t>(2), m("a")); }},
    };
    return cases;
}

std::string listDifference(const std::vector<std::string>& from, const std::vector<std::string>& without) {
    std::vector<std::string> difference;
    std::set_difference(from.begin(), from.end(), without.begin(), without.end(), std::back_inserter(difference));
    std::string text;
    for (const auto& line : difference) text += "\n        " + line;
    return text;
}

}  // namespace

TEST_CASE("conformance: Undertow matches Tidal") {
    const auto cases = readCases();
    const auto expected = readExpected();
    CHECK(!cases.empty());
    for (const auto& c : cases) {
        const auto tidal = expected.find(c.id);
        if (tidal == expected.end()) {
            check::recordFailure(__FILE__, __LINE__, c.id + ": no Tidal output; rerun tools/tidal-oracle/generate.py");
            continue;
        }
        std::vector<std::string> actual;
        try {
            P pat;
            if (c.kind == "mini") {
                pat = m(c.source);
            } else if (c.kind == "note") {
                pat = fmap(parseMiniNotes(c.source), exactFraction);
            } else {
                const auto build = expressionCases().find(c.id);
                if (build == expressionCases().end()) {
                    check::recordFailure(__FILE__, __LINE__, c.id + ": no Undertow version in test_conformance.cpp");
                    continue;
                }
                pat = build->second();
            }
            actual = describe(pat, c.cycles);
        } catch (const std::exception& e) {
            check::recordFailure(__FILE__, __LINE__, c.id + ": " + e.what());
            continue;
        }
        if (actual != tidal->second) {
            check::recordFailure(__FILE__, __LINE__,
                                 c.id + " (" + c.source + ") differs from Tidal\n      only in Tidal:" +
                                     listDifference(tidal->second, actual) +
                                     "\n      only in Undertow:" + listDifference(actual, tidal->second));
        }
    }
}
