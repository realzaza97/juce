// Synthetic-voice regression tests for the core. Builds vocal-like signals with known ground truth
// (harmonic source, vibrato, glides, breath noise, background noise, level dips) and measures
// note-level precision / recall (same MIDI note, onset within 70 ms).
#include "v2i/Converter.h"
#include "v2i/MidiFile.h"
#include "v2i/NoteSegmenter.h"
#include "v2i/PitchTracker.h"
#include "v2i/WavReader.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>

using namespace v2i;
static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FAIL: %s\n", msg); ++failures; } } while (0)

struct TNote { double start, dur; int midi; double dipDb = 0; };   // dipDb: level dip in the middle (for repeated notes)

struct SynthOpts
{
    double sr = 44100, vibDepthSt = 0.5, vibRate = 5.5, noiseDb = -55, level = 0.3, glide = 0.04, breathDb = -30;
    bool breaths = true; unsigned seed = 1;
};

static AudioData synth (const std::vector<TNote>& notes, double total, const SynthOpts& o)
{
    std::mt19937 rng (o.seed); std::normal_distribution<double> nd (0, 1);
    AudioData a; a.sampleRate = o.sr; a.samples.assign ((size_t) (total * o.sr), 0.f);
    const double TP = 2 * 3.14159265358979323846;
    double phase = 0, vph = 0; double curSt = 60; bool active = false;
    const size_t N = a.samples.size();
    // formant-ish lowpass: harmonics amplitude 1/h with a bump near 800 Hz
    for (size_t i = 0; i < N; ++i)
    {
        const double t = (double) i / o.sr;
        const TNote* cur = nullptr;
        for (auto& n : notes) if (t >= n.start && t < n.start + n.dur) { cur = &n; break; }
        double s = 0;
        if (cur)
        {
            const double tn = t - cur->start;
            double target = cur->midi;
            if (! active) { curSt = target; active = true; }
            curSt += (target - curSt) * (1.0 - std::exp (-1.0 / (o.sr * o.glide * 0.35)));   // portamento
            const double vibEnv = std::min (1.0, std::max (0.0, (tn - 0.12) / 0.15));
            vph += TP * o.vibRate / o.sr;
            const double st = curSt + o.vibDepthSt * vibEnv * std::sin (vph);
            const double f0 = 440.0 * std::pow (2.0, (st - 69.0) / 12.0);
            phase += TP * f0 / o.sr;
            double env = std::min (1.0, tn / 0.02) * std::min (1.0, (cur->dur - tn) / 0.03);
            if (cur->dipDb > 0) { const double x = (tn - cur->dur / 2) / 0.04; env *= std::pow (10.0, -cur->dipDb * std::exp (-x * x) / 20.0); }
            env *= 1.0 + 0.1 * std::sin (vph + 1.0);   // tremolo that accompanies vibrato
            for (int h = 1; h <= 14 && h * f0 < o.sr / 2.2; ++h)
            {
                const double fh = h * f0, bump = 1.0 + 1.5 * std::exp (-std::pow ((fh - 800) / 400, 2));
                s += bump / h * std::sin (h * phase);
            }
            s *= env * o.level * 0.25;
        }
        else { active = false; }
        s += nd (rng) * std::pow (10.0, o.noiseDb / 20.0);
        a.samples[i] = (float) s;
    }
    if (o.breaths)   // breath bursts in the gaps: band-limited-ish noise
    {
        double lp = 0;
        for (size_t i = 0; i < N; ++i)
        {
            const double t = (double) i / o.sr;
            bool inNote = false; for (auto& n : notes) if (t >= n.start - 0.02 && t < n.start + n.dur + 0.02) inNote = true;
            lp += 0.5 * (nd (rng) - lp);
            if (! inNote && std::fmod (t, 1.7) < 0.25) a.samples[i] += (float) (lp * std::pow (10.0, o.breathDb / 20.0));
        }
    }
    return a;
}

struct Score { int tp = 0, nTruth = 0, nDet = 0; double onsetErrMs = 0; };

static Score score (const std::vector<TNote>& truth, const std::vector<Note>& det, double tolSec = 0.07, bool verbose = false)
{
    Score s; s.nTruth = (int) truth.size(); s.nDet = (int) det.size();
    std::vector<char> used (det.size(), 0);
    for (auto& t : truth)
        for (size_t i = 0; i < det.size(); ++i)
            if (! used[i] && det[i].midi == t.midi && std::abs (det[i].startSec - t.start) < tolSec)
            { used[i] = 1; ++s.tp; s.onsetErrMs += std::abs (det[i].startSec - t.start) * 1000; break; }
    if (verbose || s.tp != s.nTruth || s.nDet != s.nTruth)
    {
        std::printf ("    truth:"); for (auto& t : truth) std::printf (" %d@%.2f", t.midi, t.start);
        std::printf ("\n    found:"); for (auto& d : det) std::printf (" %d@%.2f(%.1f)", d.midi, d.startSec, d.pitch);
        std::printf ("\n");
    }
    return s;
}

static void runCase (const char* name, const std::vector<TNote>& truth, double total, SynthOpts o, float sens = 0.5f, double minRecall = 1.0, int maxExtra = 0)
{
    auto audio = synth (truth, total, o);
    AnalysisParams p; p.sensitivity = sens;
    auto r = analyze (audio, p);
    Score s = score (truth, r.notes);
    const int extra = s.nDet - s.tp;
    std::printf ("[%s] truth %d  found %d  correct %d  extra %d  mean onset err %.1f ms\n", name, s.nTruth, s.nDet, s.tp, extra, s.tp ? s.onsetErrMs / s.tp : 0.0);
    CHECK ((double) s.tp / s.nTruth >= minRecall - 1e-9, "recall below target");
    CHECK (extra <= maxExtra, "too many false notes");
}

int main()
{
    // 1. female melody, strong vibrato (+-0.5 st), gaps, breaths
    {
        std::vector<TNote> t = { { 0.30, 0.55, 72 }, { 0.95, 0.50, 74 }, { 1.55, 0.60, 76 }, { 2.25, 0.45, 77 }, { 2.80, 0.80, 79 }, { 3.70, 0.40, 76 }, { 4.20, 0.70, 72 } };
        runCase ("female, vibrato 0.5st", t, 5.2, SynthOpts());
        SynthOpts o; o.vibDepthSt = 0.0; runCase ("female, no vibrato", t, 5.2, o);
        if (std::getenv ("V2I_DUMP")) writeWavFile16 ("/tmp/v2i_female_novib.wav", synth (t, 5.2, o));
        SynthOpts o2; o2.vibDepthSt = 0.8; o2.vibRate = 6.2; runCase ("female, extreme vibrato 0.8st", t, 5.2, o2);
    }
    // 2. male low voice, legato steps (no gaps), noisy
    {
        std::vector<TNote> t = { { 0.20, 0.50, 45 }, { 0.70, 0.50, 48 }, { 1.20, 0.50, 52 }, { 1.70, 0.50, 50 }, { 2.20, 0.60, 47 } };
        SynthOpts o; o.noiseDb = -42; o.sr = 48000; runCase ("male low, legato, noisy 48k", t, 3.2, o);
    }
    // 3. same note repeated without silence (energy re-articulation)
    {
        std::vector<TNote> t = { { 0.20, 0.30, 67 }, { 0.50, 0.30, 67, 0 }, { 0.80, 0.30, 67, 0 } };
        // build with real level dips between notes: three adjacent notes with short envelopes already dip at the joins
        runCase ("repeated notes (ta-ta-ta)", t, 1.4, SynthOpts());
    }
    // 4. fast short notes (100 ms)
    {
        std::vector<TNote> t = { { 0.2, 0.12, 64 }, { 0.34, 0.12, 67 }, { 0.48, 0.12, 71 }, { 0.62, 0.12, 69 }, { 0.76, 0.12, 66 } };
        SynthOpts o; o.vibDepthSt = 0.2; runCase ("fast runs 120ms", t, 1.3, o, 0.7f, 0.8, 1);
    }
    // 5. quiet phone recording
    {
        std::vector<TNote> t = { { 0.3, 0.5, 60 }, { 0.9, 0.5, 62 }, { 1.5, 0.5, 64 }, { 2.1, 0.7, 60 } };
        SynthOpts o; o.level = 0.01; o.noiseDb = -62; o.sr = 22050; o.breathDb = -52; runCase ("quiet phone, 22k", t, 3.1, o);
    }
    // 6. silence / noise only -> nothing
    {
        std::vector<TNote> t;
        SynthOpts o; o.noiseDb = -35; auto a = synth (t, 3.0, o);
        auto r = analyze (a, AnalysisParams());
        std::printf ("[noise only] found %zu notes\n", r.notes.size());
        CHECK (r.notes.empty(), "noise produced notes");
    }
    // 7. WAV round trip
    {
        std::vector<TNote> t = { { 0.1, 0.4, 69 } };
        auto a = synth (t, 0.7, SynthOpts());
        CHECK (writeWavFile16 ("/tmp/v2i_test.wav", a), "wav write");
        AudioData b; std::string err;
        CHECK (readWavFile ("/tmp/v2i_test.wav", b, err), "wav read");
        CHECK (b.samples.size() == a.samples.size() && std::abs (b.sampleRate - a.sampleRate) < 1, "wav size");
        std::printf ("[wav round trip] done\n");
    }
    // 8. quantise + MIDI round trip
    {
        std::vector<Note> in = { { 0.02, 0.48, 60, 60.f, 100 }, { 0.51, 0.99, 62, 62.f, 90 }, { 1.03, 1.45, 64, 64.f, 80 } };
        ConvertParams cp; cp.bpm = 120; cp.gridDiv = 8; cp.instrument = 5; cp.octaveShift = 1;   // bass: -24 +12
        auto m = convertToMidiNotes (in, cp);
        CHECK (m.size() == 3, "note count");
        CHECK (std::abs (m[0].startBeat - 0.0) < 1e-9 && std::abs (m[1].startBeat - 1.0) < 1e-9 && std::abs (m[2].startBeat - 2.0) < 1e-9, "quantised starts");
        CHECK (m[0].note == 60 - 12 && m[1].note == 62 - 12, "bass transposition");
        auto bytes = writeMidiFile (m, 120, instrumentPresets()[5].gmProgram);
        ParsedMidi pm; CHECK (parseMidiFile (bytes, pm), "midi parse");
        CHECK (pm.ppq == 480 && std::abs (pm.bpm - 120) < 0.01 && pm.program == 33, "midi header/tempo/program");
        CHECK (pm.notes.size() == 3 && pm.notes[1].note == 50 && std::abs (pm.notes[1].startBeat - 1.0) < 1e-9, "midi notes round trip");
        std::printf ("[midi/quantise] %zu bytes\n", bytes.size());
    }
    std::printf (failures ? "\n%d FAILURE(S)\n" : "\nALL TESTS PASSED\n", failures);
    return failures ? 1 : 0;
}
