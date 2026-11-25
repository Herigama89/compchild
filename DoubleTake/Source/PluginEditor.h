#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace dtui
{
    // console fader: travel follows a console scale (+10 dB at the top, -inf at the bottom)
    class ConsoleFader : public juce::Slider
    {
    public:
        ConsoleFader();
        double valueToProportionOfLength (double v) override;
        double proportionOfLengthToValue (double p) override;
    };

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();
        void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
        void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
        juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
        int getSliderThumbRadius (juce::Slider&) override { return 0; }
        juce::Font getSliderPopupFont (juce::Slider&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
        void drawPopupMenuBackground (juce::Graphics&, int, int) override;
    };

    // round voice badge / direct on-off
    class Badge : public juce::Button
    {
    public:
        Badge (juce::String t, juce::Colour c) : juce::Button (t), text (t), col (c) { setClickingTogglesState (true); }
        void paintButton (juce::Graphics&, bool over, bool down) override;
    private:
        juce::String text; juce::Colour col;
    };

    class HelpButton : public juce::Button
    {
    public:
        HelpButton() : juce::Button ("Manual") {}
        void paintButton (juce::Graphics&, bool over, bool down) override;
    };

    class Display : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        enum Kind { stage, timePitch, eq };
        Display (DoubleTakeProcessor& p, Kind k) : proc (p), kind (k) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        double lfoTime = 0.0;    // advanced by the editor, for the wobble animation
    private:
        juce::Rectangle<float> plot() const;
        void setFromMouse (juce::Point<float>);
        float pv (const juce::String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }
        DoubleTakeProcessor& proc;
        Kind kind;
        int dragging = -1;       // eq: 0 = low, 1 = high
    };
}

class DoubleTakeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DoubleTakeEditor (DoubleTakeProcessor&);
    ~DoubleTakeEditor() override;
    void paint (juce::Graphics&) override {}
    void resized() override;
    void openManual();
    void settleForSnapshot() { for (int i = 0; i < 30; ++i) timerCallback(); }

private:
    class Panel;
    void timerCallback() override;
    DoubleTakeProcessor& proc;
    std::unique_ptr<Panel> panel;
    juce::TooltipWindow tooltips { this, 600 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoubleTakeEditor)
};
