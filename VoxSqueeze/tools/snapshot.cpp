// Renders the VoxSqueeze editor to a PNG without a host (for interface checks and the manual).
// usage: vx_snapshot out.png [gate comp gain] [inDb grDb outDb]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdlib>
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    VoxSqueezeProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    auto set = [&] (const char* id, float v) { if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    if (argc > 4) { set (vids::gate, (float) std::atof (argv[2])); set (vids::comp, (float) std::atof (argv[3])); set (vids::gain, (float) std::atof (argv[4])); }
    if (argc > 7) { proc.inDb.store ((float) std::atof (argv[5])); proc.grDb.store ((float) std::atof (argv[6])); proc.outDb.store ((float) std::atof (argv[7])); }
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    if (auto* e = dynamic_cast<VoxSqueezeEditor*> (ed.get())) e->settleMetersForSnapshot();
    const float scale = std::getenv ("VX_SCALE") ? (float) std::atof (std::getenv ("VX_SCALE")) : 1.0f;
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshot.png"));
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
