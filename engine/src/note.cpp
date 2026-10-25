#include "undertow/note.h"

#include <cctype>

#include "undertow/rational.h"

namespace undertow {

std::optional<double> parseNote(const std::string& word) {
    try {
        return Rational::parse(word).toDouble();
    } catch (const std::exception&) {
        // Not a number; try a note name.
    }

    static constexpr int kPitchClasses[] = {9, 11, 0, 2, 4, 5, 7};  // a b c d e f g
    if (word.empty() || word[0] < 'a' || word[0] > 'g') return std::nullopt;
    int semitones = kPitchClasses[word[0] - 'a'];

    size_t i = 1;
    for (; i < word.size(); ++i) {
        const char c = word[i];
        if (c == 's') {
            ++semitones;
        } else if (c == 'f') {
            --semitones;
        } else if (c != 'n') {
            break;
        }
    }

    int octave = 5;
    if (i < word.size()) {
        bool negative = false;
        if (word[i] == '-') {
            negative = true;
            ++i;
        }
        if (i == word.size()) return std::nullopt;
        octave = 0;
        for (; i < word.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(word[i]))) return std::nullopt;
            octave = octave * 10 + (word[i] - '0');
            if (octave > 100) return std::nullopt;
        }
        if (negative) octave = -octave;
    }
    return static_cast<double>(semitones + (octave - 5) * 12);
}

}  // namespace undertow
