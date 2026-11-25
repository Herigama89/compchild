#include "PluginProcessor.h"
#include "PluginEditor.h"

DoubleTakeProcessor::DoubleTakeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DOUBLETAKE", dtp::layout())
{
}

bool DoubleTakeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // mono or stereo in, stereo out (a doubler is about width); mono -> mono also allowed
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == juce::AudioChannelSet::mono() || in == out;
}

void DoubleTakeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr.store (sampleRate);
    dsp.prepare (sampleRate, samplesPerBlock);
    dsp.s.align = p ("align") > 0.5f;
    reportedLatency = dsp.latencySamples();
    setLatencySamples (reportedLatency);
}

void DoubleTakeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    if (n == 0 || numOut == 0) return;
    if (numIn == 1 && numOut == 2) buffer.copyFrom (1, 0, buffer, 0, 0, n);   // mono in -> both sides
    for (int c = juce::jmax (numIn, numOut == 2 && numIn == 1 ? 2 : numIn); c < numOut; ++c) buffer.clear (c, 0, n);

    auto& s = dsp.s;
    s.fourVoices = p ("four") > 0.5f;
    s.dryOn = p ("dryOn") > 0.5f;
    s.dryGainDb = p ("dryGain"); s.dryPan = p ("dryPan");
    s.align = p ("align") > 0.5f;
    s.fbCutHz = p ("fbCut");
    s.lowGainDb = p ("lowGain"); s.lowFreq = p ("lowFreq"); s.highGainDb = p ("highGain"); s.highFreq = p ("highFreq");
    s.mix = p ("mix"); s.wetDb = p ("wet"); s.outDb = p ("out");
    for (int i = 0; i < dt::kVoices; ++i)
    {
        auto& v = s.v[(size_t) i];
        const int k = i + 1;
        v.on = p (dtp::v (k, "on")) > 0.5f;
        v.gainDb = p (dtp::v (k, "gain")); v.pan = p (dtp::v (k, "pan"));
        v.delayMs = p (dtp::v (k, "delay")); v.feedback = p (dtp::v (k, "fdbk"));
        v.octave = juce::roundToInt (p (dtp::v (k, "oct")));
        v.detuneCents = p (dtp::v (k, "detune")); v.depthCents = p (dtp::v (k, "depth")); v.rateHz = p (dtp::v (k, "rate"));
    }
    if (resyncRequest.exchange (false)) dsp.resyncModulation();
    if (dsp.latencySamples() != reportedLatency) { reportedLatency = dsp.latencySamples(); setLatencySamples (reportedLatency); }

    float* l = buffer.getWritePointer (0);
    float* r = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    dsp.process (l, r, n);

    outL.store (juce::Decibels::gainToDecibels (buffer.getMagnitude (0, 0, n), -100.0f));
    outR.store (juce::Decibels::gainToDecibels (buffer.getMagnitude (numOut > 1 ? 1 : 0, 0, n), -100.0f));
}

const juce::String DoubleTakeProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, (int) dtPresets().size()) ? juce::String (dtPresets()[(size_t) i].name) : juce::String();
}

void DoubleTakeProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) dtPresets().size())) return;
    currentPreset = index;
    const auto& pr = dtPresets()[(size_t) index];
    auto set = [this] (const juce::String& id, float value)
    {
        if (auto* prm = apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
            prm->endChangeGesture();
        }
    };
    set ("four", pr.four ? 1.0f : 0.0f); set ("mix", pr.mix); set ("dryGain", pr.dryGain); set ("dryOn", 1.0f); set ("dryPan", 0.0f);
    set ("lowGain", pr.lowGain); set ("lowFreq", pr.lowFreq); set ("highGain", pr.highGain); set ("highFreq", pr.highFreq);
    set ("fbCut", pr.fbCut); set ("wet", 0.0f); set ("out", 0.0f);
    for (int i = 0; i < 4; ++i)
    {
        const auto& v = pr.v[(size_t) i];
        const int k = i + 1;
        set (dtp::v (k, "on"), v.on ? 1.0f : 0.0f); set (dtp::v (k, "gain"), v.gain); set (dtp::v (k, "pan"), v.pan);
        set (dtp::v (k, "delay"), v.delay); set (dtp::v (k, "fdbk"), v.fdbk); set (dtp::v (k, "oct"), (float) v.oct);
        set (dtp::v (k, "detune"), v.detune); set (dtp::v (k, "depth"), v.depth); set (dtp::v (k, "rate"), v.rate);
    }
    resyncModulation();
}

void DoubleTakeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void DoubleTakeProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto t = juce::ValueTree::fromXml (*xml);
            currentPreset = juce::jlimit (0, (int) dtPresets().size() - 1, (int) t.getProperty ("preset", 0));
            apvts.replaceState (t);
        }
}

juce::AudioProcessorEditor* DoubleTakeProcessor::createEditor() { return new DoubleTakeEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DoubleTakeProcessor(); }
