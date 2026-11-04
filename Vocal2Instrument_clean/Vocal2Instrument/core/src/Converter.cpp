#include "v2i/Converter.h"
#include <algorithm>
#include <cmath>

namespace v2i
{
const std::vector<InstrumentPreset>& instrumentPresets()
{
    static const std::vector<InstrumentPreset> presets = {
        { "Piano",      0,   0,  21, 108 },   // Acoustic Grand
        { "Synth Lead", 80,  0,  36,  96 },   // Lead 1 (square)
        { "Synth Pad",  88,  0,  36,  84 },   // Pad 1 (new age)
        { "Strings",    48,  0,  36,  96 },   // String Ensemble 1
        { "Guitar",     25,  0,  40,  88 },   // Acoustic Guitar (steel)
        { "Bass",       33, -24, 28,  60 },   // Electric Bass (finger)
        { "Pluck",      45,  0,  48,  96 },   // Pizzicato Strings
        { "Bell",       14, 12,  60, 108 },   // Tubular Bells
    };
    return presets;
}

double gridBeats (int gridDiv) { return gridDiv > 0 ? 4.0 / gridDiv : 0.0; }

std::vector<MidiNote> convertToMidiNotes (const std::vector<Note>& in, const ConvertParams& p)
{
    const auto& presets = instrumentPresets();
    const auto& inst = presets[(size_t) std::max (0, std::min ((int) presets.size() - 1, p.instrument))];
    const double grid = gridBeats (p.gridDiv);
    const double toBeat = p.bpm / 60.0 / std::max (0.05, p.speed);
    const double minLen = grid > 0 ? grid : 0.03;
    const double str = std::min (1.f, std::max (0.f, p.strength));

    std::vector<MidiNote> out;
    for (const auto& n : in)
    {
        double s = n.startSec * toBeat, e = n.endSec * toBeat;
        if (grid > 0)
        {
            const double sq = std::round (s / grid) * grid, eq = std::round (e / grid) * grid;
            s += (sq - s) * str; e += (eq - e) * str;
        }
        if (e - s < minLen) e = s + minLen;

        int note = n.midi + p.octaveShift * 12 + p.semitoneShift + inst.octaveOffset;
        while (note > inst.highestNote && note - 12 >= 0) note -= 12;
        while (note < inst.lowestNote && note + 12 <= 127) note += 12;
        note = std::max (0, std::min (127, note));
        out.push_back ({ s, e - s, note, n.velocity });
    }

    // monophonic clean-up after quantisation: no overlaps, no duplicate starts
    std::sort (out.begin(), out.end(), [] (const MidiNote& a, const MidiNote& b) { return a.startBeat < b.startBeat; });
    std::vector<MidiNote> clean;
    for (auto& m : out)
    {
        if (! clean.empty())
        {
            auto& last = clean.back();
            if (m.startBeat - last.startBeat < 1e-6)
            {
                if (m.lengthBeat > last.lengthBeat) last = m;
                continue;
            }
            if (m.startBeat < last.startBeat + last.lengthBeat) last.lengthBeat = m.startBeat - last.startBeat;
        }
        clean.push_back (m);
    }
    return clean;
}
}
