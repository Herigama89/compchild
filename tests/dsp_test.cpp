// DSP checks against the measured reference (1 kHz sine, defaults: threshold 5, TC 3).
#include "../Source/dsp/VariMu.h"
#include <cstdio>
#include <cmath>
#include <vector>

static float steadyGainDb (float levelDb, float threshold, int tc, float freq = 1000.0f, float inputDb = 0.0f)
{
    const double sr = 48000.0;
    cc::VariMu vm; vm.prepare (sr);
    vm.a.threshold = vm.b.threshold = threshold; vm.a.tc = vm.b.tc = tc - 1;
    vm.a.inputDb = vm.b.inputDb = inputDb;
    const int n = (int) sr;  // 1 s
    std::vector<float> l (n), r (n);
    const float amp = std::pow (10.0f, levelDb / 20.0f);
    for (int i = 0; i < n; ++i) l[i] = r[i] = amp * (float) std::sin (2.0 * M_PI * freq * i / sr);
    for (int i = 0; i < n; i += 256) vm.process (l.data() + i, r.data() + i, std::min (256, n - i));
    double s = 0, c = 0;                         // fundamental amplitude over the last 0.5 s
    for (int i = n / 2; i < n; ++i) { s += l[i] * std::sin (2.0 * M_PI * freq * i / sr); c += l[i] * std::cos (2.0 * M_PI * freq * i / sr); }
    const double a1 = 2.0 * std::sqrt (s * s + c * c) / (n / 2);
    return (float) (20.0 * std::log10 (a1) - levelDb);
}

int main()
{
    int fails = 0;
    auto check = [&] (const char* what, float got, float want, float tol)
    {
        const bool ok = std::abs (got - want) <= tol;
        std::printf ("%-36s got %7.2f  want %7.2f  %s\n", what, got, want, ok ? "ok" : "FAIL");
        if (! ok) ++fails;
    };
    // measured TubeChild-style reference values (dB gain of a 1 kHz sine)
    check ("below threshold (-40 dBFS)",      steadyGainDb (-40, 5, 3),   8.55f, 0.1f);
    check ("knee (-20 dBFS)",                 steadyGainDb (-20, 5, 3),   4.25f, 0.6f);
    check ("heavy (-10 dBFS)",                steadyGainDb (-10, 5, 3),  -3.51f, 0.6f);
    check ("limiting (0 dBFS)",               steadyGainDb (  0, 5, 3), -12.56f, 0.6f);
    check ("threshold 10 (-10 dBFS)",         steadyGainDb (-10, 10, 3), -8.80f, 0.6f);
    check ("threshold 0 = clean (-20 dBFS)",  steadyGainDb (-20, 0, 3),   8.55f, 0.1f);
    check ("input +5.94 dB, thr 0 (-30 dBFS)",steadyGainDb (-30, 0, 3, 1000.0f, 5.94f), 14.49f, 0.1f);
    std::printf (fails ? "\n%d check(s) failed\n" : "\nall checks passed\n", fails);
    return fails ? 1 : 0;
}
