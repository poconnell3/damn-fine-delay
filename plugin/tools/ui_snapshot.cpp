// Renders the editor to a PNG after feeding it test audio, so the panel can be checked without a host.
// Usage: dfd_ui_snapshot out.png [scale] [routing 0-2] [numIn 1-2] [numOut 1-2] [doubler 0-1] [humanize 0-1]
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf (stderr, "usage: %s out.png [scale] [routing] [numIn] [numOut] [doubler] [humanize]\n", argv[0]);
        return 1;
    }
    juce::ScopedJuceInitialiser_GUI init;
    const float scale = argc > 2 ? (float) std::atof (argv[2]) : 1.0f;
    const int routing = argc > 3 ? std::atoi (argv[3]) : 0;
    const int numIn = argc > 4 ? std::atoi (argv[4]) : 2, numOut = argc > 5 ? std::atoi (argv[5]) : 2;
    const bool doubler = argc > 6 && std::atoi (argv[6]) != 0;
    const bool human = argc > 7 && std::atoi (argv[7]) != 0;

    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    auto layout = proc->getBusesLayout();
    layout.inputBuses.getReference (0) = juce::AudioChannelSet::canonicalChannelSet (numIn);
    layout.outputBuses.getReference (0) = juce::AudioChannelSet::canonicalChannelSet (numOut);
    if (! proc->setBusesLayout (layout))
    {
        std::fprintf (stderr, "layout %d in / %d out not supported\n", numIn, numOut);
        return 1;
    }
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (proc->getParameters()[0]))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) routing));
    for (auto* p : proc->getParameters())
    {
        if (p->getName (64) == "Double Mode")
            p->setValueNotifyingHost (doubler ? 1.0f : 0.0f);
        if (p->getName (64) == "Humanize")
            p->setValueNotifyingHost (human ? 1.0f : 0.0f);
    }
    proc->prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc->createEditor());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

    // About 1.5 s of a decaying chord, near -14 dBFS peak, so meters and needle sit mid-scale.
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int block = 0; block < 140; ++block)
    {
        for (int i = 0; i < 512; ++i)
        {
            const double t = (block * 512 + i) / 48000.0;
            const double env = 0.55 + 0.45 * std::exp (-3.0 * std::fmod (t, 0.5));
            const double l = env * (0.12 * std::sin (2 * juce::MathConstants<double>::pi * 220.0 * t) + 0.07 * std::sin (2 * juce::MathConstants<double>::pi * 330.0 * t));
            const double r = env * (0.09 * std::sin (2 * juce::MathConstants<double>::pi * 277.0 * t) + 0.05 * std::sin (2 * juce::MathConstants<double>::pi * 415.0 * t));
            buf.setSample (0, i, (float) l);
            buf.setSample (1, i, (float) r);
        }
        proc->processBlock (buf, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (11);
    }

    if (auto* c = editor->getConstrainer())
        editor->setSize (juce::roundToInt (c->getMaximumWidth() / 1.6), juce::roundToInt (c->getMaximumHeight() / 1.6)); // design size
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;
    if (! stream.openedOk() || ! png.writeImageToStream (image, stream))
        return 1;
    std::printf ("wrote %s (%d x %d)\n", out.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
    editor.reset();
    proc->releaseResources();
    return 0;
}
