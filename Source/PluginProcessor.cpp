#include "PluginProcessor.h"
#include "PluginEditor.h"

CompchildProcessor::CompchildProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "COMPCHILD", createParameterLayout())
{
}

bool CompchildProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void CompchildProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    vm.prepare (sampleRate);
    dry.setSize (2, samplesPerBlock);
    wet.reset (sampleRate, 0.02);
    wet.setCurrentAndTargetValue (p (ids::power) > 0.5f ? 1.0f : 0.0f);
}

void CompchildProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || numCh == 0) return;

    auto read = [this] (cc::ChannelSettings& s, const char* th, const char* in, const char* tc, const char* dc)
    {
        s.threshold = p (th);
        s.inputDb   = p (in);
        s.tc        = (int) p (tc);
        s.dcBias    = p (dc);
    };
    read (vm.a, ids::thA, ids::inA, ids::tcA, ids::dcA);
    read (vm.b, ids::thB, ids::inB, ids::tcB, ids::dcB);
    vm.link     = p (ids::link) > 0.5f;
    if (vm.link) vm.b = vm.a;                         // linked: channel A's controls drive both sides
    vm.mode     = (cc::Mode) (int) p (ids::mode);
    vm.outputDb = p (ids::output);

    wet.setTargetValue (p (ids::power) > 0.5f ? 1.0f : 0.0f);
    const bool fading = wet.isSmoothing();
    const bool on = wet.getTargetValue() > 0.5f;

    if (! on && ! fading)                             // fully off: true bypass
    {
        grA.store (0.0f); grB.store (0.0f);
        vm.reset();
        return;
    }

    if (dry.getNumSamples() < n) dry.setSize (2, n, false, false, true);
    for (int c = 0; c < numCh; ++c) dry.copyFrom (c, 0, buffer, c, 0, n);

    vm.process (buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr, n);

    if (fading || ! on)
    {
        for (int i = 0; i < n; ++i)
        {
            const float w = wet.getNextValue();
            for (int c = 0; c < numCh; ++c)
            {
                auto* d = buffer.getWritePointer (c);
                d[i] = dry.getSample (c, i) + w * (d[i] - dry.getSample (c, i));
            }
        }
    }

    grA.store (vm.meterGR[0].load());
    grB.store (vm.meterGR[1].load());
}

void CompchildProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void CompchildProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* CompchildProcessor::createEditor() { return new CompchildEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CompchildProcessor(); }
