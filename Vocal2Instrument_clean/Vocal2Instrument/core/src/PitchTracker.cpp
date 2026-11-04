#include "v2i/PitchTracker.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace v2i
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kWorkRate = 16000.0;   // analysis sample rate (voice F0 << 8 kHz)
constexpr int    kHop = 128;            // 8 ms
constexpr int    kWin = 480;            // 30 ms integration window
constexpr int    kMaxCands = 6;

struct Cand { float tau; float d; float pen = 0.f; };

/** 2nd-order Butterworth high-pass (removes rumble / DC / handling noise). */
void highPass (std::vector<float>& x, double sr, double fc)
{
    const double w0 = 2.0 * kPi * fc / sr, c = std::cos (w0), s = std::sin (w0);
    const double alpha = s / (2.0 * 0.70710678);
    const double b0 = (1 + c) / 2, b1 = -(1 + c), b2 = (1 + c) / 2, a0 = 1 + alpha, a1 = -2 * c, a2 = 1 - alpha;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    for (auto& v : x)
    {
        const double in = v;
        const double out = (b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0;
        x2 = x1; x1 = in; y2 = y1; y1 = out;
        v = (float) out;
    }
}
} // namespace

std::vector<float> PitchTracker::resample (const std::vector<float>& x, double srIn, double srOut)
{
    if (std::abs (srIn - srOut) < 1.0) return x;
    const double ratio = srIn / srOut;
    const double fc = std::min (1.0, 1.0 / ratio) * 0.92;      // cutoff relative to input Nyquist
    const double hw = 12.0 / fc;                                // 12 zero crossings each side
    constexpr int R = 256;                                      // table resolution (entries per input sample)
    const int tableN = (int) std::ceil (2 * hw * R) + 2;
    std::vector<float> table ((size_t) tableN);
    for (int i = 0; i < tableN; ++i)
    {
        const double t = (double) i / R - hw;
        const double u = t / hw;
        const double sinc = std::abs (t) < 1e-9 ? 1.0 : std::sin (kPi * fc * t) / (kPi * fc * t);
        const double win = (std::abs (u) >= 1.0) ? 0.0 : 0.42 + 0.5 * std::cos (kPi * u) + 0.08 * std::cos (2 * kPi * u);
        table[(size_t) i] = (float) (fc * sinc * win);
    }
    const size_t outN = (size_t) std::floor ((double) x.size() / ratio);
    std::vector<float> y (outN);
    const long n = (long) x.size();
    for (size_t j = 0; j < outN; ++j)
    {
        const double p = (double) j * ratio;
        const long k0 = std::max (0L, (long) std::ceil (p - hw)), k1 = std::min (n - 1, (long) std::floor (p + hw));
        double acc = 0;
        for (long k = k0; k <= k1; ++k)
        {
            const double idx = ((double) k - p + hw) * R;
            const int i0 = (int) idx; const double fr = idx - i0;
            if (i0 + 1 >= tableN) continue;
            acc += x[(size_t) k] * (table[(size_t) i0] * (1.0 - fr) + table[(size_t) i0 + 1] * fr);
        }
        y[j] = (float) acc;
    }
    return y;
}

std::vector<PitchFrame> PitchTracker::track (const AudioData& audio, const AnalysisParams& params,
                                             std::atomic<float>* progress, const std::atomic<bool>* cancel)
{
    std::vector<PitchFrame> frames;
    if (audio.samples.size() < (size_t) (audio.sampleRate * 0.1)) return frames;

    std::vector<float> x = resample (audio.samples, audio.sampleRate, kWorkRate);
    highPass (x, kWorkRate, 60.0);
    const long n = (long) x.size();

    const double sens = std::min (1.f, std::max (0.f, params.sensitivity));
    const int tauMin = std::max (2, (int) std::floor (kWorkRate / params.fMaxHz));
    const int tauMax = std::min (kWin - 2, (int) std::ceil (kWorkRate / params.fMinHz));
    const int regionLen = kWin + tauMax + 1;

    // prefix sums of squares for fast window energies
    std::vector<double> csum ((size_t) n + 1, 0.0);
    for (long i = 0; i < n; ++i) csum[(size_t) i + 1] = csum[(size_t) i] + (double) x[(size_t) i] * x[(size_t) i];
    auto energy = [&] (long a, long b) { a = std::max (0L, std::min (n, a)); b = std::max (0L, std::min (n, b)); return b > a ? csum[(size_t) b] - csum[(size_t) a] : 0.0; };

    const long numFrames = std::max (0L, (n - kHop) / kHop);
    frames.resize ((size_t) numFrames);
    std::vector<std::vector<Cand>> cands ((size_t) numFrames);

    // ---- pass 1: levels
    std::vector<float> levelDb ((size_t) numFrames);
    for (long i = 0; i < numFrames; ++i)
    {
        const long c = i * kHop + kHop / 2;
        const double e = energy (c - kWin / 2, c + kWin / 2) / kWin;
        levelDb[(size_t) i] = (float) (10.0 * std::log10 (e + 1e-12));
        frames[(size_t) i].timeSec = (double) c / kWorkRate;
        frames[(size_t) i].rmsDb = levelDb[(size_t) i];
    }
    // loudness gate relative to the loud parts of this recording (works for quiet phone recordings too)
    std::vector<float> sorted (levelDb);
    std::sort (sorted.begin(), sorted.end());
    const float refDb = sorted.empty() ? -60.f : sorted[(size_t) ((sorted.size() - 1) * 0.95)];
    const float gateDb = std::max (-62.f, refDb - (float) (18.0 + 27.0 * sens));   // sens 0 -> -18 dB, 1 -> -45 dB

    // ---- pass 2: YIN candidates
    std::vector<float> buf ((size_t) regionLen), diff ((size_t) tauMax + 2), cmnd ((size_t) tauMax + 2);
    for (long i = 0; i < numFrames; ++i)
    {
        if (cancel && cancel->load()) return {};
        if (progress && (i & 63) == 0) progress->store (0.9f * (float) i / (float) numFrames);
        if (levelDb[(size_t) i] < gateDb) continue;

        const long c = i * kHop + kHop / 2;
        const long s0 = c - regionLen / 2;
        for (int j = 0; j < regionLen; ++j) { const long k = s0 + j; buf[(size_t) j] = (k >= 0 && k < n) ? x[(size_t) k] : 0.f; }

        const double e0 = energy (s0, s0 + kWin);
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double cross = 0;
            const float* a = buf.data(); const float* b = buf.data() + tau;
            for (int j = 0; j < kWin; ++j) cross += (double) a[j] * b[j];
            const double et = energy (s0 + tau, s0 + tau + kWin);
            diff[(size_t) tau] = (float) std::max (0.0, e0 + et - 2.0 * cross);
        }
        cmnd[0] = 1.f; double run = 0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            run += diff[(size_t) tau];
            cmnd[(size_t) tau] = run > 1e-12 ? (float) (diff[(size_t) tau] * tau / run) : 1.f;
        }
        auto& cl = cands[(size_t) i];
        for (int tau = std::max (tauMin, 2); tau < tauMax; ++tau)
        {
            const float a = cmnd[(size_t) tau - 1], b = cmnd[(size_t) tau], cc = cmnd[(size_t) tau + 1];
            if (b < a && b <= cc && b < 0.55f)
            {
                const float den = a - 2 * b + cc;
                const float off = std::abs (den) > 1e-9f ? 0.5f * (a - cc) / den : 0.f;
                cl.push_back ({ (float) tau + off, std::max (0.f, b - 0.25f * (a - cc) * off) });
            }
        }
        // Keep the good minima, shortest periods first. (On clean periodic signals every multiple of the period
        // has CMNDF ~ 0, so sorting by depth alone could prune the true fundamental.)
        if (! cl.empty())
        {
            float best = 1.f; for (auto& q : cl) best = std::min (best, q.d);
            cl.erase (std::remove_if (cl.begin(), cl.end(), [best] (const Cand& q) { return q.d > best + 0.25f; }), cl.end());
            std::sort (cl.begin(), cl.end(), [] (const Cand& p, const Cand& q) { return p.tau < q.tau; });
            if ((int) cl.size() > kMaxCands) cl.resize (kMaxCands);
        }
        // sub-harmonic guard: a candidate whose period is ~2x/3x/4x another good candidate's is probably an octave/twelfth error
        for (auto& c1 : cl)
            for (auto& c2 : cl)
            {
                if (&c1 == &c2 || c2.d >= c1.d + 0.25f) continue;
                for (int k = 2; k <= 4; ++k)
                    if (std::abs (c2.tau * (float) k - c1.tau) < 0.05f * c1.tau) { c1.pen = 0.30f; break; }
                if (c1.pen > 0.f) break;
            }
    }

    // ---- pass 3: Viterbi over candidates + unvoiced state
    const double unvoicedCost = 0.18 + 0.30 * sens;
    constexpr double kSemiCost = 0.05, kSemiCap = 12.0, kVuSwitch = 0.20, kLowBias = 0.03;
    const double inf = std::numeric_limits<double>::infinity();
    std::vector<std::vector<double>> cost ((size_t) numFrames);
    std::vector<std::vector<int>> back ((size_t) numFrames);
    auto midiOf = [] (float tau) { return 69.0 + 12.0 * std::log2 ((kWorkRate / tau) / 440.0); };

    for (long i = 0; i < numFrames; ++i)
    {
        const auto& cl = cands[(size_t) i];
        const size_t ns = cl.size() + 1;                 // last = unvoiced
        cost[(size_t) i].assign (ns, inf);
        back[(size_t) i].assign (ns, 0);
        for (size_t s = 0; s < ns; ++s)
        {
            const bool unv = (s == cl.size());
            const double obs = unv ? unvoicedCost : cl[s].d + cl[s].pen + kLowBias * cl[s].tau / tauMax;
            if (i == 0) { cost[0][s] = obs; continue; }
            const auto& pc = cands[(size_t) i - 1];
            double best = inf; int bi = 0;
            for (size_t p = 0; p < pc.size() + 1; ++p)
            {
                const double pv = cost[(size_t) i - 1][p];
                if (pv == inf) continue;
                const bool punv = (p == pc.size());
                double tr;
                if (unv && punv) tr = 0.0;
                else if (unv != punv) tr = kVuSwitch;
                else tr = kSemiCost * std::min (kSemiCap, std::abs (midiOf (cl[s].tau) - midiOf (pc[p].tau)));
                if (pv + tr < best) { best = pv + tr; bi = (int) p; }
            }
            cost[(size_t) i][s] = best + obs; back[(size_t) i][s] = bi;
        }
    }
    if (numFrames > 0)
    {
        size_t s = 0; double best = inf;
        for (size_t k = 0; k < cost.back().size(); ++k) if (cost.back()[k] < best) { best = cost.back()[k]; s = k; }
        for (long i = numFrames - 1; i >= 0; --i)
        {
            const auto& cl = cands[(size_t) i];
            auto& fr = frames[(size_t) i];
            if (s < cl.size())
            {
                fr.voiced = true;
                fr.f0Hz = (float) (kWorkRate / cl[s].tau);
                fr.midi = (float) midiOf (cl[s].tau);
                fr.confidence = std::max (0.f, std::min (1.f, 1.f - cl[s].d));
            }
            s = (size_t) back[(size_t) i][s];
        }
    }
    if (progress) progress->store (0.95f);
    return frames;
}
}
