// undertow-render: renders a note pattern written in mini-notation to a MIDI file.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "undertow/midi_file.h"
#include "undertow/mini.h"
#include "undertow/pattern.h"

using namespace undertow;

namespace {

constexpr const char* kUsage = R"(Usage: undertow-render [options] NOTES

Renders a pattern of notes, written in mini-notation, to a MIDI file.
Notes are semitones from middle C ("0 3 7") or note names ("c e g", "a4").

Options:
  -o, --output FILE        MIDI file to write (default: out.mid)
  -c, --cycles N           cycles to render (default: 4)
      --bpm N              tempo in beats per minute (default: 120)
      --beats-per-cycle N  beats in one cycle (default: 4)
      --velocity PATTERN   velocities from 0 to 1, in mini-notation (default: 0.8)
      --channel N          MIDI channel, 1-16 (default: 1)
      --print              list the notes instead of writing a file
  -h, --help               show this help
)";

constexpr uint16_t kTicksPerQuarter = 480;

struct Options {
    std::string notes;
    std::string output = "out.mid";
    int64_t cycles = 4;
    double bpm = 120;
    int64_t beatsPerCycle = 4;
    std::string velocity = "0.8";
    int channel = 1;
    bool print = false;
};

struct UsageError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

int64_t parseWhole(const std::string& text, const std::string& option, int64_t min, int64_t max) {
    try {
        size_t used = 0;
        const long long value = std::stoll(text, &used);
        if (used == text.size() && value >= min && value <= max) return value;
    } catch (const std::exception&) {
    }
    throw UsageError(option + " needs a whole number from " + std::to_string(min) + " to " + std::to_string(max));
}

Options parseOptions(int argc, char** argv) {
    Options options;
    bool haveNotes = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&]() -> std::string {
            if (i + 1 >= argc) throw UsageError(arg + " needs a value");
            return argv[++i];
        };
        if (arg == "-h" || arg == "--help") {
            std::cout << kUsage;
            std::exit(0);
        } else if (arg == "-o" || arg == "--output") {
            options.output = value();
        } else if (arg == "-c" || arg == "--cycles") {
            options.cycles = parseWhole(value(), arg, 1, 100000);
        } else if (arg == "--bpm") {
            const std::string text = value();
            try {
                options.bpm = std::stod(text);
            } catch (const std::exception&) {
                options.bpm = 0;
            }
            if (!(options.bpm >= 1 && options.bpm <= 1000)) throw UsageError("--bpm needs a number from 1 to 1000");
        } else if (arg == "--beats-per-cycle") {
            options.beatsPerCycle = parseWhole(value(), arg, 1, 64);
        } else if (arg == "--velocity") {
            options.velocity = value();
        } else if (arg == "--channel") {
            options.channel = static_cast<int>(parseWhole(value(), arg, 1, 16));
        } else if (arg == "--print") {
            options.print = true;
        } else if (!arg.empty() && arg[0] == '-' && arg.size() > 1 && !std::isdigit(static_cast<unsigned char>(arg[1]))) {
            throw UsageError("unknown option " + arg);
        } else if (!haveNotes) {
            options.notes = arg;
            haveNotes = true;
        } else {
            throw UsageError("only one note pattern can be given");
        }
    }
    if (!haveNotes) throw UsageError("no note pattern given");
    return options;
}

struct Note {
    double note;
    double velocity;
};

int run(const Options& options) {
    const auto notes = parseMiniNotes(options.notes);
    const auto velocities = parseMiniNumbers(options.velocity);
    const auto pattern = appLeft(notes, velocities, [](double n, double v) { return Note{n, v}; });

    auto events = pattern.query(0, options.cycles);
    events.erase(std::remove_if(events.begin(), events.end(), [](const Event<Note>& e) { return !e.hasOnset(); }),
                 events.end());
    std::stable_sort(events.begin(), events.end(),
                     [](const Event<Note>& a, const Event<Note>& b) { return a.whole->begin < b.whole->begin; });

    const double ticksPerCycle = static_cast<double>(kTicksPerQuarter) * static_cast<double>(options.beatsPerCycle);
    std::vector<MidiFileNote> midiNotes;
    for (const auto& e : events) {
        const auto key = static_cast<uint8_t>(std::clamp<long>(60 + std::lround(e.value.note), 0, 127));
        const auto velocity = static_cast<uint8_t>(std::clamp<long>(std::lround(e.value.velocity * 127), 1, 127));
        if (options.print) {
            std::cout << e.whole->begin << '\t' << e.whole->end << "\tkey " << int(key) << "\tvelocity " << int(velocity)
                      << '\n';
            continue;
        }
        midiNotes.push_back(MidiFileNote{std::llround(e.whole->begin.toDouble() * ticksPerCycle),
                                         std::llround(e.whole->end.toDouble() * ticksPerCycle),
                                         static_cast<uint8_t>(options.channel - 1), key, velocity});
    }
    if (options.print) return 0;

    const auto bytes = writeMidiFile(midiNotes, kTicksPerQuarter, options.bpm);
    std::ofstream file(options.output, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file) {
        std::cerr << "undertow-render: couldn't write " << options.output << '\n';
        return 1;
    }
    std::cout << "Wrote " << midiNotes.size() << " notes over " << options.cycles << " cycles to " << options.output
              << '\n';
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        return run(parseOptions(argc, argv));
    } catch (const UsageError& e) {
        std::cerr << "undertow-render: " << e.what() << "\n\n" << kUsage;
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "undertow-render: " << e.what() << '\n';
        return 1;
    }
}
