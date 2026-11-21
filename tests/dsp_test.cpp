// DSP checks against the measured reference leveler (1 kHz sine, steady state).
#include "../Source/dsp/Opto2A.h"
#include <cstdio>
#include <cmath>
#include <vector>

static float steadyGainDb (float levelDb, float peak, float gain, bool limit = false, float freq = 1000.0f)
{
    const double sr = 48000.0;
    opt::Opto2A o; o.peakReduction = peak; o.gain = gain; o.limit = limit; o.prepare (sr);
    const int n = (int) (1.5 * sr);
    std::vector<float> l (n), r (n);
    const float amp = std::pow (10.0f, levelDb / 20.0f);
    for (int i = 0; i < n; ++i) l[i] = r[i] = amp * (float) std::sin (2.0 * M_PI * freq * i / sr);
    for (int i = 0; i < n; i += 256) o.process (l.data() + i, r.data() + i, std::min (256, n - i));
    double s = 0, c = 0; const int a = n - (int) (0.5 * sr);
    for (int i = a; i < n; ++i) { s += l[i] * std::sin (2.0 * M_PI * freq * i / sr); c += l[i] * std::cos (2.0 * M_PI * freq * i / sr); }
    const double a1 = 2.0 * std::sqrt (s * s + c * c) / (n - a);
    return (float) (20.0 * std::log10 (a1) - levelDb);
}

int main()
{
    int fails = 0;
    auto check = [&] (const char* what, float got, float want, float tol)
    {
        const bool ok = std::abs (got - want) <= tol;
        std::printf ("%-34s got %7.2f  want %7.2f  %s\n", what, got, want, ok ? "ok" : "FAIL");
        if (! ok) ++fails;
    };
    check ("PR 0, gain 30, -40 dBFS",   steadyGainDb (-40,  0, 30),   5.62f, 0.15f);
    check ("PR 25, -20 dBFS",           steadyGainDb (-20, 25, 30),   5.61f, 0.4f);
    check ("PR 25, 0 dBFS",             steadyGainDb (  0, 25, 30),  -1.84f, 0.7f);
    check ("PR 55, -20 dBFS",           steadyGainDb (-20, 55, 30),   2.32f, 0.7f);
    check ("PR 55, 0 dBFS",             steadyGainDb (  0, 55, 30), -13.50f, 0.7f);
    check ("PR 70, -20 dBFS",           steadyGainDb (-20, 70, 30),  -3.64f, 0.7f);
    check ("PR 70, 0 dBFS",             steadyGainDb (  0, 70, 30), -19.48f, 0.7f);
    check ("PR 70 LIMIT, 0 dBFS",       steadyGainDb (  0, 70, 30, true), -24.18f, 1.2f);
    check ("gain 60, -40 dBFS",         steadyGainDb (-40,  0, 60),  18.40f, 0.15f);
    check ("gain 60, 0 dBFS (tube clip)", steadyGainDb (0,  0, 60),   6.32f, 0.5f);
    std::printf (fails ? "\n%d check(s) failed\n" : "\nall checks passed\n", fails);
    return fails ? 1 : 0;
}
