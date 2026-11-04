// Minimal RIFF/WAVE reader (PCM 8/16/24/32, float 32/64, WAVE_FORMAT_EXTENSIBLE) + writer (PCM16).
// Used by the CLI and the tests. The plugin itself uses JUCE's AudioFormatManager (WAV + MP3).
#pragma once
#include "Types.h"
#include <string>

namespace v2i
{
/** Reads a WAV file and mixes it down to mono. Returns false (and sets error) on failure. */
bool readWavFile (const std::string& path, AudioData& out, std::string& error);
bool writeWavFile16 (const std::string& path, const AudioData& in);
}
