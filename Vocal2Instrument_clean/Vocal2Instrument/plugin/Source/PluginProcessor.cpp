#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

Vocal2InstrumentProcessor::Vocal2InstrumentProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

Vocal2InstrumentProcessor::~Vocal2InstrumentProcessor()
{
    cancelPendingUpdate();
    cancelAnalysis();
}

APVTS::ParameterLayout Vocal2InstrumentProcessor::createLayout()
{
    using namespace juce;
    APVTS::ParameterLayout l;
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "sens", 1 }, "Sensitivity", NormalisableRange<float> (0.f, 1.f, 0.01f), 0.5f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "quant", 1 }, "Quantize", StringArray { "Off", "1/4", "1/8", "1/16", "1/32" }, 3));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "octave", 1 }, "Octave", -3, 3, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "bpm", 1 }, "BPM", NormalisableRange<float> (40.f, 240.f, 0.1f), 120.f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "speed", 1 }, "Speed", NormalisableRange<float> (0.5f, 2.0f, 0.01f), 1.0f));
    StringArray inst;
    for (auto& p : v2i::instrumentPresets()) inst.add (p.name);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "inst", 1 }, "Instrument", inst, 0));
    return l;
}

// ------------------------------------------------------------------ audio clip
bool Vocal2InstrumentProcessor::loadAudioFile (const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();   // WAV, AIFF, FLAC, OGG, and MP3 (JUCE_USE_MP3AUDIOFORMAT=1)
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr) { error = "Unsupported or unreadable audio file."; return false; }
    const double maxSeconds = 600.0;
    const auto n = (juce::int64) juce::jmin ((double) reader->lengthInSamples, maxSeconds * reader->sampleRate);
    if (n < (juce::int64) (reader->sampleRate * 0.2)) { error = "Audio is too short."; return false; }

    juce::AudioBuffer<float> buf ((int) juce::jmax (1u, reader->numChannels), (int) n);
    reader->read (&buf, 0, (int) n, 0, true, true);
    v2i::AudioData d; d.sampleRate = reader->sampleRate; d.samples.assign ((size_t) n, 0.f);
    for (int c = 0; c < buf.getNumChannels(); ++c)
    {
        auto* src = buf.getReadPointer (c);
        for (int i = 0; i < (int) n; ++i) d.samples[(size_t) i] += src[i] / (float) buf.getNumChannels();
    }
    cancelAnalysis();
    audio = std::move (d); audioFile = file;
    frames.clear(); rawNotes.clear(); midiNotes.clear();
    notesEdited();
    return true;
}

// ------------------------------------------------------------------ analysis
void Vocal2InstrumentProcessor::startAnalysis (double t0, double t1)
{
    if (audio.samples.empty()) return;
    cancelAnalysis();
    const double dur = audio.durationSec();
    if (t1 <= t0) { t0 = 0; t1 = dur; }
    t0 = juce::jlimit (0.0, dur, t0); t1 = juce::jlimit (0.0, dur, t1);
    const size_t a = (size_t) (t0 * audio.sampleRate), b = (size_t) (t1 * audio.sampleRate);
    if (b <= a + 1) return;
    std::vector<float> region (audio.samples.begin() + (long) a, audio.samples.begin() + (long) b);
    analysing = true; cancelFlag = false; progress = 0.f;
    worker = std::thread ([this, r = std::move (region), sr = audio.sampleRate, t0, s = getSensitivity()]() mutable
                          { analysisThreadBody (std::move (r), sr, t0, s); });
    sendChangeMessage();
}

void Vocal2InstrumentProcessor::analysisThreadBody (std::vector<float> samples, double sr, double offset, float sens)
{
    v2i::AudioData a; a.samples = std::move (samples); a.sampleRate = sr;
    v2i::AnalysisParams p; p.sensitivity = sens;
    auto res = v2i::analyze (a, p, offset, &progress, &cancelFlag);
    if (! cancelFlag.load())
    {
        { std::lock_guard<std::mutex> lk (resultMutex); pendingResult = std::move (res); }
        triggerAsyncUpdate();
    }
    analysing = false;
}

void Vocal2InstrumentProcessor::cancelAnalysis()
{
    cancelFlag = true;
    if (worker.joinable()) worker.join();
    cancelPendingUpdate();   // drop a stale result that was posted before the cancel
    analysing = false;
}

void Vocal2InstrumentProcessor::handleAsyncUpdate()   // message thread
{
    {
        std::lock_guard<std::mutex> lk (resultMutex);
        frames = std::move (pendingResult.frames);
        rawNotes = std::move (pendingResult.notes);
    }
    convertRawNotes();
}

// ------------------------------------------------------------------ MIDI notes
v2i::ConvertParams Vocal2InstrumentProcessor::currentConvertParams() const
{
    static const int divs[] = { 0, 4, 8, 16, 32 };
    v2i::ConvertParams c;
    c.bpm = apvts.getRawParameterValue ("bpm")->load();
    c.speed = apvts.getRawParameterValue ("speed")->load();
    c.gridDiv = divs[juce::jlimit (0, 4, (int) apvts.getRawParameterValue ("quant")->load())];
    c.octaveShift = (int) apvts.getRawParameterValue ("octave")->load();
    c.instrument = (int) apvts.getRawParameterValue ("inst")->load();
    return c;
}

int Vocal2InstrumentProcessor::currentProgram() const
{
    return v2i::instrumentPresets()[(size_t) currentConvertParams().instrument].gmProgram;
}

void Vocal2InstrumentProcessor::convertRawNotes()
{
    midiNotes = v2i::convertToMidiNotes (rawNotes, currentConvertParams());
    notesEdited();
}

void Vocal2InstrumentProcessor::notesEdited()
{
    {
        const juce::SpinLock::ScopedLockType sl (snapshotLock);
        audioNotes = midiNotes;
    }
    sendChangeMessage();
}

juce::Result Vocal2InstrumentProcessor::exportMidi (const juce::File& dest) const
{
    if (midiNotes.empty()) return juce::Result::fail ("There are no notes to export. Run ANALYZE first.");
    const auto bytes = v2i::writeMidiFile (midiNotes, currentConvertParams().bpm, currentProgram(), "Vocal2Instrument");
    juce::TemporaryFile tmp (dest);
    if (! tmp.getFile().replaceWithData (bytes.data(), bytes.size()) || ! tmp.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("Could not write " + dest.getFullPathName());
    return juce::Result::ok();
}

// ------------------------------------------------------------------ realtime: MIDI out in sync with the host
void Vocal2InstrumentProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    midi.clear();

    const auto pos = getPlayHead() != nullptr ? getPlayHead()->getPosition() : juce::Optional<juce::AudioPlayHead::PositionInfo>();
    const bool playing = pos.hasValue() && pos->getIsPlaying();
    if (pos.hasValue() && pos->getBpm().hasValue()) hostBpm = *pos->getBpm();

    if (! playing)
    {
        if (wasPlaying) midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        wasPlaying = false; lastPpqEnd = -1.0;
        return;
    }

    const double bpm = pos->getBpm().orFallback (120.0);
    const double ppqStart = pos->getPpqPosition().orFallback (0.0);
    const double beatsPerSample = bpm / 60.0 / juce::jmax (1.0, getSampleRate());
    const double ppqEnd = ppqStart + buffer.getNumSamples() * beatsPerSample;

    // transport started or jumped: silence anything still ringing
    if (! wasPlaying || std::abs (ppqStart - lastPpqEnd) > 0.05)
        midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
    wasPlaying = true; lastPpqEnd = ppqEnd;

    const juce::SpinLock::ScopedTryLockType sl (snapshotLock);
    if (! sl.isLocked()) return;

    auto sampleAt = [&] (double beat) { return juce::jlimit (0, buffer.getNumSamples() - 1, (int) ((beat - ppqStart) / beatsPerSample)); };
    for (auto& n : audioNotes)      // note-offs first so a legato retrigger at the same sample is clean
    {
        const double end = n.startBeat + n.lengthBeat;
        if (end >= ppqStart && end < ppqEnd) midi.addEvent (juce::MidiMessage::noteOff (1, n.note), sampleAt (end));
    }
    for (auto& n : audioNotes)
        if (n.startBeat >= ppqStart && n.startBeat < ppqEnd)
            midi.addEvent (juce::MidiMessage::noteOn (1, n.note, (juce::uint8) n.velocity), sampleAt (n.startBeat));
}

// ------------------------------------------------------------------ state
void Vocal2InstrumentProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("NOTES"), nullptr);
    juce::ValueTree notes ("NOTES");
    for (auto& n : midiNotes)
    {
        juce::ValueTree v ("N");
        v.setProperty ("s", n.startBeat, nullptr); v.setProperty ("l", n.lengthBeat, nullptr);
        v.setProperty ("n", n.note, nullptr); v.setProperty ("v", n.velocity, nullptr);
        notes.appendChild (v, nullptr);
    }
    state.appendChild (notes, nullptr);
    state.setProperty ("audioPath", audioFile.getFullPathName(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void Vocal2InstrumentProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto vt = juce::ValueTree::fromXml (*xml);
    const auto notes = vt.getChildWithName ("NOTES");
    const juce::File f (vt.getProperty ("audioPath").toString());
    vt.removeChild (notes, nullptr);
    apvts.replaceState (vt);

    juce::String err;
    if (f.existsAsFile() && f != audioFile) loadAudioFile (f, err);
    midiNotes.clear();
    for (auto n : notes)
        midiNotes.push_back ({ (double) n["s"], (double) n["l"], (int) n["n"], (int) n["v"] });
    notesEdited();
}

juce::AudioProcessorEditor* Vocal2InstrumentProcessor::createEditor() { return new Vocal2InstrumentEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Vocal2InstrumentProcessor(); }
