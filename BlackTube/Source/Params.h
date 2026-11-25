#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace btids
{
    inline constexpr const char* output    = "output";
    inline constexpr const char* ratio     = "ratio";
    inline constexpr const char* threshold = "threshold";
    inline constexpr const char* attack    = "attack";
    inline constexpr const char* release   = "release";
    inline constexpr const char* arSelect  = "arSelect";
    inline constexpr const char* scBus     = "scBus";
    inline constexpr const char* lowCut    = "lowCut";
    inline constexpr const char* parallel  = "parallel";
    inline constexpr const char* power     = "power";
    inline constexpr const char* gen       = "generation";
    inline constexpr const char* meter     = "meter";
}

namespace btparam
{
    // Every knob sweeps 280 degrees (-140 .. +140). The parameter's 0..1 position is that sweep,
    // and the value mappings put each value under its engraved label on the panel.

    // Output: off at -140 deg, -10 dB at -84, then 10 dB per 56 deg up to +30 dB
    inline float outputFrom01 (float p) { return p < 0.2f ? -30.0f + 20.0f * p / 0.2f : -10.0f + 40.0f * (p - 0.2f) / 0.8f; }
    inline float outputTo01 (float v)   { return v < -10.0f ? 0.2f * (v + 30.0f) / 20.0f : 0.2f + 0.8f * (v + 10.0f) / 40.0f; }
    inline bool  outputIsOff (float v)  { return v <= -29.5f; }

    // Threshold: off at -140 deg, 0 dB at -84, then -10 dB per 56 deg down to -40 dB
    inline float thresholdFrom01 (float p) { return p < 0.2f ? 10.0f * (1.0f - p / 0.2f) : -40.0f * (p - 0.2f) / 0.8f; }
    inline float thresholdTo01 (float v)   { return v > 0.0f ? 0.2f * (1.0f - v / 10.0f) : 0.2f + 0.8f * (-v / 40.0f); }
    inline bool  thresholdIsOff (float v)  { return v >= 9.9f; }
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createBlackTubeLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto custom = [] (float lo, float hi, std::function<float (float)> from01, std::function<float (float)> to01)
    {
        NormalisableRange<float> r (lo, hi,
            [from01] (float, float, float x) { return from01 (x); },
            [to01]   (float, float, float v) { return to01 (v); });
        return r;
    };

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { btids::output, 1 }, "Output Volume",
        custom (-30.0f, 30.0f, btparam::outputFrom01, btparam::outputTo01), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            { return btparam::outputIsOff (v) ? String ("off") : (v > 0 ? "+" : "") + String (v, 1) + " dB"; })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { btids::ratio, 1 }, "Ratio",
        NormalisableRange<float> (2.0f, 10.0f, 0.01f), 6.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + ":1"; })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { btids::threshold, 1 }, "Threshold",
        custom (-40.0f, 10.0f, btparam::thresholdFrom01, btparam::thresholdTo01), -20.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            { return btparam::thresholdIsOff (v) ? String ("off") : String (v, 1) + " dB"; })));

    auto knob10 = [] (float v, int) { return String (v, 1); };
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { btids::attack, 1 }, "Attack",
        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f, AudioParameterFloatAttributes().withStringFromValueFunction (knob10)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { btids::release, 1 }, "Release",
        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f, AudioParameterFloatAttributes().withStringFromValueFunction (knob10)));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { btids::arSelect, 1 }, "Attack/Release Select", StringArray { "Fixed", "Fix/Man", "Manual" }, 2));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { btids::scBus, 1 }, "Sidechain Bus", StringArray { "Internal", "External" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { btids::lowCut, 1 }, "Sidechain Low Cut", StringArray { "Off", "80 Hz", "220 Hz" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { btids::parallel, 1 }, "Parallel Comp",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v <= 0.5f ? String ("dry") : v >= 99.5f ? String ("wet") : String (v, 0) + " % wet"; })));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { btids::power, 1 }, "In", true));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { btids::gen, 1 }, "Generation", StringArray { "Vintage mk I", "Modern mk II" }, 1));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { btids::meter, 1 }, "Meter", StringArray { "Input", "Compression", "Output" }, 2));

    return { p.begin(), p.end() };
}
