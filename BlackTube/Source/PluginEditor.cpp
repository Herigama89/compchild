#include "PluginEditor.h"
#include "BlackTubeData.h"

namespace
{
    constexpr int kW = 1700, kH = 620;                  // panel artwork size

    // knob centres and toggle centres on the panel
    const juce::Point<float> kOutput { 405, 125 }, kRatio { 640, 125 }, kThreshold { 876, 125 },
                             kAttack { 405, 340 }, kRelease { 640, 340 }, kParallel { 1304, 542 };
    const juce::Point<float> sMeter { 1110, 120 }, sAr { 876, 340 }, sBus { 1110, 340 }, sGen { 163, 548 }, sCut { 344, 548 };
    constexpr float knobMain = 210.0f, knobSmall = 143.0f, toggleSize = 120.0f;

    // VU meter window and needle geometry (face top-left, pivot below the window)
    const juce::Rectangle<float> vuFace { 1243.0f, 46.0f, 334.0f, 186.0f };
    constexpr float pivotX = 167.0f, pivotY = 204.0f, needleLen = 168.0f, span = 46.0f;
    const juce::Point<float> lampCentre { 1410.0f, 335.0f };
    const juce::Rectangle<float> scopeBox { 75.0f, 179.0f, 228.0f, 222.0f };   // inside the recessed window

    const juce::Colour neon (0xff57ff2a), cream (0xffd2d2ac), ink (0xffe8e4da);

    juce::Image load (const void* d, int n) { return juce::ImageCache::getFromMemory (d, n); }
}

//==============================================================================
BlackTubeLookAndFeel::BlackTubeLookAndFeel()
{
    ringMain  = load (BlackTubeData::knob_ring_main_png,  BlackTubeData::knob_ring_main_pngSize);
    ringSmall = load (BlackTubeData::knob_ring_small_png, BlackTubeData::knob_ring_small_pngSize);
    bodyMain  = load (BlackTubeData::knob_body_main_png,  BlackTubeData::knob_body_main_pngSize);
    bodySmall = load (BlackTubeData::knob_body_small_png, BlackTubeData::knob_body_small_pngSize);
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xee0b0c0d));
    setColour (juce::BubbleComponent::outlineColourId,    juce::Colour (0xff3a3d40));
    setColour (juce::TooltipWindow::textColourId,         ink);
}

void BlackTubeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s)
{
    const bool small = (bool) s.getProperties()["small"];
    const auto box = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = box.getCentre();
    const float S = box.getWidth();
    const float rOut = 0.37f * S, rIn = rOut * 0.9f, rMid = 0.5f * (rOut + rIn), width = rOut - rIn;
    const float angle = -140.0f + 280.0f * pos;
    auto& ring = small ? ringSmall : ringMain;
    auto& body = small ? bodySmall : bodyMain;
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

    // segmented ring (dims as the knob is turned up, like the original design)
    if (ring.isValid())
    {
        g.setOpacity (1.0f - 0.58f * pos);
        g.drawImage (ring, box);
        g.setOpacity (1.0f);
    }

    auto arc = [&] (float a0, float a1)
    {
        juce::Path p;
        p.addCentredArc (c.x, c.y, rMid, rMid, 0.0f, juce::degreesToRadians (a0), juce::degreesToRadians (a1), true);
        return p;
    };
    // cream track for the part not yet lit
    if (angle < 139.5f)
    {
        g.setColour (cream);
        g.strokePath (arc (angle, 140.0f), juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }
    // neon green value arc with glow
    if (angle > -139.5f)
    {
        const auto lit = arc (-140.0f, angle);
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (neon.withAlpha (0.07f * (float) (5 - i)));
            g.strokePath (lit, juce::PathStrokeType (width * (1.0f + 0.9f * (float) i), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (neon);
        g.strokePath (lit, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xffe6ffd6));
        g.strokePath (lit, juce::PathStrokeType (width * 0.32f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // fluted body, turning -125..+125 degrees
    if (body.isValid())
    {
        const float scale = S / (small ? 143.0f : 210.0f);
        const float bw = (float) body.getWidth() * scale, bh = (float) body.getHeight() * scale;
        const auto t = juce::AffineTransform::scale (scale)
                           .translated (c.x - bw * 0.5f, c.y - bh * 0.5f)
                           .rotated (juce::degreesToRadians (angle * 125.0f / 140.0f), c.x, c.y);
        g.drawImageTransformed (body, t);
    }
}

//==============================================================================
ToggleSwitch::ToggleSwitch (juce::AudioProcessorValueTreeState& state, const char* paramId, std::vector<int> imageForChoice)
    : param (state.getParameter (paramId)), imageIndex (std::move (imageForChoice))
{
    images[0] = load (BlackTubeData::toggle_left_png,   BlackTubeData::toggle_left_pngSize);
    images[1] = load (BlackTubeData::toggle_center_png, BlackTubeData::toggle_center_pngSize);
    images[2] = load (BlackTubeData::toggle_right_png,  BlackTubeData::toggle_right_pngSize);
    att = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { choice = juce::roundToInt (v); repaint(); });
    att->sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip (param->getName (64));
}

void ToggleSwitch::paint (juce::Graphics& g)
{
    const int idx = imageIndex[(size_t) juce::jlimit (0, (int) imageIndex.size() - 1, choice)];
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (images[idx], getLocalBounds().toFloat());
}

void ToggleSwitch::mouseDown (const juce::MouseEvent& e)
{
    const int n = (int) imageIndex.size();
    const int next = (choice + (e.mods.isPopupMenu() ? n - 1 : 1)) % n;
    att->setValueAsCompleteGesture ((float) next);
}

//==============================================================================
BlackTubeEditor::BlackTubeEditor (BlackTubeProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      meterSw (p.apvts, btids::meter,    { 0, 1, 2 }),
      arSw    (p.apvts, btids::arSelect, { 0, 1, 2 }),
      busSw   (p.apvts, btids::scBus,    { 0, 2 }),
      genSw   (p.apvts, btids::gen,      { 0, 2 }),
      cutSw   (p.apvts, btids::lowCut,   { 0, 1, 2 })
{
    panel = load (BlackTubeData::panel_jpg, BlackTubeData::panel_jpgSize);

    addKnob (output,    btids::output,    kOutput,    false);
    addKnob (ratio,     btids::ratio,     kRatio,     false);
    addKnob (threshold, btids::threshold, kThreshold, false);
    addKnob (attack,    btids::attack,    kAttack,    false);
    addKnob (release,   btids::release,   kRelease,   false);
    addKnob (parallel,  btids::parallel,  kParallel,  true);
    for (auto* t : { &meterSw, &arSw, &busSw, &genSw, &cutSw }) addAndMakeVisible (t);

    setSize (1190, 434);
    setResizable (true, true);
    setResizeLimits (kW / 2, kH / 2, (int) (kW * 1.25f), (int) (kH * 1.25f));
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

BlackTubeEditor::~BlackTubeEditor()
{
    for (auto* k : { &output, &ratio, &threshold, &attack, &release, &parallel }) k->slider.setLookAndFeel (nullptr);
}

void BlackTubeEditor::addKnob (Knob& k, const char* id, juce::Point<float> centre, bool small)
{
    auto& s = k.slider;
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setRotaryParameters (juce::degreesToRadians (-140.0f), juce::degreesToRadians (140.0f), true);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setLookAndFeel (&lnf);
    s.getProperties().set ("small", small);
    s.getProperties().set ("cx", centre.x);
    s.getProperties().set ("cy", centre.y);
    s.setMouseDragSensitivity (260);
    s.setVelocityBasedMode (false);
    s.setPopupDisplayEnabled (true, false, this);
    s.setScrollWheelEnabled (true);
    addAndMakeVisible (s);
    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, s);
    if (auto* prm = proc.apvts.getParameter (id))
        s.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
}

void BlackTubeEditor::resized()
{
    const auto t = juce::AffineTransform::scale ((float) getWidth() / (float) kW);
    for (auto* k : { &output, &ratio, &threshold, &attack, &release, &parallel })
    {
        auto& pr = k->slider.getProperties();
        const float size = (bool) pr["small"] ? knobSmall : knobMain;
        // the hit area is the knob itself; the drawing covers the full ring
        k->slider.setBounds (juce::Rectangle<float> (size, size).withCentre ({ (float) pr["cx"], (float) pr["cy"] }).transformedBy (t).toNearestInt());
    }
    auto place = [&] (ToggleSwitch& sw, juce::Point<float> c)
    { sw.setBounds (juce::Rectangle<float> (toggleSize, toggleSize).withCentre (c).transformedBy (t).toNearestInt()); };
    place (meterSw, sMeter); place (arSw, sAr); place (busSw, sBus); place (genSw, sGen); place (cutSw, sCut);
}

void BlackTubeEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0e0f10));
    if (panel.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (panel, getLocalBounds().toFloat());
    }
}

void BlackTubeEditor::paintOverChildren (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / (float) kW));
    const bool on = proc.apvts.getRawParameterValue (btids::power)->load() > 0.5f;

    // VU needle, clipped to the window
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (vuFace.toNearestInt());
        const float a = juce::degreesToRadians (-span + needlePos / 141.25f * 2.0f * span);
        const juce::Point<float> pv (vuFace.getX() + pivotX, vuFace.getY() + pivotY), dir (std::sin (a), -std::cos (a));
        const auto tip = pv + dir * needleLen;
        g.setColour (juce::Colour (0x38281400));
        g.drawLine ({ pv.translated (3.0f, 3.0f), tip.translated (3.0f, 4.0f) }, 2.2f);
        g.setColour (juce::Colour (0xff17120f));
        g.drawLine ({ pv, tip }, 1.6f);
    }

    // lamp: the panel shows it lit; darken it when the unit is out
    if (! on)
    {
        const auto r = juce::Rectangle<float> (36.0f, 36.0f).withCentre (lampCentre);
        juce::ColourGradient lg (juce::Colour (0xf02a302a), r.getCentreX() - 3, r.getCentreY() - 4, juce::Colour (0xf00b0e0b), r.getRight(), r.getBottom(), true);
        g.setGradientFill (lg);
        g.fillEllipse (r);
    }

    drawScope (g);
}

void BlackTubeEditor::drawScope (juce::Graphics& g)
{
    const auto b = scopeBox;
    juce::Graphics::ScopedSaveState ss (g);
    juce::Path clip; clip.addRoundedRectangle (b, 8.0f);
    g.reduceClipRegion (clip);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff07120a), b.getX(), b.getY(), juce::Colour (0xff030604), b.getX(), b.getBottom(), false));
    g.fillRect (b);

    const float W = b.getWidth(), H = b.getHeight(), GRH = 44.0f, mid = b.getY() + GRH + (H - GRH) * 0.5f;
    constexpr float floorDb = -42.0f;
    auto hOf = [&] (float db) { return juce::jmax (0.0f, (db - floorDb) / -floorDb) * ((H - GRH) * 0.5f - 4.0f); };

    g.setColour (neon.withAlpha (0.08f));
    for (int i = 0; i <= 6; ++i) g.drawVerticalLine ((int) (b.getX() + W * (float) i / 6.0f), b.getY(), b.getBottom());
    for (float db : { 0.0f, -12.0f, -24.0f, -36.0f })
        for (float sg : { -1.0f, 1.0f }) g.drawHorizontalLine ((int) (mid + sg * hOf (db)), b.getX(), b.getRight());
    g.setColour (neon.withAlpha (0.18f)); g.drawHorizontalLine ((int) mid, b.getX(), b.getRight());
    g.setColour (neon.withAlpha (0.12f)); g.drawHorizontalLine ((int) (b.getY() + GRH - 3), b.getX(), b.getRight());

    // history, oldest on the left, two pixels per column
    const int cols = juce::jmin ((int) (W / 2.0f), BlackTubeProcessor::scopeSize - 1);
    const int w = proc.scopeWrite.load (std::memory_order_acquire);
    auto col = [&] (int i) -> const BlackTubeProcessor::ScopeColumn& { return proc.scope[(w - (cols - 1 - i) + BlackTubeProcessor::scopeSize) % BlackTubeProcessor::scopeSize]; };
    auto area = [&] (bool kept)
    {
        juce::Path p;
        for (int i = 0; i < cols; ++i)
        {
            const float x = b.getX() + (float) i * 2.0f, v = hOf (kept ? col (i).keptDb : col (i).inDb);
            if (i == 0) p.startNewSubPath (x, mid - v); else p.lineTo (x, mid - v);
        }
        for (int i = cols - 1; i >= 0; --i)
            p.lineTo (b.getX() + (float) i * 2.0f, mid + hOf (kept ? col (i).keptDb : col (i).inDb));
        p.closeSubPath();
        return p;
    };
    g.setColour (juce::Colour (0x6bd7cdb9)); g.fillPath (area (false));          // squashed (input)
    const auto kept = area (true);
    g.setColour (neon.withAlpha (0.25f)); g.strokePath (kept, juce::PathStrokeType (4.0f));
    g.setColour (neon.withAlpha (0.55f)); g.fillPath (kept);                     // kept (after gain reduction)

    // threshold
    const float th = proc.apvts.getRawParameterValue (btids::threshold)->load();
    const bool vintage = (int) proc.apvts.getRawParameterValue (btids::gen)->load() == 0;
    const auto arm = (bt::ArMode) (int) proc.apvts.getRawParameterValue (btids::arSelect)->load();
    if (! btparam::thresholdIsOff (th))
    {
        const float T = th + bt::curveFor (vintage, arm).offsetDb;
        const float dash[] = { 4.0f, 4.0f };
        g.setColour (juce::Colour (0xbfffbe50));
        for (float sg : { -1.0f, 1.0f })
        {
            const float yy = mid + sg * hOf (T);
            g.drawDashedLine ({ b.getX(), yy, b.getRight(), yy }, dash, 2, 1.0f);
        }
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.drawText ("THR " + juce::String (juce::roundToInt (T)), juce::Rectangle<float> (b.getRight() - 64, mid - hOf (T) - 14, 60, 12), juce::Justification::centredRight);
    }

    // gain reduction trace in the top strip
    juce::Path grp;
    for (int i = 0; i < cols; ++i)
    {
        const float x = b.getX() + (float) i * 2.0f, y = b.getY() + 18.0f + juce::jmin (1.0f, -col (i).grDb / 20.0f) * (GRH - 22.0f);
        if (i == 0) grp.startNewSubPath (x, y); else grp.lineTo (x, y);
    }
    g.setColour (juce::Colour (0x40ff6e3c)); g.strokePath (grp, juce::PathStrokeType (4.0f));
    g.setColour (juce::Colour (0xf2ff6e3c)); g.strokePath (grp, juce::PathStrokeType (1.6f));

    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.setColour (juce::Colour (0xf2ff8250));
    g.drawText ("GR " + juce::String (col (cols - 1).grDb, 1) + " dB", juce::Rectangle<float> (b.getX() + 6, b.getY() + 2, 100, 14), juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xccd7cdb9));
    g.drawText ("SQUASHED", juce::Rectangle<float> (b.getRight() - 70, b.getY() + 2, 65, 14), juce::Justification::centredRight);
    g.setColour (neon);
    g.drawText ("KEPT", juce::Rectangle<float> (b.getRight() - 112, b.getY() + 2, 38, 14), juce::Justification::centredRight);
}

void BlackTubeEditor::mouseDown (const juce::MouseEvent& e)
{
    // the lamp is the In / bypass switch
    const auto pnt = e.position / ((float) getWidth() / (float) kW);
    if (pnt.getDistanceFrom (lampCentre) < 26.0f)
        if (auto* prm = proc.apvts.getParameter (btids::power))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
            prm->endChangeGesture();
            repaint();
        }
}

void BlackTubeEditor::timerCallback()
{
    // VU ballistics: ~300 ms rise with a little overshoot; 0 VU = -18 dBFS RMS (a sine at -15 dBFS peak)
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.001, 0.05, (now - lastTick) * 0.001);
    lastTick = now;
    const bool on = proc.apvts.getRawParameterValue (btids::power)->load() > 0.5f;
    const int mode = (int) proc.apvts.getRawParameterValue (btids::meter)->load();
    float vu = -60.0f;
    if (mode == 0)      vu = proc.inDb.load()  + 15.0f;
    else if (mode == 2) vu = proc.outDb.load() + 15.0f;
    else                vu = on ? -proc.grDb.load() : 0.0f;          // compression: rests on 0, swings left
    const float target = juce::jlimit (0.0f, 150.0f, 100.0f * std::pow (10.0f, vu / 20.0f));
    constexpr float W = 2.0f * 3.14159265f * 2.1f, Z = 0.78f;
    needleVel += (W * W * (target - needlePos) - 2.0f * Z * W * needleVel) * dt;
    needlePos += needleVel * dt;
    if (needlePos < 0.0f)   { needlePos = 0.0f; needleVel = juce::jmax (0.0f, needleVel) * -0.2f; }
    if (needlePos > 150.0f) { needlePos = 150.0f; needleVel = 0.0f; }
    const auto t = juce::AffineTransform::scale ((float) getWidth() / (float) kW);
    repaint (vuFace.expanded (4.0f).transformedBy (t).getSmallestIntegerContainer());
    repaint (scopeBox.expanded (2.0f).transformedBy (t).getSmallestIntegerContainer());
    repaint (juce::Rectangle<float> (44.0f, 44.0f).withCentre (lampCentre).transformedBy (t).getSmallestIntegerContainer());
}
