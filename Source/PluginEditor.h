#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Draws the panel's knob sprites rotated to the slider position
class Opt2aLookAndFeel : public juce::LookAndFeel_V4
{
public:
    Opt2aLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    juce::Font getSliderPopupFont (juce::Slider&) override { return juce::Font (juce::FontOptions (13.0f, juce::Font::bold)); }
    juce::Image silver, screw, selector;
};

// LIMIT / COMPRESS bat toggle
class ToggleSwitch : public juce::Button
{
public:
    ToggleSwitch() : juce::Button ("Limit") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
    juce::Image up, down;
};

// invisible click area (POWER lamp + its engraved word)
class HotSpot : public juce::Button
{
public:
    explicit HotSpot (const juce::String& n) : juce::Button (n) { setClickingTogglesState (true); }
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        if (over) { g.setColour (juce::Colours::white.withAlpha (0.04f)); g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f); }
    }
};

// engraved "?" that opens the manual
class HelpButton : public juce::Button
{
public:
    HelpButton() : juce::Button ("Manual") {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

class Opt2aEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Opt2aEditor (Opt2aProcessor&);
    ~Opt2aEditor() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void settleForSnapshot();   // used by the snapshot tool (no host / message loop)
    void openManual();

private:
    enum class Kind { Big = 0, Screw = 1, Selector = 2 };
    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    void addKnob (Knob&, const char* paramId, Kind, float cx, float cy, float size);
    void timerCallback() override;
    void drawScope (juce::Graphics&);
    void drawMeter (juce::Graphics&);

    Opt2aProcessor& proc;
    Opt2aLookAndFeel lnf;
    juce::Image background, vuCut, vuGlass, scopeGlass, lampOn, lampOff;

    Knob gain, peak, emphasis, mix, meter;
    ToggleSwitch limitSwitch;
    HotSpot power { "Power" };
    HelpButton help;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> limitAtt, powerAtt;

    static constexpr int historyLen = 432;           // scope columns (~2.2 s)
    std::array<ScopeFrame, historyLen> history {};
    int historyPos = 0;
    float needle = -53.4f;                           // displayed needle angle (degrees)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Opt2aEditor)
};
