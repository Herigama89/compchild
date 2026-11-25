// Black Tube DSP checks against the measured reference (1 kHz sine, attack 5, release 5).
// Reference values: steady gain of the reference plugin at each input level (BlackTube/docs/measurements.json).
#include "../Source/dsp/Opto.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>

static float gain (const char* thr, float ratio, int arMode, int vintage, float levelDb)
{
    const double sr = 48000.0, f = 1000.0;
    bt::Opto o; o.prepare (sr);
    o.s.thresholdOff = std::strcmp (thr, "off") == 0;
    o.s.thresholdDb = o.s.thresholdOff ? -20.0f : (float) std::atof (thr);
    o.s.ratio = ratio; o.s.arMode = (bt::ArMode) arMode; o.s.vintage = vintage != 0;
    const int n = (int) (sr * 1.5);
    std::vector<float> l (n), r (n);
    const float amp = std::pow (10.0f, levelDb / 20.0f);
    for (int i = 0; i < n; ++i) l[i] = r[i] = amp * (float) std::sin (2.0 * M_PI * f * i / sr);
    for (int i = 0; i < n; i += 256) o.process (l.data() + i, r.data() + i, std::min (256, n - i));
    double s = 0, c = 0; const int a = n - (int) (0.4 * sr);
    for (int i = a; i < n; ++i) { s += l[i] * std::sin (2.0 * M_PI * f * i / sr); c += l[i] * std::cos (2.0 * M_PI * f * i / sr); }
    return (float) (20.0 * std::log10 (2.0 * std::sqrt (s * s + c * c) / (n - a)) - levelDb);
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
    check ("thr -20, 6:1, manual, modern @ -40 dBFS", gain ("-20", 6, 2, 0, -40.0f), -0.08f, 0.8f);
    check ("thr -20, 6:1, manual, modern @ -30 dBFS", gain ("-20", 6, 2, 0, -30.0f), -4.65f, 0.8f);
    check ("thr -20, 6:1, manual, modern @ -20 dBFS", gain ("-20", 6, 2, 0, -20.0f), -12.44f, 0.8f);
    check ("thr -20, 6:1, manual, modern @ -10 dBFS", gain ("-20", 6, 2, 0, -10.0f), -20.16f, 0.8f);
    check ("thr -20, 6:1, manual, modern @ 0 dBFS", gain ("-20", 6, 2, 0, 0.0f), -27.45f, 0.8f);
    check ("thr -20, 2:1 @ -40 dBFS", gain ("-20", 2, 2, 0, -40.0f), -0.08f, 0.8f);
    check ("thr -20, 2:1 @ -30 dBFS", gain ("-20", 2, 2, 0, -30.0f), -3.66f, 0.8f);
    check ("thr -20, 2:1 @ -20 dBFS", gain ("-20", 2, 2, 0, -20.0f), -10.08f, 0.8f);
    check ("thr -20, 2:1 @ -10 dBFS", gain ("-20", 2, 2, 0, -10.0f), -16.01f, 0.8f);
    check ("thr -20, 2:1 @ 0 dBFS", gain ("-20", 2, 2, 0, 0.0f), -21.38f, 0.8f);
    check ("thr -20, 10:1 @ -40 dBFS", gain ("-20", 10, 2, 0, -40.0f), -0.08f, 0.8f);
    check ("thr -20, 10:1 @ -30 dBFS", gain ("-20", 10, 2, 0, -30.0f), -5.47f, 0.8f);
    check ("thr -20, 10:1 @ -20 dBFS", gain ("-20", 10, 2, 0, -20.0f), -14.64f, 0.8f);
    check ("thr -20, 10:1 @ -10 dBFS", gain ("-20", 10, 2, 0, -10.0f), -23.54f, 0.8f);
    check ("thr -20, 10:1 @ 0 dBFS", gain ("-20", 10, 2, 0, 0.0f), -30.95f, 0.8f);
    check ("threshold off @ -40 dBFS", gain ("off", 6, 2, 0, -40.0f), -0.08f, 0.8f);
    check ("threshold off @ -30 dBFS", gain ("off", 6, 2, 0, -30.0f), -0.08f, 0.8f);
    check ("threshold off @ -20 dBFS", gain ("off", 6, 2, 0, -20.0f), -0.08f, 0.8f);
    check ("threshold off @ -10 dBFS", gain ("off", 6, 2, 0, -10.0f), -0.08f, 0.8f);
    check ("threshold off @ 0 dBFS", gain ("off", 6, 2, 0, 0.0f), -0.08f, 0.8f);
    check ("thr -20, 6:1, vintage @ -40 dBFS", gain ("-20", 6, 2, 1, -40.0f), -1.47f, 0.8f);
    check ("thr -20, 6:1, vintage @ -30 dBFS", gain ("-20", 6, 2, 1, -30.0f), -4.98f, 0.8f);
    check ("thr -20, 6:1, vintage @ -20 dBFS", gain ("-20", 6, 2, 1, -20.0f), -11.24f, 0.8f);
    check ("thr -20, 6:1, vintage @ -10 dBFS", gain ("-20", 6, 2, 1, -10.0f), -18.81f, 0.8f);
    check ("thr -20, 6:1, vintage @ 0 dBFS", gain ("-20", 6, 2, 1, 0.0f), -27.16f, 0.8f);
    check ("thr -20, 6:1, fixed @ -40 dBFS", gain ("-20", 6, 0, 0, -40.0f), -0.08f, 0.8f);
    check ("thr -20, 6:1, fixed @ -30 dBFS", gain ("-20", 6, 0, 0, -30.0f), -3.19f, 0.8f);
    check ("thr -20, 6:1, fixed @ -20 dBFS", gain ("-20", 6, 0, 0, -20.0f), -10.33f, 0.8f);
    check ("thr -20, 6:1, fixed @ -10 dBFS", gain ("-20", 6, 0, 0, -10.0f), -17.81f, 0.8f);
    check ("thr -20, 6:1, fixed @ 0 dBFS", gain ("-20", 6, 0, 0, 0.0f), -25.23f, 0.8f);

    // timing: release time constant grows with the knob
    auto releaseTo63 = [] (float rel)
    {
        const double sr = 48000.0; bt::Opto o; o.prepare (sr);
        o.s.thresholdDb = -20.0f; o.s.release = rel;
        const int n = (int) (sr * 6.0); std::vector<float> l (n), r (n);
        for (int i = 0; i < n; ++i) { const float a = i < sr * 2 ? 0.5f : 0.0316f; l[i] = r[i] = a * (float) std::sin (2.0 * M_PI * 1000.0 * i / sr); }
        std::vector<float> gr; gr.reserve (n);
        for (int i = 0; i < n; ++i) { o.process (&l[i], &r[i], 1); gr.push_back (o.currentGRDb()); }
        const float g0 = gr[(size_t) (sr * 2) - 1], g1 = gr[(size_t) n - 1], tgt = g0 + 0.632f * (g1 - g0);
        for (int i = (int) (sr * 2); i < n; ++i) if (gr[(size_t) i] >= tgt) return (float) ((i - sr * 2) / sr * 1000.0);
        return 1.0e9f;
    };
    const float r0 = releaseTo63 (0), r5 = releaseTo63 (5), r10 = releaseTo63 (10);
    std::printf ("release 63%%: knob 0 %.0f ms, knob 5 %.0f ms, knob 10 %.0f ms (reference 43 / 1128 / 2583)\n", r0, r5, r10);
    check ("release knob 0 (ms)",  r0,  43.0f,  25.0f);
    check ("release knob 5 (ms)",  r5,  1128.0f, 250.0f);
    check ("release knob 10 (ms)", r10, 2583.0f, 600.0f);

    std::printf (fails ? "\n%d check(s) failed\n" : "\nall checks passed\n", fails);
    return fails ? 1 : 0;
}
