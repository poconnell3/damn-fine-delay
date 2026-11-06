#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace
{
using APVTS = juce::AudioProcessorValueTreeState;

juce::NormalisableRange<float> dbRange (float lo, float hi) { return { lo, hi, 0.1f }; }

juce::AudioParameterFloatAttributes dbAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withLabel ("dB")
        .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; });
}
} // namespace

APVTS::ParameterLayout DamnFineDelayProcessor::createLayout()
{
    using namespace juce;
    APVTS::ParameterLayout layout;
    const auto nonAutomatable = AudioParameterChoiceAttributes().withAutomatable (false);

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIds::routing, 1 }, "Routing",
                                                        StringArray { "Widener (A left, B right)", "Dual mono (L to A, R to B)", "Cascade (A into B)" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIds::color, 1 }, "Color",
                                                        StringArray { "A (original)", "B (leveled)" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIds::drive, 1 }, "Drive", dbRange (-20.0f, 20.0f), 0.0f, dbAttributes()));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIds::mix, 1 }, "Mix", NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 100.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (
                                                           [] (float v, int) { return String (v, 1) + " %"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIds::levelA, 1 }, "Level A", dbRange (-48.0f, 10.0f), 0.0f, dbAttributes()));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIds::levelB, 1 }, "Level B", dbRange (-48.0f, 10.0f), 0.0f, dbAttributes()));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIds::output, 1 }, "Output", dbRange (-15.0f, 15.0f), 0.0f, dbAttributes()));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIds::noise, 1 }, "Noise",
                                                        StringArray { "Off", "Authentic", "Extra" }, 1));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIds::quality, 1 }, "Quality",
                                                        StringArray { "Eco (1x)", "Normal (2x)", "HQ (4x)" }, 1, nonAutomatable));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIds::temperature, 1 }, "Temperature",
                                                       NormalisableRange<float> { 15.0f, 30.0f, 0.1f }, 20.0f,
                                                       AudioParameterFloatAttributes().withAutomatable (false).withLabel ("C").withStringFromValueFunction (
                                                           [] (float v, int) { return String (v, 1) + " C"; })));
    return layout;
}

DamnFineDelayProcessor::DamnFineDelayProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DamnFineDelay", createLayout())
{
    raw.routing = apvts.getRawParameterValue (ParamIds::routing);
    raw.color = apvts.getRawParameterValue (ParamIds::color);
    raw.drive = apvts.getRawParameterValue (ParamIds::drive);
    raw.mix = apvts.getRawParameterValue (ParamIds::mix);
    raw.levelA = apvts.getRawParameterValue (ParamIds::levelA);
    raw.levelB = apvts.getRawParameterValue (ParamIds::levelB);
    raw.output = apvts.getRawParameterValue (ParamIds::output);
    raw.noise = apvts.getRawParameterValue (ParamIds::noise);
    raw.quality = apvts.getRawParameterValue (ParamIds::quality);
    raw.temperature = apvts.getRawParameterValue (ParamIds::temperature);
    apvts.addParameterListener (ParamIds::quality, this);
    apvts.addParameterListener (ParamIds::temperature, this);
}

DamnFineDelayProcessor::~DamnFineDelayProcessor()
{
    cancelPendingUpdate();
    apvts.removeParameterListener (ParamIds::quality, this);
    apvts.removeParameterListener (ParamIds::temperature, this);
}

dfd::EngineSetup DamnFineDelayProcessor::currentSetup() const
{
    dfd::EngineSetup s;
    s.oversampling = 1 << juce::jlimit (0, 2, (int) raw.quality->load());
    s.temperatureC = raw.temperature->load();
    return s;
}

dfd::EngineParams DamnFineDelayProcessor::currentParams() const
{
    dfd::EngineParams p;
    p.routing = static_cast<dfd::Routing> (juce::jlimit (0, 2, (int) raw.routing->load()));
    p.color = (int) raw.color->load() == 0 ? dfd::Color::A : dfd::Color::B;
    p.driveDb = raw.drive->load();
    p.mix = raw.mix->load() / 100.0;
    p.levelADb = raw.levelA->load() <= -47.95f ? -300.0 : raw.levelA->load();
    p.levelBDb = raw.levelB->load() <= -47.95f ? -300.0 : raw.levelB->load();
    p.outputDb = raw.output->load();
    p.noise = static_cast<dfd::NoiseMode> (juce::jlimit (0, 2, (int) raw.noise->load()));
    return p;
}

void DamnFineDelayProcessor::prepareToPlay (double sampleRate, int)
{
    currentRate = sampleRate;
    engine.prepare (sampleRate, currentSetup(), currentParams());
    setLatencySamples (engine.latencySamples());
}

void DamnFineDelayProcessor::rebuildEngine()
{
    if (currentRate <= 0.0)
        return;
    suspendProcessing (true);
    engine.prepare (currentRate, currentSetup(), currentParams());
    setLatencySamples (engine.latencySamples());
    suspendProcessing (false);
}

void DamnFineDelayProcessor::parameterChanged (const juce::String&, float)
{
    triggerAsyncUpdate();
}

void DamnFineDelayProcessor::handleAsyncUpdate()
{
    rebuildEngine();
}

bool DamnFineDelayProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    const bool inOk = in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return inOk && outOk && ! (in == juce::AudioChannelSet::stereo() && out == juce::AudioChannelSet::mono());
}

void DamnFineDelayProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    if (numIn == 1 && numOut == 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);
    else
        for (int ch = numIn; ch < numOut; ++ch)
            buffer.clear (ch, 0, n);

    engine.setParams (currentParams());
    float* left = buffer.getWritePointer (0);
    float* right = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    engine.process (left, right, n);
}

juce::AudioProcessorEditor* DamnFineDelayProcessor::createEditor()
{
    return new DamnFineDelayEditor (*this);
}

void DamnFineDelayProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("version", kStateVersion, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DamnFineDelayProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            triggerAsyncUpdate();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DamnFineDelayProcessor();
}
