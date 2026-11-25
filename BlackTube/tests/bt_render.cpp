// Offline renderer for the Black Tube DSP core: reads a WAV, processes it, writes a 32-bit float WAV.
#include "../Source/dsp/Opto.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>

static bool readWav (const char* path, std::vector<float>& L, std::vector<float>& R, int& sr)
{
    FILE* f = std::fopen (path, "rb"); if (! f) return false;
    char id[4]; uint32_t sz; std::fread (id, 1, 4, f); std::fread (&sz, 4, 1, f); std::fread (id, 1, 4, f);
    int fmt = 0, chn = 0, bits = 0; std::vector<uint8_t> data;
    while (std::fread (id, 1, 4, f) == 4 && std::fread (&sz, 4, 1, f) == 1)
    {
        if (! std::memcmp (id, "fmt ", 4)) { std::vector<uint8_t> b (sz); std::fread (b.data(), 1, sz, f);
            fmt = b[0] | b[1] << 8; chn = b[2] | b[3] << 8; sr = (int) (b[4] | b[5] << 8 | b[6] << 16 | (uint32_t) b[7] << 24); bits = b[14] | b[15] << 8;
            if (fmt == 0xFFFE) fmt = b[24] | b[25] << 8; }
        else if (! std::memcmp (id, "data", 4)) { data.resize (sz); std::fread (data.data(), 1, sz, f); }
        else std::fseek (f, sz + (sz & 1), SEEK_CUR);
    }
    std::fclose (f);
    const int bps = bits / 8; const size_t frames = data.size() / (bps * chn);
    L.resize (frames); R.resize (frames);
    for (size_t i = 0; i < frames; ++i)
        for (int c = 0; c < std::min (chn, 2); ++c)
        {
            const uint8_t* p = data.data() + (i * chn + c) * bps; float v = 0;
            if (fmt == 3 && bits == 32) std::memcpy (&v, p, 4);
            else if (bits == 16) v = (int16_t) (p[0] | p[1] << 8) / 32768.0f;
            else if (bits == 24) v = ((int32_t) ((p[0] << 8) | (p[1] << 16) | ((uint32_t) p[2] << 24)) >> 8) / 8388608.0f;
            (c == 0 ? L : R)[i] = v;
            if (chn == 1) R[i] = v;
        }
    return true;
}

static void writeWav (const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (path, "wb");
    const uint32_t frames = (uint32_t) L.size(), dataSz = frames * 8, riff = 36 + dataSz, fmtSz = 16, rate = (uint32_t) sr, bpsec = rate * 8;
    const uint16_t tag = 3, chn = 2, align = 8, bits = 32;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f); std::fwrite ("WAVEfmt ", 1, 8, f); std::fwrite (&fmtSz, 4, 1, f);
    std::fwrite (&tag, 2, 1, f); std::fwrite (&chn, 2, 1, f); std::fwrite (&rate, 4, 1, f); std::fwrite (&bpsec, 4, 1, f);
    std::fwrite (&align, 2, 1, f); std::fwrite (&bits, 2, 1, f); std::fwrite ("data", 1, 4, f); std::fwrite (&dataSz, 4, 1, f);
    for (uint32_t i = 0; i < frames; ++i) { std::fwrite (&L[i], 4, 1, f); std::fwrite (&R[i], 4, 1, f); }
    std::fclose (f);
}

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("usage: bt_render in.wav out.wav [threshold|off] [ratio] [attack] [release] [armode 0 fix 1 fixman 2 man] [vintage 0/1] [parallel 0..1] [outputDb] [lowcut 0/1/2]\n"); return 1; }
    std::vector<float> L, R; int sr = 48000;
    if (! readWav (argv[1], L, R, sr)) { std::printf ("cannot read %s\n", argv[1]); return 1; }
    auto arg = [&] (int i, float d) { return argc > i ? (float) std::atof (argv[i]) : d; };
    bt::Opto o; o.prepare (sr);
    o.s.thresholdOff = argc > 3 && std::string (argv[3]) == "off";
    o.s.thresholdDb  = o.s.thresholdOff ? -20.0f : arg (3, -20.0f);
    o.s.ratio   = arg (4, 6.0f);
    o.s.attack  = arg (5, 5.0f);
    o.s.release = arg (6, 5.0f);
    o.s.arMode  = (bt::ArMode) (int) arg (7, 2.0f);
    o.s.vintage = arg (8, 0.0f) > 0.5f;
    o.s.parallel = arg (9, 1.0f);
    o.s.outputDb = arg (10, 0.0f);
    o.s.lowCut   = (int) arg (11, 0.0f);
    const int block = 512;
    for (size_t i = 0; i < L.size(); i += block)
        o.process (L.data() + i, R.data() + i, (int) std::min<size_t> (block, L.size() - i));
    writeWav (argv[2], L, R, sr);
    return 0;
}
