#include "PluginProcessor.h"
#include "PluginEditor.h"

VoxSqueezeProcessor::VoxSqueezeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VOXSQUEEZE", createVoxParameterLayout())
{
}

bool VoxSqueezeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void VoxSqueezeProcessor::prepareToPlay (double sampleRate, int)
{
    dsp.prepare (sampleRate);
    setLatencySamples (dsp.latencySamples());
}

static float peakDb (const juce::AudioBuffer<float>& b, int numCh, int n)
{
    float pk = 0.0f;
    for (int c = 0; c < numCh; ++c) pk = juce::jmax (pk, b.getMagnitude (c, 0, n));
    return juce::Decibels::gainToDecibels (pk, -100.0f);
}

void VoxSqueezeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || numCh == 0) return;

    // MONO: the input is summed to mono first (both outputs carry the same signal)
    if (numCh == 2 && p (vids::stereo) < 0.5f)
    {
        auto* l = buffer.getWritePointer (0); auto* r = buffer.getWritePointer (1);
        for (int i = 0; i < n; ++i) l[i] = r[i] = 0.5f * (l[i] + r[i]);
    }
    inDb.store (peakDb (buffer, numCh, n));

    dsp.s.gateDb = p (vids::gate);
    dsp.s.compDb = p (vids::comp);
    dsp.s.gainDb = p (vids::gain);
    dsp.process (buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr, n);

    outDb.store (peakDb (buffer, numCh, n));
    grDb.store (dsp.meterGR.load());
    limDb.store (dsp.meterLim.load());
    gateDb.store (dsp.meterGate.load());
}

void VoxSqueezeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

// After replaceState, push every stored value into its parameter explicitly. JUCE skips parameters whose
// cached plain value already matches, which leaves on/off parameters holding an in-between normalised
// value if the host set one; this makes a restored session exact.
static void forceParametersFromState (juce::AudioProcessorValueTreeState& apvts)
{
    for (auto* rp : apvts.processor.getParameters())
        if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (rp))
        {
            const auto child = apvts.state.getChildWithProperty ("id", prm->getParameterID());
            if (child.isValid() && child.hasProperty ("value"))
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) child.getProperty ("value")));
        }
}

void VoxSqueezeProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            forceParametersFromState (apvts);
        }
}

juce::AudioProcessorEditor* VoxSqueezeProcessor::createEditor() { return new VoxSqueezeEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VoxSqueezeProcessor(); }
