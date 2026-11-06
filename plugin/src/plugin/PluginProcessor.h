#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine.h"

namespace ParamIds
{
inline constexpr auto routing = "routing";
inline constexpr auto color = "color";
inline constexpr auto drive = "drive";
inline constexpr auto mix = "mix";
inline constexpr auto levelA = "levelA";
inline constexpr auto levelB = "levelB";
inline constexpr auto output = "output";
inline constexpr auto noise = "noise";
inline constexpr auto quality = "quality";
inline constexpr auto temperature = "temperature";
} // namespace ParamIds

class DamnFineDelayProcessor final : public juce::AudioProcessor,
                                     private juce::AudioProcessorValueTreeState::Listener,
                                     private juce::AsyncUpdater
{
public:
    static constexpr int kStateVersion = 1;

    DamnFineDelayProcessor();
    ~DamnFineDelayProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.2; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& state() noexcept { return apvts; }

    /** Live status for the editor's diagnostics line (written on the audio thread, read on the UI thread). */
    struct Diagnostics
    {
        std::atomic<std::uint64_t> blocks { 0 }, bypassedBlocks { 0 };
        std::atomic<float> inDb { -100.0f }, outDb { -100.0f };
        std::atomic<int> numIn { 0 }, numOut { 0 };
        std::atomic<double> sampleRate { 0.0 };
    };
    const Diagnostics& diagnostics() const noexcept { return diag; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    dfd::Engine engine;
    double currentRate = 0.0;
    Diagnostics diag;

    struct Raw
    {
        std::atomic<float>* routing;
        std::atomic<float>* color;
        std::atomic<float>* drive;
        std::atomic<float>* mix;
        std::atomic<float>* levelA;
        std::atomic<float>* levelB;
        std::atomic<float>* output;
        std::atomic<float>* noise;
        std::atomic<float>* quality;
        std::atomic<float>* temperature;
    } raw {};

    dfd::EngineSetup currentSetup() const;
    dfd::EngineParams currentParams() const;
    void rebuildEngine();

    void parameterChanged (const juce::String& id, float) override;
    void handleAsyncUpdate() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DamnFineDelayProcessor)
};
