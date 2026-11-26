// Renders the Black Tube editor to a PNG without a host (interface checks).
// usage: bt_snapshot out.png [grDb] — feeds a sine so the meter and scope have something to show
#define _USE_MATH_DEFINES
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdlib>
#include <cmath>
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    BlackTubeProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
    double ph = 0.0;
    for (int b = 0; b < 400; ++b)                       // ~4 s of a pulsing 120 Hz tone
    {
        const float amp = (b / 25) % 2 == 0 ? 0.6f : 0.08f;
        for (int i = 0; i < 512; ++i) { const float v = amp * (float) std::sin (ph); ph += 2.0 * M_PI * 120.0 / 48000.0; buf.setSample (0, i, v); buf.setSample (1, i, v); }
        proc.processBlock (buf, midi);
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (1700, 620);
    if (auto* e = dynamic_cast<BlackTubeEditor*> (ed.get())) e->settleForSnapshot();
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "bt_snapshot.png"));
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
