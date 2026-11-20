#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace ids
{
    inline constexpr const char* power  = "power";
    inline constexpr const char* link   = "link";
    inline constexpr const char* mode   = "mode";
    inline constexpr const char* output = "output";
    inline constexpr const char* thA = "thA", *thB = "thB";
    inline constexpr const char* inA = "inA", *inB = "inB";
    inline constexpr const char* tcA = "tcA", *tcB = "tcB";
    inline constexpr const char* dcA = "dcA", *dcB = "dcB";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto db  = [] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; };
    auto num = [] (float v, int) { return String (v, 1); };
    auto F = [&] (const char* id, const char* name, float lo, float hi, float def, std::function<String (float, int)> txt, float step = 0.01f)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, step), def,
                                                             AudioParameterFloatAttributes().withStringFromValueFunction (std::move (txt))));
    };
    const StringArray tcs { "TC 1", "TC 2", "TC 3", "TC 4", "TC 5", "TC 6" };

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::power, 1 }, "Power", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::link, 1 }, "Link", true));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::mode, 1 }, "Mode", StringArray { "L / R", "Lat / Vert", "Mid / Side" }, 0));
    F (ids::output, "Output", -10.0f, 10.0f, 0.0f, db, 0.1f);

    F (ids::thA, "Threshold A", 0.0f, 10.0f, 5.0f, num);
    F (ids::inA, "Input Gain A", -10.0f, 10.0f, 0.0f, db, 0.1f);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::tcA, 1 }, "Time Constant A", tcs, 2));
    F (ids::dcA, "DC Bias A", -1.0f, 1.0f, 0.0f, [] (float v, int) { return String (v, 2); });

    F (ids::thB, "Threshold B", 0.0f, 10.0f, 5.0f, num);
    F (ids::inB, "Input Gain B", -10.0f, 10.0f, 0.0f, db, 0.1f);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::tcB, 1 }, "Time Constant B", tcs, 2));
    F (ids::dcB, "DC Bias B", -1.0f, 1.0f, 0.0f, [] (float v, int) { return String (v, 2); });

    return { p.begin(), p.end() };
}
