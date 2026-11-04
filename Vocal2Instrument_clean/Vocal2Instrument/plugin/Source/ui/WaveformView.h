#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "v2i/Types.h"

/** Min/max waveform with drag-to-select region (double-click clears the selection). */
class WaveformView : public juce::Component
{
public:
    void setAudio (const v2i::AudioData* a) { audio = a; rebuildPeaks(); repaint(); }
    void clearSelection() { selStart = selEnd = 0; repaint(); }
    /** Returns false when nothing is selected. */
    bool getSelection (double& t0, double& t1) const;
    void setPlaceholder (const juce::String& s) { placeholder = s; repaint(); }
    std::function<void()> onLoadRequested;

    void paint (juce::Graphics&) override;
    void resized() override { rebuildPeaks(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override { if (audio == nullptr || audio->samples.empty()) { if (onLoadRequested) onLoadRequested(); } else clearSelection(); }

private:
    double xToTime (float x) const;
    void rebuildPeaks();

    const v2i::AudioData* audio = nullptr;
    std::vector<std::pair<float, float>> peaks;   // per pixel column
    double selStart = 0, selEnd = 0;
    juce::String placeholder = "Drop a WAV / MP3 here  -  or double-click to load";
};
