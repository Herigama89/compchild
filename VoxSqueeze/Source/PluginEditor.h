#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Fader whose travel follows the engraved scale of the panel (non-linear dB spacing).
// The cap is drawn by the editor; the slider itself is an invisible hit area.
class PlateFader : public juce::Slider
{
public:
    PlateFader (std::array<float, 7> values) : vals (values) {}
    double valueToProportionOfLength (double v) override;
    double proportionOfLengthToValue (double p) override;
    float capCentreY() const;               // in editor (unscaled) coordinates
    float centreX = 0.0f;
private:
    std::array<float, 7> vals;               // value at each of the 7 long tick marks, top to bottom
};

class VoxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VoxLookAndFeel();
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override {}
    int getSliderThumbRadius (juce::Slider&) override { return 0; }
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
    juce::Font getSliderPopupFont (juce::Slider&) override { return juce::Font (juce::FontOptions (13.0f, juce::Font::bold)); }
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

class VoxHelpButton : public juce::Button
{
public:
    VoxHelpButton() : juce::Button ("Manual") {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

class VoxSqueezeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VoxSqueezeEditor (VoxSqueezeProcessor&);
    ~VoxSqueezeEditor() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void settleMetersForSnapshot() { for (int i = 0; i < 40; ++i) timerCallback(); }
    void openManual();

private:
    void timerCallback() override;
    void addFader (PlateFader&, const char* paramId, float cx);

    VoxSqueezeProcessor& proc;
    VoxLookAndFeel lnf;
    juce::Image panel, cap;

    PlateFader gate { { 0.0f, -10.0f, -20.0f, -30.0f, -40.0f, -60.0f, -80.0f } };
    PlateFader comp { { 0.0f, -10.0f, -20.0f, -30.0f, -40.0f, -50.0f, -60.0f } };
    PlateFader gain { { 10.0f, 5.0f, 0.0f, -5.0f, -10.0f, -20.0f, -80.0f } };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateAtt, compAtt, gainAtt;

    juce::TextButton monoBtn { "MONO" }, stereoBtn { "STEREO" };
    VoxHelpButton help;

    float mIn = -100.0f, mOut = -100.0f, mGr = 0.0f, mLim = 0.0f, mGate = 0.0f;   // displayed meter values

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxSqueezeEditor)
};
