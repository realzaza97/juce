#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class Vocal2InstrumentProcessor;

/**
 * Piano-roll editor for the converted notes.
 *  - double-click empty area : add a note        - drag note body : move (snaps to the quantise grid)
 *  - drag right edge         : resize            - right-click / Delete : remove
 *  - wheel : scroll time      Shift+wheel : scroll pitch      Ctrl/Cmd+wheel : zoom
 * The dim cyan line is the raw detected pitch curve, so you can judge the conversion at a glance.
 */
class PianoRollView : public juce::Component
{
public:
    explicit PianoRollView (Vocal2InstrumentProcessor& p) : proc (p) { setWantsKeyboardFocus (true); }

    void fitView();   // zoom/scroll so every note is visible

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    enum class Drag { none, move, resize };

    float beatToX (double b) const { return keyW + (float) ((b - scrollBeat) * pxPerBeat); }
    double xToBeat (float x) const { return (x - keyW) / pxPerBeat + scrollBeat; }
    float noteToY (int n) const { return rulerH + (float) (topNote - n) * rowH; }
    int yToNote (float y) const { return topNote - (int) std::floor ((y - rulerH) / rowH); }
    double gridStep() const;
    double snap (double beat) const;
    int hitTest (juce::Point<float> p) const;

    Vocal2InstrumentProcessor& proc;
    double pxPerBeat = 80.0, scrollBeat = 0.0;
    int topNote = 84;
    float rowH = 15.f;
    const float keyW = 54.f, rulerH = 22.f;
    int selected = -1;
    Drag drag = Drag::none;
    double origStart = 0, origLen = 0; int origNote = 0;
};
