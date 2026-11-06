#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace
{
using APVTS = juce::AudioProcessorValueTreeState;

juce::NormalisableRange<float> dbRange (float lo, float hi) { return { lo, hi, 0.1f }; }

void atomicMax (std::atomic<float>& a, float v) noexcept
{
    for (float cur = a.load (std::memory_order_relaxed); v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed);)
    {
    }
}

void atomicAdd (std::atomic<float>& a, float v) noexcept
{
    for (float cur = a.load (std::memory_order_relaxed); ! a.compare_exchange_weak (cur, cur + v, std::memory_order_relaxed);)
    {
    }
}

/** Pan as L100..C..R100. */
juce::AudioParameterFloatAttributes panAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int)
        {
            const int n = juce::roundToInt (v);
            return n == 0 ? juce::String ("C") : (n < 0 ? "L" : "R") + juce::String (std::abs (n));
        })
        .withValueFromStringFunction ([] (const juce::String& text)
        {
            const auto t = text.trim().toUpperCase();
            const float n = (float) t.retainCharacters ("0123456789.").getFloatValue();
            return t.startsWith ("L") ? -n : (t.startsWith ("R") ? n : (t.startsWith ("C") ? 0.0f : t.getFloatValue()));
        });
}

float levelDb (const std::atomic<float>* raw) { return raw->load() <= -47.95f ? -300.0f : raw->load(); }

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
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIds::mix, 1 }, "Mix", NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 40.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (
                                                           [] (float v, int) { return String (v, 1) + " %"; })));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIds::wetOnly, 2 }, "Wet Only", false));
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
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIds::doubler, 3 }, "Double Mode", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIds::dryPan, 3 }, "Dry Pan", NormalisableRange<float> { -100.0f, 100.0f, 1.0f }, -60.0f, panAttributes()));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIds::dryLevel, 3 }, "Dry Level", dbRange (-48.0f, 10.0f), 0.0f, dbAttributes()));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIds::wetPan, 3 }, "Delay Pan", NormalisableRange<float> { -100.0f, 100.0f, 1.0f }, 60.0f, panAttributes()));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIds::wetLevel, 3 }, "Delay Level", dbRange (-48.0f, 10.0f), 0.0f, dbAttributes()));
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
    raw.wetOnly = apvts.getRawParameterValue (ParamIds::wetOnly);
    raw.levelA = apvts.getRawParameterValue (ParamIds::levelA);
    raw.levelB = apvts.getRawParameterValue (ParamIds::levelB);
    raw.output = apvts.getRawParameterValue (ParamIds::output);
    raw.noise = apvts.getRawParameterValue (ParamIds::noise);
    raw.quality = apvts.getRawParameterValue (ParamIds::quality);
    raw.temperature = apvts.getRawParameterValue (ParamIds::temperature);
    raw.doubler = apvts.getRawParameterValue (ParamIds::doubler);
    raw.dryPan = apvts.getRawParameterValue (ParamIds::dryPan);
    raw.dryLevel = apvts.getRawParameterValue (ParamIds::dryLevel);
    raw.wetPan = apvts.getRawParameterValue (ParamIds::wetPan);
    raw.wetLevel = apvts.getRawParameterValue (ParamIds::wetLevel);
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
    p.mix = raw.wetOnly->load() >= 0.5f ? 1.0 : raw.mix->load() / 100.0;
    p.levelADb = levelDb (raw.levelA);
    p.levelBDb = levelDb (raw.levelB);
    p.outputDb = raw.output->load();
    p.noise = static_cast<dfd::NoiseMode> (juce::jlimit (0, 2, (int) raw.noise->load()));
    p.doubler = raw.doubler->load() >= 0.5f;
    if (p.doubler)
        p.routing = dfd::Routing::Cascade;
    p.dryPan = raw.dryPan->load() / 100.0;
    p.wetPan = raw.wetPan->load() / 100.0;
    p.dryLevelDb = levelDb (raw.dryLevel);
    p.wetLevelDb = levelDb (raw.wetLevel);
    return p;
}

void DamnFineDelayProcessor::prepareToPlay (double sampleRate, int)
{
    currentRate = sampleRate;
    meters.numIn.store (getTotalNumInputChannels());
    meters.numOut.store (getTotalNumOutputChannels());
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

void DamnFineDelayProcessor::LevelTap::push (const float* x, int n) noexcept
{
    float p = 0.0f, sum = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        p = juce::jmax (p, std::abs (x[i]));
        sum += x[i] * x[i];
    }
    atomicMax (peak, p);
    atomicAdd (sumSquares, sum);
    count.fetch_add (n, std::memory_order_relaxed);
}

DamnFineDelayProcessor::LevelTap::Reading DamnFineDelayProcessor::LevelTap::take() noexcept
{
    const float p = peak.exchange (0.0f, std::memory_order_relaxed);
    const float sum = sumSquares.exchange (0.0f, std::memory_order_relaxed);
    const int n = count.exchange (0, std::memory_order_relaxed);
    return { p, n > 0 ? std::sqrt (sum / (float) n) : 0.0f };
}

void DamnFineDelayProcessor::meterBlock (std::array<LevelTap, 2>& taps, const juce::AudioBuffer<float>& b, int channels) noexcept
{
    for (int ch = 0; ch < juce::jmin (2, channels, b.getNumChannels()); ++ch)
        taps[(size_t) ch].push (b.getReadPointer (ch), b.getNumSamples());
}

std::pair<float*, float*> DamnFineDelayProcessor::prepareChannels (juce::AudioBuffer<float>& buffer)
{
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    if (numIn == 1 && numOut == 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);
    else
        for (int ch = numIn; ch < numOut; ++ch)
            buffer.clear (ch, 0, n);
    meters.numIn.store (numIn, std::memory_order_relaxed);
    meters.numOut.store (numOut, std::memory_order_relaxed);
    meterBlock (meters.in, buffer, numIn);
    return { buffer.getWritePointer (0), numOut > 1 ? buffer.getWritePointer (1) : nullptr };
}

void DamnFineDelayProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto [left, right] = prepareChannels (buffer);
    engine.setParams (currentParams());
    engine.process (left, right, buffer.getNumSamples());
    meterBlock (meters.out, buffer, getTotalNumOutputChannels());
    meters.processedBlocks.fetch_add (1, std::memory_order_relaxed);
}

void DamnFineDelayProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto [left, right] = prepareChannels (buffer);
    engine.processBypassed (left, right, buffer.getNumSamples());
    meterBlock (meters.out, buffer, getTotalNumOutputChannels());
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
