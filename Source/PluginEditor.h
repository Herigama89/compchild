#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Knob look: draws one of the knob images rotated to the slider position
class CompchildLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CompchildLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    juce::Font getSliderPopupFont (juce::Slider&) override { return juce::Font (juce::FontOptions (13.0f, juce::Font::bold)); }
    juce::Image knobBig, knobPointer;
};

class PowerButton : public juce::Button
{
public:
    PowerButton() : juce::Button ("Power") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
    juce::Image icon;
};

class LinkButton : public juce::Button
{
public:
    LinkButton() : juce::Button ("Link") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

class CompchildEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CompchildEditor (CompchildProcessor&);
    ~CompchildEditor() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void settleMetersForSnapshot() { for (int i = 0; i < 60; ++i) timerCallback(); }

private:
    enum class Kind { Big, TimeConstant, Mode };
    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    void addKnob (Knob&, const char* paramId, Kind, float cx, float cy, float size);
    void timerCallback() override;
    void copyAtoB();

    CompchildProcessor& proc;
    CompchildLookAndFeel lnf;
    juce::Image background;

    Knob thA, inA, tcA, dcA, thB, inB, tcB, dcB, output, mode;
    PowerButton power;
    LinkButton link;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt, linkAtt;

    float needle[2] { -52.0f, -52.0f };   // displayed needle angles (degrees)
    bool syncing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompchildEditor)
};
