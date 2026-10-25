#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

#include "undertow/pattern.h"

namespace undertow {

class MiniParseError : public std::runtime_error {
public:
    MiniParseError(const std::string& message, size_t position);
    // Character offset into the source where the problem was found.
    size_t position() const { return position_; }

private:
    size_t position_;
};

// Parses mini-notation, the rhythm language inside Tidal's quoted strings
// ("bd*2 [sn cp] <a b>"), into a pattern of the words it contains, such as
// sound names. As in Tidal, "-" is a rest here, like "~".
//
// Supported: sequences, ~ and - rests, [ ] groups, < > alternation, { }
// polymeter with %steps, , layering, * and / (with patterned amounts), ! and
// bare ! repeats, @ weights, _ elongation, . grouping, and
// (pulses,steps,rotation) Euclidean rhythms. Not yet: ? and | randomness,
// .. ranges, chords.
Pattern<std::string> parseMini(const std::string& source);

// Parses mini-notation holding notes: semitones from middle C ("0 -2 7") or
// note names ("c e g", "a4"); see parseNote.
Pattern<double> parseMiniNotes(const std::string& source);

// Parses mini-notation holding plain numbers ("0.8 1 -0.5").
Pattern<double> parseMiniNumbers(const std::string& source);

}  // namespace undertow
