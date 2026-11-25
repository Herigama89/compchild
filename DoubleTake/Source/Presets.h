#pragma once
#include <juce_core/juce_core.h>
#include <array>

// Factory presets. Each voice: on, gain dB, pan, delay ms, feedback %, octave, detune ct, depth ct, rate Hz
struct DtVoicePreset { bool on; float gain, pan, delay, fdbk; int oct; float detune, depth, rate; };
struct DtPreset
{
    const char* name;
    bool four; float mix, dryGain, lowGain, lowFreq, highGain, highFreq, fbCut;
    std::array<DtVoicePreset, 4> v;
};

inline const std::array<DtPreset, 8>& dtPresets()
{
    static const std::array<DtPreset, 8> p { {
        { "Natural Double", false, 50, 0, -4, 120, 2, 6000, 80,
          { { { true, -3, -60, 12, 0, 0, 9, 4, 0.6f }, { true, -3, 60, 19, 0, 0, -11, 5, 0.45f },
              { true, -6, -100, 26, 0, 0, 17, 6, 0.8f }, { true, -6, 100, 33, 0, 0, -15, 6, 0.7f } } } },
        { "Tight Thickener", false, 45, 0, -6, 150, 0, 6000, 80,
          { { { true, -4, -20, 8, 0, 0, 6, 3, 0.5f }, { true, -4, 20, 14, 0, 0, -6, 3, 0.4f },
              { false, -6, -60, 20, 0, 0, 10, 4, 0.6f }, { false, -6, 60, 24, 0, 0, -10, 4, 0.6f } } } },
        { "Quad Stack - Wide", true, 55, 0, -5, 140, 3, 7000, 100,
          { { { true, -2, -60, 10, 0, 0, -5, 8, 0.9f }, { true, -2, 55, 30, 0, 0, 5, 10, 1.1f },
              { true, -5, -100, 20, 0, 0, 12, 8, 0.7f }, { true, -5, 100, 40, 0, 0, -12, 8, 1.3f } } } },
        { "Chorus Cloud", true, 50, 0, -3, 200, 2, 8000, 80,
          { { { true, -4, -80, 7, 0, 0, 0, 18, 0.35f }, { true, -4, 80, 11, 0, 0, 0, 22, 0.5f },
              { true, -6, -35, 15, 0, 0, 0, 25, 0.8f }, { true, -6, 35, 18, 0, 0, 0, 20, 1.2f } } } },
        { "Slapback Pair", false, 40, 0, -8, 250, -2, 5000, 200,
          { { { true, -6, -35, 85, 15, 0, 0, 0, 1.0f }, { true, -6, 35, 100, 10, 0, 3, 0, 1.0f },
              { false, -8, -70, 60, 0, 0, 0, 0, 1.0f }, { false, -8, 70, 70, 0, 0, 0, 0, 1.0f } } } },
        { "Octave Below", false, 50, 0, 0, 80, -3, 4000, 80,
          { { { true, -8, 0, 0, 0, -1, 0, 0, 1.0f }, { false, -8, 0, 12, 0, -1, 0, 0, 1.0f },
              { false, -8, -50, 20, 0, 0, 0, 0, 1.0f }, { false, -8, 50, 25, 0, 0, 0, 0, 1.0f } } } },
        { "Octave Spread", false, 50, 0, -2, 100, -2, 6000, 80,
          { { { true, -12, -45, 5, 0, 1, 0, 3, 0.5f }, { true, -9, 45, 9, 0, -1, 0, 3, 0.4f },
              { false, -10, -80, 15, 0, 0, 8, 4, 0.6f }, { false, -10, 80, 18, 0, 0, -8, 4, 0.6f } } } },
        { "Robot Choir", true, 60, -6, -4, 120, 4, 5000, 300,
          { { { true, -4, -70, 24, 45, 0, 25, 0, 1.0f }, { true, -4, 70, 31, 45, 0, -25, 0, 1.0f },
              { true, -7, -30, 47, 35, 1, 0, 0, 1.0f }, { true, -7, 30, 53, 35, -1, 0, 0, 1.0f } } } },
    } };
    return p;
}
