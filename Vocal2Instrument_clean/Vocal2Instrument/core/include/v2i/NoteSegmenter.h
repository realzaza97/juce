// Turns a pitch track into discrete notes.
//  1. bridge tiny unvoiced gaps, 2. median filter (kills octave glitches),
//  3. moving average over ~1 vibrato cycle (separates vibrato from real note changes),
//  4. optimal piecewise-constant segmentation (dynamic programming, penalised least squares),
//  5. split repeated same-pitch notes on energy dips, 6. drop short / low-confidence notes.
#pragma once
#include "Types.h"
#include <atomic>

namespace v2i
{
class NoteSegmenter
{
public:
    static std::vector<Note> segment (const std::vector<PitchFrame>& frames, const AnalysisParams& params);
};

/** Convenience: tracker + segmenter. `offsetSec` is added to all times. */
AnalysisResult analyze (const AudioData& audio, const AnalysisParams& params,
                        double offsetSec = 0.0,
                        std::atomic<float>* progress = nullptr,
                        const std::atomic<bool>* cancel = nullptr);
}
