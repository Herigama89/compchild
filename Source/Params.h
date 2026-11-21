#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace ids
{
    inline constexpr const char* power    = "power";
    inline constexpr const char* peak     = "peak";
    inline constexpr const char* gain     = "gain";
    inline constexpr const char* limit    = "limit";
    inline constexpr const char* emphasis = "emphasis";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* meter    = "meter";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto F = [&] (const char* id, const char* name, float def, std::function<String (float, int)> txt)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 0.1f), def,
                                                             AudioParameterFloatAttributes().withStringFromValueFunction (std::move (txt))));
    };
    auto one = [] (float v, int) { return String (v, 1); };
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::power, 1 }, "Power", true));
    F (ids::peak, "Peak Reduction", 25.0f, one);
    F (ids::gain, "Gain", 30.0f, one);
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::limit, 1 }, "Limit", false,
                     AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool b, int) { return b ? String ("Limit") : String ("Compress"); })));
    F (ids::emphasis, "Emphasis", 0.0f, [] (float v, int) { return v < 0.05f ? String ("Flat") : String (v, 0) + " %"; });
    F (ids::mix, "Mix", 100.0f, [] (float v, int) { return String (v, 0) + " %"; });
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::meter, 1 }, "Meter",
                     StringArray { "Output +10", "Gain Reduction", "Output +4" }, 1));
    return { p.begin(), p.end() };
}
