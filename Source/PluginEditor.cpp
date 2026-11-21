#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    constexpr int kW = 1176, kH = 564;

    // VU meters: face top-left corners, needle pivot (centre of the dome), needle length
    constexpr float vuLeft[2] = { 378.0f, 593.0f };
    constexpr float vuTop = 60.0f, pivotX = 102.5f, pivotY = 114.7f, needleLen = 79.0f;
    constexpr float faceScale = 205.0f / 486.0f;              // face artwork coordinates -> editor

    // VU law: deflection follows voltage, +3 VU = full scale, sweep -50..+50 degrees
    float vuAngle (float vu) { return -50.0f + 100.0f * std::pow (10.0f, vu / 20.0f) / std::pow (10.0f, 3.0f / 20.0f); }
    constexpr float needleRest = -50.0f;                       // left stop (power off)

    // glass window of the meter face (same shape the face artwork was cleaned with)
    juce::Path glassPath (float left)
    {
        juce::Path p;
        auto px = [&] (float x) { return left + x * faceScale; };
        auto py = [&] (float y) { return vuTop + y * faceScale; };
        p.startNewSubPath (px (46.0f), py (262.0f));
        for (int i = 0; i <= 60; ++i)
        {
            const float x = 46.0f + (440.0f - 46.0f) * (float) i / 60.0f, t = (x - 243.0f) / 203.0f;
            p.lineTo (px (x), py (40.0f + 92.0f * t * t + 4.0f));
        }
        p.lineTo (px (440.0f), py (262.0f));
        p.closeSubPath();
        return p;
    }
    // dome sprite position inside the face (artwork made at 4x the 205x161 face)
    const juce::Rectangle<float> domeBox { 357.0f / 4.0f, 406.0f / 4.0f, 105.0f / 4.0f, 54.0f / 4.0f };

    // LINK (bottom left) and POWER (bottom right): button centres and the jewel lamps above them
    // (the lamp and its word are the clickable area)
    const juce::Point<float> linkLamp  { 188.4f, 444.4f }, powerLamp { 991.6f, 444.4f };
    const juce::Rectangle<float> hitArea { 70.0f, 50.0f };   // centred on lamp + label

    const juce::Colour orange (0xffffb020), orangeDeep (0xffff7a00), lampRed (0xffff2a12);

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
        angleDeg = -90.0f + 60.0f * (float) idx - 15.0f;     // L/R, LAT/VERT, MID/SIDE
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
void PanelButton::paintButton (juce::Graphics& g, bool over, bool)
{
    // nothing to draw: the lamp (editor) and the engraved word (panel) are the button
    if (over)
    {
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);
    }
}

//==============================================================================
CompchildEditor::CompchildEditor (CompchildProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    background = loadPng (BinaryData::background_png, BinaryData::background_pngSize);
    dome       = loadPng (BinaryData::vu_dome_png,    BinaryData::vu_dome_pngSize);
    glass      = loadPng (BinaryData::vu_glass_png,   BinaryData::vu_glass_pngSize);
    blackFace  = loadPng (BinaryData::vu_face_black_png, BinaryData::vu_face_black_pngSize);
    skin = (int) proc.apvts.state.getProperty ("meterSkin", 0) == 1 ? Skin::Black : Skin::Cream;
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
    link.onClick = [this] { if (link.getToggleState()) copyAtoB(); repaint(); };
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
    power.setBounds (hitArea.withCentre (powerLamp.translated (0.0f, 12.0f)).transformedBy (juce::AffineTransform::scale (sc)).toNearestInt());
    link.setBounds  (hitArea.withCentre (linkLamp.translated  (0.0f, 12.0f)).transformedBy (juce::AffineTransform::scale (sc)).toNearestInt());
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

    // VU needles: pivot under the dome, tapered, with a soft shadow on the paper, clipped to the glass
    const bool black = skin == Skin::Black;
    for (int i = 0; i < 2; ++i)
    {
        if (black && blackFace.isValid())   // black skin: the whole meter face is replaced
        {
            juce::Graphics::ScopedSaveState fs (g);
            juce::Path r; r.addRoundedRectangle (vuLeft[i], vuTop, 205.0f, 161.0f, 10.0f);
            g.reduceClipRegion (r);
            g.drawImage (blackFace, juce::Rectangle<float> (vuLeft[i], vuTop, 205.0f, 161.0f));
        }
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (glassPath (vuLeft[i]));
        const juce::Point<float> pivot (vuLeft[i] + pivotX, vuTop + pivotY);
        const float a = juce::degreesToRadians (needle[i]);
        const juce::Point<float> dir (std::sin (a), -std::cos (a)), nrm (std::cos (a), std::sin (a));
        auto needlePath = [&] (juce::Point<float> o)
        {
            juce::Path p;
            const auto base = pivot + o, tip = pivot + o + dir * needleLen;
            p.startNewSubPath (base + nrm * 1.6f);
            p.lineTo (tip + nrm * 0.45f);
            p.lineTo (tip - nrm * 0.45f);
            p.lineTo (base - nrm * 1.6f);
            p.closeSubPath();
            return p;
        };
        if (black)
        {
            g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillPath (needlePath ({ 2.0f, 2.8f }));
            g.setColour (juce::Colour (0xffeae6dc));              g.fillPath (needlePath ({ 0.0f, 0.0f }));   // light needle on black
        }
        else
        {
            g.setColour (juce::Colours::black.withAlpha (0.10f)); g.fillPath (needlePath ({ 2.6f, 3.4f }));
            g.setColour (juce::Colours::black.withAlpha (0.16f)); g.fillPath (needlePath ({ 1.6f, 2.2f }));
            g.setColour (juce::Colour (0xff17120d));              g.fillPath (needlePath ({ 0.0f, 0.0f }));   // dark needle on cream
        }
        if (dome.isValid())
            g.drawImage (dome, domeBox.translated (vuLeft[i], vuTop));
        if (glass.isValid())   // the glass sits in front of the needle
            g.drawImage (glass, juce::Rectangle<float> (vuLeft[i], vuTop, 205.0f, 161.0f));
    }

    // jewel lamps above LINK and POWER
    auto lamp = [&] (juce::Point<float> c, bool lit)
    {
        const auto r = juce::Rectangle<float> (12.0f, 12.0f).withCentre (c);
        if (lit)
        {
            for (int i = 3; i >= 1; --i) { g.setColour (lampRed.withAlpha (0.10f * (float) i)); g.fillEllipse (r.expanded (2.5f * (float) (4 - i))); }
            juce::ColourGradient jg (juce::Colour (0xffffb4a4), c.x - 1.5f, c.y - 2.0f, juce::Colour (0xffc4140a), r.getRight(), r.getBottom(), true);
            g.setGradientFill (jg);
        }
        else
        {
            juce::ColourGradient jg (juce::Colour (0xff5a1a14), c.x - 1.5f, c.y - 2.0f, juce::Colour (0xff1a0806), r.getRight(), r.getBottom(), true);
            g.setGradientFill (jg);
        }
        g.fillEllipse (r);
        g.setColour (juce::Colours::white.withAlpha (lit ? 0.55f : 0.18f));
        g.fillEllipse (juce::Rectangle<float> (3.0f, 2.2f).withCentre (c.translated (-2.0f, -2.6f)));
    };
    lamp (linkLamp,  link.getToggleState());
    lamp (powerLamp, power.getToggleState());
}

void CompchildEditor::setSkin (Skin s)
{
    skin = s;
    proc.apvts.state.setProperty ("meterSkin", (int) s, nullptr);   // saved with the project
    repaint();
}

void CompchildEditor::mouseDown (const juce::MouseEvent& e)
{
    const float sc = (float) getWidth() / (float) kW;
    const auto p = e.position / sc;
    const bool onMeter = juce::Rectangle<float> (vuLeft[0], vuTop, 205.0f, 161.0f).contains (p)
                      || juce::Rectangle<float> (vuLeft[1], vuTop, 205.0f, 161.0f).contains (p);

    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addSectionHeader ("Meter skin");
        m.addItem (1, "Cream", true, skin == Skin::Cream);
        m.addItem (2, "Black", true, skin == Skin::Black);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ e.getScreenX(), e.getScreenY(), 1, 1 }),
                         [safe = juce::Component::SafePointer<CompchildEditor> (this)] (int r)
                         {
                             if (safe != nullptr && r > 0) safe->setSkin (r == 2 ? Skin::Black : Skin::Cream);
                         });
    }
    else if (onMeter)
        setSkin (skin == Skin::Cream ? Skin::Black : Skin::Cream);
}

void CompchildEditor::timerCallback()
{
    const bool on = power.getToggleState();
    const float gr[2] = { proc.grA.load(), proc.grB.load() };
    bool changed = false;
    for (int i = 0; i < 2; ++i)
    {
        // reads gain reduction on the VU scale: 0 GR = "0", 3 dB = "-3", 10 dB = "-10" ...
        const float target = on ? vuAngle (-juce::jlimit (0.0f, 23.0f, gr[i])) : needleRest;
        const float next = needle[i] + (target - needle[i]) * 0.28f;     // ~300 ms VU-style ballistics at 30 Hz
        if (std::abs (next - needle[i]) > 0.01f) { needle[i] = next; changed = true; }
    }
    if (changed) repaint (juce::Rectangle<int> (370, 55, 440, 170).transformedBy (juce::AffineTransform::scale ((float) getWidth() / (float) kW)));
}
