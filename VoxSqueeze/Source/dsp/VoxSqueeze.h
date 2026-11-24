// VoxSqueeze DSP core (header-only, no JUCE): a one-fader vocal leveler.
// Signal path:  gate (downward expander)  ->  compressor with automatic makeup gain
//               ->  output gain  ->  0 dBFS look-ahead peak limiter.
// The COMP fader lowers the threshold and raises the makeup gain together, so a
// full-scale peak stays at full scale while everything quieter is brought up.
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <utility>
#include <vector>

namespace vx
{
inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

struct Settings
{
    float gateDb    = -80.0f;   // -80 = off; otherwise the expander threshold (dBFS)
    float compDb    = 0.0f;     // 0 .. -60: threshold of the compressor (dBFS)
    float gainDb    = 0.0f;     // output gain into the limiter; <= -80 = silence
    float ceilingDb = -0.1f;    // limiter ceiling
};

// Ratio rises as the fader goes down: gentle at the top, close to limiting at the bottom.
inline float ratioFor (float compDb)       { return 2.5f + 5.5f * std::clamp (-compDb / 60.0f, 0.0f, 1.0f); }
inline float makeupFor (float compDb)      { return -compDb * (1.0f - 1.0f / ratioFor (compDb)); }
constexpr float kKneeDb = 6.0f;

// Static compressor curve: gain reduction (positive dB) for a level in dBFS.
inline float staticGR (float levelDb, float compDb)
{
    const float R = ratioFor (compDb), over = levelDb - compDb, s = 1.0f - 1.0f / R;
    if (over >  kKneeDb * 0.5f) return over * s;
    if (over > -kKneeDb * 0.5f) { const float t = over + kKneeDb * 0.5f; return s * t * t / (2.0f * kKneeDb); }
    return 0.0f;
}

class Processor
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        auto c = [this] (float ms) { return std::exp (-1.0f / (float) (fs * ms * 0.001)); };
        detA  = c (0.3f);   detR  = c (40.0f);     // level detector
        gateA = c (0.5f);   gateR = c (90.0f);     // gate opens fast, closes over ~90 ms
        holdLen = (int) (fs * 0.030);              // 30 ms gate hold
        fastA = c (3.0f);   fastR = c (70.0f);     // fast follower: transients
        slowA = c (200.0f); slowR = c (900.0f);    // slow follower: sustained level (auto release)
        limRel = c (60.0f);
        look = std::max (1, (int) std::round (fs * 0.0015));   // 1.5 ms look-ahead
        for (auto& d : delay) d.assign ((size_t) look, 0.0f);
        box.assign ((size_t) look + 1, 1.0f);
        reset();
    }

    void reset()
    {
        env = 0.0f; gateDbNow = 0.0f; hold = 0; fastGr = slowGr = 0.0f; limG = 1.0f;
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        std::fill (box.begin(), box.end(), 1.0f);
        boxSum = (float) box.size(); boxPos = 0; dPos = 0; idx = 0; minQ.clear();
        meterGR.store (0.0f); meterLim.store (0.0f); meterGate.store (0.0f);
    }

    int latencySamples() const { return look; }

    // In place. r may be nullptr (mono).
    void process (float* l, float* r, int n)
    {
        const float T = s.compDb, makeup = makeupFor (T);
        const float outGain = s.gainDb <= -79.9f ? 0.0f : dbToGain (s.gainDb);
        const float ceiling = dbToGain (s.ceilingDb);
        const bool gateOn = s.gateDb > -79.9f;
        float grPeak = 0.0f, limPeak = 0.0f, gatePeak = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float xl = l[i], xr = r ? r[i] : xl;

            // stereo-linked peak detector
            const float pk = std::max (std::fabs (xl), std::fabs (xr));
            env = pk > env ? detA * env + (1 - detA) * pk : detR * env + (1 - detR) * pk;
            const float lvl = gainToDb (env);

            // gate: 1:4 downward expander, 70 dB range, 30 ms hold
            float gTarget = 0.0f;
            if (gateOn)
            {
                if (lvl >= s.gateDb)  hold = holdLen;
                else if (hold > 0)    --hold;
                else                  gTarget = std::max (-70.0f, (lvl - s.gateDb) * 3.0f);
            }
            const float gc = gTarget > gateDbNow ? gateA : gateR;
            gateDbNow = gc * gateDbNow + (1 - gc) * gTarget;

            // compressor on the gated level, with program-dependent (two-follower) release
            const float target = staticGR (lvl + gateDbNow, T);
            fastGr = target > fastGr ? fastA * fastGr + (1 - fastA) * target : fastR * fastGr + (1 - fastR) * target;
            slowGr = target > slowGr ? slowA * slowGr + (1 - slowA) * target : slowR * slowGr + (1 - slowR) * target;
            // short peaks: the fast follower lets go quickly; sustained singing builds the
            // slow follower, which holds the release back (never more than 3 dB over the target)
            const float gr = std::max (fastGr, std::min (slowGr, target + 3.0f));

            const float g = dbToGain (gateDbNow - gr + makeup) * outGain;
            const float yl = xl * g, yr = xr * g;

            // look-ahead limiter: min of required gain over the window, release, box smoothing
            const float p2 = std::max (std::fabs (yl), std::fabs (yr));
            const float need = p2 > ceiling ? ceiling / p2 : 1.0f;
            while (! minQ.empty() && minQ.back().second >= need) minQ.pop_back();
            minQ.emplace_back (idx, need);
            while (minQ.front().first < idx - look) minQ.pop_front();
            const float held = minQ.front().second;
            limG = held < limG ? held : limRel * limG + (1 - limRel) * held;
            boxSum += limG - box[(size_t) boxPos];
            box[(size_t) boxPos] = limG;
            boxPos = (boxPos + 1) % (int) box.size();
            const float lg = std::min (1.0f, boxSum / (float) box.size());
            ++idx;

            // delay the audio by the look-ahead, then apply the limiter gain
            const float dl = delay[0][(size_t) dPos], dr = delay[1][(size_t) dPos];
            delay[0][(size_t) dPos] = yl; delay[1][(size_t) dPos] = yr;
            dPos = (dPos + 1) % look;
            l[i] = std::clamp (dl * lg, -ceiling, ceiling);
            if (r) r[i] = std::clamp (dr * lg, -ceiling, ceiling);

            grPeak   = std::max (grPeak, gr);
            limPeak  = std::max (limPeak, -gainToDb (lg));
            gatePeak = std::min (gatePeak, gateDbNow);
        }
        meterGR.store (grPeak);
        meterLim.store (limPeak);
        meterGate.store (-gatePeak);   // dB of gate attenuation (positive)
    }

    Settings s;
    std::atomic<float> meterGR { 0.0f }, meterLim { 0.0f }, meterGate { 0.0f };

private:
    double fs = 48000.0;
    float detA = 0, detR = 0, gateA = 0, gateR = 0, fastA = 0, fastR = 0, slowA = 0, slowR = 0, limRel = 0;
    float env = 0, gateDbNow = 0, fastGr = 0, slowGr = 0, limG = 1, boxSum = 1;
    int hold = 0, holdLen = 0, look = 1, boxPos = 0, dPos = 0;
    long long idx = 0;
    std::vector<float> delay[2], box;
    std::deque<std::pair<long long, float>> minQ;
};
} // namespace vx
