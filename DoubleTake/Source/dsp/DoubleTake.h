// DoubleTake DSP core (header-only, no JUCE): a 2- or 4-voice doubler.
//
// Each voice: its own delay line -> pitch shifter (octave + detune + LFO "wobble")
//             -> level and pan, with feedback around the voice through a low-cut.
// The voices are summed, shaped by a low and a high shelf, and mixed with the
// direct (dry) signal. With ALIGN on, the dry path is delayed by the pitch
// shifter's built-in lag, so a voice's DELAY value is its true offset from the dry.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace dt
{
constexpr int   kVoices   = 4;
constexpr float kPi       = 3.14159265358979f;
constexpr float kWindowMs = 24.0f;    // pitch-shifter grain window
constexpr float kMaxDelayMs = 100.0f;

inline float dbToGain (float db) { return db <= -79.9f ? 0.0f : std::pow (10.0f, db * 0.05f); }

struct VoiceSettings
{
    bool  on = true;
    float gainDb = 0.0f, pan = 0.0f;      // pan -100 (L) .. +100 (R)
    float delayMs = 10.0f, feedback = 0.0f;  // feedback 0..90 %
    int   octave = 0;                      // -1, 0, +1
    float detuneCents = 0.0f, depthCents = 0.0f, rateHz = 1.0f;
};

struct Settings
{
    bool  fourVoices = true, dryOn = true, align = true;
    float dryGainDb = 0.0f, dryPan = 0.0f;
    float fbCutHz = 80.0f;
    float lowGainDb = 0.0f, lowFreq = 40.0f, highGainDb = 0.0f, highFreq = 2200.0f;
    float mix = 50.0f;                     // 0 = dry only, 50 = both full, 100 = voices only
    float wetDb = 0.0f, outDb = 0.0f;
    std::array<VoiceSettings, kVoices> v;
};

// RBJ shelving biquad (transposed direct form II)
struct Shelf
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1[2] {}, z2[2] {};
    void set (bool high, double fs, float f, float gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * kPi * std::clamp ((double) f, 10.0, fs * 0.45) / fs;
        const double cw = std::cos (w), sw = std::sin (w), al = sw / 2.0 * std::sqrt (2.0);   // S = 1
        const double sA = 2.0 * std::sqrt (A) * al;
        double B0, B1, B2, A0, A1, A2;
        if (! high) { B0 = A * ((A + 1) - (A - 1) * cw + sA); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - sA);
                      A0 = (A + 1) + (A - 1) * cw + sA;        A1 = -2 * ((A - 1) + (A + 1) * cw);    A2 = (A + 1) + (A - 1) * cw - sA; }
        else        { B0 = A * ((A + 1) + (A - 1) * cw + sA); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - sA);
                      A0 = (A + 1) - (A - 1) * cw + sA;        A1 = 2 * ((A - 1) - (A + 1) * cw);     A2 = (A + 1) - (A - 1) * cw - sA; }
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0); a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
    float tick (int c, float x)
    {
        const float y = b0 * x + z1[c];
        z1[c] = b1 * x - a1 * y + z2[c];
        z2[c] = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }
};

// One-pole smoother
struct Smooth
{
    float v = 0, c = 0.999f;
    void setTime (double fs, float ms) { c = std::exp (-1.0f / (float) (fs * ms * 0.001)); }
    float next (float target) { v = target + c * (v - target); return v; }
};

class Voice
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        size = 1;
        const int need = (int) (fs * (kMaxDelayMs + kWindowMs + 10.0f) * 0.001) + 4;
        while (size < need) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        win = (float) (fs * kWindowMs * 0.001);
        gL.setTime (fs, 20); gR.setTime (fs, 20); dSm.setTime (fs, 60); fbSm.setTime (fs, 30);
        reset();
    }
    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        w = 0; phase = 0.0f; lfo = 0.0f; hpX = hpY = 0.0f; lastOut = 0.0f;
        gL.v = gR.v = 0.0f; dSm.v = -1.0f;
    }
    void resyncLfo() { lfo = 0.0f; }

    // mono in -> stereo out (added into outL/outR)
    void process (const float* in, float* outL, float* outR, int n, const VoiceSettings& s, float fbCoef, float wetGain)
    {
        const bool active = s.on;
        const float g = active ? dbToGain (s.gainDb) * wetGain : 0.0f;
        const float p = (std::clamp (s.pan, -100.0f, 100.0f) + 100.0f) / 200.0f;
        const float tL = g * std::cos (p * kPi * 0.5f), tR = g * std::sin (p * kPi * 0.5f);
        const float dTarget = (float) (fs * std::clamp (s.delayMs, 0.0f, kMaxDelayMs) * 0.001);
        if (dSm.v < 0.0f) dSm.v = dTarget;
        const float fbT = active ? std::clamp (s.feedback, 0.0f, 90.0f) * 0.01f : 0.0f;
        const float lfoInc = std::clamp (s.rateHz, 0.05f, 40.0f) / (float) fs;
        const int mask = size - 1;

        for (int i = 0; i < n; ++i)
        {
            // feedback path: 1-pole high-pass on the voice's last output
            const float hp = fbCoef * (hpY + lastOut - hpX);
            hpX = lastOut; hpY = hp;
            buf[(size_t) w] = in[i] + fbSm.next (fbT) * hp;

            // pitch ratio from octave, detune and wobble (sine LFO in cents)
            const float cents = (float) s.octave * 1200.0f + s.detuneCents + s.depthCents * std::sin (2.0f * kPi * lfo);
            lfo += lfoInc; if (lfo >= 1.0f) lfo -= 1.0f;
            const float ratio = std::exp2 (cents / 1200.0f);

            // two crossfaded read heads sweeping across the window (classic rotating-tap shifter)
            phase += (1.0f - ratio) / win;
            phase -= std::floor (phase);
            const float d = dSm.next (dTarget);
            float y = 0.0f;
            for (int h = 0; h < 2; ++h)
            {
                float ph = phase + 0.5f * (float) h; if (ph >= 1.0f) ph -= 1.0f;
                const float amp = std::sin (kPi * ph);
                const float rp = (float) w - (d + ph * win + 1.0f);
                y += amp * amp * read (rp, mask);   // the two heads are half a window apart: sin^2 + cos^2 = 1
            }
            lastOut = y;
            outL[i] += y * gL.next (tL);
            outR[i] += y * gR.next (tR);
            w = (w + 1) & mask;
        }
    }
    float averageLagSamples() const { return win * 0.5f + 1.0f; }

private:
    float read (float pos, int mask) const
    {
        const float fl = std::floor (pos);
        const int i0 = ((int) fl) & mask;
        const float fr = pos - fl;
        // 4-point cubic (Catmull-Rom)
        const float ym1 = buf[(size_t) ((i0 - 1) & mask)], y0 = buf[(size_t) i0], y1 = buf[(size_t) ((i0 + 1) & mask)], y2 = buf[(size_t) ((i0 + 2) & mask)];
        return y0 + 0.5f * fr * (y1 - ym1 + fr * (2.0f * ym1 - 5.0f * y0 + 4.0f * y1 - y2 + fr * (3.0f * (y0 - y1) + y2 - ym1)));
    }
    double fs = 48000.0;
    std::vector<float> buf;
    int size = 1, w = 0;
    float win = 1000.0f, phase = 0, lfo = 0, hpX = 0, hpY = 0, lastOut = 0;
    Smooth gL, gR, dSm, fbSm;
};

class Processor
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        fs = sampleRate;
        for (auto& v : voices) v.prepare (fs);
        lagSamples = (int) std::round (voices[0].averageLagSamples());
        dryBuf[0].assign (4096, 0.0f); dryBuf[1].assign (4096, 0.0f); dw = 0;
        mono.assign ((size_t) std::max (1, maxBlock), 0.0f);
        wL.assign ((size_t) std::max (1, maxBlock), 0.0f); wR.assign ((size_t) std::max (1, maxBlock), 0.0f);
        for (auto* sm : { &dgL, &dgR, &outSm, &wetMix }) sm->setTime (fs, 20);
        lastEq = {};
        reset();
    }
    void reset()
    {
        for (auto& v : voices) v.reset();
        lo.reset(); hi.reset();
        std::fill (dryBuf[0].begin(), dryBuf[0].end(), 0.0f); std::fill (dryBuf[1].begin(), dryBuf[1].end(), 0.0f);
        resync = false;
    }
    void resyncModulation() { resync = true; }
    int  latencySamples() const { return s.align ? lagSamples : 0; }

    void process (float* l, float* r, int n)
    {
        if (n > (int) mono.size()) { mono.resize ((size_t) n); wL.resize ((size_t) n); wR.resize ((size_t) n); }
        if (resync) { for (auto& v : voices) v.resyncLfo(); resync = false; }

        // shelves (recomputed only when changed)
        const std::array<float, 4> eq { s.lowGainDb, s.lowFreq, s.highGainDb, s.highFreq };
        if (eq != lastEq) { lo.set (false, fs, s.lowFreq, s.lowGainDb); hi.set (true, fs, s.highFreq, s.highGainDb); lastEq = eq; }

        for (int i = 0; i < n; ++i) mono[(size_t) i] = r ? 0.5f * (l[i] + r[i]) : l[i];
        std::fill (wL.begin(), wL.begin() + n, 0.0f); std::fill (wR.begin(), wR.begin() + n, 0.0f);

        const float fbCoef = std::exp (-2.0f * kPi * std::clamp (s.fbCutHz, 10.0f, 2000.0f) / (float) fs);
        const float m = std::clamp (s.mix, 0.0f, 100.0f) * 0.01f;
        const float wetT = std::min (1.0f, 2.0f * m) * dbToGain (s.wetDb);
        const float dryT = std::min (1.0f, 2.0f * (1.0f - m)) * (s.dryOn ? dbToGain (s.dryGainDb) : 0.0f);
        const float dp = (std::clamp (s.dryPan, -100.0f, 100.0f) + 100.0f) / 200.0f;
        // dry pan as a balance: centre keeps both channels at unity
        const float dLt = dryT * std::min (1.0f, 2.0f * (1.0f - dp)), dRt = dryT * std::min (1.0f, 2.0f * dp);
        const float outT = dbToGain (s.outDb);
        const int nv = s.fourVoices ? 4 : 2;
        for (int k = 0; k < nv; ++k) voices[(size_t) k].process (mono.data(), wL.data(), wR.data(), n, s.v[(size_t) k], fbCoef, 1.0f);
        for (int k = nv; k < kVoices; ++k)   // keep unused voices silent but running down cleanly
        {
            VoiceSettings off = s.v[(size_t) k]; off.on = false;
            voices[(size_t) k].process (mono.data(), wL.data(), wR.data(), n, off, fbCoef, 0.0f);
        }

        const int lag = s.align ? lagSamples : 0;
        const int dmask = (int) dryBuf[0].size() - 1;
        float pk = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float xl = l[i], xr = r ? r[i] : l[i];
            dryBuf[0][(size_t) dw] = xl; dryBuf[1][(size_t) dw] = xr;
            const float dl = dryBuf[0][(size_t) ((dw - lag) & dmask)], dr = dryBuf[1][(size_t) ((dw - lag) & dmask)];
            dw = (dw + 1) & dmask;
            const float wm = wetMix.next (wetT);
            const float yl = lo.tick (0, hi.tick (0, wL[(size_t) i])) * wm;
            const float yr = lo.tick (1, hi.tick (1, wR[(size_t) i])) * wm;
            const float og = outSm.next (outT);
            float ol = (dl * dgL.next (dLt) + yl) * og;
            float orr = (dr * dgR.next (dRt) + yr) * og;
            if (! std::isfinite (ol)) ol = 0.0f;
            if (! std::isfinite (orr)) orr = 0.0f;
            l[i] = ol;
            if (r) r[i] = orr; else l[i] = 0.5f * (ol + orr);
            pk = std::max (pk, std::max (std::fabs (ol), std::fabs (orr)));
        }
        peak = pk;
    }

    Settings s;
    float peak = 0.0f;

private:
    double fs = 48000.0;
    std::array<Voice, kVoices> voices;
    Shelf lo, hi;
    std::array<float, 4> lastEq {};
    std::vector<float> dryBuf[2], mono, wL, wR;
    int dw = 0, lagSamples = 0;
    bool resync = false;
    Smooth dgL, dgR, outSm, wetMix;
};
} // namespace dt
