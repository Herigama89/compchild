#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    constexpr int kW = 1200, kH = 400;                                   // panel design size (artwork is 2x)

    // VU meter (from the panel artwork)
    const juce::Rectangle<float> vuRect { 450.0f, 66.0f, 300.0f, 203.0f };
    const juce::Rectangle<float> vuClip { 472.9f, 86.1f, 254.3f, 135.4f };
    const juce::Point<float> vuPivot { 601.83f, 230.18f };
    constexpr float needleLen = 119.8f;
    // meter law of this face: deflection follows voltage, +3 VU at the right stop
    float vuAngle (float vu) { return -53.4f + 105.8f * std::pow (10.0f, vu / 20.0f) / std::pow (10.0f, 3.0f / 20.0f); }
    constexpr float needleRest = -53.4f;
    const juce::Colour needleColour (0xffc81e16);

    // waveform display screen
    const juce::Rectangle<float> scopeRect { 150.0f, 36.0f, 216.0f, 88.0f };

    // controls
    const juce::Point<float> gainC { 338.0f, 236.0f }, peakC { 862.0f, 236.0f }, emphC { 222.0f, 326.0f }, mixC { 600.0f, 326.0f };
    const juce::Point<float> selC { 1030.0f, 150.0f }, toggleC { 152.0f, 236.0f }, lampC { 1030.0f, 290.0f };
    const juce::Point<float> helpC { 392.0f, 44.0f };

    juce::Image loadPng (const void* data, int size) { return juce::ImageCache::getFromMemory (data, size); }
}

//==============================================================================
Opt2aLookAndFeel::Opt2aLookAndFeel()
{
    silver   = loadPng (BinaryData::knob_silver_png, BinaryData::knob_silver_pngSize);
    screw    = loadPng (BinaryData::screw_knob_png,  BinaryData::screw_knob_pngSize);
    selector = loadPng (BinaryData::selector_png,    BinaryData::selector_pngSize);
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xee141210));
    setColour (juce::BubbleComponent::outlineColourId,    juce::Colour (0xff3a3833));
    setColour (juce::TooltipWindow::textColourId,         juce::Colour (0xffe9dfc6));
}

void Opt2aLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s)
{
    const auto kind = (int) s.getProperties()["kind"];
    const float art = (float) s.getProperties()["art"];          // artwork size inside the (larger) hit area
    const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = area.getCentre();
    const auto box = juce::Rectangle<float> (art, art).withCentre (c);

    const juce::Image* img = &silver;
    float angle = -135.0f + 270.0f * pos;
    if (kind == 1) img = &screw;
    if (kind == 2) { img = &selector; angle = -60.0f + 60.0f * (float) juce::roundToInt (pos * 2.0f); }

    if (kind == 0)   // soft drop shadow under the big knobs
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (box.reduced (art * 0.06f).translated (0.0f, art * 0.05f));
    }
    if (img->isValid())
    {
        const auto t = juce::AffineTransform::scale (box.getWidth() / (float) img->getWidth(), box.getHeight() / (float) img->getHeight())
                           .translated (box.getX(), box.getY())
                           .rotated (juce::degreesToRadians (angle), c.x, c.y);
        g.setOpacity (1.0f);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImageTransformed (*img, t);
    }
}

void ToggleSwitch::paintButton (juce::Graphics& g, bool, bool)
{
    const auto& img = getToggleState() ? up : down;   // up = LIMIT
    if (img.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (img, getLocalBounds().toFloat());
    }
}

void HelpButton::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto b = getLocalBounds().toFloat().reduced (1.5f);
    g.setColour (juce::Colour (0xffe2ded5).withAlpha (over ? 0.95f : 0.55f));
    g.drawEllipse (b, 1.3f);
    g.setFont (juce::Font (juce::FontOptions (b.getHeight() * 0.72f, juce::Font::bold)));
    g.drawText ("?", b.translated (0.0f, 0.5f), juce::Justification::centred);
}

//==============================================================================
Opt2aEditor::Opt2aEditor (Opt2aProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    background = juce::ImageCache::getFromMemory (BinaryData::background_jpg, BinaryData::background_jpgSize);
    vuCut      = loadPng (BinaryData::vu_cut_png,      BinaryData::vu_cut_pngSize);
    vuGlass    = loadPng (BinaryData::vu_glass_png,    BinaryData::vu_glass_pngSize);
    scopeGlass = loadPng (BinaryData::scope_glass_png, BinaryData::scope_glass_pngSize);
    lampOn     = loadPng (BinaryData::lamp_on_png,     BinaryData::lamp_on_pngSize);
    lampOff    = loadPng (BinaryData::lamp_off_png,    BinaryData::lamp_off_pngSize);
    limitSwitch.up   = loadPng (BinaryData::toggle_up_png,   BinaryData::toggle_up_pngSize);
    limitSwitch.down = loadPng (BinaryData::toggle_down_png, BinaryData::toggle_down_pngSize);

    addKnob (gain,     ids::gain,     Kind::Big,      gainC.x, gainC.y, 104.0f);
    addKnob (peak,     ids::peak,     Kind::Big,      peakC.x, peakC.y, 104.0f);
    addKnob (emphasis, ids::emphasis, Kind::Screw,    emphC.x, emphC.y, 15.0f);
    addKnob (mix,      ids::mix,      Kind::Screw,    mixC.x,  mixC.y,  15.0f);
    addKnob (meter,    ids::meter,    Kind::Selector, selC.x,  selC.y,  155.0f);
    meter.slider.setMouseDragSensitivity (80);

    addAndMakeVisible (limitSwitch);
    limitSwitch.setTooltip ("Up = LIMIT, down = COMPRESS");
    limitSwitch.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    limitAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::limit, limitSwitch);

    addAndMakeVisible (power);
    power.setTooltip ("Power / bypass");
    power.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::power, power);
    power.onClick = [this] { repaint(); };

    addAndMakeVisible (help);
    help.setTooltip ("Open the OPT-2A manual");
    help.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    help.onClick = [this] { openManual(); };

    setSize (1080, 360);
    setResizable (true, true);
    setResizeLimits (600, 200, 2400, 800);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    startTimerHz (30);
}

Opt2aEditor::~Opt2aEditor()
{
    for (auto* k : { &gain, &peak, &emphasis, &mix, &meter })
        k->slider.setLookAndFeel (nullptr);
}

void Opt2aEditor::addKnob (Knob& k, const char* paramId, Kind kind, float cx, float cy, float art)
{
    auto& s = k.slider;
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setLookAndFeel (&lnf);
    const float hit = kind == Kind::Screw ? 34.0f : kind == Kind::Selector ? 80.0f : art;
    s.getProperties().set ("kind", (int) kind);
    s.getProperties().set ("art", art);
    s.getProperties().set ("cx", cx);
    s.getProperties().set ("cy", cy);
    s.getProperties().set ("hit", hit);
    s.setMouseDragSensitivity (kind == Kind::Big ? 260 : 180);
    s.setPopupDisplayEnabled (true, false, this);
    s.setScrollWheelEnabled (true);
    addAndMakeVisible (s);
    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, s);
    if (auto* prm = proc.apvts.getParameter (paramId))
        s.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
}

void Opt2aEditor::resized()
{
    const float sc = (float) getWidth() / (float) kW;
    const auto S = juce::AffineTransform::scale (sc);
    for (auto* k : { &gain, &peak, &emphasis, &mix, &meter })
    {
        auto& pr = k->slider.getProperties();
        const float cx = pr["cx"], cy = pr["cy"], hit = pr["hit"], art = pr["art"];
        const float size = juce::jmax (hit, art);
        k->slider.setBounds (juce::Rectangle<float> (size, size).withCentre ({ cx, cy }).transformedBy (S).toNearestInt());
        k->slider.getProperties().set ("art", art);
    }
    // selector: the sprite is bigger than the clickable body
    meter.slider.setBounds (juce::Rectangle<float> (155.0f, 155.0f).withCentre (selC).transformedBy (S).toNearestInt());
    limitSwitch.setBounds (juce::Rectangle<float> (56.0f, 56.0f).withCentre (toggleC).transformedBy (S).toNearestInt());
    power.setBounds (juce::Rectangle<float> (64.0f, 64.0f).withCentre (lampC.translated (0.0f, 18.0f)).transformedBy (S).toNearestInt());
    help.setBounds (juce::Rectangle<float> (15.0f, 15.0f).withCentre (helpC).transformedBy (S).toNearestInt());
}

void Opt2aEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141312));
    if (background.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (background, getLocalBounds().toFloat());
    }
}

void Opt2aEditor::drawScope (juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState ss (g);
    juce::Path clip; clip.addRoundedRectangle (scopeRect, 6.0f);
    g.reduceClipRegion (clip);
    const float x0 = scopeRect.getX(), w = scopeRect.getWidth(), h = scopeRect.getHeight();
    const float mid = scopeRect.getY() + h * 0.58f, amp = h * 0.36f;
    const float colW = w / (float) historyLen;
    auto shape = [] (float pk) { return std::pow (juce::jlimit (0.0f, 1.0f, pk), 0.6f); };   // gentle log-ish scale

    juce::RectangleList<float> inBars, outBars;
    juce::Path grLine;
    for (int i = 0; i < historyLen; ++i)
    {
        const auto& f = history[(size_t) ((historyPos + i) % historyLen)];
        const float x = x0 + (float) i * colW;
        const float hi = shape (f.in) * amp, ho = shape (f.out) * amp;
        if (hi > 0.3f) inBars.addWithoutMerging ({ x, mid - hi, colW + 0.15f, hi * 1.85f });
        if (ho > 0.3f) outBars.addWithoutMerging ({ x, mid - ho, colW + 0.15f, ho * 1.85f });
        const float gy = scopeRect.getY() + 5.0f + juce::jlimit (0.0f, 20.0f, f.gr) / 9.0f * h * 0.28f;
        if (i == 0) grLine.startNewSubPath (x, gy); else grLine.lineTo (x, gy);
    }
    g.setColour (juce::Colour (0x78aaaaa5)); g.fillRectList (inBars);
    g.setColour (juce::Colour (0x40ffb048)); for (auto& r : outBars) g.fillRect (r.expanded (0.0f, 1.2f));   // glow
    g.setColour (juce::Colour (0xebffb048)); g.fillRectList (outBars);
    g.setColour (juce::Colour (0x55e63424)); g.strokePath (grLine, juce::PathStrokeType (3.2f));
    g.setColour (juce::Colour (0xffe63424)); g.strokePath (grLine, juce::PathStrokeType (1.4f));
    g.setOpacity (1.0f);
    if (scopeGlass.isValid()) g.drawImage (scopeGlass, scopeRect);
}

void Opt2aEditor::drawMeter (juce::Graphics& g)
{
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (vuClip.toNearestInt());
        const float a = juce::degreesToRadians (needle);
        const juce::Point<float> dir (std::sin (a), -std::cos (a)), nrm (std::cos (a), std::sin (a));
        auto needlePath = [&] (juce::Point<float> o)
        {
            juce::Path p;
            const auto base = vuPivot + o, tip = vuPivot + o + dir * needleLen;
            p.startNewSubPath (base + nrm * 1.4f);
            p.lineTo (tip + nrm * 0.5f);
            p.lineTo (tip - nrm * 0.5f);
            p.lineTo (base - nrm * 1.4f);
            p.closeSubPath();
            return p;
        };
        g.setColour (juce::Colours::black.withAlpha (0.22f)); g.fillPath (needlePath ({ 2.2f, 3.0f }));
        g.setColour (needleColour);                            g.fillPath (needlePath ({ 0.0f, 0.0f }));
    }
    g.setOpacity (1.0f);
    if (vuCut.isValid())   g.drawImage (vuCut, vuRect);     // the cut-out hides the needle root
    if (vuGlass.isValid()) g.drawImage (vuGlass, vuRect);
}

void Opt2aEditor::paintOverChildren (juce::Graphics& g)
{
    const float sc = (float) getWidth() / (float) kW;
    g.addTransform (juce::AffineTransform::scale (sc));
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    drawScope (g);
    drawMeter (g);
    g.setOpacity (1.0f);
    const auto& lamp = power.getToggleState() ? lampOn : lampOff;
    if (lamp.isValid()) g.drawImage (lamp, juce::Rectangle<float> (40.0f, 40.0f).withCentre (lampC));
}

void Opt2aEditor::openManual()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Herigama Audio").getChildFile ("OPT-2A");
    dir.createDirectory();
    auto f = dir.getChildFile ("OPT-2A Manual.html");
    if (f.getSize() != (juce::int64) BinaryData::manual_htmlSize)
        f.replaceWithData (BinaryData::manual_html, (size_t) BinaryData::manual_htmlSize);
    f.startAsProcess();
}

void Opt2aEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addItem (1, "Open manual...");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ e.getScreenX(), e.getScreenY(), 1, 1 }),
                         [safe = juce::Component::SafePointer<Opt2aEditor> (this)] (int r)
                         {
                             if (safe != nullptr && r == 1) safe->openManual();
                         });
    }
}

void Opt2aEditor::settleForSnapshot()
{
    for (int i = 0; i < 90; ++i) timerCallback();
}

void Opt2aEditor::timerCallback()
{
    ScopeFrame f;
    int got = 0;
    while (proc.popScope (f)) { history[(size_t) historyPos] = f; historyPos = (historyPos + 1) % historyLen; ++got; }

    const bool on = power.getToggleState();
    const int mode = juce::roundToInt (proc.apvts.getRawParameterValue (ids::meter)->load());   // 0 +10, 1 GR, 2 +4
    float target = needleRest;
    if (on)
    {
        if (mode == 1) target = vuAngle (-juce::jlimit (0.0f, 23.0f, proc.meterGR.load()));
        else
        {
            const float vu = proc.meterOutDb.load() + (mode == 2 ? 18.0f : 12.0f);   // 0 VU = -18 / -12 dBFS RMS
            target = vuAngle (juce::jlimit (-25.0f, 4.0f, vu));
        }
    }
    const float next = needle + (target - needle) * 0.28f;   // ~300 ms VU ballistics at 30 Hz
    const bool moved = std::abs (next - needle) > 0.01f;
    needle = next;

    const float sc = (float) getWidth() / (float) kW;
    if (moved) repaint (vuRect.expanded (4.0f).transformedBy (juce::AffineTransform::scale (sc)).toNearestInt());
    if (got > 0) repaint (scopeRect.expanded (2.0f).transformedBy (juce::AffineTransform::scale (sc)).toNearestInt());
}
