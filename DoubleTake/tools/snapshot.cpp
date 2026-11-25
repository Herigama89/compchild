// Renders the DoubleTake editor to a PNG without a host (interface checks, manual pictures).
// usage: dt_snapshot out.png [presetIndex] [meterDb]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdlib>
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    DoubleTakeProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    if (argc > 2) proc.setCurrentProgram (std::atoi (argv[2]));
    const float m = argc > 3 ? (float) std::atof (argv[3]) : -100.0f;
    proc.outL.store (m); proc.outR.store (m - 1.5f);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    if (auto* e = dynamic_cast<DoubleTakeEditor*> (ed.get())) e->settleForSnapshot();
    const float scale = std::getenv ("DT_SCALE") ? (float) std::atof (std::getenv ("DT_SCALE")) : 1.0f;
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshot.png"));
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
