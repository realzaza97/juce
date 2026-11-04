// Vocal2Instrument core - shared data types (no JUCE dependency).
#pragma once
#include <vector>

namespace v2i
{
/** Mono audio buffer. */
struct AudioData
{
    std::vector<float> samples;
    double sampleRate = 44100.0;
    double durationSec() const { return sampleRate > 0 ? (double) samples.size() / sampleRate : 0.0; }
};

/** One analysis frame of the pitch tracker. */
struct PitchFrame
{
    double timeSec = 0.0;    // frame centre, seconds
    float  f0Hz = 0.f;       // 0 when unvoiced
    float  midi = 0.f;       // fractional MIDI pitch (valid when voiced)
    float  confidence = 0.f; // 1 - CMNDF value, 0..1
    float  rmsDb = -120.f;   // frame level, dBFS
    bool   voiced = false;
};

/** A note detected in the audio (time in seconds). */
struct Note
{
    double startSec = 0.0;
    double endSec = 0.0;
    int    midi = 60;      // rounded MIDI note
    float  pitch = 60.f;   // fractional (median) pitch, shows how far from the grid the singer was
    int    velocity = 90;
};

/** A note ready for MIDI / piano roll (time in beats = quarter notes). */
struct MidiNote
{
    double startBeat = 0.0;
    double lengthBeat = 0.25;
    int    note = 60;
    int    velocity = 90;
};

struct AnalysisParams
{
    /** 0..1. Higher = detects quieter / shorter / subtler notes (and more false positives). */
    float sensitivity = 0.5f;
    /** <=0 => derived from sensitivity (110 ms ... 40 ms). */
    float minNoteMs = -1.f;
    float fMinHz = 65.f;    // ~C2
    float fMaxHz = 1100.f;  // ~C#6
};

struct AnalysisResult
{
    std::vector<PitchFrame> frames;
    std::vector<Note> notes;
};
} // namespace v2i
