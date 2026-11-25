// DoubleTake DSP checks.
#include "../Source/dsp/DoubleTake.h"
#include <cstdio>
#include <cmath>
#include <vector>

static const double SR = 48000.0;

static dt::Settings dryOnly()
{
    dt::Settings s; s.mix = 0.0f;
    for (auto& v : s.v) v.on = false;
    return s;
}

static dt::Settings oneVoice (float delayMs, int oct = 0, float cents = 0.0f)
{
    dt::Settings s; s.mix = 100.0f; s.dryOn = false; s.fourVoices = false;
    for (auto& v : s.v) v.on = false;
    s.v[0].on = true; s.v[0].delayMs = delayMs; s.v[0].octave = oct; s.v[0].detuneCents = cents; s.v[0].depthCents = 0; s.v[0].pan = 0;
    return s;
}

static void run (dt::Settings s, std::vector<float>& l, std::vector<float>& r)
{
    dt::Processor p; p.prepare (SR, 256); p.s = s;
    for (size_t i = 0; i < l.size(); i += 256) p.process (l.data() + i, r.data() + i, (int) std::min<size_t> (256, l.size() - i));
}

static double freqOf (const std::vector<float>& x, size_t from, double around)
{
    // strongest spectral line within +-8 % of the expected frequency (Hann-windowed DFT, 0.1 Hz steps)
    const size_t n = x.size() - from;
    double best = 0, bestF = 0;
    for (double f = around * 0.92; f <= around * 1.08; f += 0.1)
    {
        double re = 0, im = 0;
        for (size_t i = 0; i < n; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2 * M_PI * i / (n - 1));
            re += w * x[from + i] * std::cos (2 * M_PI * f * i / SR);
            im += w * x[from + i] * std::sin (2 * M_PI * f * i / SR);
        }
        if (re * re + im * im > best) { best = re * re + im * im; bestF = f; }
    }
    return bestF;
}

static double rmsDb (const std::vector<float>& x, size_t from)
{
    double a = 0; for (size_t i = from; i < x.size(); ++i) a += (double) x[i] * x[i];
    return 10.0 * std::log10 (a / (double) (x.size() - from) + 1e-20);
}

int main()
{
    int fails = 0;
    auto check = [&] (const char* what, double got, double want, double tol)
    {
        const bool ok = std::abs (got - want) <= tol;
        std::printf ("%-48s got %9.2f  want %9.2f  %s\n", what, got, want, ok ? "ok" : "FAIL");
        if (! ok) ++fails;
    };
    const size_t N = (size_t) (SR * 1.0);

    { // dry only, align off: bit-transparent level
        std::vector<float> l (N), r (N);
        for (size_t i = 0; i < N; ++i) l[i] = r[i] = 0.5f * (float) std::sin (2 * M_PI * 440 * i / SR);
        auto s = dryOnly(); s.align = false; run (s, l, r);
        check ("dry only: level unchanged (dB)", rmsDb (l, N / 2), 20 * std::log10 (0.5 / std::sqrt (2.0)), 0.05);
    }
    { // impulse through one voice: arrives at delay + shifter lag
        std::vector<float> l (N, 0.0f), r (N, 0.0f); l[1000] = r[1000] = 1.0f;
        run (oneVoice (20.0f), l, r);
        size_t at = 0; float mx = 0; for (size_t i = 0; i < N; ++i) if (std::fabs (l[i]) > mx) { mx = std::fabs (l[i]); at = i; }
        dt::Processor p; p.prepare (SR, 256); p.s.align = true;
        check ("voice delay 20 ms: impulse position (ms)", (double) (at - 1000) / SR * 1000.0, 20.0 + p.latencySamples() / SR * 1000.0, 0.1);
    }
    { // pitch. The test tone is chosen so the two read heads (half a 24 ms window apart) are in phase,
      // which isolates the pitch-ratio maths from the normal grain crossfade of a rotating-tap shifter.
        const double f0 = 48000.0 / 576.0 * 5.0;   // 416.67 Hz
        for (auto c : { std::pair<int, float> { 0, 100.0f }, { 1, 0.0f }, { -1, 0.0f }, { 0, -12.0f } })
        {
            std::vector<float> l (N), r (N);
            for (size_t i = 0; i < N; ++i) l[i] = r[i] = 0.5f * (float) std::sin (2 * M_PI * f0 * i / SR);
            run (oneVoice (10.0f, c.first, c.second), l, r);
            const double want = f0 * std::pow (2.0, c.first + c.second / 1200.0);
            char name[80]; std::snprintf (name, sizeof name, "pitch oct %+d detune %+.0f ct (Hz)", c.first, c.second);
            check (name, freqOf (l, N / 2, want), want, want * 0.004);
        }
    }
    { // feedback 90 % with low-cut stays stable and decays
        std::vector<float> l (N * 3, 0.0f), r (N * 3, 0.0f);
        for (size_t i = 0; i < 2000; ++i) l[i] = r[i] = 0.5f * (float) std::sin (2 * M_PI * 300 * i / SR);
        auto s = oneVoice (40.0f); s.v[0].feedback = 90.0f; run (s, l, r);
        float pk = 0; bool finite = true; for (auto x : l) { pk = std::max (pk, std::fabs (x)); finite &= std::isfinite (x); }
        check ("feedback 90%: output finite (1 = yes)", finite ? 1 : 0, 1, 0);
        check ("feedback 90%: tail after 2.5 s below -40 dB", rmsDb (l, (size_t) (SR * 2.5)) < -40.0 ? 1 : 0, 1, 0);
        check ("feedback 90%: peak bounded (<= 1)", pk <= 1.0f ? 1 : 0, 1, 0);
    }
    { // pan hard left: right channel silent
        std::vector<float> l (N), r (N);
        for (size_t i = 0; i < N; ++i) l[i] = r[i] = 0.5f * (float) std::sin (2 * M_PI * 440 * i / SR);
        auto s = oneVoice (10.0f); s.v[0].pan = -100.0f; run (s, l, r);
        check ("voice pan L100: right channel (dB)", rmsDb (r, N / 2) < -90.0 ? -100.0 : rmsDb (r, N / 2), -100.0, 0.0);
    }
    { // shelves: +6 dB low shelf boosts 50 Hz by ~6 dB, leaves 5 kHz alone
        for (auto f : { 50.0, 5000.0 })
        {
            std::vector<float> l (N), r (N);
            for (size_t i = 0; i < N; ++i) l[i] = r[i] = 0.25f * (float) std::sin (2 * M_PI * f * i / SR);
            auto s = oneVoice (10.0f); s.lowGainDb = 6.0f; s.lowFreq = 300.0f; s.v[0].pan = 0; run (s, l, r);
            // a centred voice is -3 dB per side (equal-power pan)
            const double got = rmsDb (l, N / 2) - (20 * std::log10 (0.25 / std::sqrt (2.0)) - 3.01);
            check (f < 100 ? "low shelf +6 dB @300 Hz: gain at 50 Hz" : "low shelf +6 dB @300 Hz: gain at 5 kHz", got, f < 100 ? 6.0 : 0.0, 0.6);
        }
    }
    std::printf (fails ? "\n%d check(s) failed\n" : "\nall checks passed\n", fails);
    return fails ? 1 : 0;
}
