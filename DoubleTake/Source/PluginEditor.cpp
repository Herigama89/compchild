#include "PluginEditor.h"
#include "BinaryData.h"
#include <complex>

namespace
{
    constexpr int kW = 1180, kH = 760;
    const juce::Colour bg (0xff2a3136), headerC (0xff1d2225), panelC (0xff353d43), panelHi (0xff435057), panelLo (0xff1b2023),
                       deep (0xff12161a), boxC (0xff1d2225), ink (0xffd5dde1), muted (0xffa3afb5), faint (0xff7d8a91),
                       bright (0xfff2f5f6), orange (0xffff9a3c), grid (0xff2c353a), gridMid (0xff46545b);
    const juce::Colour voiceCol[4] = { juce::Colour (0xffff9a3c), juce::Colour (0xff7fc4e6), juce::Colour (0xffb99cf0), juce::Colour (0xff8fd16a) };

    juce::Font sans (float h, bool bold = true) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
    juce::Font mono (float h) { return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::plain)); }
    juce::String u8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }

    // console fader scale: value (dB) -> position 0 (top) .. 200 (bottom)
    const float consolePts[9][2] = { { 10, 0 }, { 5, 22 }, { 0, 46 }, { -5, 70 }, { -10, 94 }, { -20, 128 }, { -30, 152 }, { -40, 172 }, { -80, 200 } };
    float consoleY (float v)
    {
        if (v >= 10.0f) return 0.0f;
        for (int i = 0; i < 8; ++i)
            if (v <= consolePts[i][0] && v >= consolePts[i + 1][0])
                return consolePts[i][1] + (consolePts[i][0] - v) / (consolePts[i][0] - consolePts[i + 1][0]) * (consolePts[i + 1][1] - consolePts[i][1]);
        return 200.0f;
    }
    float consoleV (float y)
    {
        y = juce::jlimit (0.0f, 200.0f, y);
        for (int i = 0; i < 8; ++i)
            if (y >= consolePts[i][1] && y <= consolePts[i + 1][1])
                return consolePts[i][0] + (y - consolePts[i][1]) / (consolePts[i + 1][1] - consolePts[i][1]) * (consolePts[i + 1][0] - consolePts[i][0]);
        return -80.0f;
    }

    void drawPanel (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (panelLo); g.fillRoundedRectangle (r.translated (0, 1), 8.0f);
        g.setColour (panelC);  g.fillRoundedRectangle (r, 8.0f);
        g.setColour (panelHi); g.drawLine (r.getX() + 6, r.getY() + 0.5f, r.getRight() - 6, r.getY() + 0.5f, 1.0f);
    }
    void label (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Colour c = muted, float h = 10.5f)
    {
        g.setColour (c); g.setFont (sans (h)); g.drawText (t, r, juce::Justification::centred);
    }
}

using namespace dtui;

//==============================================================================
ConsoleFader::ConsoleFader()
{
    setSliderStyle (juce::Slider::LinearVertical);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setSliderSnapsToMousePosition (false);
    setMouseDragSensitivity (300);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}
double ConsoleFader::valueToProportionOfLength (double v) { return 1.0 - consoleY ((float) v) / 200.0; }
double ConsoleFader::proportionOfLengthToValue (double p) { return consoleV ((float) ((1.0 - p) * 200.0)); }

//==============================================================================
dtui::LookAndFeel::LookAndFeel()
{
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xee12161a));
    setColour (juce::BubbleComponent::outlineColourId, juce::Colour (0xff3a4348));
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf012161a));
    setColour (juce::TooltipWindow::textColourId, ink);
    setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0xff3a4348));
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1d2225));
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, orange);
    setColour (juce::PopupMenu::highlightedTextColourId, headerC);
}

juce::Font dtui::LookAndFeel::getSliderPopupFont (juce::Slider&) { return sans (13.0f); }

juce::Slider::SliderLayout dtui::LookAndFeel::getSliderLayout (juce::Slider& s)
{
    juce::Slider::SliderLayout l;
    l.sliderBounds = s.getLocalBounds();
    if (dynamic_cast<ConsoleFader*> (&s) != nullptr)
    {
        const int pad = juce::roundToInt ((float) s.getHeight() * 20.0f / 240.0f);
        l.sliderBounds = s.getLocalBounds().withTrimmedTop (pad).withTrimmedBottom (pad);
    }
    return l;
}

void dtui::LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s)
{
    const auto col = juce::Colour ((juce::uint32) (int) s.getProperties().getWithDefault ("col", (int) orange.getARGB()));
    const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);

    if ((bool) s.getProperties().getWithDefault ("box", false))
    {
        // number box: drag up / down
        const auto r = area.reduced (0.5f);
        g.setColour (boxC); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (s.isMouseOverOrDragging() ? col.withAlpha (0.6f) : deep); g.drawRoundedRectangle (r, 3.0f, 1.0f);
        g.setColour (col); g.setFont (mono (12.0f));
        g.drawText (s.getTextFromValue (s.getValue()), r, juce::Justification::centred);
        return;
    }

    const float sz = juce::jmin (area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    const float k = sz / 60.0f, rad = 23.0f * k, sw = 6.0f * k;
    const float a0 = juce::MathConstants<float>::pi * -0.75f, a1 = juce::MathConstants<float>::pi * 0.75f;
    const float ang = a0 + pos * (a1 - a0);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zero = bipolar ? a0 + (float) s.valueToProportionOfLength (0.0) * (a1 - a0) : a0;

    juce::Path track; track.addCentredArc (c.x, c.y, rad, rad, 0.0f, a0, a1, true);
    g.setColour (juce::Colour (0xff1a1f22)); g.strokePath (track, juce::PathStrokeType (sw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (std::abs (ang - zero) > 0.01f)
    {
        juce::Path val; val.addCentredArc (c.x, c.y, rad, rad, 0.0f, juce::jmin (zero, ang), juce::jmax (zero, ang), true);
        g.setColour (col); g.strokePath (val, juce::PathStrokeType (sw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (juce::Colour (0xff3e474d)); g.fillEllipse (juce::Rectangle<float> (32.0f * k, 32.0f * k).withCentre (c));
    g.setColour (juce::Colour (0xff1b2023)); g.drawEllipse (juce::Rectangle<float> (32.0f * k, 32.0f * k).withCentre (c), 2.0f * k);
    const juce::Point<float> p0 = c.getPointOnCircumference (5.0f * k, ang), p1 = c.getPointOnCircumference (14.0f * k, ang);
    g.setColour (bright); g.drawLine ({ p0, p1 }, 3.5f * k);
}

void dtui::LookAndFeel::drawLinearSlider (juce::Graphics& g, int, int, int w, int h, float sliderPos, float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    // console fader, drawn in a 66 x 240 box
    const float k = (float) h / 240.0f;
    const auto capLine = juce::Colour ((juce::uint32) (int) s.getProperties().getWithDefault ("col", (int) 0xffffffff));
    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::scale (k).translated (((float) w - 66.0f * k) * 0.5f, 0.0f));
    struct T { float y; const char* t; bool major, unity; };
    const T ticks[] = { { 0, "+10", true, false }, { 11, nullptr, false, false }, { 22, "+5", true, false }, { 34, nullptr, false, false },
                        { 46, "0", true, true }, { 58, nullptr, false, false }, { 70, "5", true, false }, { 82, nullptr, false, false },
                        { 94, "10", true, false }, { 128, "20", true, false }, { 152, "30", true, false }, { 172, "40", true, false }, { 200, "-inf", true, false } };
    g.setFont (mono (7.5f));
    for (auto& t : ticks)
    {
        const float y = 20.0f + t.y;
        if (t.t != nullptr)
        {
            g.setColour (t.unity ? juce::Colours::white : juce::Colour (0xffa9b6bc));
            g.drawText (t.t, juce::Rectangle<float> (0.0f, y - 5.0f, 18.0f, 10.0f), juce::Justification::centredRight);
        }
        g.setColour (t.unity ? juce::Colours::white : juce::Colour (0xff8c9aa1));
        const float th = t.unity ? 1.5f : 1.0f;
        g.fillRect (t.unity ? 20.0f : (t.major ? 20.0f : 22.0f), y - th * 0.5f, t.unity ? 10.0f : (t.major ? 7.0f : 5.0f), th);
        if (t.major) g.fillRect (t.unity ? 54.0f : 57.0f, y - th * 0.5f, t.unity ? 10.0f : 6.0f, th);
    }
    g.setColour (juce::Colour (0xff06090b)); g.fillRoundedRectangle (38.0f, 12.0f, 8.0f, 216.0f, 4.0f);
    g.setColour (juce::Colour (0xff4f5e66)); g.drawRoundedRectangle (38.0f, 12.0f, 8.0f, 216.0f, 4.0f, 0.75f);
    g.setColour (juce::Colour (0xff1e2529)); g.fillRoundedRectangle (41.0f, 16.0f, 2.0f, 208.0f, 1.0f);

    // cap
    const float cy = sliderPos / k - 21.0f;
    const juce::Rectangle<float> cap (28.0f, cy, 28.0f, 42.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (cap.translated (1.0f, 4.0f), 3.0f);
    {
        juce::ColourGradient body (juce::Colour (0xff4a4a4a), 0, cap.getY(), juce::Colour (0xff0e0e0e), 0, cap.getBottom(), false);
        body.addColour (0.5, juce::Colour (0xff1c1c1c));
        g.setGradientFill (body); g.fillRoundedRectangle (cap, 3.0f);
        juce::ColourGradient side (juce::Colours::black.withAlpha (0.5f), cap.getX(), 0, juce::Colours::black.withAlpha (0.5f), cap.getRight(), 0, false);
        side.addColour (0.15, juce::Colours::transparentBlack); side.addColour (0.85, juce::Colours::transparentBlack);
        g.setGradientFill (side); g.fillRoundedRectangle (cap, 3.0f);
    }
    g.setColour (juce::Colour (0xff050505)); g.drawRoundedRectangle (cap, 3.0f, 1.0f);
    g.setColour (juce::Colour (0xb38a8a8a)); g.fillRoundedRectangle (cap.getX() + 2, cap.getY() + 1, 24, 2, 1);
    for (float ry : { 7.0f, 11.0f, 15.0f, 27.0f, 31.0f, 35.0f })
    {
        g.setColour (juce::Colour (0xff5a5a5a)); g.fillRect (cap.getX() + 3, cap.getY() + ry, 22.0f, 1.0f);
        g.setColour (juce::Colours::black);      g.fillRect (cap.getX() + 3, cap.getY() + ry + 1, 22.0f, 1.0f);
    }
    g.setColour (capLine); g.fillRect (cap.getX() + 1.5f, cap.getY() + 19.5f, 25.0f, 3.0f);
}

void dtui::LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat();
    const bool lit = b.getToggleState() && (bool) b.getProperties().getWithDefault ("lights", true);
    auto c = lit ? orange : juce::Colour ((juce::uint32) (int) b.getProperties().getWithDefault ("bg", (int) 0xff2f373c));
    if (! lit && over) c = c.brighter (0.15f);
    if (down) c = c.darker (0.15f);
    g.setColour (c); g.fillRoundedRectangle (r, 3.0f);
}

void dtui::LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool lit = b.getToggleState() && (bool) b.getProperties().getWithDefault ("lights", true);
    g.setColour (lit ? headerC : juce::Colour ((juce::uint32) (int) b.getProperties().getWithDefault ("ink", (int) muted.getARGB())));
    g.setFont (sans (juce::jmin (12.0f, (float) b.getHeight() * 0.48f)));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void dtui::LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (juce::Colour (0xff1d2225));
    g.setColour (juce::Colour (0xff3a4348)); g.drawRect (0, 0, w, h);
}

void Badge::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto r = getLocalBounds().toFloat().reduced (1.5f);
    const bool on = getToggleState();
    if (on) { g.setColour (col); g.fillEllipse (r); }
    g.setColour (over ? col.brighter (0.3f) : col); g.drawEllipse (r, 2.0f);
    if (text.isNotEmpty())
    {
        g.setColour (on ? headerC : muted); g.setFont (sans (r.getHeight() * 0.48f));
        g.drawText (text, r, juce::Justification::centred);
    }
}

void HelpButton::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto b = getLocalBounds().toFloat().reduced (1.5f);
    g.setColour (muted.withAlpha (over ? 1.0f : 0.75f));
    g.drawEllipse (b, 1.4f);
    g.setFont (sans (b.getHeight() * 0.7f));
    g.drawText ("?", b.translated (0.0f, 0.5f), juce::Justification::centred);
}

//==============================================================================
juce::Rectangle<float> Display::plot() const
{
    // a 330 x 160 drawing area, centred
    return juce::Rectangle<float> (330.0f, 160.0f).withCentre (getLocalBounds().toFloat().getCentre());
}

void Display::paint (juce::Graphics& g)
{
    const auto box = getLocalBounds().toFloat();
    g.setColour (deep); g.fillRoundedRectangle (box, 6.0f);
    g.setColour (juce::Colour (0xff0b0e10)); g.drawRoundedRectangle (box.reduced (0.5f), 6.0f, 1.0f);
    const auto P = plot();
    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::translation (P.getX(), P.getY()));
    auto hline = [&] (float y, juce::Colour c) { g.setColour (c); g.drawHorizontalLine ((int) y, 20.0f, 300.0f); };
    auto vline = [&] (float x, float y0, float y1) { g.setColour (grid); g.drawVerticalLine ((int) x, y0, y1); };
    auto txt = [&] (const juce::String& t, float x, float y, juce::Justification j = juce::Justification::centredLeft, float w = 30.0f)
    {
        g.setColour (faint); g.setFont (mono (8.0f));
        const float x0 = j == juce::Justification::centred ? x - w * 0.5f : x;
        g.drawText (t, juce::Rectangle<float> (x0, y - 6.0f, w, 10.0f), j);
    };
    const bool four = pv ("four") > 0.5f;
    const int nv = four ? 4 : 2;

    if (kind == stage)
    {
        for (float y : { 30.0f, 60.0f, 90.0f, 120.0f }) hline (y, grid);
        for (float x : { 20.0f, 90.0f, 160.0f, 230.0f, 300.0f }) vline (x, 20.0f, 140.0f);
        txt ("10", 304, 121); txt ("40", 304, 91); txt ("70", 304, 61); txt ("100 ms", 304, 31);
        g.setFont (sans (9.0f)); g.setColour (faint);
        g.drawText ("L", juce::Rectangle<float> (16, 146, 10, 12), juce::Justification::centred);
        g.drawText ("C", juce::Rectangle<float> (155, 146, 10, 12), juce::Justification::centred);
        g.drawText ("R", juce::Rectangle<float> (294, 146, 10, 12), juce::Justification::centred);
        juce::Path tri; tri.addTriangle (150, 146, 160, 136, 170, 146);
        g.setColour (muted); g.fillPath (tri);
        auto dot = [&] (float x, float y, juce::Colour c, float a)
        {
            const auto r = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ x, y });
            g.setColour (c.withAlpha (a * 0.25f)); g.fillEllipse (r.expanded (3.0f));
            g.setColour (c.withAlpha (a)); g.fillEllipse (r);
            g.setColour (deep); g.drawEllipse (r, 2.0f);
        };
        dot (160.0f + pv ("dryPan") * 1.4f, 136.0f, bright, pv ("dryOn") > 0.5f ? 1.0f : 0.2f);
        for (int i = 1; i <= 4; ++i)
        {
            const bool on = i <= nv && pv (dtp::v (i, "on")) > 0.5f;
            dot (160.0f + pv (dtp::v (i, "pan")) * 1.4f, 136.0f - pv (dtp::v (i, "delay")) / 100.0f * 120.0f, voiceCol[i - 1], on ? 1.0f : 0.15f);
        }
    }
    else if (kind == timePitch)
    {
        for (int i = 0; i < 6; ++i) vline (20.0f + (float) i * 56.0f, 16.0f, 136.0f);
        for (int k = 0; k < 5; ++k) hline (16.0f + 30.0f * (float) k, k == 2 ? gridMid : grid);
        for (int i = 0; i < 6; ++i) txt (juce::String (i * 20), 20.0f + (float) i * 56.0f, 151.0f, juce::Justification::centred);
        txt ("+50", 304, 17); txt ("0", 304, 77); txt (u8 ("\xe2\x88\x92" "50"), 304, 137);
        for (int i = 1; i <= 4; ++i)
        {
            const bool on = i <= nv && pv (dtp::v (i, "on")) > 0.5f;
            const auto c = voiceCol[i - 1];
            const float x = 20.0f + pv (dtp::v (i, "delay")) / 100.0f * 280.0f;
            const float cents = pv (dtp::v (i, "detune")), depth = pv (dtp::v (i, "depth"));
            auto yOf = [] (float ct) { return 76.0f - juce::jlimit (-50.0f, 50.0f, ct) / 50.0f * 60.0f; };
            const float top = yOf (cents + depth), bot = yOf (cents - depth);
            g.setColour (c.withAlpha (on ? 0.35f : 0.08f)); g.fillRoundedRectangle (x - 2.0f, top, 4.0f, juce::jmax (0.5f, bot - top), 2.0f);
            const float now = cents;   // dots stay where they are set; the bar shows the wobble range
            const auto r = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ x, yOf (now) });
            g.setColour (c.withAlpha (on ? 1.0f : 0.15f)); g.fillEllipse (r);
            g.setColour (deep); g.drawEllipse (r, 2.0f);
            const int oct = juce::roundToInt (pv (dtp::v (i, "oct")));
            if (oct != 0 && on)
            {
                g.setColour (c); g.setFont (sans (9.0f));
                g.drawText (oct > 0 ? "+8va" : "-8va", juce::Rectangle<float> (x + 8.0f, yOf (now) - 14.0f, 30.0f, 10.0f), juce::Justification::centredLeft);
            }
        }
    }
    else
    {
        auto fx = [] (float f) { return 20.0f + std::log (f / 20.0f) / std::log (1000.0f) * 280.0f; };
        auto xf = [] (float x) { return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, (x - 20.0f) / 280.0f)); };
        auto gy = [] (float db) { return 76.0f - db / 18.0f * 60.0f; };
        for (float f : { 50.0f, 150.0f, 500.0f, 1500.0f, 5000.0f, 15000.0f }) vline (fx (f), 16.0f, 136.0f);
        for (int k = 0; k < 5; ++k) hline (16.0f + 30.0f * (float) k, k == 2 ? gridMid : grid);
        const char* fl[] = { "50", "150", "500", "1.5k", "5k", "15k" };
        const float fv[] = { 50, 150, 500, 1500, 5000, 15000 };
        for (int i = 0; i < 6; ++i) txt (fl[i], fx (fv[i]), 151.0f, juce::Justification::centred);
        txt ("+18", 304, 17); txt ("0", 304, 77); txt (u8 ("\xe2\x88\x92" "18"), 304, 137);

        dt::Shelf lo, hi;
        const double fs = 48000.0;
        lo.set (false, fs, pv ("lowFreq"), pv ("lowGain"));
        hi.set (true, fs, pv ("highFreq"), pv ("highGain"));
        auto mag = [&] (const dt::Shelf& s, float f)
        {
            const double w = juce::MathConstants<double>::twoPi * f / fs;
            const std::complex<double> z = std::polar (1.0, -w), z2 = z * z;
            const auto H = ((double) s.b0 + (double) s.b1 * z + (double) s.b2 * z2) / (1.0 + (double) s.a1 * z + (double) s.a2 * z2);
            return 20.0 * std::log10 (std::abs (H) + 1e-12);
        };
        juce::Path curve, fill;
        for (int xi = 0; xi <= 140; ++xi)
        {
            const float x = 20.0f + 2.0f * (float) xi;
            const float f = juce::jmin (20000.0f, xf (x));
            const float y = gy ((float) (mag (lo, f) + mag (hi, f)));
            if (xi == 0) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, 76.0f); }
            else curve.lineTo (x, y);
            fill.lineTo (x, y);
        }
        fill.lineTo (300.0f, 76.0f); fill.closeSubPath();
        g.setColour (orange.withAlpha (0.12f)); g.fillPath (fill);
        g.setColour (orange); g.strokePath (curve, juce::PathStrokeType (2.0f));
        auto handle = [&] (float x, float y, const char* t)
        {
            const auto r = juce::Rectangle<float> (20.0f, 20.0f).withCentre ({ x, y });
            g.setColour (boxC); g.fillEllipse (r);
            g.setColour (orange); g.drawEllipse (r, 2.0f);
            g.setColour (bright); g.setFont (sans (11.0f)); g.drawText (t, r, juce::Justification::centred);
        };
        handle (fx (pv ("lowFreq")), gy (pv ("lowGain")), "L");
        handle (fx (pv ("highFreq")), gy (pv ("highGain")), "H");
    }
}

void Display::setFromMouse (juce::Point<float> m)
{
    if (kind != eq || dragging < 0) return;
    const auto P = plot();
    const float x = m.x - P.getX(), y = m.y - P.getY();
    const float f = 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, (x - 20.0f) / 280.0f));
    const float db = juce::jlimit (-18.0f, 18.0f, (76.0f - y) / 60.0f * 18.0f);
    const juce::String g = dragging == 0 ? "lowGain" : "highGain", fq = dragging == 0 ? "lowFreq" : "highFreq";
    if (auto* p = proc.apvts.getParameter (g)) p->setValueNotifyingHost (p->convertTo0to1 (db));
    if (auto* p = proc.apvts.getParameter (fq)) p->setValueNotifyingHost (p->convertTo0to1 (f));
}

void Display::mouseDown (const juce::MouseEvent& e)
{
    if (kind != eq) return;
    const auto P = plot();
    auto fx = [] (float f) { return 20.0f + std::log (f / 20.0f) / std::log (1000.0f) * 280.0f; };
    const float x = e.position.x - P.getX();
    dragging = std::abs (x - fx (pv ("lowFreq"))) <= std::abs (x - fx (pv ("highFreq"))) ? 0 : 1;
    for (auto* id : { dragging == 0 ? "lowGain" : "highGain", dragging == 0 ? "lowFreq" : "highFreq" })
        if (auto* p = proc.apvts.getParameter (id)) p->beginChangeGesture();
    setFromMouse (e.position);
}
void Display::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e.position); }
void Display::mouseUp (const juce::MouseEvent&)
{
    if (kind != eq || dragging < 0) return;
    for (auto* id : { dragging == 0 ? "lowGain" : "highGain", dragging == 0 ? "lowFreq" : "highFreq" })
        if (auto* p = proc.apvts.getParameter (id)) p->endChangeGesture();
    dragging = -1;
}
void Display::mouseDoubleClick (const juce::MouseEvent&)
{
    if (kind != eq) return;
    for (auto* id : { "lowGain", "highGain" })
        if (auto* p = proc.apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (0.0f)); p->endChangeGesture(); }
}

//==============================================================================
class DoubleTakeEditor::Panel : public juce::Component
{
public:
    explicit Panel (DoubleTakeEditor& e) : ed (e), proc (e.proc),
        stageD (proc, Display::stage), pitchD (proc, Display::timePitch), eqD (proc, Display::eq),
        dryBadge ("", bright)
    {
        setLookAndFeel (&lnf);
        for (auto* d : { &stageD, &pitchD, &eqD }) addAndMakeVisible (d);
        eqD.setTooltip ("Drag L (low shelf) or H (high shelf): left/right = frequency, up/down = gain. Double-click to flatten.");

        // header
        for (auto* b : { &twoBtn, &fourBtn }) { addAndMakeVisible (b); b->setColour (juce::TextButton::buttonColourId, deep); b->getProperties().set ("bg", (int) deep.getARGB()); }
        twoBtn.onClick  = [this] { setParam ("four", 0.0f); };
        fourBtn.onClick = [this] { setParam ("four", 1.0f); };
        for (auto* b : { &prevBtn, &nextBtn, &menuBtn }) addAndMakeVisible (b);
        prevBtn.onClick = [this] { stepPreset (-1); };
        nextBtn.onClick = [this] { stepPreset (+1); };
        menuBtn.onClick = [this] { showMenu(); };
        addAndMakeVisible (presetBox);
        presetBox.getProperties().set ("bg", (int) deep.getARGB()); presetBox.getProperties().set ("ink", (int) orange.getARGB());
        presetBox.onClick = [this] { showPresetMenu(); };
        presetBox.setTooltip ("Factory presets");
        addAndMakeVisible (help); help.onClick = [this] { ed.openManual(); }; help.setTooltip ("Open the DoubleTake manual");
        help.setMouseCursor (juce::MouseCursor::PointingHandCursor);

        // global row
        addAndMakeVisible (dryBadge); dryBadge.setTooltip ("Direct (dry) signal on / off");
        btnAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "dryOn", dryBadge));
        knob (dryGain, "dryGain", bright, "Direct level");
        knob (dryPan, "dryPan", bright, "Direct pan");
        addAndMakeVisible (alignBtn); alignBtn.setClickingTogglesState (true);
        alignBtn.setTooltip ("Delay the direct signal to line up with the voices' pitch shifter (adds a little latency, compensated by the host)");
        btnAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "align", alignBtn));
        box (fbCut, "fbCut", orange, "Low-cut inside every voice's feedback loop: keeps repeats from getting muddy");
        addAndMakeVisible (resyncBtn); resyncBtn.getProperties().set ("ink", (int) bright.getARGB());
        resyncBtn.onClick = [this] { proc.resyncModulation(); ed.panel->resyncFlash = 8; };
        resyncBtn.setTooltip ("Restart every voice's wobble at the same point");
        knob (lowGain, "lowGain", orange, "Low shelf gain (voices only)");
        box (lowFreq, "lowFreq", orange, "Low shelf frequency");
        knob (highGain, "highGain", orange, "High shelf gain (voices only)");
        box (highFreq, "highFreq", orange, "High shelf frequency");
        knob (mix, "mix", orange, "Balance: 0% = direct only, 50% = both at full level, 100% = voices only");

        // voices
        for (int i = 0; i < 4; ++i)
        {
            auto& r = rows[(size_t) i];
            const int k = i + 1;
            const auto c = voiceCol[i];
            r.badge = std::make_unique<Badge> (juce::String (k), c);
            addAndMakeVisible (*r.badge); r.badge->setTooltip ("Voice " + juce::String (k) + " on / off");
            btnAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, dtp::v (k, "on"), *r.badge));
            knob (r.gain, dtp::v (k, "gain"), c, "Voice " + juce::String (k) + " level");
            knob (r.pan, dtp::v (k, "pan"), c, "Voice " + juce::String (k) + " pan");
            box (r.delay, dtp::v (k, "delay"), c, "Delay behind the direct signal (ms)");
            box (r.fdbk, dtp::v (k, "fdbk"), c, "Feedback: repeats of this voice");
            box (r.detune, dtp::v (k, "detune"), c, "Fine pitch, in cents (100 = a semitone)");
            box (r.depth, dtp::v (k, "depth"), c, "Wobble depth: how far the pitch drifts (cents)");
            box (r.rate, dtp::v (k, "rate"), c, "Wobble rate (Hz)");
            for (auto* b : { &r.octDn, &r.octUp }) addAndMakeVisible (b);
            const auto id = dtp::v (k, "oct");
            r.octDn.onClick = [this, id] { setParam (id, juce::jmax (-1.0f, std::round (pv (id)) - 1.0f)); };
            r.octUp.onClick = [this, id] { setParam (id, juce::jmin (1.0f, std::round (pv (id)) + 1.0f)); };
            r.octDn.setTooltip ("Octave down"); r.octUp.setTooltip ("Octave up");
        }

        // output
        fader (wet, "wet", orange, "Level of all voices together");
        fader (out, "out", juce::Colours::white, "Output level");
    }
    ~Panel() override { setLookAndFeel (nullptr); }

    void layout()
    {
        twoBtn.setBounds (706, 7, 72, 20); fourBtn.setBounds (780, 7, 72, 20);
        prevBtn.setBounds (866, 6, 22, 22); presetBox.setBounds (892, 6, 200, 22); nextBtn.setBounds (1096, 6, 22, 22);
        help.setBounds (1140, 6, 22, 22); menuBtn.setBounds (12, 6, 26, 22);

        const float dw = (1152.0f - 20.0f) / 3.0f;
        Display* ds[3] = { &stageD, &pitchD, &eqD };
        for (int i = 0; i < 3; ++i) ds[i]->setBounds (juce::roundToInt (14.0f + (float) i * (dw + 10.0f) + 12.0f), 82, juce::roundToInt (dw - 24.0f), 160);

        // global row: panel y 262..366
        const int gy = 300;
        dryBadge.setBounds (40, gy + 6, 30, 30);
        dryGain->setBounds (80, gy - 4, 46, 46); dryPan->setBounds (136, gy - 4, 46, 46);
        alignBtn.setBounds (262, gy + 7, 64, 28);
        fbCut->setBounds (372, gy + 7, 84, 28);
        resyncBtn.setBounds (500, gy + 7, 90, 28);
        lowGain->setBounds (660, gy - 4, 46, 46); lowFreq->setBounds (714, gy + 7, 70, 28);
        highGain->setBounds (840, gy - 4, 46, 46); highFreq->setBounds (894, gy + 7, 70, 28);
        mix->setBounds (1060, gy - 4, 46, 46);

        // voices table: panel x 14..906, rows from y 418
        for (int i = 0; i < 4; ++i)
        {
            auto& r = rows[(size_t) i];
            const int y = rowY (i);
            r.badge->setBounds (colX (0) - 16, y + 22, 32, 32);
            r.gain->setBounds (colX (1) - 21, y + 17, 42, 42);
            r.pan->setBounds (colX (2) - 21, y + 17, 42, 42);
            r.delay->setBounds (colX (3) - 34, y + 24, 68, 28);
            r.fdbk->setBounds (colX (4) - 34, y + 24, 68, 28);
            r.octDn.setBounds (colX (5) - 42, y + 24, 22, 28); r.octUp.setBounds (colX (5) + 20, y + 24, 22, 28);
            r.detune->setBounds (colX (6) - 34, y + 24, 68, 28);
            r.depth->setBounds (colX (7) - 34, y + 24, 68, 28);
            r.rate->setBounds (colX (8) - 34, y + 24, 68, 28);
        }
        wet->setBounds (946, 410, 66, 240);
        out->setBounds (1022, 410, 66, 240);
    }

    static int colX (int c) { return 14 + 20 + 44 + c * 96; }       // column centres
    static int rowY (int i) { return 418 + i * 76; }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (bg);
        // header
        g.setColour (headerC); g.fillRect (0, 0, kW, 34);
        g.setColour (deep); g.fillRect (0, 33, kW, 1);
        g.setFont (sans (19.0f)); g.setColour (bright);
        g.drawText ("DOUBLE", 48, 0, 80, 34, juce::Justification::centredLeft);
        g.setColour (orange); g.drawText ("TAKE", 118, 0, 60, 34, juce::Justification::centredLeft);
        g.setFont (sans (12.0f)); g.setColour (juce::Colour (0xff8f9ba1));
        g.drawText ("VOICE DOUBLER", 172, 0, 140, 34, juce::Justification::centredLeft);
        g.setColour (deep); g.fillRoundedRectangle (704.0f, 5.0f, 150.0f, 24.0f, 4.0f);
        g.setColour (juce::Colour (0xff3a4348)); g.fillRect (1128, 8, 1, 18);

        // displays
        const float dw = (1152.0f - 20.0f) / 3.0f;
        const char* titles[3] = { "STAGE", "TIME & PITCH", "WET EQ" };
        const juce::String subs[3] = { u8 ("where each voice stands \xc2\xb7 further back = later"),
                                       u8 ("delay \xe2\x86\x92 \xc2\xb7 detune \xe2\x86\x95 \xc2\xb7 bar = wobble"), "drag L and H" };
        for (int i = 0; i < 3; ++i)
        {
            const auto r = juce::Rectangle<float> (14.0f + (float) i * (dw + 10.0f), 46.0f, dw, 206.0f);
            drawPanel (g, r);
            g.setColour (bright); g.setFont (sans (16.0f));
            g.drawText (titles[i], r.getX() + 12, r.getY() + 8, 200, 20, juce::Justification::centredLeft);
            g.setColour (juce::Colour (0xff8f9ba1)); g.setFont (sans (10.0f, false));
            g.drawText (subs[i], r.getX() + 100, r.getY() + 8, r.getWidth() - 112, 20, juce::Justification::centredRight);
        }

        // global row
        drawPanel (g, { 14.0f, 262.0f, 1152.0f, 104.0f });
        const float ly = 276.0f;
        label (g, "DIRECT", { 40, ly, 142, 14 });
        label (g, "GAIN", { 80, ly + 12, 46, 12 }, faint, 9); label (g, "PAN", { 136, ly + 12, 46, 12 }, faint, 9);
        label (g, "ALIGN", { 244, ly, 100, 14 });
        label (g, "FEEDBACK LOW-CUT", { 344, ly, 140, 14 });
        label (g, "MODULATION", { 490, ly, 110, 14 });
        label (g, "LOW SHELF", { 650, ly, 140, 14 });
        label (g, "HIGH SHELF", { 830, ly, 140, 14 });
        label (g, "MIX", { 1040, ly, 86, 14 });
        g.setColour (grid);
        for (int x : { 214, 622, 1000 }) g.fillRect (x, 274, 1, 82);
        auto valueUnder = [&] (juce::Slider& s, juce::Colour c)
        {
            g.setColour (c); g.setFont (mono (10.0f));
            g.drawText (s.getTextFromValue (s.getValue()), s.getBounds().toFloat().withY ((float) s.getBottom() - 1.0f).withHeight (13.0f).expanded (14.0f, 0.0f), juce::Justification::centred);
        };
        valueUnder (*dryGain, bright); valueUnder (*dryPan, bright); valueUnder (*lowGain, orange); valueUnder (*highGain, orange); valueUnder (*mix, orange);

        // voices panel
        drawPanel (g, { 14.0f, 376.0f, 892.0f, 370.0f });
        const char* cols[9] = { "VOICE", "GAIN", "PAN", "DELAY ms", "FDBK", "OCT", "DETUNE ct", "DEPTH", "RATE Hz" };
        for (int c = 0; c < 9; ++c) label (g, cols[c], { (float) colX (c) - 49.0f, 390.0f, 98.0f, 16.0f });
        const bool four = pv ("four") > 0.5f;
        for (int i = 0; i < 4; ++i)
        {
            const int y = rowY (i);
            g.setColour (grid); g.fillRect (30, y, 860, 1); g.fillRect (30, y + 76, i == 3 ? 860 : 0, 1);
            const float a = (i < 2 || four) ? 1.0f : 0.35f;
            const int oct = juce::roundToInt (pv (dtp::v (i + 1, "oct")));
            const auto r = juce::Rectangle<float> ((float) colX (5) - 18.0f, (float) y + 24.0f, 36.0f, 28.0f);
            g.setColour (boxC.withMultipliedAlpha (a)); g.fillRoundedRectangle (r, 3.0f);
            g.setColour (voiceCol[i].withMultipliedAlpha (a)); g.setFont (mono (12.0f));
            g.drawText (oct == 0 ? juce::String ("0") : (oct > 0 ? "+1" : u8 ("\xe2\x88\x92" "1")), r, juce::Justification::centred);
        }

        // output panel
        drawPanel (g, { 916.0f, 376.0f, 250.0f, 370.0f });
        label (g, "OUTPUT", { 916, 388, 250, 14 });
        auto fTxt = [] (float v) { return v <= -79.9f ? u8 ("\xe2\x88\x92\xe2\x88\x9e") : (v > 0.05f ? "+" : "") + juce::String (v, 1); };
        label (g, "WET " + fTxt ((float) wet->getValue()), { 930, 656, 100, 14 }, muted, 10);
        label (g, "OUT " + fTxt ((float) out->getValue()), { 1005, 656, 100, 14 }, muted, 10);
        // stereo meter
        const juce::Rectangle<float> mbox (1104.0f, 428.0f, 22.0f, 208.0f);
        g.setColour (deep); g.fillRoundedRectangle (mbox, 3.0f);
        const float lv[2] = { mL, mR };
        for (int c = 0; c < 2; ++c)
        {
            const juce::Rectangle<float> bar (mbox.getX() + 4.0f + (float) c * 8.0f, mbox.getY() + 4.0f, 6.0f, 200.0f);
            g.setColour (juce::Colour (0xff0d1114)); g.fillRect (bar);
            const float f = juce::jlimit (0.0f, 1.0f, (lv[c] + 60.0f) / 60.0f);
            juce::ColourGradient grad (juce::Colour (0xff6fbf3a), 0, bar.getBottom(), juce::Colour (0xffe8553d), 0, bar.getY(), false);
            grad.addColour (0.62, juce::Colour (0xff9bd34a)); grad.addColour (0.82, juce::Colour (0xfff2d14b));
            g.setGradientFill (grad); g.fillRect (bar.withTop (bar.getBottom() - f * bar.getHeight()));
        }
        g.setColour (faint); g.setFont (mono (8.0f));
        g.drawText ("0", juce::Rectangle<float> (1128, 428, 20, 10), juce::Justification::centredLeft);
        g.drawText ("-60", juce::Rectangle<float> (1128, 626, 24, 10), juce::Justification::centredLeft);
        g.setColour (muted); g.setFont (mono (10.0f));
        g.drawText ("PEAK " + juce::String (juce::jmax (mL, mR) <= -59.9f ? juce::String ("-inf") : juce::String (juce::jmax (mL, mR), 1)) + " dB",
                    juce::Rectangle<float> (916, 690, 250, 14), juce::Justification::centred);
        g.setColour (faint); g.setFont (sans (9.5f, false));
        g.drawText (u8 ("drag \xc2\xb7 Shift = fine \xc2\xb7 double-click resets"),
                    juce::Rectangle<float> (916, 716, 250, 14), juce::Justification::centred);
    }

    void tick()
    {
        mL = juce::jmax (proc.outL.load(), mL - 1.6f);
        mR = juce::jmax (proc.outR.load(), mR - 1.6f);
        const bool four = pv ("four") > 0.5f;
        twoBtn.setToggleState (! four, juce::dontSendNotification);
        fourBtn.setToggleState (four, juce::dontSendNotification);
        alignBtn.setButtonText (alignBtn.getToggleState() ? "ON" : "OFF");
        resyncBtn.setToggleState (resyncFlash > 0, juce::dontSendNotification); if (resyncFlash > 0) --resyncFlash;
        presetBox.setButtonText (proc.getProgramName (proc.getCurrentProgram()));
        for (int i = 2; i < 4; ++i)
        {
            auto& r = rows[(size_t) i];
            const float a = four ? 1.0f : 0.35f;
            for (juce::Component* c : { (juce::Component*) r.badge.get(), (juce::Component*) r.gain.get(), (juce::Component*) r.pan.get(),
                                        (juce::Component*) r.delay.get(), (juce::Component*) r.fdbk.get(), (juce::Component*) r.detune.get(),
                                        (juce::Component*) r.depth.get(), (juce::Component*) r.rate.get(), (juce::Component*) &r.octDn, (juce::Component*) &r.octUp })
                c->setAlpha (a);
        }
        t += 1.0 / 30.0;
        for (auto* d : { &stageD, &pitchD, &eqD }) { d->lfoTime = t; d->repaint(); }
        repaint (916, 376, 250, 370);
        repaint (14, 376, 892, 370);
        repaint (0, 0, kW, 34);
        repaint (14, 262, 1152, 104);
    }

    int resyncFlash = 0;

private:
    float pv (const juce::String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void setParam (const juce::String& id, float v)
    {
        if (auto* p = proc.apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); }
    }
    void stepPreset (int d)
    {
        const int n = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() + d + n) % n);
    }
    void showPresetMenu()
    {
        juce::PopupMenu m;
        for (int i = 0; i < proc.getNumPrograms(); ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBox),
                         [safe = juce::Component::SafePointer<Panel> (this)] (int r) { if (safe != nullptr && r > 0) safe->proc.setCurrentProgram (r - 1); });
    }
    void showMenu()
    {
        juce::PopupMenu m, pm;
        for (int i = 0; i < proc.getNumPrograms(); ++i) pm.addItem (100 + i, proc.getProgramName (i), true, i == proc.getCurrentProgram());
        m.addSubMenu ("Presets", pm);
        m.addItem (1, "Open manual...");
        m.addItem (2, "Re-sync modulation");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuBtn),
                         [safe = juce::Component::SafePointer<Panel> (this)] (int r)
                         {
                             if (safe == nullptr) return;
                             if (r == 1) safe->ed.openManual();
                             if (r == 2) safe->proc.resyncModulation();
                             if (r >= 100) safe->proc.setCurrentProgram (r - 100);
                         });
    }
    void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu()) showMenu(); }

    void knob (std::unique_ptr<juce::Slider>& s, const juce::String& id, juce::Colour c, const juce::String& tip)
    {
        s = std::make_unique<juce::Slider> (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox);
        s->getProperties().set ("col", (int) c.getARGB());
        s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s->setMouseDragSensitivity (180);
        s->setVelocityModeParameters (0.6, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        s->setPopupDisplayEnabled (true, true, this);
        s->setTooltip (tip);
        s->setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        addAndMakeVisible (*s);
        attach (*s, id);
    }
    void box (std::unique_ptr<juce::Slider>& s, const juce::String& id, juce::Colour c, const juce::String& tip)
    {
        knob (s, id, c, tip);
        s->getProperties().set ("box", true);
        s->setPopupDisplayEnabled (false, false, nullptr);
    }
    void fader (std::unique_ptr<juce::Slider>& s, const juce::String& id, juce::Colour c, const juce::String& tip)
    {
        s = std::make_unique<ConsoleFader>();
        s->getProperties().set ("col", (int) c.getARGB());
        s->setPopupDisplayEnabled (true, true, this);
        s->setTooltip (tip);
        addAndMakeVisible (*s);
        attach (*s, id);
    }
    void attach (juce::Slider& s, const juce::String& id)
    {
        sliderAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, s));
        if (auto* p = proc.apvts.getParameter (id)) s.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    }

    DoubleTakeEditor& ed;
    DoubleTakeProcessor& proc;
    dtui::LookAndFeel lnf;
    Display stageD, pitchD, eqD;
    juce::TextButton twoBtn { "2 VOICES" }, fourBtn { "4 VOICES" }, prevBtn { "<" }, nextBtn { ">" }, menuBtn { u8 ("\xe2\x89\xa1") }, presetBox;
    HelpButton help;
    Badge dryBadge;
    std::unique_ptr<juce::Slider> dryGain, dryPan, fbCut, lowGain, lowFreq, highGain, highFreq, mix, wet, out;
    juce::TextButton alignBtn { "ON" }, resyncBtn { "RE-SYNC" };
    struct Row
    {
        std::unique_ptr<Badge> badge;
        std::unique_ptr<juce::Slider> gain, pan, delay, fdbk, detune, depth, rate;
        juce::TextButton octDn { u8 ("\xe2\x88\x92") }, octUp { "+" };
    };
    std::array<Row, 4> rows;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> btnAtt;
    float mL = -100.0f, mR = -100.0f;
    double t = 0.0;
    friend class DoubleTakeEditor;
};

//==============================================================================
DoubleTakeEditor::DoubleTakeEditor (DoubleTakeProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    panel = std::make_unique<Panel> (*this);
    addAndMakeVisible (*panel);
    panel->setBounds (0, 0, kW, kH);
    panel->layout();
    setSize (kW, kH);
    setResizable (true, true);
    setResizeLimits (kW / 2, kH / 2, kW * 2, kH * 2);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    startTimerHz (30);
}

DoubleTakeEditor::~DoubleTakeEditor() { stopTimer(); }

void DoubleTakeEditor::resized()
{
    panel->setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) kW));
}

void DoubleTakeEditor::timerCallback() { panel->tick(); }

void DoubleTakeEditor::openManual()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Herigama Audio").getChildFile ("DoubleTake");
    dir.createDirectory();
    auto f = dir.getChildFile ("DoubleTake Manual.html");
    if (f.getSize() != (juce::int64) BinaryData::manual_htmlSize)
        f.replaceWithData (BinaryData::manual_html, (size_t) BinaryData::manual_htmlSize);
    f.startAsProcess();
}
