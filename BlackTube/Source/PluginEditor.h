#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Knob look: griddy ring (dims as it turns), neon value arc over a cream track, fluted body that turns
class BlackTubeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BlackTubeLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    juce::Font getSliderPopupFont (juce::Slider&) override { return juce::Font (juce::FontOptions (14.0f, juce::Font::bold)); }
    juce::Image ringMain, ringSmall, bodyMain, bodySmall;
};

// Chrome toggle: steps through its positions on click (right-click steps back)
class ToggleSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    ToggleSwitch (juce::AudioProcessorValueTreeState&, const char* paramId, std::vector<int> imageForChoice);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    juce::Image images[3];   // left, centre, right
private:
    juce::RangedAudioParameter* param;
    std::unique_ptr<juce::ParameterAttachment> att;
    std::vector<int> imageIndex;
    int choice = 0;
};

class BlackTubeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BlackTubeEditor (BlackTubeProcessor&);
    ~BlackTubeEditor() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void settleForSnapshot() { for (int i = 0; i < 90; ++i) timerCallback(); }

private:
    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    void addKnob (Knob&, const char* paramId, juce::Point<float> centre, bool small);
    void timerCallback() override;
    void drawScope (juce::Graphics&);

    BlackTubeProcessor& proc;
    BlackTubeLookAndFeel lnf;
    juce::Image panel;

    Knob output, ratio, threshold, attack, release, parallel;
    ToggleSwitch meterSw, arSw, busSw, genSw, cutSw;

    float needlePos = 0.0f, needleVel = 0.0f;   // VU needle, % of the 0 VU deflection
    double lastTick = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlackTubeEditor)
};
