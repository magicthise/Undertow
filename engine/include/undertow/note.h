#pragma once

#include <optional>
#include <string>

namespace undertow {

// Reads a note the way Tidal writes them: semitones from middle C ("0", "-3",
// "7.5"), or a note name with optional sharps (s), flats (f), naturals (n) and
// octave, where octave 5 is the middle one: "c" and "c5" are 0, "fs" is 6,
// "ef4" is -9.
std::optional<double> parseNote(const std::string& word);

}  // namespace undertow
