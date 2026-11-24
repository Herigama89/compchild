// VoxSqueeze DSP checks (1 kHz sine, steady state).
#include "../Source/dsp/VoxSqueeze.h"
#include <cstdio>
#include <cmath>
#include <vector>

struct Result { float gainDb, peakDb; };

static Result run (float levelDb, float comp, float gain = 0.0f, float gate = -80.0f, float freq = 1000.0f)
{
    const double sr = 48000.0;
    vx::Processor p; p.prepare (sr);
    p.s.compDb = comp; p.s.gainDb = gain; p.s.gateDb = gate;
    const int n = (int) (sr * 1.5);
    std::vector<float> l (n), r (n);
    const float amp = std::pow (10.0f, levelDb / 20.0f);
    for (int i = 0; i < n; ++i) l[i] = r[i] = amp * (float) std::sin (2.0 * M_PI * freq * i / sr);
    for (int i = 0; i < n; i += 256) p.process (l.data() + i, r.data() + i, std::min (256, n - i));
    double s = 0, c = 0; float pk = 0;                  // fundamental over the last 0.5 s
    const int from = n - (int) (sr * 0.5);
    for (int i = from; i < n; ++i)
    {
        s += l[i] * std::sin (2.0 * M_PI * freq * (i - p.latencySamples()) / sr);
        c += l[i] * std::cos (2.0 * M_PI * freq * (i - p.latencySamples()) / sr);
        pk = std::max (pk, std::fabs (l[i]));
    }
    const double a1 = 2.0 * std::sqrt (s * s + c * c) / (n - from);
    return { (float) (20.0 * std::log10 (std::max (a1, 1e-12)) - levelDb), 20.0f * std::log10 (std::max (pk, 1e-9f)) };
}

int main()
{
    int fails = 0;
    auto check = [&] (const char* what, float got, float want, float tol)
    {
        const bool ok = std::abs (got - want) <= tol;
        std::printf ("%-44s got %7.2f  want %7.2f  %s\n", what, got, want, ok ? "ok" : "FAIL");
        if (! ok) ++fails;
    };
    auto atMost = [&] (const char* what, float got, float limit)
    {
        const bool ok = got <= limit;
        std::printf ("%-44s got %7.2f  max  %7.2f  %s\n", what, got, limit, ok ? "ok" : "FAIL");
        if (! ok) ++fails;
    };

    check  ("COMP 0: unity at -20 dBFS",             run (-20,   0).gainDb,  0.0f, 0.1f);
    check  ("COMP -18: quiet -40 dBFS gets makeup",  run (-40, -18).gainDb,  vx::makeupFor (-18), 0.2f);
    check  ("COMP -18: -10 dBFS (curve)",            run (-10, -18).gainDb,  vx::makeupFor (-18) - vx::staticGR (-10 + 0.0f, -18) , 1.0f);
    atMost ("COMP -18: 0 dBFS peak stays <= ceiling",run (  0, -18).peakDb, -0.09f);
    atMost ("COMP -60: 0 dBFS peak stays <= ceiling",run (  0, -60).peakDb, -0.09f);
    atMost ("GAIN +10, COMP -30: peaks limited",     run ( -6, -30, 10).peakDb, -0.09f);
    check  ("GAIN -6 at COMP 0 (-20 dBFS)",          run (-20,   0, -6).gainDb, -6.0f, 0.1f);
    check  ("GATE -40: -30 dBFS passes",             run (-30,   0, 0, -40).gainDb,   0.0f, 0.2f);
    atMost ("GATE -40: -60 dBFS is gated (>40 dB)",  run (-60,   0, 0, -40).gainDb, -40.0f);
    check  ("GATE off: -60 dBFS passes",             run (-60,   0).gainDb,  0.0f, 0.2f);
    std::printf (fails ? "\n%d check(s) failed\n" : "\nall checks passed\n", fails);
    return fails ? 1 : 0;
}
