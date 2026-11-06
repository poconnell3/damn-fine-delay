#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine.h"

namespace ParamIds
{
inline constexpr auto routing = "routing";
inline constexpr auto color = "color";
inline constexpr auto drive = "drive";
inline constexpr auto mix = "mix";
inline constexpr auto wetOnly = "wetOnly";
inline constexpr auto levelA = "levelA";
inline constexpr auto levelB = "levelB";
inline constexpr auto output = "output";
inline constexpr auto noise = "noise";
inline constexpr auto quality = "quality";
inline constexpr auto temperature = "temperature";
inline constexpr auto doubler = "doubler";
inline constexpr auto dryPan = "dryPan";
inline constexpr auto dryLevel = "dryLevel";
inline constexpr auto wetPan = "wetPan";
inline constexpr auto wetLevel = "wetLevel";
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

    /** Lock-free level tap: the audio thread accumulates, the UI takes and resets. */
    class LevelTap
    {
    public:
        struct Reading
        {
            float peak = 0.0f, rms = 0.0f;
        };

        void push (const float* x, int n) noexcept;
        Reading take() noexcept;

    private:
        std::atomic<float> peak { 0.0f }, sumSquares { 0.0f };
        std::atomic<int> count { 0 };
    };

    struct Metering
    {
        std::array<LevelTap, 2> in, out;
        std::atomic<int> numIn { 2 }, numOut { 2 };
        std::atomic<std::uint32_t> processedBlocks { 0 };
    };
    Metering& metering() noexcept { return meters; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    dfd::Engine engine;
    double currentRate = 0.0;
    Metering meters;

    struct Raw
    {
        std::atomic<float>* routing;
        std::atomic<float>* color;
        std::atomic<float>* drive;
        std::atomic<float>* mix;
        std::atomic<float>* wetOnly;
        std::atomic<float>* levelA;
        std::atomic<float>* levelB;
        std::atomic<float>* output;
        std::atomic<float>* noise;
        std::atomic<float>* quality;
        std::atomic<float>* temperature;
        std::atomic<float>* doubler;
        std::atomic<float>* dryPan;
        std::atomic<float>* dryLevel;
        std::atomic<float>* wetPan;
        std::atomic<float>* wetLevel;
    } raw {};

    dfd::EngineSetup currentSetup() const;
    dfd::EngineParams currentParams() const;
    void rebuildEngine();
    std::pair<float*, float*> prepareChannels (juce::AudioBuffer<float>&);
    void meterBlock (std::array<LevelTap, 2>&, const juce::AudioBuffer<float>&, int channels) noexcept;

    void parameterChanged (const juce::String& id, float) override;
    void handleAsyncUpdate() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DamnFineDelayProcessor)
};
