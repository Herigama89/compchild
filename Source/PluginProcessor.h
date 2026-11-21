#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "dsp/Opto2A.h"

// one column of the scrolling waveform display
struct ScopeFrame { float in = 0, out = 0, gr = 0; };

class Opt2aProcessor : public juce::AudioProcessor
{
public:
    Opt2aProcessor();
    ~Opt2aProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // meters + scope (written on the audio thread, read by the editor)
    std::atomic<float> meterGR { 0.0f }, meterOutDb { -100.0f };
    bool popScope (ScopeFrame&);

private:
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void pushScope (ScopeFrame f);

    opt::Opto2A core;
    juce::dsp::Oversampling<float> os { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };
    juce::AudioBuffer<float> dry;
    juce::SmoothedValue<float> wet;
    int dryDelay = 0;
    juce::AudioBuffer<float> delayBuf; int delayPos = 0;

    // scope accumulation: one frame every ~5 ms
    int scopeHop = 240, scopeCount = 0; ScopeFrame acc;
    juce::AbstractFifo fifo { 1024 };
    std::array<ScopeFrame, 1024> fifoData;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Opt2aProcessor)
};
