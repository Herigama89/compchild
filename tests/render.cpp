// Offline render harness: render <in.f32> <out.f32> <peak> <gain> <limit> [emphasis] [mix]
// raw mono float32 at 48 kHz in and out (processed as dual-mono stereo)
#include "../Source/dsp/Opto2A.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
int main (int argc, char** argv)
{
    if (argc < 6) { std::fprintf (stderr, "usage\n"); return 1; }
    FILE* f = std::fopen (argv[1], "rb"); std::vector<float> x;
    float b[4096]; size_t k; while ((k = std::fread (b, 4, 4096, f)) > 0) x.insert (x.end(), b, b + k); std::fclose (f);
    opt::Opto2A o; o.peakReduction = (float) atof (argv[3]); o.gain = (float) atof (argv[4]); o.limit = atoi (argv[5]) != 0;
    if (argc > 6) o.emphasis = (float) atof (argv[6]);
    if (argc > 7) o.mix = (float) atof (argv[7]);
    o.prepare (48000.0);
    std::vector<float> L (x), R (x);
    for (size_t i = 0; i < x.size(); i += 512) { int n = (int) std::min<size_t> (512, x.size() - i); o.process (L.data() + i, R.data() + i, n); }
    FILE* g = std::fopen (argv[2], "wb"); std::fwrite (L.data(), 4, L.size(), g); std::fclose (g);
    return 0;
}
