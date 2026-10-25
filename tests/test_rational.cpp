#include <cstdint>
#include <limits>

#include "check.h"
#include "undertow/rational.h"
#include "undertow/span.h"

using undertow::Rational;
using undertow::TimeSpan;

TEST_CASE("rational: stored in lowest terms with a positive denominator") {
    CHECK_EQ(Rational(2, 4), Rational(1, 2));
    CHECK_EQ(Rational(1, -2).num(), -1);
    CHECK_EQ(Rational(1, -2).den(), 2);
    CHECK_EQ(Rational(0, 5), Rational(0));
    CHECK_THROWS(Rational(1, 0), std::domain_error);
}

TEST_CASE("rational: arithmetic") {
    CHECK_EQ(Rational(1, 2) + Rational(1, 3), Rational(5, 6));
    CHECK_EQ(Rational(1, 2) - Rational(3, 4), Rational(-1, 4));
    CHECK_EQ(Rational(1, 2) * Rational(2, 3), Rational(1, 3));
    CHECK_EQ(Rational(1, 2) / Rational(1, 4), Rational(2));
    CHECK_EQ(-Rational(1, 2), Rational(-1, 2));
    CHECK_THROWS(Rational(1) / Rational(0), std::domain_error);
}

TEST_CASE("rational: comparison") {
    CHECK(Rational(1, 3) < Rational(1, 2));
    CHECK(Rational(-1, 2) < Rational(-1, 3));
    CHECK(Rational(7, 2) > Rational(3));
    CHECK(Rational(2, 4) <= Rational(1, 2));
}

TEST_CASE("rational: floor and ceil round the right way for negatives") {
    CHECK_EQ(Rational(3, 2).floor(), 1);
    CHECK_EQ(Rational(-1, 2).floor(), -1);
    CHECK_EQ(Rational(-1, 2).ceil(), 0);
    CHECK_EQ(Rational(2).floor(), 2);
    CHECK_EQ(undertow::cyclePos(Rational(-1, 4)), Rational(3, 4));
    CHECK_EQ(undertow::floorDiv(-1, 3), -1);
    CHECK_EQ(undertow::floorMod(-1, 3), 2);
    CHECK_EQ(undertow::floorMod(4, 3), 1);
}

TEST_CASE("rational: parse") {
    CHECK_EQ(Rational::parse("3"), Rational(3));
    CHECK_EQ(Rational::parse("0.25"), Rational(1, 4));
    CHECK_EQ(Rational::parse("-1.5"), Rational(-3, 2));
    CHECK_EQ(Rational::parse(".5"), Rational(1, 2));
    CHECK_EQ(Rational::parse("3/4"), Rational(3, 4));
    CHECK_THROWS(Rational::parse(""), std::invalid_argument);
    CHECK_THROWS(Rational::parse("-"), std::invalid_argument);
    CHECK_THROWS(Rational::parse("bd"), std::invalid_argument);
    CHECK_THROWS(Rational::parse("1/0"), std::invalid_argument);
    CHECK_THROWS(Rational::parse("1.5x"), std::invalid_argument);
}

TEST_CASE("rational: overflow is reported, not wrapped") {
    const Rational big(std::numeric_limits<int64_t>::max() / 2);
    CHECK_THROWS(big * Rational(4), undertow::RationalOverflow);
    CHECK_THROWS(big + big + big, undertow::RationalOverflow);
}

TEST_CASE("span: splitting at cycle boundaries") {
    const auto pieces = TimeSpan(Rational(1, 2), Rational(5, 2)).cycles();
    CHECK_EQ(pieces.size(), 3u);
    CHECK_EQ(pieces[0], TimeSpan(Rational(1, 2), 1));
    CHECK_EQ(pieces[1], TimeSpan(1, 2));
    CHECK_EQ(pieces[2], TimeSpan(2, Rational(5, 2)));
    CHECK_EQ(TimeSpan(1, 1).cycles().size(), 1u);
}

TEST_CASE("span: intersection excludes the end of a span") {
    CHECK(!TimeSpan(0, 1).intersection(TimeSpan(1, 2)));
    CHECK_EQ(*TimeSpan(0, 1).intersection(TimeSpan(Rational(1, 2), 2)), TimeSpan(Rational(1, 2), 1));
    // An instant at the start of a span is inside it; at the end it isn't.
    CHECK_EQ(*TimeSpan(1, 1).intersection(TimeSpan(1, 2)), TimeSpan(1, 1));
    CHECK(!TimeSpan(1, 1).intersection(TimeSpan(0, 1)));
}
