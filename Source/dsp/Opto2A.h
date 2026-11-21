#pragma once
// OPT-2A optical leveling amplifier: JUCE-free DSP core.
// Every constant below was fitted to measurements of a reference tube opto leveler
// (static curves at 100 Hz / 1 kHz / 5 kHz, attack/release bursts, sweeps, harmonic steps).
#include <atomic>
#include <cmath>
#include <algorithm>

namespace opt
{
inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[2] {}, z2[2] {};
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    // RBJ low shelf
    void lowShelf (double fs, double f0, double gainDb, double slope = 1.0)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w0 = 2.0 * M_PI * f0 / fs;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / 2.0 * std::sqrt ((A + 1.0 / A) * (1.0 / slope - 1.0) + 2.0);
        const double sA = 2.0 * std::sqrt (A) * alpha;
        const double a0 = (A + 1) + (A - 1) * cw + sA;
        b0 = A * ((A + 1) - (A - 1) * cw + sA) / a0;
        b1 = 2 * A * ((A - 1) - (A + 1) * cw) / a0;
        b2 = A * ((A + 1) - (A - 1) * cw - sA) / a0;
        a1 = -2 * ((A - 1) + (A + 1) * cw) / a0;
        a2 = ((A + 1) + (A - 1) * cw - sA) / a0;
    }
    // RBJ high shelf
    void highShelf (double fs, double f0, double gainDb, double slope = 1.0)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w0 = 2.0 * M_PI * std::min (f0, 0.45 * fs) / fs;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / 2.0 * std::sqrt ((A + 1.0 / A) * (1.0 / slope - 1.0) + 2.0);
        const double sA = 2.0 * std::sqrt (A) * alpha;
        const double a0 = (A + 1) - (A - 1) * cw + sA;
        b0 = A * ((A + 1) + (A - 1) * cw + sA) / a0;
        b1 = -2 * A * ((A - 1) + (A + 1) * cw) / a0;
        b2 = A * ((A + 1) + (A - 1) * cw - sA) / a0;
        a1 = 2 * ((A - 1) - (A + 1) * cw) / a0;
        a2 = ((A + 1) - (A - 1) * cw - sA) / a0;
    }
    inline float process (float x, int c)
    {
        const double y = b0 * x + z1[c];
        z1[c] = b1 * x - a1 * y + z2[c];
        z2[c] = b2 * x - a2 * y;
        return (float) y;
    }
};

class Opto2A
{
public:
    // ---- controls (set before process) ----
    float peakReduction = 25.0f;   // 0..100
    float gain          = 30.0f;   // 0..100
    float emphasis      = 0.0f;    // 0..100  (side-chain HF emphasis, 0 = flat)
    float mix           = 100.0f;  // 0..100 % (parallel blend of the leveled signal)
    bool  limit         = false;   // false = compress, true = limit

    // ---- meters (read from the UI thread) ----
    std::atomic<float> meterGR { 0.0f };     // dB of gain reduction
    std::atomic<float> meterOutDb { -100.0f }; // output RMS, dBFS
    float blockInPeak = 0, blockOutPeak = 0, blockGR = 0;  // updated each process() call

    // ---- fitted model constants ----
    struct Fit
    {
        // static curve: GR = s * (u - w * (1 - e^(-u/w))), u = level - T
        static constexpr float sComp = 0.7715f, wComp = 1.796f;
        static constexpr float sLim  = 0.95f,   wLim  = 3.0f;
        // detector (rectifier + light) and the three photocell components
        static constexpr float envAttMs = 1.4725f, envRelMs = 0.5f;
        static constexpr float capSlow = 6.433f, slowAttMs = 19.087f, slowRelMs = 456.1f;
        static constexpr float fastAttMs = 0.2f, fastRelMs = 38.06f;
        static constexpr float capMem = 0.42f, memAttMs = 564.9f, memRelMs = 3698.0f;
        // tube output stage: soft clip with a knee, slightly asymmetric
        // side-chain voicing (detector is a little more sensitive at the frequency extremes)
        static constexpr float scLowDb = 1.5f, scLowHz = 250.0f, scHighDb = 1.3f, scHighHz = 3000.0f;
        // photocell non-linearity while leveling: ~-52 dB 2nd, ~-56 dB 3rd harmonic
        static constexpr float cellH2 = 0.0024f, cellH3 = 0.0036f, cellOnsetDb = 3.0f;
        static constexpr float clipPos = 1.6479f, clipNeg = 1.5468f, knee = 0.9502f, even = 0.0094f;
    };

    // Peak Reduction -> threshold (dBFS of the detector), piecewise through the fitted points
    static float thresholdFor (float pr)
    {
        static constexpr float P[] = { 0.0f, 25.0f, 40.0f, 55.0f, 70.0f, 90.0f, 100.0f };
        static constexpr float T[] = { -3.5f, -17.07f, -25.21f, -32.07f, -39.87f, -50.94f, -56.5f };
        pr = std::clamp (pr, 0.0f, 100.0f);
        for (int i = 0; i < 6; ++i)
            if (pr <= P[i + 1])
                return T[i] + (T[i + 1] - T[i]) * (pr - P[i]) / (P[i + 1] - P[i]);
        return T[6];
    }
    // Gain knob -> makeup in dB (measured: 30 -> +5.62 dB, 60 -> +18.40 dB)
    static float makeupDbFor (float g) { return 5.62f + (std::clamp (g, 0.0f, 100.0f) - 30.0f) * 0.426f; }

    static float staticGR (float levelDb, float T, bool lim)
    {
        const float s = lim ? Fit::sLim : Fit::sComp, w = lim ? Fit::wLim : Fit::wComp;
        const float u = levelDb - T;
        return u > 0 ? s * (u - w * (1.0f - std::exp (-u / w))) : 0.0f;
    }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        auto c = [this] (float ms) { return (float) std::exp (-1.0 / (ms * 1e-3 * fs)); };
        envA = c (Fit::envAttMs); envR = c (Fit::envRelMs);
        slowA = c (Fit::slowAttMs); slowR = c (Fit::slowRelMs);
        fastA = c (Fit::fastAttMs); fastR = c (Fit::fastRelMs);
        memA = c (Fit::memAttMs); memR = c (Fit::memRelMs);
        meterK = c (300.0f);
        dcR = (float) std::exp (-2.0 * M_PI * 1.0 / fs);                 // 1 Hz DC blocker after the tube stage
        hfShelf.highShelf (fs, 13000.0, 0.9);
        scLow.lowShelf (fs, Fit::scLowHz, Fit::scLowDb, 0.7);
        scHigh.highShelf (fs, Fit::scHighHz, Fit::scHighDb, 0.7);                             // the unit's gentle top-end lift
        makeupSm = dbToGain (makeupDbFor (gain));
        mixSm = mix * 0.01f;
        smoothK = c (20.0f);
        lastEmph = -1.0f;
        reset();
    }

    void reset()
    {
        env = 0; grSlow = 0; kFast = 0; grMem = 0; meterSq = 0;
        hfShelf.reset(); scShelf.reset(); scLow.reset(); scHigh.reset();
        dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0;
        meterGR.store (0.0f);
    }

    static float tubeStage (float v)
    {
        const float u = v + Fit::even * v * v;
        const float c = u >= 0 ? Fit::clipPos : Fit::clipNeg;
        const float x = std::abs (u), t = std::min (Fit::knee, c * 0.98f);
        const float g = x < t ? x : t + (c - t) * std::tanh ((x - t) / (c - t));
        return u >= 0 ? g : -g;
    }

    // stereo-linked processing; R may be null (mono)
    void process (float* L, float* R, int n)
    {
        if (emphasis != lastEmph)
        {
            scShelf.highShelf (fs, 3000.0, 12.0 * emphasis * 0.01, 0.8);
            lastEmph = emphasis;
        }
        const bool emph = emphasis > 0.05f;
        const float T = thresholdFor (peakReduction);
        const float makeupT = dbToGain (makeupDbFor (gain));
        const float mixT = std::clamp (mix * 0.01f, 0.0f, 1.0f);
        float inPk = 0, outPk = 0, grMax = 0;

        for (int i = 0; i < n; ++i)
        {
            const float xl = L[i], xr = R ? R[i] : xl;
            inPk = std::max (inPk, std::max (std::abs (xl), std::abs (xr)));

            // side chain: rectifier and electroluminescent panel
            float sl = scHigh.process (scLow.process (xl, 0), 0), sr = scHigh.process (scLow.process (xr, 1), 1);
            if (emph) { sl = scShelf.process (sl, 0); sr = scShelf.process (sr, 1); }
            const float r = std::max (std::abs (sl), std::abs (sr));
            env = r > env ? envA * env + (1 - envA) * r : envR * env + (1 - envR) * r;
            const float level = 20.0f * std::log10 (env + 1e-9f);

            // photocell: memory + slow + fast components
            const float gt = staticGR (level, T, limit);
            const float tm = Fit::capMem * (1.0f - std::exp (-gt / Fit::capMem));
            const float g2 = gt - tm;
            const float ts = Fit::capSlow * (1.0f - std::exp (-g2 / Fit::capSlow));
            const float kft = dbToGain (g2 - ts) - 1.0f;
            grMem  = tm  > grMem  ? memA  * grMem  + (1 - memA)  * tm  : memR  * grMem  + (1 - memR)  * tm;
            grSlow = ts  > grSlow ? slowA * grSlow + (1 - slowA) * ts  : slowR * grSlow + (1 - slowR) * ts;
            kFast  = kft > kFast  ? fastA * kFast  + (1 - fastA) * kft : fastR * kFast  + (1 - fastR) * kft;
            const float gr = grMem + grSlow + 20.0f * std::log10 (1.0f + kFast);
            grMax = std::max (grMax, gr);

            makeupSm = smoothK * makeupSm + (1 - smoothK) * makeupT;
            mixSm    = smoothK * mixSm    + (1 - smoothK) * mixT;
            const float cell = dbToGain (-gr);
            const float g = (mixSm * cell + (1.0f - mixSm)) * makeupSm;

            // photocell non-linearity: level-independent 2nd/3rd harmonics that appear while leveling
            const float amt = mixSm * cell * makeupSm * (1.0f - std::exp (-gr / Fit::cellOnsetDb));
            const float pk = env * 1.5708f + 1e-6f;
            auto cellDist = [&] (float x)
            {
                const float xn = std::clamp (x / pk, -1.5f, 1.5f);
                return amt * pk * (Fit::cellH2 * (xn * xn - 0.5f) + Fit::cellH3 * xn * xn * xn);
            };
            float yl = tubeStage (hfShelf.process (xl * g + cellDist (xl), 0));
            float yr = tubeStage (hfShelf.process (xr * g + cellDist (xr), 1));
            // DC blocker (coupling capacitor)
            const float ol = yl - dcX[0] + dcR * dcY[0]; dcX[0] = yl; dcY[0] = ol;
            const float orr = yr - dcX[1] + dcR * dcY[1]; dcX[1] = yr; dcY[1] = orr;
            L[i] = ol;
            if (R) R[i] = orr;

            outPk = std::max (outPk, std::max (std::abs (ol), std::abs (orr)));
            const float sq = 0.5f * (ol * ol + orr * orr);
            meterSq = meterK * meterSq + (1 - meterK) * sq;
        }
        blockInPeak = inPk; blockOutPeak = outPk; blockGR = grMax;
        meterGR.store (grMem + grSlow + 20.0f * std::log10 (1.0f + kFast));
        meterOutDb.store (10.0f * std::log10 (meterSq + 1e-12f));
    }

    double sampleRate() const { return fs; }

private:
    double fs = 48000.0;
    float envA = 0, envR = 0, slowA = 0, slowR = 0, fastA = 0, fastR = 0, memA = 0, memR = 0, meterK = 0, smoothK = 0, dcR = 0;
    float env = 0, grSlow = 0, kFast = 0, grMem = 0, meterSq = 0;
    float makeupSm = 1, mixSm = 1, lastEmph = -1;
    float dcX[2] {}, dcY[2] {};
    Biquad hfShelf, scShelf, scLow, scHigh;
};
} // namespace opt
