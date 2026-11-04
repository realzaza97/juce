// Note list -> MIDI-ready notes (beats): time mapping, quantisation, transposition, instrument range.
#pragma once
#include "Types.h"
#include <string>

namespace v2i
{
struct InstrumentPreset
{
    const char* name;
    int gmProgram;      // General MIDI program (0-based). Only meaningful to GM synths; VSTs ignore it.
    int octaveOffset;   // semitones added so the vocal lands in the instrument's natural register
    int lowestNote;     // notes are folded by octaves into [lowestNote, highestNote]
    int highestNote;
};

const std::vector<InstrumentPreset>& instrumentPresets();   // Piano, Synth Lead, Synth Pad, Strings, Guitar, Bass, Pluck, Bell

struct ConvertParams
{
    double bpm = 120.0;
    double speed = 1.0;        // >1 = result plays faster (positions are divided by speed)
    int    gridDiv = 16;       // 0 = off, 4 = 1/4, 8 = 1/8, 16 = 1/16, 32 = 1/32
    float  strength = 1.f;     // 0..1 quantise amount
    int    octaveShift = 0;    // user transposition in octaves
    int    semitoneShift = 0;
    int    instrument = 0;     // index into instrumentPresets()
};

std::vector<MidiNote> convertToMidiNotes (const std::vector<Note>& notes, const ConvertParams& p);
double gridBeats (int gridDiv);
}
