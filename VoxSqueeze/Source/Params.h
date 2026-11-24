#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace vids
{
    inline constexpr const char* gate   = "gate";
    inline constexpr const char* comp   = "comp";
    inline constexpr const char* gain   = "gain";
    inline constexpr const char* stereo = "stereo";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createVoxParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto db = [] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; };
    auto F = [&] (const char* id, const char* name, float lo, float hi, float def, std::function<String (float, int)> txt)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.1f), def,
                                                             AudioParameterFloatAttributes().withStringFromValueFunction (std::move (txt))));
    };
    F (vids::gate, "Gate", -80.0f, 0.0f, -80.0f, [db] (float v, int n) { return v <= -79.9f ? String ("OFF") : db (v, n); });
    F (vids::comp, "Comp", -60.0f, 0.0f,   0.0f, db);
    F (vids::gain, "Gain", -80.0f, 10.0f,  0.0f, [db] (float v, int n) { return v <= -79.9f ? String ("-inf") : db (v, n); });
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { vids::stereo, 1 }, "Stereo", true));
    return { p.begin(), p.end() };
}
