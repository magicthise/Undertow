#include <string>

#include "check.h"
#include "show.h"
#include "undertow/mini.h"
#include "undertow/note.h"

using namespace undertow;
using undertow::testing::show;

namespace {

std::string mini(const std::string& source, const Rational& cycles = 1) { return show(parseMini(source), 0, cycles); }

// Returns the error position for source that should fail to parse, or -1.
long errorPosition(const std::string& source) {
    try {
        parseMini(source);
    } catch (const MiniParseError& e) {
        return static_cast<long>(e.position());
    }
    return -1;
}

}  // namespace

TEST_CASE("mini: sequences, rests and groups") {
    CHECK_EQ(mini("a b c"), "0-1/3:a 1/3-2/3:b 2/3-1:c");
    CHECK_EQ(mini("a ~ b"), "0-1/3:a 2/3-1:b");
    CHECK_EQ(mini("a [b c]"), "0-1/2:a 1/2-3/4:b 3/4-1:c");
    CHECK_EQ(mini("  a   b  "), "0-1/2:a 1/2-1:b");
    CHECK_EQ(mini(""), "");
}

TEST_CASE("mini: layers with commas") {
    CHECK_EQ(mini("[a b, c]"), "0-1/2:a 0-1:c 1/2-1:b");
    CHECK_EQ(mini("a b, c"), "0-1/2:a 0-1:c 1/2-1:b");
}

TEST_CASE("mini: alternation and polymeter") {
    CHECK_EQ(mini("<a b>", 2), "0-1:a 1-2:b");
    CHECK_EQ(mini("a <b c>", 2), "0-1/2:a 1/2-1:b 1-3/2:a 3/2-2:c");
    CHECK_EQ(mini("{a b c, d e}"), "0-1/3:a 0-1/3:d 1/3-2/3:b 1/3-2/3:e 2/3-1:c 2/3-1:d");
    CHECK_EQ(mini("{a b c}%4"), "0-1/4:a 1/4-1/2:b 1/2-3/4:c 3/4-1:a");
}

TEST_CASE("mini: speeding up and slowing down") {
    CHECK_EQ(mini("a*2"), "0-1/2:a 1/2-1:a");
    // A plain number applies directly, so the slowed event isn't split at the cycle boundary.
    CHECK_EQ(mini("a/2", 2), "0-2:a");
    CHECK_EQ(mini("a*<1 2>", 2), "0-1:a 1-3/2:a 3/2-2:a");
}

TEST_CASE("mini: repeats, weights and grouping") {
    CHECK_EQ(mini("a!3 b"), "0-1/4:a 1/4-1/2:a 1/2-3/4:a 3/4-1:b");
    CHECK_EQ(mini("a ! ! b"), mini("a!3 b"));
    CHECK_EQ(mini("a! b"), "0-1/3:a 1/3-2/3:a 2/3-1:b");
    CHECK_EQ(mini("a@3 b"), "0-3/4:a 3/4-1:b");
    CHECK_EQ(mini("a _ _ b"), mini("a@3 b"));
    CHECK_EQ(mini("a b . c d e"), "0-1/4:a 1/4-1/2:b 1/2-2/3:c 2/3-5/6:d 5/6-1:e");
}

TEST_CASE("mini: euclidean rhythms") {
    CHECK_EQ(mini("a(3,8)"), "0-1/8:a 3/8-1/2:a 3/4-7/8:a");
    CHECK_EQ(mini("a(3,8,2)"), "1/8-1/4:a 1/2-5/8:a 3/4-7/8:a");
    CHECK_EQ(mini("a( 3 , 8 )"), mini("a(3,8)"));
}

TEST_CASE("mini: words keep their text") {
    CHECK_EQ(mini("bd:3 0.5 a-b c#4"), "0-1/4:bd:3 1/4-1/2:0.5 1/2-3/4:a-b 3/4-1:c#4");
}

TEST_CASE("mini: a dash is a rest among words but a minus sign in numbers") {
    CHECK_EQ(mini("a - b"), "0-1/3:a 2/3-1:b");
    CHECK_EQ(mini("a -2"), "0-1/3:a 2/3-1:2");
    const auto notes = parseMiniNotes("0 -2 - c e a4").query(0, 1);
    CHECK_EQ(notes.size(), 5u);
    CHECK_EQ(notes[1].value, -2.0);
    CHECK_EQ(notes[4].value, -3.0);
    CHECK_THROWS(parseMiniNotes("0 x"), MiniParseError);
    CHECK_EQ(parseMiniNumbers("0.5 -1").query(0, 1)[1].value, -1.0);
    CHECK_THROWS(parseMiniNumbers("0 c"), MiniParseError);
}

TEST_CASE("mini: errors say where the problem is") {
    CHECK_THROWS(parseMini("a [b"), MiniParseError);
    CHECK_EQ(errorPosition("a]"), 1);
    CHECK_EQ(errorPosition("a b*x"), 4);
    CHECK_EQ(errorPosition("a(3,1.5)"), 4);
    CHECK_EQ(errorPosition("a(3)"), 3);
    CHECK_EQ(errorPosition("_ a"), 0);
    CHECK_THROWS(parseMini("a?"), MiniParseError);
    CHECK_THROWS(parseMini("a | b"), MiniParseError);
    CHECK_THROWS(parseMini("0 .. 3"), MiniParseError);
}

TEST_CASE("note: numbers and note names") {
    CHECK_EQ(*parseNote("0"), 0.0);
    CHECK_EQ(*parseNote("-3.5"), -3.5);
    CHECK_EQ(*parseNote("c"), 0.0);
    CHECK_EQ(*parseNote("c5"), 0.0);
    CHECK_EQ(*parseNote("c6"), 12.0);
    CHECK_EQ(*parseNote("a4"), -3.0);
    CHECK_EQ(*parseNote("fs"), 6.0);
    CHECK_EQ(*parseNote("ef"), 3.0);
    CHECK_EQ(*parseNote("en"), 4.0);
    CHECK(!parseNote("c#"));  // Tidal doesn't accept # for sharp either
    CHECK(!parseNote("x"));
    CHECK(!parseNote("c4x"));
}
