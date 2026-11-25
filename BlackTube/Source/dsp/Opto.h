#pragma once
// Black Tube — stereo optical tube compressor core. No JUCE dependency, so it can be tested on its own.
//
// Model fitted to black-box measurements of a CL 1B-style opto compressor plugin
// (BlackTube/docs/measurements.json, 1 kHz sines, stepped levels, level bursts, sweeps):
//
//   detector  : stereo-linked power detector (x^2), one-pole attack / release in the power domain,
//               level = 10*log10(2*e)  (a sine reads its peak level in dBFS)
//   threshold : onset T = threshold knob (dB) + offset   (modern -15.8, vintage -11.5, fixed -13.5 dB)
//   curve     : soft knee of width W, slope (1/R - 1) above it, gain reduction saturating toward
//               -G (G = 69 dB, the opto cell's limit):  GR = -G * tanh(-gr / G)
//   ratio     : the knob is not literal — 2 / 6 / 10 measured as 2.67 / 5.08 / 16.8 (modern, manual)
//   timing    : attack 21 ms / 227 ms / 935 ms and release 11 ms / 281 ms / 1.90 s at knob 0 / 5 / 10
//               (log-interpolated in between); fixed mode 2.5 ms / 2.1 ms; vintage about 4x faster
//   vintage   : -0.79 dB, 2nd-order low cut near 18 Hz, gentle top roll-off, half-wave detector
//               (gain ripple at the fundamental gives the measured 2nd-harmonic colour when compressing)
//   modern    : clean, flat, -0.08 dB
#include <cmath>
#include <algorithm>
#include <atomic>

namespace bt
{
    inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
    inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

    enum class ArMode { Fixed = 0, FixMan = 1, Manual = 2 };
    enum class Meter  { Input = 0, Compression = 1, Output = 2 };

    struct Settings
    {
        bool  thresholdOff = false;
        float thresholdDb  = -20.0f;   // knob 0 .. -40 dB
        float ratio        = 6.0f;     // knob 2 .. 10
        float attack       = 5.0f;     // knob 0 .. 10
        float release      = 5.0f;     // knob 0 .. 10
        ArMode arMode      = ArMode::Manual;
        bool  vintage      = false;    // CL1B generation: vintage mk I / modern mk II
        int   lowCut       = 0;        // sidechain low cut: 0 off, 1 = 80 Hz, 2 = 220 Hz
        bool  external     = false;    // sidechain bus: internal / external
        float parallel     = 1.0f;     // 0 dry .. 1 wet
        float outputDb     = 0.0f;
        bool  outputOff    = false;
    };

    // ---- measured tables ----------------------------------------------------------------------
    // piecewise log interpolation through values at knob 0, 5 and 10
    inline float knobTime (float k, float v0, float v5, float v10)
    {
        k = std::clamp (k, 0.0f, 10.0f);
        const float a = k <= 5.0f ? std::log (v0) : std::log (v5), b = k <= 5.0f ? std::log (v5) : std::log (v10);
        const float t = k <= 5.0f ? k / 5.0f : (k - 5.0f) / 5.0f;
        return std::exp (a + (b - a) * t);
    }
    inline float attackSeconds  (float k) { return knobTime (k, 0.0077f, 0.227f, 0.935f); }
    inline float releaseSeconds (float k) { return knobTime (k, 0.0110f, 0.281f, 1.898f); }

    // the cell gets slower as the ratio goes up (measured at ratio 2 / 6 / 10)
    inline float ratioTimeFactor (float knob, float f2, float f10)
    {
        knob = std::clamp (knob, 2.0f, 10.0f);
        const float x = std::log (knob), x1 = std::log (6.0f);
        return knob <= 6.0f ? std::exp (std::log (f2)  * (x1 - x) / (x1 - std::log (2.0f)))
                            : std::exp (std::log (f10) * (x - x1) / (std::log (10.0f) - x1));
    }

    // ratio knob -> effective ratio (modern, manual), log-log interpolation through the measurements
    inline float effectiveRatio (float knob)
    {
        knob = std::clamp (knob, 2.0f, 10.0f);
        const float x0 = std::log (2.0f), x1 = std::log (6.0f), x2 = std::log (10.0f), x = std::log (knob);
        const float y0 = std::log (2.675f), y1 = std::log (5.078f), y2 = std::log (16.80f);
        return std::exp (x <= x1 ? y0 + (y1 - y0) * (x - x0) / (x1 - x0) : y1 + (y2 - y1) * (x - x1) / (x2 - x1));
    }

    struct Curve { float offsetDb, kneeDb, slopeScale; };
    inline Curve curveFor (bool vintage, ArMode m)
    {
        if (vintage)             return { -11.54f, 32.3f, 1.613f };   // BT06 fit
        if (m == ArMode::Fixed)  return { -13.46f, 10.1f, 0.861f };   // BT09 fit
        if (m == ArMode::FixMan) return { -14.6f,   6.0f, 0.93f  };   // between fixed and manual (estimate)
        return { -15.80f, 3.33f, 1.0f };                               // BT01-03 joint fit
    }
    constexpr float kCeilingDb = 68.9f;   // opto cell limit on gain reduction

    // gain change in dB (<= 0) for a detector level in dBFS
    inline float staticGain (float levelDb, float T, float W, float ratio)
    {
        const float over = levelDb - T, k = 1.0f / ratio - 1.0f;
        float g;
        if (over <= -0.5f * W)     g = 0.0f;
        else if (over >= 0.5f * W) g = k * over;
        else                       { const float u = over + 0.5f * W; g = k * u * u / (2.0f * W); }
        return -kCeilingDb * std::tanh (-g / kCeilingDb);
    }

    // ---- small filters --------------------------------------------------------------------------
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void reset() { z1 = z2 = 0; }
        float process (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        void highpass (double sr, double f, double q)
        {
            const double w = 2.0 * 3.141592653589793 * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1.0 + al;
            b0 = (float) ((1.0 + c) / 2.0 / a0); b1 = (float) (-(1.0 + c) / a0); b2 = b0;
            a1 = (float) (-2.0 * c / a0); a2 = (float) ((1.0 - al) / a0);
        }
        void lowpass1 (double sr, double f)   // first order, bilinear with prewarp
        {
            const double k = std::tan (3.141592653589793 * std::min (f, 0.45 * sr) / sr);
            b0 = b1 = (float) (k / (1.0 + k)); b2 = 0; a1 = (float) ((k - 1.0) / (k + 1.0)); a2 = 0;
        }
        void bypass() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    };

    // ---- the compressor ---------------------------------------------------------------------------
    class Opto
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            smoothA = 1.0f - std::exp (-1.0f / (0.02f * (float) sr));
            for (int c = 0; c < 2; ++c)
            {
                vintHp[c].highpass (sr, 18.0, 0.62);
                vintLp[c].lowpass1 (sr, 36000.0);
            }
            lastLowCut = -1;
            reset();
        }
        void reset()
        {
            eMan = eFix = 0.0f; grNow = 0.0f; cell = 0.0f;
            for (auto* b : { &vintHp[0], &vintHp[1], &vintLp[0], &vintLp[1], &scHp[0], &scHp[1] }) b->reset();
            meterGR.store (0.0f); meterIn.store (-100.0f); meterOut.store (-100.0f);
        }

        Settings s;
        bool enabled = true;

        // colour of the gain cell (fitted to the measured harmonics)
        float asymModern = 0.08f, asymVintage = 0.55f;   // share of half-wave detection -> 2nd harmonic
        float cellHz = 200.0f;                           // opto cell response: smooths gain ripple above this
        float fixedCellHz = 4000.0f;

        // meters (written by process, read by the editor)
        std::atomic<float> meterGR { 0.0f }, meterIn { -100.0f }, meterOut { -100.0f };
        // per-block peaks for the dynamics scope: input peak dB and the compressed ("kept") peak dB
        float blockInPeakDb = -100.0f, blockKeptPeakDb = -100.0f, blockGRDb = 0.0f;

        // left/right in place; sidechain pointers may be null (then the main input feeds the detector)
        void process (float* left, float* right, int n, const float* scL = nullptr, const float* scR = nullptr)
        {
            const int numCh = right != nullptr ? 2 : 1;
            updateCoefficients();

            const Curve cv = curveFor (s.vintage, s.arMode);
            const float T = s.thresholdDb + cv.offsetDb;
            const float R = 1.0f + (effectiveRatio (s.ratio) - 1.0f) * cv.slopeScale;
            const float makeup = s.vintage ? -0.79f : -0.08f;
            const float outTarget = s.outputOff ? 0.0f : dbToGain (s.outputDb + makeup);
            const float mixTarget = std::clamp (s.parallel, 0.0f, 1.0f);
            const bool useSc = s.external && scL != nullptr;

            float inPk = 0.0f, keptPk = 0.0f, inSq = 0.0f, outSq = 0.0f, grMax = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                outG += (outTarget - outG) * smoothA;
                mix  += (mixTarget - mix)  * smoothA;
                float l = left[i], r = numCh == 2 ? right[i] : l;

                if (s.vintage) { l = vintLp[0].process (vintHp[0].process (l)); r = vintLp[1].process (vintHp[1].process (r)); }

                // detector input
                float dl = useSc ? scL[i] : l, dr = useSc ? (scR ? scR[i] : scL[i]) : r;
                if (s.lowCut > 0) { dl = scHp[0].process (dl); dr = scHp[1].process (dr); }
                // detector: full-wave power with a share of half-wave (asymmetric cell -> even harmonics)
                const float h = (s.vintage ? asymVintage : asymModern) * (s.arMode == ArMode::Fixed ? 0.12f : 1.0f);
                const float hl = std::max (dl, 0.0f), hr = std::max (dr, 0.0f);
                const float pw = (1.0f - h) * std::max (dl * dl, dr * dr) + h * 2.0f * std::max (hl * hl, hr * hr);

                float gr = 0.0f;
                if (! s.thresholdOff && enabled)
                {
                    eMan += (pw - eMan) * (pw > eMan ? aAttMan : aRelMan);
                    eFix += (pw - eFix) * (pw > eFix ? aAttFix : aRelFix);
                    const float e = wFix * eFix + (1.0f - wFix) * eMan;
                    const float target = staticGain (10.0f * std::log10 (2.0f * e + 1.0e-20f), T, cv.kneeDb, R);
                    cell += (target - cell) * aCell;     // the opto cell cannot follow fast ripple
                    gr = cell;
                }
                else { eMan = eFix = 0.0f; cell = 0.0f; }
                grNow = gr;
                const float g = dbToGain (gr);

                const float cl = l * g, cr = r * g;
                inPk = std::max (inPk, std::max (std::abs (l), std::abs (r)));
                keptPk = std::max (keptPk, std::max (std::abs (cl), std::abs (cr)));
                inSq += 0.5f * (l * l + r * r);
                grMax = std::min (grMax, gr);

                float ol = (mix * cl + (1.0f - mix) * l) * outG;
                float orr = (mix * cr + (1.0f - mix) * r) * outG;
                outSq += 0.5f * (ol * ol + orr * orr);
                left[i] = ol;
                if (numCh == 2) right[i] = orr;
            }
            const float nInv = 1.0f / (float) std::max (n, 1);
            meterGR.store (-grMax);
            meterIn.store (10.0f * std::log10 (inSq * nInv * 2.0f + 1.0e-12f));
            meterOut.store (10.0f * std::log10 (outSq * nInv * 2.0f + 1.0e-12f));
            blockInPeakDb = gainToDb (inPk); blockKeptPeakDb = gainToDb (keptPk); blockGRDb = grMax;
        }

        float currentGRDb() const { return grNow; }

    private:
        void updateCoefficients()
        {
            const float speed = s.vintage ? 0.25f : 1.0f;                 // vintage measured ~4x faster
            const float ta = std::max (attackSeconds (s.attack) * speed * ratioTimeFactor (s.ratio, 0.63f, 1.33f), 0.0004f);
            const float tr = std::max (releaseSeconds (s.release) * speed * ratioTimeFactor (s.ratio, 0.52f, 1.57f), 0.0012f);
            aAttMan = coef (ta); aRelMan = coef (tr);
            aAttFix = coef (s.vintage ? 0.0015f : 0.00254f); aRelFix = coef (s.vintage ? 0.0012f : 0.00208f);
            const float fc = s.arMode == ArMode::Fixed ? fixedCellHz : cellHz;
            aCell = 1.0f - std::exp (-2.0f * 3.14159265f * fc / (float) sr);
            wFix = s.arMode == ArMode::Fixed ? 1.0f : s.arMode == ArMode::FixMan ? 0.5f : 0.0f;
            if (s.arMode == ArMode::FixMan) aAttFix = aAttMan;                // fix/man: manual attack, mixed release
            if (s.lowCut != lastLowCut)
            {
                lastLowCut = s.lowCut;
                for (auto& f : scHp) { if (s.lowCut > 0) f.highpass (sr, s.lowCut == 1 ? 80.0 : 220.0, 0.707); else f.bypass(); f.reset(); }
            }
        }
        float coef (float seconds) const { return 1.0f - std::exp (-1.0f / (seconds * (float) sr)); }

        double sr = 48000.0;
        float smoothA = 0.001f, outG = 1.0f, mix = 1.0f;
        float eMan = 0.0f, eFix = 0.0f, grNow = 0.0f, wFix = 0.0f, cell = 0.0f, aCell = 0.03f;
        float aAttMan = 0.01f, aRelMan = 0.001f, aAttFix = 0.1f, aRelFix = 0.1f;
        Biquad vintHp[2], vintLp[2], scHp[2];
        int lastLowCut = -1;
    };
}
