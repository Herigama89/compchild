#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace dtp
{
    inline juce::String v (int voice, const char* what) { return "v" + juce::String (voice) + "_" + what; }   // voice 1..4

    inline juce::String dbTxt (float x, int) { return x <= -79.9f ? juce::String ("-inf") : (x > 0.05f ? "+" : "") + juce::String (x, 1) + " dB"; }
    inline juce::String panTxt (float x, int) { return std::abs (x) < 0.5f ? juce::String ("C") : (x < 0 ? "L" : "R") + juce::String (std::abs (x), 0); }
    inline juce::String hzTxt (float x, int)  { return x >= 1000.0f ? juce::String (x / 1000.0f, x >= 10000.0f ? 1 : 2) + "k" : juce::String (x, 0) + " Hz"; }

    inline juce::AudioProcessorValueTreeState::ParameterLayout layout()
    {
        using namespace juce;
        std::vector<std::unique_ptr<RangedAudioParameter>> p;
        auto F = [&] (const String& id, const String& name, NormalisableRange<float> r, float def, std::function<String (float, int)> txt)
        {
            p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                                                                 AudioParameterFloatAttributes().withStringFromValueFunction (std::move (txt))));
        };
        auto B = [&] (const String& id, const String& name, bool def) { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
        auto logRange = [] (float lo, float hi) { NormalisableRange<float> r (lo, hi, 0.1f); r.setSkewForCentre (std::sqrt (lo * hi)); return r; };

        B ("four", "4 Voices", false);
        B ("dryOn", "Direct On", true);
        F ("dryGain", "Direct Gain", { -24.0f, 6.0f, 0.1f }, 0.0f, dbTxt);
        F ("dryPan", "Direct Pan", { -100.0f, 100.0f, 1.0f }, 0.0f, panTxt);
        B ("align", "Align", true);
        F ("fbCut", "Feedback Low-Cut", logRange (20.0f, 1000.0f), 80.0f, hzTxt);
        F ("lowGain", "Low Shelf Gain", { -18.0f, 18.0f, 0.1f }, -4.0f, dbTxt);
        F ("lowFreq", "Low Shelf Freq", logRange (20.0f, 500.0f), 120.0f, hzTxt);
        F ("highGain", "High Shelf Gain", { -18.0f, 18.0f, 0.1f }, 2.0f, dbTxt);
        F ("highFreq", "High Shelf Freq", logRange (1000.0f, 16000.0f), 6000.0f, hzTxt);
        F ("mix", "Mix", { 0.0f, 100.0f, 1.0f }, 50.0f, [] (float x, int) { return String (x, 0) + "%"; });
        F ("wet", "Wet Level", { -80.0f, 10.0f, 0.1f }, 0.0f, dbTxt);
        F ("out", "Output Level", { -80.0f, 10.0f, 0.1f }, 0.0f, dbTxt);

        // defaults = the "Natural Double" preset for voices 1-2, a wider pair for 3-4
        const float def[4][8] = { { -3, -60, 12, 0, 9, 4, 0.6f, 0 }, { -3, 60, 19, 0, -11, 5, 0.45f, 0 },
                                  { -6, -100, 26, 0, 17, 6, 0.8f, 0 }, { -6, 100, 33, 0, -15, 6, 0.7f, 0 } };
        for (int i = 1; i <= 4; ++i)
        {
            const auto* d = def[i - 1];
            const String n = "Voice " + String (i) + " ";
            B (v (i, "on"), n + "On", true);
            F (v (i, "gain"), n + "Gain", { -24.0f, 6.0f, 0.1f }, d[0], dbTxt);
            F (v (i, "pan"), n + "Pan", { -100.0f, 100.0f, 1.0f }, d[1], panTxt);
            F (v (i, "delay"), n + "Delay", { 0.0f, 100.0f, 0.1f }, d[2], [] (float x, int) { return String (x, 1) + " ms"; });
            F (v (i, "fdbk"), n + "Feedback", { 0.0f, 90.0f, 1.0f }, d[3], [] (float x, int) { return String (x, 0) + "%"; });
            p.push_back (std::make_unique<AudioParameterInt> (ParameterID { v (i, "oct"), 1 }, n + "Octave", -1, 1, (int) d[7]));
            F (v (i, "detune"), n + "Detune", { -50.0f, 50.0f, 1.0f }, d[4], [] (float x, int) { return (x > 0 ? "+" : "") + String (x, 0) + " ct"; });
            F (v (i, "depth"), n + "Wobble Depth", { 0.0f, 50.0f, 1.0f }, d[5], [] (float x, int) { return String (x, 0) + " ct"; });
            NormalisableRange<float> rr (0.1f, 40.0f, 0.01f); rr.setSkewForCentre (2.0f);
            F (v (i, "rate"), n + "Wobble Rate", rr, d[6], [] (float x, int) { return String (x, x < 10 ? 2 : 1) + " Hz"; });
        }
        return { p.begin(), p.end() };
    }
}
