#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "dsp/VariMu.h"

class CompchildProcessor : public juce::AudioProcessor
{
public:
    CompchildProcessor();
    ~CompchildProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // gain reduction in dB per channel, for the meters
    std::atomic<float> grA { 0.0f }, grB { 0.0f };

private:
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    cc::VariMu vm;
    juce::AudioBuffer<float> dry;
    juce::SmoothedValue<float> wet;   // 1 = processed, 0 = bypassed (power off)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompchildProcessor)
};
