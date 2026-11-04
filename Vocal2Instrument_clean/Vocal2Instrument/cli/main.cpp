// v2i_cli: offline vocal -> MIDI converter. Handy for batch tests and for judging detection quality
// without opening a DAW.   v2i_cli in.wav out.mid [--bpm 120] [--quant 16] [--sens 0.5] [--inst piano] [--oct 0] [--verbose]
#include "v2i/Converter.h"
#include "v2i/MidiFile.h"
#include "v2i/NoteSegmenter.h"
#include "v2i/WavReader.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

static const char* noteName (int m)
{
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static char buf[16]; std::snprintf (buf, sizeof buf, "%s%d", n[m % 12], m / 12 - 1); return buf;
}

int main (int argc, char** argv)
{
    if (argc < 3) { std::fprintf (stderr, "usage: %s in.wav out.mid [--bpm N] [--quant 0|4|8|16|32] [--sens 0..1] [--inst name] [--oct N] [--verbose]\n", argv[0]); return 1; }
    v2i::AnalysisParams ap; v2i::ConvertParams cp; bool verbose = false, dumpFrames = false;
    for (int i = 3; i < argc; ++i)
    {
        auto is = [&] (const char* k) { return std::strcmp (argv[i], k) == 0 && i + 1 < argc; };
        if (is ("--bpm")) cp.bpm = std::atof (argv[++i]);
        else if (is ("--quant")) cp.gridDiv = std::atoi (argv[++i]);
        else if (is ("--sens")) ap.sensitivity = (float) std::atof (argv[++i]);
        else if (is ("--oct")) cp.octaveShift = std::atoi (argv[++i]);
        else if (is ("--inst"))
        {
            const char* want = argv[++i]; const auto& ps = v2i::instrumentPresets();
            for (size_t k = 0; k < ps.size(); ++k) { std::string a = ps[k].name, b = want; for (auto& c : a) c = (char) std::tolower (c); for (auto& c : b) c = (char) std::tolower (c); if (a.find (b) != std::string::npos) cp.instrument = (int) k; }
        }
        else if (std::strcmp (argv[i], "--verbose") == 0) verbose = true;
        else if (std::strcmp (argv[i], "--frames") == 0) dumpFrames = true;
    }
    v2i::AudioData audio; std::string err;
    if (! v2i::readWavFile (argv[1], audio, err)) { std::fprintf (stderr, "error: %s\n", err.c_str()); return 2; }
    auto res = v2i::analyze (audio, ap);
    auto notes = v2i::convertToMidiNotes (res.notes, cp);
    if (dumpFrames)
        for (auto& f : res.frames) std::printf ("%.3f %s midi %.2f conf %.2f rms %.1f\n", f.timeSec, f.voiced ? "V" : "-", f.midi, f.confidence, f.rmsDb);
    if (verbose)
        for (auto& n : res.notes) std::printf ("%7.3f-%7.3f s  %-4s (%.2f)  vel %d\n", n.startSec, n.endSec, noteName (n.midi), n.pitch, n.velocity);
    const auto& inst = v2i::instrumentPresets()[(size_t) cp.instrument];
    if (! v2i::saveMidiFile (argv[2], v2i::writeMidiFile (notes, cp.bpm, inst.gmProgram))) { std::fprintf (stderr, "cannot write %s\n", argv[2]); return 3; }
    std::printf ("%zu notes -> %s (%s, %.0f BPM)\n", notes.size(), argv[2], inst.name, cp.bpm);
    return 0;
}
