#include "PluginProcessor.h"
#include "PluginEditor.h"

BlackTubeProcessor::BlackTubeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "BLACKTUBE", createBlackTubeLayout())
{
}

bool BlackTubeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet() != out) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void BlackTubeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    opto.prepare (sampleRate);
    dry.setSize (2, samplesPerBlock);
    wet.reset (sampleRate, 0.02);
    wet.setCurrentAndTargetValue (p (btids::power) > 0.5f ? 1.0f : 0.0f);
    scopeEvery = juce::jmax (64, (int) (sampleRate / 60.0));
    scopeAcc = 0;
}

void BlackTubeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto mainIn = getBusBuffer (buffer, true, 0);
    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (mainIn.getNumChannels(), 2);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || numCh == 0) return;

    auto& s = opto.s;
    const float th = p (btids::threshold), out = p (btids::output);
    s.thresholdOff = btparam::thresholdIsOff (th);
    s.thresholdDb  = juce::jmin (th, 0.0f) + juce::jmax (th, 0.0f);      // 0..+10 region still compresses lightly
    s.ratio    = p (btids::ratio);
    s.attack   = p (btids::attack);
    s.release  = p (btids::release);
    s.arMode   = (bt::ArMode) (int) p (btids::arSelect);
    s.vintage  = (int) p (btids::gen) == 0;
    s.lowCut   = (int) p (btids::lowCut);
    s.external = (int) p (btids::scBus) == 1;
    s.parallel = p (btids::parallel) * 0.01f;
    s.outputOff = btparam::outputIsOff (out);
    s.outputDb  = out;

    wet.setTargetValue (p (btids::power) > 0.5f ? 1.0f : 0.0f);
    const bool fading = wet.isSmoothing();
    const bool on = wet.getTargetValue() > 0.5f;

    // sidechain (only when the host connected it)
    const float* scL = nullptr; const float* scR = nullptr;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        if (sc.getNumChannels() > 0) { scL = sc.getReadPointer (0); scR = sc.getNumChannels() > 1 ? sc.getReadPointer (1) : nullptr; }
    }

    if (! on && ! fading)
    {
        // true bypass; keep the meters reading the input
        float pk = 0.0f;
        for (int c = 0; c < numCh; ++c) pk = juce::jmax (pk, mainIn.getMagnitude (c, 0, n));
        inDb.store (juce::Decibels::gainToDecibels (pk, -100.0f)); outDb.store (inDb.load()); grDb.store (0.0f);
        opto.reset();
        return;
    }

    if (dry.getNumSamples() < n) dry.setSize (2, n, false, false, true);
    for (int c = 0; c < numCh; ++c) dry.copyFrom (c, 0, mainIn, c, 0, n);

    opto.process (mainIn.getWritePointer (0), numCh > 1 ? mainIn.getWritePointer (1) : nullptr, n, scL, scR);

    if (fading || ! on)
        for (int i = 0; i < n; ++i)
        {
            const float w = wet.getNextValue();
            for (int c = 0; c < numCh; ++c)
            {
                auto* d = mainIn.getWritePointer (c);
                d[i] = dry.getSample (c, i) + w * (d[i] - dry.getSample (c, i));
            }
        }

    inDb.store (opto.meterIn.load());
    outDb.store (opto.meterOut.load());
    grDb.store (opto.meterGR.load() * opto.s.parallel);

    // scope: keep the loudest moment of each ~1/60 s column
    scopeHold.inDb   = juce::jmax (scopeHold.inDb, opto.blockInPeakDb);
    scopeHold.keptDb = juce::jmax (scopeHold.keptDb, opto.blockKeptPeakDb);
    scopeHold.grDb   = juce::jmin (scopeHold.grDb, opto.blockGRDb * opto.s.parallel);
    scopeAcc += n;
    if (scopeAcc >= scopeEvery)
    {
        scopeAcc = 0;
        const int w = (scopeWrite.load (std::memory_order_relaxed) + 1) % scopeSize;
        scope[w] = scopeHold;
        scopeWrite.store (w, std::memory_order_release);
        scopeHold = {};
    }
}

void BlackTubeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void BlackTubeProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* BlackTubeProcessor::createEditor() { return new BlackTubeEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BlackTubeProcessor(); }
