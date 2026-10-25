#include "undertow/mini.h"

#include "undertow/note.h"

#include <cctype>
#include <utility>
#include <vector>

namespace undertow {

MiniParseError::MiniParseError(const std::string& message, size_t position)
    : std::runtime_error(message + " (at character " + std::to_string(position + 1) + ")"), position_(position) {}

namespace {

using Text = std::string;
using TextPattern = Pattern<Text>;

struct Step {
    TextPattern pattern;
    Rational weight = 1;
};
using Sequence = std::vector<Step>;

// What the words being read will be used as. This changes how some text
// reads, as in Tidal: in Text, "-" is always a rest, while elsewhere "-2" is
// a negative number. Values are checked as they're read, so mistakes are
// reported where they are.
enum class WordKind { Text, Note, Number, Integer };

bool isDigit(char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }
bool isAlnum(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0; }
bool isSpace(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

Rational totalWeight(const Sequence& steps) {
    Rational total = 0;
    for (const auto& step : steps) total += step.weight;
    return total;
}

TextPattern sequencePattern(const Sequence& steps) {
    if (steps.empty()) return silence<Text>();
    if (steps.size() == 1) return steps.front().pattern;
    std::vector<std::pair<Rational, TextPattern>> weighted;
    weighted.reserve(steps.size());
    for (const auto& step : steps) weighted.emplace_back(step.weight, step.pattern);
    return timecat(weighted);
}

// Layers that each play `stepsPerCycle` of their own steps per cycle, so
// layers of different lengths drift against each other. < > is the case of
// one step per cycle.
TextPattern polymeter(const std::vector<Sequence>& layers, const Rational& stepsPerCycle) {
    std::vector<TextPattern> pats;
    for (const auto& layer : layers) {
        const Rational count = totalWeight(layer);
        if (count == 0) continue;
        pats.push_back(fast(stepsPerCycle / count, sequencePattern(layer)));
    }
    if (pats.empty()) return silence<Text>();
    return pats.size() == 1 ? pats.front() : stack(std::move(pats));
}

Pattern<Rational> toNumbers(const TextPattern& pat) {
    return fmap(pat, [](const Text& word) { return Rational::parse(word); });
}

Pattern<int64_t> toIntegers(const TextPattern& pat) {
    return fmap(pat, [](const Text& word) { return Rational::parse(word).num(); });
}

class Parser {
public:
    Parser(const std::string& source, WordKind kind) : src_(source), kind_(kind) {}

    TextPattern parse() {
        TextPattern pat = parseLayers('\0');
        skipSpace();
        if (!atEnd()) fail(std::string("unexpected '") + peek() + "'");
        return pat;
    }

private:
    // Sets the kind of word expected for the lifetime of the scope.
    class KindScope {
    public:
        KindScope(Parser& parser, WordKind kind) : parser_(parser), saved_(parser.kind_) { parser.kind_ = kind; }
        ~KindScope() { parser_.kind_ = saved_; }
        KindScope(const KindScope&) = delete;
        KindScope& operator=(const KindScope&) = delete;

    private:
        Parser& parser_;
        WordKind saved_;
    };

    [[noreturn]] void fail(const std::string& message) const { fail(message, pos_); }
    [[noreturn]] void fail(const std::string& message, size_t at) const { throw MiniParseError(message, at); }

    bool atEnd() const { return pos_ >= src_.size(); }
    char peek(size_t ahead = 0) const { return pos_ + ahead < src_.size() ? src_[pos_ + ahead] : '\0'; }
    void skipSpace() {
        while (!atEnd() && isSpace(peek())) ++pos_;
    }
    bool consume(char c) {
        skipSpace();
        if (atEnd() || peek() != c) return false;
        ++pos_;
        return true;
    }
    void expect(char c) {
        if (!consume(c)) fail(std::string("expected '") + c + "'");
    }

    // Sequences separated by ',' play at the same time. Used at the top level and in [ ].
    TextPattern parseLayers(char closing) {
        std::vector<TextPattern> layers{sequencePattern(parseSequence(closing))};
        while (consume(',')) layers.push_back(sequencePattern(parseSequence(closing)));
        return layers.size() == 1 ? layers.front() : stack(std::move(layers));
    }

    std::vector<Sequence> parseSequenceList(char closing) {
        std::vector<Sequence> list{parseSequence(closing)};
        while (consume(',')) list.push_back(parseSequence(closing));
        return list;
    }

    // Steps up to a ',' or the closing bracket. A lone '.' splits the steps
    // into groups that each take one step.
    Sequence parseSequence(char closing) {
        std::vector<Sequence> groups(1);
        while (true) {
            skipSpace();
            const char c = peek();
            if (atEnd() || c == closing || c == ',') break;
            if (c == ']' || c == '>' || c == '}' || c == ')') fail(std::string("unexpected '") + c + "'");
            if (c == '.' && peek(1) == '.') fail("ranges with '..' aren't supported yet");
            if (c == '|') fail("random choice with '|' isn't supported yet");
            if (c == '.') {
                ++pos_;
                groups.emplace_back();
                continue;
            }
            Sequence& steps = groups.back();
            if (c == '_') {
                if (steps.empty()) fail("'_' needs a step before it to lengthen");
                ++pos_;
                steps.back().weight += 1;
                continue;
            }
            if (c == '!') {
                if (steps.empty()) fail("'!' needs a step before it to repeat");
                ++pos_;
                steps.push_back(steps.back());
                continue;
            }
            Sequence parsed = parseStep();
            steps.insert(steps.end(), parsed.begin(), parsed.end());
        }
        if (groups.size() == 1) return std::move(groups.front());
        Sequence grouped;
        for (const auto& group : groups) {
            if (!group.empty()) grouped.push_back(Step{sequencePattern(group), 1});
        }
        return grouped;
    }

    // A term followed by any modifiers written directly after it.
    Sequence parseStep() {
        Step step{parseTerm(), 1};
        size_t copies = 1;
        while (true) {
            const char c = peek();
            if (c == '*') {
                ++pos_;
                step.pattern = fast(parseAmount(), step.pattern);
            } else if (c == '/') {
                ++pos_;
                step.pattern = slow(parseAmount(), step.pattern);
            } else if (c == '@') {
                ++pos_;
                step.weight = parseLiteralNumber();
            } else if (c == '!') {
                ++pos_;
                copies = isDigit(peek()) ? parseCount() : copies + 1;
            } else if (c == '(') {
                ++pos_;
                step.pattern = parseEuclid(step.pattern);
            } else if (c == '?') {
                fail("random removal with '?' isn't supported yet");
            } else {
                break;
            }
        }
        return Sequence(copies, step);
    }

    TextPattern parseTerm() {
        const char c = peek();
        if (c == '[') {
            ++pos_;
            TextPattern inner = parseLayers(']');
            expect(']');
            return inner;
        }
        if (c == '<') {
            ++pos_;
            auto layers = parseSequenceList('>');
            expect('>');
            return polymeter(layers, 1);
        }
        if (c == '{') {
            ++pos_;
            auto layers = parseSequenceList('}');
            expect('}');
            if (peek() == '%') {
                ++pos_;
                return patternify(parseAmount(), [layers](const Rational& steps) { return polymeter(layers, steps); });
            }
            return polymeter(layers, layers.empty() ? Rational(0) : totalWeight(layers.front()));
        }
        if (c == '~' || (c == '-' && !isWordStart())) {
            ++pos_;
            return silence<Text>();
        }
        if (isWordStart()) {
            const size_t start = pos_;
            return pure(checkWord(readWord(), start));
        }
        if (atEnd()) fail("expected a step but reached the end");
        fail(std::string("unexpected '") + c + "'");
    }

    // The amount after * or / or %: a number, or a bracketed pattern of numbers.
    Pattern<Rational> parseAmount() {
        KindScope scope(*this, WordKind::Number);
        return toNumbers(parseTerm());
    }

    TextPattern parseEuclid(const TextPattern& pat) {
        Pattern<int64_t> pulses;
        Pattern<int64_t> steps;
        Pattern<int64_t> rotation = pure<int64_t>(0);
        {
            KindScope scope(*this, WordKind::Integer);
            pulses = parseEuclidArgument();
            expect(',');
            steps = parseEuclidArgument();
            if (consume(',')) rotation = parseEuclidArgument();
        }
        expect(')');
        return euclid(pulses, steps, rotation, pat);
    }

    Pattern<int64_t> parseEuclidArgument() {
        Sequence argument = parseSequence(')');
        if (argument.empty()) fail("expected a number");
        return toIntegers(sequencePattern(argument));
    }

    Rational parseLiteralNumber() {
        const size_t start = pos_;
        if (!isWordStart()) fail("expected a number");
        const Text word = readWord();
        try {
            return Rational::parse(word);
        } catch (const std::exception&) {
            fail("expected a number, found '" + word + "'", start);
        }
    }

    size_t parseCount() {
        const size_t start = pos_;
        size_t count = 0;
        while (isDigit(peek())) {
            count = count * 10 + static_cast<size_t>(peek() - '0');
            if (count > 4096) fail("too many repeats", start);
            ++pos_;
        }
        if (count == 0) fail("repeat count must be at least 1", start);
        return count;
    }

    bool isWordStart() const {
        const char c = peek();
        if (isAlnum(c)) return true;
        return c == '-' && kind_ != WordKind::Text && isDigit(peek(1));
    }

    Text readWord() {
        const size_t start = pos_;
        ++pos_;
        while (!atEnd()) {
            const char c = peek();
            if (isAlnum(c) || c == '#' || c == ':' || c == '\'' || c == '_' || c == '-') {
                ++pos_;
            } else if (c == '.' && isAlnum(peek(1))) {
                ++pos_;
            } else {
                break;
            }
        }
        return src_.substr(start, pos_ - start);
    }

    Text checkWord(const Text& word, size_t start) const {
        if (kind_ == WordKind::Text) return word;
        if (kind_ == WordKind::Note) {
            if (!parseNote(word)) fail("expected a note, found '" + word + "'", start);
            return word;
        }
        Rational value;
        try {
            value = Rational::parse(word);
        } catch (const std::exception&) {
            fail("expected a number, found '" + word + "'", start);
        }
        if (kind_ == WordKind::Integer && value.den() != 1) fail("expected a whole number, found '" + word + "'", start);
        return word;
    }

    const std::string& src_;
    size_t pos_ = 0;
    WordKind kind_;
};

}  // namespace

Pattern<std::string> parseMini(const std::string& source) { return Parser(source, WordKind::Text).parse(); }

Pattern<double> parseMiniNotes(const std::string& source) {
    // Every word was checked while parsing, so parseNote can't fail here.
    return fmap(Parser(source, WordKind::Note).parse(), [](const Text& word) { return *parseNote(word); });
}

Pattern<double> parseMiniNumbers(const std::string& source) {
    return fmap(Parser(source, WordKind::Number).parse(),
                [](const Text& word) { return Rational::parse(word).toDouble(); });
}

}  // namespace undertow
