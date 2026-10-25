#pragma once

#include <cstdint>
#include <vector>

namespace undertow {

struct MidiFileNote {
    int64_t startTick;
    int64_t endTick;
    uint8_t channel;   // 0-15
    uint8_t key;       // 0-127
    uint8_t velocity;  // 1-127
};

// Builds a single-track Standard MIDI File (format 0) holding the notes and a
// tempo. Notes may be in any order.
std::vector<uint8_t> writeMidiFile(const std::vector<MidiFileNote>& notes, uint16_t ticksPerQuarter, double bpm);

}  // namespace undertow
