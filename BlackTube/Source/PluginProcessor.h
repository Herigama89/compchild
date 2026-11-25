#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "dsp/Opto.h"

class BlackTubeProcessor : public juce::AudioProcessor
{
public:
    BlackTubeProcessor();
    ~BlackTubeProcessor() override = default;

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

    // meters (dB): input / output read like a sine's peak level, gain reduction is positive
    std::atomic<float> inDb { -100.0f }, outDb { -100.0f }, grDb { 0.0f };

    // dynamics scope history, one column about every 1/60 s
    static constexpr int scopeSize = 512;
    struct ScopeColumn { float inDb = -100.0f, keptDb = -100.0f, grDb = 0.0f; };
    ScopeColumn scope[scopeSize];
    std::atomic<int> scopeWrite { 0 };

private:
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    bt::Opto opto;
    juce::AudioBuffer<float> dry;
    juce::SmoothedValue<float> wet;
    double sr = 48000.0;
    int scopeAcc = 0, scopeEvery = 800;
    ScopeColumn scopeHold;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlackTubeProcessor)
};
