// Standard MIDI File writer (format 0, 480 PPQ) and a small parser used by the tests.
#pragma once
#include "Types.h"
#include <cstdint>
#include <string>

namespace v2i
{
constexpr int kMidiPpq = 480;

std::vector<uint8_t> writeMidiFile (const std::vector<MidiNote>& notes, double bpm, int gmProgram,
                                    const std::string& trackName = "Vocal2Instrument", int channel = 0);
bool saveMidiFile (const std::string& path, const std::vector<uint8_t>& bytes);

struct ParsedMidi { int ppq = 0; double bpm = 0; int program = -1; std::string trackName; std::vector<MidiNote> notes; };
bool parseMidiFile (const std::vector<uint8_t>& bytes, ParsedMidi& out);
}
