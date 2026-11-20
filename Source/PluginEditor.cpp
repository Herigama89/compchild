#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    constexpr int kW = 1176, kH = 564;

    // VU meters: face top-left corners, needle pivot inside the face, needle length
    constexpr float vuLeft[2] = { 378.0f, 593.0f };
    constexpr float vuTop = 60.0f, pivotX = 102.5f, pivotY = 114.7f, needleLen = 80.0f;
    constexpr float needleRest = -52.0f, needleZero = 0.0f;   // left peg / 0 dB GR mark

    // power button + LED, link button (editor coordinates, from the design)
    const juce::Point<float> powerCentre { 738.0f, 476.0f };
    const juce::Point<float> ledCentre   { 758.5f, 475.5f };
    const juce::Rectangle<int> linkBounds { 554, 226, 68, 24 };

    const juce::Colour orange (0xffffb020), orangeDeep (0xffff7a00);

    juce::Image loadPng (const void* data, int size) { return juce::ImageCache::getFromMemory (data, size); }
}

//==============================================================================
CompchildLookAndFeel::CompchildLookAndFeel()
{
    knobBig     = loadPng (BinaryData::knob_big_png,     BinaryData::knob_big_pngSize);
    knobPointer = loadPng (BinaryData::knob_pointer_png, BinaryData::knob_pointer_pngSize);
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xee141210));
    setColour (juce::BubbleComponent::outlineColourId,    juce::Colour (0xff3a3833));
    setColour (juce::TooltipWindow::textColourId,         juce::Colour (0xffe9dfc6));
}

void CompchildLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s)
{
    const auto kind = (int) s.getProperties()["kind"];
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = bounds.getCentre();

    float angleDeg;
    const juce::Image* img = &knobBig;
    if (kind == 1)        // time constant: 6 detents at -100..+100 degrees
    {
        const int idx = juce::roundToInt (pos * 5.0f);
        angleDeg = -100.0f + 40.0f * (float) idx - 15.0f;
        img = &knobPointer;
    }
    else if (kind == 2)   // mode: L/R, LAT/VERT, MID/SIDE
    {
        const int idx = juce::roundToInt (pos * 2.0f);
        angleDeg = -50.0f + 50.0f * (float) idx - 15.0f;
        img = &knobPointer;
    }
    else                  // big black knob: image is drawn at its minimum position
        angleDeg = 270.0f * pos;

    // soft drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (bounds.reduced (bounds.getWidth() * 0.2f).translated (0.0f, bounds.getHeight() * 0.06f));

    if (img->isValid())
    {
        const auto t = juce::AffineTransform::scale (bounds.getWidth() / (float) img->getWidth(), bounds.getHeight() / (float) img->getHeight())
                           .translated (bounds.getX(), bounds.getY())
                           .rotated (juce::degreesToRadians (angleDeg), c.x, c.y);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImageTransformed (*img, t);
    }
}

//==============================================================================
void PowerButton::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();

    if (on)
    {
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (orangeDeep.withAlpha (0.10f * (float) i));
            g.fillEllipse (b.expanded ((float) (4 - i) * 2.0f).reduced (0.0f));
        }
    }
    juce::ColourGradient grad (juce::Colour (0xff2c2b28), b.getX() + b.getWidth() * 0.4f, b.getY() + b.getHeight() * 0.35f,
                               juce::Colour (0xff0b0b0a), b.getRight(), b.getBottom(), true);
    g.setGradientFill (grad);
    g.fillEllipse (b);
    g.setColour (juce::Colour (over ? 0xff57534a : 0xff3a3833));
    g.drawEllipse (b, 1.5f);

    if (icon.isValid())
    {
        const auto ib = b.reduced (b.getWidth() * 0.12f);
        if (on)
        {
            g.setColour (orange);
            g.drawImage (icon, ib, juce::RectanglePlacement::centred, true);   // tint the icon orange
        }
        else
        {
            g.setOpacity (0.75f);
            g.drawImage (icon, ib, juce::RectanglePlacement::centred, false);
        }
    }
}

void LinkButton::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    g.setColour (juce::Colour (0xff0f0f0e));
    g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
    g.setColour (juce::Colour (over ? 0xff57534a : 0xff3a3833));
    g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.0f);

    const juce::Rectangle<float> led (b.getX() + 12.0f, b.getCentreY() - 3.5f, 7.0f, 7.0f);
    if (on)
    {
        g.setColour (orangeDeep.withAlpha (0.35f));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (on ? orange : juce::Colour (0xff3a3833));
    g.fillEllipse (led);

    g.setColour (on ? juce::Colour (0xffffc061) : juce::Colour (0xff8a8273));
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)).withExtraKerningFactor (0.14f));
    g.drawText ("LINK", b.withTrimmedLeft (24.0f), juce::Justification::centredLeft);
}

//==============================================================================
CompchildEditor::CompchildEditor (CompchildProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    background = loadPng (BinaryData::background_png, BinaryData::background_pngSize);
    power.icon = loadPng (BinaryData::power_png, BinaryData::power_pngSize);

    // layout from the Compchild wood-panel design (1176 x 564)
    const float A = 280.0f, B = 896.0f;
    addKnob (thA, ids::thA, Kind::Big,          A,        172.0f, 136.0f);
    addKnob (inA, ids::inA, Kind::Big,          A - 82,   322.0f,  88.0f);
    addKnob (tcA, ids::tcA, Kind::TimeConstant, A + 82,   322.0f,  77.0f);
    addKnob (dcA, ids::dcA, Kind::Big,          A,        448.0f,  65.0f);
    addKnob (thB, ids::thB, Kind::Big,          B,        172.0f, 136.0f);
    addKnob (inB, ids::inB, Kind::Big,          B - 82,   322.0f,  88.0f);
    addKnob (tcB, ids::tcB, Kind::TimeConstant, B + 82,   322.0f,  77.0f);
    addKnob (dcB, ids::dcB, Kind::Big,          B,        448.0f,  65.0f);
    addKnob (output, ids::output, Kind::Big,    588.0f,   330.0f, 136.0f);
    addKnob (mode, ids::mode, Kind::Mode,       438.0f,   462.0f,  58.0f);

    // linked: turning a channel A control moves the matching channel B control
    auto follow = [this] (Knob& src, Knob& dst)
    {
        src.slider.onValueChange = [this, &src, &dst]
        {
            if (! syncing && link.getToggleState())
            {
                syncing = true;
                dst.slider.setValue (src.slider.getValue(), juce::sendNotificationSync);
                syncing = false;
            }
        };
    };
    follow (thA, thB); follow (inA, inB); follow (tcA, tcB); follow (dcA, dcB);
    follow (thB, thA); follow (inB, inA); follow (tcB, tcA); follow (dcB, dcA);

    addAndMakeVisible (power);
    addAndMakeVisible (link);
    power.setTooltip ("Power / bypass");
    link.setTooltip ("Link: channel A controls both channels");
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::power, power);
    linkAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::link,  link);
    link.onClick = [this] { if (link.getToggleState()) copyAtoB(); };
    power.onClick = [this] { repaint(); };

    setSize (kW, kH);
    setResizable (true, true);
    setResizeLimits (kW / 2, kH / 2, kW * 2, kH * 2);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    startTimerHz (30);
}

CompchildEditor::~CompchildEditor()
{
    for (auto* k : { &thA, &inA, &tcA, &dcA, &thB, &inB, &tcB, &dcB, &output, &mode })
        k->slider.setLookAndFeel (nullptr);
}

void CompchildEditor::addKnob (Knob& k, const char* paramId, Kind kind, float cx, float cy, float size)
{
    auto& s = k.slider;
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setLookAndFeel (&lnf);
    s.getProperties().set ("kind", kind == Kind::Big ? 0 : kind == Kind::TimeConstant ? 1 : 2);
    s.getProperties().set ("cx", cx);
    s.getProperties().set ("cy", cy);
    s.getProperties().set ("size", size);
    s.setMouseDragSensitivity (kind == Kind::Big ? 260 : 120);
    s.setPopupDisplayEnabled (true, false, this);
    s.setScrollWheelEnabled (true);
    addAndMakeVisible (s);
    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, s);
    if (auto* prm = proc.apvts.getParameter (paramId))
        s.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
}

void CompchildEditor::copyAtoB()
{
    syncing = true;
    thB.slider.setValue (thA.slider.getValue(), juce::sendNotificationSync);
    inB.slider.setValue (inA.slider.getValue(), juce::sendNotificationSync);
    tcB.slider.setValue (tcA.slider.getValue(), juce::sendNotificationSync);
    dcB.slider.setValue (dcA.slider.getValue(), juce::sendNotificationSync);
    syncing = false;
}

void CompchildEditor::resized()
{
    const float sc = (float) getWidth() / (float) kW;
    for (auto* k : { &thA, &inA, &tcA, &dcA, &thB, &inB, &tcB, &dcB, &output, &mode })
    {
        auto& props = k->slider.getProperties();
        const float cx = (float) props["cx"], cy = (float) props["cy"], size = (float) props["size"];
        k->slider.setBounds (juce::Rectangle<float> (size, size).withCentre ({ cx, cy }).transformedBy (juce::AffineTransform::scale (sc)).toNearestInt());
    }
    power.setBounds (juce::Rectangle<float> (24.0f, 24.0f).withCentre (powerCentre).transformedBy (juce::AffineTransform::scale (sc)).toNearestInt());
    link.setBounds (linkBounds.toFloat().transformedBy (juce::AffineTransform::scale (sc)).toNearestInt());
}

void CompchildEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1611));
    if (background.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (background, getLocalBounds().toFloat());
    }
}

void CompchildEditor::paintOverChildren (juce::Graphics& g)
{
    const float sc = (float) getWidth() / (float) kW;
    g.addTransform (juce::AffineTransform::scale (sc));

    // VU needles
    for (int i = 0; i < 2; ++i)
    {
        const juce::Point<float> pivot (vuLeft[i] + pivotX, vuTop + pivotY);
        const float a = juce::degreesToRadians (needle[i]);
        const juce::Point<float> tip (pivot.x + needleLen * std::sin (a), pivot.y - needleLen * std::cos (a));
        const juce::Point<float> base (pivot.x + 22.0f * std::sin (a), pivot.y - 22.0f * std::cos (a));
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawLine ({ base.translated (1.5f, 2.0f), tip.translated (1.5f, 2.0f) }, 2.0f);
        g.setColour (juce::Colour (0xff1b1510));
        g.drawLine ({ base, tip }, 1.8f);
    }

    // power LED
    const bool on = power.getToggleState();
    const auto led = juce::Rectangle<float> (7.0f, 7.0f).withCentre (ledCentre);
    if (on)
    {
        g.setColour (orangeDeep.withAlpha (0.35f));
        g.fillEllipse (led.expanded (4.0f));
    }
    g.setColour (on ? orange : juce::Colour (0xff3a3833));
    g.fillEllipse (led);
}

void CompchildEditor::timerCallback()
{
    const bool on = power.getToggleState();
    const float gr[2] = { proc.grA.load(), proc.grB.load() };
    bool changed = false;
    for (int i = 0; i < 2; ++i)
    {
        const float target = on ? needleZero - (needleZero - needleRest) * juce::jlimit (0.0f, 1.0f, gr[i] / 20.0f) : needleRest;
        const float next = needle[i] + (target - needle[i]) * 0.28f;     // ~300 ms VU-style ballistics at 30 Hz
        if (std::abs (next - needle[i]) > 0.01f) { needle[i] = next; changed = true; }
    }
    if (changed) repaint (juce::Rectangle<int> (370, 55, 440, 170).transformedBy (juce::AffineTransform::scale ((float) getWidth() / (float) kW)));
}
