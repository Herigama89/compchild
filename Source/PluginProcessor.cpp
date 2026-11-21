#include "PluginProcessor.h"
#include "PluginEditor.h"

Opt2aProcessor::Opt2aProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "OPT2A", createParameterLayout())
{
}

bool Opt2aProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void Opt2aProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    os.reset();
    os.initProcessing ((size_t) samplesPerBlock);
    core.prepare (sampleRate * 2.0);
    dryDelay = juce::roundToInt (os.getLatencyInSamples());
    setLatencySamples (dryDelay);
    delayBuf.setSize (2, dryDelay + 1); delayBuf.clear(); delayPos = 0;
    dry.setSize (2, samplesPerBlock);
    wet.reset (sampleRate, 0.03);
    wet.setCurrentAndTargetValue (p (ids::power) > 0.5f ? 1.0f : 0.0f);
    scopeHop = juce::jmax (32, juce::roundToInt (sampleRate * 0.005));
    scopeCount = 0; acc = {};
}

void Opt2aProcessor::pushScope (ScopeFrame f)
{
    int s1, n1, s2, n2;
    fifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0) fifoData[(size_t) s1] = f;
    fifo.finishedWrite (n1);
}

bool Opt2aProcessor::popScope (ScopeFrame& f)
{
    int s1, n1, s2, n2;
    fifo.prepareToRead (1, s1, n1, s2, n2);
    if (n1 <= 0) return false;
    f = fifoData[(size_t) s1];
    fifo.finishedRead (n1);
    return true;
}

void Opt2aProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || numCh == 0) return;

    core.peakReduction = p (ids::peak);
    core.gain          = p (ids::gain);
    core.limit         = p (ids::limit) > 0.5f;
    core.emphasis      = p (ids::emphasis);
    core.mix           = p (ids::mix);

    wet.setTargetValue (p (ids::power) > 0.5f ? 1.0f : 0.0f);
    const bool fading = wet.isSmoothing();
    const bool on = wet.getTargetValue() > 0.5f;

    // keep a latency-aligned dry copy for the power (bypass) crossfade
    if (dry.getNumSamples() < n) dry.setSize (2, n, false, false, true);
    const int dl = delayBuf.getNumSamples();
    for (int c = 0; c < numCh; ++c)
    {
        auto* in = buffer.getReadPointer (c); auto* d = dry.getWritePointer (c); auto* db = delayBuf.getWritePointer (c);
        int pos = delayPos;
        for (int i = 0; i < n; ++i) { db[pos] = in[i]; d[i] = db[(pos + dl - dryDelay) % dl]; pos = (pos + 1) % dl; }
    }
    delayPos = (delayPos + n) % dl;

    float inPk = 0;
    for (int c = 0; c < numCh; ++c) inPk = juce::jmax (inPk, buffer.getMagnitude (c, 0, n));

    if (! on && ! fading)
    {
        for (int c = 0; c < numCh; ++c) buffer.copyFrom (c, 0, dry, c, 0, n);
        meterGR.store (0.0f); meterOutDb.store (-100.0f);
        core.reset();
        return;
    }

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
    auto up = os.processSamplesUp (block);
    core.process (up.getChannelPointer (0), numCh > 1 ? up.getChannelPointer (1) : nullptr, (int) up.getNumSamples());
    os.processSamplesDown (block);

    if (fading || ! on)
        for (int i = 0; i < n; ++i)
        {
            const float w = wet.getNextValue();
            for (int c = 0; c < numCh; ++c)
            {
                auto* d = buffer.getWritePointer (c);
                d[i] = dry.getSample (c, i) + w * (d[i] - dry.getSample (c, i));
            }
        }

    meterGR.store (core.meterGR.load());
    meterOutDb.store (core.meterOutDb.load());

    float outPk = 0;
    for (int c = 0; c < numCh; ++c) outPk = juce::jmax (outPk, buffer.getMagnitude (c, 0, n));
    acc.in = juce::jmax (acc.in, inPk); acc.out = juce::jmax (acc.out, outPk); acc.gr = juce::jmax (acc.gr, core.blockGR);
    scopeCount += n;
    const int frames = scopeCount / scopeHop;
    scopeCount %= scopeHop;
    for (int f = 0; f < frames; ++f) pushScope (acc);
    if (frames > 0) acc = {};
}

void Opt2aProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void Opt2aProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* Opt2aProcessor::createEditor() { return new Opt2aEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Opt2aProcessor(); }
