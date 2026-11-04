#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Theme.h"
#include "ui/WaveformView.h"
#include "ui/PianoRollView.h"

/** Button that, when dragged, hands a temporary .mid file to the host / OS (drop it into FL Studio's piano roll or playlist). */
class MidiDragHandle : public juce::Component
{
public:
    std::function<juce::File()> makeFile;   // returns an existing .mid file or File() on failure
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (theme::panelHi); g.fillRoundedRectangle (r, 6.f);
        g.setColour (theme::green); g.drawRoundedRectangle (r, 6.f, 1.2f);
        g.setColour (theme::green); g.setFont (juce::Font (juce::FontOptions (13.f, juce::Font::bold)));
        g.drawText (juce::String::fromUTF8 ("\xe2\x87\xa9  DRAG MIDI"), getLocalBounds(), juce::Justification::centred);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() < 6 || dragging) return;
        auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
        if (container == nullptr || ! makeFile) return;
        const auto f = makeFile();
        if (f == juce::File()) return;
        dragging = true;
        container->performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragging = false; });
    }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
    void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
private:
    bool dragging = false;
};

class Vocal2InstrumentEditor : public juce::AudioProcessorEditor,
                               public juce::DragAndDropContainer,
                               public juce::FileDragAndDropTarget,
                               private juce::ChangeListener,
                               private juce::Timer
{
public:
    explicit Vocal2InstrumentEditor (Vocal2InstrumentProcessor&);
    ~Vocal2InstrumentEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    struct Labelled
    {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void chooseFile();
    void loadFile (const juce::File&);
    void doAnalyze();
    void doConvert();
    void doExport();
    juce::File makeTempMidi();
    void setStatus (const juce::String& s, bool warn = false) { status = s; statusWarn = warn; repaint (statusArea); }
    void addSlider (Labelled& l, const juce::String& text, const juce::String& paramId, const juce::String& suffix = {});

    Vocal2InstrumentProcessor& proc;
    theme::LookAndFeel laf;

    WaveformView waveform;
    PianoRollView roll;
    MidiDragHandle dragHandle;
    juce::TextButton loadBtn { "LOAD" }, recBtn { "REC" }, analyzeBtn { "ANALYZE" }, convertBtn { "CONVERT TO MIDI" }, exportBtn { "EXPORT MIDI" }, hostBpmBtn { "HOST" };
    Labelled sens, bpm, speed, octave;
    juce::Label quantLabel, instLabel;
    juce::ComboBox quantBox, instBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> quantAttach, instAttach;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::String status = "Load a vocal recording (WAV / MP3) to begin.";
    bool statusWarn = false, wasAnalysing = false;
    juce::Rectangle<int> statusArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Vocal2InstrumentEditor)
};
