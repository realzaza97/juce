#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <thread>
#include <mutex>
#include "v2i/Converter.h"
#include "v2i/MidiFile.h"
#include "v2i/NoteSegmenter.h"

/**
 * Owns the audio clip, the analysis result and the editable MIDI notes.
 *
 * Threading:
 *  - message thread : loads files, edits notes, starts analysis, exports
 *  - worker thread  : runs v2i::analyze() (never on the audio thread)
 *  - audio thread   : only reads a snapshot of the notes (try-lock) and emits MIDI in sync with the host transport
 */
class Vocal2InstrumentProcessor : public juce::AudioProcessor,
                                  public juce::ChangeBroadcaster,
                                  private juce::AsyncUpdater
{
public:
    Vocal2InstrumentProcessor();
    ~Vocal2InstrumentProcessor() override;

    // ---- AudioProcessor
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Vocal2Instrument"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- audio clip
    bool loadAudioFile (const juce::File& file, juce::String& error);
    const v2i::AudioData& getAudio() const { return audio; }
    juce::String getAudioName() const { return audioFile.getFileName(); }

    // ---- analysis (async)
    void startAnalysis (double regionStartSec, double regionEndSec);   // end <= start => whole clip
    void cancelAnalysis();
    bool isAnalysing() const { return analysing.load(); }
    float getProgress() const { return progress.load(); }
    const std::vector<v2i::PitchFrame>& getFrames() const { return frames; }
    const std::vector<v2i::Note>& getRawNotes() const { return rawNotes; }

    // ---- MIDI notes (message thread only; call notesEdited() after changing)
    v2i::ConvertParams currentConvertParams() const;
    void convertRawNotes();                                   // raw notes -> editable MIDI notes using current parameters
    std::vector<v2i::MidiNote>& editableNotes() { return midiNotes; }
    void notesEdited();                                       // publish to the audio thread + notify UI
    juce::Result exportMidi (const juce::File& dest) const;
    int currentProgram() const;

    // ---- parameters
    juce::AudioProcessorValueTreeState apvts;
    float getSensitivity() const { return apvts.getRawParameterValue ("sens")->load(); }
    double getHostBpm() const { return hostBpm.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleAsyncUpdate() override;
    void analysisThreadBody (std::vector<float> samples, double sr, double offset, float sens);

    v2i::AudioData audio;
    juce::File audioFile;
    std::vector<v2i::PitchFrame> frames;
    std::vector<v2i::Note> rawNotes;
    std::vector<v2i::MidiNote> midiNotes;

    // analysis worker
    std::thread worker;
    std::atomic<bool> analysing { false }, cancelFlag { false };
    std::atomic<float> progress { 0.f };
    std::mutex resultMutex;
    v2i::AnalysisResult pendingResult;

    // audio thread snapshot
    juce::SpinLock snapshotLock;
    std::vector<v2i::MidiNote> audioNotes;
    std::atomic<double> hostBpm { 0.0 };
    bool wasPlaying = false;
    double lastPpqEnd = -1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Vocal2InstrumentProcessor)
};
