#include "undertow/midi_file.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>

namespace undertow {

namespace {

struct TrackEvent {
    int64_t tick;
    // Note-offs sort before note-ons at the same tick, so a repeated note
    // isn't cut off by the end of the one before it.
    int order;
    std::vector<uint8_t> bytes;
};

void appendBigEndian(std::vector<uint8_t>& out, uint32_t value, int bytes) {
    for (int shift = (bytes - 1) * 8; shift >= 0; shift -= 8) out.push_back(static_cast<uint8_t>(value >> shift));
}

// MIDI's variable-length quantity: 7 bits per byte, high bit set on all but the last.
void appendVariableLength(std::vector<uint8_t>& out, uint32_t value) {
    uint8_t buffer[5];
    int count = 0;
    do {
        buffer[count++] = static_cast<uint8_t>(value & 0x7F);
        value >>= 7;
    } while (value != 0);
    while (count-- > 0) out.push_back(static_cast<uint8_t>(buffer[count] | (count > 0 ? 0x80 : 0x00)));
}

}  // namespace

std::vector<uint8_t> writeMidiFile(const std::vector<MidiFileNote>& notes, uint16_t ticksPerQuarter, double bpm) {
    if (ticksPerQuarter == 0 || ticksPerQuarter > 0x7FFF) throw std::invalid_argument("ticks per quarter must be 1-32767");
    if (!(bpm > 0)) throw std::invalid_argument("bpm must be positive");

    std::vector<TrackEvent> events;
    const auto microsPerQuarter = static_cast<uint32_t>(std::lround(60'000'000.0 / bpm));
    if (microsPerQuarter == 0 || microsPerQuarter > 0xFFFFFF) throw std::invalid_argument("bpm out of range");
    std::vector<uint8_t> tempo{0xFF, 0x51, 0x03};
    appendBigEndian(tempo, microsPerQuarter, 3);
    events.push_back({0, 0, tempo});

    for (const auto& note : notes) {
        if (note.startTick < 0 || note.endTick < note.startTick) throw std::invalid_argument("note with invalid timing");
        const auto channel = static_cast<uint8_t>(note.channel & 0x0F);
        const auto key = static_cast<uint8_t>(note.key & 0x7F);
        const auto velocity = static_cast<uint8_t>(std::clamp<int>(note.velocity, 1, 127));
        events.push_back({note.startTick, 2, {static_cast<uint8_t>(0x90 | channel), key, velocity}});
        events.push_back({note.endTick, 1, {static_cast<uint8_t>(0x80 | channel), key, 0}});
    }
    std::stable_sort(events.begin(), events.end(), [](const TrackEvent& a, const TrackEvent& b) {
        return std::tie(a.tick, a.order) < std::tie(b.tick, b.order);
    });

    std::vector<uint8_t> track;
    int64_t previousTick = 0;
    for (const auto& event : events) {
        const int64_t delta = event.tick - previousTick;
        if (delta > 0x0FFFFFFF) throw std::invalid_argument("gap between notes too long for a MIDI file");
        appendVariableLength(track, static_cast<uint32_t>(delta));
        track.insert(track.end(), event.bytes.begin(), event.bytes.end());
        previousTick = event.tick;
    }
    appendVariableLength(track, 0);
    track.insert(track.end(), {0xFF, 0x2F, 0x00});  // end of track

    std::vector<uint8_t> file{'M', 'T', 'h', 'd'};
    appendBigEndian(file, 6, 4);
    appendBigEndian(file, 0, 2);  // format 0: a single track
    appendBigEndian(file, 1, 2);  // one track
    appendBigEndian(file, ticksPerQuarter, 2);
    file.insert(file.end(), {'M', 'T', 'r', 'k'});
    appendBigEndian(file, static_cast<uint32_t>(track.size()), 4);
    file.insert(file.end(), track.begin(), track.end());
    return file;
}

}  // namespace undertow
