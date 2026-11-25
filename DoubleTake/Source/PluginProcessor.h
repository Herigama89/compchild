#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Presets.h"
#include "dsp/DoubleTake.h"

class DoubleTakeProcessor : public juce::AudioProcessor
{
public:
    DoubleTakeProcessor();
    ~DoubleTakeProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return (int) dtPresets().size(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    void resyncModulation() { resyncRequest.store (true); }
    double getSampleRateForUi() const { return sr.load(); }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> outL { -100.0f }, outR { -100.0f };

private:
    float p (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    dt::Processor dsp;
    int currentPreset = 0;
    std::atomic<bool> resyncRequest { false };
    std::atomic<double> sr { 48000.0 };
    int reportedLatency = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoubleTakeProcessor)
};
