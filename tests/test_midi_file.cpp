#include <cstdint>
#include <vector>

#include "check.h"
#include "undertow/midi_file.h"

using undertow::MidiFileNote;
using undertow::writeMidiFile;

TEST_CASE("midi file: one note at 120 bpm") {
    const auto bytes = writeMidiFile({MidiFileNote{0, 480, 0, 60, 100}}, 480, 120);
    const std::vector<uint8_t> expected{
        'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0x01, 0xE0,  // header: format 0, 1 track, 480 ticks
        'M', 'T', 'r', 'k', 0, 0, 0, 20,                         // track of 20 bytes
        0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20,                // tempo: 500000 us per quarter
        0x00, 0x90, 60, 100,                                     // note on
        0x83, 0x60, 0x80, 60, 0,                                 // 480 ticks later, note off
        0x00, 0xFF, 0x2F, 0x00,                                  // end of track
    };
    CHECK(bytes == expected);
}

TEST_CASE("midi file: a note ending where the next starts is released first") {
    const auto bytes = writeMidiFile({MidiFileNote{480, 960, 0, 60, 100}, MidiFileNote{0, 480, 0, 60, 100}}, 480, 120);
    // After the tempo and the first note on: note off, then note on, at the same tick.
    const std::vector<uint8_t> middle{0x83, 0x60, 0x80, 60, 0, 0x00, 0x90, 60, 100};
    CHECK(std::vector<uint8_t>(bytes.begin() + 33, bytes.begin() + 42) == middle);
}
