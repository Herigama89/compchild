#pragma once
// Compchild — stereo vari-mu limiter core. No JUCE dependency, so it can be unit-tested on its own.
//
// Model fitted to measurements of a vari-mu tube limiter plugin (see docs/measurements.json):
//   * below threshold the path is clean and linear (+8.55 dB fixed make-up at default input/output)
//   * static curve:  u = max(0, L - T);  GR = s * (u - w * (1 - exp(-u / w)))   [dB]
//       T(k) = -16.75 - 8.97 * ln(k) dBFS for threshold knob k in (0, 10];  k = 0 -> no compression
//       w = 11 dB knee (DC Bias widens / narrows it), s = 1 (limiting at the top)
//   * gain-reduction ballistics: one attack time; release in two stages,
//       61 % of the GR recovers with tau, the rest with 4 * tau  (TC1..TC4 measured, TC5/TC6 estimated)
//   * distortion comes from the gain riding inside low-frequency cycles (no static waveshaper)
//   * output hard ceiling at -0.04 dBFS
#include <cmath>
#include <algorithm>
#include <atomic>

namespace cc
{
    inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
    inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

    struct TimeConstant { float attackMs, tauFast, wFast, tauSlow; };

    // index 0..5 = TC 1..6
    inline TimeConstant timeConstant (int tc)
    {
        static const TimeConstant table[6] = {
            { 0.8f,  0.0237f, 0.60f, 0.098f },   // TC1 measured
            { 0.8f,  0.0733f, 0.61f, 0.291f },   // TC2 measured
            { 1.4f,  0.212f,  0.61f, 0.849f },   // TC3 measured (default)
            { 2.7f,  0.434f,  0.62f, 1.712f },   // TC4 measured
            { 1.4f,  0.212f,  0.55f, 2.5f   },   // TC5 estimated: TC3 front, long program-dependent tail
            { 0.8f,  0.040f,  0.45f, 4.5f   },   // TC6 estimated: fast front, very long tail
        };
        return table[std::clamp (tc, 0, 5)];
    }

    // threshold knob 0..10 -> onset level in dBFS (detector input, i.e. after Input Gain)
    inline float thresholdDb (float knob)
    {
        if (knob <= 0.02f) return 60.0f;                       // effectively off
        return -16.75f - 8.97f * std::log (knob);
    }

    inline float staticGR (float levelDb, float T, float w)
    {
        const float u = levelDb - T;
        if (u <= 0.0f) return 0.0f;
        return u - w * (1.0f - std::exp (-u / w));
    }

    struct ChannelSettings
    {
        float threshold = 5.0f;  // knob 0..10
        int   tc        = 2;     // 0..5  (TC1..TC6)
        float dcBias    = 0.0f;  // -1..+1, shapes the knee
        float inputDb   = 0.0f;  // input gain into the tube stage
    };

    // One gain-reduction channel: detector + ballistics
    class GainChannel
    {
    public:
        void prepare (double sampleRate) { sr = (float) sampleRate; reset(); setTC (tcIndex, true); setDetector (detTau, levelOffsetDb); setModulation (modBetaDefault, modPowDefault, modCornerDefault); }
        void setDetector (float tauSeconds, float offsetDb)
        {
            detTau = tauSeconds; levelOffsetDb = offsetDb;
            detRel = detTau > 0.0f ? std::exp (-1.0f / (detTau * sr)) : 0.0f;
        }
        void reset() { env = 0.0f; s1 = s2 = 0.0f; pk = 0.0f; mod = 0.0f; }

        void setTC (int tc, bool force = false)
        {
            if (tc == tcIndex && ! force) return;
            tcIndex = tc;
            const auto t = timeConstant (tc);
            aAtt  = 1.0f - std::exp (-1.0f / (t.attackMs * 0.001f * sr));
            aRel1 = 1.0f - std::exp (-1.0f / (t.tauFast * sr));
            aRel2 = 1.0f - std::exp (-1.0f / (t.tauSlow * sr));
            wFast = t.wFast;
        }

        // returns the GR target (dB) for this sample's detector input
        float target (float x, float T, float w)
        {
            const float a = std::abs (x);
            env = a > env ? a : env * detRel + a * (1.0f - detRel);
            return staticGR (gainToDb (env) + levelOffsetDb, T, w);
        }

        // vari-mu gain modulation: the tube's gain rides on the rectified signal inside each cycle.
        // Depth grows with gain reduction; a ~200 Hz low-pass gives the 1/f rise toward the low end.
        void setModulation (float beta, float power, float cornerHz)
        {
            modBeta = beta; modPow = power;
            modA = 1.0f - std::exp (-2.0f * 3.14159265f * cornerHz / sr);
            pkRel = std::exp (-1.0f / (0.05f * sr));
        }
        float modulationDb (float x, float grDb)
        {
            const float a = std::abs (x);
            pk = a > pk ? a : pk * pkRel;
            const float rect = pk > 1.0e-6f ? a / pk - 0.63662f : 0.0f;
            mod += (rect - mod) * modA;
            return grDb > 0.0f ? modBeta * std::pow (grDb, modPow) * mod : 0.0f;
        }

        // advance the ballistics toward a GR target, return the smoothed GR in dB
        float smooth (float tgt)
        {
            s1 += (tgt - s1) * (tgt > s1 ? aAtt : aRel1);
            s2 += (tgt - s2) * (tgt > s2 ? aAtt : aRel2);
            return wFast * s1 + (1.0f - wFast) * s2;
        }

        static constexpr float modBetaDefault = 0.0045f, modPowDefault = 1.67f, modCornerDefault = 210.0f;
        float detTau = 0.0f;          // 0 = instantaneous rectifier (best fit to the measured curve and distortion)
        float levelOffsetDb = 0.3f;   // detector calibration (fitted: curve within 0.25 dB of the reference)

    private:
        float sr = 48000.0f, env = 0.0f, s1 = 0.0f, s2 = 0.0f, detRel = 0.99f;
        float modBeta = 0.0f, modPow = 1.0f, modA = 0.02f, pkRel = 0.999f, pk = 0.0f, mod = 0.0f;
        float aAtt = 0.1f, aRel1 = 0.001f, aRel2 = 0.0005f, wFast = 0.61f;
        int tcIndex = 2;
    };

    enum class Mode { LR = 0, LatVert = 1, MidSide = 2 };

    class VariMu
    {
    public:
        static constexpr float makeupDb = 8.55f;     // fixed gain of the tube stage at default settings
        static constexpr float ceiling  = 0.99541f;  // -0.04 dBFS

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            for (auto& c : ch) c.prepare (sampleRate);
            smoothA = 1.0f - std::exp (-1.0f / (0.02f * (float) sampleRate));   // 20 ms smoothing for controls
            reset();
        }
        void reset() { for (auto& c : ch) c.reset(); grOut[0] = grOut[1] = 0.0f; }
        void setDetector (float tauSeconds, float offsetDb) { for (auto& c : ch) c.setDetector (tauSeconds, offsetDb); }
        void setModulation (float beta, float power, float cornerHz) { for (auto& c : ch) c.setModulation (beta, power, cornerHz); }

        // controls (set from the host thread before each block)
        ChannelSettings a, b;
        float outputDb = 0.0f;
        bool  link = true;
        Mode  mode = Mode::LR;
        bool  clipOutput = true;

        // gain reduction per channel in dB (for the meters)
        std::atomic<float> meterGR[2] { 0.0f, 0.0f };

        void process (float* left, float* right, int n)
        {
            const int numCh = right != nullptr ? 2 : 1;
            ch[0].setTC (a.tc);
            ch[1].setTC (numCh == 2 ? b.tc : a.tc);
            const float Ta = thresholdDb (a.threshold), Tb = thresholdDb (b.threshold);
            const float wa = 11.0f * std::pow (2.0f, a.dcBias), wb = 11.0f * std::pow (2.0f, b.dcBias);
            const float inTargetA = dbToGain (a.inputDb), inTargetB = dbToGain (numCh == 2 ? b.inputDb : a.inputDb);
            const float outTarget = dbToGain (makeupDb + outputDb);
            float peak0 = 0.0f, peak1 = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                inGA += (inTargetA - inGA) * smoothA;
                inGB += (inTargetB - inGB) * smoothA;
                outG += (outTarget - outG) * smoothA;

                float l = left[i];
                float r = numCh == 2 ? right[i] : l;
                float p = l, q = r;                       // the two processed signals
                if (mode != Mode::LR && numCh == 2) { p = 0.5f * (l + r); q = 0.5f * (l - r); }
                p *= inGA; q *= inGB;

                float t0 = ch[0].target (p, Ta, wa);
                float t1 = ch[1].target (q, numCh == 2 ? Tb : Ta, numCh == 2 ? wb : wa);
                if (mode == Mode::MidSide) t1 = 0.0f;     // side passes uncompressed
                if (link && mode == Mode::LR) t0 = t1 = std::max (t0, t1);

                const float g0 = ch[0].smooth (t0), g1 = ch[1].smooth (t1);
                peak0 = std::max (peak0, g0); peak1 = std::max (peak1, g1);

                const float m0 = ch[0].modulationDb (p, g0), m1 = ch[1].modulationDb (q, g1);
                p *= dbToGain (-(g0 + m0));
                q *= dbToGain (-(g1 + m1));
                if (mode != Mode::LR && numCh == 2) { l = p + q; r = p - q; } else { l = p; r = q; }

                l *= outG; r *= outG;
                if (clipOutput) { l = std::clamp (l, -ceiling, ceiling); r = std::clamp (r, -ceiling, ceiling); }
                left[i] = l;
                if (numCh == 2) right[i] = r;
            }
            meterGR[0].store (peak0);
            meterGR[1].store (numCh == 2 ? peak1 : peak0);
        }

    private:
        GainChannel ch[2];
        double sr = 48000.0;
        float smoothA = 0.001f, inGA = 1.0f, inGB = 1.0f, outG = dbToGain (makeupDb), grOut[2] {};
    };
}
