#include "PluginEditor.h"

Vocal2InstrumentEditor::Vocal2InstrumentEditor (Vocal2InstrumentProcessor& p)
    : AudioProcessorEditor (&p), proc (p), roll (p)
{
    setLookAndFeel (&laf);
    setResizable (true, true);
    setResizeLimits (820, 560, 1800, 1100);
    setSize (980, 700);

    addAndMakeVisible (waveform);
    addAndMakeVisible (roll);
    addAndMakeVisible (dragHandle);
    for (auto* b : { &loadBtn, &recBtn, &analyzeBtn, &convertBtn, &exportBtn, &hostBpmBtn }) addAndMakeVisible (b);

    analyzeBtn.setColour (juce::TextButton::buttonColourId, theme::purple.darker (0.2f));
    convertBtn.setColour (juce::TextButton::buttonColourId, theme::blue.darker (0.5f));
    exportBtn.setColour (juce::TextButton::buttonColourId, theme::green.darker (0.6f));
    recBtn.setEnabled (false);
    recBtn.setTooltip ("Microphone recording: planned for the next milestone");
    hostBpmBtn.setTooltip ("Use the host's current tempo");

    loadBtn.onClick = [this] { chooseFile(); };
    analyzeBtn.onClick = [this] { doAnalyze(); };
    convertBtn.onClick = [this] { doConvert(); };
    exportBtn.onClick = [this] { doExport(); };
    hostBpmBtn.onClick = [this]
    {
        const double h = proc.getHostBpm();
        if (h > 20.0) { if (auto* prm = proc.apvts.getParameter ("bpm")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) h)); }
        else setStatus ("The host did not report a tempo.", true);
    };
    waveform.onLoadRequested = [this] { chooseFile(); };
    dragHandle.makeFile = [this] { return makeTempMidi(); };

    addSlider (sens, "SENSITIVITY", "sens");
    addSlider (bpm, "BPM", "bpm");
    addSlider (speed, "SPEED", "speed", "x");
    addSlider (octave, "OCTAVE", "octave");

    for (auto* l : { &quantLabel, &instLabel }) { l->setFont (juce::Font (juce::FontOptions (11.f, juce::Font::bold))); addAndMakeVisible (l); }
    quantLabel.setText ("QUANTIZE", juce::dontSendNotification);
    instLabel.setText ("INSTRUMENT", juce::dontSendNotification);
    for (auto* c : { &quantBox, &instBox }) addAndMakeVisible (c);
    quantBox.addItemList ({ "Off", "1/4", "1/8", "1/16", "1/32" }, 1);
    for (auto& ip : v2i::instrumentPresets()) instBox.addItem (ip.name, instBox.getNumItems() + 1);
    quantAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "quant", quantBox);
    instAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "inst", instBox);

    waveform.setAudio (&proc.getAudio());
    proc.addChangeListener (this);
    startTimerHz (15);
}

Vocal2InstrumentEditor::~Vocal2InstrumentEditor()
{
    stopTimer();
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void Vocal2InstrumentEditor::addSlider (Labelled& l, const juce::String& text, const juce::String& id, const juce::String& suffix)
{
    l.label.setText (text, juce::dontSendNotification);
    l.label.setFont (juce::Font (juce::FontOptions (11.f, juce::Font::bold)));
    l.slider.setSliderStyle (juce::Slider::LinearHorizontal);
    l.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
    l.slider.setTextValueSuffix (suffix);
    addAndMakeVisible (l.label); addAndMakeVisible (l.slider);
    l.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, l.slider);
}

// ------------------------------------------------------------------ actions
void Vocal2InstrumentEditor::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a vocal recording", juce::File(), "*.wav;*.mp3;*.aiff;*.flac;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) loadFile (fc.getResult()); });
}

void Vocal2InstrumentEditor::loadFile (const juce::File& f)
{
    juce::String err;
    if (! proc.loadAudioFile (f, err)) { setStatus (err, true); return; }
    waveform.setAudio (&proc.getAudio());
    waveform.clearSelection();
    setStatus (f.getFileName() + "  -  " + juce::String (proc.getAudio().durationSec(), 1) + " s, "
               + juce::String (proc.getAudio().sampleRate / 1000.0, 1) + " kHz. Drag on the waveform to pick a region, then press ANALYZE.");
}

void Vocal2InstrumentEditor::doAnalyze()
{
    if (proc.getAudio().samples.empty()) { setStatus ("Load an audio file first.", true); return; }
    double a = 0, b = 0;
    if (! waveform.getSelection (a, b)) a = b = 0;
    proc.startAnalysis (a, b);
    setStatus ("Analysing...");
}

void Vocal2InstrumentEditor::doConvert()
{
    if (proc.getRawNotes().empty()) { setStatus ("Nothing to convert yet. Run ANALYZE first.", true); return; }
    proc.convertRawNotes();
    roll.fitView();
    setStatus (juce::String ((int) proc.editableNotes().size()) + " MIDI notes (manual edits were replaced).");
}

juce::File Vocal2InstrumentEditor::makeTempMidi()
{
    const auto stem = juce::File (proc.getAudioName()).getFileNameWithoutExtension();
    const auto base = stem.isEmpty() ? juce::String ("Vocal2Instrument") : stem;
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (base + "_v2i.mid");
    const auto r = proc.exportMidi (f);
    if (r.failed()) { setStatus (r.getErrorMessage(), true); return {}; }
    return f;
}

void Vocal2InstrumentEditor::doExport()
{
    if (proc.editableNotes().empty()) { setStatus ("There are no notes to export. Run ANALYZE first.", true); return; }
    auto def = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile (juce::File (proc.getAudioName()).getFileNameWithoutExtension() + ".mid");
    chooser = std::make_unique<juce::FileChooser> ("Export MIDI", def, "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              f = f.withFileExtension ("mid");
                              const auto r = proc.exportMidi (f);
                              setStatus (r.wasOk() ? "Exported " + f.getFullPathName() : r.getErrorMessage(), r.failed());
                          });
}

// ------------------------------------------------------------------ callbacks
void Vocal2InstrumentEditor::changeListenerCallback (juce::ChangeBroadcaster*) { roll.repaint(); }

void Vocal2InstrumentEditor::timerCallback()
{
    const bool now = proc.isAnalysing();
    analyzeBtn.setEnabled (! now);
    if (now) setStatus ("Analysing...  " + juce::String (juce::roundToInt (proc.getProgress() * 100.f)) + " %");
    if (wasAnalysing && ! now)
    {
        roll.fitView();
        const auto n = proc.editableNotes().size();
        setStatus (n == 0 ? juce::String ("No notes found. Try raising SENSITIVITY, or check that the recording is a single voice.")
                          : juce::String ((int) n) + " notes detected. Edit in the piano roll, then drag the MIDI into FL Studio or EXPORT.", n == 0);
    }
    wasAnalysing = now;
}

bool Vocal2InstrumentEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (juce::File (f).hasFileExtension ("wav;mp3;aiff;aif;flac;ogg")) return true;
    return false;
}
void Vocal2InstrumentEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files) if (juce::File (f).hasFileExtension ("wav;mp3;aiff;aif;flac;ogg")) { loadFile (juce::File (f)); return; }
}

// ------------------------------------------------------------------ layout
void Vocal2InstrumentEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
    g.setColour (theme::text); g.setFont (juce::Font (juce::FontOptions (20.f, juce::Font::bold)));
    g.drawText ("VOCAL", 18, 10, 70, 28, juce::Justification::centredLeft);
    g.setColour (theme::purple); g.drawText ("2", 80, 10, 16, 28, juce::Justification::centred);
    g.setColour (theme::text); g.drawText ("INSTRUMENT", 96, 10, 140, 28, juce::Justification::centredLeft);
    g.setColour (statusWarn ? theme::warn : theme::textDim); g.setFont (12.f);
    g.drawText (status, statusArea, juce::Justification::centredRight, true);
}

void Vocal2InstrumentEditor::resized()
{
    auto r = getLocalBounds().reduced (16, 10);
    auto top = r.removeFromTop (30);
    top.removeFromLeft (230);
    statusArea = top;
    r.removeFromTop (8);

    auto btnRow = r.removeFromTop (34);
    loadBtn.setBounds (btnRow.removeFromLeft (80)); btnRow.removeFromLeft (6);
    recBtn.setBounds (btnRow.removeFromLeft (64)); btnRow.removeFromLeft (14);
    analyzeBtn.setBounds (btnRow.removeFromLeft (120)); btnRow.removeFromLeft (6);
    convertBtn.setBounds (btnRow.removeFromLeft (160)); btnRow.removeFromLeft (14);
    exportBtn.setBounds (btnRow.removeFromLeft (130)); btnRow.removeFromLeft (6);
    dragHandle.setBounds (btnRow.removeFromLeft (130));
    r.removeFromTop (10);

    waveform.setBounds (r.removeFromTop (juce::jmax (90, r.getHeight() / 5)));
    r.removeFromTop (10);

    auto controls = r.removeFromTop (58);
    const int colW = controls.getWidth() / 3;
    auto place = [&] (juce::Rectangle<int> area, Labelled& l)
    {
        l.label.setBounds (area.removeFromTop (18));
        l.slider.setBounds (area);
    };
    auto c1 = controls.removeFromLeft (colW).reduced (0, 0).withTrimmedRight (12);
    auto c2 = controls.removeFromLeft (colW).withTrimmedRight (12);
    auto c3 = controls;
    // two rows of controls: row 1 = sens / bpm / speed ; row 2 handled below via quant+inst+octave
    place (c1, sens);
    auto bpmArea = c2; hostBpmBtn.setBounds (bpmArea.removeFromRight (46).removeFromBottom (24)); place (bpmArea.withTrimmedRight (4), bpm);
    place (c3, speed);

    auto controls2 = r.removeFromTop (58);
    auto d1 = controls2.removeFromLeft (colW).withTrimmedRight (12);
    auto d2 = controls2.removeFromLeft (colW).withTrimmedRight (12);
    auto d3 = controls2;
    quantLabel.setBounds (d1.removeFromTop (18)); quantBox.setBounds (d1.removeFromTop (26));
    instLabel.setBounds (d2.removeFromTop (18));  instBox.setBounds (d2.removeFromTop (26));
    place (d3, octave);
    r.removeFromTop (6);

    roll.setBounds (r);
}
