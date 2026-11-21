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

// LINK / POWER: invisible click area over the jewel lamp and its engraved word
class PanelButton : public juce::Button
{
public:
    enum class Icon { Power, Link };
    PanelButton (const juce::String& name, Icon i) : juce::Button (name), iconType (i) { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
    juce::Image icon;   // power symbol artwork (Link is drawn)
private:
    Icon iconType;
};

// Engraved "?" that opens the manual
class HelpButton : public juce::Button
{
public:
    HelpButton() : juce::Button ("Manual") {}
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
    void mouseDown (const juce::MouseEvent&) override;

    enum class Skin { Cream = 0, Black = 1 };
    void setSkin (Skin);

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
    juce::Image background, dome, glass, blackFace;
    Skin skin = Skin::Cream;

    Knob thA, inA, tcA, dcA, thB, inB, tcB, dcB, output, mode;
    PanelButton link  { "Link",  PanelButton::Icon::Link };
    PanelButton power { "Power", PanelButton::Icon::Power };
    HelpButton help;
    void openManual();
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt, linkAtt;

    float needle[2] { -50.0f, -50.0f };   // displayed needle angles (degrees)
    bool syncing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompchildEditor)
};
