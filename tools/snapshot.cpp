// Renders the Compchild editor to a PNG without a host (for interface checks).
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdlib>
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    CompchildProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    if (argc > 2) { proc.grA.store (6.0f); proc.grB.store (12.0f); }
    if (argc > 3) proc.apvts.state.setProperty ("meterSkin", 1, nullptr);   // black skin
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    if (auto* e = dynamic_cast<CompchildEditor*> (ed.get())) e->settleMetersForSnapshot();
    const float scale = std::getenv ("CC_SCALE") ? (float) std::atof (std::getenv ("CC_SCALE")) : 1.0f;
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
    juce::File out (argc > 1 ? argv[1] : "snapshot.png");
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
