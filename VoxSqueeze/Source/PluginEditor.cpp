#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    constexpr int kW = 876, kH = 628, kHeader = 34;

    // panel geometry (panel artwork coordinates; the panel sits under the 34 px header)
    constexpr float tickY[7] = { 98.0f, 167.0f, 230.0f, 286.0f, 342.0f, 402.0f, 456.0f };   // the 7 long tick marks
    constexpr float faderX[3] = { 185.5f, 437.5f, 683.5f };                                  // slot centres
    constexpr float capW = 63.0f, capH = 102.0f;
    constexpr float meterDX = 231.0f - 185.5f;      // meter segment column relative to the slot
    constexpr int   kSegs = 34;
    constexpr float segPitch = 11.06f, segW = 15.0f, segH = 8.0f;
    float segTop (int i) { return 125.0f + (float) (30 - i) * segPitch; }   // i = 0 is the bottom segment
    constexpr float ledY = 523.0f, readoutY = 546.0f;

    const juce::Colour orange (0xffff9a3c), amber (0xffffa53a), iceWhite (0xffcfe3f2), white (0xfff6f8f9),
                       red (0xffff5a3c), engrave (0xffd7dde0), scaleInk (0xffb9c2c6), header (0xff1d2225);

    juce::Image loadPng (const void* data, int size) { return juce::ImageCache::getFromMemory (data, size); }
    float py (float y) { return y + (float) kHeader; }   // panel y -> editor y

    juce::Font mono (float size)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::bold));
    }
    juce::Font sans (float size) { return juce::Font (juce::FontOptions (size, juce::Font::bold)); }

    juce::String dbText (float v)
    {
        return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB";
    }
}

//==============================================================================
double PlateFader::valueToProportionOfLength (double v)
{
    // 7 ticks spaced as engraved; return the position (0 = bottom, 1 = top) along the travel
    const float x = (float) v;
    float y = tickY[6];
    if (x >= vals[0]) y = tickY[0];
    else
        for (int i = 0; i < 6; ++i)
            if (x <= vals[(size_t) i] && x >= vals[(size_t) i + 1])
            {
                y = tickY[i] + (vals[(size_t) i] - x) / (vals[(size_t) i] - vals[(size_t) i + 1]) * (tickY[i + 1] - tickY[i]);
                break;
            }
    return 1.0 - (y - tickY[0]) / (tickY[6] - tickY[0]);
}

double PlateFader::proportionOfLengthToValue (double p)
{
    const float y = tickY[0] + (1.0f - (float) juce::jlimit (0.0, 1.0, p)) * (tickY[6] - tickY[0]);
    for (int i = 0; i < 6; ++i)
        if (y >= tickY[i] && y <= tickY[i + 1])
            return vals[(size_t) i] + (y - tickY[i]) / (tickY[i + 1] - tickY[i]) * (vals[(size_t) i + 1] - vals[(size_t) i]);
    return vals[6];
}

float PlateFader::capCentreY() const
{
    const auto prop = (float) const_cast<PlateFader*> (this)->valueToProportionOfLength (getValue());
    return py (tickY[0] + (1.0f - prop) * (tickY[6] - tickY[0]));
}

//==============================================================================
VoxLookAndFeel::VoxLookAndFeel()
{
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xee12161a));
    setColour (juce::BubbleComponent::outlineColourId,    juce::Colour (0xff3a4348));
    setColour (juce::TooltipWindow::textColourId,         juce::Colour (0xffd5dde1));
}

juce::Slider::SliderLayout VoxLookAndFeel::getSliderLayout (juce::Slider& s)
{
    // the component is taller than the travel by half a cap at each end, so the cap is clickable at the extremes
    juce::Slider::SliderLayout l;
    const float sc = (float) s.getHeight() / (tickY[6] - tickY[0] + capH);
    l.sliderBounds = s.getLocalBounds().reduced (0, juce::roundToInt (capH * 0.5f * sc));
    return l;
}

void VoxLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool)
{
    const auto r = b.getLocalBounds().toFloat();
    g.setColour (b.getToggleState() ? orange : (over ? juce::Colour (0xff2f373c) : juce::Colours::transparentBlack));
    g.fillRoundedRectangle (r, 3.0f);
}

void VoxLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? header : juce::Colour (0xffa3afb5));
    g.setFont (sans ((float) b.getHeight() * 0.55f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void VoxHelpButton::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto b = getLocalBounds().toFloat().reduced (1.5f);
    g.setColour (juce::Colour (0xffd5dde1).withAlpha (over ? 0.95f : 0.6f));
    g.drawEllipse (b, 1.3f);
    g.setFont (sans (b.getHeight() * 0.72f));
    g.drawText ("?", b.translated (0.0f, 0.5f), juce::Justification::centred);
}

//==============================================================================
VoxSqueezeEditor::VoxSqueezeEditor (VoxSqueezeProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    panel = loadPng (BinaryData::panel_png,     BinaryData::panel_pngSize);
    cap   = loadPng (BinaryData::fader_cap_png, BinaryData::fader_cap_pngSize);

    addFader (gate, vids::gate, faderX[0]);
    addFader (comp, vids::comp, faderX[1]);
    addFader (gain, vids::gain, faderX[2]);
    gateAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, vids::gate, gate);
    compAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, vids::comp, comp);
    gainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, vids::gain, gain);
    for (auto* f : { &gate, &comp, &gain })
        if (auto* prm = proc.apvts.getParameter (f == &gate ? vids::gate : f == &comp ? vids::comp : vids::gain))
            f->setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    gate.setTooltip ("Gate: quiet sounds below this level are turned down (bottom = off)");
    comp.setTooltip ("Comp: pull down for more compression; makeup gain follows automatically");
    gain.setTooltip ("Gain: output level into the 0 dBFS limiter");

    for (auto* b : { &monoBtn, &stereoBtn })
    {
        b->setLookAndFeel (&lnf);
        b->setClickingTogglesState (false);
        addAndMakeVisible (b);
    }
    auto setStereo = [this] (bool st)
    {
        if (auto* prm = proc.apvts.getParameter (vids::stereo))
        {
            prm->beginChangeGesture(); prm->setValueNotifyingHost (st ? 1.0f : 0.0f); prm->endChangeGesture();
        }
    };
    monoBtn.onClick   = [setStereo] { setStereo (false); };
    stereoBtn.onClick = [setStereo] { setStereo (true); };
    monoBtn.setTooltip ("Mono: the input is summed to mono");
    stereoBtn.setTooltip ("Stereo: left and right are processed together, keeping the stereo image");

    addAndMakeVisible (help);
    help.setTooltip ("Open the VoxSqueeze manual");
    help.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    help.onClick = [this] { openManual(); };

    setSize (kW, kH);
    setResizable (true, true);
    setResizeLimits (kW / 2, kH / 2, kW * 2, kH * 2);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    startTimerHz (30);
}

VoxSqueezeEditor::~VoxSqueezeEditor()
{
    for (auto* f : { &gate, &comp, &gain }) f->setLookAndFeel (nullptr);
    monoBtn.setLookAndFeel (nullptr); stereoBtn.setLookAndFeel (nullptr);
}

void VoxSqueezeEditor::addFader (PlateFader& f, const char*, float cx)
{
    f.centreX = cx;
    f.setSliderStyle (juce::Slider::LinearVertical);
    f.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    f.setLookAndFeel (&lnf);
    f.setSliderSnapsToMousePosition (false);   // grab and move the cap like a real fader
    f.setMouseDragSensitivity (360);
    f.setPopupDisplayEnabled (true, false, this);
    f.setScrollWheelEnabled (true);
    f.setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    addAndMakeVisible (f);
}

void VoxSqueezeEditor::openManual()
{
    // the manual is built into the plugin; write it out and open it in the default browser
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Herigama Audio").getChildFile ("VoxSqueeze");
    dir.createDirectory();
    auto f = dir.getChildFile ("VoxSqueeze Manual.html");
    if (f.getSize() != (juce::int64) BinaryData::manual_htmlSize)
        f.replaceWithData (BinaryData::manual_html, (size_t) BinaryData::manual_htmlSize);
    f.startAsProcess();
}

void VoxSqueezeEditor::resized()
{
    const float sc = (float) getWidth() / (float) kW;
    auto S = [sc] (juce::Rectangle<float> r) { return r.transformedBy (juce::AffineTransform::scale (sc)).toNearestInt(); };
    for (auto* f : { &gate, &comp, &gain })
        f->setBounds (S ({ f->centreX - 48.0f, py (tickY[0] - capH * 0.5f), 96.0f, tickY[6] - tickY[0] + capH }));
    monoBtn.setBounds   (S ({ 556.0f, 7.0f, 58.0f, 20.0f }));
    stereoBtn.setBounds (S ({ 616.0f, 7.0f, 66.0f, 20.0f }));
    help.setBounds      (S ({ 840.0f, 9.0f, 16.0f, 16.0f }));
}

void VoxSqueezeEditor::paint (juce::Graphics& g)
{
    const float sc = (float) getWidth() / (float) kW;
    g.fillAll (juce::Colour (0xff0b0d0e));
    g.addTransform (juce::AffineTransform::scale (sc));

    // header bar (FL Studio-style plugin title strip)
    g.setColour (header);
    g.fillRect (0.0f, 0.0f, (float) kW, (float) kHeader);
    g.setColour (juce::Colours::black);
    g.fillRect (0.0f, (float) kHeader - 1.0f, (float) kW, 1.0f);
    g.setFont (sans (17.0f));
    g.setColour (juce::Colour (0xfff2f5f6));
    g.drawText ("VOX", 14, 0, 40, kHeader, juce::Justification::centredLeft);
    g.setColour (orange);
    g.drawText ("SQUEEZE", 52, 0, 100, kHeader, juce::Justification::centredLeft);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.setColour (juce::Colour (0xff8f9ba1));
    g.drawText ("one-fader vocal leveler", 140, 0, 220, kHeader, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff12161a));
    g.fillRoundedRectangle (552.0f, 5.0f, 134.0f, 24.0f, 4.0f);

    if (panel.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (panel, juce::Rectangle<float> (0.0f, (float) kHeader, (float) kW, (float) (kH - kHeader)));
    }

    // engraved legends and scale numbers
    const char* names[3] = { "GATE", "COMP", "GAIN" };
    const char* scales[3][7] = { { "0", "10", "20", "30", "40", "60", "OFF" },
                                 { "0", "10", "20", "30", "40", "50", "60" },
                                 { "+10", "+5", "0", "5", "10", "20", "-inf" } };
    const char* meters[3] = { "IN", "GR", "OUT" };
    for (int f = 0; f < 3; ++f)
    {
        const float cx = faderX[f];
        g.setFont (sans (15.0f));
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawText (names[f], juce::Rectangle<float> (cx - 60.0f, py (45.0f), 120.0f, 18.0f), juce::Justification::centred);
        g.setColour (juce::Colour (0xfff2f5f6));
        g.drawText (names[f], juce::Rectangle<float> (cx - 60.0f, py (46.0f), 120.0f, 18.0f), juce::Justification::centred);

        g.setFont (mono (10.0f));
        for (int t = 0; t < 7; ++t)
        {
            const bool unity = f == 2 && t == 2;
            const auto r = juce::Rectangle<float> (cx - 116.0f, py (tickY[t] - 7.0f), 38.0f, 14.0f);
            g.setColour (juce::Colours::black.withAlpha (0.8f));
            g.drawText (scales[f][t], r.translated (0.0f, -1.0f), juce::Justification::centredRight);
            g.setColour (unity ? juce::Colour (0xfff2f5f6) : scaleInk);
            g.drawText (scales[f][t], r, juce::Justification::centredRight);
        }
        g.setFont (sans (9.0f));
        g.setColour (juce::Colour (0xff9aa4a9));
        g.drawText (meters[f], juce::Rectangle<float> (cx + meterDX - 12.0f, py (486.0f), segW + 24.0f, 12.0f), juce::Justification::centred);
    }
    g.setFont (sans (10.0f));
    g.setColour (juce::Colour (0xff8f9ba1));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("AUTO RELEASE  \xc2\xb7  0 dBFS LIMITER")), juce::Rectangle<float> (70.0f, py (566.0f), 300.0f, 14.0f), juce::Justification::centredLeft);
}

void VoxSqueezeEditor::paintOverChildren (juce::Graphics& g)
{
    const float sc = (float) getWidth() / (float) kW;
    g.addTransform (juce::AffineTransform::scale (sc));

    // LED meters over the panel's unlit segments
    auto lit = [&] (int f, int count, bool fromTop, std::function<juce::Colour (int)> colourOf)
    {
        const float x = faderX[f] + meterDX;
        for (int i = 0; i < kSegs; ++i)
        {
            const bool on = fromTop ? (kSegs - 1 - i) < count : i < count;
            if (! on) continue;
            const auto c = colourOf (i);
            const auto r = juce::Rectangle<float> (x, py (segTop (i)), segW, segH);
            g.setColour (c.withAlpha (0.22f)); g.fillRoundedRectangle (r.expanded (2.5f), 2.5f);   // glow
            g.setColour (c);                   g.fillRoundedRectangle (r, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.25f)); g.fillRect (r.withHeight (1.5f).reduced (1.0f, 0.0f));
        }
    };
    auto segsFor = [] (float db) { return juce::jlimit (0, kSegs, juce::roundToInt ((db + 60.0f) / 60.0f * (float) kSegs)); };
    lit (0, segsFor (mIn), false, [] (int) { return iceWhite; });
    lit (1, juce::jlimit (0, kSegs, juce::roundToInt ((mGr + mLim) / 20.0f * (float) kSegs)), true, [] (int) { return amber; });
    lit (2, segsFor (mOut), false, [] (int i) { return i >= kSegs - 2 ? red : white; });

    // fader caps (drawn here so they sit above the slot, with a soft shadow)
    for (auto* f : { &gate, &comp, &gain })
    {
        const auto r = juce::Rectangle<float> (capW, capH).withCentre ({ f->centreX - 1.0f, f->capCentreY() });
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.11f));
            g.fillRoundedRectangle (r.translated (0.0f, 3.0f + (float) i).expanded ((float) i), 5.0f + (float) i);
        }
        if (cap.isValid())
        {
            g.setOpacity (1.0f);   // the shadow colour above would otherwise make the cap translucent
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (cap, r);
        }
    }

    // status LEDs
    const bool gateOpen = mGate < 3.0f, comping = mGr > 1.0f, limiting = mLim > 0.1f;
    auto led = [&] (float cx, bool on, juce::Colour c)
    {
        const auto r = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ cx, py (ledY) });
        if (on)
        {
            for (int i = 3; i >= 1; --i) { g.setColour (c.withAlpha (0.10f * (float) i)); g.fillEllipse (r.expanded (2.5f * (float) (4 - i))); }
            g.setGradientFill (juce::ColourGradient (juce::Colours::white, r.getCentreX() - 1.5f, r.getCentreY() - 2.0f, c, r.getRight(), r.getBottom(), true));
        }
        else
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a4044), r.getCentreX() - 1.5f, r.getCentreY() - 2.0f, juce::Colour (0xff121416), r.getRight(), r.getBottom(), true));
        g.fillEllipse (r);
    };
    led (faderX[0], gateOpen, iceWhite);
    led (faderX[1], comping, orange);
    led (faderX[2], true, limiting ? red : white);

    // readouts under the LEDs
    const float gv = (float) gate.getValue(), cv = (float) comp.getValue(), nv = (float) gain.getValue();
    const juce::String txt[3] = { gv <= -79.9f ? juce::String ("OFF") : dbText (gv), dbText (cv), nv <= -79.9f ? juce::String ("-inf") : dbText (nv) };
    const juce::Colour ink[3] = { juce::Colour (0xffe6eef3), orange, white };
    g.setFont (mono (12.0f));
    for (int f = 0; f < 3; ++f)
    {
        const auto r = juce::Rectangle<float> (faderX[f] - 60.0f, py (readoutY - 7.0f), 120.0f, 16.0f);
        g.setColour (juce::Colours::black); g.drawText (txt[f], r.translated (0.0f, 1.0f), juce::Justification::centred);
        g.setColour (ink[f]);               g.drawText (txt[f], r, juce::Justification::centred);
    }
    g.setFont (mono (10.0f));
    g.setColour (scaleInk);
    g.drawText ("GR -" + juce::String (mGr + mLim, 1) + juce::String (juce::CharPointer_UTF8 (" dB  \xc2\xb7  OUT ")) + juce::String (juce::jmax (-99.9f, mOut), 1) + " dBFS",
                juce::Rectangle<float> ((float) kW - 370.0f, py (566.0f), 300.0f, 14.0f), juce::Justification::centredRight);
}

void VoxSqueezeEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addItem (1, "Open manual...");
        m.addItem (2, "Reset all faders");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ e.getScreenX(), e.getScreenY(), 1, 1 }),
                         [safe = juce::Component::SafePointer<VoxSqueezeEditor> (this)] (int r)
                         {
                             if (safe == nullptr) return;
                             if (r == 1) safe->openManual();
                             if (r == 2)
                                 for (auto* id : { vids::gate, vids::comp, vids::gain })
                                     if (auto* prm = safe->proc.apvts.getParameter (id))
                                     { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getDefaultValue()); prm->endChangeGesture(); }
                         });
    }
}

void VoxSqueezeEditor::timerCallback()
{
    // peak meters: instant attack, ~1.2 s fall from full scale (30 Hz)
    auto fall = [] (float shown, float target, float perFrame) { return target >= shown ? target : juce::jmax (target, shown - perFrame); };
    mIn   = fall (mIn,   proc.inDb.load(),   1.6f);
    mOut  = fall (mOut,  proc.outDb.load(),  1.6f);
    mGr   = proc.grDb.load() >= mGr ? proc.grDb.load() : mGr + (proc.grDb.load() - mGr) * 0.25f;
    mLim  = proc.limDb.load() >= mLim ? proc.limDb.load() : mLim * 0.75f;
    mGate = proc.gateDb.load();

    const bool st = proc.apvts.getRawParameterValue (vids::stereo)->load() > 0.5f;
    stereoBtn.setToggleState (st, juce::dontSendNotification);
    monoBtn.setToggleState (! st, juce::dontSendNotification);
    repaint();
}
