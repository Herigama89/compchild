// Renders the OPT-2A editor to a PNG without a host (interface checks).
// opt_snapshot out.png [feed]   feed = run a test tone through the processor first (needle + scope)
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdlib>
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    Opt2aProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    if (auto* pr = proc.apvts.getParameter (ids::peak)) pr->setValueNotifyingHost (pr->convertTo0to1 (55.0f));
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    if (argc > 2)
    {
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; double ph = 0;
        for (int blk = 0; blk < 450; ++blk)
        {
            const float env = 0.08f + 0.5f * std::pow (std::abs (std::sin (blk * 0.045)), 3.0f);
            for (int i = 0; i < 512; ++i) { const float v = env * (float) std::sin (ph); ph += 2 * M_PI * 220.0 / 48000.0; b.setSample (0, i, v); b.setSample (1, i, v); }
            proc.processBlock (b, m);
            if (blk % 3 == 0) if (auto* e = dynamic_cast<Opt2aEditor*> (ed.get())) e->settleForSnapshot();
        }
    }
    if (auto* e = dynamic_cast<Opt2aEditor*> (ed.get())) e->settleForSnapshot();
    const float scale = std::getenv ("OPT_SCALE") ? (float) std::atof (std::getenv ("OPT_SCALE")) : 1.0f;
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshot.png"));
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
