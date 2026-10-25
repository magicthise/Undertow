#include <string>
#include <vector>

#include "check.h"
#include "show.h"
#include "undertow/pattern.h"

using namespace undertow;
using undertow::testing::show;

namespace {

using P = Pattern<std::string>;

P word(const std::string& w) { return pure(w); }

P seq(const std::vector<std::string>& words) {
    std::vector<P> pats;
    for (const auto& w : words) pats.push_back(word(w));
    return fastcat(pats);
}

}  // namespace

TEST_CASE("pattern: pure fills each cycle and fragments partial queries") {
    CHECK_EQ(show(word("a"), 0, 2), "0-1:a 1-2:a");
    CHECK_EQ(show(word("a"), Rational(1, 2), Rational(3, 2)), "1/2-1(0-1):a 1-3/2(1-2):a");
    CHECK_EQ(show(silence<std::string>(), 0, 4), "");
}

TEST_CASE("pattern: fastcat and slowcat") {
    CHECK_EQ(show(seq({"a", "b"}), 0, 1), "0-1/2:a 1/2-1:b");
    CHECK_EQ(show(slowcat<std::string>({word("a"), word("b"), word("c")}), 0, 3), "0-1:a 1-2:b 2-3:c");
    // Each pattern in a slowcat carries on from its own last cycle.
    CHECK_EQ(show(slowcat<std::string>({slowcat<std::string>({word("a"), word("b")}), word("c")}), 0, 4),
             "0-1:a 1-2:c 2-3:b 3-4:c");
}

TEST_CASE("pattern: fast, slow, early, late") {
    CHECK_EQ(show(fast(2, seq({"a", "b"})), 0, 1), "0-1/4:a 1/4-1/2:b 1/2-3/4:a 3/4-1:b");
    CHECK_EQ(show(slow(2, seq({"a", "b"})), 0, 2), "0-1:a 1-2:b");
    CHECK_EQ(show(fast(0, seq({"a", "b"})), 0, 1), "");
    CHECK_EQ(show(early(Rational(1, 4), seq({"a", "b"})), 0, 1), "0-1/4(-1/4-1/4):a 1/4-3/4:b 3/4-1(3/4-5/4):a");
    CHECK_EQ(show(late(Rational(1, 4), seq({"a", "b"})), 0, 1), "0-1/4(-1/4-1/4):b 1/4-3/4:a 3/4-1(3/4-5/4):b");
}

TEST_CASE("pattern: rev") {
    CHECK_EQ(show(rev(seq({"a", "b", "c"})), 0, 1), "0-1/3:c 1/3-2/3:b 2/3-1:a");
    CHECK_EQ(show(fast(-1, seq({"a", "b"})), 0, 1), "0-1/2:b 1/2-1:a");
}

TEST_CASE("pattern: stack, compress, timecat") {
    CHECK_EQ(show(stack<std::string>({word("a"), seq({"b", "c"})}), 0, 1), "0-1/2:b 0-1:a 1/2-1:c");
    CHECK_EQ(show(compress(Rational(1, 4), Rational(3, 4), seq({"a", "b"})), 0, 1), "1/4-1/2:a 1/2-3/4:b");
    CHECK_EQ(show(compress(Rational(1, 2), Rational(1, 4), word("a")), 0, 1), "");
    CHECK_EQ(show(timecat<std::string>({{1, word("a")}, {2, word("b")}}), 0, 1), "0-1/3:a 1/3-1:b");
}

TEST_CASE("pattern: every applies on matching cycles") {
    auto twice = [](const P& p) { return fast(2, p); };
    CHECK_EQ(show(every(2, twice, word("a")), 0, 2), "0-1/2:a 1/2-1:a 1-2:a");
    CHECK_EQ(show(every(0, twice, word("a")), 0, 1), "0-1:a");
}

TEST_CASE("pattern: patterned arguments switch while each value is active") {
    const Pattern<Rational> factors = fastcat<Rational>({pure(Rational(1)), pure(Rational(2))});
    CHECK_EQ(show(fast(factors, seq({"a", "b"})), 0, 1), "0-1/2:a 1/2-3/4:a 3/4-1:b");
}

TEST_CASE("pattern: combining values from two patterns") {
    const P numbers = seq({"1", "2", "3"});
    auto join = [](const std::string& l, const std::string& r) { return l + r; };
    CHECK_EQ(show(appLeft(seq({"a", "b"}), numbers, join), 0, 1),
             "0-1/3(0-1/2):a1 1/3-1/2(0-1/2):a2 1/2-2/3(1/2-1):b2 2/3-1(1/2-1):b3");
    CHECK_EQ(show(appRight(seq({"a", "b"}), numbers, join), 0, 1),
             "0-1/3:a1 1/3-1/2(1/3-2/3):a2 1/2-2/3(1/3-2/3):b2 2/3-1:b3");
    CHECK_EQ(show(appBoth(seq({"a", "b"}), numbers, join), 0, 1), "0-1/3:a1 1/3-1/2:a2 1/2-2/3:b2 2/3-1:b3");
}

TEST_CASE("pattern: bjorklund rhythms") {
    CHECK(bjorklund(3, 8) == (std::vector<bool>{1, 0, 0, 1, 0, 0, 1, 0}));
    CHECK(bjorklund(5, 8) == (std::vector<bool>{1, 0, 1, 1, 0, 1, 1, 0}));
    CHECK(bjorklund(0, 4) == (std::vector<bool>{0, 0, 0, 0}));
    CHECK(bjorklund(4, 4) == (std::vector<bool>{1, 1, 1, 1}));
    CHECK(bjorklund(5, 3) == (std::vector<bool>{1, 1, 1, 1, 1}));
}

TEST_CASE("pattern: euclid") {
    CHECK_EQ(show(euclid(3, 8, 0, word("x")), 0, 1), "0-1/8:x 3/8-1/2:x 3/4-7/8:x");
    CHECK_EQ(show(euclid(3, 8, 2, word("x")), 0, 1), "1/8-1/4:x 1/2-5/8:x 3/4-7/8:x");
    CHECK_EQ(show(euclid(-3, 8, 0, word("x")), 0, 1), "1/8-1/4:x 1/4-3/8:x 1/2-5/8:x 5/8-3/4:x 7/8-1:x");
    CHECK_EQ(show(euclid(3, 0, 0, word("x")), 0, 1), "");
}
