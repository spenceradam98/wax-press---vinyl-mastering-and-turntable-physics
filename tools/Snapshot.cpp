// Renders the editor to PNGs (one per theme) for docs and review, and smoke-tests every factory
// preset through the processor (finite output, no runaway level). Dev only, not shipped.
//   WaxPressSnapshot <out dir>
#include "../Source/plugin/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    int failures = 0;
    WaxPressProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 256);
    proc.prepareToPlay (48000.0, 256);
    juce::AudioBuffer<float> buf (2, 256);
    juce::MidiBuffer midi;
    auto feed = [&] (WaxPressProcessor& p, int blocks, float& peak, bool& finite)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < 256; ++i)
            {
                const float t = (float) (b * 256 + i) / 48000.0f;
                const float v = 0.35f * std::sin (6.2831853f * 55.0f * t) * (0.5f + 0.5f * std::sin (6.2831853f * 1.5f * t))
                              + 0.12f * std::sin (6.2831853f * 3520.0f * t);
                buf.setSample (0, i, v); buf.setSample (1, i, -0.4f * v + 0.2f * std::sin (6.2831853f * 660.0f * t));
            }
            p.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 256; ++i)
                {
                    const float s = buf.getSample (c, i);
                    finite = finite && std::isfinite (s);
                    peak = std::max (peak, std::abs (s));
                }
        }
    };

    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        proc.setCurrentProgram (i);
        proc.prepareToPlay (48000.0, 256);
        float peak = 0; bool finite = true;
        feed (proc, 400, peak, finite);
        const bool ok = finite && peak < 3.0f;
        std::printf ("%s  %-26s peak %.2f\n", ok ? "ok  " : "FAIL", proc.getProgramName (i).toRawUTF8(), peak);
        if (! ok) ++failures;
    }

    // screenshots: a preset with a bit of everything, out-of-phase bass so the lamp lights
    proc.setCurrentProgram (12);   // Dusty Breaks
    if (auto* prm = proc.apvts.getParameter ("mono_bass")) prm->setValueNotifyingHost (prm->convertTo0to1 (40.0f));
    if (auto* prm = proc.apvts.getParameter ("needle")) prm->setValueNotifyingHost (prm->convertTo0to1 (0.55f));
    proc.prepareToPlay (48000.0, 256);
    float peak = 0; bool finite = true;
    feed (proc, 120, peak, finite);
    for (int t = 0; t < (int) themes().size(); ++t)
    {
        proc.themeIndex = t;
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        auto* e = dynamic_cast<WaxPressEditor*> (ed.get());
        e->setSize (WaxPressContent::W, WaxPressContent::H);
        for (int k = 0; k < 12; ++k) { feed (proc, 4, peak, finite); e->refreshNow(); }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        juce::FileOutputStream os (outDir.getChildFile ("theme-" + juce::String (t) + "-" + themes()[(size_t) t].name.replaceCharacter (' ', '-').toLowerCase() + ".png"));
        os.setPosition (0); os.truncate();
        juce::PNGImageFormat().writeImageToStream (img, os);
    }
    std::printf ("%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
