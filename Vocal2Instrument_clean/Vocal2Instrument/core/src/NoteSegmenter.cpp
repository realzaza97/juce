#include <atomic>
#include "v2i/NoteSegmenter.h"
#include "v2i/PitchTracker.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace v2i
{
namespace
{
double median (std::vector<double> v)
{
    if (v.empty()) return 0.0;
    const size_t m = v.size() / 2;
    std::nth_element (v.begin(), v.begin() + (long) m, v.end());
    double r = v[m];
    if (v.size() % 2 == 0) r = 0.5 * (r + *std::max_element (v.begin(), v.begin() + (long) m));
    return r;
}

/** Optimal piecewise-constant fit of y[0..n) minimising SSE + beta * (#segments), segment length in [minSeg, maxSeg].
    Returns segment start indices (always begins with 0). */
std::vector<int> piecewiseConstant (const std::vector<double>& y, double beta, int minSeg, int maxSeg)
{
    const int n = (int) y.size();
    if (n < 2 * minSeg) return { 0 };
    std::vector<double> s1 ((size_t) n + 1, 0.0), s2 ((size_t) n + 1, 0.0);
    for (int i = 0; i < n; ++i) { s1[(size_t) i + 1] = s1[(size_t) i] + y[(size_t) i]; s2[(size_t) i + 1] = s2[(size_t) i] + y[(size_t) i] * y[(size_t) i]; }
    auto sse = [&] (int a, int b) { const double len = b - a, m = s1[(size_t) b] - s1[(size_t) a]; return (s2[(size_t) b] - s2[(size_t) a]) - m * m / len; };
    const double inf = std::numeric_limits<double>::infinity();
    std::vector<double> dp ((size_t) n + 1, inf);
    std::vector<int> prev ((size_t) n + 1, 0);
    dp[0] = 0.0;
    for (int j = minSeg; j <= n; ++j)
        for (int i = std::max (0, j - maxSeg); i <= j - minSeg; ++i)
        {
            if (dp[(size_t) i] == inf) continue;
            const double c = dp[(size_t) i] + sse (i, j) + beta;
            if (c < dp[(size_t) j]) { dp[(size_t) j] = c; prev[(size_t) j] = i; }
        }
    if (dp[(size_t) n] == inf) return { 0 };
    std::vector<int> starts;
    for (int j = n; j > 0; j = prev[(size_t) j]) starts.push_back (prev[(size_t) j]);
    std::reverse (starts.begin(), starts.end());
    return starts;
}

struct Seg { int a, b; };   // frame range [a,b)

/** Recursively split [a,b) at deep energy valleys (re-articulation of the same pitch). */
void splitOnEnergy (const std::vector<double>& env, int a, int b, int minFrames, int win, double depthDb, std::vector<Seg>& out)
{
    int bestM = -1; double bestDepth = depthDb;
    for (int m = a + minFrames; m <= b - minFrames; ++m)
    {
        if (! (env[(size_t) m] <= env[(size_t) m - 1] && env[(size_t) m] <= env[(size_t) m + 1])) continue;
        double lmax = -1e9, rmax = -1e9;
        for (int k = std::max (a, m - win); k <= m; ++k) lmax = std::max (lmax, env[(size_t) k]);
        for (int k = m; k < std::min (b, m + win + 1); ++k) rmax = std::max (rmax, env[(size_t) k]);
        const double depth = std::min (lmax, rmax) - env[(size_t) m];
        if (depth >= bestDepth) { bestDepth = depth; bestM = m; }
    }
    if (bestM < 0) { out.push_back ({ a, b }); return; }
    splitOnEnergy (env, a, bestM, minFrames, win, depthDb, out);
    splitOnEnergy (env, bestM, b, minFrames, win, depthDb, out);
}
} // namespace

std::vector<Note> NoteSegmenter::segment (const std::vector<PitchFrame>& frames, const AnalysisParams& p)
{
    std::vector<Note> notes;
    const int n = (int) frames.size();
    if (n < 4) return notes;
    const double hop = frames[1].timeSec - frames[0].timeSec;
    const double sens = std::min (1.f, std::max (0.f, p.sensitivity));

    const double minNoteMs   = p.minNoteMs > 0 ? p.minNoteMs : 110.0 - 70.0 * sens;
    const int    minNoteFr   = std::max (3, (int) std::ceil (minNoteMs / 1000.0 / hop));
    const int    bridgeFr    = std::max (1, (int) std::lround (0.045 / hop));
    const int    medianW     = 2 * std::max (1, (int) std::lround (0.020 / hop)) + 1;      // ~40 ms
    const int    smoothW     = 2 * std::max (1, (int) std::lround (0.060 / hop)) + 1;      // ~120 ms
    const double beta        = 2.4 - 1.6 * sens;                                            // semitone² * frames per extra note
    const double reartDb     = 9.0 - 5.0 * sens;
    const int    reartWin    = std::max (4, (int) std::lround (0.150 / hop));
    const double minConf     = 0.30 - 0.15 * sens;

    // --- 1. pitch array with small gaps bridged
    std::vector<double> pitch ((size_t) n, 0.0);
    std::vector<char> voiced ((size_t) n, 0);
    for (int i = 0; i < n; ++i) if (frames[(size_t) i].voiced) { voiced[(size_t) i] = 1; pitch[(size_t) i] = frames[(size_t) i].midi; }
    for (int i = 1; i < n; )
    {
        if (! voiced[(size_t) i] && voiced[(size_t) i - 1])
        {
            int j = i; while (j < n && ! voiced[(size_t) j]) ++j;
            if (j < n && j - i <= bridgeFr && std::abs (pitch[(size_t) j] - pitch[(size_t) i - 1]) < 1.0)
                for (int k = i; k < j; ++k)
                {
                    const double t = (double) (k - i + 1) / (j - i + 1);
                    pitch[(size_t) k] = pitch[(size_t) i - 1] * (1 - t) + pitch[(size_t) j] * t; voiced[(size_t) k] = 1;
                }
            i = j;
        }
        else ++i;
    }

    // smoothed energy envelope (3 frames)
    std::vector<double> env ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        double a = 0; int c = 0;
        for (int k = std::max (0, i - 1); k <= std::min (n - 1, i + 1); ++k) { a += frames[(size_t) k].rmsDb; ++c; }
        env[(size_t) i] = a / c;
    }

    std::vector<Seg> segs;
    std::vector<double> medP ((size_t) n, 0.0);

    // --- 2..5 per voiced run
    for (int a = 0; a < n; )
    {
        if (! voiced[(size_t) a]) { ++a; continue; }
        int b = a; while (b < n && voiced[(size_t) b]) ++b;
        const int len = b - a;
        if (len >= 2)
        {
            // median filter
            std::vector<double> med ((size_t) len), sm ((size_t) len);
            const int hw = medianW / 2;
            for (int i = 0; i < len; ++i)
            {
                std::vector<double> w;
                for (int k = std::max (0, i - hw); k <= std::min (len - 1, i + hw); ++k) w.push_back (pitch[(size_t) (a + k)]);
                med[(size_t) i] = median (w);
                medP[(size_t) (a + i)] = med[(size_t) i];
            }
            // vibrato removal: a ~100 ms median keeps note steps sharp (a moving average would create ramps
            // that the segmenter would turn into false 'passing' notes) while flattening +-0.5 st vibrato.
            const int sw = smoothW / 2;
            for (int i = 0; i < len; ++i)
            {
                std::vector<double> w;
                for (int k = std::max (0, i - sw); k <= std::min (len - 1, i + sw); ++k) w.push_back (med[(size_t) k]);
                sm[(size_t) i] = median (w);
            }
            const auto starts = piecewiseConstant (sm, beta, std::max (3, minNoteFr / 2), 600);
            std::vector<Seg> local;
            for (size_t s = 0; s < starts.size(); ++s)
                local.push_back ({ a + starts[s], a + (s + 1 < starts.size() ? starts[s + 1] : len) });

            // merge neighbours that round to the same note
            auto segPitch = [&] (const Seg& g)
            {
                std::vector<double> v; const int L = g.b - g.a, m = L > 8 ? L * 15 / 100 : 0;
                for (int k = g.a + m; k < g.b - m; ++k) v.push_back (medP[(size_t) k]);
                return median (v);
            };
            // short scoops / drifts at the start or end of a phrase are not notes: fold them into the neighbour
            const int edgeFr = std::max (3, (int) std::lround ((0.11 - 0.05 * sens) / hop));
            for (bool changed = true; changed && local.size() > 1; )
            {
                changed = false;
                if (local.front().b - local.front().a < edgeFr && std::abs (segPitch (local[0]) - segPitch (local[1])) < 1.5)
                { local[1].a = local[0].a; local.erase (local.begin()); changed = true; }
                else if (local.size() > 1 && local.back().b - local.back().a < edgeFr
                         && std::abs (segPitch (local.back()) - segPitch (local[local.size() - 2])) < 1.5)
                { local[local.size() - 2].b = local.back().b; local.pop_back(); changed = true; }
            }
            std::vector<Seg> merged;
            for (auto& g : local)
            {
                if (! merged.empty() && std::lround (segPitch (merged.back())) == std::lround (segPitch (g))) merged.back().b = g.b;
                else merged.push_back (g);
            }
            // energy-based re-articulation
            for (auto& g : merged) splitOnEnergy (env, g.a, g.b, minNoteFr, reartWin, reartDb, segs);
        }
        a = b;
    }

    // --- 6. build notes, filter
    std::vector<double> noteDb;
    for (auto& g : segs)
    {
        const int L = g.b - g.a;
        if (L < minNoteFr) continue;
        double conf = 0, db = 0; int cn = 0;
        for (int k = g.a; k < g.b; ++k) { if (frames[(size_t) k].voiced) { conf += frames[(size_t) k].confidence; ++cn; } db += frames[(size_t) k].rmsDb; }
        if (cn < L / 2 || (cn ? conf / cn : 0.0) < minConf) continue;

        std::vector<double> v; const int m = L > 8 ? L * 15 / 100 : 0;
        for (int k = g.a + m; k < g.b - m; ++k) v.push_back (medP[(size_t) k]);
        const double pm = median (v);
        Note nt;
        nt.startSec = std::max (0.0, frames[(size_t) g.a].timeSec - hop / 2);
        nt.endSec = frames[(size_t) g.b - 1].timeSec + hop / 2;
        nt.pitch = (float) pm;
        nt.midi = (int) std::lround (pm);
        if (nt.midi < 0 || nt.midi > 127) continue;
        notes.push_back (nt);
        noteDb.push_back (db / L);
    }
    if (! notes.empty())
    {
        auto sortedDb = noteDb; std::sort (sortedDb.begin(), sortedDb.end());
        const double ref = sortedDb[(size_t) ((sortedDb.size() - 1) * 0.9)];
        for (size_t i = 0; i < notes.size(); ++i)
            notes[i].velocity = (int) std::lround (std::min (127.0, std::max (35.0, 100.0 + (noteDb[i] - ref) * 2.5)));
    }
    return notes;
}

AnalysisResult analyze (const AudioData& audio, const AnalysisParams& params, double offsetSec,
                        std::atomic<float>* progress, const std::atomic<bool>* cancel)
{
    AnalysisResult r;
    r.frames = PitchTracker::track (audio, params, progress, cancel);
    if (cancel && cancel->load()) return {};
    r.notes = NoteSegmenter::segment (r.frames, params);
    for (auto& f : r.frames) f.timeSec += offsetSec;
    for (auto& n : r.notes) { n.startSec += offsetSec; n.endSec += offsetSec; }
    if (progress) progress->store (1.f);
    return r;
}
}
