// Monophonic pitch tracker: YIN (de Cheveigné & Kawahara 2002) candidates + Viterbi decoding
// over (candidates ∪ unvoiced) states, in the spirit of pYIN (Mauch & Dixon 2014).
#pragma once
#include "Types.h"
#include <atomic>

namespace v2i
{
class PitchTracker
{
public:
    /** Returns one frame every ~8 ms. `progress` (0..1) and `cancel` are optional. */
    static std::vector<PitchFrame> track (const AudioData& audio, const AnalysisParams& params,
                                          std::atomic<float>* progress = nullptr,
                                          const std::atomic<bool>* cancel = nullptr);

    /** Band-limited resampler (windowed sinc). Exposed for tests. */
    static std::vector<float> resample (const std::vector<float>& x, double srIn, double srOut);
};
}
